/* Private I0 MOC3 compatibility probe. Prints counts and hashes, never asset data. */
#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PurismCore.h"

enum { MAX_MOC_BYTES = 16 * 1024 * 1024, MAX_PARAMETERS = 4096,
       MAX_DRAWABLES = 100000, MAX_VERTICES = 2000000 };

static int fail(const char *reason) {
    fprintf(stderr, "I0_REJECT:%s\n", reason);
    return 2;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int pose_hash(csmModel *model, uint64_t *result) {
    const int count = csmGetDrawableCount(model);
    const int *vertices = csmGetDrawableVertexCounts(model);
    const csmVector2 **positions = csmGetDrawableVertexPositions(model);
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t total = 0;
    if (count <= 0 || count > MAX_DRAWABLES || !vertices || !positions) return 0;
    for (int i = 0; i < count; ++i) {
        if (vertices[i] < 0 || (size_t)vertices[i] > MAX_VERTICES - total ||
            (vertices[i] && !positions[i])) return 0;
        total += (size_t)vertices[i];
        hash = hash_bytes(hash, &vertices[i], sizeof vertices[i]);
        for (int j = 0; j < vertices[i]; ++j)
            hash = hash_bytes(hash, &positions[i][j], sizeof positions[i][j]);
    }
    *result = hash;
    return 1;
}

int main(int argc, char **argv) {
    FILE *file = NULL;
    void *moc_data = NULL, *model_data = NULL;
    csmMoc *moc = NULL;
    csmModel *model = NULL;
    int answer = 2;
    long length;
    unsigned int model_size;
    int params, parts, drawables, offscreens, masked = 0, blended = 0;
    int texture_max = -1, pose_indices[2] = {-1, -1};
    uint64_t baseline, pose_hashes[2] = {0, 0};
    const int *mask_counts, *texture_indices, *blend_modes;
    const float *mins, *maxs, *defaults;
    float *values;

    if (argc != 2) return fail("usage");
    file = fopen(argv[1], "rb");
    if (!file) return fail("open");
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        length < 4 || length > MAX_MOC_BYTES || fseek(file, 0, SEEK_SET)) {
        answer = fail("size"); goto done;
    }
    moc_data = _aligned_malloc((size_t)length, csmAlignofMoc);
    if (!moc_data || fread(moc_data, 1, (size_t)length, file) != (size_t)length) {
        answer = fail("read"); goto done;
    }
    fclose(file); file = NULL;
    if (memcmp(moc_data, "MOC3", 4) != 0 ||
        csmGetMocVersion(moc_data, (unsigned int)length) == csmMocVersion_Unknown ||
        !csmHasMocConsistency(moc_data, (unsigned int)length)) {
        answer = fail("consistency"); goto done;
    }
    moc = csmReviveMocInPlace(moc_data, (unsigned int)length);
    if (!moc || !(model_size = csmGetSizeofModel(moc)) || model_size > 64u * 1024u * 1024u) {
        answer = fail("revive_or_model_size"); goto done;
    }
    model_data = _aligned_malloc(model_size, csmAlignofModel);
    if (!model_data || !(model = csmInitializeModelInPlace(moc, model_data, model_size))) {
        answer = fail("initialize"); goto done;
    }
    params = csmGetParameterCount(model);
    parts = csmGetPartCount(model);
    drawables = csmGetDrawableCount(model);
    offscreens = csmGetOffscreenCount(model);
    if (params < 0 || params > MAX_PARAMETERS || parts < 0 ||
        drawables <= 0 || drawables > MAX_DRAWABLES || offscreens < 0 ||
        !pose_hash(model, &baseline)) {
        answer = fail("inventory_bounds"); goto done;
    }
    mask_counts = csmGetDrawableMaskCounts(model);
    texture_indices = csmGetDrawableTextureIndices(model);
    blend_modes = csmGetDrawableBlendModes(model);
    if (!mask_counts || !texture_indices || !blend_modes) {
        answer = fail("inventory_null"); goto done;
    }
    for (int i = 0; i < drawables; ++i) {
        if (mask_counts[i] > 0) ++masked;
        if (blend_modes[i] != csmColorBlendType_Normal) ++blended;
        if (texture_indices[i] > texture_max) texture_max = texture_indices[i];
    }
    mins = csmGetParameterMinimumValues(model);
    maxs = csmGetParameterMaximumValues(model);
    defaults = csmGetParameterDefaultValues(model);
    values = csmGetParameterValues(model);
    if (params && (!mins || !maxs || !defaults || !values)) {
        answer = fail("parameter_null"); goto done;
    }
    for (int i = 0; i < params && pose_indices[1] < 0; ++i) {
        uint64_t candidate_hash;
        if (mins[i] >= maxs[i]) continue;
        for (int j = 0; j < params; ++j) values[j] = defaults[j];
        values[i] = maxs[i] != defaults[i] ? maxs[i] : mins[i];
        csmUpdateModel(model);
        if (csmGetLastError(model) != csmError_NoError ||
            !pose_hash(model, &candidate_hash)) {
            answer = fail("pose_update"); goto done;
        }
        if (candidate_hash != baseline && candidate_hash != pose_hashes[0]) {
            const int slot = pose_indices[0] < 0 ? 0 : 1;
            pose_indices[slot] = i;
            pose_hashes[slot] = candidate_hash;
        }
    }
    printf("{\"moc_version\":%u,\"bytes\":%ld,\"parameters\":%d,"
           "\"parts\":%d,\"drawables\":%d,\"offscreen\":%d,"
           "\"masked_drawables\":%d,\"non_normal_blend_drawables\":%d,"
           "\"texture_index_max\":%d,\"baseline_geometry_hash\":\"%016llx\","
           "\"pose_1_index\":%d,\"pose_1_geometry_hash\":\"%016llx\","
           "\"pose_2_index\":%d,\"pose_2_geometry_hash\":\"%016llx\"}\n",
           (unsigned)csmGetMocVersion(moc_data, (unsigned int)length),
           length, params, parts, drawables, offscreens, masked, blended,
           texture_max, (unsigned long long)baseline, pose_indices[0],
           (unsigned long long)pose_hashes[0], pose_indices[1],
           (unsigned long long)pose_hashes[1]);
    answer = pose_indices[1] >= 0 ? 0 : 3;
done:
    if (file) fclose(file);
    _aligned_free(model_data);
    _aligned_free(moc_data);
    return answer;
}
