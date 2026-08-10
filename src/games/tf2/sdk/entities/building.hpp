/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/games/tf2/sdk/entities/building.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef BUILDING_HPP
#define BUILDING_HPP
#include "entity.hpp"

class Building : public Entity {
public:
  int get_object_mode(void) {
    static const int object_mode_offset = tf2_netvars::find_offset("DT_BaseObject", {"m_iObjectMode"});
    return object_mode_offset > 0
      ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + object_mode_offset)
      : -1;
  }

  int get_health(void) {
    return *(int*)(this + 0x1388);
  }

  int get_max_health(void) {
    return *(int*)(this + 0x138C);
  }

  bool is_sapped(void) {
    return *(bool*)(this + 0x1380);
  }

  bool is_carried(void) {
    return *(bool*)(this + 0x1396) || this->is_carried_deploy();
  }

  bool is_carried_deploy(void) {
    return *(bool*)(this + 0x1397);
  }

  bool is_mini_sentry(void) {
    return *(bool*)(this + 0x1399);
  }

  int get_building_level(void) {
    return *(int*)(this + 0x1334);
  }

  int get_sentry_ammo_shells(void) {
    static const int offset = tf2_netvars::find_offset("DT_ObjectSentrygun", {"m_iAmmoShells"});
    return offset > 0 ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + offset) : 0;
  }

  int get_sentry_ammo_rockets(void) {
    static const int offset = tf2_netvars::find_offset("DT_ObjectSentrygun", {"m_iAmmoRockets"});
    return offset > 0 ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + offset) : 0;
  }

  int get_sentry_max_ammo_shells(void) {
    return is_mini_sentry() || get_building_level() == 1 ? 150 : 200;
  }

  int get_sentry_max_ammo_rockets(void) {
    return is_mini_sentry() || get_building_level() < 3 ? 0 : 20;
  }
};
#endif
