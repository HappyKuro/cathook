/*
/^-----^\   data: 2026-04-05
V  o o  V  file: src/features/automation/navbot/navbot_debug.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef NAVBOT_DEBUG_HPP
#define NAVBOT_DEBUG_HPP
#include "core/types.hpp"
#include "features/automation/navbot/navbot_types.hpp"

struct ImDrawList;

namespace navbot
{

void draw_path_imgui(
  ImDrawList* draw_list,
  const path_result& path,
  bool draw_boxes,
  const RGBA_float& color);
void draw_debug_overlay_imgui(ImDrawList* draw_list, const navbot_debug_state& debug_state);

}
#endif
