#pragma once
#include "../../src/core/render_frame.hpp"
#include "../assets/asset_test_support.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
namespace ars::testing39 {
inline void check(ars_status expected,ars_status actual,const char *what){if(expected!=actual){
    std::printf("FAIL %s expected=%d actual=%d\n",what,expected,actual);std::exit(1);}}
inline void near(double a,double b,const char *what){test::check(std::abs(a-b)<1e-5,what);}
inline ars_motion_channel channel(const char *id,std::array<float,4> value,std::uint32_t kind=ARS_VALUE_SCALAR){
    ars_motion_channel c{};c.semantic_id={id,static_cast<std::uint32_t>(std::strlen(id))};c.kind=kind;
    std::copy(value.begin(),value.end(),c.value);return c;
}
struct Fixture {
    ars_runtime_h runtime{};ars_model_h model{};ars_character_h character{};
    ars_ingress_cap cap{};std::uint64_t time=1,sequence=0;
    explicit Fixture(const std::filesystem::path &path){
        auto bytes=test::file(path);ars_runtime_desc d{};d.struct_size=sizeof(d);d.abi_version=ARS_ABI_VERSION;
        check(ARS_OK,ars_runtime_create(&d,&runtime),"numeric runtime");
        ars_asset_source s{sizeof(s),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};
        check(ARS_OK,ars_model_load(runtime,&s,&model),"generated numeric fixture admission");
        ars_character_desc cd{sizeof(cd),ARS_ABI_VERSION,ARS_MODE_2D,0};
        check(ARS_OK,ars_character_create(runtime,model,&cd,&character),"numeric character");
        check(ARS_OK,ars_runtime_update(runtime,time),"actual neutral CPU commit");
        auto frame=capture();std::vector<ars_utf8_view> ids;
        for(const auto &s2:frame->model()->semantics())ids.push_back({s2.id.data(),static_cast<std::uint32_t>(s2.id.size())});
        ars_source_desc source{};source.struct_size=sizeof(source);source.abi_version=ARS_ABI_VERSION;
        source.allowed_channel_ids=ids.data();source.allowed_channel_count=static_cast<std::uint32_t>(ids.size());
        source.max_channels_per_frame=source.allowed_channel_count;source.max_frames_per_second=240;
        source.lease_ttl_ns=UINT64_C(3600000000000);ars_source_admin_h admin{};
        check(ARS_OK,ars_source_register(runtime,character,&source,&admin,&cap),"numeric all-channel authority");
    }
    std::unique_ptr<core::RenderFrameLease> capture(){auto f=std::make_unique<core::RenderFrameLease>();
        check(ARS_OK,core::global_router().capture_render(runtime,character,f.get()),"capture actual producer");return f;}
    void submit(std::initializer_list<ars_motion_channel> channels){
        ars_motion_frame f{};f.struct_size=sizeof(f);f.abi_version=ARS_ABI_VERSION;f.sequence=++sequence;
        f.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;f.channels=channels.begin();
        f.channel_count=static_cast<std::uint32_t>(channels.size());f.confidence=1;
        check(ARS_OK,ars_motion_submit(runtime,cap,&f),"actual motion ingress");
        check(ARS_OK,ars_runtime_update(runtime,++time),"actual dynamic CPU commit");
    }
    ~Fixture(){const auto closing=ars_runtime_begin_close(runtime);test::check(closing==ARS_OK||closing==ARS_E_CLOSED,"numeric close");
        check(ARS_OK,ars_runtime_finish_close(runtime),"numeric custody fully released");}
};
}
