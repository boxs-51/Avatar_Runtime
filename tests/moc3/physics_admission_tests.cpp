#include "../../src/moc3/package_admission.hpp"
#include "../../src/assets/asset_limits.hpp"
#include <cstdlib>
#include <iostream>
#include <malloc.h>
#include <string>
using namespace ars::moc3;
void* ARS_CALL alloc(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(n,a);}
void ARS_CALL free_mem(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
void check(bool v){if(!v){std::cerr<<"PHYSICS_ADMISSION_FAIL\n";std::exit(1);}}
std::string replace(std::string s,const std::string& a,const std::string& b){auto p=s.find(a);check(p!=std::string::npos);s.replace(p,a.size(),b);return s;}
int main(){
    const std::string valid=R"({"Version":3,"Meta":{"PhysicsSettingCount":1,"TotalInputCount":1,"TotalOutputCount":1,"VertexCount":2,"EffectiveForces":{"Gravity":{"X":0,"Y":1},"Wind":{"X":0,"Y":0}},"Fps":60},"PhysicsSettings":[{"Id":"GeneratedChain","Input":[{"Source":{"Target":"Parameter","Id":"P"},"Weight":100,"Type":"Angle","Reflect":false}],"Output":[{"Destination":{"Target":"Parameter","Id":"P"},"Weight":100,"Type":"Angle","Reflect":false,"VertexIndex":1,"Scale":1}],"Vertices":[{"Position":{"X":0,"Y":0},"Mobility":1,"Delay":1,"Acceleration":1,"Radius":0},{"Position":{"X":0,"Y":1},"Mobility":1,"Delay":1,"Acceleration":1,"Radius":1}],"Normalization":{"Position":{"Minimum":-1,"Default":0,"Maximum":1},"Angle":{"Minimum":-30,"Default":0,"Maximum":30}}}]})";
    auto validate=[&](const std::string& s,const std::vector<ParameterInfo>& catalog){
        try{ars::assets::Resource r(alloc,free_mem,nullptr,8*ars::assets::MiB);ars::assets::Limits limits;ars::assets::Work work{limits.work};
            admission::validate_physics({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()},catalog,r,limits,work);return true;
        }catch(...){return false;}
    };
    std::vector<ParameterInfo> catalog{{"P",-1,0,1}};check(validate(valid,catalog));std::size_t n=0;
    for(auto [a,b]:{std::pair{"\"Version\":3","\"Version\":2"},{"\"Version\":3","\"Version\":true"},
        {"\"TotalInputCount\":1","\"TotalInputCount\":2"},{"\"VertexCount\":2","\"VertexCount\":3"},
        {"\"Id\":\"P\"","\"Id\":\"Unknown\""},{"\"Target\":\"Parameter\"","\"Target\":\"Part\""},
        {"\"Reflect\":false","\"Reflect\":0"},{"\"Type\":\"Angle\"","\"Type\":\"Z\""},
        {"\"VertexIndex\":1","\"VertexIndex\":0"},{"\"Mobility\":1","\"Mobility\":1.01"},
        {"\"Fps\":60","\"Fps\":241"},{"\"Scale\":1","\"Scale\":1e999"},
        {"\"Scale\":1","\"Scale\":NaN"},{"\"Weight\":100","\"Weight\":101"},
        {"\"Version\":3","\"Version\":3,\"Version\":3"},{"\"Minimum\":-1","\"Minimum\":1"}}){check(!validate(replace(valid,a,b),catalog));++n;}
    check(!validate(valid,{}));catalog.push_back(catalog[0]);check(!validate(valid,catalog));n+=2;
    std::cout<<"PHYSICS_ADMISSION_PASS rejected="<<n<<" GPU_CREATIONS=0\n";
}
