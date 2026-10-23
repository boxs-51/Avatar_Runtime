#define _CRT_SECURE_NO_WARNINGS
#define COBJMACROS
/* Installed-header-only C11: real public admission with a retained character. */
#include <ars/ars.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <windows.h>
#include <d3d11.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#ifdef ARS_QUALIFICATION_INTERNAL_PROBE
int qualification_bind_gpu(ars_runtime_h runtime);
uint64_t qualification_gpu_attempts(void);
#endif
struct memory {uint64_t live;};
struct gpu {
    ID3D11Device *device;ID3D11DeviceContext *context;
    ID3D11Texture2D *texture,*staging;ID3D11RenderTargetView *view;
    ars_render_target target;
};
static ars_status drain(ars_runtime_h runtime){
    unsigned i;ars_status result=ARS_E_BUSY;
    for(i=0;i<5000&&result==ARS_E_BUSY;++i){result=ars_runtime_drain_gpu(runtime);if(result==ARS_E_BUSY)Sleep(1);}return result;
}
static void gpu_init(struct gpu *g){
    D3D_FEATURE_LEVEL level;D3D11_TEXTURE2D_DESC td={0};
    CHECK(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_DEBUG,NULL,0,D3D11_SDK_VERSION,&g->device,&level,&g->context)));
    td.Width=td.Height=64;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(g->device,&td,NULL,&g->texture)));
    CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(g->device,(ID3D11Resource*)g->texture,NULL,&g->view)));
    td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(g->device,&td,NULL,&g->staging)));
    g->target.struct_size=sizeof(g->target);g->target.abi_version=ARS_ABI_VERSION;
    g->target.d3d11_device=g->device;g->target.d3d11_immediate_context=g->context;g->target.d3d11_rtv=g->view;g->target.device_generation=1;
    g->target.width_px=g->target.height_px=64;g->target.dxgi_format=DXGI_FORMAT_R8G8B8A8_UNORM;g->target.sample_count=1;
    g->target.color_space=ARS_COLOR_SRGB;g->target.alpha_mode=ARS_ALPHA_PREMULTIPLIED;
}
static void pixels(ars_runtime_h runtime,struct gpu *g,unsigned char *bytes){
    unsigned i,y;ars_status result=ARS_E_BUSY;D3D11_MAPPED_SUBRESOURCE map;
    for(i=0;i<5000&&result==ARS_E_BUSY;++i){result=ars_runtime_render(runtime,&g->target);if(result==ARS_E_BUSY)Sleep(1);}
    CHECK(result==ARS_OK);
    ID3D11DeviceContext_CopyResource(g->context,(ID3D11Resource*)g->staging,(ID3D11Resource*)g->texture);
    CHECK(SUCCEEDED(ID3D11DeviceContext_Map(g->context,(ID3D11Resource*)g->staging,0,D3D11_MAP_READ,0,&map)));
    for(y=0;y<64;++y)memcpy(bytes+y*64*4,(const unsigned char*)map.pData+y*map.RowPitch,64*4);
    ID3D11DeviceContext_Unmap(g->context,(ID3D11Resource*)g->staging,0);
    CHECK(drain(runtime)==ARS_OK);
}
static void *ARS_CALL allocate(void *user,uint64_t n,uint32_t a){
    struct memory *m=(struct memory*)user;void *p=_aligned_malloc((size_t)(n?n:1),a<sizeof(void*)?sizeof(void*):a);
    if(p)m->live+=n;return p;
}
static void ARS_CALL release(void *user,void *p,uint64_t n,uint32_t a){
    struct memory *m=(struct memory*)user;(void)a;CHECK(m->live>=n);m->live-=n;_aligned_free(p);
}
static ars_status load(ars_runtime_h runtime,const char *path,ars_model_h *model){
    FILE *f=fopen(path,"rb");long n;unsigned char *bytes;ars_asset_source source={0};ars_status result;
    CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);n=ftell(f);CHECK(n>0);rewind(f);
    bytes=(unsigned char*)malloc((size_t)n);CHECK(bytes!=NULL);CHECK(fread(bytes,1,(size_t)n,f)==(size_t)n);CHECK(fclose(f)==0);
    source.struct_size=sizeof(source);source.abi_version=ARS_ABI_VERSION;source.kind=ARS_ASSET_MEMORY_BUNDLE;source.bytes=bytes;source.byte_count=(uint64_t)n;
#ifdef ARS_QUALIFICATION_INTERNAL_PROBE
    {uint64_t before=qualification_gpu_attempts();result=ars_model_load(runtime,&source,model);
     CHECK(qualification_gpu_attempts()==before);}
#else
    result=ars_model_load(runtime,&source,model);
#endif
    memset(bytes,0,(size_t)n);free(bytes);return result;
}
int main(int argc,char **argv){
    struct memory memory={0};ars_runtime_desc descriptor={0};ars_runtime_h runtime={{0,0,0}};
    ars_model_h baseline={{0,0,0}};ars_character_h character={{0,0,0}};ars_character_desc cd={0};
    FILE *cases;char path[4096],line[2048];unsigned count=0;uint64_t clock=1;ars_status status;
    struct gpu gpu={0};unsigned char neutral[64*64*4],accepted[64*64*4],after[64*64*4];ars_layer_change layer={0};
    if(argc!=3)return 2;
    descriptor.struct_size=sizeof(descriptor);descriptor.abi_version=ARS_ABI_VERSION;descriptor.alloc=allocate;descriptor.free_fn=release;descriptor.allocator_user_data=&memory;
    CHECK(ars_runtime_create(&descriptor,&runtime)==ARS_OK);
#ifdef ARS_QUALIFICATION_INTERNAL_PROBE
    CHECK(qualification_bind_gpu(runtime));
#endif
    CHECK(snprintf(path,sizeof path,"%s/positive-2d.arschar",argv[1])<(int)sizeof path);
    CHECK(load(runtime,path,&baseline)==ARS_OK);
    cd.struct_size=sizeof(cd);cd.abi_version=ARS_ABI_VERSION;cd.initial_mode=ARS_MODE_AUTO;
    CHECK(ars_character_create(runtime,baseline,&cd,&character)==ARS_OK);
    CHECK(ars_runtime_update(runtime,clock)==ARS_OK);
    gpu_init(&gpu);pixels(runtime,&gpu,neutral);
    layer.struct_size=sizeof(layer);layer.abi_version=ARS_ABI_VERSION;layer.layer_id.ptr="face.mouth";layer.layer_id.byte_count=10;
    layer.visible=1;layer.opacity=.5f;layer.z_index=5;
    CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_OK);
    layer.layer_id.ptr="base.body";layer.layer_id.byte_count=9;
    CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_OK);
    CHECK(ars_runtime_update(runtime,++clock)==ARS_OK);pixels(runtime,&gpu,accepted);
    CHECK(memcmp(neutral,accepted,sizeof accepted)!=0);
    {unsigned i,lit=0;for(i=3;i<sizeof accepted;i+=4)if(accepted[i])++lit;CHECK(lit>0);}
#ifdef ARS_QUALIFICATION_INTERNAL_PROBE
    CHECK(qualification_gpu_attempts()>0); /* Positive control: actual renderer uses the probe. */
#endif
    cases=fopen(argv[2],"r");CHECK(cases!=NULL);
    while(fgets(line,sizeof line,cases)){
        char *end,*name,*nl;long expected;ars_model_h candidate={{9,9,9}};ars_character_h probe={{0,0,0}};
        expected=strtol(line,&end,10);CHECK(end!=line&&*end=='\t');name=end+1;nl=strchr(name,'\n');CHECK(nl!=NULL);*nl=0;
        if(nl>name&&nl[-1]=='\r')nl[-1]=0;
        CHECK(*name&&strchr(name,'/')==NULL&&strchr(name,'\\')==NULL&&strstr(name,"..")==NULL);
        CHECK(expected==ARS_OK||expected==ARS_E_BAD_ASSET||expected==ARS_E_UNSUPPORTED||expected==ARS_E_OUT_OF_BUDGET);
        CHECK(snprintf(path,sizeof path,"%s/%s",argv[1],name)<(int)sizeof path);
        status=load(runtime,path,&candidate);
        if(status!=expected){fprintf(stderr,"FAIL case %s expected=%ld actual=%d\n",name,expected,(int)status);return 1;}
        if(status==ARS_OK){CHECK(candidate.id.runtime_uid!=0);CHECK(ars_model_release(runtime,candidate)==ARS_OK);CHECK(ars_model_release(runtime,candidate)==ARS_E_STALE_HANDLE);}
        else CHECK(candidate.id.runtime_uid==0&&candidate.id.slot==0&&candidate.id.generation==0);
        pixels(runtime,&gpu,after);CHECK(memcmp(accepted,after,sizeof accepted)==0);
        /* A rejected candidate must not invalidate the existing model/character. */
        CHECK(ars_character_create(runtime,baseline,&cd,&probe)==ARS_OK);
        CHECK(ars_character_destroy(runtime,probe)==ARS_OK);
        CHECK(ars_runtime_update(runtime,++clock)==ARS_OK);
        printf("CASE %s %d PASS\n",name,(int)status);fflush(stdout);++count;
    }
    CHECK(!ferror(cases));CHECK(fclose(cases)==0);CHECK(count>0);
    CHECK(ars_model_release(runtime,baseline)==ARS_OK);CHECK(ars_runtime_update(runtime,++clock)==ARS_OK);
    CHECK(ars_character_destroy(runtime,character)==ARS_OK);CHECK(ars_runtime_begin_close(runtime)==ARS_OK);
    ID3D11RenderTargetView_Release(gpu.view);ID3D11Texture2D_Release(gpu.texture);ID3D11Texture2D_Release(gpu.staging);
    CHECK(drain(runtime)==ARS_OK);
    status=ARS_E_BUSY;
    {unsigned i;for(i=0;i<5000&&status==ARS_E_BUSY;++i){status=ars_runtime_finish_close(runtime);if(status==ARS_E_BUSY)Sleep(1);}}
    CHECK(status==ARS_OK);CHECK(memory.live==0);
    ID3D11DeviceContext_Release(gpu.context);ID3D11Device_Release(gpu.device);
    printf("ASSET_QUALIFICATION_PASS cases=%u live=0\n",count);return 0;
}
