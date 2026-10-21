#include "../../src/assets/cpu_model_packet.hpp"
#include "../../src/rendering/scene2d.hpp"
#include "../../src/rendering/scene3d.hpp"
#include "asset_test_support.hpp"
#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <map>
#include <string>
using namespace ars::assets;
namespace {
struct Host {
    std::map<void *, std::pair<std::uint64_t, std::uint32_t>> records;
    std::size_t calls = 0, fail_at = 0;
    bool throwing = false;
    ~Host() { test::check(records.empty(), "host storage balanced at destruction"); }
    static void *ARS_CALL allocate(void *u, std::uint64_t n, std::uint32_t a) {
        auto &h = *static_cast<Host *>(u); ++h.calls;
        if (h.throwing) throw 42;
        if (h.calls == h.fail_at) return nullptr;
        auto *p = _aligned_malloc(n ? static_cast<std::size_t>(n) : 1, std::max<std::size_t>(a, alignof(void *)));
        if (p) h.records.emplace(p, std::pair{n,a});
        return p;
    }
    static void ARS_CALL release(void *u, void *p, std::uint64_t n, std::uint32_t a) {
        auto &h = *static_cast<Host *>(u); auto it = h.records.find(p);
        test::check(it != h.records.end() && it->second == std::pair{n,a}, "matching bytes/alignment/free exactly once");
        h.records.erase(it); _aligned_free(p);
    }
};
void stress(const std::vector<std::uint8_t> &bytes, packet::Identity id) {
    Host host; Resource r(Host::allocate, Host::release, &host);
    std::shared_ptr<const ValidatedCpuModel> old;
    test::check(packet::decode(bytes,id,r,old)==ARS_OK, "baseline decode");
    const auto allocations=host.calls;
    const auto live=r.live(); const auto records=host.records.size();
    for (std::size_t i=1;i<=allocations;++i) {
        host.fail_at=host.calls+i;
        auto *prior=old.get();
        test::check(packet::decode(bytes,id,r,old)==ARS_E_OUT_OF_BUDGET && old.get()==prior && r.live()==live && host.records.size()==records,"decode OOM transactional at every allocation");
    }
    host.fail_at=0;
    for (std::size_t n=0;n<bytes.size();++n) {
        auto *prior=old.get();
        test::check(packet::decode(std::span<const std::uint8_t>(bytes).first(n),id,r,old)!=ARS_OK && old.get()==prior && r.live()==live,"all truncations preserve old model");
    }
    std::pmr::vector<std::uint8_t> encoded(&r);
    const auto before_encode=host.calls;
    test::check((old->complete_hierarchy() ? packet::encode_full(*old,id,r,encoded) : packet::encode(*old,id,r,encoded))==ARS_OK && std::equal(encoded.begin(),encoded.end(),bytes.begin(),bytes.end()),"canonical encode equality");
    const auto encode_allocs=host.calls-before_encode;
    for (std::size_t i=1;i<=encode_allocs;++i) {
        const auto encoder_live=r.live(); auto copy=std::vector<std::uint8_t>(encoded.begin(),encoded.end()); host.fail_at=host.calls+i;
        test::check((old->complete_hierarchy() ? packet::encode_full(*old,id,r,encoded) : packet::encode(*old,id,r,encoded))==ARS_E_OUT_OF_BUDGET && std::equal(encoded.begin(),encoded.end(),copy.begin(),copy.end()) && r.live()==encoder_live,"encode OOM preserves old bytes");
    }
    host.fail_at=0;
    test::check(r.begin_job(),"reserve resource job");
    test::check(packet::decode(bytes,id,r,old)==ARS_E_BUSY && (old->complete_hierarchy() ? packet::encode_full(*old,id,r,encoded) : packet::encode(*old,id,r,encoded))==ARS_E_BUSY,"busy resource"); r.end_job();
    auto limits=Limits{}; limits.work=1;
    test::check(packet::decode(bytes,id,r,old,limits)==ARS_E_OUT_OF_BUDGET,"lower work cap");
    limits=Limits{}; limits.pixels=1;
    if (!old->image().rgba.empty() || (old->glb() && !old->glb()->data().images.empty()))
        test::check(packet::decode(bytes,id,r,old,limits)==ARS_E_OUT_OF_BUDGET,"lower pixel cap");
    if (old->glb() && !old->parts().empty() && !old->glb()->data().images.empty()) {
        const auto two = old->image().rgba.size(); std::size_t three = 0;
        for (const auto &im : old->glb()->data().images) three += im.rgba.size();
        limits=Limits{}; limits.pixels=std::max(two,three);
        const auto *prior=old.get(); const auto before=r.live();
        test::check(packet::decode(bytes,id,r,old,limits)==ARS_E_OUT_OF_BUDGET && old.get()==prior && r.live()==before,"dual aggregate pixel cap preserves previous model");
    }
    host.throwing=true;
    test::check(packet::decode(bytes,id,r,old)==ARS_E_INTERNAL,"throwing host allocator fenced"); host.throwing=false;
    std::vector<std::uint8_t> changed(bytes); std::shared_ptr<const ValidatedCpuModel> copied;
    test::check(packet::decode(changed,id,r,copied)==ARS_OK,"buffer lifetime decode");
    changed.assign(changed.size(),0); changed.clear(); changed.shrink_to_fit();
    std::pmr::vector<std::uint8_t> after(&r); test::check((copied->complete_hierarchy() ? packet::encode_full(*copied,id,r,after) : packet::encode(*copied,id,r,after))==ARS_OK && std::equal(after.begin(),after.end(),bytes.begin(),bytes.end()),"model owns data after input destroyed");
    if (!old->parts().empty()) {
    ars::rendering::PreparedScene2D a,b;
    test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(*old),a)==ARS_OK && ars::rendering::prepare_static_2d(ars::rendering::scene_input(*copied),b)==ARS_OK && a.draws.size()==b.draws.size(),"prepared geometry after packet destruction");
    }
    if (old->glb()) { ars::rendering::PreparedScene3D a,b; test::check(ars::rendering::prepare_static_3d(*old,a)==ARS_OK && ars::rendering::prepare_static_3d(*copied,b)==ARS_OK && a.draws.size()==b.draws.size(),"3D survives input buffer destruction"); }
    std::printf("STRESS_PASS decode_allocations=%zu encode_allocations=%zu truncations=%zu\n",allocations,encode_allocs,bytes.size());
}
} // namespace
int main(int argc,char **argv) {
    if(argc!=4 && argc!=5)return 2;
    packet::Identity id; const std::string_view nonce=argv[1], hash=argv[2];
    auto parsed=std::from_chars(nonce.data(),nonce.data()+nonce.size(),id.nonce);
    if(parsed.ec!=std::errc{}||parsed.ptr!=nonce.data()+nonce.size()||!id.nonce||hash.size()!=64)return 2;
    for(std::size_t i=0;i<32;++i) {
        unsigned value=0; auto part=hash.substr(i*2,2); auto n=std::from_chars(part.data(),part.data()+2,value,16);
        if(n.ec!=std::errc{}||n.ptr!=part.data()+2)return 2; id.request[i]=static_cast<std::uint8_t>(value);
    }
    _setmode(_fileno(stdin),_O_BINARY); std::vector<std::uint8_t> bytes;
    for(int c;(c=std::getchar())!=EOF;) { if(bytes.size()==packet::byte_cap)return 2; bytes.push_back(static_cast<std::uint8_t>(c)); }
    if(std::string_view(argv[3])=="--stress") { stress(bytes,id); return 0; }
    Host host; ars_status status=ARS_E_INTERNAL;
    {
        Resource resource(Host::allocate,Host::release,&host);
        std::shared_ptr<const ValidatedCpuModel> model;
        if(argc==5) {
            if(std::string_view(argv[3])!="--retain")return 2;
            auto baseline=test::file(argv[4]);
            test::check(packet::decode(baseline,id,resource,model)==ARS_OK,"retained baseline");
            const auto *prior=model.get(); const auto live=resource.live(); const auto records=host.records.size();
            status=packet::decode(bytes,id,resource,model);
            test::check(status!=ARS_OK && model.get()==prior && resource.live()==live && host.records.size()==records,"hostile packet preserves prior model/storage");
        } else status=packet::decode(bytes,id,resource,model);
        if(status==ARS_OK) {
            std::pmr::vector<std::uint8_t> output(&resource);
            test::check((model->complete_hierarchy() ? packet::encode_full(*model,id,resource,output) : packet::encode(*model,id,resource,output))==ARS_OK && std::equal(output.begin(),output.end(),bytes.begin(),bytes.end()),"complete canonical roundtrip");
            if (!model->parts().empty()) {
                ars::rendering::PreparedScene2D scene;
                test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(*model),scene)==ARS_OK,"independent decoded model prepares geometry");
            }
            if (model->glb()) {
                ars::rendering::PreparedScene3D scene;
                test::check(ars::rendering::prepare_static_3d(*model,scene)==ARS_OK,"typed GLB prepares 3D geometry");
            }
        }
    }
    test::check(host.records.empty(),"all host allocations retired");
    std::printf("{\"status\":%d,\"allocations\":%zu,\"balanced\":true}\n",status,host.calls);
    return status==ARS_OK?0:3;
}
