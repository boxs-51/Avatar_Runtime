#include "../studio_v1_recovery.h"
#include <stdio.h>
#include <string.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"RECOVERY_GATE_FAILED line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 StudioV1Recovery state={0},fresh={0};
 const char *a="aaaaaaaaaaaaaaaaaaaaaaaa";
 const char *b="bbbbbbbbbbbbbbbbbbbbbbbb";
 const char *c="cccccccccccccccccccccccc";
 REQ(studio_v1_recovery_valid_token(a));
 REQ(!studio_v1_recovery_valid_token("not_a_valid_request"));
 REQ(studio_v1_recovery_lookup(&state,a)==-1);
 REQ(studio_v1_recovery_store(&state,a,1));
 REQ(studio_v1_recovery_store(&state,b,0));
 REQ(!studio_v1_recovery_store(&state,a,0)); /* old result not overwritten */
 REQ(studio_v1_recovery_lookup(&state,a)==1);
 REQ(studio_v1_recovery_lookup(&state,b)==0);
 REQ(studio_v1_recovery_lookup(&state,c)==-1);
 REQ(studio_v1_recovery_lookup(&fresh,a)==-1); /* volatile after restart */
 puts("STUDIO_V1_RECOVERY_PASS duplicate=blocked unknown=no_replay");
 return 0;
}
