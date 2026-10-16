#include "glb_reader.hpp"
#include "../validation/glb_validator.hpp"
namespace ars::assets {
namespace {
std::uint32_t u32(std::span<const std::uint8_t> bytes, std::size_t at) {
    require(at <= bytes.size() && bytes.size() - at >= 4);
    return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 | std::uint32_t(bytes[at + 2]) << 16 |
           std::uint32_t(bytes[at + 3]) << 24;
}
} // namespace
std::shared_ptr<GlbParseArena> make_glb_arena(Resource &resource) {
    return std::allocate_shared<GlbParseArena>(std::pmr::polymorphic_allocator<GlbParseArena>(&resource),
                                               resource);
}
class GlbBuilder {
  public:
    static std::shared_ptr<const GlbModel> build(std::span<const std::uint8_t> input, Resource &r,
                                                 const Limits &limits, Work &work,
                                                 std::shared_ptr<GlbParseArena> arena) {
        require(input.size() <= limits.expanded, ARS_E_OUT_OF_BUDGET);
        require(input.size() >= 20 && u32(input, 0) == 0x46546c67 && u32(input, 4) == 2 &&
                u32(input, 8) == input.size());
        if (!arena)
            arena = make_glb_arena(r);
        require(arena->owner() == &r, ARS_E_INVALID_ARGUMENT);
        auto *memory = r.allocate(sizeof(GlbModel), alignof(GlbModel));
        auto *raw = new (memory) GlbModel(r, std::move(arena));
        std::shared_ptr<GlbModel> model(
            raw,
            [&r](auto *p) {
                p->~GlbModel();
                r.deallocate(p, sizeof(GlbModel), alignof(GlbModel));
            },
            std::pmr::polymorphic_allocator<std::byte>(&r));
        std::span<const std::uint8_t> binary;
        std::size_t at = 12, chunks = 0;
        bool json = false, bin = false;
        while (at < input.size()) {
            work.take();
            require(++chunks <= 65536, ARS_E_OUT_OF_BUDGET);
            require(input.size() - at >= 8);
            const auto size = u32(input, at), type = u32(input, at + 4);
            require(size % 4 == 0 && size <= input.size() - at - 8);
            at += 8;
            if (type == 0x4e4f534a) {
                require(chunks == 1 && !json && size > 0);
                require(size <= limits.json, ARS_E_OUT_OF_BUDGET);
                validation::parse_json({reinterpret_cast<const char *>(input.data() + at), size},
                                       model->json_, limits, work);
                json = true;
            } else if (type == 0x004e4942) {
                require(json && chunks == 2 && !bin);
                binary = input.subspan(at, size);
                bin = true;
            } else
                require(json); // Unknown chunk types are safely ignored, as glTF requires.
            at += size;
        }
        require(json && at == input.size());
        model->disabled_ = validation::validate_glb(model->json_, binary, model->data_, r, limits, work);
        return model;
    }
};
std::shared_ptr<const GlbModel> prepare_glb(std::span<const std::uint8_t> input, Resource &r,
                                            const Limits &limits, Work &work,
                                            std::shared_ptr<GlbParseArena> arena) {
    return GlbBuilder::build(input, r, limits, work, std::move(arena));
}
ars_status ingest_glb(std::span<const std::uint8_t> input, Resource &resource,
                      std::shared_ptr<const GlbModel> &out, const Limits &limits) noexcept {
    out.reset();
    if (!resource.begin_job())
        return ARS_E_BUSY;
    struct Guard {
        Resource &r;
        ~Guard() { r.end_job(); }
    } guard{resource};
    try {
        require(limits.valid(), ARS_E_INVALID_ARGUMENT);
        Work work{limits.work};
        auto model = prepare_glb(input, resource, limits, work);
        require(!model->requires_disabled(), ARS_E_UNSUPPORTED);
        out = std::move(model);
        return ARS_OK;
    } catch (const Failure &error) {
        return error.status;
    } catch (const std::bad_alloc &) {
        return ARS_E_OUT_OF_BUDGET;
    } catch (...) {
        return ARS_E_INTERNAL;
    }
}
} // namespace ars::assets
