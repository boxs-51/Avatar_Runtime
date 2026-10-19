#pragma once
#include "independent_snapshot.hpp"
namespace ars::assets {class Resource;struct Limits;struct Work;}
#include <span>
namespace ars::moc3::admission {
void validate_physics(std::span<const std::uint8_t>,const std::vector<ParameterInfo>&,
                      assets::Resource&,const assets::Limits&,assets::Work&);
int package_worker(bool retained=false,bool simulate=false);
}
