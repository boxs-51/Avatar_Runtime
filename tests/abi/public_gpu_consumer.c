#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS
#include "ars/ars.h"
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL C11 GPU line%d\n",__LINE__);return 1;} } while(0)
static ars_status drain(ars_runtime_h runtime) {
    unsigned i;ars_status status=ARS_E_BUSY;
    for(i=0;i<5000 && status==ARS_E_BUSY;++i){status=ars_runtime_drain_gpu(runtime);if(status==ARS_E_BUSY)Sleep(1);}
    return status;
}
int main(int argc,char **argv) {
    FILE *file;long length;unsigned char *bytes;
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;
    ID3D11Texture2D *texture=NULL;ID3D11RenderTargetView *view=NULL;
    D3D_FEATURE_LEVEL level;D3D11_TEXTURE2D_DESC td;
    ars_runtime_desc rd;ars_runtime_h runtime;ars_model_h model;ars_character_h character;
    ars_asset_source asset;ars_character_desc cd;ars_render_target target;ars_close_state state;
    if(argc!=2)return 2;
    file=fopen(argv[1],"rb");CHECK(file!=NULL);CHECK(fseek(file,0,SEEK_END)==0);length=ftell(file);CHECK(length>0);rewind(file);
    bytes=(unsigned char*)malloc((size_t)length);CHECK(bytes!=NULL);
    CHECK(fread(bytes,1,(size_t)length,file)==(size_t)length);fclose(file);
    CHECK(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_DEBUG,
        NULL,0,D3D11_SDK_VERSION,&device,&level,&context)));
    memset(&td,0,sizeof(td));td.Width=td.Height=64;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&td,NULL,&texture)));
    CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)texture,NULL,&view)));
    memset(&rd,0,sizeof(rd));rd.struct_size=sizeof(rd);rd.abi_version=ARS_ABI_VERSION;
    CHECK(ars_runtime_create(&rd,&runtime)==ARS_OK);
    memset(&asset,0,sizeof(asset));asset.struct_size=sizeof(asset);asset.abi_version=ARS_ABI_VERSION;
    asset.kind=ARS_ASSET_MEMORY_BUNDLE;asset.bytes=bytes;asset.byte_count=(uint64_t)length;
    CHECK(ars_model_load(runtime,&asset,&model)==ARS_OK);memset(bytes,0,(size_t)length);free(bytes);
    memset(&cd,0,sizeof(cd));cd.struct_size=sizeof(cd);cd.abi_version=ARS_ABI_VERSION;cd.initial_mode=ARS_MODE_2D;
    CHECK(ars_character_create(runtime,model,&cd,&character)==ARS_OK);
    CHECK(ars_model_release(runtime,model)==ARS_OK);CHECK(ars_runtime_update(runtime,1)==ARS_OK);
    memset(&target,0,sizeof(target));target.struct_size=sizeof(target);target.abi_version=ARS_ABI_VERSION;
    target.d3d11_device=device;target.d3d11_immediate_context=context;target.d3d11_rtv=view;target.device_generation=1;
    target.width_px=target.height_px=64;target.dxgi_format=DXGI_FORMAT_R8G8B8A8_UNORM;target.sample_count=1;
    target.color_space=ARS_COLOR_SRGB;target.alpha_mode=ARS_ALPHA_PREMULTIPLIED;
    CHECK(ars_runtime_render(runtime,&target)==ARS_E_BUSY);
    /* The SDK's own COM custody must survive release of all host target refs. */
    ID3D11RenderTargetView_Release(view);view=NULL;ID3D11Texture2D_Release(texture);texture=NULL;
    CHECK(drain(runtime)==ARS_OK);
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&td,NULL,&texture)));
    CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)texture,NULL,&view)));
    target.d3d11_rtv=view;CHECK(ars_runtime_render(runtime,&target)==ARS_E_BUSY);
    CHECK(ars_runtime_begin_close(runtime)==ARS_OK);CHECK(ars_runtime_finish_close(runtime)==ARS_E_BUSY);
    ID3D11RenderTargetView_Release(view);ID3D11Texture2D_Release(texture);
    CHECK(drain(runtime)==ARS_OK);
    memset(&state,0,sizeof(state));state.struct_size=sizeof(state);state.abi_version=ARS_ABI_VERSION;
    CHECK(ars_runtime_query_close(runtime,&state)==ARS_OK);
    CHECK(state.state==ARS_FENCE_COMPLETE && state.outstanding_gpu_retires==0 && state.outstanding_render_snapshot_pins==0);
    CHECK(ars_runtime_finish_close(runtime)==ARS_OK);
    ID3D11DeviceContext_Release(context);ID3D11Device_Release(device);
    puts("PASS installed C11 GPU render/drain/borrowed target release/close custody");return 0;
}
