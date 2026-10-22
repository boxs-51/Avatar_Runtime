#include "../../src/moc3/retained_physics.hpp"
#include "../../src/assets/asset_limits.hpp"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <thread>
using namespace ars::moc3;
using namespace ars::moc3::admission;
void* ARS_CALL alloc(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(n,a);}
void ARS_CALL free_mem(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
void check(bool v){if(!v){std::cerr<<"RETAINED_PHYSICS_FAIL\n";std::exit(1);}}
template<class F>void reject(F f){bool caught=false;try{f();}catch(...){caught=true;}check(caught);}
int main(int argc,char** argv){
    check(argc==2||argc==3);std::ifstream file(argv[1],std::ios::binary);std::string raw((std::istreambuf_iterator<char>(file)),{});check(!raw.empty());
    auto bytes=std::span(reinterpret_cast<const std::uint8_t*>(raw.data()),raw.size());
    ars::assets::Resource resource(alloc,free_mem,nullptr,8*ars::assets::MiB);ars::assets::Limits limits;ars::assets::Work work{limits.work};
    std::vector<ParameterInfo> catalog{{"P",-30,0,30},{"Q",-45,0,45}};
    RetainedPhysics physics(bytes,catalog,resource,limits,work),other(bytes,catalog,resource,limits,work);
    std::vector<Parameter> manual{{"P",15}};auto initial=physics.prepare(0,{},true,true);physics.commit(initial);
    auto candidate=physics.prepare(33333334,manual,true,false);check(candidate.steps()==2&&candidate.parameters()[1].value!=0);
    check(physics.prepare(33333334,manual,true,false).parameters()[1].value==candidate.parameters()[1].value);
    reject([&]{other.commit(candidate);});physics.commit(candidate);reject([&]{physics.commit(candidate);});
    auto before_failure=physics.prepare(66666668,{},true,false);
    reject([&]{physics.prepare(0,{},true,false);});reject([&]{physics.prepare(1000000000,{},true,false);});
    reject([&]{std::vector<Parameter> bad{{"UNKNOWN",0}};physics.prepare(33333334,bad,true,false);});
    reject([&]{std::vector<Parameter> bad{{"P",31}};physics.prepare(33333334,bad,true,false);});
    reject([&]{std::vector<Parameter> bad{{"P",0},{"P",1}};physics.prepare(33333334,bad,true,false);});
    reject([&]{std::vector<Parameter> bad{{"P",std::numeric_limits<float>::quiet_NaN()}};physics.prepare(33333334,bad,true,false);});
    check(physics.prepare(66666668,{},true,false).parameters()[1].value==before_failure.parameters()[1].value);
    bool wrong_thread=false;std::thread t([&]{try{physics.prepare(33333334,{},true,false);}catch(...){wrong_thread=true;}});t.join();check(wrong_thread);
    auto release=physics.prepare(66666668,{},true,false);check(release.parameters()[0].value==0&&release.parameters()[1].value!=0);physics.commit(release);
    auto off=physics.prepare(100000002,{},false,false);check(off.parameters()[1].value==0);physics.commit(off);
    auto reset=physics.prepare(133333336,{},true,true);check(reset.steps()==0&&reset.parameters()[1].value==initial.parameters()[1].value);physics.commit(reset);
    // Non-step remainder is retained, reset/off clears it, failed preparation cannot advance it.
    auto partial=physics.prepare(133333337,{},true,false);check(partial.steps()==0);physics.commit(partial);
    auto carry=physics.prepare(150000003,{},true,false);check(carry.steps()==1);
    if(argc==3){
        RetainedPhysics trace(bytes,catalog,resource,limits,work);std::cout<<std::setprecision(17)<<'[';
        for(unsigned i=0;i<7;++i){auto control=i==0||i==5?3:i==4?0:1;auto c=trace.prepare(i*33333334ull,i==1?std::span<const Parameter>(manual):std::span<const Parameter>{},control&1,control&2);
            if(i)std::cout<<',';std::cout<<'['<<c.parameters()[0].value<<','<<c.parameters()[1].value<<']';trace.commit(c);}
        std::cout<<"]\n";
    }else std::cout<<"RETAINED_PHYSICS_PASS\n";
}
