/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/games/tf2/sdk/interfaces/model_render.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef MODEL_RENDER_HPP
#define MODEL_RENDER_HPP
#include "games/tf2/sdk/materials/material.hpp"

struct DrawModelState {
  void* studio_hdr;
  void* studio_hw_data;
  void* renderable;
  const matrix_3x4* model_to_world;
  unsigned short decals;
  int draw_flags;
  int lod;
};

struct ModelRenderInfo {
  Vec3 origin;
  Vec3 angles;
  void* renderable;
  const model_t* model;
  const matrix_3x4* model_to_world;
  const matrix_3x4* lighting_offset;
  const Vec3* lighting_origin;
  int flags;
  int entity_index;
  int skin;
  int body;
  int hitboxset;
  unsigned short instance;
};

enum OverrideType {
  OVERRIDE_NORMAL = 0,
  OVERRIDE_BUILD_SHADOWS,
  OVERRIDE_DEPTH_WRITE,
  OVERRIDE_SSAO_DEPTH_WRITE,
};

class ModelRender {
public:
  void forced_material_override(Material* material, OverrideType override_type = OVERRIDE_NORMAL) {
    void** vtable = *(void***)this;
    auto fn = (void (*)(void*, Material*, OverrideType))vtable[1];
    fn(this, material, override_type);
  }

  void get_material_override(Material** material, OverrideType* override_type) {
    void** vtable = *(void***)this;
    auto fn = (void (*)(void*, Material**, OverrideType*))vtable[25];
    fn(this, material, override_type);
  }

  void draw_model_execute(const DrawModelState& state, const ModelRenderInfo& info, matrix_3x4* bones) {
    void** vtable = *(void***)this;
    auto fn = (void (*)(void*, const DrawModelState&, const ModelRenderInfo&, matrix_3x4*))vtable[19];
    fn(this, state, info, bones);
  }
};

inline static ModelRender* model_render;
#endif
