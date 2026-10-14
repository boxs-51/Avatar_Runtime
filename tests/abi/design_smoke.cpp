/* G00 C++20 compatibility layout smoke, no runtime SDK implementation. */
#include "ars/ars.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>
static_assert(sizeof(ars_handle_id)==16);
static_assert(sizeof(ars_runtime_h)==16);
static_assert(sizeof(ars_ingress_cap)==16);
static_assert(std::is_standard_layout_v<ars_runtime_desc>);
static_assert(std::is_standard_layout_v<ars_render_target>);
static_assert(std::is_same_v<ars_status,std::int32_t>);
static_assert(offsetof(ars_capabilities,struct_size)==0);
static_assert(offsetof(ars_capabilities,abi_version)==4);
int main() {
  ars_runtime_h h{};
  ars_capabilities caps{};
  caps.struct_size=sizeof(caps);
  caps.abi_version=ARS_ABI_VERSION;
  return ars_runtime_get_capabilities(h,&caps)==ARS_E_UNSUPPORTED
         && caps.feature_bits==0 ? 0:1;
}
