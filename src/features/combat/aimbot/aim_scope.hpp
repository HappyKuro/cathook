#ifndef AIM_SCOPE_HPP
#define AIM_SCOPE_HPP
#include "aim_utils.hpp"

namespace aim_scope {

enum class action {
  none,
  scope,
  unscope
};

struct decision {
  action requested = action::none;
  aimbot_debug_reason reason = aimbot_debug_reason::none;
};

inline int pending_scope_state = -1;
inline float pending_scope_request_time = -FLT_MAX;

inline bool is_sniper_rifle(Player* localplayer, Weapon* weapon) {
  return localplayer != nullptr &&
    weapon != nullptr &&
    localplayer->get_tf_class() == tf_class::SNIPER &&
    weapon->is_sniper_rifle();
}

inline bool can_toggle(Player* localplayer, Weapon* weapon) {
  return is_sniper_rifle(localplayer, weapon) && weapon->can_secondary_attack();
}

inline void reset_auto_scope() {
  pending_scope_state = -1;
  pending_scope_request_time = -FLT_MAX;
}

inline bool scoped_only(Player* localplayer, Weapon* weapon) {
  return is_sniper_rifle(localplayer, weapon) &&
    aimbot_modifier_enabled(Aim::hitscan_mod_scoped_only);
}

inline bool policy_requires_scope(Player* localplayer, Weapon* weapon) {
  if (!is_sniper_rifle(localplayer, weapon) ||
      weapon->get_weapon_id() == TF_WEAPON_SNIPERRIFLE_CLASSIC) {
    return false;
  }

  return scoped_only(localplayer, weapon) ||
    aimbot_weapon_requires_scope(weapon) ||
    aimbot_modifier_enabled(Aim::hitscan_mod_wait_for_headshot) ||
    aimbot_modifier_enabled(Aim::hitscan_mod_wait_for_charge);
}

inline bool target_within_auto_scope_range(Player* localplayer, Entity* entity) {
  if (localplayer == nullptr || entity == nullptr || entity->is_dormant()) {
    return false;
  }

  constexpr float auto_scope_range = 1850.0f;
  const Vec3 delta = entity->get_origin() - localplayer->get_origin();
  return (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z) <=
    auto_scope_range * auto_scope_range;
}

inline bool enemy_target_within_auto_scope_range(Player* localplayer) {
  if (localplayer == nullptr) {
    return false;
  }

  Weapon* weapon = localplayer->get_weapon();
  if (weapon == nullptr) {
    return false;
  }

  for (const entity_cache_player_entry& entry : entity_cache_players()) {
    if (entry.player != nullptr &&
        aimbot_player_skip_reason_for(localplayer, entry, weapon) == aimbot_player_skip_reason::none &&
        target_within_auto_scope_range(localplayer, entry.entity)) {
      return true;
    }
  }

  constexpr class_id building_ids[] = {
    class_id::SENTRY,
    class_id::DISPENSER,
    class_id::TELEPORTER
  };
  for (const class_id building_id : building_ids) {
    for (Entity* building : entity_cache_entities(building_id)) {
      if (building != nullptr &&
          !aimbot_should_skip_non_player_target(localplayer, building) &&
          target_within_auto_scope_range(localplayer, building)) {
        return true;
      }
    }
  }

  for (Entity* npc : entity_cache_npcs()) {
    if (npc != nullptr &&
        aimbot_entity_is_enemy_owned(localplayer, npc) &&
        target_within_auto_scope_range(localplayer, npc)) {
      return true;
    }
  }
  return false;
}

inline decision resolve(Player* localplayer, Weapon* weapon, const aimbot_candidate& candidate) {
  if (!can_toggle(localplayer, weapon)) {
    pending_scope_state = -1;
    return {};
  }

  const bool selected_target_needs_scope = candidate.entity != nullptr &&
    policy_requires_scope(localplayer, weapon);

  const bool headshot_wait_needs_scope = is_sniper_rifle(localplayer, weapon) &&
    weapon->get_weapon_id() != TF_WEAPON_SNIPERRIFLE_CLASSIC &&
    aimbot_modifier_enabled(Aim::hitscan_mod_wait_for_headshot);
  const bool should_scope = selected_target_needs_scope ||
    headshot_wait_needs_scope ||
    (config.aimbot.sniper_auto_scope &&
      enemy_target_within_auto_scope_range(localplayer));
  const bool scope_confirmed = aimbot_sniper_scope_confirmed(localplayer);
  if (should_scope == scope_confirmed) {
    reset_auto_scope();
    return {};
  }

  const int desired_state = should_scope ? 1 : 0;

  constexpr float scope_toggle_retry_seconds = 0.35f;
  const float now = global_vars != nullptr ? global_vars->curtime : 0.0f;
  if (pending_scope_state == desired_state &&
      now >= pending_scope_request_time &&
      now - pending_scope_request_time < scope_toggle_retry_seconds) {
    return {};
  }

  pending_scope_state = desired_state;
  pending_scope_request_time = now;
  return {
    .requested = should_scope ? action::scope : action::unscope,
    .reason = should_scope ? aimbot_debug_reason::auto_scope : aimbot_debug_reason::auto_unscope
  };
}

inline bool fire_ready(Player* localplayer, Weapon* weapon) {
  if (localplayer == nullptr || weapon == nullptr) {
    return false;
  }

  if (!policy_requires_scope(localplayer, weapon)) {
    return true;
  }
  if (!aimbot_sniper_scope_confirmed(localplayer)) {
    return false;
  }

  const bool waits_for_headshot =
    aimbot_modifier_enabled(Aim::hitscan_mod_wait_for_headshot) &&
    weapon->get_weapon_id() != TF_WEAPON_SNIPERRIFLE_CLASSIC;
  return !waits_for_headshot || aimbot_sniper_scope_time_ready(localplayer);
}

inline bool apply(user_cmd* cmd, const decision& value) {
  if (cmd == nullptr || value.requested == action::none) {
    return false;
  }

  cmd->buttons |= IN_ATTACK2;
  cmd->buttons &= ~IN_ATTACK;
  return true;
}

}
#endif
