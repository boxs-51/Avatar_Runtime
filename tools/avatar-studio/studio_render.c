/* Preserve the Studio viewport across the sample renderer's mask prepasses.
 * No vendor source is modified. Purism Core sample renderer is MIT licensed. */
#include "viewer.h"
typedef struct {unsigned id;int w,h;} ParentTarget;
static ParentTarget targets[8];
static int target_depth;
static void nested_begin(RenderTexture2D target){
    if(target_depth>=8)abort();
    targets[target_depth++]=(ParentTarget){(unsigned)rlGetActiveFramebuffer(),rlGetFramebufferWidth(),rlGetFramebufferHeight()};
    BeginTextureMode(target);
}
static void nested_end(void){
    EndTextureMode();
    if(target_depth<=0)abort();
    ParentTarget p=targets[--target_depth];
    rlEnableFramebuffer(p.id);rlViewport(0,0,p.w,p.h);
    rlSetFramebufferWidth(p.w);rlSetFramebufferHeight(p.h);
    rlMatrixMode(RL_PROJECTION);rlLoadIdentity();rlOrtho(0,p.w,p.h,0,-1,1);
    rlMatrixMode(RL_MODELVIEW);rlLoadIdentity();
}
#define BeginTextureMode nested_begin
#define EndTextureMode nested_end
#include "render.c"
