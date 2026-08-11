#ifndef MELEE_AIM_HPP
#define MELEE_AIM_HPP
#include <cmath>
#include "aim_utils.hpp"
#include "resolver.hpp"

namespace melee_aim_detail {

inline bool is_knife(Weapon* weapon) {
  if (weapon == nullptr) {
    return false;
  }

  switch (weapon->get_def_id()) {
  case Spy_t_Knife:
  case Spy_t_KnifeR:
  case Spy_t_YourEternalReward:
  case Spy_t_ConniversKunai:
  case Spy_t_TheBigEarner:
  case Spy_t_TheWangaPrick:
  case Spy_t_TheSharpDresser:
  case Spy_t_TheSpycicle:
  case Spy_t_FestiveKnife:
  case Spy_t_TheBlackRose:
  case Spy_t_SilverBotkillerKnifeMkI:
  case Spy_t_GoldBotkillerKnifeMkI:
  case Spy_t_RustBotkillerKnifeMkI:
  case Spy_t_BloodBotkillerKnifeMkI:
  case Spy_t_CarbonadoBotkillerKnifeMkI:
  case Spy_t_DiamondBotkillerKnifeMkI:
  case Spy_t_SilverBotkillerKnifeMkII:
  case Spy_t_GoldBotkillerKnifeMkII:
    return true;
  default:
    return false;
  }
}

inline Vec3 forward_xy(const Vec3& angles) {
  Vec3 forward{};
  angle_vectors(angles, &forward, nullptr, nullptr);
  forward.z = 0.0f;
  const float length = std::sqrt((forward.x * forward.x) + (forward.y * forward.y));
  return length > 0.0001f ? forward * (1.0f / length) : Vec3{1.0f, 0.0f, 0.0f};
}

struct melee_movement_path {
  std::vector<Vec3> origins{};
  bool simulated = false;
};

inline float movement_interval() {
  if (global_vars != nullptr && std::isfinite(global_vars->interval_per_tick) &&
      global_vars->interval_per_tick > 0.0001f) {
    return global_vars->interval_per_tick;
  }
  return TICK_INTERVAL;
}

inline bool build_movement_path(Player* player, user_cmd* command, int ticks, bool local,
  melee_movement_path* out) {
  if (out == nullptr) {
    return false;
  }
  *out = {};
  if (player == nullptr || game_movement == nullptr || move_helper == nullptr ||
      prediction == nullptr || global_vars == nullptr || !player->is_alive()) {
    return false;
  }

  const int move_type = player->get_move_type();
  if (player->get_water_level() > 1 ||
      (move_type != MOVETYPE_WALK && move_type != MOVETYPE_NOCLIP)) {
    return false;
  }

  struct state_guard {
    Player* player;
    Vec3 origin;
    Vec3 abs_origin;
    Vec3 velocity;
    Vec3 base_velocity;
    Vec3 view_offset;
    int flags;
    int ground_entity;
    int buttons;
    int last_buttons;
    bool ducked;
    bool ducking;
    bool in_duck_jump;
    float duck_time;
    float duck_jump_time;
    float fall_velocity;
    int tickbase;
    user_cmd* current_command;
    float curtime;
    float frametime;
    int tickcount;
    bool in_prediction;
    bool first_time_predicted;

    explicit state_guard(Player* value)
      : player(value), origin(value->get_origin()), abs_origin(value->get_abs_origin()),
        velocity(value->get_velocity()), base_velocity(value->get_base_velocity()),
        view_offset(value->get_view_offset()), flags(value->get_flags()),
        ground_entity(value->get_ground_entity_handle()), buttons(value->get_buttons()),
        last_buttons(value->get_last_buttons()), ducked(value->get_ducked()),
        ducking(value->get_ducking_state()), in_duck_jump(value->get_in_duck_jump()),
        duck_time(value->get_duck_time()), duck_jump_time(value->get_duck_jump_time()),
        fall_velocity(value->get_fall_velocity()), tickbase(value->get_tickbase()),
        current_command(value->get_current_cmd()), curtime(global_vars->curtime),
        frametime(global_vars->frametime), tickcount(global_vars->tickcount),
        in_prediction(prediction->in_prediction),
        first_time_predicted(prediction->first_time_predicted) {}

    ~state_guard() {
      player->set_origin(origin);
      player->set_abs_origin(abs_origin);
      player->set_velocity(velocity);
      player->set_base_velocity(base_velocity);
      player->set_view_offset(view_offset);
      player->set_flags(flags);
      player->set_ground_entity_handle(ground_entity);
      player->set_buttons(buttons);
      player->set_last_buttons(last_buttons);
      player->set_ducked(ducked);
      player->set_ducking_state(ducking);
      player->set_in_duck_jump(in_duck_jump);
      player->set_duck_time(duck_time);
      player->set_duck_jump_time(duck_jump_time);
      player->set_fall_velocity(fall_velocity);
      player->set_tickbase(tickbase);
      player->set_current_cmd(current_command);
      move_helper->set_host(nullptr);
      global_vars->curtime = curtime;
      global_vars->frametime = frametime;
      global_vars->tickcount = tickcount;
      prediction->in_prediction = in_prediction;
      prediction->first_time_predicted = first_time_predicted;
    }
  } guard{player};

  const Vec3 observed_velocity = player->get_velocity();
  const bool grounded = (player->get_flags() & FL_ONGROUND) != 0;
  if (player->get_flags() & FL_DUCKING) {
    player->set_ducked(true);
    player->set_ducking_state(false);
    player->set_in_duck_jump(false);
    player->set_duck_time(0.0f);
    player->set_duck_jump_time(0.0f);
    player->set_flags(player->get_flags() & ~FL_DUCKING);
  }
  player->set_base_velocity({});
  if (grounded) {
    Vec3 velocity = player->get_velocity();
    velocity.z = std::min(velocity.z, 0.0f);
    player->set_velocity(velocity);
  } else {
    player->set_ground_entity_handle(0);
  }

  user_cmd simulated_command{};
  simulated_command.command_number = global_vars->tickcount;
  simulated_command.tick_count = global_vars->tickcount;
  simulated_command.buttons = local && command != nullptr ? command->buttons : player->get_buttons();
  simulated_command.view_angles = local && command != nullptr ? command->view_angles : player->get_eye_angles();
  player->set_current_cmd(&simulated_command);

  MoveData move{};
  move.m_bFirstRunOfFunctions = false;
  move.m_bGameCodeMovedPlayer = false;
  move.m_nPlayerHandle = player->get_ref_handle();
  move.m_vecVelocity = player->get_velocity();
  move.SetAbsOrigin(player->get_origin());
  move.m_flClientMaxSpeed = player->get_max_speed();
  move.m_flMaxSpeed = move.m_flClientMaxSpeed > 1.0f ? move.m_flClientMaxSpeed : 320.0f;
  move.m_nButtons = simulated_command.buttons;
  move.m_nOldButtons = player->get_last_buttons();
  move.m_vecViewAngles = simulated_command.view_angles;
  move.m_vecViewAngles.x = 0.0f;
  move.m_vecViewAngles.z = 0.0f;
  move.m_vecAbsViewAngles = move.m_vecViewAngles;
  move.m_vecAngles = move.m_vecViewAngles;
  move.m_vecOldAngles = move.m_vecViewAngles;

  const float speed = std::hypot(observed_velocity.x, observed_velocity.y);
  const float max_speed = move.m_flMaxSpeed > 1.0f ? move.m_flMaxSpeed : 320.0f;
  if (local && command != nullptr) {
    move.m_flForwardMove = command->forwardmove;
    move.m_flOldForwardMove = command->forwardmove;
    move.m_flSideMove = command->sidemove;
    move.m_flUpMove = command->upmove;
  } else {
    Vec3 direction = observed_velocity;
    direction.z = 0.0f;
    move.m_flForwardMove = grounded ? std::min(speed, max_speed) : 0.0f;
    move.m_flOldForwardMove = move.m_flForwardMove;
    move.m_flSideMove = 0.0f;
    move.m_flUpMove = 0.0f;
    if (speed > 1.0f) {
      move.m_vecViewAngles = aimbot_direction_to_angles(direction);
      move.m_vecViewAngles.x = 0.0f;
      move.m_vecViewAngles.z = 0.0f;
      move.m_vecAbsViewAngles = move.m_vecViewAngles;
      move.m_vecAngles = move.m_vecViewAngles;
      move.m_vecOldAngles = move.m_vecViewAngles;
    }
  }
  move.m_vecConstraintCenter = player->get_constraint_center();
  move.m_flConstraintRadius = player->get_constraint_radius();
  move.m_flConstraintWidth = player->get_constraint_width();
  move.m_flConstraintSpeedFactor = player->get_constraint_speed_factor();

  out->origins.reserve(static_cast<std::size_t>(std::max(ticks, 0)) + 1);
  out->origins.push_back(player->get_origin());
  prediction->in_prediction = true;
  prediction->first_time_predicted = false;
  move_helper->set_host(player);
  const float interval = movement_interval();
  const float start_time = global_vars->curtime;
  for (int tick = 0; tick < std::clamp(ticks, 0, 32); ++tick) {
    global_vars->curtime = start_time + static_cast<float>(tick + 1) * interval;
    global_vars->frametime = prediction->engine_paused ? 0.0f : interval;
    global_vars->tickcount += 1;
    simulated_command.command_number += 1;
    simulated_command.tick_count = global_vars->tickcount;
    simulated_command.forwardmove = move.m_flForwardMove;
    simulated_command.sidemove = move.m_flSideMove;
    simulated_command.upmove = move.m_flUpMove;
    simulated_command.view_angles = move.m_vecViewAngles;
    if (!game_movement->process_movement(player, &move)) {
      return false;
    }
    player->set_velocity(move.m_vecVelocity);
    player->set_origin(move.GetAbsOrigin());
    player->set_abs_origin(move.GetAbsOrigin());
    out->origins.push_back(move.GetAbsOrigin());
    move.m_nOldButtons = move.m_nButtons;
  }
  out->simulated = true;
  return true;
}

inline float backstab_target_yaw(Player* target) {
  if (target == nullptr) {
    return 0.0f;
  }

  const resolver::resolver_debug_info info = resolver::debug_for_player(target);
  return info.active && std::isfinite(info.yaw) ? info.yaw : target->get_eye_angles().y;
}

inline bool razorback_blocks_backstab(Player* target) {
  if (target == nullptr || !config.aimbot.melee_ignore_razorback || attribute_manager == nullptr) {
    return false;
  }
  if (attribute_manager->attrib_hook_value(0.0f, "set_blockbackstab_once", target->to_entity()) == 0.0f) {
    return false;
  }

  for (Entity* wearable : entity_cache_entities(class_id::WEARABLE_RAZORBACK)) {
    if (wearable != nullptr && wearable->get_owner_entity() == target && wearable->should_draw()) {
      return true;
    }
  }
  return false;
}

inline bool backstab_geometry_ok(Player* target,
  const Vec3& target_origin,
  const Vec3& swing_start,
  const Vec3& aim_angles) {
  if (target == nullptr) {
    return false;
  }

  Vec3 to_target = target_origin - swing_start;
  to_target.z = 0.0f;
  const float distance = std::sqrt((to_target.x * to_target.x) + (to_target.y * to_target.y));
  if (distance < 0.0884f) {
    return false;
  }
  to_target = to_target * (1.0f / distance);

  const Vec3 owner_forward = forward_xy(aim_angles);
  const Vec3 target_forward = forward_xy(Vec3{0.0f, backstab_target_yaw(target), 0.0f});
  const float compression = 0.125f;
  const float extra = compression / distance;
  const float position_vs_target = (to_target.x * target_forward.x) + (to_target.y * target_forward.y);
  const float position_vs_owner = (to_target.x * owner_forward.x) + (to_target.y * owner_forward.y);
  const float view_dot = (target_forward.x * owner_forward.x) + (target_forward.y * owner_forward.y);

  return position_vs_target > 0.0031f + extra &&
    position_vs_owner > 0.5f + extra &&
    view_dot > -0.2969f;
}

inline Vec3 backstab_approach_position(Player* localplayer, Player* target) {
  if (target == nullptr) {
    return {};
  }

  const Vec3 target_origin = target->get_origin();
  const Vec3 target_forward = forward_xy(Vec3{0.0f, backstab_target_yaw(target), 0.0f});
  const Vec3 target_side{-target_forward.y, target_forward.x, 0.0f};
  const float side_sign = localplayer != nullptr &&
      ((localplayer->get_index() + target->get_index()) & 1) != 0
    ? 1.0f
    : -1.0f;
  return target_origin - target_forward * 68.0f + target_side * (side_sign * 28.0f);
}

inline bool extrapolated_reach(Player* localplayer,
  Weapon* weapon,
  Player* target,
  const Vec3& target_origin,
  const Vec3& swing_start,
  const Vec3& aim_angles) {
  if (localplayer == nullptr || weapon == nullptr || target == nullptr || engine_trace == nullptr) {
    return false;
  }

  if (!aimbot_vec3_is_finite(target_origin) || !aimbot_vec3_is_finite(swing_start) ||
      !aimbot_vec3_is_finite(aim_angles)) {
    return false;
  }

  struct target_origin_guard {
    Player* target;
    Vec3 origin;
    Vec3 abs_origin;

    explicit target_origin_guard(Player* value, const Vec3& predicted_origin)
      : target(value), origin(value->get_origin()), abs_origin(value->get_abs_origin()) {
      target->set_origin(predicted_origin);
      target->set_abs_origin(predicted_origin);
    }

    ~target_origin_guard() {
      target->set_origin(origin);
      target->set_abs_origin(abs_origin);
    }
  } guard{target, target_origin};

  return aimbot_trace_melee_swing(localplayer, weapon, target, swing_start, aim_angles);
}

struct swing_solution {
  bool valid = false;
  Vec3 target_origin{};
  Vec3 swing_start{};
  Vec3 aim_position{};
  Vec3 aim_angles{};
  int tick = 0;
};

inline Vec3 clamp_point_to_player_bounds(Player* player, const Vec3& origin, Vec3 point) {
  if (player == nullptr) {
    return point;
  }
  const Vec3 mins = player->get_player_mins(player->is_ducking()) + origin;
  const Vec3 maxs = player->get_player_maxs(player->is_ducking()) + origin;
  point.x = std::clamp(point.x, mins.x, maxs.x);
  point.y = std::clamp(point.y, mins.y, maxs.y);
  point.z = std::clamp(point.z, mins.z, maxs.z);
  return point;
}

inline int swing_delay_ticks(Weapon* weapon) {
  if (weapon == nullptr || is_knife(weapon)) {
    return 0;
  }
  return std::clamp(static_cast<int>(std::ceil(weapon->get_smack_delay() / movement_interval())), 0, 32);
}

inline swing_solution find_predicted_swing(Player* localplayer, Weapon* weapon,
  Player* target, user_cmd* command, const aimbot_point& point) {
  swing_solution solution{};
  if (localplayer == nullptr || weapon == nullptr || target == nullptr || !point.valid) {
    return solution;
  }

  const int swing_ticks = swing_delay_ticks(weapon);
  const int simulated_ticks = config.aimbot.melee_swing_prediction
    ? std::clamp(config.aimbot.melee_swing_ticks, 0, 14)
    : 0;
  const int max_ticks = std::max(swing_ticks, simulated_ticks);
  melee_movement_path local_path{};
  melee_movement_path target_path{};
  const bool simulated = max_ticks > 0 &&
    build_movement_path(localplayer, command, max_ticks, true, &local_path) &&
    build_movement_path(target, nullptr, max_ticks, false, &target_path);

  std::vector<int> validation_ticks{};
  const auto add_tick = [&](int tick) {
    if (tick <= 0 || std::find(validation_ticks.begin(), validation_ticks.end(), tick) == validation_ticks.end()) {
      validation_ticks.push_back(tick);
    }
  };
  const int validation_mode = std::clamp(config.aimbot.melee_swing_validate_mode, 0, 2);
  if (!simulated || validation_mode == 0) {
    add_tick(0);
  }
  if (simulated) {
    switch (validation_mode) {
    case 1: add_tick(swing_ticks); break;
    case 2: add_tick(simulated_ticks); break;
    default:
      add_tick(simulated_ticks);
      add_tick(swing_ticks);
      break;
    }
  }
  if (validation_ticks.empty()) {
    validation_ticks.push_back(0);
  }

  for (const int tick : validation_ticks) {
    const float seconds = static_cast<float>(tick) * movement_interval();
    const bool have_local_origin = simulated && tick >= 0 &&
      static_cast<std::size_t>(tick) < local_path.origins.size();
    const bool have_target_origin = simulated && tick >= 0 &&
      static_cast<std::size_t>(tick) < target_path.origins.size();
    if (tick > 0 && (!have_local_origin || !have_target_origin)) {
      continue;
    }

    const Vec3 swing_start = tick > 0
      ? local_path.origins[static_cast<std::size_t>(tick)] + localplayer->get_view_offset()
      : localplayer->get_shoot_pos();
    const Vec3 target_origin = tick > 0
      ? (config.aimbot.melee_swing_predict_lag
        ? target_path.origins[static_cast<std::size_t>(tick)]
        : target->get_origin() + target->get_velocity() * seconds)
      : target->get_origin();
    Vec3 aim_position = clamp_point_to_player_bounds(target, target_origin, point.position);
    if (is_knife(weapon) && config.aimbot.melee_auto_backstab) {
      aim_position.x = target_origin.x;
      aim_position.y = target_origin.y;
    }
    const Vec3 aim_angles = aimbot_calculate_angles_to_position(swing_start, aim_position);
    if (!extrapolated_reach(localplayer, weapon, target, target_origin, swing_start, aim_angles) ||
        (is_knife(weapon) && config.aimbot.melee_auto_backstab &&
          (razorback_blocks_backstab(target) ||
            !backstab_geometry_ok(target, target_origin, swing_start, aim_angles)))) {
      continue;
    }

    solution.valid = true;
    solution.target_origin = target_origin;
    solution.swing_start = swing_start;
    solution.aim_position = aim_position;
    solution.aim_angles = aim_angles;
    solution.tick = tick;
    return solution;
  }
  return solution;
}

}

inline uint32_t melee_aim_configured_hitbox_mask() {
  const uint32_t configured_mask = config.aimbot.melee_hitboxes & aim_hitbox_mask_all;
  return configured_mask != aim_hitbox_mask_none
    ? configured_mask
    : aim_hitbox_mask_default_melee;
}

inline bool melee_aim_trace_candidate(Player* localplayer,
  Weapon* weapon,
  Player* target,
  const Vec3& target_origin,
  const Vec3& swing_start,
  const Vec3& aim_angles) {
  if (!melee_aim_detail::extrapolated_reach(
        localplayer,
        weapon,
        target,
        target_origin,
        swing_start,
        aim_angles)) {
    return false;
  }

  if (melee_aim_detail::is_knife(weapon) && config.aimbot.melee_auto_backstab) {
    if (melee_aim_detail::razorback_blocks_backstab(target) ||
        !melee_aim_detail::backstab_geometry_ok(target, target_origin, swing_start, aim_angles)) {
      return false;
    }
  }
  return true;
}

inline bool melee_aim_trace_candidate(Player* localplayer,
  Weapon* weapon,
  Player* target,
  const Vec3& target_origin,
  const Vec3& aim_angles) {
  return localplayer != nullptr && melee_aim_trace_candidate(
    localplayer,
    weapon,
    target,
    target_origin,
    localplayer->get_shoot_pos(),
    aim_angles);
}

inline aimbot_candidate melee_aim_find_candidate(Player* localplayer,
  Weapon* weapon,
  Player* player,
  user_cmd* command,
  const Vec3& original_view_angles) {
  aimbot_candidate candidate{};
  if (localplayer == nullptr || weapon == nullptr || player == nullptr) {
    return candidate;
  }

  const aimbot_point point = aimbot_find_best_point(
    localplayer,
    player,
    weapon,
    original_view_angles,
    melee_aim_configured_hitbox_mask(),
    false);
  if (!point.valid) {
    return candidate;
  }

  const melee_aim_detail::swing_solution solution = melee_aim_detail::find_predicted_swing(
    localplayer, weapon, player, command, point);
  if (!solution.valid) {
    return candidate;
  }

  candidate.entity = player;
  candidate.player = player;
  candidate.preferred = aimbot_player_is_preferred(player);
  candidate.bone = point.bone;
  candidate.hitbox = point.hitbox;
  candidate.studio_hitbox = point.studio_hitbox;
  candidate.aim_position = solution.aim_position;
  candidate.approach_position = melee_aim_detail::is_knife(weapon) && config.aimbot.melee_auto_backstab
    ? melee_aim_detail::backstab_approach_position(localplayer, player)
    : solution.target_origin;
  candidate.aim_angles = solution.aim_angles;
  candidate.command_angles = solution.aim_angles;
  candidate.fov = aimbot_calculate_fov(solution.aim_angles, original_view_angles);
  candidate.distance = std::sqrt(aimbot_distance_squared(solution.swing_start, solution.aim_position));
  candidate.health = player->get_health();
  candidate.visible = true;
  candidate.simulation_time = player->get_simulation_time();
  candidate.predicted_origin = solution.target_origin;
  candidate.predicted_origin_valid = solution.tick > 0;
  candidate.melee_swing_start = solution.swing_start;
  candidate.melee_swing_tick = solution.tick;
  return candidate;
}
#endif
