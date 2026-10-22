/* Avatar Studio: local interactive model lab, not public SDK qualification. */
#include "viewer.h"
#include "studio_backend.h"
#include <math.h>
#include <stdint.h>

#define MAX_ITEMS 4096
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
    int failures,checks,physics_expected;float paused_time;double paused_checksum;
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
    memset(s.weights,0,sizeof s.weights);s.weights[0]=1;s.physics_reset=1;
}
static void free_model(void){
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
    free_model();s.core=core;s.renderer=renderer;s.sorted=sorted;s.manual=manual;s.pins=pins;s.part_values=part_values;
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
static void save_preset(void){
    if(!s.core.model)return;char path[1200],temporary[1200];snprintf(path,sizeof path,"%s/preset.txt",s.folder);
    snprintf(temporary,sizeof temporary,"%s/preset.tmp",s.folder);
    FILE *f=fopen(temporary,"w");if(!f){toast("Cannot save preset.");return;}
    fprintf(f,"AVATAR_STUDIO_1 %llu %d %d\n",(unsigned long long)signature(),s.count,s.parts);
    Settings *q=&s.settings;
    fprintf(f,"%d %d %d %d %d %d %d %d %d %d\n",q->playing,q->physics,q->blink,q->breath,q->gaze,q->speech,q->cycle,q->grid,q->particles,q->background);
    fprintf(f,"%.9g %.9g %.9g %.9g %.9g %.9g %d\n",q->speed,q->strength,q->wind,q->blink_interval,q->cycle_interval,q->zoom,s.selected);
    for(int i=0;i<s.count;++i)fprintf(f,"%d %.9g\n",s.pins[i],s.manual[i]);
    for(int i=0;i<s.parts;++i)fprintf(f,"%.9g\n",s.part_values[i]);
    int failed=ferror(f);if(fclose(f))failed=1;
    if(!failed&&!studio_replace_file(temporary,path))failed=1;
    toast(failed?"Preset write failed; previous preset kept.":"Preset saved locally.");
}
static void load_preset(void){
    if(!s.core.model)return;char path[1200];snprintf(path,sizeof path,"%s/preset.txt",s.folder);FILE *f=fopen(path,"r");
    if(!f){toast("No saved preset yet.");return;}
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
    free(manual);free(pins);free(opacity);toast(ok?"Preset restored.":"Preset rejected; current settings kept.");
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
static void inspector(void){
    float x=GetScreenWidth()-316.0f;label("MODEL INSPECTOR",x,110,12,MUTED);
    if(button((Rectangle){x,136,136,32},"Parameters",s.tab==0)){s.tab=0;s.scroll=0;}
    if(button((Rectangle){x+144,136,136,32},"Layers",s.tab==1)){s.tab=1;s.scroll=0;}
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
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!s.active_slider)s.dragging=(p.y-s.viewport.y)/s.viewport.height<.4f?1:(p.y-s.viewport.y)/s.viewport.height<.7f?2:3;
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
}
static double parameter_checksum(void){
    const float *v=csmGetParameterValues(s.core.model);double sum=0;
    for(int i=0;i<s.count;++i)sum+=v[i]*(i+1);return sum;
}
static void verify(int pass,const char *name){
    ++s.checks;if(!pass){++s.failures;fprintf(stderr,"Studio check FAILED: %s\n",name);}
}
static void frame(void){
    float dt=bound(GetFrameTime(),.001f,.05f);int w=GetScreenWidth(),h=GetScreenHeight();
    s.viewport=(Rectangle){288,136,(float)w-636,(float)h-250};
    int rw=(int)s.viewport.width,rh=(int)s.viewport.height;
    if(rw>1280){rh=(int)(rh*1280.0f/rw);rw=1280;}
    if(s.target.texture.width!=rw||s.target.texture.height!=rh){
        RenderTexture2D next=LoadRenderTexture(rw,rh);if(next.id){if(s.target.id)UnloadRenderTexture(s.target);s.target=next;
            float ratio=s.view.sw?(float)rw/s.view.sw:1;s.view.sw=rw;s.view.sh=rh;s.base_zoom*=ratio;RendererResize(&s.renderer,rw,rh);SetTextureFilter(s.target.texture,TEXTURE_FILTER_BILINEAR);}
    }
    if(!s.smoke)input(dt);update(dt);s.view.zoom=s.base_zoom*s.settings.zoom;
    if(s.smoke){const float *v=csmGetParameterValues(s.core.model),*lo=csmGetParameterMinimumValues(s.core.model),*hi=csmGetParameterMaximumValues(s.core.model);
        for(int i=0;i<s.count;++i)if(!isfinite(v[i])||v[i]<lo[i]||v[i]>hi[i]){verify(0,"finite bounded parameters");break;}
        if(s.physics_expected&&!s.linked)verify(0,"linked physics continuity");}
    BeginTextureMode(s.target);stage_background();RenderModel(s.core.model,&s.renderer,&s.view,1,&s.opt,s.sorted);EndTextureMode();
    BeginDrawing();ClearBackground(BG);
    DrawRectangle(0,0,w,74,CARD);DrawCircle(35,34,13,ACCENT);label("A",29,23,22,BG);
    label("AVATAR STUDIO",59,18,23,TEXT);label("Interactive model lab",60,45,12,MUTED);
    label(TextSubtext(s.model_name,0,40),290,28,17,MUTED);
    if(button((Rectangle){(float)w-325,23,130,32},"Open model...",0)){char path[1024];if(studio_pick_model(path,sizeof path))load_model(path);}
    if(button((Rectangle){(float)w-180,23,140,32},s.settings.playing?"LIVE PREVIEW":"PAUSED",1))s.settings.playing=!s.settings.playing;
    card((Rectangle){16,90,256,(float)h-140});card((Rectangle){(float)w-332,90,316,(float)h-140});
    camera_ui();
    DrawTexturePro(s.target.texture,(Rectangle){0,0,(float)s.target.texture.width,-(float)s.target.texture.height},s.viewport,(Vector2){0,0},0,WHITE);
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
        if(s.frame==235){capture("studio-minimum",1);verify(s.target.texture.width==614&&s.target.texture.height==670,"resize viewport");}
        if(s.frame==245)SetWindowSize(1460,1000);
    }
}
int main(int argc,char **argv){
    const char *model=NULL;snprintf(s.folder,sizeof s.folder,"build/studio-user");
    for(int i=1;i<argc;++i){if(!strcmp(argv[i],"--model")&&i+1<argc)model=argv[++i];else if(!strcmp(argv[i],"--evidence")&&i+1<argc){const char *folder=argv[++i];if(strlen(folder)>=sizeof s.folder)return 2;snprintf(s.folder,sizeof s.folder,"%s",folder);}else if(!strcmp(argv[i],"--frames")&&i+1<argc){char *end;long n=strtol(argv[++i],&end,10);if(*end||n<1||n>1000000)return 2;s.limit=(int)n;}else if(!strcmp(argv[i],"--smoke"))s.smoke=1;else return 2;}
    if(!model||(s.smoke&&s.limit<270))return 2;SetTraceLogLevel(LOG_WARNING);csmSetLogFunction(NULL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|FLAG_MSAA_4X_HINT);InitWindow(1460,1000,"Avatar Studio | Interactive Model Lab");SetWindowMinSize(1250,920);SetTargetFPS(60);SetExitKey(KEY_NULL);
    s.font=LoadUiFont();s.view.sw=824;s.view.sh=750;s.opt.maskScale=.75f;s.opt.maxOrder=1<<30;s.opt.onlyDrawable=-1;
    if(!load_model(model)){fprintf(stderr,"Studio model load failed: %s\n",s.notice);if(s.font.texture.id!=GetFontDefault().texture.id)UnloadFont(s.font);CloseWindow();return 3;}
    s.physics_expected=s.linked!=NULL;if(s.smoke)s.settings.gaze=0;
    while(!WindowShouldClose()&&(!s.limit||s.frame<s.limit))frame();
    state_file();
    if(s.smoke){verify(s.frame==s.limit,"complete frame count");verify(s.linked!=NULL||!s.physics_expected,"physics still available");
        verify(GetFPS()>0&&s.time>s.frame/120.0f,"real frame timing and presentation");
        char path[1200];snprintf(path,sizeof path,"%s/smoke.json",s.folder);FILE *f=fopen(path,"w");
        if(f){fprintf(f,"{\"checks\":%d,\"failures\":%d,\"frames\":%d,\"linked_physics\":%d}\n",s.checks,s.failures,s.frame,s.linked!=NULL);fclose(f);}else ++s.failures;
    }
    free_model();if(s.target.id)UnloadRenderTexture(s.target);if(s.font.texture.id!=GetFontDefault().texture.id)UnloadFont(s.font);CloseWindow();return s.failures?4:0;
}
