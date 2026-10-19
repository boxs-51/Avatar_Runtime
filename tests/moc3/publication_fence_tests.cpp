#include "rendering/d3d11_dual_mode_host.hpp"
#include "moc3/d3d11_renderer.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
using ars::graphics::Result;
using Host=ars::rendering::D3D11DualModeHost;
void check(bool ok){if(!ok){std::cerr<<"PUBLICATION_FENCE_FAIL\n";std::exit(1);}}
struct Fence:Host::PublicationFence {unsigned* calls;explicit Fence(unsigned& value):calls(&value){};Result decision=Result::failed;Result commit() noexcept override {++*calls;return decision;}};
int main(){
    Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    auto scene=std::make_shared<ars::moc3::Snapshot>();scene->generation=1;ars::moc3::Drawable draw;draw.texture_index=0;draw.double_sided=true;draw.screen={0,0,0,1};draw.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};draw.indices={0,1,2};scene->drawables.push_back(draw);
    auto atlas=std::make_shared<ars::moc3::AtlasSet>();atlas->images.push_back({1,1,{255,0,0,255}});
    for(bool close:{false,true}){
        unsigned calls=0;Host host(device.Get(),context.Get(),1);auto fence=std::make_shared<Fence>(calls);std::weak_ptr<Fence> pin=fence;
        auto pass=[&](auto& backend,auto* target,auto info,auto& query){return ars::moc3::render_offscreen_moc3(backend,target,info,scene,atlas,{},query);};
        check(host.request_moc(pass,1,32,32,false,fence)==Result::ok);fence.reset();check(!pin.expired());
        if(close)host.begin_close();auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);Result result=Result::busy;
        while(result==Result::busy&&std::chrono::steady_clock::now()<deadline){result=close?host.poll_close():host.poll_commit();std::this_thread::yield();}
        check(result==Result::failed&&!host.pending()&&pin.expired()&&calls==(close?0:1));check(host.texture()==nullptr&&host.target_epoch()==0&&host.pose_generation()==0);
        host.begin_close();check(host.poll_close()==Result::ok);
    }
    std::cout<<"OWNED_PUBLICATION_FENCE_PASS\n";
}
