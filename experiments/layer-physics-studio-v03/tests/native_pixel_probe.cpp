// Native Windows WARP pixel probe for original admitted .arschar and XPBD 2D.
// A diagnostic executable, NOT interactive Viewer and NOT public ABI activation.
#include "../../../src/core/render_frame.hpp"
#include "../../../src/rendering/committed_scene.hpp"
#include "../../../src/rendering/d3d11_scene2d.hpp"
#include "../native/native_mesh_xpbd.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
using namespace ars;
using namespace ars::experimental::lps;
namespace {
void require(bool success,const char*message){if(!success){std::fprintf(stderr,"FAIL %s\n",message);std::exit(2);}}
struct Model {
  ars_runtime_h runtime{};ars_model_h model{};ars_character_h character{};
  std::vector<unsigned char> bytes;
  explicit Model(const char*path){
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    require(bool(stream)&&stream.tellg()>std::streampos(0)&&stream.tellg()<=std::streampos(16*1024*1024),"bundle path/cap");
    const auto size=std::size_t(stream.tellg());bytes.resize(size);stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(size));require(bool(stream),"bundle bytes");
    ars_runtime_desc d{};d.struct_size=sizeof(d);d.abi_version=ARS_ABI_VERSION;
    require(ars_runtime_create(&d,&runtime)==ARS_OK,"runtime create");
    ars_asset_source s{sizeof(s),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};
    require(ars_model_load(runtime,&s,&model)==ARS_OK,"real native admission");
    ars_character_desc desc{sizeof(desc),ARS_ABI_VERSION,ARS_MODE_2D,0};
    require(ars_character_create(runtime,model,&desc,&character)==ARS_OK,"native 2D character");
    require(ars_runtime_update(runtime,1)==ARS_OK,"real committed pose");
  }
  ~Model(){if(runtime.id.runtime_uid){ars_runtime_begin_close(runtime);ars_runtime_finish_close(runtime);}}
  std::unique_ptr<core::RenderFrameLease> capture(){
    auto frame=std::make_unique<core::RenderFrameLease>();
    require(core::global_router().capture_render(runtime,character,frame.get())==ARS_OK,"actual pose capture");
    return frame;
  }
};
struct Gpu {
  ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Texture2D> texture,staging;ComPtr<ID3D11RenderTargetView> view;
  graphics::d3d11::HostDevice* backend=nullptr;
  std::unique_ptr<graphics::d3d11::HostDevice> owner;
  unsigned width=0,height=0;
  Gpu(unsigned w,unsigned h):width(w),height(h){
    require(w>0&&h>0&&w<=1024&&h<=1024,"bounded native output");
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
      &device,nullptr,&context)),"WARP real D3D11 device");
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&texture)),"offscreen texture");
    require(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&view)),"offscreen RTV");
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging image");
    owner=std::make_unique<graphics::d3d11::HostDevice>(device.Get(),context.Get(),71);backend=owner.get();
  }
  std::vector<unsigned char> render(const rendering::PreparedScene2D&scene,const assets::Image&atlas){
    graphics::d3d11::QueryRetirement q;
    const graphics::TargetInfo info{71,width,height};
    require(rendering::render_offscreen_2d(*backend,view.Get(),info,scene,atlas,q)==graphics::Result::ok,
       "real native D3D11 mesh render");
    auto result=graphics::Result::busy;
    for(unsigned i=0;i<20000&&result==graphics::Result::busy;++i){
      result=q.poll(71);if(result==graphics::Result::busy)std::this_thread::yield();}
    require(result==graphics::Result::ok&&q.complete(),"exact D3D11 query complete");
    context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"real pixel readback");
    std::vector<unsigned char> image(std::size_t(width)*height*4);
    for(unsigned y=0;y<height;++y)
      std::memcpy(image.data()+std::size_t(y)*width*4,
        static_cast<const unsigned char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,width*4);
    context->Unmap(staging.Get(),0);return image;
  }
};
rendering::PreparedScene2D project(const core::RenderFrameLease &frame){
  std::vector<rendering::LayerControl> controls;for(auto l:frame.layers())controls.push_back({l.visible,l.opacity,l.z});
  rendering::PreparedScene2D scene;
  require(rendering::prepare_committed_2d(*frame.model(),
     {frame.pose()->outputs.data(),frame.pose()->channel_count},controls,scene)==ARS_OK,"real committed mesh");
  return scene;
}
void ppm(const std::string&path,const std::vector<unsigned char>&rgba,unsigned w,unsigned h){
  std::ofstream f(path,std::ios::binary);require(bool(f),"pixel output file");
  f<<"P6\n"<<w<<' '<<h<<"\n255\n";
  for(std::size_t i=0;i<rgba.size();i+=4)f.write(reinterpret_cast<const char*>(rgba.data()+i),3);
  require(bool(f),"pixel output complete");
}
}
int main(int argc,char**argv){
  require(argc==2||argc==3,"usage: ars_lps_native_pixels model.arschar [output-prefix]");
  Model model(argv[1]);auto frame=model.capture();auto original=project(*frame);
  require(!original.draws.empty(),"whole original native2D model supported");
  auto canvas=original.canvas;
  Gpu gpu(std::min(canvas[0],1024u),std::min(canvas[1],1024u));
  auto before=gpu.render(original,frame->model()->image());
  Options opts;opts.gravity_y=40000;opts.wind_x=10000;opts.iterations=10;
  for(std::size_t i=0;i<std::min<std::size_t>(original.draws.size(),2);++i)
    if(original.draws[i].vertices.size()>=3)
      opts.dynamic_parts.push_back({original.draws[i].part_index,0,1});
  require(!opts.dynamic_parts.empty(),"at least one deformable part");
  if(opts.dynamic_parts.size()==2)opts.joints.push_back({
      {opts.dynamic_parts[0].part,2},{opts.dynamic_parts[1].part,2},0.0001});
  Session<rendering::PreparedScene2D> physics;
  require(physics.bind(original,opts,13)==Status::ok,"XPBD bound real native mesh");
  auto after=before;
  for(int step=0;step<24;++step){
    decltype(physics)::Candidate c;require(physics.prepare(original,13,c)==Status::ok,"XPBD CPU step");
    after=gpu.render(c.scene,frame->model()->image());
    require(physics.accept(std::move(c),13)==Status::ok,"only after successful GPU query accepts candidate");
  }
  std::size_t diff=0;for(std::size_t i=0;i<before.size();++i)diff+=(before[i]!=after[i]);
  std::printf("LPS_NATIVE_PIXELS original_parts=%zu selected=%zu changed_rgba_bytes=%zu\n",
              original.draws.size(),opts.dynamic_parts.size(),diff);
  require(diff>0,"XPBD changes REAL native D3D11 output pixels");
  if(argc==3){ppm(std::string(argv[2])+"-neutral.ppm",before,gpu.width,gpu.height);
              ppm(std::string(argv[2])+"-xpbd.ppm",after,gpu.width,gpu.height);}
  frame.reset();
  std::puts("PASS real native D3D11 WARP pixels with XPBD; still not interactive Viewer");
}
