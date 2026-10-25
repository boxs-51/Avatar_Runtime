/* Synthetic C11 status oracle, no Cubism model and no GUI. */
#include "../studio_g5_live_status.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define REQ(e) do{if(!(e)){fprintf(stderr,"G5_READINESS_FAIL line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 char frame[96]={0};
 REQ(studio_g5_live_status_format(frame,sizeof frame,99,8));
 REQ(strcmp(frame,"writer=HOLD txn=OFF frame=UNVERIFIED gen=99 rigrev=8")==0);
 REQ(studio_g5_live_status_format(frame,sizeof frame,
             UINT64_C(9223372036854775807),UINT64_C(9223372036854775807)));
 REQ(strstr(frame,"writer=HOLD txn=OFF frame=UNVERIFIED")!=NULL);
 REQ(!studio_g5_live_status_format(frame,5,99,8)&&frame[0]==0);
 REQ(!studio_g5_live_status_format(frame,sizeof frame,
             UINT64_C(9223372036854775808),8)&&frame[0]==0);
 REQ(!studio_g5_live_status_format(NULL,0,99,8));
 puts("G5_LIVE_READINESS_PASS writer=HOLD txn=OFF");
 return 0;
}
