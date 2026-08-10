void (*model_render_draw_model_execute_original)(void*, const DrawModelState&, const ModelRenderInfo&, matrix_3x4*) = nullptr;

void model_render_draw_model_execute_hook(void* me, const DrawModelState& state, const ModelRenderInfo& info, matrix_3x4* bones)
{
  CATHOOK_HOOK_GUARD();
  entity_visuals::on_draw_model_execute(me, state, info, bones);
}
