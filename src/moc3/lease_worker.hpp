#pragma once
#include "lease_protocol.hpp"
#include "retained_physics.hpp"
namespace ars::moc3::admission {
int lease_worker(Session&,RetainedPhysics&,const AtlasSet&,const Digest&,std::uint64_t,std::uint32_t);
}
