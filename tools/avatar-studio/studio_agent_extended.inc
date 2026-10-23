/* Native Studio local agent extensions. No filesystem path from API inputs. */
static int studio_agent_eof(FILE *f){
 char extra;return fscanf(f," %c",&extra)==EOF;
}
static int studio_agent_extended(FILE *f,const char *op,char *receipt,size_t cap){
 int a=0,b=0,c=0;float x=0,y=0;
 char name[48]={0};
 if(!strcmp(op,"STUDIO_EPOCH")){
  if(!studio_agent_eof(f)||!s.core.model||!s.model_generation)return 0;
  snprintf(receipt,cap,"generation=%llu",
           (unsigned long long)s.model_generation);return 1;
 }
 if(!strcmp(op,"PARAM_INSPECT_AT")||!strcmp(op,"PART_INSPECT_AT")){
  unsigned long long generation=0;int index=-1;
  if(fscanf(f,"%llu %d",&generation,&index)!=2||!studio_agent_eof(f)||
     !generation||generation!=s.model_generation||!s.core.model)return 0;
  if(!strcmp(op,"PARAM_INSPECT_AT")){
   if(index<0||index>=s.count)return 0;
   const float *lo=csmGetParameterMinimumValues(s.core.model);
   const float *hi=csmGetParameterMaximumValues(s.core.model);
   float *v=csmGetParameterValues(s.core.model);
   if(!lo||!hi||!v)return 0;
   snprintf(receipt,cap,"index=%d value=%.7g low=%.7g high=%.7g pin=%d",
       index,v[index],lo[index],hi[index],s.pins[index]);return 1;
  }
  if(index<0||index>=s.parts)return 0;
  snprintf(receipt,cap,"index=%d opacity=%.7g",index,s.part_values[index]);return 1;
 }
 if(!strcmp(op,"STUDIO_TAB")||!strcmp(op,"STUDIO_BACKGROUND")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>3)return 0;
  if(!strcmp(op,"STUDIO_TAB")){s.tab=a;s.scroll=0;}
  else s.settings.background=a;
  snprintf(receipt,cap,"ui=%s index=%d",op,a);return 1;
 }
 if(!strcmp(op,"PARAM_INSPECT")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>=s.count)return 0;
  const float *lo=csmGetParameterMinimumValues(s.core.model);
  const float *hi=csmGetParameterMaximumValues(s.core.model);
  float *values=csmGetParameterValues(s.core.model);
  if(!lo||!hi||!values)return 0;
  snprintf(receipt,cap,"index=%d value=%.7g low=%.7g high=%.7g pin=%d",
    a,values[a],lo[a],hi[a],s.pins[a]);return 1;
 }
 if(!strcmp(op,"PART_INSPECT")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>=s.parts)return 0;
  snprintf(receipt,cap,"index=%d opacity=%.7g",a,s.part_values[a]);return 1;
 }
 if(!strcmp(op,"XPBD_READ")){
  if(fscanf(f,"%47s",name)!=1||!studio_agent_eof(f))return 0;
  const float *v=NULL;
  if(!strcmp(name,"gravity"))v=&s.xpbd_gravity;
  if(!strcmp(name,"wind"))v=&s.xpbd_wind;
  if(!strcmp(name,"damping"))v=&s.xpbd_damping;
  if(!strcmp(name,"edge_compliance"))v=&s.xpbd_compliance;
  if(!strcmp(name,"collision"))v=&s.xpbd_collision;
  if(!strcmp(name,"mass"))v=&s.xpbd_mass;
  if(!strcmp(name,"contact_compliance"))v=&s.xpbd_contact_compliance;
  if(!strcmp(name,"iterations"))v=&s.xpbd_iters;
  if(!strcmp(name,"step"))v=&s.xpbd_step;
  if(!strcmp(name,"pin"))v=&s.xpbd_pin;
  if(!strcmp(name,"substeps"))v=&s.xpbd_substeps;
  if(!strcmp(name,"pose"))v=&s.xpbd_pose_compliance;
  if(!strcmp(name,"transfer"))v=&s.xpbd_source_transfer;
  if(!strcmp(name,"bending"))v=&s.xpbd_bending;
  if(!strcmp(name,"surface"))v=&s.xpbd_surface;
  if(!strcmp(name,"anchors"))v=&s.xpbd_anchors;
  if(!v)return 0;
  snprintf(receipt,cap,"field=%s value=%.8g",name,*v);return 1;
 }
 if(!strcmp(op,"STUDIO_STATUS")){
  if(!studio_agent_eof(f))return 0;
  snprintf(receipt,cap,"frame=%d playing=%d expression=%d tab=%d xpbd=%d params=%d parts=%d drawables=%d",
    s.frame,s.settings.playing,s.selected,s.tab,s.xpbd!=NULL,s.count,s.parts,
    csmGetDrawableCount(s.core.model));return 1;
 }
 if(!strcmp(op,"STUDIO_METRICS")){
  if(!studio_agent_eof(f))return 0;
  StudioXpbdMetrics m={0};
  if(!studio_xpbd_metrics(s.xpbd,&m))return 0;
  snprintf(receipt,cap,"frames=%d step=%.7g accel=%.7g pin=%.7g",
    m.accepted_frames,m.current_step,m.max_acceleration,m.pin_drift);
  return 1;
 }
 if(!strcmp(op,"STUDIO_PLAY")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>1)return 0;
  s.settings.playing=a;snprintf(receipt,cap,"playing=%d",a);return 1;
 }
 if(!strcmp(op,"STUDIO_STEP")){
  if(!studio_agent_eof(f))return 0;
  s.settings.playing=0;s.step_requested=1;
  snprintf(receipt,cap,"step=queued");return 1;
 }
 if(!strcmp(op,"STUDIO_EXPRESSION")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>=9)return 0;
  s.selected=a;s.settings.cycle=0;snprintf(receipt,cap,"expression=%d",a);return 1;
 }
 if(!strcmp(op,"STUDIO_TOGGLE")){
  if(fscanf(f,"%47s %d",name,&a)!=2||!studio_agent_eof(f)||a<0||a>1)return 0;
  int *slot=NULL;
  if(!strcmp(name,"physics"))slot=&s.settings.physics;
  if(!strcmp(name,"blink"))slot=&s.settings.blink;
  if(!strcmp(name,"breath"))slot=&s.settings.breath;
  if(!strcmp(name,"gaze"))slot=&s.settings.gaze;
  if(!strcmp(name,"speech"))slot=&s.settings.speech;
  if(!strcmp(name,"cycle"))slot=&s.settings.cycle;
  if(!strcmp(name,"grid"))slot=&s.settings.grid;
  if(!strcmp(name,"particles"))slot=&s.settings.particles;
  if(!slot)return 0;
  *slot=a;snprintf(receipt,cap,"setting=%s enabled=%d",name,a);return 1;
 }
 if(!strcmp(op,"STUDIO_SETTING")){
  if(fscanf(f,"%47s %f",name,&x)!=2||!studio_agent_eof(f)||!isfinite(x))return 0;
  float *slot=NULL,lo=0,hi=1;
  if(!strcmp(name,"motion_speed")){slot=&s.settings.speed;lo=.1f;hi=2.5f;}
  if(!strcmp(name,"motion_amount")){slot=&s.settings.strength;hi=1.5f;}
  if(!strcmp(name,"wind_drive"))slot=&s.settings.wind;
  if(!strcmp(name,"blink_interval")){slot=&s.settings.blink_interval;lo=1;hi=8;}
  if(!strcmp(name,"cycle_interval")){slot=&s.settings.cycle_interval;lo=3;hi=20;}
  if(!slot||x<lo||x>hi)return 0;
  *slot=x;snprintf(receipt,cap,"setting=%s value=%.7g",name,x);return 1;
 }
 if(!strcmp(op,"STUDIO_CAMERA")){
  if(fscanf(f,"%47s",name)!=1||!studio_agent_eof(f))return 0;
  if(!strcmp(name,"fit")){FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;s.settings.zoom=1;}
  else if(!strcmp(name,"face")){s.settings.zoom=1.8f;s.view.panx=0;s.view.pany=s.view.sh*.28f;}
  else return 0;
  snprintf(receipt,cap,"camera=%s",name);return 1;
 }
 if(!strcmp(op,"STUDIO_ZOOM")){
  if(fscanf(f,"%f",&x)!=1||!studio_agent_eof(f)||!isfinite(x)||x<.3f||x>3)return 0;
  s.settings.zoom=x;snprintf(receipt,cap,"zoom=%.7g",x);return 1;
 }
 if(!strcmp(op,"STUDIO_PAN")){
  if(fscanf(f,"%f %f",&x,&y)!=2||!studio_agent_eof(f)||
     !isfinite(x)||!isfinite(y)||fabsf(x)>10000||fabsf(y)>10000)return 0;
  s.view.panx=x;s.view.pany=y;snprintf(receipt,cap,"pan=updated");return 1;
 }
 if(!strcmp(op,"STUDIO_RESET")){
  if(!studio_agent_eof(f))return 0;
  defaults();reset_manual();FitView(&s.view,s.core.model);s.base_zoom=s.view.zoom;
  snprintf(receipt,cap,"reset=completed");return 1;
 }
 if(!strcmp(op,"STUDIO_PRESET")){
  if(fscanf(f,"%47s",name)!=1||!studio_agent_eof(f))return 0;
  if(!strcmp(name,"save")){if(!save_preset())return 0;}
  else if(!strcmp(name,"load")){if(!load_preset())return 0;}
  else return 0;
  /* Success is acknowledged only after local save or validated load. */
  snprintf(receipt,cap,"preset=%s success=1",name);
  return 1;
 }
 if(!strcmp(op,"STUDIO_CAPTURE")){
  if(fscanf(f,"%47s",name)!=1||!studio_agent_eof(f)||
     strcmp(name,"viewport")||!s.target.id)return 0;
  char path[1200];
  snprintf(path,sizeof path,"%s/agent-viewport.png",s.folder);
  Image img=LoadImageFromTexture(s.target.texture);
  ImageFlipVertical(&img);
  int saved=ExportImage(img,path);
  UnloadImage(img);
  if(!saved)return 0;
  snprintf(receipt,cap,"local_viewport=stored");return 1;
 }
 if(!strcmp(op,"PARAM_SET")){
  if(fscanf(f,"%d %f",&a,&x)!=2||!studio_agent_eof(f)||
     a<0||a>=s.count||!isfinite(x))return 0;
  const float *lo=csmGetParameterMinimumValues(s.core.model);
  const float *hi=csmGetParameterMaximumValues(s.core.model);
  if(!lo||!hi||x<lo[a]||x>hi[a])return 0;
  s.manual[a]=x;s.pins[a]=1;
  snprintf(receipt,cap,"param=%d pinned=1",a);return 1;
 }
 if(!strcmp(op,"PARAM_CLEAR")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||a<0||a>=s.count)return 0;
  s.pins[a]=0;snprintf(receipt,cap,"param=%d pinned=0",a);return 1;
 }
 if(!strcmp(op,"PARAM_RESET")){
  if(!studio_agent_eof(f))return 0;
  reset_manual();snprintf(receipt,cap,"parameters=reset");return 1;
 }
 if(!strcmp(op,"PART_OPACITY")){
  if(fscanf(f,"%d %f",&a,&x)!=2||!studio_agent_eof(f)||
     a<0||a>=s.parts||!isfinite(x)||x<0||x>1)return 0;
  s.part_values[a]=x;
  snprintf(receipt,cap,"part=%d opacity=%.7g",a,x);return 1;
 }
 if(!strcmp(op,"RIG_VERTEX")){
  if(fscanf(f,"%d",&a)!=1||!studio_agent_eof(f)||
     a<0||a>=s.rig.vertex_count)return 0;
  s.rig_selected=a;s.tab=3;snprintf(receipt,cap,"vertex=%d",a);return 1;
 }
 if(!strcmp(op,"RIG_STORE")){
  if(fscanf(f,"%47s",name)!=1||!studio_agent_eof(f)||
     s.rig.vertex_count<=0)return 0;
  char path[1200];
  snprintf(path,sizeof path,"%s/rig-layer-%d.txt",s.folder,s.rig.drawable);
  int done=0;
  if(!strcmp(name,"save"))done=studio_rig_save(&s.rig,path);
  if(!strcmp(name,"load"))done=studio_rig_load(&s.rig,path,
           signature(),s.rig.vertex_count);
  if(!done)return 0;
  snprintf(receipt,cap,"rig=%s success=1",name);return 1;
 }
 if(!strcmp(op,"XPBD_SET")){
  if(fscanf(f,"%47s %f",name,&x)!=2||!studio_agent_eof(f)||!isfinite(x))return 0;
  float *slot=NULL,lo=0,hi=1;
  int integral=0;
  if(!strcmp(name,"gravity")){slot=&s.xpbd_gravity;lo=-80;hi=80;}
  if(!strcmp(name,"wind")){slot=&s.xpbd_wind;lo=-60;hi=60;}
  if(!strcmp(name,"damping"))slot=&s.xpbd_damping;
  if(!strcmp(name,"edge_compliance")){slot=&s.xpbd_compliance;hi=.0002f;}
  if(!strcmp(name,"collision")){slot=&s.xpbd_collision;hi=.15f;}
  if(!strcmp(name,"mass")){slot=&s.xpbd_mass;lo=.1f;hi=10;}
  if(!strcmp(name,"contact_compliance")){slot=&s.xpbd_contact_compliance;hi=.001f;}
  if(!strcmp(name,"iterations")){slot=&s.xpbd_iters;lo=1;hi=24;integral=1;}
  if(!strcmp(name,"step")){slot=&s.xpbd_step;lo=1.f/240;hi=1.f/60;}
  if(!strcmp(name,"pin")){slot=&s.xpbd_pin;hi=100;integral=1;}
  if(!strcmp(name,"substeps")){slot=&s.xpbd_substeps;lo=1;hi=8;integral=1;}
  if(!strcmp(name,"pose")){slot=&s.xpbd_pose_compliance;hi=.02f;}
  if(!strcmp(name,"transfer"))slot=&s.xpbd_source_transfer;
  if(!strcmp(name,"bending")){slot=&s.xpbd_bending;hi=.002f;}
  if(!strcmp(name,"surface")){slot=&s.xpbd_surface;hi=10;}
  if(!strcmp(name,"anchors")){slot=&s.xpbd_anchors;lo=1;hi=8;integral=1;}
  if(!slot||x<lo-1e-8f||x>hi+1e-8f||(integral&&x!=floorf(x)))return 0;
  *slot=x;snprintf(receipt,cap,"xpbd_setting=%s pending_apply=1",name);
  return 1;
 }
 if(!strcmp(op,"XPBD_SELECT")){
  if(fscanf(f,"%d %d %d",&a,&b,&c)!=3||!studio_agent_eof(f)||
     a<0||a>=csmGetDrawableCount(s.core.model)||b< -1||
     b>=csmGetDrawableCount(s.core.model)||b==a||
     c<0||c>1||(c&&b<0))return 0;
  s.xpbd_first=a;s.xpbd_second=b;s.xpbd_joint=c;
  snprintf(receipt,cap,"mesh_a=%d mesh_b=%d joint=%d pending_apply=1",a,b,c);
  return 1;
 }
 if(!strcmp(op,"XPBD_APPLY")){
  if(!studio_agent_eof(f))return 0;
  StudioXpbdOptions o={s.xpbd_first,s.xpbd_second,s.xpbd_joint,
    s.xpbd_gravity,s.xpbd_wind,s.xpbd_compliance,s.xpbd_collision,
    s.xpbd_mass,s.xpbd_damping,s.xpbd_contact_compliance,
    s.xpbd_step,(int)s.xpbd_iters,(int)s.xpbd_pin,(int)s.xpbd_substeps,
    s.xpbd_pose_compliance,s.xpbd_source_transfer,s.xpbd_bending,
    s.xpbd_surface,(int)s.xpbd_anchors};
  void *next=studio_xpbd_bind(s.core.model,o);
  if(!next)return 0;
  studio_xpbd_disable(s.xpbd);s.xpbd=next;
  snprintf(receipt,cap,"xpbd=applied");return 1;
 }
 if(!strcmp(op,"XPBD_DISABLE")){
  if(!studio_agent_eof(f))return 0;
  studio_xpbd_disable(s.xpbd);s.xpbd=NULL;
  snprintf(receipt,cap,"xpbd=off");return 1;
 }
 if(!strcmp(op,"XPBD_INSPECT")){
  if(!studio_agent_eof(f))return 0;
  snprintf(receipt,cap,"active=%d a=%d b=%d joint=%d gravity=%.5g wind=%.5g damping=%.5g",
    s.xpbd!=NULL,s.xpbd_first,s.xpbd_second,s.xpbd_joint,
    s.xpbd_gravity,s.xpbd_wind,s.xpbd_damping);
  return 1;
 }
 return -1;
}
