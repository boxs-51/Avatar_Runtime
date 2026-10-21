#include "typed_glb_packet.hpp"
#include "cpu_model_packet.hpp"
#include "../validation/glb_validator.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
namespace ars::assets {
class GlbPacketBuilder {
  public:
    static std::shared_ptr<GlbModel> make(Resource &r) {
        auto arena = std::allocate_shared<GlbParseArena>(std::pmr::polymorphic_allocator<GlbParseArena>(&r), r);
        auto *raw = new (r.allocate(sizeof(GlbModel), alignof(GlbModel))) GlbModel(r, std::move(arena));
        return {raw, [&r](auto *p) { p->~GlbModel(); r.deallocate(p, sizeof(GlbModel), alignof(GlbModel)); },
                std::pmr::polymorphic_allocator<std::byte>(&r)};
    }
    static auto &document(GlbModel &m) { return m.json_; }
    static auto &data(GlbModel &m) { return m.data_; }
};
namespace packet {
namespace {
using validation::Document;
using validation::Kind;
void utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        auto c = static_cast<unsigned char>(s[i++]);
        if (c < 128) continue;
        unsigned n = 0; std::uint32_t v = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { n = 1; v = c & 31; minimum = 128; }
        else if (c >= 0xe0 && c <= 0xef) { n = 2; v = c & 15; minimum = 2048; }
        else if (c >= 0xf0 && c <= 0xf4) { n = 3; v = c & 7; minimum = 65536; }
        else fail();
        require(n <= s.size() - i);
        while (n--) { c = static_cast<unsigned char>(s[i++]); require((c & 0xc0) == 0x80); v = (v << 6) | (c & 63); }
        require(v >= minimum && v <= 0x10ffff && !(v >= 0xd800 && v <= 0xdfff));
    }
}
struct Wire {
    std::pmr::vector<std::uint8_t> &out;
    void add(std::span<const std::uint8_t> s) {
        require(s.size() <= byte_cap - out.size(), ARS_E_OUT_OF_BUDGET); out.insert(out.end(), s.begin(), s.end());
    }
    void integer(std::uint64_t v, unsigned n = 4) {
        std::array<std::uint8_t, 8> a{}; for (unsigned i = 0; i < n; ++i) a[i] = static_cast<std::uint8_t>(v >> (8*i)); add({a.data(), n});
    }
    void text(std::string_view s) { integer(s.size()); add({reinterpret_cast<const std::uint8_t *>(s.data()), s.size()}); }
    void node(const Document &doc, std::size_t index, Work &work, std::size_t depth) {
        require(depth <= 64 && index < doc.nodes.size()); work.take(); const auto &n = doc.nodes[index];
        integer(static_cast<unsigned>(n.kind)); text(n.key);
        switch (n.kind) {
        case Kind::null_value: break;
        case Kind::boolean: integer(n.boolean); break;
        case Kind::number: integer(std::bit_cast<std::uint64_t>(n.number), 8); break;
        case Kind::string: text(n.text); break;
        case Kind::array: case Kind::object:
            integer(n.children.size()); for (auto c : n.children) node(doc, c, work, depth + 1); break;
        default: fail();
        }
    }
};
struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t &pos;
    const Limits &limits;
    Work &work;
    std::size_t tree_bytes = 0;
    auto take(std::size_t n) { require(pos <= bytes.size() && n <= bytes.size() - pos); auto s = bytes.subspan(pos, n); pos += n; return s; }
    std::uint64_t integer(unsigned n = 4) { auto s = take(n); std::uint64_t v = 0; for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(s[i]) << (8*i); return v; }
    std::size_t count(std::size_t cap, std::size_t minimum) { auto n = integer(); require(n <= cap && n <= (bytes.size() - pos) / minimum); return static_cast<std::size_t>(n); }
    void text(std::pmr::string &out) {
        const auto n = count(limits.json, 1); require(n <= limits.json - tree_bytes, ARS_E_OUT_OF_BUDGET); tree_bytes += n;
        auto s = take(n); std::string_view view{reinterpret_cast<const char *>(s.data()), s.size()}; utf8(view); out = view; work.take(n / 16 + 1);
    }
    double number() { auto v = std::bit_cast<double>(integer(8)); require(std::isfinite(v)); return v; }
    std::size_t node(Document &doc, std::size_t depth, bool keyed) {
        require(depth <= limits.depth && doc.nodes.size() < limits.nodes, ARS_E_OUT_OF_BUDGET); work.take();
        // Charge fixed typed fields as well as string bytes to the metadata cap.
        require(limits.json >= 16 && tree_bytes <= limits.json - 16, ARS_E_OUT_OF_BUDGET); tree_bytes += 16;
        const auto index = doc.nodes.size(); doc.nodes.emplace_back(doc.nodes.get_allocator().resource());
        const auto kind = integer(); require(kind <= static_cast<unsigned>(Kind::object)); doc.nodes[index].kind = static_cast<Kind>(kind);
        text(doc.nodes[index].key); require(keyed ? doc.nodes[index].key.size() <= 1024 : doc.nodes[index].key.empty());
        switch (doc.nodes[index].kind) {
        case Kind::null_value: break;
        case Kind::boolean: { auto v = integer(); require(v <= 1); doc.nodes[index].boolean = v != 0; break; }
        case Kind::number: doc.nodes[index].number = number(); break;
        case Kind::string: text(doc.nodes[index].text); break;
        case Kind::array: case Kind::object: {
            const bool object = doc.nodes[index].kind == Kind::object;
            const auto n = count(limits.nodes, 8);
            require(n <= limits.nodes - doc.nodes.size(), ARS_E_OUT_OF_BUDGET);
            doc.nodes[index].children.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                const auto child = node(doc, depth + 1, object);
                if (object) for (auto other : doc.nodes[index].children) { work.take(); require(doc.nodes[other].key != doc.nodes[child].key); }
                doc.nodes[index].children.push_back(child);
            }
            break;
        }
        default: fail();
        }
        return index;
    }
};
} // namespace
void write_glb(const GlbModel &m, std::pmr::vector<std::uint8_t> &out, Work &work) {
    Wire w{out}; w.node(m.metadata_document(), 0, work, 0);
    w.integer(m.data().accessors.size());
    for (const auto &a : m.data().accessors) { w.integer(a.values.size()); work.take(a.values.size()); for (auto v : a.values) w.integer(std::bit_cast<std::uint64_t>(v), 8); }
    w.integer(m.data().images.size());
    for (const auto &image : m.data().images) { w.integer(image.width); w.integer(image.height); w.integer(image.rgba.size()); w.add(image.rgba); }
}
std::shared_ptr<const GlbModel> read_glb(std::span<const std::uint8_t> bytes, std::size_t &pos,
                                      Resource &r, const Limits &limits, Work &work) {
    Reader rd{bytes, pos, limits, work}; auto m = GlbPacketBuilder::make(r);
    rd.node(GlbPacketBuilder::document(*m), 0, false);
    GlbData decoded(&r);
    const auto n = rd.count(65536, 4); decoded.accessors.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto count = rd.count(limits.work, 8); work.take(count); auto &a = decoded.accessors.emplace_back(&r); a.values.reserve(count);
        for (std::size_t j = 0; j < count; ++j) a.values.push_back(rd.number());
    }
    const auto images = rd.count(65536, 12); std::size_t pixels = 0; decoded.images.reserve(images);
    for (std::size_t i = 0; i < images; ++i) {
        auto &image = decoded.images.emplace_back(&r); image.width = static_cast<std::uint32_t>(rd.integer()); image.height = static_cast<std::uint32_t>(rd.integer());
        require(image.width && image.height && image.width <= limits.edge && image.height <= limits.edge);
        const auto count = static_cast<std::size_t>(rd.integer());
        require(count <= limits.pixels - pixels, ARS_E_OUT_OF_BUDGET);
        require(std::uint64_t(image.width) * image.height * 4 == count); pixels += count;
        auto rgba = rd.take(count); image.rgba.assign(rgba.begin(), rgba.end());
    }
    require(!validation::validate_typed_glb(GlbPacketBuilder::document(*m), decoded, GlbPacketBuilder::data(*m), r, limits, work), ARS_E_UNSUPPORTED);
    return m;
}
} // namespace packet
} // namespace ars::assets
