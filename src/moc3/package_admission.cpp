#include "package_admission.hpp"
#include "drawable_packet.hpp"
#include "../assets/zip_reader.hpp"
#include "../assets/png_decoder.hpp"
#include "../validation/strict_json.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <malloc.h>
#include <set>
#include <bit>
#include <cmath>
namespace ars::moc3::admission {
namespace {
using assets::require;using validation::Kind;
void* ARS_CALL allocate(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(static_cast<std::size_t>(n),a);}
void ARS_CALL release(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
std::uint64_t read(unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i){int c=std::cin.get();require(c!=EOF);v|=std::uint64_t(c)<<(8*i);}return v;}
void keys(const validation::Document& doc,validation::Value v,std::initializer_list<std::string_view> allowed){
    require(v.kind()==Kind::object);for(auto i:v.node().children)require(std::find(allowed.begin(),allowed.end(),std::string_view(doc.nodes[i].key))!=allowed.end());
}
}
struct Binding {std::uint32_t slot;float value;};
struct CompleteFrame {std::uint64_t sequence,clock;std::vector<Binding> bindings;std::vector<Parameter> parameters;};
void emit(std::uint64_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)std::cout.put(static_cast<char>(value>>(i*8)));}
int package_worker(bool retained){
    const char* rejection="PACKAGE_ENVELOPE_REJECTED";
    try{
        char magic[8]{};require(bool(std::cin.read(magic,8))&&std::memcmp(magic,"ARSMPK01",8)==0);
        auto nonce=read(8),name_size=read(4),size=read(4);require(nonce&&name_size&&name_size<=512&&size>=22&&size<=96ull*assets::MiB);
        std::string name(static_cast<std::size_t>(name_size),'\0');require(bool(std::cin.read(name.data(),name.size())));
        assets::Resource resource(allocate,release,nullptr,384ull*assets::MiB);
        assets::Limits limits;limits.compressed=96*assets::MiB;limits.expanded=128*assets::MiB;limits.entries=256;
        assets::Work work{limits.work};assets::canonical_path(name,&resource,limits);require(name.ends_with(".model3.json"));
        assets::OwnedBundle bundle(resource);std::vector<CompleteFrame> frames;
        {std::pmr::vector<std::uint8_t> bytes(&resource);bytes.resize(static_cast<std::size_t>(size));
         require(bool(std::cin.read(reinterpret_cast<char*>(bytes.data()),bytes.size())));
         if(retained){
             rejection="PACKAGE_COMMAND_REJECTED";auto count=read(4);require(count&&count<=8);frames.reserve(static_cast<std::size_t>(count));
             std::uint64_t last=0;
             for(std::uint32_t i=0;i<count;++i){
                 CompleteFrame f;f.sequence=read(8);f.clock=read(8);require(f.sequence==i+1&&f.clock<=1000000000ull);
                 require(i?f.clock>last&&f.clock-last<=8ull*16666667:f.clock==0);last=f.clock;
                 require(read(4)==0);auto n=read(4);require(n<=32);std::set<std::uint32_t> seen;
                 for(std::uint32_t j=0;j<n;++j){auto slot=static_cast<std::uint32_t>(read(4));auto value=std::bit_cast<float>(static_cast<std::uint32_t>(read(4)));
                     require(slot<4096&&seen.insert(slot).second&&std::isfinite(value)&&value>=-1&&value<=1);f.bindings.push_back({slot,value});}
                 frames.push_back(std::move(f));
             }
         }
         require(std::cin.peek()==EOF&&std::cin.eof()&&!std::cin.bad());
         rejection="PACKAGE_ZIP_REJECTED";assets::Work zip_work{limits.compressed+limits.work};assets::read_zip(bytes,bundle,resource,limits,zip_work);}
        rejection="PACKAGE_MANIFEST_REJECTED";auto& manifest=bundle.find(name);validation::Document doc(&resource);
        validation::parse_json({reinterpret_cast<const char*>(manifest.bytes.data()),manifest.bytes.size()},doc,limits,work);
        auto root=doc.root();keys(doc,root,{"Version","FileReferences","Groups","HitAreas","Layout"});require(root.at("Version").u32()==3);
        auto refs=root.at("FileReferences");keys(doc,refs,{"Moc","Textures","Physics","Motions","Expressions","Pose","UserData","DisplayInfo"});
        auto slash=name.find_last_of('/');auto parent=slash==std::string::npos?std::string{}:name.substr(0,slash+1);
        std::set<std::string> selected;
        auto reference=[&](validation::Value value,std::string_view suffix)->const assets::Member&{
            auto relative=value.string();assets::canonical_path(relative,&resource,limits);require(relative.ends_with(suffix));
            auto path=parent+std::string(relative);auto alias=assets::canonical_path(path,&resource,limits);
            require(selected.insert(std::string(alias)).second);auto& member=bundle.find(path);require(!member.directory);return member;
        };
        auto& moc=reference(refs.at("Moc"),".moc3");require(moc.bytes.size()>=4&&moc.bytes.size()<=16*assets::MiB);
        auto textures=refs.at("Textures");require(textures.kind()==Kind::array&&textures.size()&&textures.size()<=16);
        rejection="PACKAGE_PNG_REJECTED";AtlasSet atlases;std::uint64_t total=0;
        for(std::size_t i=0;i<textures.size();++i){
            auto& member=reference(textures.at(i),".png");assets::Image decoded(&resource);
            auto remaining=128*assets::MiB-total;require(remaining>0,ARS_E_OUT_OF_BUDGET);limits.pixels=static_cast<std::size_t>(remaining);
            assets::decode_png(member.bytes,decoded,resource,limits,work);total+=decoded.rgba.size();
            Atlas image;image.width=decoded.width;image.height=decoded.height;image.rgba.assign(decoded.rgba.begin(),decoded.rgba.end());atlases.images.push_back(std::move(image));
        }
        rejection="PACKAGE_MOC_REJECTED";Budget budget;budget.max_drawables=4096;budget.max_atlases=16;std::unique_ptr<Session> session;
        require(Session::open_blob(moc.bytes,atlases.images.size(),budget,session)==Result::ok);
        std::uint32_t flags=0;
        if(refs.has("Physics")){
            rejection="PACKAGE_PHYSICS_REJECTED";auto& physics=reference(refs.at("Physics"),".physics3.json");std::vector<ParameterInfo> catalog;
            require(session->parameter_info(catalog)==Result::ok);validate_physics(physics.bytes,catalog,resource,limits,work);flags|=physics_validated;
        }
        unsigned bit=2;for(auto key:{"Motions","Expressions","Pose","UserData","DisplayInfo"}){if(refs.has(key))flags|=1u<<bit;++bit;}
        // Worker output is deliberately not a trust verdict. Host repeats complete validation.
        if(retained){
            rejection="PACKAGE_COMMAND_REJECTED";std::vector<ParameterInfo> catalog;require(session->parameter_info(catalog)==Result::ok&&catalog.size()<=4096);
            std::set<std::string> names;
            for(auto& p:catalog)require(names.insert(p.id).second&&std::isfinite(p.minimum)&&std::isfinite(p.default_value)&&std::isfinite(p.maximum)&&p.minimum>=-1e6&&p.maximum<=1e6&&p.minimum<=p.default_value&&p.default_value<=p.maximum);
            // Every command is bound/validated before the first evaluator update or output byte.
            for(auto& frame:frames)for(auto binding:frame.bindings){
                require(binding.slot<catalog.size());auto& p=catalog[binding.slot];auto v=binding.value;
                auto mapped=double(p.default_value)+double(v)*(v>=0?double(p.maximum)-p.default_value:double(p.default_value)-p.minimum);
                auto value=std::clamp(static_cast<float>(mapped),p.minimum,p.maximum);
                require(std::isfinite(value)&&value>=p.minimum&&value<=p.maximum);frame.parameters.push_back({p.id,value});
            }
            auto one=encoded_size(*session->current(),atlases);require(one+24<=(packet_cap-32)/frames.size(),ARS_E_OUT_OF_BUDGET);
            auto total=32+(one+24)*frames.size();rejection="PACKAGE_OUTPUT_REJECTED";
            std::cout.write("ARSEVAL1",8);emit(1,4);emit(frames.size(),4);emit(nonce,8);emit(total,8);
            for(auto& frame:frames){
                require(session->update_complete(frame.parameters)==Result::ok);
                auto snapshot=session->current();require(snapshot->generation==frame.sequence+1&&encoded_size(*snapshot,atlases)==one);
                emit(frame.sequence,8);emit(frame.clock,8);emit(one,8);encode_frame(std::cout,*snapshot,atlases,nonce,flags,2);
            }
        }else{rejection="PACKAGE_OUTPUT_REJECTED";encode(std::cout,*session->current(),atlases,nonce,flags);}
        std::cout.flush();return std::cout?0:42;
    }catch(const assets::Failure& failure){std::cout<<(failure.status==ARS_E_OUT_OF_BUDGET?"PACKAGE_BUDGET_REJECTED":rejection)<<"\n";return 0;}
     catch(const std::bad_alloc&){std::cout<<"PACKAGE_BUDGET_REJECTED\n";return 0;}
     catch(...){std::cout<<rejection<<"\n";return 0;}
}
}
