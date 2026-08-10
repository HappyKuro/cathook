/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/core/hooks/in_cond.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include "features/menu/config.hpp"
#include "core/detach.hpp"
#include "games/tf2/sdk/interfaces/entity_list.hpp"
#include "games/tf2/sdk/entities/player.hpp"

extern thread_local bool g_taunt_create_move_override;

bool in_cond_hook(void* me, int mask) {
  CATHOOK_HOOK_GUARD();
  if (cathook::core::is_detach_pending()) {
    return false;
  }

  if (mask == TF_COND_ZOOMED && config.visuals.removals.scope == true) {
    return false;
  }

  if (g_taunt_create_move_override && mask == TF_COND_TAUNTING &&
      config.misc.movement.taunt_slide && entity_list != nullptr) {
    Player* localplayer = entity_list->get_localplayer();
    if (localplayer != nullptr && me == localplayer->get_shared() &&
        localplayer->is_alive() && localplayer->allow_move_during_taunt()) {
      return false;
    }
  }

  return tf_player_shared_in_cond(me, mask);
}
