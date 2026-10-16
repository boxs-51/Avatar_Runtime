#include "png_decoder.hpp"
#include <cstring>
#include <png.h>
#include <zlib.h>
namespace ars::assets {
namespace {
struct alignas(16) Header {
    std::size_t bytes, reserved;
};
struct State {
    BoundedMemory memory;
    std::span<const std::uint8_t> input;
    Image *image;
    std::size_t at = 0;
    png_structp png = nullptr;
    png_infop info = nullptr;
    bool oom = false, internal = false;
    State(Resource &r, std::span<const std::uint8_t> bytes, Image &out)
        : memory(&r, 64 * MiB), input(bytes), image(&out) {}
};
void *allocate(png_structp png, png_alloc_size_t n) noexcept {
    auto &s = *static_cast<State *>(png_get_mem_ptr(png));
    try {
        require(n <= SIZE_MAX - sizeof(Header), ARS_E_OUT_OF_BUDGET);
        auto *h = static_cast<Header *>(s.memory.allocate(n + sizeof(Header), 16));
        h->bytes = n + sizeof(Header);
        return h + 1;
    } catch (const Failure &f) {
        s.internal = f.status == ARS_E_INTERNAL;
        s.oom = !s.internal;
    } catch (...) {
        s.oom = true;
    }
    return nullptr;
}
void release(png_structp png, void *p) noexcept {
    if (p) {
        auto *h = static_cast<Header *>(p) - 1;
        static_cast<State *>(png_get_mem_ptr(png))->memory.deallocate(h, h->bytes, 16);
    }
}
void error(png_structp png, const char *) { png_longjmp(png, 1); }
void warning(png_structp, const char *) {}
void read(png_structp png, png_bytep out, png_size_t n) {
    auto &s = *static_cast<State *>(png_get_io_ptr(png));
    if (n > s.input.size() - s.at)
        png_error(png, "truncated");
    std::memcpy(out, s.input.data() + s.at, n);
    s.at += n;
}
#ifdef _MSC_VER
#pragma warning(push)
// This setjmp frame has only scalar locals. Every owning C++ object is in the
// caller's State/Image and is destroyed after this function returns, never skipped.
#pragma warning(disable : 4611)
#endif
bool decode(State *s, std::size_t edge) {
    if (setjmp(png_jmpbuf(s->png)))
        return false;
    png_set_read_fn(s->png, s, read);
    png_set_user_limits(s->png, static_cast<png_uint_32>(edge), static_cast<png_uint_32>(edge));
    png_set_chunk_malloc_max(s->png, 65536);
    png_set_chunk_cache_max(s->png, 128);
    png_set_crc_action(s->png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
    png_read_info(s->png, s->info);
    const auto color = png_get_color_type(s->png, s->info), depth = png_get_bit_depth(s->png, s->info);
    if (depth == 16)
        png_set_strip_16(s->png);
    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(s->png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(s->png);
    const bool transparency = png_get_valid(s->png, s->info, PNG_INFO_tRNS) != 0;
    if (transparency)
        png_set_tRNS_to_alpha(s->png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(s->png);
    if (!(color & PNG_COLOR_MASK_ALPHA) && !transparency)
        png_set_add_alpha(s->png, 255, PNG_FILLER_AFTER);
    const int passes = png_set_interlace_handling(s->png);
    png_read_update_info(s->png, s->info);
    if (png_get_rowbytes(s->png, s->info) != std::size_t(s->image->width) * 4)
        png_error(s->png, "row bounds");
    for (int pass = 0; pass < passes; ++pass)
        for (std::uint32_t y = 0; y < s->image->height; ++y)
            png_read_row(s->png, s->image->rgba.data() + std::size_t(y) * s->image->width * 4, nullptr);
    png_read_end(s->png, s->info);
    return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
std::uint32_t be(std::span<const std::uint8_t> b, std::size_t at) {
    require(at <= b.size() && b.size() - at >= 4);
    return std::uint32_t(b[at]) << 24 | std::uint32_t(b[at + 1]) << 16 | std::uint32_t(b[at + 2]) << 8 |
           b[at + 3];
}
} // namespace
void decode_png(std::span<const std::uint8_t> input, Image &out, Resource &resource, const Limits &limits,
                Work &work) {
    static constexpr std::uint8_t signature[]{137, 80, 78, 71, 13, 10, 26, 10};
    require(input.size() >= 33 && std::memcmp(input.data(), signature, 8) == 0);
    std::size_t at = 8, count = 0;
    bool ended = false, data = false, after_data = false;
    while (at < input.size()) {
        work.take();
        require(input.size() - at >= 12);
        const auto n = be(input, at), type = be(input, at + 4);
        require(n <= input.size() - at - 12);
        require(++count <= 4096, ARS_E_OUT_OF_BUDGET);
        if (count == 1)
            require(type == 0x49484452 && n == 13);
        else
            require(type != 0x49484452);
        if (type != 0x49444154)
            require(n <= 65536, ARS_E_OUT_OF_BUDGET);
        require(crc32(0, input.data() + at + 4, n + 4) == be(input, at + 8 + n));
        if (type == 0x49444154) {
            require(!after_data);
            data = true;
        } else if (data)
            after_data = true;
        at += std::size_t(n) + 12;
        if (type == 0x49454e44) {
            require(n == 0 && data && at == input.size());
            ended = true;
            break;
        }
    }
    require(ended);
    out.width = be(input, 16);
    out.height = be(input, 20);
    require(out.width && out.height);
    require(out.width <= limits.edge && out.height <= limits.edge, ARS_E_OUT_OF_BUDGET);
    const auto bytes = std::uint64_t(out.width) * out.height * 4;
    require(bytes <= limits.pixels, ARS_E_OUT_OF_BUDGET);
    work.take(static_cast<std::size_t>(bytes / 64 + 1));
    out.rgba.resize(static_cast<std::size_t>(bytes));
    State state(resource, input, out);
    state.png =
        png_create_read_struct_2(PNG_LIBPNG_VER_STRING, nullptr, error, warning, &state, allocate, release);
    if (state.png)
        state.info = png_create_info_struct(state.png);
    bool ok = false;
    if (state.png && state.info)
        ok = decode(&state, limits.edge);
    if (state.png)
        png_destroy_read_struct(&state.png, &state.info, nullptr);
    require(!state.internal, ARS_E_INTERNAL);
    require(!state.oom, ARS_E_OUT_OF_BUDGET);
    require(ok);
}
} // namespace ars::assets
