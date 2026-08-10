/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/core/hooks/intro_menu_on_tick.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include <unistd.h>
#include "games/tf2/sdk/interfaces/global_vars.hpp"
#include "games/tf2/sdk/interfaces/entity_list.hpp"
#include "games/tf2/sdk/interfaces/engine.hpp"
#include "games/tf2/sdk/entities/player.hpp"
#include "core/print.hpp"
#include "features/automation/misc/misc.hpp"

void (*intro_menu_on_tick_original)(void*) = NULL;

static float last_time2 = 0.0;

void intro_menu_on_tick_hook(void* me) {
  CATHOOK_HOOK_GUARD();
  intro_menu_on_tick_original(me);
  automation::controller().on_menu_tick();
  return;

}
