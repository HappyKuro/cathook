/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/core/hooks/text_window_show_panel.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include <unistd.h>
#include "features/menu/config.hpp"
#include "features/automation/misc/misc.hpp"

void (*text_window_show_panel_original)(void*, bool) = NULL;

void text_window_show_panel_hook(void* me, bool show) {
  CATHOOK_HOOK_GUARD();
  const bool dont_close_during_warmup =
      config.misc.automation.anti_motd_dont_close_during_warmup && automation::controller().is_setup_time();

  if (config.misc.automation.anti_motd == true && !dont_close_during_warmup) {
    text_window_show_panel_original(me, false);
  } else {
    text_window_show_panel_original(me, show);
  }
}
