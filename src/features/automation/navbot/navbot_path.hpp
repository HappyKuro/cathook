/*
/^-----^\   data: 2026-04-05
V  o o  V  file: src/features/automation/navbot/navbot_path.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef NAVBOT_PATH_HPP
#define NAVBOT_PATH_HPP
#include <vector>
#include "features/automation/navbot/navbot_hazards.hpp"
#include "features/automation/navbot/navbot_mesh.hpp"
#include "features/automation/navbot/navbot_types.hpp"

namespace navbot
{

path_result solve_path_request(const navbot_mesh& mesh, const navbot_hazards& hazards, const path_request& request, const cancellation_token& token, float current_time);

}
#endif
