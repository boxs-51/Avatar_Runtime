#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "cpu_model_packet.hpp"
#include "typed_glb_packet.hpp"
#include "../validation/glb_validator.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace ars::assets {
class CpuPacketBuilder {
  public:
    static std::shared_ptr<ValidatedCpuModel> make(Resource &r) {
        auto *raw = new (r.allocate(sizeof(ValidatedCpuModel), alignof(ValidatedCpuModel))) ValidatedCpuModel(r);
        return {raw, [&r](auto *p) {
            p->~ValidatedCpuModel(); r.deallocate(p, sizeof(ValidatedCpuModel), alignof(ValidatedCpuModel));
        }, std::pmr::polymorphic_allocator<std::byte>(&r)};
    }
    static auto &layers(ValidatedCpuModel &m) { return m.layers_; }
    static auto &parameters(ValidatedCpuModel &m) { return m.parameters_; }
    static auto &parts(ValidatedCpuModel &m) { return m.parts_; }
    static auto &semantics(ValidatedCpuModel &m) { return m.semantics_; }
    static auto &image(ValidatedCpuModel &m) { return m.image_; }
    static auto &canvas(ValidatedCpuModel &m) { return m.canvas_; }
    static auto &glb(ValidatedCpuModel &m) { return m.glb_; }
    static auto &mode(ValidatedCpuModel &m) { return m.default_mode_; }
    static auto &hierarchy(ValidatedCpuModel &m) { return m.complete_hierarchy_; }
};
namespace packet {
namespace {
constexpr std::string_view magic = "ARSCPU2D", full_magic = "ARSCPUFN";
struct Job {
    Resource &r;
    explicit Job(Resource &resource) : r(resource) { require(r.begin_job(), ARS_E_BUSY); }
    ~Job() { r.end_job(); }
};
bool error_status(ars_status s) {
    return s == ARS_E_BAD_ASSET || s == ARS_E_OUT_OF_BUDGET || s == ARS_E_UNSUPPORTED || s == ARS_E_INTERNAL;
}
void utf8(std::string_view s, bool empty = false, std::size_t byte_limit = 512) {
    require((empty || !s.empty()) && s.size() <= byte_limit);
    std::size_t count = 0;
    for (std::size_t i = 0; i < s.size();) {
        auto c = static_cast<unsigned char>(s[i++]);
        require(c != 0 && ++count <= 128);
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
void binding_id(std::string_view s) {
    require(!s.empty() && s.size() <= 128 && ((s.front() >= 'a' && s.front() <= 'z') || (s.front() >= 'A' && s.front() <= 'Z')));
    for (char c : s) require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-');
}
void canonical(std::string_view s) {
    require(!s.empty() && s.size() <= 128 && s.front() >= 'a' && s.front() <= 'z');
    for (char c : s) require((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-');
}
struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t pos = 0;
    auto take(std::size_t n) {
        require(n <= bytes.size() - pos);
        auto out = bytes.subspan(pos, n); pos += n; return out;
    }
    std::uint64_t integer(unsigned n) {
        std::uint64_t v = 0; auto data = take(n);
        for (unsigned j = 0; j < n; ++j) v |= std::uint64_t(data[j]) << (j * 8);
        return v;
    }
    std::uint32_t u32() { return static_cast<std::uint32_t>(integer(4)); }
    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
    float f32() { auto v = std::bit_cast<float>(u32()); require(std::isfinite(v)); return v; }
    double f64() { auto v = std::bit_cast<double>(integer(8)); require(std::isfinite(v)); return v; }
    std::size_t count(std::size_t cap, std::size_t minimum) {
        auto n = u32(); require(n <= cap && n <= (bytes.size() - pos) / minimum); return n;
    }
    void text(std::pmr::string &s, bool empty = false) {
        auto n = count(512, 1); auto data = take(n);
        std::string_view view{reinterpret_cast<const char *>(data.data()), data.size()}; utf8(view, empty); s = view;
    }
    Pair pair() { const auto x = f32(); return {x, f32()}; }
};
struct Writer {
    std::pmr::vector<std::uint8_t> bytes;
    explicit Writer(Resource &r) : bytes(&r) {}
    void add(std::span<const std::uint8_t> data) {
        require(data.size() <= byte_cap - bytes.size(), ARS_E_OUT_OF_BUDGET);
        bytes.insert(bytes.end(), data.begin(), data.end());
    }
    void integer(std::uint64_t v, unsigned n = 4) {
        std::array<std::uint8_t, 8> data{};
        for (unsigned i = 0; i < n; ++i) data[i] = static_cast<std::uint8_t>(v >> (i * 8));
        add({data.data(), n});
    }
    void f32(float v) { integer(std::bit_cast<std::uint32_t>(v)); }
    void f64(double v) { integer(std::bit_cast<std::uint64_t>(v), 8); }
    void text(std::string_view s) { integer(s.size()); add({reinterpret_cast<const std::uint8_t *>(s.data()), s.size()}); }
    void pair(Pair p) { f32(p.x); f32(p.y); }
};
template<class Values> void unique(const Values &values, Work &work, std::size_t bytes = 512) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        utf8(values[i].id, false, bytes);
        for (std::size_t j = 0; j < i; ++j) { work.take(); require(values[i].id != values[j].id); }
    }
}
void mappings(const ValidatedCpuModel &m, Work &work) {
    // Reconstruct only a typed manifest projection, never encoded JSON.
    auto *memory = m.semantics().get_allocator().resource();
    validation::Document doc(memory);
    auto add = [&](validation::Kind kind, std::string_view key, std::size_t parent = SIZE_MAX) {
        const auto i = doc.nodes.size(); doc.nodes.emplace_back(memory); doc.nodes[i].kind = kind; doc.nodes[i].key = key;
        if (parent != SIZE_MAX) doc.nodes[parent].children.push_back(i); return i;
    };
    auto text = [&](std::string_view key, std::string_view value, std::size_t parent) {
        doc.nodes[add(validation::Kind::string, key, parent)].text = value;
    };
    auto root = add(validation::Kind::object, "");
    auto semantics = add(validation::Kind::array, "rig_semantics", root);
    auto reps = add(validation::Kind::object, "representations", root);
    auto three = add(validation::Kind::object, "3d", reps);
    auto maps = add(validation::Kind::array, "mappings", three);
    for (const auto &s : m.semantics()) {
        auto entry = add(validation::Kind::object, "", semantics);
        text("id", s.id, entry); text("type", s.type, entry); text("units", s.units, entry);
        // The common mapping validator reads default only for scalar morphs.
        doc.nodes[add(validation::Kind::number, "default", entry)].number = s.initial[0];
        if (!s.target3d.empty()) {
            auto map = add(validation::Kind::object, "", maps); text("semantic", s.id, map); text("target", s.target3d, map);
        }
    }
    validation::validate_typed_glb_mappings(doc.root(), *m.glb(), work);
}
void validate(const ValidatedCpuModel &m, const Limits &limits, Work &work, bool full = false) {
    require(full || !m.glb(), ARS_E_UNSUPPORTED);
    const bool two = !m.parts().empty();
    require((two || (full && m.glb())) && m.parts().size() <= 128 && (!two || !m.layers().empty()) && m.layers().size() <= 128);
    if (full) {
        require(m.complete_hierarchy() && m.default_mode() <= 1);
        require(m.default_mode() == 0 ? two : bool(m.glb()));
        for (std::size_t i = 0; i < m.layers().size(); ++i) {
            const auto &l = m.layers()[i]; bool visible = true; float opacity = 1; auto current = static_cast<std::uint32_t>(i);
            for (std::size_t depth = 0; current != UINT32_MAX; ++depth) {
                require(depth < m.layers().size() && current < m.layers().size()); work.take();
                const auto &p = m.layers()[current];
                require(std::isfinite(p.local_opacity) && p.local_opacity >= 0 && p.local_opacity <= 1);
                visible &= p.local_visible; opacity *= p.local_opacity; current = p.parent;
            }
            require(visible == l.visible && opacity == l.opacity);
        }
    }
    require(m.parameters().size() <= 512 && m.semantics().size() <= 512);
    for (auto edge : m.canvas()) require(two ? edge && edge <= limits.edge : edge == 0);
    const auto &image = m.image();
    require(two ? image.width && image.height && image.width <= limits.edge && image.height <= limits.edge : image.width == 0 && image.height == 0 && m.parameters().empty());
    const auto pixels = std::uint64_t(image.width) * image.height * 4;
    require(pixels <= limits.pixels, ARS_E_OUT_OF_BUDGET); require(pixels == image.rgba.size());
    unique(m.layers(), work, 128); unique(m.parts(), work); unique(m.parameters(), work); unique(m.semantics(), work, 128);
    for (const auto &l : m.layers()) { if (two) binding_id(l.id); require(l.z >= -10000 && l.z <= 10000 && std::isfinite(l.opacity) && l.opacity >= 0 && l.opacity <= 1); }
    for (const auto &p : m.parameters()) require(std::isfinite(p.minimum) && std::isfinite(p.maximum) && std::isfinite(p.initial) && p.minimum <= p.initial && p.initial <= p.maximum);
    for (std::size_t i = 0; i < m.semantics().size(); ++i) {
        const auto &s = m.semantics()[i]; canonical(s.id);
        if (i) require(m.semantics()[i - 1].id < s.id);
        require(s.units.empty() || s.units == "normalized" || s.units == "radians" || s.units == "meters" || s.units == "unitless");
        require(full || s.target3d.empty()); utf8(s.target2d, true); utf8(s.target3d, true);
        if (!s.target3d.empty()) require(bool(m.glb()));
        require(std::isfinite(s.minimum) && std::isfinite(s.maximum) && std::abs(s.minimum) <= 3.402823466e38 && std::abs(s.maximum) <= 3.402823466e38);
        if (!s.has_minimum) require(s.minimum == 0);
        if (!s.has_maximum) require(s.maximum == 0);
        if (s.has_minimum && s.has_maximum) require(s.minimum <= s.maximum);
        const unsigned n = s.type == "float" || s.type == "bool" ? 1 : s.type == "vec2" ? 2 : s.type == "vec3" ? 3 : s.type == "quat" ? 4 : 0;
        require(n || s.type == "trigger"); require(s.has_default == (s.type != "trigger"));
        if (s.type == "bool") require(!s.has_minimum && !s.has_maximum && (s.initial[0] == 0 || s.initial[0] == 1));
        double norm = 0;
        for (unsigned j = 0; j < 4; ++j) {
            work.take(); auto v = s.initial[j]; require(std::isfinite(v) && std::abs(v) <= 3.402823466e38);
            if (j >= n) require(v == 0);
            else { if (s.has_minimum) require(v >= s.minimum); if (s.has_maximum) require(v <= s.maximum); norm += v * v; }
        }
        if (s.type == "quat") require(std::abs(norm - 1) <= 0.001);
        if (!s.target2d.empty()) {
            require(s.type == "float"); bool found = false;
            for (const auto &p : m.parameters()) { work.take(); if (s.target2d == p.id) {
                found = true; require(static_cast<float>(s.initial[0]) == p.initial);
                if (s.has_minimum) require(static_cast<float>(s.minimum) == p.minimum);
                if (s.has_maximum) require(static_cast<float>(s.maximum) == p.maximum);
            }}
            require(found);
        }
    }
    std::array<bool, 128> used{}; std::size_t vertices = 0;
    for (const auto &p : m.parts()) {
        binding_id(p.id);
        require(p.z >= -10000 && p.z <= 10000 && p.layer < m.layers().size()); used[p.layer] = true;
        require(p.vertices.size() >= 3 && p.vertices.size() <= 65535 && p.vertices.size() == p.uv.size());
        require(p.vertices.size() <= limits.vertices - vertices, ARS_E_OUT_OF_BUDGET); vertices += p.vertices.size();
        require(!p.triangles.empty() && p.triangles.size() <= 100000 && p.deforms.size() <= 64);
        require(std::isfinite(p.pivot.x) && std::isfinite(p.pivot.y) && p.pivot.x >= 0 && p.pivot.x <= 1 && p.pivot.y >= 0 && p.pivot.y <= 1);
        const auto &r = p.rect;
        require(r[2] && r[3] && r[0] <= image.width && r[1] <= image.height && r[2] <= image.width - r[0] && r[3] <= image.height - r[1]);
        for (const auto *pairs : {&p.vertices, &p.uv}) for (auto v : *pairs) { work.take(); require(std::isfinite(v.x) && std::isfinite(v.y)); }
        for (auto t : p.triangles) { work.take(3); for (auto v : t) require(v < p.vertices.size()); }
        for (std::size_t i = 0; i < p.deforms.size(); ++i) {
            const auto &d = p.deforms[i]; require(d.parameter < m.parameters().size() && d.deltas.size() == p.vertices.size());
            for (std::size_t j = 0; j < i; ++j) { work.take(); require(d.parameter != p.deforms[j].parameter); }
            for (auto v : d.deltas) { work.take(); require(std::isfinite(v.x) && std::isfinite(v.y)); }
        }
    }
    if (two) for (std::size_t i = 0; i < m.layers().size(); ++i) require(used[i]);
    if (full && m.glb()) {
        auto total = pixels;
        for (const auto &im : m.glb()->data().images) { require(im.rgba.size() <= limits.pixels - total, ARS_E_OUT_OF_BUDGET); total += im.rgba.size(); }
        mappings(m, work);
    }
}
void header(Writer &w, ars_status status, Identity id, bool full = false) {
    require(id.nonce != 0, ARS_E_INVALID_ARGUMENT);
    const auto marker = full ? full_magic : magic;
    w.add({reinterpret_cast<const std::uint8_t *>(marker.data()), marker.size()});
    w.integer(1); w.integer(full ? 2 : 1); w.integer(0); w.integer(static_cast<std::uint32_t>(status));
    w.integer(id.nonce, 8); w.add(id.request); Digest zero{}; w.add(zero);
}
void finish(Writer &w) {
    Digest hash{}; auto status = digest(std::span<const std::uint8_t>(w.bytes).subspan(header_bytes), hash); require(status == ARS_OK, status);
    std::copy(hash.begin(), hash.end(), w.bytes.begin() + 64);
    const auto n = w.bytes.size(); for (unsigned j = 0; j < 4; ++j) w.bytes[16 + j] = static_cast<std::uint8_t>(n >> (j * 8));
}
template<class F> ars_status boundary(F &&f) noexcept {
    try { f(); return ARS_OK; } catch (const Failure &e) { return e.status; }
    catch (const std::bad_alloc &) { return ARS_E_OUT_OF_BUDGET; } catch (...) { return ARS_E_INTERNAL; }
}
} // namespace
ars_status digest(std::span<const std::uint8_t> bytes, Digest &out) noexcept {
    if (bytes.size() > byte_cap) return ARS_E_OUT_OF_BUDGET;
    Digest next{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
                   static_cast<ULONG>(bytes.size()), next.data(), static_cast<ULONG>(next.size())) < 0) return ARS_E_INTERNAL;
    out = next; return ARS_OK;
}
ars_status rejection(ars_status status, Identity id, Resource &r, std::pmr::vector<std::uint8_t> &out) noexcept {
    return boundary([&] { require(error_status(status) && out.get_allocator().resource() == &r, ARS_E_INVALID_ARGUMENT);
        Job job(r); Writer w(r); header(w, status, id); finish(w); out.swap(w.bytes); });
}
ars_status encode_impl(const ValidatedCpuModel &m, Identity id, Resource &r, std::pmr::vector<std::uint8_t> &out, bool full) noexcept {
    return boundary([&] {
        require(out.get_allocator().resource() == &r, ARS_E_INVALID_ARGUMENT); Job job(r);
        Work work{Limits{}.work}; validate(m, Limits{}, work, full);
        Writer w(r); header(w, ARS_OK, id, full);
        if (full) { w.integer(m.default_mode()); w.integer(bool(m.glb())); }
        for (auto v : m.canvas()) w.integer(v);
        w.integer(m.image().width); w.integer(m.image().height);
        w.integer(m.layers().size()); w.integer(m.parameters().size()); w.integer(m.parts().size()); w.integer(m.semantics().size()); w.integer(m.image().rgba.size());
        for (const auto &l : m.layers()) { w.text(l.id); w.integer(static_cast<std::uint32_t>(l.z)); w.integer(l.visible); w.f32(l.opacity); if (full) { w.integer(l.parent); w.integer(l.local_visible); w.f32(l.local_opacity); } }
        for (const auto &p : m.parameters()) { w.text(p.id); w.f32(p.minimum); w.f32(p.maximum); w.f32(p.initial); }
        for (const auto &s : m.semantics()) {
            for (std::string_view v : std::array<std::string_view, 4>{s.id, s.type, s.units, s.target2d}) w.text(v);
            if (full) w.text(s.target3d);
            w.integer(unsigned(s.has_minimum) | (unsigned(s.has_maximum) << 1) | (unsigned(s.has_default) << 2));
            for (auto v : s.initial) w.f64(v); w.f64(s.minimum); w.f64(s.maximum);
        }
        for (const auto &p : m.parts()) {
            w.text(p.id); for (auto v : p.rect) w.integer(v); w.pair(p.pivot); w.integer(static_cast<std::uint32_t>(p.z));
            w.integer(p.layer); w.integer(p.vertices.size()); w.integer(p.triangles.size()); w.integer(p.deforms.size());
            for (auto v : p.vertices) w.pair(v); for (auto v : p.uv) w.pair(v);
            for (auto t : p.triangles) for (auto v : t) w.integer(v);
            for (const auto &d : p.deforms) { w.integer(d.parameter); for (auto v : d.deltas) w.pair(v); }
        }
        w.add(m.image().rgba); if (full && m.glb()) write_glb(*m.glb(), w.bytes, work); finish(w); out.swap(w.bytes);
    });
}
ars_status encode(const ValidatedCpuModel &m, Identity id, Resource &r, std::pmr::vector<std::uint8_t> &out) noexcept { return encode_impl(m, id, r, out, false); }
ars_status encode_full(const ValidatedCpuModel &m, Identity id, Resource &r, std::pmr::vector<std::uint8_t> &out) noexcept { return encode_impl(m, id, r, out, true); }
ars_status rejection_full(ars_status status, Identity id, Resource &r, std::pmr::vector<std::uint8_t> &out) noexcept {
    return boundary([&] { require(error_status(status) && out.get_allocator().resource() == &r, ARS_E_INVALID_ARGUMENT);
        Job job(r); Writer w(r); header(w, status, id, true); finish(w); out.swap(w.bytes); });
}
ars_status decode(std::span<const std::uint8_t> bytes, Identity expected, Resource &r,
                  std::shared_ptr<const ValidatedCpuModel> &out, const Limits &limits) noexcept {
    return boundary([&] {
        require(expected.nonce && limits.valid(), ARS_E_INVALID_ARGUMENT); Job job(r);
        require(bytes.size() >= header_bytes && bytes.size() <= byte_cap);
        Reader rd{bytes}; auto marker = rd.take(8); const bool full = std::equal(marker.begin(), marker.end(), full_magic.begin());
        require(full || std::equal(marker.begin(), marker.end(), magic.begin()));
        require(rd.u32() == 1 && rd.u32() == (full ? 2u : 1u) && rd.u32() == bytes.size());
        auto status = static_cast<ars_status>(rd.u32()); require(status == ARS_OK || error_status(status));
        require(rd.integer(8) == expected.nonce); auto request_hash = rd.take(32);
        require(std::equal(request_hash.begin(), request_hash.end(), expected.request.begin()));
        auto payload_hash = rd.take(32); Digest actual{}; auto hashed = digest(bytes.subspan(header_bytes), actual); require(hashed == ARS_OK, hashed);
        require(std::equal(payload_hash.begin(), payload_hash.end(), actual.begin()));
        if (status != ARS_OK) { require(bytes.size() == header_bytes); fail(status); }
        auto m = CpuPacketBuilder::make(r);
        bool has_glb = false;
        if (full) { CpuPacketBuilder::mode(*m) = rd.u32(); auto flags = rd.u32(); require(flags <= 1); has_glb = flags != 0; CpuPacketBuilder::hierarchy(*m) = true; }
        for (auto &v : CpuPacketBuilder::canvas(*m)) v = rd.u32();
        auto &image = CpuPacketBuilder::image(*m); image.width = rd.u32(); image.height = rd.u32();
        auto nl = rd.count(128, 16), np = rd.count(512, 16), nt = rd.count(128, 64), ns = rd.count(512, 68);
        const auto pixels = rd.u32(); require(pixels <= limits.pixels, ARS_E_OUT_OF_BUDGET);
        require(image.width <= limits.edge && image.height <= limits.edge && std::uint64_t(image.width) * image.height * 4 == pixels);
        require((image.width && image.height) || (full && has_glb && !image.width && !image.height && !nt && !np));
        auto &layers = CpuPacketBuilder::layers(*m);
        while (nl--) { auto &l = layers.emplace_back(&r); rd.text(l.id); l.z = rd.i32(); const auto visible = rd.u32(); require(visible <= 1); l.visible = visible != 0; l.opacity = rd.f32(); if (full) { l.parent = rd.u32(); auto local = rd.u32(); require(local <= 1); l.local_visible = local != 0; l.local_opacity = rd.f32(); } }
        auto &params = CpuPacketBuilder::parameters(*m);
        while (np--) { auto &p = params.emplace_back(&r); rd.text(p.id); p.minimum = rd.f32(); p.maximum = rd.f32(); p.initial = rd.f32(); }
        auto &semantics = CpuPacketBuilder::semantics(*m);
        while (ns--) {
            auto &s = semantics.emplace_back(&r); rd.text(s.id); rd.text(s.type); rd.text(s.units, true); rd.text(s.target2d, true); if (full) rd.text(s.target3d, true);
            const auto flags = rd.u32(); require(flags <= 7); s.has_minimum = (flags & 1) != 0; s.has_maximum = (flags & 2) != 0; s.has_default = (flags & 4) != 0;
            for (auto &v : s.initial) v = rd.f64(); s.minimum = rd.f64(); s.maximum = rd.f64();
        }
        auto &parts = CpuPacketBuilder::parts(*m); std::size_t vertices = 0; Work parse_work{limits.work};
        while (nt--) {
            auto &p = parts.emplace_back(&r); rd.text(p.id); for (auto &v : p.rect) v = rd.u32(); p.pivot = rd.pair(); p.z = rd.i32(); p.layer = rd.u32();
            const auto nv = rd.count(65535, 16), triangles = rd.count(100000, 12), deforms = rd.count(64, 4);
            require(nv <= limits.vertices - vertices, ARS_E_OUT_OF_BUDGET); vertices += nv;
            parse_work.take(nv * 2 + triangles * 3 + deforms * nv);
            for (std::size_t i = 0; i < nv; ++i) p.vertices.push_back(rd.pair());
            for (std::size_t i = 0; i < nv; ++i) p.uv.push_back(rd.pair());
            for (std::size_t i = 0; i < triangles; ++i) { const auto a = rd.u32(), b = rd.u32(), c = rd.u32(); p.triangles.push_back({a,b,c}); }
            for (std::size_t i = 0; i < deforms; ++i) { auto &d = p.deforms.emplace_back(&r); d.parameter = rd.u32(); for (std::size_t j = 0; j < nv; ++j) d.deltas.push_back(rd.pair()); }
        }
        auto rgba = rd.take(pixels); image.rgba.assign(rgba.begin(), rgba.end());
        if (has_glb) { auto remaining = limits; remaining.pixels -= pixels; CpuPacketBuilder::glb(*m) = read_glb(bytes, rd.pos, r, remaining, parse_work); }
        require(rd.pos == bytes.size());
        validate(*m, limits, parse_work, full); out = std::move(m);
    });
}
} // namespace packet
} // namespace ars::assets
