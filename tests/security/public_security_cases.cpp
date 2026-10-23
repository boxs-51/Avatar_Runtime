// Linked public ABI security scenarios. No private runtime or test hooks.
#include <ars/ars.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <malloc.h>
#include <random>
#include <string>
#include <thread>
#include <vector>
#define REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL line=%d assertion=%s\n",__LINE__,#x);std::exit(1);} } while(0)
namespace {
struct Allocator {
    std::map<void*,std::pair<uint64_t,uint32_t>> live;
    uint64_t bytes=0,peak=0;
    static void *ARS_CALL allocate(void *user,uint64_t n,uint32_t a){
        auto &s=*static_cast<Allocator*>(user);
        auto *p=_aligned_malloc(static_cast<size_t>(n?n:1),a<sizeof(void*)?sizeof(void*):a);
        if(p){REQUIRE(s.live.emplace(p,std::pair{n,a}).second);s.bytes+=n;if(s.bytes>s.peak)s.peak=s.bytes;}
        return p;
    }
    static void ARS_CALL release(void *user,void *p,uint64_t n,uint32_t a){
        auto &s=*static_cast<Allocator*>(user);auto it=s.live.find(p);
        REQUIRE(it!=s.live.end());REQUIRE(it->second==std::make_pair(n,a));
        REQUIRE(s.bytes>=n);s.bytes-=n;s.live.erase(it);_aligned_free(p);
    }
};
struct Fixture {
    Allocator allocator;ars_runtime_h runtime{};ars_model_h model{};
    explicit Fixture(const std::vector<uint8_t> &bytes){
        ars_runtime_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;
        d.alloc=Allocator::allocate;d.free_fn=Allocator::release;d.allocator_user_data=&allocator;
        REQUIRE(ars_runtime_create(&d,&runtime)==ARS_OK);
        ars_capabilities caps{};caps.struct_size=sizeof caps;caps.abi_version=ARS_ABI_VERSION;
        REQUIRE(ars_runtime_get_capabilities(runtime,&caps)==ARS_OK);
        REQUIRE(caps.feature_bits==ARS_CAP_HOST_SCOPED_MOTION_INGRESS);
        ars_asset_source source{sizeof(source),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};
        REQUIRE(ars_model_load(runtime,&source,&model)==ARS_OK);
    }
    ars_character_h character(){
        ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{};
        REQUIRE(ars_character_create(runtime,model,&d,&c)==ARS_OK);return c;
    }
    void close(){
        REQUIRE(ars_model_release(runtime,model)==ARS_OK);
        REQUIRE(ars_runtime_begin_close(runtime)==ARS_OK);
        ars_status status=ARS_E_BUSY;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(status==ARS_E_BUSY && std::chrono::steady_clock::now()<end){
            status=ars_runtime_finish_close(runtime);
            if(status==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(status==ARS_OK);REQUIRE(allocator.live.empty());REQUIRE(allocator.bytes==0);
        runtime={};
    }
    ~Fixture(){if(runtime.id.runtime_uid)close();}
};
void stale(ars_runtime_h runtime,ars_character_h c){REQUIRE(ars_character_destroy(runtime,c)==ARS_E_STALE_HANDLE);}
void hdl(const std::string &id,const std::vector<uint8_t> &bytes){
    if(id=="HDL-004"){
        Fixture old(bytes);auto a=old.character();const auto prior=old.runtime;
        REQUIRE(ars_character_destroy(prior,a)==ARS_OK);old.close();
        Fixture next(bytes);auto b=next.character();
        REQUIRE(prior.id.runtime_uid!=next.runtime.id.runtime_uid);
        stale(next.runtime,a);stale(prior,b);
        REQUIRE(ars_character_set_mode(next.runtime,b,ARS_MODE_2D)==ARS_OK);
        REQUIRE(ars_character_destroy(next.runtime,b)==ARS_OK);
        std::puts("TRACE runtime_uid differs; old runtime/character rejected; new character unaffected");return;
    }
    Fixture f(bytes);auto a=f.character();
    if(id=="HDL-001"){
        REQUIRE(ars_character_destroy(f.runtime,a)==ARS_OK);stale(f.runtime,a);
        REQUIRE(ars_character_set_mode(f.runtime,a,ARS_MODE_2D)==ARS_E_STALE_HANDLE);
        std::puts("TRACE logical destroy then stale destroy/mode; no crash; sanitizers NOT_RUN");
    }else if(id=="HDL-002"){
        REQUIRE(ars_character_destroy(f.runtime,a)==ARS_OK);auto b=f.character();
        REQUIRE(a.id.slot==b.id.slot);REQUIRE(a.id.generation!=b.id.generation);stale(f.runtime,a);
        REQUIRE(ars_character_set_mode(f.runtime,b,ARS_MODE_2D)==ARS_OK);
        REQUIRE(ars_runtime_update(f.runtime,1)==ARS_OK);REQUIRE(ars_character_destroy(f.runtime,b)==ARS_OK);
        std::puts("TRACE same slot, changed generation, A stale, B mode/update succeeds");
    }else if(id=="HDL-003"){
        ars_character_h forged{f.model.id};stale(f.runtime,forged);
        REQUIRE(ars_character_set_mode(f.runtime,forged,ARS_MODE_2D)==ARS_E_STALE_HANDLE);
        REQUIRE(ars_character_set_mode(f.runtime,a,ARS_MODE_2D)==ARS_OK);
        REQUIRE(ars_character_destroy(f.runtime,a)==ARS_OK);
        std::puts("TRACE model ID rejected as character; real character unaffected");
    }else if(id=="HDL-012"){
        std::array<ars_character_h,3> bad{};bad[1]=a;bad[1].id.runtime_uid^=UINT64_C(0x8000000000000000);
        bad[2]=a;bad[2].id.slot=UINT32_MAX;
        for(auto c:bad){
            const auto destroy=ars_character_destroy(f.runtime,c),mode=ars_character_set_mode(f.runtime,c,ARS_MODE_2D);
            REQUIRE(destroy==ARS_E_STALE_HANDLE || destroy==ARS_E_INVALID_ARGUMENT);
            REQUIRE(mode==ARS_E_STALE_HANDLE || mode==ARS_E_INVALID_ARGUMENT);
            std::printf("TRACE malformed handle destroy=%d mode=%d\n",destroy,mode);
        }
        REQUIRE(ars_character_set_mode(f.runtime,a,ARS_MODE_2D)==ARS_OK);
        REQUIRE(ars_character_destroy(f.runtime,a)==ARS_OK);
        std::puts("TRACE zero, foreign runtime_uid, UINT32_MAX slot rejected; valid handle unaffected");
    }else if(id=="HDL-014"){
        std::array<ars_character_h,16> live{};live[0]=a;
        for(size_t i=1;i<live.size();++i)live[i]=f.character();
        // Warm one full reuse wave before freezing the resource bound.
        for(auto &c:live){REQUIRE(ars_character_destroy(f.runtime,c)==ARS_OK);c=f.character();}
        const auto bound=f.allocator.peak;const auto sentinel=live[0];std::mt19937 rng(42);
        for(unsigned i=0;i<100000;++i){
            auto &c=live[rng()%live.size()];auto old=c;
            REQUIRE(ars_character_destroy(f.runtime,c)==ARS_OK);stale(f.runtime,old);c=f.character();
            REQUIRE(c.id.slot==old.id.slot);REQUIRE(c.id.generation>old.id.generation);
            REQUIRE(f.allocator.peak<=bound);
        }
        stale(f.runtime,sentinel);
        for(auto c:live)REQUIRE(ars_character_destroy(f.runtime,c)==ARS_OK);
        std::printf("TRACE seed=42 cycles=100000 live_width=16 bounded_peak=%llu no ABA; final cleanup follows\n",static_cast<unsigned long long>(bound));
    }else REQUIRE(false);
}
void source_case(const std::string &id,const std::vector<uint8_t> &bytes){
    Fixture f(bytes);auto c=f.character();ars_utf8_view allowed{"mouth.open",10};
    ars_source_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;
    d.max_channels_per_frame=1;d.max_frames_per_second=240;d.lease_ttl_ns=UINT64_C(3600000000000);
    if(id=="SRC-019"){d.allowed_channel_ids=&allowed;d.allowed_channel_count=1;}
    ars_source_admin_h admin{};ars_ingress_cap cap{};
    REQUIRE(ars_source_register(f.runtime,c,&d,&admin,&cap)==ARS_OK);
    ars_motion_channel ch{allowed,ARS_VALUE_SCALAR,0,{.75f,0,0,0}};
    ars_motion_frame frame{};frame.struct_size=sizeof frame;frame.abi_version=ARS_ABI_VERSION;
    frame.sequence=1;frame.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;frame.channel_count=1;frame.channels=&ch;frame.confidence=1;
    if(id=="SRC-017"){
        REQUIRE(ars_motion_submit(f.runtime,cap,&frame)==ARS_E_SCOPE_DENIED);
        std::puts("TRACE registered zero-channel capability denies mouth.open; no wildcard grant");
    }else{
        REQUIRE(id=="SRC-019");ars_ingress_cap forged{admin.id.runtime_uid,(uint64_t(admin.id.generation)<<32)|admin.id.slot};
        REQUIRE(ars_motion_submit(f.runtime,forged,&frame)==ARS_E_SCOPE_DENIED);
        REQUIRE(ars_motion_submit(f.runtime,cap,&frame)==ARS_OK);
        std::puts("TRACE admin handle bits rejected as bearer cap; real bearer accepts same sequence");
    }
    REQUIRE(ars_runtime_update(f.runtime,1)==ARS_OK);
    REQUIRE(ars_source_revoke(f.runtime,admin)==ARS_OK);
    REQUIRE(ars_motion_submit(f.runtime,cap,&frame)==ARS_E_SCOPE_DENIED);
    REQUIRE(ars_source_admin_release(f.runtime,admin)==ARS_OK);
    REQUIRE(ars_character_destroy(f.runtime,c)==ARS_OK);
}
}
int main(int argc,char **argv){
    if(argc!=3)return 2;const std::string id=argv[1];
    const std::array<std::string,8> ids{"HDL-001","HDL-002","HDL-003","HDL-004","HDL-012","HDL-014","SRC-017","SRC-019"};
    bool known=false;for(const auto &item:ids)known|=id==item;if(!known)return 2;
    std::ifstream stream(argv[2],std::ios::binary);REQUIRE(stream.good());
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};REQUIRE(!bytes.empty());
    if(id.starts_with("HDL-"))hdl(id,bytes);else source_case(id,bytes);
    std::printf("SECURITY_CASE %s PASS seed=42 live=0\n",id.c_str());return 0;
}
