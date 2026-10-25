#pragma once
#include "studio_geometry_document.h"
int studio_geometry_runtime_compile(const StudioGeometryDocument*,uint64_t,FILE*);
int studio_geometry_runtime_load(StudioGeometryDocument*,uint64_t,FILE*);
