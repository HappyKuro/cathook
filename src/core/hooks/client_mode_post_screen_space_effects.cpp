#include "core/types.hpp"
#include "features/visuals/entity_visuals_effects.hpp"

bool (*client_mode_post_screen_space_effects_original)(void*, const view_setup*);

bool client_mode_post_screen_space_effects_hook(void* me, const view_setup* setup)
{
  CATHOOK_HOOK_GUARD();
  const bool result = client_mode_post_screen_space_effects_original(me, setup);
  if (result) entity_visuals::on_post_screen_space_effects();
  return result;
}
