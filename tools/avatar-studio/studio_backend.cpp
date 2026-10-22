// Local diagnostic adapter. Reuses the existing validated retained Physics3
// solver; it does not change ARS public admission or provide host gate evidence.
#include "studio_backend.h"
#include "../../src/moc3/retained_physics.hpp"
#include "../../src/validation/strict_json.hpp"
#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <malloc.h>
using namespace ars;
namespace {
void* ARS_CALL allocate(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(n,a);}
void ARS_CALL release(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
std::vector<std::uint8_t> read(const std::filesystem::path& p,std::size_t cap){
    auto n=std::filesystem::file_size(p);assets::require(n>0&&n<=cap);
    std::vector<std::uint8_t> b(static_cast<std::size_t>(n));std::ifstream f(p,std::ios::binary);
    assets::require(bool(f.read(reinterpret_cast<char*>(b.data()),b.size())));return b;
}
void copy(char *out,std::size_t n,std::string_view s){assets::require(s.size()<n);std::memcpy(out,s.data(),s.size());out[s.size()]=0;}
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
std::filesystem::path reference(const std::filesystem::path& root,std::string_view name){
    assets::require(!name.empty()&&name.size()<512);
    for(unsigned char c:name)assets::require(c>=32&&c!=':'&&c!='\\');
    auto rel=std::filesystem::u8path(name);assets::require(!rel.is_absolute());
    for(const auto& part:rel)assets::require(part!="..");
    auto path=std::filesystem::canonical(root/rel);
    auto relative=path.lexically_relative(root);assets::require(!relative.empty()&&!relative.is_absolute());
    for(const auto& part:relative)assets::require(part!="..");
    assets::require(std::filesystem::is_regular_file(path));return path;
}
struct Physics {
    assets::Resource resource{allocate,release,nullptr,8*assets::MiB};
    assets::Limits limits;
    std::vector<std::uint8_t> bytes;
    std::vector<moc3::ParameterInfo> catalog;
    std::unique_ptr<moc3::admission::RetainedPhysics> solver;
    moc3::admission::PhysicsCheckpoint checkpoint;
    std::uint64_t clock=0;unsigned generation=0;
    void construct(){assets::Work work{limits.work};solver=std::make_unique<moc3::admission::RetainedPhysics>(bytes,catalog,resource,limits,work);}
};
}
extern "C" int studio_read_model(const char* path,StudioInput* out){
    try{
        auto p=std::filesystem::canonical(std::filesystem::u8path(path)),root=p.parent_path();
        auto bytes=read(p,assets::MiB);assets::Resource r(allocate,release,nullptr,8*assets::MiB);
        assets::Limits limits;assets::Work work{limits.work};validation::Document doc(&r);
        validation::parse_json({reinterpret_cast<const char*>(bytes.data()),bytes.size()},doc,limits,work);
        auto refs=doc.root().at("FileReferences");StudioInput next{};
        copy(next.directory,sizeof next.directory,utf8(root)+"/");
        copy(next.moc,sizeof next.moc,utf8(reference(root,refs.at("Moc").string())));
        auto textures=refs.at("Textures");assets::require(textures.kind()==validation::Kind::array&&textures.size()>0&&textures.size()<=64);
        next.texture_count=static_cast<int>(textures.size());
        for(int i=0;i<next.texture_count;++i){auto name=textures.at(i).string();reference(root,name);
            assets::require(std::strlen(next.directory)+name.size()<1024);copy(next.textures[i],512,name);}
        if(refs.has("Physics"))copy(next.physics,sizeof next.physics,utf8(reference(root,refs.at("Physics").string())));
        *out=next;return 1;
    }catch(...){return 0;}
}
extern "C" void* studio_physics_create(const char* path,csmModel* model){
    try{
        auto p=std::make_unique<Physics>();p->bytes=read(std::filesystem::u8path(path),assets::MiB);
        auto n=csmGetParameterCount(model);assets::require(n>0&&n<=4096);
        auto ids=csmGetParameterIds(model);auto lo=csmGetParameterMinimumValues(model);
        auto hi=csmGetParameterMaximumValues(model);auto defaults=csmGetParameterDefaultValues(model);
        for(int i=0;i<n;++i)p->catalog.push_back({ids[i],lo[i],defaults[i],hi[i]});
        p->construct();return p.release();
    }catch(...){return nullptr;}
}
extern "C" int studio_physics_step(void* opaque,float* values,unsigned dt,int enabled,int reset){
    try{
        auto& p=*static_cast<Physics*>(opaque);
        assets::require(dt<=125000000); // UI: max 50 ms/frame at 2.5x speed.
        std::vector<moc3::Parameter> manual;manual.reserve(p.catalog.size());
        for(std::size_t i=0;i<p.catalog.size();++i)manual.push_back({p.catalog[i].id,values[i]});
        unsigned remaining=reset?0:dt;
        do{
            if(p.generation==8){p.construct();p.solver->restore_epoch(p.checkpoint);p.clock=0;p.generation=0;}
            const auto room=8u*16666667u-static_cast<unsigned>(p.checkpoint.remainder);
            const unsigned advance=std::min(remaining,room);
            if(reset)p.clock=0;else p.clock+=advance;
            auto c=p.solver->prepare(p.clock,manual,enabled!=0,reset!=0);
            p.checkpoint=p.solver->checkpoint(c,{});p.solver->commit(c);++p.generation;
            for(std::size_t i=0;i<p.catalog.size();++i)values[i]=c.parameters()[i].value;
            remaining-=advance;reset=0;
        }while(remaining);
        return 1;
    }catch(...){return 0;}
}
extern "C" void studio_physics_free(void* p){delete static_cast<Physics*>(p);}
extern "C" int studio_physics_probe(const char* path,csmModel* model){
    void *physics=studio_physics_create(path,model);if(!physics)return 0;
    const int count=csmGetParameterCount(model);std::vector<float> values(static_cast<std::size_t>(count));
    bool ok=true;
    for(unsigned i=0;i<80&&ok;++i){
        std::copy_n(csmGetParameterDefaultValues(model),count,values.begin());
        ok=studio_physics_step(physics,values.data(),125000000,1,i==0)!=0;
    }
    studio_physics_free(physics);return ok;
}
extern "C" int studio_pick_model(char* path,int cap){
    wchar_t file[1024]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof ofn;ofn.lpstrFile=file;ofn.nMaxFile=1024;
    ofn.lpstrFilter=L"Model3 manifest\0*.model3.json\0";ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&ofn))return 0;
    return WideCharToMultiByte(CP_UTF8,0,file,-1,path,cap,nullptr,nullptr)>0;
}
extern "C" int studio_replace_file(const char* temporary,const char* destination){
    try{return MoveFileExW(std::filesystem::u8path(temporary).c_str(),
                         std::filesystem::u8path(destination).c_str(),
                         MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    }catch(...){return 0;}
}
