/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/games/tf2/sdk/entities/team_objective_resource.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef TEAM_OBJECTIVE_RESOURCE_HPP
#define TEAM_OBJECTIVE_RESOURCE_HPP
#include "entity.hpp"
#include "core/print.hpp"
#define MAX_CONTROL_POINTS 8

class TeamObjectiveResource {
public:

  int get_mvm_wave_count(void) {
    static const int offset = tf2_netvars::find_offset(
      "DT_TeamObjectiveResource", {"m_nMannVsMachineWaveCount"});
    return offset > 0
      ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + offset)
      : 0;
  }

  int get_mvm_max_wave_count(void) {
    static const int offset = tf2_netvars::find_offset(
      "DT_TeamObjectiveResource", {"m_nMannVsMachineMaxWaveCount"});
    return offset > 0
      ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + offset)
      : 0;
  }

  bool is_mvm_between_waves(void) {
    static const int offset = tf2_netvars::find_offset(
      "DT_TeamObjectiveResource", {"m_bMannVsMachineBetweenWaves"});
    return offset > 0
      && *reinterpret_cast<bool*>(reinterpret_cast<uintptr_t>(this) + offset);
  }

  int get_num_control_points(void) {
    return *(int*)(this + 0x7B8);
  }

  bool is_playing_mini_rounds(void) {
    return *(bool*)(this + 0x7C0);
  }

  int get_owning_team(int index) {
    return (((int*)(this + 0x1AE4)))[index];
  }

  bool is_in_mini_round(int index) {
    return ((bool*)(this + 0x10B4))[index];
  }

  bool is_locked(int index) {
    return ((bool*)(this + 0x1914))[index];

  }

  bool can_team_capture(int index, enum tf_team team) {
    int array_index = index + ((int)(team) * MAX_CONTROL_POINTS);
    return ((bool*)(this + 0xF74))[array_index];
  }

  Vec3 get_origin(int index) {

    return ((Vec3*)(this + 0x7CC))[index];
  }
};
#endif
