#include "studio_g5_recovery_wire.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define G5_WIRE_LIMIT 40
#define G5_ID_MAX UINT64_C(0x7fffffffffffffff)
G5Code g5_recovery_handle(const G5Bus *bus,G5Ingress ingress,
                          uint64_t trusted_session,
                          const unsigned char *request,size_t length,
                          char *response,size_t cap){
 if(response&&cap)response[0]=0;
 if(!request||!response||cap==0||length<=7||length>G5_WIRE_LIMIT)
  return G5_INVALID_DRAFT;
 if(memcmp(request,"STATUS ",7)!=0)return G5_INVALID_DRAFT;
 if(request[7]<'1'||request[7]>'9')return G5_INVALID_DRAFT;
 uint64_t id=0;
 for(size_t i=7;i<length;i++){
  const unsigned char c=request[i];
  if(c<'0'||c>'9')return G5_INVALID_DRAFT;
  const uint64_t digit=(uint64_t)(c-'0');
  if(id>(G5_ID_MAX-digit)/10)return G5_INVALID_DRAFT;
  id=id*10+digit;
 }
 G5Receipt receipt={0};
 G5Code code=g5_bus_recover(bus,ingress,trusted_session,id,&receipt);
 if(code!=G5_OK&&code!=G5_UNKNOWN)return code;
 char local[100];
 int n=0;
 if(code==G5_UNKNOWN)
  n=snprintf(local,sizeof local,"outcome=UNKNOWN request=%llu revision=0",
             (unsigned long long)id);
 else{
  if(receipt.outcome!=G5_COMMITTED)return G5_UNKNOWN;
  n=snprintf(local,sizeof local,
             "outcome=COMMITTED request=%llu revision=%llu",
             (unsigned long long)id,(unsigned long long)receipt.revision);
 }
 if(n<=0||(size_t)n>=sizeof local||(size_t)n+1>cap)
  return G5_OUT_OF_BUDGET;
 memcpy(response,local,(size_t)n+1);
 return code;
}
