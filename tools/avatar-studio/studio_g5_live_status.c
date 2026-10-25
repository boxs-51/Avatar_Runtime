#include "studio_g5_live_status.h"
#include <inttypes.h>
#include <stdio.h>
int studio_g5_live_status_format(char *buffer,size_t capacity,
                                  uint64_t model_generation,
                                  uint64_t legacy_rig_revision){
 if(!buffer||!capacity)return 0;
 buffer[0]=0;
 if(model_generation>=UINT64_C(0x8000000000000000)||
    legacy_rig_revision>=UINT64_C(0x8000000000000000))
  return 0;
 int n=snprintf(buffer,capacity,
  "writer=HOLD txn=OFF frame=UNVERIFIED gen=%" PRIu64
  " rigrev=%" PRIu64,model_generation,legacy_rig_revision);
 if(n<0||(size_t)n>=capacity){buffer[0]=0;return 0;}
 return 1;
}
