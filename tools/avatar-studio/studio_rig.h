#pragma once
#include <stdint.h>
#define STUDIO_RIG_MAX_ANCHORS 64
#define STUDIO_RIG_MAX_BEAMS 128
typedef struct {int a,b;} StudioBeam;
typedef struct {
 int drawable,vertex_count,anchor_count,beam_count;
 uint64_t model_signature;
 int anchors[STUDIO_RIG_MAX_ANCHORS];
 StudioBeam beams[STUDIO_RIG_MAX_BEAMS];
} StudioRig;
int studio_rig_init(StudioRig*,int drawable,uint64_t signature,int vertex_count);
int studio_rig_toggle_anchor(StudioRig*,int vertex);
int studio_rig_toggle_beam(StudioRig*,int a,int b);
int studio_rig_save(const StudioRig*,const char*);
int studio_rig_load(StudioRig*,const char*,uint64_t expected_signature,int expected_vertices);

#define STUDIO_RIG_BANK_CAP 32
typedef struct {
 uint64_t signature;
 int count;
 StudioRig rigs[STUDIO_RIG_BANK_CAP];
} StudioRigBank;
int studio_rig_bank_select(StudioRigBank *bank,StudioRig *current,
                           int drawable,uint64_t signature,int vertices);
