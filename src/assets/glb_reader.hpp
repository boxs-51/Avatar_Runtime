#pragma once
#include "asset_limits.hpp"
#include <memory>
#include <span>
namespace ars::assets {
class GlbModel;
// Private CPU-only synchronous ingestion. Result owns every published value.
ars_status ingest_glb(std::span<const std::uint8_t>, Resource &,
                      std::shared_ptr<const GlbModel> &, const Limits & = {}) noexcept;
}
