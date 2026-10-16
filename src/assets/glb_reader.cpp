#include "glb_reader.hpp"
namespace ars::assets {
ars_status ingest_glb(std::span<const std::uint8_t>, Resource &,
                      std::shared_ptr<const GlbModel> &out, const Limits &) noexcept {
    out.reset();
    return ARS_E_UNSUPPORTED;
}
}
