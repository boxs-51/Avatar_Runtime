#include "studio_v1_recovery.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
int studio_v1_recovery_valid_token(const char *token){
 if(!token||strlen(token)!=24)return 0;
 for(unsigned i=0;i<24;i++){
  char c=token[i];
  if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;
 }
 return 1;
}
int studio_v1_recovery_lookup(const StudioV1Recovery *state,const char *token){
 if(!state||!studio_v1_recovery_valid_token(token))return -1;
 for(unsigned i=0;i<STUDIO_V1_RECOVERY_CAP;i++){
  const StudioV1Receipt *e=&state->entries[i];
  if(e->outcome&&strcmp(e->token,token)==0)
   return e->outcome==1?1:0;
 }
 return -1; /* unknown is NOT safe to replay */
}
int studio_v1_recovery_store(StudioV1Recovery *state,const char *token,int ok){
 if(!state||!studio_v1_recovery_valid_token(token)||(ok!=0&&ok!=1))
  return 0;
 if(studio_v1_recovery_lookup(state,token)>=0)return 0;
 StudioV1Receipt *e=&state->entries[state->next%STUDIO_V1_RECOVERY_CAP];
 memcpy(e->token,token,25);
 e->outcome=(unsigned char)(ok?1:2);
 state->next=(state->next+1)%STUDIO_V1_RECOVERY_CAP;
 return 1;
}
static int path(char out[1200],const char *folder,const char *name){
 int n=snprintf(out,1200,"%s/api/recovery/%s",folder,name);
 return n>0&&n<1200;
}
void studio_v1_recovery_poll(const StudioV1Recovery *state,const char *folder,
                              StudioRecoveryPublish publish){
 if(!state||!folder||!publish)return;
 char request[1200],response[1200],temp[1200];
 if(!path(request,folder,"request.txt")||
    !path(response,folder,"response.txt")||
    !path(temp,folder,"response.tmp"))return;
 struct _stat info;
 if(_stat(response,&info)==0)return; /* previous ACK belongs to prior caller */
 if(_stat(temp,&info)==0){(void)publish(temp,response);return;}
 FILE *in=fopen(request,"rb");
 if(!in)return;
 char frame[90]={0};
 size_t n=fread(frame,1,sizeof frame,in);
 fclose(in);remove(request);
 if(n==0||n>=sizeof frame)return;
 frame[n]=0;
 char token[25]={0},original[25]={0},trailing=0;
 int matched=sscanf(frame,"%24s RECOVER %24s %c",token,original,&trailing);
 if(matched!=2||!studio_v1_recovery_valid_token(token)||
    !studio_v1_recovery_valid_token(original))return;
 int result=studio_v1_recovery_lookup(state,original);
 FILE *out=fopen(temp,"wb");
 if(!out)return;
 int bytes= result<0?fprintf(out,"%s UNKNOWN\n",token):
    fprintf(out,"%s FOUND %s\n",token,result?"OK":"ERR");
 int flush=fflush(out),close=fclose(out);
 if(bytes<1||flush||close){remove(temp);return;}
 (void)publish(temp,response);
}
