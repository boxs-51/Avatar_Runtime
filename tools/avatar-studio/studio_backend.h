#pragma once
#include "PurismCore.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct StudioInput {
    char moc[1024], directory[1024], physics[1024];
    char textures[64][512];
    int texture_count;
} StudioInput;
int studio_read_model(const char *path, StudioInput *out);
void *studio_physics_create(const char *path, csmModel *model);
int studio_physics_step(void *physics, float *values, unsigned advance_ns, int enabled, int reset);
int studio_physics_probe(const char *path, csmModel *model);
void studio_physics_free(void *physics);
int studio_pick_model(char *path, int capacity);
int studio_replace_file(const char *temporary, const char *destination);
#ifdef __cplusplus
}
#endif
