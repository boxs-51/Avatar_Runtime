/* Avatar Studio: local interactive model lab, not public SDK qualification. */
#include "viewer.h"
#include "studio_backend.h"
#include "studio_xpbd.h"
#include "studio_layer_analysis.h"
extern __declspec(dllimport) void __stdcall glFinish(void);
extern __declspec(dllimport) unsigned int __stdcall glGetError(void);
#define GL_NO_ERROR 0
#include <math.h>
#include <stdint.h>
/* Windows CNG import without windows.h (raylib uses Rectangle/CloseWindow). */
extern __declspec(dllimport) long __stdcall BCryptGenRandom(void *,unsigned char *,unsigned long,unsigned long);

#define MAX_ITEMS 4096
#define STUDIO_RENDER_SCALE 1.5f
static const Color BG={12,17,27,255},CARD={20,28,42,255},LINE={40,53,73,255};
static const Color TEXT={224,232,243,255},MUTED={134,153,177,255},ACCENT={88,211,195,255};
static const char *expressions[]={"Idle","Happy","Surprised","Angry","Shy","Dance","Sleepy","Wink","Talking"};
typedef struct {
    int playing,physics,blink,breath,gaze,speech,cycle,grid,particles,background;
    float speed,strength,wind,blink_interval,cycle_interval,zoom;
} Settings;
typedef struct {
    Core core;Renderer renderer;View view;Options opt;Font font;
    int *sorted,count,parts,selected,tab,active_slider,search_focus,dragging,frame,limit,smoke;
    float *manual,*part_values,*part_defaults;unsigned char *pins;
    float time,gaze_x,gaze_y,pull_x,pull_y,base_zoom,weights[9],fps_history[120];
    int history;float scroll;char search[96],model_name[256],folder[1024],notice[192];
    double notice_until;void *linked;int physics_reset,step_requested;
    int failures,checks,physics_expected,xpbd_smoke,xpbd_smoke_frames,xpbd_motion_case,xpbd_motion_rebinds;float paused_time;double paused_checksum;
    void *xpbd;int xpbd_first,xpbd_second,xpbd_joint,xpbd_page,rig_selected,rig_beam_start,rig_overlay,rig_parent_start,rig_advanced,rig_cross_child;float rig_brush_radius,rig_brush_target;StudioRig rig;StudioRigBank rig_bank;StudioLayerSuggestion layer_proposal;StudioLayerTopology layer_topology;int layer_topology_valid;
     int layer_proposal_valid,layer_proposal_drawable;uint64_t layer_proposal_signature;float xpbd_gravity,xpbd_wind,xpbd_compliance,xpbd_collision,xpbd_mass,xpbd_damping,xpbd_contact_compliance,xpbd_step,xpbd_iters,xpbd_pin,xpbd_substeps,xpbd_pose_compliance,xpbd_source_transfer,xpbd_bending,xpbd_surface,xpbd_anchors;
    uint64_t model_generation; /* fresh OS random 63-bit tag per successful load */
    Settings settings;RenderTexture2D target;Rectangle viewport;
} Studio;
static Studio s;
static void verify(int pass,const char *name);
static float bound(float v,float a,float b){return fminf(b,fmaxf(a,v));}
static float mix(float a,float b,float t){return a+(b-a)*t;}
static int hit(Rectangle r){return CheckCollisionPointRec(GetMousePosition(),r);}
static void label(const char *text,float x,float y,float size,Color c){DrawTextEx(s.font,text,(Vector2){x,y},size,0.5f,c);}
static void card(Rectangle r){DrawRectangleRounded(r,.06f,8,CARD);}
static void toast(const char *text){snprintf(s.notice,sizeof s.notice,"%s",text);s.notice_until=GetTime()+4;}
static int button(Rectangle r,const char *text,int selected){
    Color fill=selected?(Color){36,78,82,255}:hit(r)?(Color){40,54,74,255}:(Color){27,38,55,255};
    DrawRectangleRounded(r,.2f,6,fill);
    if(selected)DrawRectangleRoundedLinesEx(r,.2f,6,1,ACCENT);
    Vector2 m=MeasureTextEx(s.font,text,16,.5f);label(text,r.x+(r.width-m.x)/2,r.y+(r.height-16)/2,16,selected?ACCENT:TEXT);
    return hit(r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static void toggle(float x,float y,const char *name,int *value){
    Rectangle r={x,y,224,30};label(name,x,y+7,16,TEXT);
    DrawRectangleRounded((Rectangle){x+186,y+5,38,20},1,8,*value?(Color){47,132,123,255}:LINE);
    DrawCircle((int)(x+196+(*value?18:0)),(int)y+15,7,*value?ACCENT:MUTED);
    if(hit(r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))*value=!*value;
}
static int slider(int id,Rectangle r,float *value,float low,float high){
    float old=*value;
    if(hit((Rectangle){r.x,r.y-7,r.width,24})&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))s.active_slider=id;
    if(s.active_slider==id&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&high>low)
        *value=low+bound((GetMouseX()-r.x)/r.width,0,1)*(high-low);
    float fraction=high>low?bound((*value-low)/(high-low),0,1):0;
    DrawRectangleRounded(r,1,6,LINE);
    if(fraction>0)DrawRectangleRounded((Rectangle){r.x,r.y,r.width*fraction,r.height},1,6,ACCENT);
    DrawCircle((int)(r.x+r.width*fraction),(int)(r.y+r.height/2),5,WHITE);
    return old!=*value;
}
static void control(int id,float x,float y,const char *name,float *value,float low,float high){
    label(name,x,y,15,MUTED);label(TextFormat("%.2f",*value),x+181,y,15,TEXT);
    slider(id,(Rectangle){x,y+28,224,4},value,low,high);
}
static void defaults(void){
    s.settings=(Settings){1,1,1,1,1,0,0,1,1,0,1,.7f,.15f,3.5f,7,1};
    s.selected=0;s.time=0;s.gaze_x=s.gaze_y=s.pull_x=s.pull_y=0;
    s.rig_parent_start=-1;s.rig_advanced=0;s.rig_cross_child=1;s.rig_brush_radius=.7f;s.rig_brush_target=.35f;s.xpbd_first=0;s.xpbd_second=-1;s.xpbd_joint=0;s.xpbd_gravity=-12;s.xpbd_wind=3;s.xpbd_compliance=.0000001f;s.xpbd_collision=0;s.xpbd_mass=1;s.xpbd_damping=.96f;s.xpbd_contact_compliance=0;s.xpbd_step=1.f/60;s.xpbd_iters=8;s.xpbd_pin=0;s.xpbd_substeps=4;s.xpbd_pose_compliance=.004f;s.xpbd_source_transfer=.03f;s.xpbd_bending=.00001f;s.xpbd_surface=.02f;s.xpbd_anchors=2;
    memset(s.weights,0,sizeof s.weights);s.weights[0]=1;s.physics_reset=1;
}
static void free_model(void){
    memset(&s.rig,0,sizeof s.rig);memset(&s.rig_bank,0,sizeof s.rig_bank);s.layer_proposal_valid=0;s.layer_topology_valid=0;
    studio_xpbd_disable(s.xpbd);s.xpbd=NULL;
    if(s.core.model){studio_physics_free(s.linked);s.linked=NULL;RendererFree(&s.renderer);FreeCore(&s.core);}
    free(s.sorted);free(s.manual);free(s.pins);free(s.part_values);free(s.part_defaults);s.part_defaults=NULL;
    s.sorted=NULL;s.manual=NULL;s.pins=NULL;s.part_values=NULL;s.core.model=NULL;
}
static int load_model(const char *path){
    StudioInput input;Core core={0};Renderer renderer={0};Model3 m3={0};
    if(!studio_read_model(path,&input)||!LoadCore(input.moc,&core)){toast("Cannot load this local model. Previous model kept.");return 0;}
    int count=csmGetParameterCount(core.model),parts=csmGetPartCount(core.model);
    int drawables=csmGetDrawableCount(core.model)+csmGetOffscreenCount(core.model);
    if(count<1||count>MAX_ITEMS||parts<0||parts>MAX_ITEMS||drawables<1||drawables>100256){FreeCore(&core);toast("Model exceeds viewer limits.");return 0;}
    m3.texCount=input.texture_count;memcpy(m3.tex,input.textures,sizeof m3.tex);
    int *sorted=calloc((size_t)drawables,sizeof(int));float *manual=calloc((size_t)count,sizeof(float));
    unsigned char *pins=calloc((size_t)count,1);float *part_values=calloc((size_t)(parts?parts:1),sizeof(float));
    float *part_defaults=calloc((size_t)(parts?parts:1),sizeof(float));
    if(!sorted||!manual||!pins||!part_values||!part_defaults||!RendererInit(&renderer,&m3,input.directory,core.model,s.view.sw,s.view.sh,.75f)){
        if(renderer.texCount)RendererFree(&renderer);FreeCore(&core);free(sorted);free(manual);free(pins);free(part_values);free(part_defaults);
        toast("Not enough resources. Previous model kept.");return 0;
    }
    for(int i=0;i<renderer.texCount;++i)if(!renderer.textures[i].id){
        RendererFree(&renderer);FreeCore(&core);free(sorted);free(manual);free(pins);free(part_values);free(part_defaults);
        toast("A model texture could not be loaded.");return 0;
    }
    void *physics=input.physics[0]?studio_physics_create(input.physics,core.model):NULL;
    if(s.smoke&&physics)verify(studio_physics_probe(input.physics,core.model),"physics maximum-speed boundary and epoch continuity");
    uint64_t generation=0;
    if(BCryptGenRandom(NULL,(unsigned char*)&generation,sizeof generation,
          2u /* BCRYPT_USE_SYSTEM_PREFERRED_RNG */)<0 ||
       (generation &= UINT64_C(0x7fffffffffffffff))==0){
        RendererFree(&renderer);FreeCore(&core);
        free(sorted);free(manual);free(pins);free(part_values);free(part_defaults);
        if(physics)studio_physics_free(physics);
         toast("Model generation unavailable; previous model kept.");return 0;
    }
    free_model();s.core=core;s.renderer=renderer;s.sorted=sorted;s.manual=manual;s.pins=pins;s.part_values=part_values;
    s.model_generation=generation;
    s.part_defaults=part_defaults;s.count=count;s.parts=parts;s.linked=physics;memcpy(manual,csmGetParameterDefaultValues(core.model),(size_t)count*sizeof(float));
    memcpy(part_values,csmGetPartOpacities(core.model),(size_t)parts*sizeof(float));
    memcpy(part_defaults,part_values,(size_t)parts*sizeof(float));
    s.scroll=0;s.search[0]=0;defaults();FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;
    snprintf(s.model_name,sizeof s.model_name,"%s",GetFileName(path));
    toast(physics?"Model ready. Linked Physics3 active.":"Model ready. No supported linked Physics3.");return 1;
}
static void reset_manual(void){
    if(s.core.model){memset(s.pins,0,(size_t)s.count);memcpy(s.manual,csmGetParameterDefaultValues(s.core.model),(size_t)s.count*sizeof(float));
        memcpy(s.part_values,s.part_defaults,(size_t)s.parts*sizeof(float));}
    s.physics_reset=1;toast("Manual overrides cleared.");
}
static void drive(const char *name,float value){
    const char **ids=csmGetParameterIds(s.core.model);float *values=csmGetParameterValues(s.core.model);
    const float *low=csmGetParameterMinimumValues(s.core.model),*high=csmGetParameterMaximumValues(s.core.model);
    for(int i=0;i<s.count;++i)if(!strcmp(ids[i],name)){values[i]=bound(value,low[i],high[i]);return;}
}
static void update(float dt){
    if(!s.core.model)return;
    float advance=(s.settings.playing||s.step_requested)?dt*s.settings.speed:0;
    if(s.step_requested)advance=1.0f/60;s.step_requested=0;s.time+=advance;
    if(s.settings.cycle)s.selected=(int)(s.time/s.settings.cycle_interval)%9;
    float ease=1-expf(-advance*9);
    for(int i=0;i<9;++i)s.weights[i]=mix(s.weights[i],i==s.selected?1.0f:0.0f,ease);
    float *v=csmGetParameterValues(s.core.model);const float *initial=csmGetParameterDefaultValues(s.core.model);
    const float *lo=csmGetParameterMinimumValues(s.core.model),*hi=csmGetParameterMaximumValues(s.core.model);
    memcpy(v,initial,(size_t)s.count*sizeof(float));
    float t=s.time,amplitude=s.settings.strength;
    float hx=amplitude*sinf(t*1.05f)*7,hy=amplitude*cosf(t*.8f)*4,hz=amplitude*sinf(t*.85f)*4;
    float eye=1,smile=0,mouth=0,cheek=0,brow=0,form=0;
    if(s.settings.blink){float blink=fmodf(t,s.settings.blink_interval);if(blink<.16f)eye=1-sinf(blink/.16f*PI);}
    smile=s.weights[1]+s.weights[4]*.3f;mouth=s.weights[1]*.15f+s.weights[2]*.85f;
    form=s.weights[1]-.8f*s.weights[3]+.4f*s.weights[4];cheek=s.weights[1]*.45f+s.weights[4]*.9f;
    brow=s.weights[2]*.8f-s.weights[3]*.7f;
    eye=mix(eye,.25f,s.weights[6]);hy-=s.weights[6]*8;hz+=s.weights[4]*6;
    if(s.settings.speech||s.selected==8)mouth=fmaxf(mouth,(sinf(t*11)+sinf(t*17)*.3f)*.55f);
    hx+=s.weights[5]*sinf(t*5)*15;hz+=s.weights[5]*cosf(t*4)*10;
    hx+=s.settings.wind*sinf(t*2)*12;
    if(s.settings.gaze){hx+=s.gaze_x*15;hy-=s.gaze_y*10;}
    hx+=s.pull_x;hy+=s.pull_y;
    drive("ParamAngleX",hx);drive("ParamAngleY",hy);drive("ParamAngleZ",hz);
    drive("ParamBodyAngleX",hx*.3f);drive("ParamBodyAngleY",hy*.2f);drive("ParamBodyAngleZ",hz*.25f);
    drive("ParamEyeBallX",s.settings.gaze?s.gaze_x:0);drive("ParamEyeBallY",s.settings.gaze?-s.gaze_y:0);
    drive("ParamEyeLOpen",eye*(1-s.weights[7]));drive("ParamEyeROpen",eye);
    drive("ParamEyeLSmile",smile);drive("ParamEyeRSmile",smile);
    drive("ParamMouthOpenY",fmaxf(0,mouth));drive("ParamMouthForm",form);drive("ParamCheek",cheek);
    drive("ParamBrowLY",brow);drive("ParamBrowRY",brow);
    drive("ParamBrowLAngle",s.weights[3]*.7f);drive("ParamBrowRAngle",s.weights[3]*.7f);
    drive("ParamBreath",s.settings.breath?(sinf(t*2)*.5f+.5f)*amplitude:0);
    drive("Param1000",s.dragging==2?s.pull_x:0);
    for(int i=0;i<s.count;++i)if(s.pins[i])v[i]=bound(s.manual[i],lo[i],hi[i]);
    if(s.linked&&!studio_physics_step(s.linked,v,(unsigned)(advance*1e9f),s.settings.physics,s.physics_reset)){
        studio_physics_free(s.linked);s.linked=NULL;toast("Physics stopped: solver rejected this update.");
    }
    s.physics_reset=0;
    for(int i=0;i<s.count;++i){if(s.pins[i])v[i]=bound(s.manual[i],lo[i],hi[i]);v[i]=bound(v[i],lo[i],hi[i]);}
    float *parts=csmGetPartOpacities(s.core.model);for(int i=0;i<s.parts;++i)parts[i]=s.part_values[i];
    csmUpdateModel(s.core.model);
    if(!s.dragging){s.pull_x*=expf(-dt*6);s.pull_y*=expf(-dt*6);}
}
static uint64_t signature(void){
    uint64_t h=1469598103934665603ull;const char **ids=csmGetParameterIds(s.core.model);
    for(int i=0;i<s.count;++i){for(const unsigned char *p=(const unsigned char*)ids[i];*p;++p){h^=*p;h*=1099511628211ull;}h^=255;h*=1099511628211ull;}
    ids=csmGetPartIds(s.core.model);
    for(int i=0;i<s.parts;++i){for(const unsigned char *p=(const unsigned char*)ids[i];*p;++p){h^=*p;h*=1099511628211ull;}h^=254;h*=1099511628211ull;}
    return h;
}
static int save_preset(void){
    if(!s.core.model)return 0;char path[1200],temporary[1200];snprintf(path,sizeof path,"%s/preset.txt",s.folder);
    snprintf(temporary,sizeof temporary,"%s/preset.tmp",s.folder);
    FILE *f=fopen(temporary,"w");if(!f){toast("Cannot save preset.");return 0;}
    fprintf(f,"AVATAR_STUDIO_1 %llu %d %d\n",(unsigned long long)signature(),s.count,s.parts);
    Settings *q=&s.settings;
    fprintf(f,"%d %d %d %d %d %d %d %d %d %d\n",q->playing,q->physics,q->blink,q->breath,q->gaze,q->speech,q->cycle,q->grid,q->particles,q->background);
    fprintf(f,"%.9g %.9g %.9g %.9g %.9g %.9g %d\n",q->speed,q->strength,q->wind,q->blink_interval,q->cycle_interval,q->zoom,s.selected);
    for(int i=0;i<s.count;++i)fprintf(f,"%d %.9g\n",s.pins[i],s.manual[i]);
    for(int i=0;i<s.parts;++i)fprintf(f,"%.9g\n",s.part_values[i]);
    int failed=ferror(f);if(fclose(f))failed=1;
    if(!failed&&!studio_replace_file(temporary,path))failed=1;
    toast(failed?"Preset write failed; previous preset kept.":"Preset saved locally.");return !failed;
}
static int load_preset(void){
    if(!s.core.model)return 0;char path[1200];snprintf(path,sizeof path,"%s/preset.txt",s.folder);FILE *f=fopen(path,"r");
    if(!f){toast("No saved preset yet.");return 0;}
    unsigned long long hash=0;int count=0,parts=0,selected=0;Settings q={0};char magic[32]={0};
    float *manual=calloc((size_t)s.count,sizeof(float)),*opacity=calloc((size_t)(s.parts?s.parts:1),sizeof(float));
    unsigned char *pins=calloc((size_t)s.count,1);int ok=manual&&opacity&&pins;
    if(ok)ok=fscanf(f,"%31s %llu %d %d",magic,&hash,&count,&parts)==4&&!strcmp(magic,"AVATAR_STUDIO_1")&&hash==signature()&&count==s.count&&parts==s.parts;
    if(ok)ok=fscanf(f,"%d %d %d %d %d %d %d %d %d %d",&q.playing,&q.physics,&q.blink,&q.breath,&q.gaze,&q.speech,&q.cycle,&q.grid,&q.particles,&q.background)==10;
    if(ok){int flags[]={q.playing,q.physics,q.blink,q.breath,q.gaze,q.speech,q.cycle,q.grid,q.particles};for(int i=0;i<9;++i)if(flags[i]<0||flags[i]>1)ok=0;if(q.background<0||q.background>3)ok=0;}
    if(ok)ok=fscanf(f,"%f %f %f %f %f %f %d",&q.speed,&q.strength,&q.wind,&q.blink_interval,&q.cycle_interval,&q.zoom,&selected)==7;
    if(ok)ok=isfinite(q.speed)&&q.speed>=.1f&&q.speed<=2.5f&&isfinite(q.strength)&&q.strength>=0&&q.strength<=1.5f&&isfinite(q.wind)&&q.wind>=0&&q.wind<=1&&isfinite(q.blink_interval)&&q.blink_interval>=1&&q.blink_interval<=8&&isfinite(q.cycle_interval)&&q.cycle_interval>=3&&q.cycle_interval<=20&&isfinite(q.zoom)&&q.zoom>=.3f&&q.zoom<=3&&selected>=0&&selected<9;
    const float *lo=csmGetParameterMinimumValues(s.core.model),*hi=csmGetParameterMaximumValues(s.core.model);
    for(int i=0;i<s.count&&ok;++i){int pin;ok=fscanf(f,"%d %f",&pin,&manual[i])==2&&pin>=0&&pin<=1&&isfinite(manual[i])&&manual[i]>=lo[i]&&manual[i]<=hi[i];if(ok)pins[i]=(unsigned char)pin;}
    for(int i=0;i<s.parts&&ok;++i)ok=fscanf(f,"%f",&opacity[i])==1&&isfinite(opacity[i])&&opacity[i]>=0&&opacity[i]<=1;
    if(ok){int ch;do{ch=fgetc(f);}while(ch==' '||ch=='\n'||ch=='\r'||ch=='\t');ok=ch==EOF;}
    fclose(f);
    if(ok){s.settings=q;s.selected=selected;memcpy(s.manual,manual,(size_t)s.count*sizeof(float));memcpy(s.pins,pins,(size_t)s.count);memcpy(s.part_values,opacity,(size_t)s.parts*sizeof(float));s.physics_reset=1;}
    free(manual);free(pins);free(opacity);toast(ok?"Preset restored.":"Preset rejected; current settings kept.");return ok;
}
static void capture(const char *name,int window){
    char path[1200];snprintf(path,sizeof path,"%s/%s.png",s.folder,name);
    if(window){Image image=LoadImageFromScreen();ExportImage(image,path);UnloadImage(image);}
    else if(s.target.id){Image image=LoadImageFromTexture(s.target.texture);ImageFlipVertical(&image);int ok=ExportImage(image,path);UnloadImage(image);toast(ok?"Viewport PNG saved locally.":"PNG capture failed.");}
}
static void stage_background(void){
    int w=s.view.sw,h=s.view.sh;
    Color top=s.settings.background==1?(Color){215,225,238,255}:s.settings.background==2?(Color){18,62,67,255}:(Color){26,32,54,255};
    Color bottom=s.settings.background==1?(Color){166,184,210,255}:s.settings.background==2?(Color){10,24,38,255}:(Color){13,20,34,255};
    ClearBackground(top);DrawRectangleGradientV(0,0,w,h,top,bottom);
    if(s.settings.background==3){for(int y=0;y<h;y+=24)for(int x=0;x<w;x+=24)DrawRectangle(x,y,24,24,((x+y)/24)%2?(Color){45,54,69,255}:(Color){32,40,55,255});}
    if(s.settings.grid){for(int x=0;x<w;x+=48)DrawLine(x,0,x,h,Fade(ACCENT,.045f));for(int y=0;y<h;y+=48)DrawLine(0,y,w,y,Fade(ACCENT,.045f));}
    if(s.settings.particles)for(int i=0;i<28;++i){float x=fmodf(i*119.3f+sinf(s.time*.3f+i)*18+w,w);float y=fmodf(i*83.7f-s.time*(5+i%4)+h*100,h);DrawCircleV((Vector2){x,y},1.5f+(i%3)*.5f,Fade(ACCENT,.12f));}
    DrawEllipse(w/2,(int)(h*.92f),w*.26f,18,Fade(BLACK,.18f));
}
static void camera_ui(void){
    float x=s.viewport.x,y=s.viewport.y-43;
    if(button((Rectangle){x,y,64,30},"Fit",0)){FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;s.settings.zoom=1;}
    if(button((Rectangle){x+72,y,64,30},"Face",0)){s.settings.zoom=1.8f;s.view.panx=0;s.view.pany=s.view.sh*.28f;}
    if(button((Rectangle){x+144,y,64,30},s.settings.playing?"Pause":"Play",!s.settings.playing))s.settings.playing=!s.settings.playing;
    if(button((Rectangle){x+216,y,64,30},"Step",0)){s.settings.playing=0;s.step_requested=1;}
    if(button((Rectangle){x+288,y,86,30},"Capture",0))capture("viewport",0);
    if(button((Rectangle){x+382,y,64,30},"Reset",0)){defaults();reset_manual();FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;}
    if(s.viewport.width>640){label(TextFormat("%.0f%%",s.settings.zoom*100),x+464,y+7,14,MUTED);
        slider(8,(Rectangle){x+524,y+14,s.viewport.width-536,4},&s.settings.zoom,.3f,3);}
}
static void left_ui(void){
    float x=32,y=110;label("PERFORMANCE",x,y,12,MUTED);y+=26;
    label(TextFormat("%d",GetFPS()),x,y,34,ACCENT);label("FPS",x+70,y+14,14,MUTED);y+=52;
    label("EXPRESSION",x,y,12,MUTED);y+=24;
    for(int i=0;i<9;++i){Rectangle r={x+(i%2)*116,y+(i/2)*36,108,30};if(button(r,expressions[i],s.selected==i)){s.selected=i;s.settings.cycle=0;}}
    y+=190;
    toggle(x,y,"Expression cycle",&s.settings.cycle);y+=34;
    toggle(x,y,"Linked physics",&s.settings.physics);y+=34;
    toggle(x,y,"Auto blink",&s.settings.blink);y+=34;
    toggle(x,y,"Breathing",&s.settings.breath);y+=34;
    toggle(x,y,"Follow cursor",&s.settings.gaze);y+=34;
    toggle(x,y,"Auto speech",&s.settings.speech);y+=42;
    control(1,x,y,"Motion speed",&s.settings.speed,.1f,2.5f);y+=52;
    control(2,x,y,"Motion amount",&s.settings.strength,0,1.5f);y+=52;
    control(3,x,y,"Wind drive",&s.settings.wind,0,1);y+=52;
    if(y<GetScreenHeight()-120){control(4,x,y,"Blink interval",&s.settings.blink_interval,1,8);y+=52;}
    if(y<GetScreenHeight()-120)control(5,x,y,"Cycle interval",&s.settings.cycle_interval,3,20);
    if(button((Rectangle){x,(float)GetScreenHeight()-98,108,30},"Save preset",0))save_preset();
    if(button((Rectangle){x+116,(float)GetScreenHeight()-98,108,30},"Load preset",0))load_preset();
}
static int contains(const char *haystack,const char *needle){
    if(!needle[0])return 1;size_t n=strlen(needle);
    for(;*haystack;++haystack){size_t i=0;while(i<n&&haystack[i]&&tolower((unsigned char)haystack[i])==tolower((unsigned char)needle[i]))++i;if(i==n)return 1;}return 0;
}
static void rig_overlay_ui(void){
 if(s.tab!=3||!s.core.model)return;
 int d=s.xpbd_first;
 int count=csmGetDrawableCount(s.core.model);
 if(d<0||d>=count)return;
 const int *counts=csmGetDrawableVertexCounts(s.core.model);
 const csmVector2 **vertices=csmGetDrawableVertexPositions(s.core.model);
 if(!counts||!vertices||!vertices[d]||counts[d]<1)return;
 int n=counts[d];
 if(s.rig.vertex_count!=n||s.rig.drawable!=d)
   if(!studio_rig_bank_select(&s.rig_bank,&s.rig,d,signature(),n))return;
 Vector2 mouse=GetMousePosition();
 float best=18.f*18.f;int nearest=-1;
 for(int j=0;j<n;++j){
  float px,py;
  WorldToScreen(&s.view,vertices[d][j].X,vertices[d][j].Y,&px,&py);
  Vector2 view_point={s.viewport.x+px*s.viewport.width/s.view.sw,
    s.viewport.y+py*s.viewport.height/s.view.sh};
  float dd=(view_point.x-mouse.x)*(view_point.x-mouse.x)+
           (view_point.y-mouse.y)*(view_point.y-mouse.y);
  if(dd<best){best=dd;nearest=j;}
 }
 // A selected vertex is scoped to the visible viewport only.
 if(CheckCollisionPointRec(mouse,s.viewport)&&nearest>=0&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
  s.rig_selected=nearest;
  if(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT))
   studio_rig_toggle_anchor(&s.rig,nearest);
  else if(IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)){
   if(s.rig_beam_start<0)s.rig_beam_start=nearest;
   else {studio_rig_toggle_beam(&s.rig,s.rig_beam_start,nearest);s.rig_beam_start=-1;}
  }
 }
 const Color rigAnchor={252,166,76,240},rigBeam={104,222,190,230};
 for(int j=0;j<s.rig.beam_count;++j){
  int a=s.rig.beams[j].a,b=s.rig.beams[j].b;
  if(a>=n||b>=n)continue;
  float ax,ay,bx,by;
  WorldToScreen(&s.view,vertices[d][a].X,vertices[d][a].Y,&ax,&ay);
  WorldToScreen(&s.view,vertices[d][b].X,vertices[d][b].Y,&bx,&by);
  DrawLineEx((Vector2){s.viewport.x+ax*s.viewport.width/s.view.sw,s.viewport.y+ay*s.viewport.height/s.view.sh},
             (Vector2){s.viewport.x+bx*s.viewport.width/s.view.sw,s.viewport.y+by*s.viewport.height/s.view.sh},2,rigBeam);
 }
 for(int j=0;j<n;++j){
  int pinned=0;
  for(int k=0;k<s.rig.anchor_count;++k)if(s.rig.anchors[k]==j){pinned=1;break;}
  if(!pinned&&j!=s.rig_selected&&j!=nearest)continue;
  float px,py;
  WorldToScreen(&s.view,vertices[d][j].X,vertices[d][j].Y,&px,&py);
  float x=s.viewport.x+px*s.viewport.width/s.view.sw;
  float y=s.viewport.y+py*s.viewport.height/s.view.sh;
  if(x<s.viewport.x||y<s.viewport.y||x>s.viewport.x+s.viewport.width||
      y>s.viewport.y+s.viewport.height)continue;
  Color color=pinned?rigAnchor:rigBeam;
  DrawCircleV((Vector2){x,y},j==s.rig_selected?8:5,color);
 }
}
static int studio_model_y_bounds(float *low,float *high){
 if(!s.core.model||!low||!high)return 0;
 const int n=csmGetDrawableCount(s.core.model);
 const int *counts=csmGetDrawableVertexCounts(s.core.model);
 const csmVector2 **pos=csmGetDrawableVertexPositions(s.core.model);
 if(n<1||n>4096||!counts||!pos)return 0;
 float mn=INFINITY,mx=-INFINITY;
 for(int i=0;i<n;++i){
  if(counts[i]<0||counts[i]>8192||(counts[i]&&!pos[i]))return 0;
  for(int j=0;j<counts[i];++j){
   float y=pos[i][j].Y;
   if(!isfinite(y))return 0;
   if(y<mn)mn=y;if(y>mx)mx=y;
  }
 }
 if(!isfinite(mn)||!isfinite(mx)||mx<=mn)return 0;
 *low=mn;*high=mx;return 1;
}
static int studio_suggest_drawable_cached(int d,int n,const int *counts,
          const csmVector2 **pos,const char **ids,float low,float high,
          StudioLayerSuggestion *out){
 if(!out||d<0||d>=n||!counts||!pos||!ids||
    !ids[d]||counts[d]<3||counts[d]>2048||!pos[d])return 0;
 StudioLayerVertex sample[2048];
 for(int j=0;j<counts[d];++j)
  sample[j]=(StudioLayerVertex){pos[d][j].X,pos[d][j].Y};
 StudioLayerInput input={ids[d],sample,counts[d],low,high,high-low};
 return studio_layer_analyze(&input,out);
}
static int studio_suggest_drawable_bounds(int d,float low,float high,
                                          StudioLayerSuggestion *out){
 return studio_suggest_drawable_cached(d,csmGetDrawableCount(s.core.model),
     csmGetDrawableVertexCounts(s.core.model),
     csmGetDrawableVertexPositions(s.core.model),
     csmGetDrawableIds(s.core.model),low,high,out);
}
static int studio_suggest_drawable(int d,StudioLayerSuggestion *out){
 float low,high;
 return studio_model_y_bounds(&low,&high)&&
   studio_suggest_drawable_bounds(d,low,high,out);
}
static int studio_component_suggest(int component,StudioLayerComponent *out){
 int d=s.xpbd_first,n=csmGetDrawableCount(s.core.model);
 const int *vc=csmGetDrawableVertexCounts(s.core.model);
 const int *ic=csmGetDrawableIndexCounts(s.core.model);
 const unsigned short **idx=csmGetDrawableIndices(s.core.model);
 const csmVector2 **positions=csmGetDrawableVertexPositions(s.core.model);
 if(!out||d<0||d>=n||!vc||!ic||!idx||!positions||
    vc[d]<3||vc[d]>2048||!positions[d]||!idx[d])return 0;
 StudioLayerVertex vertices[2048];
 for(int i=0;i<vc[d];++i)
  vertices[i]=(StudioLayerVertex){positions[d][i].X,positions[d][i].Y};
 return studio_layer_component_suggest(idx[d],ic[d],vertices,vc[d],
                                     component,out);
}
static int studio_selected_topology(int drawable,StudioLayerTopology *out){
 int n=csmGetDrawableCount(s.core.model);
 const int *vc=csmGetDrawableVertexCounts(s.core.model);
 const int *ic=csmGetDrawableIndexCounts(s.core.model);
 const unsigned short **indices=csmGetDrawableIndices(s.core.model);
 if(!out||drawable<0||drawable>=n||!vc||!ic||!indices||
    vc[drawable]<3||vc[drawable]>2048||ic[drawable]<3||
    ic[drawable]>65535||!indices[drawable])return 0;
 return studio_layer_components(indices[drawable],ic[drawable],vc[drawable],out);
}
static int studio_propose_layer(int drawable){
 StudioLayerSuggestion next;
 if(!studio_suggest_drawable(drawable,&next)){s.layer_proposal_valid=0;return 0;}
 s.layer_topology_valid=studio_selected_topology(drawable,&s.layer_topology);
 s.layer_proposal=next;
 s.layer_proposal_drawable=drawable;
 s.layer_proposal_signature=signature();
 s.layer_proposal_valid=1;
 return 1;
}
static int studio_adopt_proposal(void){
 if(!s.layer_proposal_valid||
    s.layer_proposal_drawable!=s.rig.drawable||
    s.layer_proposal_signature!=signature()||
    s.layer_proposal.kind==STUDIO_LAYER_UNKNOWN||
    s.layer_proposal.confidence<80)return 0;
 StudioRig next=s.rig;
 for(int i=0;i<s.layer_proposal.anchor_count;++i){
  const int v=s.layer_proposal.anchors[i];
  int present=0;
  for(int j=0;j<next.anchor_count;++j)if(next.anchors[j]==v)present=1;
  if(!present&&!studio_rig_toggle_anchor(&next,v))return 0;
 }
 s.rig=next;
 return 1;
}
static void rig_ui(float x){
 int d=s.xpbd_first,n=csmGetDrawableCount(s.core.model);
 if(d>=n||d<0)return;
 const int *vc=csmGetDrawableVertexCounts(s.core.model);
 if(s.rig.vertex_count!=vc[d]||s.rig.drawable!=d){
     if(!studio_rig_bank_select(&s.rig_bank,&s.rig,d,signature(),vc[d]))return;
   s.rig_selected=0;s.rig_beam_start=-1;s.rig_parent_start=-1;
 }
 label("LAYER RIG EDITOR",x,190,16,ACCENT);
  if(button((Rectangle){x+180,185,100,30},
       (s.rig_advanced==0?"Hierarchy":(s.rig_advanced==1?"Cross":"Basic")),s.rig_advanced!=0))
    s.rig_advanced=(s.rig_advanced+1)%3;
 label(TextFormat("Drawable %d / %d",d,n),x,224,14,TEXT);
 if(button((Rectangle){x+176,218,40,27},"-",0))s.xpbd_first=(d+n-1)%n;
 if(button((Rectangle){x+226,218,40,27},"+",0))s.xpbd_first=(d+1)%n;
 label(TextFormat("Vertex: %d / %d",s.rig_selected,vc[d]),x,264,14,TEXT);
 if(button((Rectangle){x+176,260,40,27},"-",0))s.rig_selected=(s.rig_selected+vc[d]-1)%vc[d];
 if(button((Rectangle){x+226,260,40,27},"+",0))s.rig_selected=(s.rig_selected+1)%vc[d];
 if(button((Rectangle){x,315,280,33},"Toggle anchor",0)){
   studio_rig_toggle_anchor(&s.rig,s.rig_selected);
 }
 if(button((Rectangle){x,358,280,33},s.rig_beam_start<0?"Start support beam":"Finish support beam",0)){
   if(s.rig_beam_start<0)s.rig_beam_start=s.rig_selected;
   else {studio_rig_toggle_beam(&s.rig,s.rig_beam_start,s.rig_selected);s.rig_beam_start=-1;}
 }
 label(TextFormat("Anchors %d / Support beams %d",s.rig.anchor_count,s.rig.beam_count),x,410,14,ACCENT);
 if(button((Rectangle){x,454,132,31},"Save rig",0)){
  char path[1200];snprintf(path,sizeof path,"%s/rig-layer-%d.txt",s.folder,d);
  toast(studio_rig_save(&s.rig,path)?"Rig saved locally.":"Save rig failed.");
 }
 if(button((Rectangle){x+144,454,136,31},"Load rig",0)){
  char path[1200];snprintf(path,sizeof path,"%s/rig-layer-%d.txt",s.folder,d);
  toast(studio_rig_load(&s.rig,path,signature(),vc[d])?"Rig loaded.":"Rig signature mismatch.");
 }
 if(button((Rectangle){x,505,280,34},"Apply rig to XPBD",0)){
  StudioXpbdOptions o={d,
       s.rig.cross_count?s.rig.cross_links[0].drawable:-1,
       0,s.xpbd_gravity,s.xpbd_wind,s.xpbd_compliance,
    s.xpbd_collision,s.xpbd_mass,s.xpbd_damping,s.xpbd_contact_compliance,
    s.xpbd_step,(int)s.xpbd_iters,(int)s.xpbd_pin,(int)s.xpbd_substeps,
    s.xpbd_pose_compliance,s.xpbd_source_transfer,s.xpbd_bending,s.xpbd_surface,
    (int)s.xpbd_anchors};
  void *next=studio_xpbd_bind_rig(s.core.model,o,&s.rig);
  if(next){studio_xpbd_disable(s.xpbd);s.xpbd=next;toast("Rig and support beams activated.");}
  else toast("Rig rejected: invalid vertex/mesh.");
  }
 label("Click vertex to select, Shift+click anchor",x,580,12,MUTED);
 label("Ctrl+click two vertices for a beam",x,600,12,MUTED);
 if(s.rig_advanced==1){
   label(TextFormat("Parents %d / Weights %d",
     s.rig.parent_count,s.rig.weight_count),x,633,13,ACCENT);
   if(button((Rectangle){x,660,135,32},"Mark parent",0))
     s.rig_parent_start=s.rig_selected;
   if(button((Rectangle){x+145,660,135,32},"Attach child",0)){
     int ok=s.rig_parent_start>=0&&
       studio_rig_parent(&s.rig,s.rig_parent_start,s.rig_selected,.85f,1);
     toast(ok?"Parent added; Apply rig separately.":"Parent rejected: invalid/cycle.");
     s.rig_parent_start=-1;
   }
   if(button((Rectangle){x,702,280,31},"Remove child's parent",0)){
     int done=0;
     for(int i=0;i<s.rig.parent_count;++i)
       if(s.rig.parents[i].child==s.rig_selected){
         done=studio_rig_parent(&s.rig,s.rig.parents[i].parent,
             s.rig_selected,s.rig.parents[i].stiffness,0);break;
       }
     toast(done?"Parent removed.":"Selected vertex has no parent.");
   }
   float influence=studio_rig_weight_get(&s.rig,s.rig_selected);
   label(TextFormat("Selected vertex weight: %.2f",influence),x,753,14,TEXT);
   if(slider(7701,(Rectangle){x,786,270,7},&influence,0,1))
     studio_rig_weight(&s.rig,s.rig_selected,influence);
   label("0=fixed; 1=full motion. Apply rig separately.",x,822,12,MUTED);
    label(TextFormat("Brush radius %.2f",s.rig_brush_radius),x,841,12,TEXT);
    label(TextFormat("Target %.2f",s.rig_brush_target),x+148,841,12,TEXT);
    slider(7711,(Rectangle){x,869,132,8},&s.rig_brush_radius,.01f,3.f);
    slider(7712,(Rectangle){x+148,869,132,8},&s.rig_brush_target,0,1);
    if(button((Rectangle){x,885,280,30},"Paint selected vertex region",0)){
      const csmVector2 **pos=csmGetDrawableVertexPositions(s.core.model);
      if(pos&&pos[d]&&vc[d]<=2048){
        StudioRigPoint points[2048];
        for(int j=0;j<vc[d];++j)
          points[j]=(StudioRigPoint){pos[d][j].X,pos[d][j].Y};
        int touched=0;
        toast(studio_rig_brush(&s.rig,points,vc[d],s.rig_selected,
          s.rig_brush_radius,s.rig_brush_target,&touched)?
          "Region painted; Apply rig separately.":
          "Weight brush rejected: geometry or capacity.");
      }
    }
 }else if(s.rig_advanced==2){
    label(TextFormat("Cross links %d",s.rig.cross_count),x,636,13,ACCENT);
    if(button((Rectangle){x,665,280,31},"Next target drawable",0)){
      s.xpbd_second=(s.xpbd_second+n+1)%n;
      if(s.xpbd_second==d)s.xpbd_second=(d+1)%n;
    }
    int target=s.xpbd_second;
    if(target>=0&&target<n&&target!=d&&vc[target]>=3){
      int count=vc[target];
      s.rig_cross_child=(s.rig_cross_child+count)%count;
      label(TextFormat("Target %d / child %d",target,s.rig_cross_child),
            x,710,12,TEXT);
      if(button((Rectangle){x,740,100,30},"Child -",0))
        s.rig_cross_child=(s.rig_cross_child+count-1)%count;
      if(button((Rectangle){x+112,740,100,30},"Child +",0))
        s.rig_cross_child=(s.rig_cross_child+1)%count;
      if(button((Rectangle){x,784,280,31},"Attach to selected parent",0))
        toast(studio_rig_cross(&s.rig,s.rig_selected,target,
          s.rig_cross_child,count,.85f,1)?
          "Cross link added; Apply rig.":"Cross link rejected.");
    }else label("Choose a second drawable first.",x,713,12,MUTED);
    label("Two drawable limit; no auto-linking.",x,838,12,MUTED);
  }else{
   if(button((Rectangle){x,640,135,32},"Analyze layer",0)){
    if(!studio_propose_layer(d))toast("Layer analysis unavailable.");
  }
  if(button((Rectangle){x+145,640,135,32},"Adopt anchors",0)){
    toast(studio_adopt_proposal()?"Suggested anchors added (not applied to XPBD).":
      "No confident layer proposal. Rig unchanged.");
  }
  if(s.layer_proposal_valid&&s.layer_proposal_drawable==d&&
      s.layer_proposal_signature==signature()){
    label(TextFormat("Type: %s  Confidence: %d%%",
      studio_layer_kind_name(s.layer_proposal.kind),s.layer_proposal.confidence),
      x,695,13,ACCENT);
    label(TextFormat("Region: %s  Shape: %s",
      studio_layer_region_name(s.layer_proposal.region),
      studio_layer_shape_name(s.layer_proposal.shape)),x,748,12,MUTED);
    label(TextFormat("Suggested anchors: %d (preview only)",
      s.layer_proposal.anchor_count),x,718,12,MUTED);
    if(s.layer_topology_valid)
      label(TextFormat("Islands %d  Loose %d  Triangles %d",
        s.layer_topology.islands,s.layer_topology.isolated,
        s.layer_topology.triangles),x,860,12,MUTED);
  }
   label(TextFormat("Confirmed role: %s",
     studio_layer_kind_name((StudioLayerKind)s.rig.role)),x,788,12,TEXT);
   const char *roles[]={"Unknown","Hair","Skirt","Strap"};
   for(int i=0;i<4;++i)
     if(button((Rectangle){x+i*71.f,815,67,29},roles[i],s.rig.role==i))
       studio_rig_set_role(&s.rig,i);
 }

}
static void xpbd_ui(float x){
 int n=csmGetDrawableCount(s.core.model);
 label("XPBD: maximum 2048 mesh vertices",x,188,12,ACCENT);
 label(TextFormat("Mesh A: %d",s.xpbd_first),x,224,14,TEXT);
 if(button((Rectangle){x+170,217,40,26},"-",0))s.xpbd_first=(s.xpbd_first+n-1)%n;
 if(button((Rectangle){x+221,217,40,26},"+",0))s.xpbd_first=(s.xpbd_first+1)%n;
 label(s.xpbd_second<0?"Mesh B: none":TextFormat("Mesh B: %d",s.xpbd_second),x,264,14,TEXT);
 if(button((Rectangle){x+170,257,40,26},"-",0))s.xpbd_second=(s.xpbd_second+n)%(n+1)-1;
 if(button((Rectangle){x+221,257,40,26},"+",0))s.xpbd_second=(s.xpbd_second+2)%(n+1)-1;
 if(button((Rectangle){x,304,135,30},"Forces",s.xpbd_page==0))s.xpbd_page=0;
 if(button((Rectangle){x+145,304,135,30},"Advanced",s.xpbd_page==1))s.xpbd_page=1;
 if(s.xpbd_page==0){
  control(41,x,361,"Gravity",&s.xpbd_gravity,-80,80);
  control(42,x,423,"Wind",&s.xpbd_wind,-60,60);
  control(43,x,485,"Edge compliance",&s.xpbd_compliance,0,.0002f);
  control(44,x,547,"Collision radius",&s.xpbd_collision,0,.15f);
  toggle(x,609,"Two-mesh joint",&s.xpbd_joint);
 }else{
  control(45,x,361,"Mass",&s.xpbd_mass,.1f,10);
  control(46,x,423,"Damping",&s.xpbd_damping,0,1);
  control(47,x,485,"Contact softness",&s.xpbd_contact_compliance,0,.001f);
  control(48,x,547,"Solver iterations",&s.xpbd_iters,1,24);
  control(49,x,609,"Step duration",&s.xpbd_step,1.f/240,1.f/60);
  control(50,x,671,"Pin vertex",&s.xpbd_pin,0,100);
  control(51,x,728,"Pose softness",&s.xpbd_pose_compliance,0,.02f);
  control(52,x,790,"Bending stiffness",&s.xpbd_bending,0,.002f);
 }
 if(button((Rectangle){x,771,135,32},"Apply XPBD",0)){
  StudioXpbdOptions o={s.xpbd_first,s.xpbd_second,s.xpbd_joint,s.xpbd_gravity,s.xpbd_wind,
   s.xpbd_compliance,s.xpbd_collision,s.xpbd_mass,s.xpbd_damping,s.xpbd_contact_compliance,
   s.xpbd_step,(int)s.xpbd_iters,(int)s.xpbd_pin,(int)s.xpbd_substeps,s.xpbd_pose_compliance,s.xpbd_source_transfer,s.xpbd_bending,s.xpbd_surface,(int)s.xpbd_anchors};
  void *next=studio_xpbd_bind(s.core.model,o);
  if(next){studio_xpbd_disable(s.xpbd);s.xpbd=next;toast("XPBD enabled with new parameters.");}
  else toast("XPBD rejected: invalid geometry or settings.");
 }
 if(button((Rectangle){x+145,771,135,32},"Disable",0)){
  studio_xpbd_disable(s.xpbd);s.xpbd=NULL;toast("Original MOC3 geometry restored.");
 }
 label(s.xpbd?"XPBD ON":"XPBD OFF",x,815,14,s.xpbd?ACCENT:MUTED);
}
static void inspector(void){
    float x=GetScreenWidth()-316.0f;label("MODEL INSPECTOR",x,110,12,MUTED);
    if(button((Rectangle){x,136,68,32},"Params",s.tab==0)){s.tab=0;s.scroll=0;}
    if(button((Rectangle){x+73,136,68,32},"Layers",s.tab==1)){s.tab=1;s.scroll=0;}
    if(button((Rectangle){x+146,136,68,32},"XPBD",s.tab==2)){s.tab=2;s.scroll=0;}
    if(button((Rectangle){x+219,136,61,32},"Rig",s.tab==3)){s.tab=3;s.scroll=0;}
    if(s.tab==3){rig_ui(x);return;}
    if(s.tab==2){xpbd_ui(x);return;}
    Rectangle search={x,180,280,32};DrawRectangleRounded(search,.2f,6,s.search_focus?LINE:BG);
    label(s.search[0]?s.search:"Search by ID...",x+10,188,14,s.search[0]?TEXT:MUTED);
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))s.search_focus=hit(search);
    if(s.search_focus){int key;while((key=GetCharPressed())>0)if(key>=32&&key<127&&strlen(s.search)<sizeof(s.search)-1){size_t n=strlen(s.search);s.search[n]=(char)key;s.search[n+1]=0;s.scroll=0;}
        if(IsKeyPressed(KEY_BACKSPACE)&&s.search[0]){s.search[strlen(s.search)-1]=0;s.scroll=0;}}
    Rectangle list={x,226,280,(float)GetScreenHeight()-352};
    const char **ids=s.tab?csmGetPartIds(s.core.model):csmGetParameterIds(s.core.model);
    int count=s.tab?s.parts:s.count,matches=0;for(int i=0;i<count;++i)if(contains(ids[i],s.search))++matches;
    float max_scroll=fmaxf(0,matches*66-list.height);if(hit(list))s.scroll=bound(s.scroll-GetMouseWheelMove()*48,0,max_scroll);
    s.scroll=bound(s.scroll,0,max_scroll);BeginScissorMode((int)x,(int)list.y,280,(int)list.height);
    float y=list.y-s.scroll;float *values=csmGetParameterValues(s.core.model);
    const float *lo=csmGetParameterMinimumValues(s.core.model),*hi=csmGetParameterMaximumValues(s.core.model);
    for(int i=0;i<count;++i){if(!contains(ids[i],s.search))continue;if(y+64>=list.y&&y<list.y+list.height){
        label(TextSubtext(ids[i],0,27),x,y+3,14,TEXT);
        float value=s.tab?s.part_values[i]:s.pins[i]?s.manual[i]:values[i];
        label(TextFormat("%.3f",value),x+213,y+3,13,MUTED);
        int previous=s.active_slider;
        if(!hit(list))s.active_slider=-1;
        if(slider(100+i+s.tab*MAX_ITEMS,(Rectangle){x+2,y+33,270,4},&value,s.tab?0:lo[i],s.tab?1:hi[i])){
            if(s.tab)s.part_values[i]=value;else {s.manual[i]=value;s.pins[i]=1;}}
        if(!hit(list))s.active_slider=previous;
        if(!s.tab&&s.pins[i]){label("PINNED",x,y+45,10,ACCENT);if(hit((Rectangle){x,y,280,60})&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))s.pins[i]=0;}
        DrawLine((int)x,(int)y+62,(int)x+280,(int)y+62,LINE);
    }y+=66;}
    EndScissorMode();
    if(max_scroll>0){float fraction=list.height/(matches*66);DrawRectangleRounded((Rectangle){x+279,list.y+(list.height-list.height*fraction)*s.scroll/max_scroll,3,list.height*fraction},1,6,MUTED);}
    label(TextFormat("%d / %d visible",matches,count),x,(float)GetScreenHeight()-116,13,MUTED);
    if(button((Rectangle){x,(float)GetScreenHeight()-90,280,30},"Clear manual overrides",0))reset_manual();
}
static void input(float dt){
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))s.active_slider=0;
    if(!s.search_focus){if(IsKeyPressed(KEY_SPACE))s.settings.playing=!s.settings.playing;
        if(IsKeyPressed(KEY_P))s.settings.physics=!s.settings.physics;
        if(IsKeyPressed(KEY_R)){FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;s.settings.zoom=1;}
        for(int i=0;i<9;++i)if(IsKeyPressed(KEY_ONE+i)){s.selected=i;s.settings.cycle=0;}
        if(IsKeyPressed(KEY_F12))capture("viewport",0);}
    if(hit(s.viewport)){
        Vector2 p=GetMousePosition();float gx=bound((p.x-s.viewport.x)/s.viewport.width*2-1,-1,1),gy=bound((p.y-s.viewport.y)/s.viewport.height*2-1,-1,1);
        s.gaze_x=mix(s.gaze_x,gx,1-expf(-dt*8));s.gaze_y=mix(s.gaze_y,gy,1-expf(-dt*8));
        s.settings.zoom=bound(s.settings.zoom*powf(1.1f,GetMouseWheelMove()),.3f,3);
        if(s.tab!=3&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!s.active_slider)s.dragging=(p.y-s.viewport.y)/s.viewport.height<.4f?1:(p.y-s.viewport.y)/s.viewport.height<.7f?2:3;
        if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){Vector2 d=GetMouseDelta();s.view.panx+=d.x*s.view.sw/s.viewport.width;s.view.pany+=d.y*s.view.sh/s.viewport.height;}
    }else{s.gaze_x*=expf(-dt*3);s.gaze_y*=expf(-dt*3);}
    if(s.dragging&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){Vector2 d=GetMouseDelta();s.pull_x=bound(s.pull_x+d.x*.35f,-35,35);s.pull_y=bound(s.pull_y-d.y*.25f,-25,25);}
    else s.dragging=0;
}
static void state_file(void){
    char path[1200],temporary[1200];snprintf(path,sizeof path,"%s/state.json",s.folder);
    snprintf(temporary,sizeof temporary,"%s/state.tmp",s.folder);FILE *f=fopen(temporary,"w");if(!f)return;
    float *v=csmGetParameterValues(s.core.model);double checksum=0;for(int i=0;i<s.count;++i)checksum+=v[i]*(i+1);
    int pins=0;for(int i=0;i<s.count;++i)pins+=s.pins[i]!=0;
    fprintf(f,"{\"frame\":%d,\"playing\":%d,\"expression\":%d,\"physics_available\":%d,\"physics_enabled\":%d,\"time\":%.6f,\"parameter_checksum\":%.9g,\"zoom\":%.3f,\"fps\":%d,\"pins\":%d,\"background\":%d,\"tab\":%d,\"speed\":%.4f,\"pan_x\":%.3f,\"pan_y\":%.3f}\n",s.frame,s.settings.playing,s.selected,s.linked!=NULL,s.settings.physics,s.time,checksum,s.settings.zoom,GetFPS(),pins,s.settings.background,s.tab,s.settings.speed,s.view.panx,s.view.pany);
    if(!fclose(f))studio_replace_file(temporary,path);
    StudioXpbdMetrics m={0};
    if(studio_xpbd_metrics(s.xpbd,&m)){
        char dest[1200];snprintf(dest,sizeof dest,"%s/xpbd_metrics.json",s.folder);
        FILE *mf=fopen(dest,"w");
        if(mf){fprintf(mf,"{\"frame\":%d,\"xpbd_max_step\":%.8g,\"xpbd_max_acceleration\":%.8g,\"xpbd_pin_drift\":%.8g,\"xpbd_current_step\":%.8g,\"accepted_frames\":%d,\"interaction_pull_x\":%.8g,\"interaction_pull_y\":%.8g}\n",
            s.frame,m.max_step,m.max_acceleration,m.pin_drift,m.current_step,m.accepted_frames,s.pull_x,s.pull_y);fclose(mf);}
    }
}
static double parameter_checksum(void){
    const float *v=csmGetParameterValues(s.core.model);double sum=0;
    for(int i=0;i<s.count;++i)sum+=v[i]*(i+1);return sum;
}
static void verify(int pass,const char *name){
    ++s.checks;if(!pass){++s.failures;fprintf(stderr,"Studio check FAILED: %s\n",name);}
}
#include "studio_agent_extended.inc"
static void process_agent_request(void){
 char path[1200],out[1200],token[48]={0},op[48]={0};
 snprintf(path,sizeof path,"%s/api/request.txt",s.folder);
 FILE *f=fopen(path,"r");if(!f)return;
 long wire_bytes=-1;
 if(fseek(f,0,SEEK_END)==0){wire_bytes=ftell(f);rewind(f);}
 if(fscanf(f,"%47s %47s",token,op)!=2){fclose(f);remove(path);return;}
 int ok=0;char receipt[96]="rejected";
 int header_valid=strlen(token)==24;
 for(int k=0;k<24&&header_valid;++k)
  if(!((token[k]>='0'&&token[k]<='9')||
       (token[k]>='a'&&token[k]<='f')))header_valid=0;
 if(wire_bytes<27||wire_bytes>512||!header_valid)goto finish_request;
 int d=s.xpbd_first,n=csmGetDrawableCount(s.core.model);
 if(!strcmp(op,"RIG_SELECT")){
  int v,tail;if(fscanf(f,"%d",&v)==1&&studio_agent_eof(f)&&v>=0&&v<n){
    const int *vc=csmGetDrawableVertexCounts(s.core.model);
    ok=studio_rig_bank_select(&s.rig_bank,&s.rig,v,signature(),vc[v]);
    if(ok){s.xpbd_first=v;s.tab=3;s.rig_selected=0;s.rig_beam_start=-1;s.rig_parent_start=-1;
      snprintf(receipt,sizeof receipt,"drawable=%d",v);}
  }
 }else if(!strcmp(op,"RIG_INSPECT")){
  int tail;
  if(studio_agent_eof(f)&&s.rig.vertex_count>0){
   ok=1;
   snprintf(receipt,sizeof receipt,"drawable=%d anchors=%d beams=%d role=%s parents=%d weights=%d cross=%d",
       s.rig.drawable,s.rig.anchor_count,s.rig.beam_count,
       studio_layer_kind_name((StudioLayerKind)s.rig.role),
        s.rig.parent_count,s.rig.weight_count,s.rig.cross_count);
  }
 }else if(!strcmp(op,"LAYER_COMPONENT_INSPECT")||
           !strcmp(op,"LAYER_COMPONENT_ADOPT")){
  int component;char extra;StudioLayerComponent detail={0};
  if(fscanf(f,"%d",&component)==1&&fscanf(f," %c",&extra)==EOF&&
     component>=0&&component<2048&&studio_component_suggest(component,&detail)){
   int changing=!strcmp(op,"LAYER_COMPONENT_ADOPT");
   int present=0;
   if(changing){
    if(s.rig.drawable!=s.xpbd_first||s.rig.vertex_count<=0||
       s.rig.model_signature!=signature())changing=-1;
    if(changing>0)for(int j=0;j<s.rig.anchor_count;++j)
     if(s.rig.anchors[j]==detail.anchor)present=1;
    if(changing>0&&!present&&!studio_rig_toggle_anchor(&s.rig,detail.anchor))changing=-1;
   }
   ok=changing>=0;
   snprintf(receipt,sizeof receipt,
     "component=%d vertices=%d root=%d anchor=%d adopted=%d",
     component,detail.vertices,detail.root,detail.anchor,changing);
  }
 }else if(!strcmp(op,"LAYER_TOPOLOGY_SCAN")){
  char extra;
  const int n=csmGetDrawableCount(s.core.model);
  const int *v=csmGetDrawableVertexCounts(s.core.model);
  const int *ic=csmGetDrawableIndexCounts(s.core.model);
  const unsigned short **idx=csmGetDrawableIndices(s.core.model);
  if(fscanf(f," %c",&extra)==EOF&&n>0&&n<=4096&&v&&ic&&idx){
   int multi=0,loose=0,bad=0,maximum=0,good=0;
   for(int j=0;j<n;++j){
    StudioLayerTopology top={0};
    if(!studio_layer_components(idx[j],ic[j],v[j],&top)){++bad;continue;}
    ++good;
    if(top.islands>1)++multi;
    if(top.isolated>0)++loose;
    if(top.islands>maximum)maximum=top.islands;
   }
   ok=1;
   snprintf(receipt,sizeof receipt,
      "total=%d analyzed=%d multi=%d loose=%d invalid=%d max_islands=%d",
      n,good,multi,loose,bad,maximum);
  }
 }else if(!strcmp(op,"LAYER_TOPOLOGY")){
  char extra;
  StudioLayerTopology top={0};
  if(fscanf(f," %c",&extra)==EOF&&studio_selected_topology(s.xpbd_first,&top)){
   ok=1;
   snprintf(receipt,sizeof receipt,"islands=%d used=%d loose=%d largest=%d triangles=%d",
       top.islands,top.used,top.isolated,top.largest,top.triangles);
  }
 }else if(!strcmp(op,"LAYER_SCAN")){
  double scan_started=GetTime();
   int tail;const int count=csmGetDrawableCount(s.core.model);float low,high;
  if(studio_agent_eof(f)&&count>=0&&count<=4096&&studio_model_y_bounds(&low,&high)){
    const int *counts=csmGetDrawableVertexCounts(s.core.model);
    const csmVector2 **positions=csmGetDrawableVertexPositions(s.core.model);
    const char **ids=csmGetDrawableIds(s.core.model);
    int cats[4]={0},regions[5]={0},unsupported=0;ok=1;
    if(!counts||!positions||!ids)ok=0;
    for(int j=0;ok&&j<count;++j){
     StudioLayerSuggestion suggestion={0};
     if(!studio_suggest_drawable_cached(j,count,counts,positions,ids,low,high,&suggestion)){
      ++cats[0];++unsupported;continue;
     }
     if(suggestion.kind>=STUDIO_LAYER_UNKNOWN&&suggestion.kind<=STUDIO_LAYER_STRAP)
      ++cats[suggestion.kind];
     if(suggestion.region>=1&&suggestion.region<=4)
      ++regions[suggestion.region];
    }
    snprintf(receipt,sizeof receipt,
      "total=%d hair=%d skirt=%d strap=%d unknown=%d up=%d mid=%d low=%d span=%d bad=%d",
      count,cats[1],cats[2],cats[3],cats[0],
      regions[1],regions[2],regions[3],regions[4],unsupported);
    if(ok){size_t used=strlen(receipt);
      if(used<sizeof receipt)snprintf(receipt+used,sizeof receipt-used," ms=%d",
        (int)((GetTime()-scan_started)*1000));}
   }
 }else if(!strcmp(op,"LAYER_SUGGEST")){
  int tail;
  if(studio_agent_eof(f)&&studio_propose_layer(s.xpbd_first)){
   ok=1;
   snprintf(receipt,sizeof receipt,
      "kind=%s confidence=%d anchors=%d region=%s shape=%s",
      studio_layer_kind_name(s.layer_proposal.kind),
      s.layer_proposal.confidence,s.layer_proposal.anchor_count,
      studio_layer_region_name(s.layer_proposal.region),
      studio_layer_shape_name(s.layer_proposal.shape));
  }
 }else if(!strcmp(op,"LAYER_ADOPT")){
  int tail;
  if(studio_agent_eof(f)&&studio_adopt_proposal()){
   ok=1;
   snprintf(receipt,sizeof receipt,"anchors=%d pending_rig_apply=1",
     s.rig.anchor_count);
  }
 }else if(!strcmp(op,"LAYER_LABEL")){
  char name[16];int tail;
  if(fscanf(f,"%15s",name)==1&&studio_agent_eof(f)&&s.rig.vertex_count>0){
   int role=-1;
   if(!strcmp(name,"unknown"))role=0;
   if(!strcmp(name,"hair"))role=1;
   if(!strcmp(name,"skirt"))role=2;
   if(!strcmp(name,"strap"))role=3;
   if(role>=0&&studio_rig_set_role(&s.rig,role)){
     ok=1;snprintf(receipt,sizeof receipt,"role=%s save_pending=1",name);
   }
  }
 }else if(!strcmp(op,"RIG_CROSS_INSPECT")){
   char extra;
   if(fscanf(f," %c",&extra)==EOF&&s.rig.vertex_count>0){
     ok=1;int target=s.rig.cross_count?s.rig.cross_links[0].drawable:-1;
     snprintf(receipt,sizeof receipt,"cross=%d target=%d pending_apply=1",
        s.rig.cross_count,target);
   }
 }else if(!strcmp(op,"RIG_CROSS")){
   int target,parent,child,enabled;float stiffness;char extra;
   if(fscanf(f,"%d %d %d %f %d",&target,&parent,&child,
          &stiffness,&enabled)==5&&
      fscanf(f," %c",&extra)==EOF&&s.rig.vertex_count>0&&
      s.rig.drawable==s.xpbd_first&&target>=0&&
      target<csmGetDrawableCount(s.core.model)){
     const int *vc=csmGetDrawableVertexCounts(s.core.model);
     StudioRig draft=s.rig;
     if(vc&&studio_rig_cross(&draft,parent,target,child,vc[target],
                             stiffness,enabled)){
       s.rig=draft;ok=1;
       snprintf(receipt,sizeof receipt,"cross=%d target=%d pending_apply=1",
         s.rig.cross_count,target);
     }
   }
 }else if(!strcmp(op,"RIG_BRUSH")){
   float radius,weight;char extra;
   const int d=s.rig.drawable,n=csmGetDrawableCount(s.core.model);
   if(fscanf(f,"%f %f",&radius,&weight)==2&&
      fscanf(f," %c",&extra)==EOF&&
      d>=0&&d<n&&d==s.xpbd_first&&s.rig.vertex_count>0){
     const int *counts=csmGetDrawableVertexCounts(s.core.model);
     const csmVector2 **positions=csmGetDrawableVertexPositions(s.core.model);
     if(counts&&positions&&counts[d]==s.rig.vertex_count&&positions[d]&&
        counts[d]<=2048&&s.rig_selected>=0&&s.rig_selected<counts[d]){
       StudioRigPoint points[2048];
       for(int i=0;i<counts[d];++i)
         points[i]=(StudioRigPoint){positions[d][i].X,positions[d][i].Y};
       int affected=0;
       if(studio_rig_brush(&s.rig,points,counts[d],s.rig_selected,
                           radius,weight,&affected)){
         ok=1;snprintf(receipt,sizeof receipt,
           "touched=%d weights=%d pending_apply=1",
           affected,s.rig.weight_count);
       }
     }
   }
 }else if(!strcmp(op,"RIG_PARENT_INSPECT")){
   int child,tail;
   if(fscanf(f,"%d",&child)==1&&studio_agent_eof(f)&&
      child>=0&&child<s.rig.vertex_count){
     ok=1;int parent=-1;float stiffness=0;
     for(int i=0;i<s.rig.parent_count;++i)
       if(s.rig.parents[i].child==child){
         parent=s.rig.parents[i].parent;
         stiffness=s.rig.parents[i].stiffness;
         break;
       }
     snprintf(receipt,sizeof receipt,"child=%d parent=%d stiffness=%.7g",
        child,parent,stiffness);
   }
 }else if(!strcmp(op,"RIG_WEIGHT_INSPECT")){
   int vertex,tail;
   if(fscanf(f,"%d",&vertex)==1&&studio_agent_eof(f)&&
      vertex>=0&&vertex<s.rig.vertex_count){
     const float weight=studio_rig_weight_get(&s.rig,vertex);
     if(weight>=0){ok=1;snprintf(receipt,sizeof receipt,
       "vertex=%d weight=%.7g",vertex,weight);}
   }
 }else if(!strcmp(op,"RIG_WEIGHT")){
   int vertex,tail;float weight;
   if(fscanf(f,"%d %f",&vertex,&weight)==2&&
      studio_agent_eof(f)&&s.rig.vertex_count>0){
     StudioRig candidate=s.rig;
     if(studio_rig_weight(&candidate,vertex,weight)){
       s.rig=candidate;ok=1;
       snprintf(receipt,sizeof receipt,
         "vertex=%d weight=%.7g pending_apply=1",vertex,weight);
     }
   }
 }else if(!strcmp(op,"RIG_PARENT")){
   int parent,child,enabled,tail;float stiffness;
   if(fscanf(f,"%d %d %f %d",&parent,&child,&stiffness,&enabled)==4&&
      studio_agent_eof(f)&&s.rig.vertex_count>0){
     StudioRig candidate=s.rig;
     if(studio_rig_parent(&candidate,parent,child,stiffness,enabled)){
       s.rig=candidate;ok=1;
       snprintf(receipt,sizeof receipt,
         "parents=%d pending_apply=1",s.rig.parent_count);
     }
   }
 }else if(!strcmp(op,"LAYER_COMPONENT_RELATION")){
   int first,second,tail;
   if(fscanf(f,"%d %d",&first,&second)==2&&
      studio_agent_eof(f)&&first>=0&&second>=0&&
      first!=second&&first<2048&&second<2048){
     const int d=s.xpbd_first,n=csmGetDrawableCount(s.core.model);
     const int *vc=csmGetDrawableVertexCounts(s.core.model);
     const int *ic=csmGetDrawableIndexCounts(s.core.model);
     const unsigned short **idx=csmGetDrawableIndices(s.core.model);
     const csmVector2 **pos=csmGetDrawableVertexPositions(s.core.model);
     if(d>=0&&d<n&&vc&&ic&&idx&&pos&&vc[d]>=3&&
         vc[d]<=2048&&pos[d]&&idx[d]){
       StudioLayerVertex coords[2048];
       for(int j=0;j<vc[d];++j)
         coords[j]=(StudioLayerVertex){pos[d][j].X,pos[d][j].Y};
       StudioLayerRelation rel={0};
       if(studio_layer_component_relation(idx[d],ic[d],coords,vc[d],
                first,second,&rel)){
         ok=1;
         snprintf(receipt,sizeof receipt,
           "a=%d b=%d nearest_a=%d nearest_b=%d gap=%.8g",
           first,second,rel.vertex_a,rel.vertex_b,rel.distance);
       }
     }
   }
 }else if(!strcmp(op,"RIG_ANCHOR")||!strcmp(op,"RIG_BEAM")){
  int a,b,e,tail;int beam=!strcmp(op,"RIG_BEAM");
  int parsed=beam?fscanf(f,"%d %d %d",&a,&b,&e):fscanf(f,"%d %d",&a,&e);
  if(parsed==(beam?3:2)&&studio_agent_eof(f)&&e>=0&&e<=1&&s.rig.vertex_count>0){
   if(beam){
    int exists=0;for(int i=0;i<s.rig.beam_count;++i)
      if(s.rig.beams[i].a==(a<b?a:b)&&s.rig.beams[i].b==(a<b?b:a))exists=1;
    ok=(exists==e)||studio_rig_toggle_beam(&s.rig,a,b);
    snprintf(receipt,sizeof receipt,"beams=%d",s.rig.beam_count);
   }else{
    int exists=0;for(int i=0;i<s.rig.anchor_count;++i)if(s.rig.anchors[i]==a)exists=1;
    ok=(exists==e)||studio_rig_toggle_anchor(&s.rig,a);
    snprintf(receipt,sizeof receipt,"anchors=%d",s.rig.anchor_count);
   }
  }
 }else if(!strcmp(op,"PHYSICS_SET")){
  float g,w,dec;int tail;
  if(fscanf(f,"%f %f %f",&g,&w,&dec)==3&&studio_agent_eof(f)&&isfinite(g)&&isfinite(w)&&isfinite(dec)&&
   g>=-80&&g<=80&&w>=-60&&w<=60&&dec>=0&&dec<=1){
   s.xpbd_gravity=g;s.xpbd_wind=w;s.xpbd_damping=dec;ok=1;
   snprintf(receipt,sizeof receipt,"settings=updated");
  }
 }else if(!strcmp(op,"ACTION")){
  char kind[24];float strength;int tail;
  if(fscanf(f,"%23s %f",kind,&strength)==2&&studio_agent_eof(f)&&isfinite(strength)&&strength>=0&&strength<=1){
   if(!strcmp(kind,"pull")){s.pull_x=bound(s.pull_x+strength*12,-35,35);ok=1;}
   if(!strcmp(kind,"release")){s.dragging=0;s.pull_x=0;s.pull_y=0;ok=1;}
   if(!strcmp(kind,"shake")){s.pull_x=bound(s.pull_x-strength*15,-35,35);ok=1;}
   if(!strcmp(kind,"settle")){s.pull_x=0;s.pull_y=0;ok=1;}
   if(ok)snprintf(receipt,sizeof receipt,"action=%s",kind);
  }
 }else if(!strcmp(op,"RIG_APPLY")&&studio_agent_eof(f)&&s.rig.vertex_count>0){
  StudioXpbdOptions opt={s.rig.drawable,
     s.rig.cross_count?s.rig.cross_links[0].drawable:-1,0,s.xpbd_gravity,s.xpbd_wind,
    s.xpbd_compliance,s.xpbd_collision,s.xpbd_mass,s.xpbd_damping,s.xpbd_contact_compliance,
    s.xpbd_step,(int)s.xpbd_iters,(int)s.xpbd_pin,(int)s.xpbd_substeps,
    s.xpbd_pose_compliance,s.xpbd_source_transfer,s.xpbd_bending,s.xpbd_surface,(int)s.xpbd_anchors};
  void *next=studio_xpbd_bind_rig(s.core.model,opt,&s.rig);
  if(next){studio_xpbd_disable(s.xpbd);s.xpbd=next;ok=1;snprintf(receipt,sizeof receipt,"rig=applied");}
 }
 if(!ok){int extra=studio_agent_extended(f,op,receipt,sizeof receipt);if(extra>=0)ok=extra;}
finish_request:
 fclose(f);remove(path);
 char temp_reply[1200];
 int nout=snprintf(out,sizeof out,"%s/api/response.txt",s.folder);
 int ntmp=snprintf(temp_reply,sizeof temp_reply,"%s/api/response.tmp",s.folder);
 if(nout>0&&nout<(int)sizeof out&&ntmp>0&&ntmp<(int)sizeof temp_reply){
  FILE *w=fopen(temp_reply,"wb");
  if(w){
   int wrote=fprintf(w,"%s %s %s\n",token,ok?"OK":"ERR",receipt);
   int flushed=fflush(w);
   int closed=fclose(w);
   if(wrote>0&&flushed==0&&closed==0){
    if(!studio_replace_file(temp_reply,out))remove(temp_reply);
   }else remove(temp_reply);
  }
 }
}
static void frame(void){
    float dt=bound(GetFrameTime(),.001f,.05f);int w=GetScreenWidth(),h=GetScreenHeight();
    s.viewport=(Rectangle){288,136,(float)w-636,(float)h-250};
    int rw=(int)(s.viewport.width*STUDIO_RENDER_SCALE),rh=(int)(s.viewport.height*STUDIO_RENDER_SCALE);
    if(rw>1280){rh=(int)(rh*1280.0f/rw);rw=1280;}
    if(s.target.texture.width!=rw||s.target.texture.height!=rh){
        RenderTexture2D next=LoadRenderTexture(rw,rh);if(next.id){if(s.target.id)UnloadRenderTexture(s.target);s.target=next;
            float ratio=s.view.sw?(float)rw/s.view.sw:1;s.view.sw=rw;s.view.sh=rh;s.base_zoom*=ratio;RendererResize(&s.renderer,rw,rh);SetTextureFilter(s.target.texture,TEXTURE_FILTER_BILINEAR);}
    }
    process_agent_request();if(!s.smoke)input(dt);update(dt);s.view.zoom=s.base_zoom*s.settings.zoom;
    if(s.xpbd_smoke&&s.frame==12){int count=csmGetDrawableCount(s.core.model);const int *vertices=csmGetDrawableVertexCounts(s.core.model);
      for(int d=0;d<count;++d)if(vertices[d]>=3&&vertices[d]<=2048){StudioXpbdOptions o={d,-1,0,-12,3,.0000001f,0,1,.995f,0,1.f/120,8,0,3,.004f,.08f,.00001f,.02f,2};
        s.xpbd=studio_xpbd_bind(s.core.model,o);if(s.xpbd)break;}
      verify(s.xpbd!=NULL,"XPBD binds a physical drawable");}
    if(s.xpbd_motion_case&&s.frame==44&&s.xpbd){
      StudioXpbdOptions o={s.xpbd_first,-1,0,(s.xpbd_motion_case==1?60:-30),(s.xpbd_motion_case==2?55:-20),.000001f,0,2,(s.xpbd_motion_case==3?.2f:.995f),0,1.f/120,12,0,3,.004f,.08f,.00001f,.02f,2};
      studio_xpbd_disable(s.xpbd);s.xpbd=studio_xpbd_bind(s.core.model,o);if(s.xpbd)++s.xpbd_motion_rebinds;else verify(0,"XPBD parameter rebind");}
    if(s.xpbd_motion_case&&s.frame==74&&s.xpbd){
      StudioXpbdOptions o={s.xpbd_first,-1,0,-12,10,.0000001f,0,1,.90f,0,1.f/240,16,0,3,.004f,.08f,.00001f,.02f,2};
      studio_xpbd_disable(s.xpbd);s.xpbd=studio_xpbd_bind(s.core.model,o);if(s.xpbd)++s.xpbd_motion_rebinds;else verify(0,"XPBD second rebind");}
    if(s.xpbd_smoke&&s.frame==90){verify(!s.xpbd_motion_case||s.xpbd_motion_rebinds==2,"XPBD two parameter changes");verify(s.xpbd_smoke_frames>=20,"XPBD prepare/render/accept continuity");
      studio_xpbd_disable(s.xpbd);s.xpbd=NULL;}
    if(s.smoke){const float *v=csmGetParameterValues(s.core.model),*lo=csmGetParameterMinimumValues(s.core.model),*hi=csmGetParameterMaximumValues(s.core.model);
        for(int i=0;i<s.count;++i)if(!isfinite(v[i])||v[i]<lo[i]||v[i]>hi[i]){verify(0,"finite bounded parameters");break;}
        if(s.physics_expected&&!s.linked)verify(0,"linked physics continuity");}
    studio_xpbd_set_interaction(s.xpbd,s.pull_x*8.f,s.pull_y*8.f);
    if(s.xpbd&&!studio_xpbd_prepare(s.xpbd,s.core.model)){studio_xpbd_disable(s.xpbd);s.xpbd=NULL;toast("XPBD stopped: invalid geometry.");}
    BeginTextureMode(s.target);stage_background();RenderModel(s.core.model,&s.renderer,&s.view,1,&s.opt,s.sorted);EndTextureMode();
    if(s.xpbd){rlDrawRenderBatchActive();glFinish();
        if(glGetError()!=GL_NO_ERROR||!studio_xpbd_accept(s.xpbd)){studio_xpbd_disable(s.xpbd);s.xpbd=NULL;toast("XPBD render failed.");}else if(s.xpbd_smoke)++s.xpbd_smoke_frames;
    }
    BeginDrawing();ClearBackground(BG);
    DrawRectangle(0,0,w,74,CARD);DrawCircle(35,34,13,ACCENT);label("A",29,23,22,BG);
    label("AVATAR STUDIO",59,18,23,TEXT);label("Interactive model lab",60,45,12,MUTED);
    label(TextSubtext(s.model_name,0,40),290,28,17,MUTED);
    if(button((Rectangle){(float)w-325,23,130,32},"Open model...",0)){char path[1024];if(studio_pick_model(path,sizeof path))load_model(path);}
    if(button((Rectangle){(float)w-180,23,140,32},s.settings.playing?"LIVE PREVIEW":"PAUSED",1))s.settings.playing=!s.settings.playing;
    card((Rectangle){16,90,256,(float)h-140});card((Rectangle){(float)w-332,90,316,(float)h-140});
    camera_ui();
    DrawTexturePro(s.target.texture,(Rectangle){0,0,(float)s.target.texture.width,-(float)s.target.texture.height},s.viewport,(Vector2){0,0},0,WHITE);
    rig_overlay_ui();
    DrawRectangleLinesEx(s.viewport,1,LINE);label(s.linked?"LINKED PHYSICS3":"NO LINKED PHYSICS3",s.viewport.x+14,s.viewport.y+14,11,s.linked?ACCENT:MUTED);
    if(s.dragging)label(s.dragging==1?"PULL / HEAD":s.dragging==2?"PULL / ARMS":"PULL / BODY",s.viewport.x+14,s.viewport.y+35,12,ACCENT);
    float y=s.viewport.y+s.viewport.height+14;
    label("SCENE",s.viewport.x,y+8,12,MUTED);
    for(int i=0;i<4;++i)if(button((Rectangle){s.viewport.x+62+i*75,y,68,28},(const char*[]){"Night","Light","Ocean","Checker"}[i],s.settings.background==i))s.settings.background=i;
    if(button((Rectangle){s.viewport.x+370,y,60,28},"Grid",s.settings.grid))s.settings.grid=!s.settings.grid;
    if(button((Rectangle){s.viewport.x+438,y,72,28},"Particles",s.settings.particles))s.settings.particles=!s.settings.particles;
    s.fps_history[s.history++%120]=dt*1000;
    y+=42;label(TextFormat("FRAME %.1f ms",dt*1000),s.viewport.x,y,12,MUTED);
    for(int i=1;i<120;++i){float a=s.fps_history[(s.history+i-1)%120],b=s.fps_history[(s.history+i)%120];
        DrawLine((int)(s.viewport.x+150+(i-1)*(s.viewport.width-150)/120),(int)(y+20-bound(a,0,50)*.4f),(int)(s.viewport.x+150+i*(s.viewport.width-150)/120),(int)(y+20-bound(b,0,50)*.4f),Fade(ACCENT,.6f));}
    left_ui();inspector();
    DrawRectangle(0,h-34,w,34,CARD);label("LMB pull   /   RMB pan   /   Wheel zoom   /   Space pause   /   P physics   /   1-9 expression",20,(float)h-24,12,MUTED);
    label("Local preview",(float)w-120,(float)h-24,12,ACCENT);
    if(GetTime()<s.notice_until){Vector2 size=MeasureTextEx(s.font,s.notice,15,.5f);Rectangle r={s.viewport.x+12,s.viewport.y+s.viewport.height-42,size.x+24,30};DrawRectangleRounded(r,.25f,6,(Color){30,65,67,245});label(s.notice,r.x+12,r.y+7,15,TEXT);}
    EndDrawing();++s.frame;
    if(s.frame%15==0)state_file();
    if(s.smoke){
        if(s.frame==20){capture("01-idle",0);capture("studio-ui",1);}
        if(s.frame==40)s.selected=1;
        if(s.frame==70){capture("02-happy",0);save_preset();float wind=s.settings.wind;s.settings.wind=.8f;load_preset();verify(s.settings.wind==wind,"preset roundtrip");}
        if(s.frame==90){s.selected=5;s.settings.wind=.7f;}
        if(s.frame==130){capture("03-dance",0);s.settings.playing=0;s.paused_time=s.time;s.paused_checksum=parameter_checksum();}
        if(s.frame==145){capture("04-paused",0);verify(s.time==s.paused_time&&parameter_checksum()==s.paused_checksum,"pause freezes pose and clock");}
        if(s.frame==160){s.step_requested=1;s.settings.physics=0;}
        if(s.frame==180){s.settings.physics=1;s.settings.playing=1;s.settings.zoom=1.35f;}
        if(s.frame==210)capture("05-zoom",0);
        if(s.frame==220)SetWindowSize(1250,920);
        if(s.frame==235){capture("studio-minimum",1);verify(s.target.texture.width==921&&s.target.texture.height==1005,"resize viewport");}
        if(s.frame==245)SetWindowSize(1460,1000);
    }
}
int main(int argc,char **argv){
    const char *model=NULL;snprintf(s.folder,sizeof s.folder,"build/studio-user");
    for(int i=1;i<argc;++i){if(!strcmp(argv[i],"--model")&&i+1<argc)model=argv[++i];else if(!strcmp(argv[i],"--evidence")&&i+1<argc){const char *folder=argv[++i];if(strlen(folder)>=sizeof s.folder)return 2;snprintf(s.folder,sizeof s.folder,"%s",folder);}else if(!strcmp(argv[i],"--frames")&&i+1<argc){char *end;long n=strtol(argv[++i],&end,10);if(*end||n<1||n>1000000)return 2;s.limit=(int)n;}else if(!strcmp(argv[i],"--smoke"))s.smoke=1;else if(!strcmp(argv[i],"--xpbd-smoke"))s.xpbd_smoke=1;else if(!strcmp(argv[i],"--xpbd-motion-case")&&i+1<argc){s.xpbd_motion_case=atoi(argv[++i]);if(s.xpbd_motion_case<1||s.xpbd_motion_case>3)return 2;s.xpbd_smoke=1;}else return 2;}
    if(!model||(s.smoke&&s.limit<270))return 2;SetTraceLogLevel(LOG_WARNING);csmSetLogFunction(NULL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|FLAG_MSAA_4X_HINT);InitWindow(1460,1000,"Avatar Studio | Interactive Model Lab");SetWindowMinSize(1250,920);SetTargetFPS(60);SetExitKey(KEY_NULL);
    s.font=LoadUiFont();s.view.sw=824;s.view.sh=750;s.opt.maskScale=.75f;s.opt.maxOrder=1<<30;s.opt.onlyDrawable=-1;
    if(!load_model(model)){fprintf(stderr,"Studio model load failed: %s\n",s.notice);if(s.font.texture.id!=GetFontDefault().texture.id)UnloadFont(s.font);CloseWindow();return 3;}
    s.physics_expected=s.linked!=NULL;if(s.smoke)s.settings.gaze=0;
    while(!WindowShouldClose()&&(!s.limit||s.frame<s.limit))frame();
    state_file();
    if(s.xpbd_smoke){verify(s.xpbd_smoke_frames>=20,"XPBD accepted frames");
      fprintf(stderr,"XPBD_SMOKE_RESULT frames=%d accepted=%d checks=%d failures=%d active=%d\\n",s.frame,s.xpbd_smoke_frames,s.checks,s.failures,s.xpbd!=NULL);}
    if(s.smoke){verify(s.frame==s.limit,"complete frame count");verify(s.linked!=NULL||!s.physics_expected,"physics still available");
        verify(GetFPS()>0&&s.time>s.frame/120.0f,"real frame timing and presentation");
        char path[1200];snprintf(path,sizeof path,"%s/smoke.json",s.folder);FILE *f=fopen(path,"w");
        if(f){fprintf(f,"{\"checks\":%d,\"failures\":%d,\"frames\":%d,\"linked_physics\":%d}\n",s.checks,s.failures,s.frame,s.linked!=NULL);fclose(f);}else ++s.failures;
    }
    free_model();if(s.target.id)UnloadRenderTexture(s.target);if(s.font.texture.id!=GetFontDefault().texture.id)UnloadFont(s.font);CloseWindow();return s.failures?4:0;
}
