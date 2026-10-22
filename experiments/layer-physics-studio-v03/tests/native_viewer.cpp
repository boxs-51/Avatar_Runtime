// Experimental native Windows viewer: original .arschar CPU admission +
// committed 2D geometry -> XPBD -> native D3D11 WARP -> visible Win32 window.
// A DXGI swapchain presents query-completed D3D11 pixels in a native child HWND.
// No claim of alpha MOC3/GLB feature parity or public ABI support.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define main lps_existing_pixel_probe_entry
#include "native_pixel_probe.cpp"
#undef main
#include <algorithm>
#include <memory>
#include <cstdio>
#include <string>
#include <utility>

namespace {
// Native DXGI presentation of pixels already generated and query-completed by
// the original D3D11 offscreen renderer. The swapchain is confined to its
// child HWND; editor controls remain separate Win32 child controls.
struct DxgiPresenter {
  ComPtr<IDXGISwapChain> chain;
  ComPtr<ID3D11Texture2D> backbuffer;
  explicit DxgiPresenter(Gpu &gpu,HWND canvas){
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
    require(SUCCEEDED(gpu.device.As(&dxgi)),"DXGI adapter device");
    require(SUCCEEDED(dxgi->GetAdapter(&adapter)),"DXGI physical adapter");
    require(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))),"DXGI factory");
    DXGI_SWAP_CHAIN_DESC d{};
    d.BufferCount=2;d.BufferDesc.Width=gpu.width;d.BufferDesc.Height=gpu.height;
    d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
    d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=canvas;
    d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    require(SUCCEEDED(factory->CreateSwapChain(gpu.device.Get(),&d,&chain)),"DXGI viewer swapchain");
    require(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&backbuffer))),"DXGI viewer buffer");
  }
  bool present(Gpu &gpu){
    gpu.context->CopyResource(backbuffer.Get(),gpu.texture.Get());
    return SUCCEEDED(chain->Present(1,0));
  }
};
constexpr int kList=101,kDynamic=102,kGravityUp=103,kGravityDown=104,kWindUp=105,
 kWindDown=106,kSoft=107,kStiff=108,kCollision=109,kJoint=110,kPause=111,kReset=112;
class NativeViewer {
 public:
  Model model;
  std::unique_ptr<core::RenderFrameLease> captured;
  ars::rendering::PreparedScene2D base;
  std::unique_ptr<Gpu> gpu;
  std::unique_ptr<DxgiPresenter> presenter;
  HWND canvas=nullptr;
  Session<ars::rendering::PreparedScene2D> solver;
  Options options;
  HWND window=nullptr,listing=nullptr,status=nullptr;
  std::vector<bool> dynamic;
  std::vector<unsigned char> pixels;
  unsigned selected=0,rendered=0;
  unsigned viewport_w=0,viewport_h=0;
  bool paused=false,contact=false,joined=false,smoke=false,solver_ready=false,failed=false;
  std::uint64_t tick=1;
  explicit NativeViewer(const char *bundle,bool test):model(bundle),smoke(test){
    captured=model.capture();base=project(*captured);
    require(!base.draws.empty(),"native viewer requires drawable original 2D mesh");
    viewport_w=std::min(768u, std::max(128u,base.canvas[0]));
    viewport_h=std::min(768u, std::max(128u,base.canvas[1]));
    dynamic.resize(base.draws.size(),false);
    for(std::size_t i=0;i<base.draws.size()&&i<2;++i)
      if(base.draws[i].vertices.size()>2)dynamic[i]=true;
    options.gravity_y=360;options.wind_x=500;options.iterations=10;
    rebuild();
  }
  void rebuild(){
    options.dynamic_parts.clear();options.joints.clear();
    for(std::size_t i=0;i<dynamic.size();++i)
      if(dynamic[i]&&base.draws[i].vertices.size()>2)
        options.dynamic_parts.push_back({base.draws[i].part_index,0,1});
    if(joined && options.dynamic_parts.size()>=2){
      auto a=options.dynamic_parts[0],b=options.dynamic_parts[1];
      options.joints.push_back({{a.part,2},{b.part,2},0.0002});
    }
    options.contact_radius=contact?3.0:0.0;
    solver_ready=solver.bind(base,options,17)==Status::ok;
    update_label();
  }
  void update_label(){
    if(!window)return;
    wchar_t title[260]{};
    swprintf_s(title,L"Avatar Runtime — native D3D11 XPBD | %zu parts | gravity %.0f | wind %.0f | %s",
      base.draws.size(),options.gravity_y,options.wind_x,solver_ready?L"XPBD enabled":L"XPBD HOLD");
    SetWindowTextW(window,title);
    if(status)SetWindowTextW(status,solver_ready?
      L"D3D11 WARP native pixels; SPACE pause; R reset; click a layer to edit":
      L"XPBD unavailable for this model; original render retained");
  }
  void command(int id,int notification){
    if(id==kList){
      if(notification==LBN_SELCHANGE){
        auto n=SendMessageW(listing,LB_GETCURSEL,0,0);
        if(n!=LB_ERR)selected=static_cast<unsigned>(n);
      }return;
    }
    switch(id){
      case kDynamic:dynamic[selected]=!dynamic[selected];break;
      case kGravityUp:options.gravity_y=std::min(6000.,options.gravity_y+80);break;
      case kGravityDown:options.gravity_y=std::max(-6000.,options.gravity_y-80);break;
      case kWindUp:options.wind_x=std::min(6000.,options.wind_x+150);break;
      case kWindDown:options.wind_x=std::max(-6000.,options.wind_x-150);break;
      case kSoft:options.edge_compliance=std::min(0.001,options.edge_compliance*2);break;
      case kStiff:options.edge_compliance=std::max(1e-10,options.edge_compliance/2);break;
      case kCollision:contact=!contact;break;
      case kJoint:joined=!joined;break;
      case kPause:paused=!paused;update_label();return;
      case kReset:break;
      default:return;
    }
    rebuild();
  }
  bool advance(){
    if(ars_runtime_update(model.runtime,++tick)!=ARS_OK)return false;
    auto next=model.capture();
    auto scene=project(*next);
    if(scene.draws.size()!=base.draws.size())return false;
    if(!paused&&solver_ready){
      Session<ars::rendering::PreparedScene2D>::Candidate candidate;
      if(solver.prepare(scene,17,candidate)!=Status::ok){
        // Hard-fail XPBD and keep original model view, never silently publish it.
        solver_ready=false;update_label();
      }else{
        pixels=gpu->render(candidate.scene,next->model()->image());
        if(!presenter->present(*gpu))return false;
        if(solver.accept(std::move(candidate),17)!=Status::ok)return false;
      }
    }
    if(paused||!solver_ready){
      pixels=gpu->render(scene,next->model()->image());
      if(!presenter->present(*gpu))return false;
    }
    if(pixels.size()!=std::size_t(viewport_w)*viewport_h*4)return false;
    // BI_RGB 32-bit GDI expects B,G,R,A; D3D11 readback is R,G,B,A.
    for(std::size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    captured=std::move(next);
    ++rendered;
    // Presented by DXGI; GDI readback is kept solely for smoke/pixel inspection.
    return true;
  }
  void paint(){
    PAINTSTRUCT ps{};
    HDC dc=BeginPaint(window,&ps);
    if(!pixels.empty()){
      BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth=LONG(viewport_w);
      bmi.bmiHeader.biHeight=-LONG(viewport_h);
      bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;
      bmi.bmiHeader.biCompression=BI_RGB;
      StretchDIBits(dc,0,0,viewport_w,viewport_h,0,0,viewport_w,viewport_h,
        pixels.data(),&bmi,DIB_RGB_COLORS,SRCCOPY);
    }
    EndPaint(window,&ps);
  }
  static LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    auto *self=reinterpret_cast<NativeViewer*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){
      self=static_cast<NativeViewer*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
      SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(self){
      switch(msg){
        case WM_COMMAND:self->command(LOWORD(w),HIWORD(w));return 0;
        case WM_TIMER:if(!self->advance()){self->failed=true;DestroyWindow(hwnd);}return 0;
        case WM_PAINT:self->paint();return 0;
        case WM_KEYDOWN:
          if(w==VK_SPACE)self->command(kPause,0);
          if(w=='R')self->command(kReset,0);
          return 0;
        case WM_CLOSE:DestroyWindow(hwnd);return 0;
        case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
      }
    }
    return DefWindowProcW(hwnd,msg,w,l);
  }
  void button(int id,const wchar_t *title,int x,int y,int width=110){
    CreateWindowW(L"BUTTON",title,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,y,width,28,
      window,reinterpret_cast<HMENU>(std::intptr_t(id)),GetModuleHandleW(nullptr),nullptr);
  }
  int run(){
    WNDCLASSW cls{};cls.lpfnWndProc=wndproc;
    cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"ARS_LPS_NATIVE_VIEWER_03";
    cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    require(RegisterClassW(&cls)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"register native viewer");
    RECT rect{0,0,LONG(viewport_w+258),LONG(std::max(510u,viewport_h+30))};
    AdjustWindowRect(&rect,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
    window=CreateWindowW(cls.lpszClassName,L"Native XPBD Viewer",
      WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,
      rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,cls.hInstance,this);
    require(window!=nullptr,"native viewer window");
    const int x=int(viewport_w)+8;
    canvas=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_BLACKRECT,
      0,0,viewport_w,viewport_h,window,nullptr,cls.hInstance,nullptr);
    require(canvas!=nullptr,"create native DXGI canvas");
    listing=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",
      WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY,x,8,238,182,
      window,reinterpret_cast<HMENU>(std::intptr_t(kList)),cls.hInstance,nullptr);
    for(const auto &draw:base.draws){
      wchar_t label[90]{};
      swprintf_s(label,L"Part %u (%zu vertices)",draw.part_index,draw.vertices.size());
      SendMessageW(listing,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    }
    SendMessageW(listing,LB_SETCURSEL,0,0);
    button(kDynamic,L"XPBD selected part",x,196,238);
    button(kGravityUp,L"Gravity +",x,235);
    button(kGravityDown,L"Gravity -",x+120,235);
    button(kWindUp,L"Wind +",x,268);
    button(kWindDown,L"Wind -",x+120,268);
    button(kSoft,L"Softer",x,301);
    button(kStiff,L"Stiffer",x+120,301);
    button(kCollision,L"Toggle collision",x,334,238);
    button(kJoint,L"Toggle two-way joint",x,367,238);
    button(kPause,L"Pause / Resume",x,400);
    button(kReset,L"Reset physics",x+120,400);
    status=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,
      8,int(viewport_h)+4,int(viewport_w)+248,22,window,nullptr,cls.hInstance,nullptr);
    update_label();
    gpu=std::make_unique<Gpu>(viewport_w,viewport_h);
    presenter=std::make_unique<DxgiPresenter>(*gpu,canvas);
    ShowWindow(window,smoke?SW_SHOWNOACTIVATE:SW_SHOW);
    UpdateWindow(window);
    // Windows UI and solver both run on the model/GPU owner thread.
    if(smoke){
      for(int i=0;i<4;++i){
        if(i==1)command(kGravityUp,0);
        if(i==2)command(kWindDown,0);
        if(!advance()){failed=true;break;}
      }
      DestroyWindow(window);
    }else{
      require(SetTimer(window,1,33,nullptr)!=0,"native timer");
      MSG message{};
      while(GetMessageW(&message,nullptr,0,0)>0){
        TranslateMessage(&message);DispatchMessageW(&message);
      }
    }
    presenter.reset();
    gpu.reset();
    captured.reset();
    if(smoke&&!failed)std::printf("PASS Win32 native D3D11 WARP DXGI swapchain UI smoke; original parts=%zu frames=%u\n",
      base.draws.size(),rendered);
    return failed?3:0;
  }
};
}
int main(int argc,char **argv){
  if(argc<2||argc>3){std::fprintf(stderr,"Usage: ars_lps_native_viewer file.arschar [--smoke]\n");return 2;}
  bool smoke=argc==3&&std::strcmp(argv[2],"--smoke")==0;
  if(argc==3&&!smoke)return 2;
  NativeViewer viewer(argv[1],smoke);return viewer.run();
}
