#include "zip_reader.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <zlib.h>
namespace ars::assets {
namespace {
struct Bytes {
    std::span<const std::uint8_t> data;
    void bounds(std::size_t p, std::size_t n) const { require(p <= data.size() && n <= data.size() - p); }
    std::uint16_t u16(std::size_t p) const {
        bounds(p, 2);
        return std::uint16_t(data[p]) | std::uint16_t(data[p + 1]) << 8;
    }
    std::uint32_t u32(std::size_t p) const {
        bounds(p, 4);
        return std::uint32_t(u16(p)) | std::uint32_t(u16(p + 2)) << 16;
    }
};
struct alignas(16) Allocation {
    std::size_t size, reserved;
};
struct InflateMemory {
    Resource *r;
    bool exhausted = false, internal = false;
};
voidpf allocate(voidpf context, uInt n, uInt size) noexcept {
    auto &m = *static_cast<InflateMemory *>(context);
    try {
        require(size == 0 || n <= SIZE_MAX / size, ARS_E_OUT_OF_BUDGET);
        const auto bytes = std::size_t(n) * size;
        require(bytes <= SIZE_MAX - sizeof(Allocation), ARS_E_OUT_OF_BUDGET);
        auto *p = static_cast<Allocation *>(m.r->allocate(bytes + sizeof(Allocation), 16));
        p->size = bytes + sizeof(Allocation);
        return p + 1;
    } catch (const Failure &f) {
        m.internal = f.status == ARS_E_INTERNAL;
        m.exhausted = !m.internal;
    } catch (...) {
        m.exhausted = true;
    }
    return nullptr;
}
void release(voidpf context, voidpf pointer) noexcept {
    if (pointer) {
        auto *p = static_cast<Allocation *>(pointer) - 1;
        static_cast<InflateMemory *>(context)->r->deallocate(p, p->size, 16);
    }
}
void extra(const Bytes &b, std::size_t p, std::size_t size, Work &work, bool &disabled) {
    work.take(size);
    const auto end = p + size;
    while (p < end) {
        require(end - p >= 4);
        const auto id = b.u16(p), n = b.u16(p + 2);
        p += 4;
        require(n <= end - p);
        if (id == 0x0001) {
            require(n == 8 || n == 16 || n == 24 || n == 28);
            disabled = true;
        }
        require(id != 0x7075);
        p += n;
    }
}
} // namespace
void read_zip(std::span<const std::uint8_t> input, OwnedBundle &out, Resource &resource, const Limits &limits,
              Work &work) {
    require(limits.valid(), ARS_E_INVALID_ARGUMENT);
    require(input.size() <= limits.compressed, ARS_E_OUT_OF_BUDGET);
    require(input.size() >= 22);
    out.input.assign(input.begin(), input.end());
    out.members.clear();
    Bytes b{out.input};
    const auto length = b.data.size();
    std::size_t end = length;
    bool found = false;
    const auto earliest = length > 65557 ? length - 65557 : 0;
    for (std::size_t p = length - 22;; --p) {
        work.take();
        if (b.u32(p) == 0x06054b50 && b.u16(p + 20) == length - p - 22) {
            end = p;
            found = true;
            break;
        }
        if (p == earliest)
            break;
    }
    require(found && b.u16(end + 4) == 0 && b.u16(end + 6) == 0 && b.u16(end + 8) == b.u16(end + 10));
    const auto count = b.u16(end + 10);
    require(count <= limits.entries, ARS_E_OUT_OF_BUDGET);
    const auto central = b.u32(end + 16), central_size = b.u32(end + 12);
    b.bounds(central, central_size);
    require(std::size_t(central) + central_size == end);
    out.members.reserve(count);
    std::size_t at = central, expanded = 0;
    bool disabled = false;
    for (std::uint32_t i = 0; i < count; ++i) {
        work.take();
        b.bounds(at, 46);
        require(b.u32(at) == 0x02014b50);
        const auto name_size = b.u16(at + 28), extra_size = b.u16(at + 30), comment_size = b.u16(at + 32);
        b.bounds(at + 46, std::size_t(name_size) + extra_size + comment_size);
        require(at + 46 + name_size + extra_size + comment_size <= end && b.u16(at + 34) == 0);
        out.members.emplace_back(&resource);
        auto &m = out.members.back();
        m.flags = b.u16(at + 8);
        m.method = b.u16(at + 10);
        m.crc = b.u32(at + 16);
        m.packed = b.u32(at + 20);
        m.size = b.u32(at + 24);
        m.offset = b.u32(at + 42);
        require((m.flags & ~std::uint16_t(0x080e)) == 0);
        require(m.method == 0 || m.method == 8);
        require(m.method == 8 || (m.flags & 6) == 0);
        const auto external = b.u32(at + 38);
        require(((external >> 16) & 0170000) != 0120000);
        require((external & 0x400) == 0);
        m.name.assign(reinterpret_cast<const char *>(b.data.data() + at + 46), name_size);
        m.directory = !m.name.empty() && m.name.back() == '/';
        if (!(m.flags & 0x0800))
            for (unsigned char c : m.name)
                require(c < 128);
        m.alias = canonical_path(m.name, &resource, limits);
        for (std::size_t j = 0; j + 1 < out.members.size(); ++j) {
            work.take();
            require(out.members[j].alias != m.alias);
        }
        require(m.size <= limits.expanded - expanded, ARS_E_OUT_OF_BUDGET);
        expanded += m.size;
        require(std::uint64_t(m.size) <= std::uint64_t(std::max(m.packed, 1u)) * limits.ratio,
                ARS_E_OUT_OF_BUDGET);
        if (m.directory)
            require(m.size == 0 && m.packed == 0);
        extra(b, at + 46 + name_size, extra_size, work, disabled);
        at += 46 + name_size + extra_size + comment_size;
    }
    require(at == end);
    std::pmr::vector<std::size_t> order(&resource);
    order.resize(count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](auto a, auto c) { return out.members[a].offset < out.members[c].offset; });
    std::size_t next = 0;
    for (auto index : order) {
        auto &m = out.members[index];
        const auto p = std::size_t(m.offset);
        require(p == next);
        b.bounds(p, 30);
        require(b.u32(p) == 0x04034b50 && b.u16(p + 6) == m.flags && b.u16(p + 8) == m.method);
        const auto names = b.u16(p + 26), extras = b.u16(p + 28);
        b.bounds(p + 30, std::size_t(names) + extras);
        require(names == m.name.size() && std::memcmp(b.data.data() + p + 30, m.name.data(), names) == 0);
        extra(b, p + 30 + names, extras, work, disabled);
        if (m.flags & 8) {
            require((b.u32(p + 14) == 0 || b.u32(p + 14) == m.crc) &&
                    (b.u32(p + 18) == 0 || b.u32(p + 18) == m.packed) &&
                    (b.u32(p + 22) == 0 || b.u32(p + 22) == m.size));
        } else
            require(b.u32(p + 14) == m.crc && b.u32(p + 18) == m.packed && b.u32(p + 22) == m.size);
        const auto data = p + 30 + names + extras;
        b.bounds(data, m.packed);
        next = data + m.packed;
        require(next <= central);
        if (m.flags & 8) {
            if (b.u32(next) == 0x08074b50)
                next += 4;
            b.bounds(next, 12);
            require(b.u32(next) == m.crc && b.u32(next + 4) == m.packed && b.u32(next + 8) == m.size);
            next += 12;
            require(next <= central);
        }
    }
    require(next == central);
    // Finish every bounded envelope/name/metadata check before dispatching a
    // recognized disabled ZIP64 extension. No 64-bit value is narrowed/decoded.
    require(!disabled, ARS_E_UNSUPPORTED);
    for (auto &m : out.members) {
        const auto p = std::size_t(m.offset);
        const auto data = p + 30 + b.u16(p + 26) + b.u16(p + 28);
        m.bytes.resize(m.size);
        work.take(m.packed);
        if (m.method == 0) {
            require(m.packed == m.size);
            std::copy_n(b.data.data() + data, m.size, m.bytes.data());
        } else {
            InflateMemory memory{&resource};
            z_stream stream{};
            stream.zalloc = allocate;
            stream.zfree = release;
            stream.opaque = &memory;
            const auto init = inflateInit2(&stream, -MAX_WBITS);
            require(init == Z_OK, memory.internal    ? ARS_E_INTERNAL
                                  : memory.exhausted ? ARS_E_OUT_OF_BUDGET
                                                     : ARS_E_BAD_ASSET);
            struct Guard {
                z_stream *s;
                ~Guard() { inflateEnd(s); }
            } guard{&stream};
            std::uint8_t sentinel = 0;
            stream.next_in = const_cast<Bytef *>(b.data.data() + data);
            stream.avail_in = m.packed;
            stream.next_out = m.size ? m.bytes.data() : &sentinel;
            stream.avail_out = m.size ? m.size : 1;
            const auto status = inflate(&stream, Z_FINISH);
            require(!memory.internal, ARS_E_INTERNAL);
            require(!memory.exhausted, ARS_E_OUT_OF_BUDGET);
            require(status == Z_STREAM_END && stream.total_out == m.size && stream.total_in == m.packed);
        }
        require(crc32(0, m.bytes.data(), static_cast<uInt>(m.bytes.size())) == m.crc);
    }
}
} // namespace ars::assets
