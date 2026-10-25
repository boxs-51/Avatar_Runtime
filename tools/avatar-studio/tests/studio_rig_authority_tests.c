#include "../studio_rig_authority.h"
#include <stdio.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"RIG_AUTH_RED %d\n",__LINE__);return 1;}}while(0)
static int anchor(StudioRig *r,void *a){return studio_rig_toggle_anchor(r,*(int*)a);}
static int role(StudioRig *r,void *a){return studio_rig_set_role(r,*(int*)a);}
static int noop(StudioRig*r,void*a){(void)r;(void)a;return 1;}
static int identity(StudioRig*r,void*a){(void)a;r->model_signature++;return 1;}
int main(void){
 StudioRig initial={0};StudioRigAuthority a={0};StudioRigAuthorityResult out={0};
 int v=2,roleval=3;
 REQ(studio_rig_init(&initial,0,42,12));
 REQ(studio_rig_authority_init(&a,&initial,101,55,7)==G5_OK);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_UI,101,55,7,501,anchor,&v,&out)==STUDIO_RIG_AUTH_OK);
 REQ(out.revision==8&&a.bus.txn.live.anchor_count==1);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_AGENT,101,55,8,502,role,&roleval,&out)==STUDIO_RIG_AUTH_OK);
 REQ(out.revision==9&&a.bus.txn.live.role==3);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_V1,101,55,9,503,anchor,&v,&out)==STUDIO_RIG_AUTH_OK);
 REQ(out.revision==10&&a.bus.txn.live.anchor_count==0);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_UI,101,55,9,504,role,&roleval,&out)==STUDIO_RIG_AUTH_STALE);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_AGENT,101,56,10,505,role,&roleval,&out)==STUDIO_RIG_AUTH_STALE);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_V1,101,55,10,502,anchor,&v,&out)==STUDIO_RIG_AUTH_REJECT);
 REQ(a.bus.txn.revision==10&&a.bus.txn.receipt_count==3);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_UI,101,55,10,506,identity,0,&out)==STUDIO_RIG_AUTH_REJECT);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_UI,101,55,10,507,noop,0,&out)==STUDIO_RIG_AUTH_UNCHANGED);
 REQ(a.bus.txn.revision==10&&a.bus.txn.receipt_count==3);
 REQ(a.bus.txn.applied_revision==7);
 G5TrustedActor ui={G5_UI,1};
 G5BusCommand cmd={0};G5BusResult bus_result={0};
 cmd.ingress=G5_UI;cmd.operation=G5_BEGIN;
 cmd.session=101;cmd.generation=55;cmd.expected_revision=10;
 REQ(g5_bus_submit_trusted(&a.bus,&ui,&cmd,&bus_result)==G5_OK);
 uint64_t lease_draft=bus_result.draft_id;
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_AGENT,101,55,10,508,
                                anchor,&v,&out)==STUDIO_RIG_AUTH_BUSY);
 REQ(a.bus.txn.revision==10);
 cmd.operation=G5_ABORT;cmd.draft_id=lease_draft;
 REQ(g5_bus_submit_trusted(&a.bus,&ui,&cmd,&bus_result)==G5_OK);
 REQ(studio_rig_authority_submit(&a,STUDIO_RIG_AGENT,101,55,10,508,
                                anchor,&v,&out)==STUDIO_RIG_AUTH_OK);
 REQ(out.revision==11&&a.bus.txn.live.anchor_count==1);
 G5Receipt receipt={0};
 G5TrustedActor agent={G5_AGENT,1};
 REQ(g5_bus_recover_trusted(&a.bus,&agent,101,508,&receipt)==G5_OK);
 REQ(receipt.operation==G5_COMMIT&&receipt.revision==11);
 REQ(a.bus.txn.applied_revision==7);

 puts("G5_SHARED_AUTHORITY_PASS");
 return 0;
}
