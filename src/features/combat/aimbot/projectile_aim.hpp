#ifndef PROJECTILE_AIM_HPP
#define PROJECTILE_AIM_HPP
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <vector>
#include "core/types.hpp"
#include "aimbot.hpp"
#include "aim_state.hpp"
#include "aim_utils.hpp"
#include "splashbot.hpp"
#include "core/entity_cache.hpp"
#include "games/tf2/sdk/interfaces/client_state.hpp"
#include "games/tf2/sdk/interfaces/engine_trace.hpp"
#include "games/tf2/sdk/interfaces/game_movement.hpp"
#include "games/tf2/sdk/interfaces/global_vars.hpp"
#include "games/tf2/sdk/interfaces/move_helper.hpp"
#include "games/tf2/sdk/interfaces/prediction.hpp"

namespace projectile_aim {
namespace detail {

static constexpr unsigned int projectile_collision_mask =
  MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX;

enum class launch_type {
  fire_setup,
  hand,
  bat,
  grenade
};

struct projectile_info {
  float speed = 0.0f;
  float gravity_mod = 0.0f;
  float drag_mod = 0.0f;
  float life_time = 0.0f;
  float splash_radius = 0.0f;
  float initial_up_velocity = 0.0f;
  float release_delay = 0.0f;
  Vec3 offset{};
  Vec3 hull{};
  unsigned int collision_mask = projectile_collision_mask;
  launch_type launch = launch_type::fire_setup;
  bool trace_launch = false;
  bool direct_hit = true;
  bool secondary_attack = false;
};

struct target_seed {
  Entity* entity = nullptr;
  Player* player = nullptr;
  Vec3 origin{};
  Vec3 aim_offset{};
  Vec3 velocity{};
  float current_fov = FLT_MAX;
  float distance = FLT_MAX;
  bool preferred = false;
  bool fixed_aim_position_valid = false;
  Vec3 fixed_aim_position{};
};

struct solution {
  bool valid = false;
  Vec3 angles{};
  Vec3 aim_position{};
  Vec3 predicted_origin{};
  bool predicted_origin_valid = false;
  float time = 0.0f;
  float predicted_fov = FLT_MAX;
};

struct movement_path {
  std::vector<Vec3> origins{};
  bool simulated = false;
  Vec3 terminal_velocity{};
};

inline float interval() {
  if (global_vars != nullptr && std::isfinite(global_vars->interval_per_tick) && global_vars->interval_per_tick > 0.0001f) {
    return global_vars->interval_per_tick;
  }
  return TICK_INTERVAL;
}

inline bool finite(const Vec3& value) {
  return aimbot_vec3_is_finite(value);
}

inline float attribute(float fallback, const char* name, Entity* entity) {
  return attribute_manager != nullptr ? attribute_manager->attrib_hook_value(fallback, name, entity) : fallback;
}

inline int weapon_id(Weapon* weapon);

struct projectile_random_stream {
  static constexpr int table_size = 32;
  static constexpr int ia = 16807;
  static constexpr int im = 2147483647;
  static constexpr int iq = 127773;
  static constexpr int ir = 2836;
  static constexpr int ndiv = 1 + ((im - 1) / table_size);
  static constexpr double am = 1.0 / static_cast<double>(im);
  static constexpr double rnmx = 1.0 - 1.2e-7;

  int seed_value = 0;
  int shuffle_value = 0;
  int table[table_size]{};

  void set_seed(int seed) {
    seed_value = seed < 0 ? seed : -seed;
    shuffle_value = 0;
  }

  int generate_random_number() {
    if (seed_value <= 0 || shuffle_value == 0) {
      seed_value = -seed_value < 1 ? 1 : -seed_value;
      for (int j = table_size + 7; j >= 0; --j) {
        const int k = seed_value / iq;
        seed_value = ia * (seed_value - (k * iq)) - (ir * k);
        if (seed_value < 0) {
          seed_value += im;
        }
        if (j < table_size) {
          table[j] = seed_value;
        }
      }
      shuffle_value = table[0];
    }

    const int k = seed_value / iq;
    seed_value = ia * (seed_value - (k * iq)) - (ir * k);
    if (seed_value < 0) {
      seed_value += im;
    }

    int j = shuffle_value / ndiv;
    if (j >= table_size || j < 0) {
      j &= table_size - 1;
    }
    shuffle_value = table[j];
    table[j] = seed_value;
    return shuffle_value;
  }

  float random_float(float lo, float hi) {
    double value = am * static_cast<double>(generate_random_number());
    if (value > rnmx) {
      value = rnmx;
    }
    return static_cast<float>((value * static_cast<double>(hi - lo)) + static_cast<double>(lo));
  }

  int random_int(int lo, int hi) {
    if (hi <= lo) {
      return lo;
    }
    const std::uint32_t range = static_cast<std::uint32_t>(hi - lo) + 1U;
    const std::uint32_t limit = 0x80000000U - (0x80000000U % range);
    std::uint32_t value = 0;
    do {
      value = static_cast<std::uint32_t>(generate_random_number());
    } while (value >= limit);
    return lo + static_cast<int>(value % range);
  }
};

inline std::uint32_t projectile_crc32_byte(std::uint32_t crc, std::uint8_t byte) {
  crc ^= byte;
  for (int bit = 0; bit < 8; ++bit) {
    const std::uint32_t mask = 0U - (crc & 1U);
    crc = (crc >> 1) ^ (0xEDB88320U & mask);
  }
  return crc;
}

inline std::uint32_t projectile_seed_file_line_hash(int seed, const char* name, int additional_seed) {
  std::uint32_t crc = 0xFFFFFFFFU;
  const auto process_int = [&crc](int value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    for (std::size_t index = 0; index < sizeof(value); ++index) {
      crc = projectile_crc32_byte(crc, bytes[index]);
    }
  };
  process_int(seed);
  process_int(additional_seed);
  if (name != nullptr) {
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
      crc = projectile_crc32_byte(crc, static_cast<std::uint8_t>(*cursor));
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

struct projectile_randomness {
  bool valid = false;
  float syringe_pitch = 0.0f;
  float syringe_yaw = 0.0f;
  float arrow_pitch = 0.0f;
  float arrow_yaw = 0.0f;
  float grenade_up = 0.0f;
  float grenade_right = 0.0f;
};

inline projectile_randomness projectile_randomness_for(Weapon* weapon, user_cmd* cmd) {
  projectile_randomness result{};
  if (weapon == nullptr || cmd == nullptr || cmd->command_number <= 0) {
    return result;
  }

  const int seed = static_cast<int>(MD5_PseudoRandom(static_cast<unsigned int>(cmd->command_number)) & INT_MAX);
  projectile_random_stream stream{};
  stream.set_seed(static_cast<int>(projectile_seed_file_line_hash(seed, "SelectWeightedSequence", 0)));
  for (int index = 0; index < 6; ++index) {
    (void)stream.random_float(0.0f, 1.0f);
  }

  const int id = weapon_id(weapon);
  if (id == TF_WEAPON_SYRINGEGUN_MEDIC) {
    result.syringe_pitch = stream.random_float(-1.5f, 1.5f);
    result.syringe_yaw = stream.random_float(-1.5f, 1.5f);
  }
  else if (id == TF_WEAPON_COMPOUND_BOW) {
    const float charge_begin = weapon->get_charge_begin_time();
    const float now = global_vars != nullptr ? global_vars->curtime : 0.0f;
    if (charge_begin > 0.0f && now - charge_begin >= 5.0f) {
      result.arrow_pitch = (static_cast<float>(stream.random_int(0, INT_MAX)) /
        static_cast<float>(INT_MAX)) * 12.0f - 6.0f;
      result.arrow_yaw = (static_cast<float>(stream.random_int(0, INT_MAX)) /
        static_cast<float>(INT_MAX)) * 12.0f - 6.0f;
    }
  }
  else if (id == TF_WEAPON_GRENADELAUNCHER || id == TF_WEAPON_PIPEBOMBLAUNCHER || id == TF_WEAPON_CANNON) {
    result.grenade_up = stream.random_float(-10.0f, 10.0f);
    result.grenade_right = stream.random_float(-10.0f, 10.0f);
  }
  result.valid = true;
  return result;
}

inline float projectile_speed(Weapon* weapon, float fallback) {
  if (weapon == nullptr) {
    return fallback;
  }

  const float data_speed = weapon->get_projectile_speed_from_data();
  const float base_speed = data_speed > 1.0f ? data_speed : fallback;
  const float modified_speed = attribute(base_speed, "mult_projectile_speed", weapon->to_entity());
  return std::isfinite(modified_speed) ? std::clamp(modified_speed, 1.0f, 5000.0f) : base_speed;
}

inline int weapon_id(Weapon* weapon) {
  if (weapon == nullptr) {
    return TF_WEAPON_NONE;
  }
  const int id = weapon->get_weapon_id();
  if (id == TF_WEAPON_GRENADE_THROWABLE) {
    return TF_WEAPON_THROWABLE;
  }
  if (id != TF_WEAPON_NONE) {
    return id;
  }

  switch (weapon->get_def_id()) {
  case Soldier_m_RocketLauncher:
  case Soldier_m_RocketLauncherR:
  case Soldier_m_TheBlackBox:
  case Soldier_m_RocketJumper:
  case Soldier_m_TheLibertyLauncher:
  case Soldier_m_TheCowMangler5000:
  case Soldier_m_TheOriginal:
  case Soldier_m_FestiveRocketLauncher:
  case Soldier_m_TheBeggarsBazooka:
  case Soldier_m_FestiveBlackBox:
  case Soldier_m_TheAirStrike:
    return TF_WEAPON_ROCKETLAUNCHER;
  case Soldier_m_TheDirectHit:
    return TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT;
  case Soldier_s_TheRighteousBison:
    return TF_WEAPON_RAYGUN;
  case Demoman_m_GrenadeLauncher:
  case Demoman_m_GrenadeLauncherR:
  case Demoman_m_TheLochnLoad:
  case Demoman_m_TheLooseCannon:
  case Demoman_m_FestiveGrenadeLauncher:
  case Demoman_m_TheIronBomber:
    return weapon->get_def_id() == Demoman_m_TheLooseCannon ? TF_WEAPON_CANNON : TF_WEAPON_GRENADELAUNCHER;
  case Demoman_s_StickybombLauncher:
  case Demoman_s_StickybombLauncherR:
  case Demoman_s_FestiveStickybombLauncher:
  case Demoman_s_TheScottishResistance:
  case Demoman_s_TheQuickiebombLauncher:
    return TF_WEAPON_PIPEBOMBLAUNCHER;
  case Medic_m_CrusadersCrossbow:
  case Medic_m_FestiveCrusadersCrossbow:
    return TF_WEAPON_CROSSBOW;
  case Medic_m_SyringeGun:
  case Medic_m_SyringeGunR:
  case Medic_m_TheBlutsauger:
  case Medic_m_TheOverdose:
    return TF_WEAPON_SYRINGEGUN_MEDIC;
  case Engi_m_TheRescueRanger:
    return TF_WEAPON_SHOTGUN_BUILDING_RESCUE;
  case Engi_m_ThePomson6000:
    return TF_WEAPON_DRG_POMSON;
  case Sniper_m_TheHuntsman:
  case Sniper_m_FestiveHuntsman:
  case Sniper_m_TheFortifiedCompound:
    return TF_WEAPON_COMPOUND_BOW;
  case Pyro_s_TheFlareGun:
  case Pyro_s_TheDetonator:
  case Pyro_s_TheManmelter:
  case Pyro_s_TheScorchShot:
  case Pyro_s_FestiveFlareGun:
    return TF_WEAPON_FLAREGUN;
  case Pyro_m_DragonsFury:
    return TF_WEAPON_FLAMETHROWER;
  case Scout_s_MadMilk:
  case Scout_s_MutatedMilk:
    return TF_WEAPON_JAR_MILK;
  case Scout_s_TheFlyingGuillotine:
  case Scout_s_TheFlyingGuillotineG:
    return TF_WEAPON_CLEAVER;
  case Sniper_s_Jarate:
  case Sniper_s_FestiveJarate:
  case Sniper_s_TheSelfAwareBeautyMark:
    return TF_WEAPON_JAR;
  case Pyro_s_GasPasser:
    return TF_WEAPON_GRENADE_GAS;
  default:
    return TF_WEAPON_NONE;
  }
}

inline bool is_grenade_launcher(int weapon_id) {
  return weapon_id == TF_WEAPON_GRENADELAUNCHER ||
    weapon_id == TF_WEAPON_PIPEBOMBLAUNCHER ||
    weapon_id == TF_WEAPON_CANNON ||
    weapon_id == TF_WEAPON_STICKY_BALL_LAUNCHER;
}

inline bool is_rocket_weapon(int weapon_id) {
  return weapon_id == TF_WEAPON_ROCKETLAUNCHER ||
    weapon_id == TF_WEAPON_PARTICLE_CANNON ||
    weapon_id == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT;
}

inline float game_convar_float(const char* name, float fallback) {
  if (name == nullptr || convar_system == nullptr) {
    return fallback;
  }
  Convar* var = convar_system->find_var(name);
  if (var == nullptr) {
    return fallback;
  }
  const float value = var->get_float();
  return std::isfinite(value) && value >= 0.0f ? value : fallback;
}

inline bool get_info(Player* local, Weapon* weapon, projectile_info& out) {
  out = {};
  if (local == nullptr || weapon == nullptr) {
    return false;
  }

  const int id = weapon_id(weapon);
  const bool ducking = local->is_ducking();
  const float weapon_z = ducking ? 8.0f : -3.0f;

  if (weapon->is_flamethrower()) {
    out.speed = 2000.0f;
    out.splash_radius = 0.18f;
    out.offset = {23.5f, 12.0f, weapon_z};
    return true;
  }

  switch (id) {
  case TF_WEAPON_ROCKETLAUNCHER:
  case TF_WEAPON_PARTICLE_CANNON:
  case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
    out.speed = projectile_speed(weapon, 1100.0f);
    out.splash_radius = id == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT ? 0.0f :
      (weapon->get_def_id() == Soldier_m_TheAirStrike ? 130.0f : 170.0f);
    out.offset = {23.5f, attribute(0.0f, "centerfire_projectile", weapon->to_entity()) != 0.0f ? 0.0f : 12.0f, weapon_z};
    out.trace_launch = true;
    return true;

  case TF_WEAPON_GRENADELAUNCHER: {
    const bool loch = weapon->get_def_id() == Demoman_m_TheLochnLoad;
    const bool iron_bomber = weapon->get_def_id() == Demoman_m_TheIronBomber;
    out.speed = std::min(projectile_speed(weapon, 1200.0f), 3500.0f);
    out.gravity_mod = 1.0f;
    out.drag_mod = loch ? 0.07f : 0.11f;
    out.life_time = iron_bomber ? 1.4f : game_convar_float("tf_grenadelauncher_livetime", 0.8f);
    out.splash_radius = 146.0f;
    out.offset = {16.0f, 8.0f, -6.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.initial_up_velocity = 200.0f;
    out.trace_launch = true;
    return true;
  }

  case TF_WEAPON_PIPEBOMBLAUNCHER:
  case TF_WEAPON_STICKY_BALL_LAUNCHER: {
    const float charge_rate = std::max(attribute(4.0f, "stickybomb_charge_rate", weapon->to_entity()), 0.1f);
    const float charge_begin = weapon->get_charge_begin_time();
    const float now = global_vars != nullptr ? global_vars->curtime : local->get_tickbase() * interval();
    const float charge = std::clamp(now - charge_begin, 0.0f, charge_rate);
    out.speed = std::min(std::lerp(900.0f, 2400.0f, charge / charge_rate), 3500.0f);
    out.gravity_mod = 1.0f;
    out.drag_mod = 0.16f;
    out.splash_radius = 146.0f;
    out.offset = {16.0f, 8.0f, -6.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.initial_up_velocity = 200.0f;
    out.trace_launch = true;
    return true;
  }

  case TF_WEAPON_CANNON:
    out.speed = 1100.0f;
    out.gravity_mod = 1.0f;
    out.drag_mod = 0.05f;
    out.life_time = 0.95f;
    out.splash_radius = 146.0f;
    out.offset = {16.0f, 8.0f, -6.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.initial_up_velocity = 200.0f;
    out.trace_launch = true;
    return true;

  case TF_WEAPON_FLAREGUN:
    out.speed = 2000.0f;
    out.gravity_mod = 0.3f;
    out.initial_up_velocity = weapon->get_def_id() == Pyro_s_TheScorchShot ? 150.0f : 0.0f;
    out.offset = {23.5f, 12.0f, weapon_z};
    return true;

  case TF_WEAPON_FLAREGUN_REVENGE:
    out.speed = 3000.0f;
    out.gravity_mod = 0.45f;
    out.offset = {23.5f, 12.0f, weapon_z};
    return true;

  case TF_WEAPON_RAYGUN:
  case TF_WEAPON_DRG_POMSON:
    out.speed = 1200.0f;
    out.hull = {1.0f, 1.0f, 1.0f};
    out.offset = {23.5f, 12.0f, weapon_z};
    out.trace_launch = true;
    return true;

  case TF_WEAPON_FLAMETHROWER:
    out.speed = 2000.0f;
    out.splash_radius = 0.18f;
    out.offset = {23.5f, 12.0f, weapon_z};
    return true;

  case TF_WEAPON_COMPOUND_BOW: {
    const float begin = weapon->get_charge_begin_time();
    const float now = global_vars != nullptr ? global_vars->curtime : local->get_tickbase() * interval();
    const float charge = begin > 0.0f ? std::clamp(now - begin, 0.0f, 1.0f) : 0.0f;

    out.speed = std::lerp(1800.0f, 2600.0f, charge);
    out.gravity_mod = std::lerp(0.5f, 0.1f, charge);
    out.life_time = 10.0f;
    out.offset = {23.5f, 8.0f, -3.0f};
    out.hull = {1.0f, 1.0f, 1.0f};
    return true;
  }

  case TF_WEAPON_CROSSBOW:
  case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
    out.speed = id == TF_WEAPON_CROSSBOW ? 2400.0f : 2400.0f;
    out.gravity_mod = 0.2f;
    out.offset = {23.5f, 8.0f, -3.0f};
    return true;

  case TF_WEAPON_SYRINGEGUN_MEDIC:
    out.speed = 1000.0f;
    out.gravity_mod = 0.3f;
    out.hull = {1.0f, 1.0f, 1.0f};
    out.offset = {16.0f, 6.0f, -8.0f};
    return true;

  case TF_WEAPON_JAR:
  case TF_WEAPON_JAR_MILK:
    out.speed = 1000.0f;
    out.gravity_mod = 1.0f;
    out.drag_mod = 1.0f;
    out.life_time = 2.2f;
    out.splash_radius = 200.0f;
    out.offset = {16.0f, 8.0f, -6.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.initial_up_velocity = 200.0f;
    out.release_delay = 0.1f;
    out.launch = launch_type::hand;
    out.trace_launch = true;
    return true;

  case TF_WEAPON_CLEAVER:
  case TF_WEAPON_GRENADE_CLEAVER:
    out.speed = 3000.0f * (10.0f / std::sqrt(101.0f));
    out.gravity_mod = 1.0f;
    out.drag_mod = 1.0f;
    out.life_time = 2.2f;
    out.offset = {16.0f, 8.0f, -6.0f};
    out.initial_up_velocity = 3000.0f / std::sqrt(101.0f);
    out.release_delay = 0.1f;
    out.hull = {1.0f, 1.0f, 10.0f};
    out.launch = launch_type::hand;
    out.trace_launch = true;
    return true;

  case TF_WEAPON_BAT_WOOD:
  case TF_WEAPON_BAT_GIFTWRAP:
    out.speed = 3000.0f * (10.0f / std::sqrt(101.0f));
    out.gravity_mod = 1.0f;
    out.drag_mod = 1.0f;
    out.splash_radius = id == TF_WEAPON_BAT_GIFTWRAP ? 50.0f : 0.0f;
    out.offset = {0.0f, 0.0f, 0.0f};
    out.hull = {3.0f, 3.0f, 3.0f};
    out.initial_up_velocity = 3000.0f / std::sqrt(101.0f);
    out.release_delay = 0.1f;
    out.launch = launch_type::bat;
    out.secondary_attack = true;
    return true;

  case TF_WEAPON_GRENADE_GAS:
    out.speed = 950.0f;
    out.gravity_mod = 0.4f;
    out.drag_mod = 1.0f;
    out.life_time = 3.0f;
    out.splash_radius = 256.0f;
    out.offset = {3.0f, 7.0f, -9.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.trace_launch = true;
    out.launch = launch_type::grenade;
    out.direct_hit = false;
    return true;

  case TF_WEAPON_THROWABLE:
    out.speed = 1000.0f;
    out.gravity_mod = 1.0f;
    out.drag_mod = 1.0f;
    out.life_time = attribute(5.0f, "throwable_detonation_time", weapon->to_entity());
    out.splash_radius = 250.0f;
    out.offset = {3.0f, 7.0f, -9.0f};
    out.hull = {2.0f, 2.0f, 2.0f};
    out.initial_up_velocity = 200.0f;
    out.release_delay = 0.1f;
    out.launch = launch_type::hand;
    out.trace_launch = true;
    return true;

  default:
    break;
  }

  return false;
}

inline float latency_seconds() {
  if (client_state == nullptr || client_state->m_NetChannel == nullptr) {
    return 0.0f;
  }

  const float latency = client_state->m_NetChannel->get_latency(0);
  return std::isfinite(latency) ? std::clamp(latency, 0.0f, 0.25f) : 0.0f;
}

inline int target_position(Player* player, Weapon* weapon) {
  if (player == nullptr) {
    return 2;
  }

  const int id = weapon_id(weapon);
  if (id == TF_WEAPON_COMPOUND_BOW) {
    return 3;
  }
  if (is_rocket_weapon(id) || is_grenade_launcher(id)) {
    return player->is_on_ground() ? 1 : 2;
  }
  return 2;
}

inline Vec3 target_offset_for_position(Player* player, int position) {
  if (player == nullptr) {
    return {};
  }

  const Vec3 origin = player->get_origin();
  const Vec3 maxs = player->get_player_maxs();
  if (position == 1) {
    return {0.0f, 0.0f, maxs.z * 0.10f};
  }
  if (position == 3) {
    Vec3 head{};
    if (aimbot_get_hitbox_center(player, aim_hitbox_head, &head)) {
      return head - origin;
    }
    return {0.0f, 0.0f, maxs.z * 0.93f};
  }

  return {0.0f, 0.0f, maxs.z * 0.50f};
}

inline Vec3 target_offset(Player* player, Weapon* weapon) {
  if (player == nullptr) {
    return {};
  }
  const int position = config.aimbot.projectile_aim_pos == 0
    ? target_position(player, weapon)
    : config.aimbot.projectile_aim_pos;
  return target_offset_for_position(player, position);
}

inline Vec3 path_origin(const target_seed& seed, const movement_path& path, float seconds) {
  if (!path.origins.empty()) {
    const float tick = std::max(seconds, 0.0f) / interval();
    const float last_tick = static_cast<float>(path.origins.size() - 1);
    if (tick > last_tick && path.simulated && finite(path.terminal_velocity)) {
      return path.origins.back() + path.terminal_velocity * ((tick - last_tick) * interval());
    }
    const std::size_t low = std::min(static_cast<std::size_t>(tick), path.origins.size() - 1);
    const std::size_t high = std::min(low + 1, path.origins.size() - 1);
    const float fraction = std::clamp(tick - static_cast<float>(low), 0.0f, 1.0f);
    return path.origins[low] + (path.origins[high] - path.origins[low]) * fraction;
  }
  return seed.origin + seed.velocity * seconds;
}

inline Vec3 target_point(const target_seed& seed, const movement_path& path, float seconds) {
  if (seed.fixed_aim_position_valid) {
    return seed.fixed_aim_position;
  }
  return path_origin(seed, path, seconds) + seed.aim_offset;
}

inline bool build_move_path(const target_seed& seed, float max_time, movement_path& out) {
  out = {};
  if (seed.player == nullptr || game_movement == nullptr || move_helper == nullptr ||
      prediction == nullptr || global_vars == nullptr) {
    return false;
  }

  const int move_type = seed.player->get_move_type();
  if (!seed.player->is_alive() || seed.player->get_water_level() > 1 ||
      (move_type != MOVETYPE_WALK && move_type != MOVETYPE_NOCLIP)) {
    return false;
  }

  struct movement_state_guard {
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
    bool prediction_in_prediction;
    bool prediction_first_time_predicted;

    explicit movement_state_guard(Player* value)
      : player(value), origin(value->get_origin()), abs_origin(value->get_abs_origin()),
        velocity(value->get_velocity()),
        base_velocity(value->get_base_velocity()), view_offset(value->get_view_offset()),
        flags(value->get_flags()), ground_entity(value->get_ground_entity_handle()),
        buttons(value->get_buttons()), last_buttons(value->get_last_buttons()),
        ducked(value->get_ducked()), ducking(value->get_ducking_state()),
        in_duck_jump(value->get_in_duck_jump()), duck_time(value->get_duck_time()),
        duck_jump_time(value->get_duck_jump_time()), fall_velocity(value->get_fall_velocity()),
        tickbase(value->get_tickbase()), current_command(value->get_current_cmd()),
        curtime(global_vars->curtime), frametime(global_vars->frametime),
        tickcount(global_vars->tickcount), prediction_in_prediction(prediction->in_prediction),
        prediction_first_time_predicted(prediction->first_time_predicted) {}

    ~movement_state_guard() {
      if (player == nullptr) {
        return;
      }
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
      prediction->in_prediction = prediction_in_prediction;
      prediction->first_time_predicted = prediction_first_time_predicted;
    }
  } state_guard{seed.player};

  prediction->in_prediction = true;
  prediction->first_time_predicted = false;
  move_helper->set_host(seed.player);

  const bool was_grounded = seed.player->get_ground_entity() != nullptr;
  if (seed.player->get_flags() & FL_DUCKING) {
    seed.player->set_ducked(true);
    seed.player->set_ducking_state(false);
    seed.player->set_in_duck_jump(false);
    seed.player->set_duck_time(0.0f);
    seed.player->set_duck_jump_time(0.0f);
    seed.player->set_flags(seed.player->get_flags() & ~FL_DUCKING);
  }
  seed.player->set_base_velocity({});
  if (seed.player->get_flags() & FL_ONGROUND) {
    Vec3 velocity = seed.player->get_velocity();
    velocity.z = std::min(velocity.z, 0.0f);
    seed.player->set_velocity(velocity);
  } else {
    seed.player->set_ground_entity_handle(0);
  }

  user_cmd simulated_command{};
  simulated_command.view_angles = seed.player->get_eye_angles();
  simulated_command.command_number = global_vars->tickcount;
  simulated_command.tick_count = global_vars->tickcount;
  simulated_command.forwardmove = 0.0f;
  simulated_command.sidemove = 0.0f;
  simulated_command.upmove = 0.0f;
  simulated_command.buttons = seed.player->get_buttons();
  seed.player->set_current_cmd(&simulated_command);

  const float simulation_start = seed.entity != nullptr &&
      std::isfinite(seed.entity->get_simulation_time())
    ? seed.entity->get_simulation_time()
    : global_vars->curtime;

  MoveData move{};
  move.m_bFirstRunOfFunctions = false;
  move.m_bGameCodeMovedPlayer = false;
  move.m_nPlayerHandle = seed.player->get_ref_handle();
  move.m_vecVelocity = seed.player->get_velocity();
  move.SetAbsOrigin(seed.origin);
  move.m_flClientMaxSpeed = seed.player->get_max_speed();
  move.m_flMaxSpeed = move.m_flClientMaxSpeed > 1.0f ? move.m_flClientMaxSpeed : 320.0f;
  move.m_nButtons = seed.player->get_buttons();
  move.m_nOldButtons = seed.player->get_last_buttons();
  Vec3 horizontal_velocity = seed.velocity;
  horizontal_velocity.z = 0.0f;
  move.m_vecViewAngles = horizontal_velocity.x * horizontal_velocity.x + horizontal_velocity.y * horizontal_velocity.y > 1.0f
    ? aimbot_direction_to_angles(horizontal_velocity)
    : seed.player->get_eye_angles();
  if (!finite(move.m_vecViewAngles)) {
    move.m_vecViewAngles = aimbot_direction_to_angles(horizontal_velocity);
  }
  move.m_vecViewAngles.x = 0.0f;
  move.m_vecViewAngles.z = 0.0f;
  move.m_vecAbsViewAngles = move.m_vecViewAngles;
  move.m_vecAngles = move.m_vecViewAngles;
  move.m_vecOldAngles = move.m_vecViewAngles;

  const float speed = std::hypot(seed.velocity.x, seed.velocity.y);
  const float max_speed = move.m_flMaxSpeed > 1.0f ? move.m_flMaxSpeed : 320.0f;

  move.m_flForwardMove = was_grounded ? std::min(speed, max_speed) : 0.0f;
  move.m_flOldForwardMove = move.m_flForwardMove;
  move.m_flSideMove = 0.0f;
  move.m_flUpMove = 0.0f;
  move.m_vecConstraintCenter = seed.player->get_constraint_center();
  move.m_flConstraintRadius = seed.player->get_constraint_radius();
  move.m_flConstraintWidth = seed.player->get_constraint_width();
  move.m_flConstraintSpeedFactor = seed.player->get_constraint_speed_factor();

  const int max_ticks = std::clamp(static_cast<int>(std::ceil(max_time / interval())), 1, 400);
  out.origins.reserve(static_cast<std::size_t>(max_ticks) + 1);
  out.origins.push_back(seed.origin);
  for (int tick = 0; tick < max_ticks; ++tick) {
    global_vars->curtime = simulation_start + static_cast<float>(tick) * interval();
    global_vars->frametime = prediction->engine_paused ? 0.0f : interval();
    global_vars->tickcount = static_cast<int>(global_vars->curtime / interval());
    simulated_command.command_number++;
    simulated_command.tick_count = global_vars->tickcount;
    simulated_command.forwardmove = move.m_flForwardMove;
    simulated_command.sidemove = move.m_flSideMove;
    simulated_command.upmove = move.m_flUpMove;
    simulated_command.view_angles = move.m_vecViewAngles;
    if (!game_movement->process_movement(seed.player, &move)) {
      out = {};
      return false;
    }

    seed.player->set_velocity(move.m_vecVelocity);
    seed.player->set_origin(move.GetAbsOrigin());
    seed.player->set_abs_origin(move.GetAbsOrigin());
    out.origins.push_back(move.GetAbsOrigin());
    out.terminal_velocity = move.m_vecVelocity;
    move.m_nOldButtons = move.m_nButtons;
  }
  out.simulated = true;
  return true;
}

inline bool launch_position(Player* local, const projectile_info& info, const Vec3& angles,
  bool ignore_friendly_players,
  Vec3& out, Vec3* launch_angles_out = nullptr) {
  if (local == nullptr || !finite(angles)) {
    return false;
  }

  Vec3 launch_angles = aimbot_clamp_angles(angles);
  Vec3 forward{};
  Vec3 right{};
  Vec3 up{};
  angle_vectors(launch_angles, &forward, &right, &up);

  const Vec3 eye = local->get_shoot_pos();

  if (info.launch == launch_type::fire_setup) {
    Vec3 fire_offset = info.offset;

    static Convar* cl_flipviewmodels = nullptr;
    if (cl_flipviewmodels == nullptr && convar_system != nullptr) {
      cl_flipviewmodels = convar_system->find_var("cl_flipviewmodels");
    }
    if (cl_flipviewmodels != nullptr && cl_flipviewmodels->get_int() != 0) {
      fire_offset.y *= -1.0f;
    }
    out = eye + (forward * fire_offset.x) + (right * fire_offset.y) + (up * fire_offset.z);

    Vec3 fire_end = eye + forward * 2000.0f;
    Vec3 effective_end = fire_end;
    if (engine_trace != nullptr) {
      Vec3 trace_start = eye;
      Vec3 trace_end = fire_end;
      ray_t ray = engine_trace->init_ray(&trace_start, &trace_end);
      trace_filter filter{};
      if (ignore_friendly_players) {
        engine_trace->init_projectile_trace_filter(&filter, local->to_entity());
      } else {
        engine_trace->init_trace_filter(&filter, local->to_entity());
      }
      trace_t trace{};
      engine_trace->trace_ray(&ray, projectile_collision_mask, &filter, &trace);
      if (trace.start_solid || trace.all_solid) {
        return false;
      }
      effective_end = trace.fraction > 0.1f ? trace.endpos : fire_end;
    }

    launch_angles = aimbot_calculate_angles_to_position(out, effective_end);
    if (!finite(out) || !finite(launch_angles)) {
      return false;
    }

    if (launch_angles_out != nullptr) {
      *launch_angles_out = launch_angles;
    }
    return true;
  }

  if (info.launch == launch_type::bat) {
    out = local->get_origin() + Vec3{0.0f, 0.0f, 50.0f} + (forward * 32.0f);
    if (launch_angles_out != nullptr) {
      *launch_angles_out = launch_angles;
    }
    return finite(out);
  }

  if (info.launch == launch_type::grenade) {
    out = eye + forward * 16.0f - right * 8.0f - up * 20.0f;
  }
  else {
    out = eye + (forward * info.offset.x) + (right * info.offset.y) + (up * info.offset.z);
  }

  if (info.trace_launch && engine_trace != nullptr) {
    Vec3 mins{-8.0f, -8.0f, -8.0f};
    Vec3 maxs{8.0f, 8.0f, 8.0f};
    Vec3 trace_start = eye;
    Vec3 trace_end = out;
    ray_t ray = engine_trace->init_ray(&trace_start, &trace_end, &mins, &maxs);
    trace_filter filter{};
    engine_trace->init_world_and_props_trace_filter(&filter);
    trace_t trace{};
    engine_trace->trace_ray(&ray, projectile_collision_mask, &filter, &trace);
    if (trace.start_solid || trace.all_solid || trace.fraction < 0.999f) {
      return false;
    }
    out = trace.endpos;
  }

  if (launch_angles_out != nullptr) {
    *launch_angles_out = launch_angles;
  }
  return finite(out);
}

inline Vec3 compensate_projectile_spread(Player* local, Weapon* weapon,
  user_cmd* cmd, const projectile_info& info, const Vec3& desired_angles) {
  if (local == nullptr || weapon == nullptr || cmd == nullptr || !finite(desired_angles)) {
    return desired_angles;
  }

  const projectile_randomness random = projectile_randomness_for(weapon, cmd);
  if (!random.valid) {
    return desired_angles;
  }

  Vec3 compensated = desired_angles;
  const int id = weapon_id(weapon);
  if (id == TF_WEAPON_SYRINGEGUN_MEDIC) {

    compensated.x -= random.syringe_pitch;
    compensated.y -= random.syringe_yaw;
  }
  else if (id == TF_WEAPON_COMPOUND_BOW) {
    compensated.x -= random.arrow_pitch;
    compensated.y -= random.arrow_yaw;
  }

  if (random.grenade_up == 0.0f && random.grenade_right == 0.0f) {
    return aimbot_clamp_angles(compensated);
  }

  const bool ignore_friendly_players = !is_rocket_weapon(id);
  for (int iteration = 0; iteration < 4; ++iteration) {
    Vec3 launch{};
    Vec3 launch_angles{};
    if (!launch_position(local, info, compensated, ignore_friendly_players, launch, &launch_angles)) {
      break;
    }

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    angle_vectors(launch_angles, &forward, &right, &up);
    const Vec3 base_velocity = forward * info.speed + up * info.initial_up_velocity;
    const Vec3 actual_velocity = base_velocity + up * random.grenade_up + right * random.grenade_right;
    const Vec3 actual_angles = aimbot_direction_to_angles(aimbot_normalize_vector(actual_velocity));
    const Vec3 error = aimbot_normalize_angle_delta(launch_angles, actual_angles);
    compensated = aimbot_clamp_angles(compensated + error);
    if (!finite(compensated)) {
      return desired_angles;
    }
  }

  return aimbot_clamp_angles(compensated);
}

inline bool solve_ballistic(const projectile_info& info, const Vec3& from, const Vec3& to,
  float drag_factor, Vec3& angles, float& time) {
  const Vec3 delta = to - from;
  const float horizontal = std::hypot(delta.x, delta.y);
  if (horizontal <= 0.001f || !std::isfinite(info.speed) || info.speed <= 0.0f) {
    return false;
  }

  float speed = info.speed;
  const float gravity = 800.0f * info.gravity_mod;
  for (int iteration = 0; iteration < 2; ++iteration) {
    if (gravity > 0.001f) {
      const float v0 = std::hypot(speed, info.initial_up_velocity);
      const float launch_pitch = std::atan2(info.initial_up_velocity, speed);
      const float root = v0 * v0 * v0 * v0 - gravity * (gravity * horizontal * horizontal +
        2.0f * delta.z * v0 * v0);
      if (root < 0.0f) {
        return false;
      }
      const float pitch = std::atan((v0 * v0 - std::sqrt(root)) / (gravity * horizontal));
      angles = {
        -((pitch - launch_pitch) * radpi),
        std::atan2(delta.y, delta.x) * radpi,
        0.0f
      };
      time = horizontal / (std::max(std::cos(pitch) * v0, 0.001f));
    } else {
      angles = aimbot_calculate_angles_to_position(from, to);
      time = distance_3d(from, to) / speed;
    }

    if (drag_factor <= 0.0f || !std::isfinite(time)) {
      break;
    }
    const float drag_loss = std::clamp(drag_factor * time * 0.5f, 0.0f, 0.75f);
    speed = info.speed * (1.0f - drag_loss);
  }

  angles = aimbot_clamp_angles(angles);
  return finite(angles) && std::isfinite(time) && time >= 0.0f &&
    (info.life_time <= 0.0f || time <= info.life_time);
}

inline float point_segment_distance_squared(const Vec3& point, const Vec3& start, const Vec3& end) {
  const Vec3 segment = end - start;
  const float length_squared = segment.x * segment.x + segment.y * segment.y + segment.z * segment.z;
  if (length_squared <= 0.0001f) {
    const Vec3 delta = point - start;
    return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
  }
  const Vec3 to_point = point - start;
  const float fraction = std::clamp((to_point.x * segment.x + to_point.y * segment.y + to_point.z * segment.z) / length_squared, 0.0f, 1.0f);
  const Vec3 closest = start + segment * fraction;
  const Vec3 delta = point - closest;
  return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

inline float target_radius(const target_seed& seed) {
  if (seed.player != nullptr) {
    const Vec3 mins = seed.player->get_player_mins();
    const Vec3 maxs = seed.player->get_player_maxs();
    return std::max({std::fabs(mins.x), std::fabs(mins.y), std::fabs(maxs.x), std::fabs(maxs.y), 8.0f});
  }
  if (seed.entity != nullptr) {
    const Vec3 mins = seed.entity->get_collideable_mins();
    const Vec3 maxs = seed.entity->get_collideable_maxs();
    return std::max({std::fabs(mins.x), std::fabs(mins.y), std::fabs(maxs.x), std::fabs(maxs.y), 8.0f});
  }
  return 16.0f;
}

inline bool same_entity(Entity* left, Entity* right) {
  return left != nullptr && right != nullptr &&
    (left == right || left->get_index() == right->get_index());
}

inline bool projectile_fov_exceeds_limit(float fov) {
  const float limit = config.aimbot.projectile_fov;
  if (!std::isfinite(fov)) {
    return true;
  }
  if (!std::isfinite(limit) || limit <= 0.0f) {
    return false;
  }

  if (limit >= 180.0f) {
    return false;
  }

  return fov > limit;
}

inline bool trace_projectile_segment(Player* local, const projectile_info& info,
  const Vec3& start, const Vec3& end, bool ignore_friendly_players,
  Entity* ignored_target, trace_t& out) {
  if (engine_trace == nullptr || local == nullptr) {
    return false;
  }
  Vec3 mins = info.hull * -1.0f;
  Vec3 maxs = info.hull;
  Vec3 trace_start = start;
  Vec3 trace_end = end;
  ray_t ray = engine_trace->init_ray(&trace_start, &trace_end, &mins, &maxs);
  trace_filter filter{};
  if (ignore_friendly_players || ignored_target != nullptr) {
    engine_trace->init_projectile_trace_filter(&filter, local->to_entity(),
      ignored_target, ignored_target != nullptr);
  } else {
    engine_trace->init_trace_filter(&filter, local->to_entity());
  }
  out = {};
  engine_trace->trace_ray(&ray, info.collision_mask, &filter, &out);
  return true;
}

inline bool reaches_target(Player* local, Weapon* weapon, const projectile_info& info, const target_seed& seed,
  const movement_path& path, const solution& shot, bool allow_splash = true) {
  if (!shot.valid || local == nullptr) {
    return false;
  }

  Vec3 launch{};
  Vec3 launch_angles{};
  const bool ignore_friendly_players = !is_rocket_weapon(weapon_id(weapon));
  if (!launch_position(local, info, shot.angles, ignore_friendly_players, launch, &launch_angles)) {
    return false;
  }

  Vec3 forward{};
  Vec3 up{};
  angle_vectors(launch_angles, &forward, nullptr, &up);
  Vec3 velocity = forward * info.speed + up * info.initial_up_velocity;
  const float gravity = 800.0f * info.gravity_mod;
  const float drag_factor = info.drag_mod > 0.0f ?
    (is_grenade_launcher(weapon_id(weapon)) ? 0.12f : 0.08f) : 0.0f;
  const bool splash_allowed = allow_splash && config.aimbot.projectile_splash_policy != 0 && info.splash_radius > 0.0f;
  const float multipoint_scale = std::clamp(config.aimbot.projectile_multipoint_scale / 100.0f, 0.5f, 1.0f);
  const float tolerance = target_radius(seed) * multipoint_scale +
    std::max({info.hull.x, info.hull.y, info.hull.z, 1.0f});
  const int max_ticks = std::clamp(static_cast<int>(std::ceil((shot.time + info.release_delay + interval()) / interval())) + 2, 1, 500);

  Vec3 position = launch;
  for (int tick = 0; tick < max_ticks; ++tick) {
    const float elapsed = static_cast<float>(tick + 1) * interval();
    Vec3 next = position + velocity * interval();
    next.z += -0.5f * gravity * interval() * interval();

    trace_t trace{};
    if (!trace_projectile_segment(local, info, position, next, ignore_friendly_players,
        seed.entity, trace)) {
      return false;
    }

    const Vec3 target_position = target_point(seed, path,
      elapsed + info.release_delay + latency_seconds());
    const bool near_target = point_segment_distance_squared(target_position, position, next) <= tolerance * tolerance;
    Entity* hit_entity = static_cast<Entity*>(trace.entity);
    if (hit_entity != nullptr) {
      if (same_entity(hit_entity, seed.entity)) {
        if (near_target) {
          return info.direct_hit || splash_allowed;
        }
        position = next;
        continue;
      }
      if (splash_allowed && point_segment_distance_squared(target_position, position, trace.endpos) <=
          (info.splash_radius + tolerance) * (info.splash_radius + tolerance)) {
        return true;
      }
      return false;
    }

    if (trace.start_solid || trace.all_solid || trace.fraction < 0.999f) {
      if (splash_allowed) {
        const Vec3 impact = trace.endpos;
        const Vec3 delta = target_position - impact;
        if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <=
            (info.splash_radius + tolerance) * (info.splash_radius + tolerance)) {
          return true;
        }
      }
      return false;
    }

    if (near_target && (info.direct_hit || splash_allowed)) {
      return true;
    }

    position = next;
    velocity.z -= gravity * interval();
    if (drag_factor > 0.0f) {
      velocity = velocity * std::clamp(1.0f - drag_factor * interval(), 0.5f, 1.0f);
    }
    if (elapsed >= shot.time + info.release_delay) {
      break;
    }
  }

  return false;
}

inline bool reaches_splash_candidate(Player* local, Weapon* weapon, const projectile_info& info,
  const target_seed& seed, const movement_path& path, const solution& shot) {
  if (!shot.valid || local == nullptr || engine_trace == nullptr) {
    return false;
  }

  Vec3 launch{};
  Vec3 launch_angles{};
  const bool ignore_friendly_players = !is_rocket_weapon(weapon_id(weapon));
  if (!launch_position(local, info, shot.angles, ignore_friendly_players, launch, &launch_angles)) {
    return false;
  }

  Vec3 forward{};
  Vec3 up{};
  angle_vectors(launch_angles, &forward, nullptr, &up);
  Vec3 velocity = forward * info.speed + up * info.initial_up_velocity;
  const float gravity = 800.0f * info.gravity_mod;
  const float drag_factor = info.drag_mod > 0.0f ?
    (is_grenade_launcher(weapon_id(weapon)) ? 0.12f : 0.08f) : 0.0f;
  const float tolerance = std::sqrt(info.hull.x * info.hull.x + info.hull.y * info.hull.y + info.hull.z * info.hull.z) + 4.0f;
  const int max_ticks = std::clamp(static_cast<int>(std::ceil(
    (shot.time + info.release_delay + interval()) / interval())) + 2, 1, 500);

  Vec3 position = launch;
  for (int tick = 0; tick < max_ticks; ++tick) {
    const float elapsed = static_cast<float>(tick + 1) * interval();
    Vec3 next = position + velocity * interval();
    next.z += -0.5f * gravity * interval() * interval();

    Vec3 trace_start = position;
    Vec3 trace_end = next;
    Vec3 hull_mins = info.hull * -1.0f;
    Vec3 hull_maxs = info.hull;
    ray_t ray = engine_trace->init_ray(&trace_start, &trace_end, &hull_mins, &hull_maxs);
    trace_filter filter{};
    engine_trace->init_world_and_props_trace_filter(&filter);
    trace_t trace{};
    engine_trace->trace_ray(&ray, projectile_collision_mask, &filter, &trace);

    const Vec3 moving_point = path_origin(seed, path,
      elapsed + info.release_delay + latency_seconds()) + seed.aim_offset;
    if (trace.start_solid || trace.all_solid) {
      return false;
    }
    if (trace.fraction < 1.0f) {
      return point_segment_distance_squared(moving_point, trace_start, trace.endpos) <= tolerance * tolerance &&
        distance_3d(trace.endpos, moving_point) <= tolerance;
    }

    position = next;
    velocity.z -= gravity * interval();
    if (drag_factor > 0.0f) {
      velocity = velocity * std::clamp(1.0f - drag_factor * interval(), 0.5f, 1.0f);
    }
    if (elapsed >= shot.time + info.release_delay) {
      break;
    }
  }

  return false;
}

inline splash_target_state target_splash_state(const target_seed& seed) {
  splash_target_state state{};
  state.mins = seed.origin;
  state.maxs = seed.origin;
  state.body = seed.origin + seed.aim_offset;
  if (seed.player != nullptr) {
    state.mins = seed.origin + seed.player->get_player_mins();
    state.maxs = seed.origin + seed.player->get_player_maxs();
  }
  else if (seed.entity != nullptr) {
    state.mins = seed.origin + seed.entity->get_collideable_mins();
    state.maxs = seed.origin + seed.entity->get_collideable_maxs();
  }
  return state;
}

inline solution solve_target(Player* local, Weapon* weapon, const projectile_info& info,
  const target_seed& seed, const movement_path& path);

inline bool solve_splash_target(Player* local, Weapon* weapon, const projectile_info& info,
  const target_seed& seed, const movement_path& path, solution& best_solution, float& best_score) {
  if (local == nullptr || weapon == nullptr || info.splash_radius <= 0.0f) {
    return false;
  }

  const splash_target_state target = target_splash_state(seed);
  std::array<splash_candidate, splashbot::candidate_limit> candidates{};
  const int count = splashbot_instance.collect_candidates(
    target, info.splash_radius, info.hull, candidates.data(), static_cast<int>(candidates.size()));
  if (count <= 0) {
    return false;
  }

  best_solution = {};
  best_score = -FLT_MAX;
  for (int index = 0; index < count; ++index) {
    const splash_candidate& candidate = candidates[index];
    target_seed candidate_seed = seed;
    candidate_seed.fixed_aim_position_valid = true;
    candidate_seed.fixed_aim_position = candidate.point;
    const solution candidate_solution = solve_target(local, weapon, info, candidate_seed, path);
    if (!candidate_solution.valid || !reaches_splash_candidate(
        local, weapon, info, candidate_seed, path, candidate_solution) ||
        !splashbot_instance.has_exposure(target, candidate, info.splash_radius)) {
      continue;
    }

    const float score = candidate.falloff * 1000.0f - candidate_solution.time * 0.01f;
    if (score > best_score) {
      best_solution = candidate_solution;
      best_score = score;
    }
  }
  return best_score > -FLT_MAX;
}

inline solution solve_target(Player* local, Weapon* weapon, const projectile_info& info,
  const target_seed& seed, const movement_path& path) {
  solution result{};
  const bool ignore_friendly_players = !is_rocket_weapon(weapon_id(weapon));
  Vec3 point = target_point(seed, path, 0.0f);
  Vec3 view_angles = aimbot_calculate_angles_to_position(local->get_shoot_pos(), point);
  float time = 0.0f;
  const float drag_factor = info.drag_mod > 0.0f ?
    (is_grenade_launcher(weapon_id(weapon)) ? 0.12f : 0.08f) : 0.0f;

  for (int iteration = 0; iteration < 8; ++iteration) {
    Vec3 launch{};
    Vec3 launch_angles{};
    if (!launch_position(local, info, view_angles, ignore_friendly_players, launch, &launch_angles)) {
      return {};
    }

    Vec3 desired_launch_angles{};
    if (!solve_ballistic(info, launch, point, drag_factor, desired_launch_angles, time)) {
      return {};
    }

    if (info.launch == launch_type::fire_setup) {
      view_angles = aimbot_clamp_angles(
        view_angles + aimbot_normalize_angle_delta(desired_launch_angles, launch_angles));
    }
    else {
      view_angles = desired_launch_angles;
    }

    const float target_time = time + info.release_delay + latency_seconds();
    const Vec3 next_point = target_point(seed, path, target_time);
    const float point_delta = distance_3d(next_point, point);
    point = next_point;
    if (point_delta <= 0.25f) {
      break;
    }
  }

  Vec3 launch{};
  Vec3 launch_angles{};
  if (!launch_position(local, info, view_angles, ignore_friendly_players, launch, &launch_angles)) {
    return {};
  }

  Vec3 desired_launch_angles{};
  if (!solve_ballistic(info, launch, point, drag_factor, desired_launch_angles, time)) {
    return {};
  }

  if (info.launch == launch_type::fire_setup) {
    view_angles = aimbot_clamp_angles(
      view_angles + aimbot_normalize_angle_delta(desired_launch_angles, launch_angles));

    if (!launch_position(local, info, view_angles, ignore_friendly_players, launch, &launch_angles) ||
        !solve_ballistic(info, launch, point, drag_factor, desired_launch_angles, time)) {
      return {};
    }
  }

  result.valid = finite(view_angles) && finite(point) && std::isfinite(time);
  result.angles = aimbot_clamp_angles(view_angles);
  result.aim_position = point;
  result.predicted_origin = seed.fixed_aim_position_valid
    ? path_origin(seed, path, time + info.release_delay + latency_seconds())
    : point - seed.aim_offset;
  result.predicted_origin_valid = finite(result.predicted_origin);
  result.time = time;
  result.predicted_fov = aimbot_calculate_fov(result.angles,
    engine != nullptr ? local->get_eye_angles() : result.angles);
  return result;
}

inline bool seed_better(const target_seed& left, const target_seed& right) {
  if (right.entity == nullptr) return true;
  if (left.preferred != right.preferred) return left.preferred;
  switch (config.aimbot.projectile_mode) {
  case 2:
    return left.distance < right.distance;
  case 0:
  case 1:
  default:
    return left.current_fov < right.current_fov;
  }
}

inline bool candidate_better(const aimbot_candidate& left, const aimbot_candidate& right) {
  if (right.entity == nullptr) return true;
  if (left.preferred != right.preferred) return left.preferred;
  if (config.aimbot.projectile_mode == 2) {
    return left.distance < right.distance;
  }

  return left.fov < right.fov;
}

}

struct charge_state {
  Weapon* weapon = nullptr;
  int weapon_def_id = TF_WEAPON_NONE;
  bool last_attack = false;
  bool last_aiming = false;
};

inline charge_state projectile_charge_state{};

inline float current_time(Player* local);

inline bool projectile_modifier_enabled(uint32_t modifier) {
  return (config.aimbot.projectile_modifiers & modifier) != 0;
}

inline bool is_bow(Weapon* weapon) {
  return weapon != nullptr && detail::weapon_id(weapon) == TF_WEAPON_COMPOUND_BOW;
}

inline float charge_elapsed(Weapon* weapon, Player* local) {
  if (weapon == nullptr) {
    return 0.0f;
  }

  const float begin = weapon->get_charge_begin_time();
  if (begin <= 0.0f) {
    return 0.0f;
  }

  const float now = current_time(local);
  return std::isfinite(now) ? std::max(now - begin, 0.0f) : 0.0f;
}

inline bool same_charge_weapon(Weapon* weapon) {
  return weapon != nullptr && projectile_charge_state.weapon == weapon &&
    projectile_charge_state.weapon_def_id == weapon->get_def_id();
}

inline void reset_charge_tracking() {
  projectile_charge_state = {};
}

inline bool cancel_charge_if_needed(user_cmd* cmd, Player* local, Weapon* weapon) {
  if (cmd == nullptr || local == nullptr || !is_bow(weapon) ||
      !same_charge_weapon(weapon) || !projectile_charge_state.last_aiming ||
      !projectile_modifier_enabled(Aim::projectile_mod_cancel_charge)) {
    return false;
  }

  const bool attack_held = (cmd->buttons & IN_ATTACK) != 0;
  const float charge = charge_elapsed(weapon, local);
  const bool released = !attack_held;
  const bool overcharged = charge >= 0.95f;

  if (!projectile_charge_state.last_attack || (!released && !overcharged)) {
    return false;
  }

  cmd->buttons |= IN_ATTACK2;
  cmd->buttons &= ~IN_ATTACK;
  projectile_charge_state.last_attack = false;
  projectile_charge_state.last_aiming = false;
  return true;
}

inline float current_time(Player* local) {
  return global_vars != nullptr
    ? global_vars->curtime
    : (local != nullptr ? local->get_tickbase() * detail::interval() : 0.0f);
}

struct apply_result {
  bool attack_ready = false;
  bool requested_shot = false;
  bool psilent = false;
};

inline aimbot_candidate find_candidate(Player* local, Weapon* weapon, const Vec3& original_view_angles) {
  aimbot_candidate best{};
  if (local == nullptr || weapon == nullptr || !config.aimbot.projectile_active) {
    return best;
  }

  detail::projectile_info info{};
  if (!detail::get_info(local, weapon, info)) {
    return best;
  }

  std::vector<detail::target_seed> seeds{};
  seeds.reserve(entity_cache_players().size() + 3);
  const Vec3 shoot_pos = local->get_shoot_pos();

  for (const entity_cache_player_entry& entry : entity_cache_players()) {
    ++aim_state::scan.candidates_total;
    const auto skip_reason = aimbot_player_skip_reason_for(local, entry, weapon);
    if (skip_reason != aimbot_player_skip_reason::none) {
      aim_state::record_player_skip(skip_reason, entry.player);
      continue;
    }

    detail::target_seed seed{};
    seed.entity = entry.entity != nullptr ? entry.entity : entry.player->to_entity();
    seed.player = entry.player;
    seed.origin = entry.player->get_origin();
    seed.aim_offset = detail::target_offset(entry.player, weapon);
    seed.velocity = entry.player->get_velocity();
    seed.current_fov = aimbot_calculate_fov(
      aimbot_calculate_angles_to_position(shoot_pos, seed.origin + seed.aim_offset), original_view_angles);
    seed.distance = distance_3d(shoot_pos, seed.origin);
    seed.preferred = aimbot_player_is_preferred(entry.player);
    if (config.aimbot.projectile_mode == 0 && detail::projectile_fov_exceeds_limit(seed.current_fov)) {
      aim_state::record_reject(aim_state::make_reject_debug(seed.entity, aimbot_reject_reason::fov,
        seed.current_fov, config.aimbot.projectile_fov, seed.distance));
      continue;
    }
    seeds.push_back(seed);
  }

  constexpr class_id building_ids[] = {class_id::SENTRY, class_id::DISPENSER, class_id::TELEPORTER};
  if (aimbot_aim_at_enabled(Aim::aim_at_buildings)) {
    for (const class_id id : building_ids) {
      for (Entity* entity : entity_cache[id]) {
        if (aimbot_should_skip_non_player_target(local, entity)) {
          continue;
        }
        detail::target_seed seed{};
        seed.entity = entity;
        seed.origin = entity->get_collision_origin();
        const Vec3 mins = entity->get_collideable_mins();
        const Vec3 maxs = entity->get_collideable_maxs();
        seed.aim_offset = (mins + maxs) * 0.5f;
        seed.current_fov = aimbot_calculate_fov(
          aimbot_calculate_angles_to_position(shoot_pos, seed.origin + seed.aim_offset), original_view_angles);
        seed.distance = distance_3d(shoot_pos, seed.origin);
        if (config.aimbot.projectile_mode == 0 && detail::projectile_fov_exceeds_limit(seed.current_fov)) {
          continue;
        }
        seeds.push_back(seed);
      }
    }
  }

  const auto append_static_target = [&](Entity* entity) {
    if (entity == nullptr || aimbot_should_skip_non_player_target(local, entity)) {
      return;
    }

    detail::target_seed seed{};
    seed.entity = entity;
    seed.origin = entity->get_collision_origin();
    const Vec3 mins = entity->get_collideable_mins();
    const Vec3 maxs = entity->get_collideable_maxs();
    seed.aim_offset = (mins + maxs) * 0.5f;
    seed.current_fov = aimbot_calculate_fov(
      aimbot_calculate_angles_to_position(shoot_pos, seed.origin + seed.aim_offset), original_view_angles);
    seed.distance = distance_3d(shoot_pos, seed.origin);
    if (config.aimbot.projectile_mode == 0 && detail::projectile_fov_exceeds_limit(seed.current_fov)) {
      return;
    }
    seeds.push_back(seed);
  };

  if (aimbot_aim_at_enabled(Aim::aim_at_npcs)) {
    for (Entity* entity : entity_cache_npcs()) {
      append_static_target(entity);
    }
  }
  if (aimbot_aim_at_enabled(Aim::aim_at_stickies)) {
    for (Entity* entity : entity_cache_entities(class_id::PILL_OR_STICKY)) {
      append_static_target(entity);
    }
  }
  if (aimbot_aim_at_enabled(Aim::aim_at_bombs)) {
    for (Entity* entity : entity_cache_entities(class_id::PUMPKIN)) {
      append_static_target(entity);
    }
  }

  std::stable_sort(seeds.begin(), seeds.end(), detail::seed_better);
  const int max_attempts = std::clamp(config.aimbot.projectile_max_sim_targets, 1, 6);
  int attempts = 0;
  for (const detail::target_seed& seed : seeds) {
    if (attempts >= max_attempts && seed.distance > 400.0f) {
      continue;
    }
    ++attempts;

    detail::movement_path path{};
    if (config.aimbot.projectile_prediction_mode == Aim::ProjectilePredictionMode::MOVE_SIM && seed.player != nullptr) {

      const Vec3 initial_delta = seed.origin + seed.aim_offset - shoot_pos;
      const float initial_distance = distance_3d(shoot_pos, seed.origin + seed.aim_offset);
      const float distance_scale = initial_distance > 0.001f ? 1.0f / initial_distance : 0.0f;
      const Vec3 initial_direction = initial_delta * distance_scale;
      const float target_closing_speed = seed.velocity.x * initial_direction.x +
        seed.velocity.y * initial_direction.y + seed.velocity.z * initial_direction.z;
      const float effective_projectile_speed = std::max(
        info.speed - target_closing_speed, info.speed * 0.25f);
      const float estimated_flight_time = info.speed > 0.0f
        ? initial_distance / effective_projectile_speed : 0.0f;
      const float required_sim_time = estimated_flight_time + detail::latency_seconds() +
        info.release_delay + detail::interval() * 2.0f;
      const float sim_time = std::max(config.aimbot.projectile_max_sim_time,
        std::min(required_sim_time, 5.0f));
      if (!detail::build_move_path(seed, sim_time, path)) {
        path = {};
      }
    }

    std::array<int, 4> positions{ config.aimbot.projectile_aim_pos, 0, 0, 0 };
    int position_count = 1;
    if (seed.player != nullptr && config.aimbot.projectile_aim_pos == 0) {
      positions[0] = detail::target_position(seed.player, weapon);
      for (const int position : {1, 2, 3}) {
        if (position != positions[0]) {
          positions[position_count++] = position;
        }
      }
    }

    bool accepted = false;
    float last_fov = seed.current_fov;
    for (int position_index = 0; position_index < position_count; ++position_index) {
      detail::target_seed point_seed = seed;
      if (point_seed.player != nullptr) {
        point_seed.aim_offset = detail::target_offset_for_position(
          point_seed.player, positions[position_index]);
      }

      const auto make_candidate = [&](const detail::solution& shot,
        const detail::target_seed& shot_seed, aimbot_candidate& out) {
        if (!shot.valid) {
          return false;
        }
        const float current_fov = aimbot_calculate_fov(
          aimbot_calculate_angles_to_position(
            shoot_pos, shot_seed.origin + shot_seed.aim_offset),
          original_view_angles);
        const float predicted_fov = aimbot_calculate_fov(shot.angles, original_view_angles);
        const float fov = config.aimbot.projectile_mode == 0 ? current_fov : predicted_fov;
        last_fov = fov;
        if (config.aimbot.projectile_mode != 2 && detail::projectile_fov_exceeds_limit(fov)) {
          return false;
        }

        out = {};
        out.entity = shot_seed.entity;
        out.player = shot_seed.player;
        out.aim_position = shot.aim_position;
        out.predicted_origin = shot.predicted_origin;
        out.predicted_origin_valid = shot.predicted_origin_valid && shot_seed.player != nullptr;
        out.aim_angles = shot.angles;
        out.command_angles = shot.angles;
        out.fov = fov;
        out.distance = shot_seed.distance;
        out.health = shot_seed.player != nullptr ? shot_seed.player->get_health() : aimbot_entity_health(shot_seed.entity);
        out.simulation_time = shot_seed.entity != nullptr ? shot_seed.entity->get_simulation_time() : 0.0f;
        out.visible = true;
        out.preferred = shot_seed.preferred;
        out.hitbox = positions[position_index] == 3 ? aim_hitbox_head : -1;
        out.debug_reason = aimbot_debug_reason::attack_ready;
        return true;
      };

      if (!info.direct_hit) {
        if (info.splash_radius <= 0.0f) {
          continue;
        }

        detail::solution splash_solution{};
        float splash_score = -FLT_MAX;
        aimbot_candidate splash{};
        if (!detail::solve_splash_target(local, weapon, info, point_seed, path,
            splash_solution, splash_score) || !make_candidate(splash_solution, point_seed, splash)) {
          continue;
        }

        ++aim_state::scan.candidates_visible;
        if (detail::candidate_better(splash, best)) {
          best = splash;
        }
        accepted = true;
        break;
      }

      detail::solution direct_solution{};
      const auto direct_candidate = [&]() {
        aimbot_candidate result{};
        direct_solution = detail::solve_target(local, weapon, info, point_seed, path);
        if (detail::reaches_target(local, weapon, info, point_seed, path, direct_solution, false) &&
            make_candidate(direct_solution, point_seed, result)) {
          return std::pair<bool, aimbot_candidate>{true, result};
        }
        return std::pair<bool, aimbot_candidate>{false, result};
      };

      const int splash_policy = std::clamp(config.aimbot.projectile_splash_policy, 0, 2);
      auto [direct_valid, direct] = direct_candidate();
      detail::solution splash_solution{};
      float splash_score = -FLT_MAX;
      aimbot_candidate splash{};

      const bool splash_valid = (position_index == 0 || point_seed.player == nullptr) &&
        splash_policy != 0 && info.splash_radius > 0.0f &&
        detail::solve_splash_target(local, weapon, info, point_seed, path, splash_solution, splash_score) &&
        make_candidate(splash_solution, point_seed, splash);

      aimbot_candidate selected{};
      bool selected_valid = false;
      if (splash_policy == 2) {
        selected_valid = splash_valid ? (selected = splash, true) : (direct_valid ? (selected = direct, true) : false);
      }
      else if (splash_policy == 1) {
        const float direct_score = direct_valid ? 1000.0f - direct_solution.time * 0.01f : -FLT_MAX;
        const bool prefer_splash = splash_valid && (!direct_valid || splash_score > direct_score);
        selected_valid = prefer_splash ? (selected = splash, true) : (direct_valid ? (selected = direct, true) : false);
      }
      else {
        selected_valid = direct_valid ? (selected = direct, true) : false;
      }

      if (!selected_valid) {
        continue;
      }

      ++aim_state::scan.candidates_visible;
      if (detail::candidate_better(selected, best)) {
        best = selected;
      }
      accepted = true;
      break;
    }

    if (!accepted) {
      aim_state::record_reject(aim_state::make_reject_debug(seed.entity,
        aimbot_reject_reason::no_candidate, last_fov, config.aimbot.projectile_fov, seed.distance));
    }
  }

  return best;
}

inline apply_result apply(user_cmd* cmd, Player* local, Weapon* weapon,
  const Vec3& original_view_angles, const aimbot_candidate& target,
  bool manual_attack = false) {
  apply_result result{};
  if (cmd == nullptr || local == nullptr || weapon == nullptr || target.entity == nullptr) {
    return result;
  }

  detail::projectile_info info{};
  if (!detail::get_info(local, weapon, info)) {
    return result;
  }

  const int attack_button = info.secondary_attack ? IN_ATTACK2 : IN_ATTACK;
  const int id = detail::weapon_id(weapon);
  const bool charge_weapon = id == TF_WEAPON_COMPOUND_BOW || id == TF_WEAPON_PIPEBOMBLAUNCHER;
  const float charge_time = charge_weapon ? charge_elapsed(weapon, local) : 0.0f;
  const bool charged = charge_weapon && charge_time > 0.0f;
  const bool cannon_detonating = id == TF_WEAPON_CANNON && weapon->get_detonate_time() > 0.0f;
  const bool has_ammo = weapon->get_clip1() != 0;
  const bool raw_attack = (cmd->buttons & attack_button) != 0;
  bool manual_bow_release = is_bow(weapon) && same_charge_weapon(weapon) &&
    projectile_charge_state.last_aiming && projectile_charge_state.last_attack && !raw_attack;
  const bool can_attack = has_ammo;

  Vec3 target_angles = target.command_angles;
  const Aim::AimMode aim_mode = static_cast<Aim::AimMode>(std::clamp(
    static_cast<int>(config.aimbot.aim_mode), 0, 3));
  if (config.aimbot.projectile_smooth_flamethrowers_active && weapon->is_flamethrower()) {
    target_angles = aimbot_lerp_angles(original_view_angles, target_angles,
      std::clamp(config.aimbot.projectile_smooth_flamethrowers / 100.0f, 0.0f, 1.0f));
  } else if (aim_mode == Aim::AimMode::SMOOTH || aim_mode == Aim::AimMode::ASSISTIVE) {
    const aimbot::aimbot_state& state = aimbot::current_state();
    target_angles = aimbot_apply_mode_angles(
      original_view_angles,
      target_angles,
      state.last_input_angles,
      state.last_input_angles_valid,
      target);
  }

  const bool manual_release_ready = manual_bow_release;
  const bool release_requested = !manual_attack && config.aimbot.auto_shoot && has_ammo &&
    (charged || cannon_detonating);
  if (release_requested || manual_release_ready) {

    cmd->buttons &= ~attack_button;
    if (release_requested) {
      result.requested_shot = true;
    }
  }
  else if (!manual_attack && config.aimbot.auto_shoot && has_ammo && !charged && !cannon_detonating &&
      !manual_bow_release) {
    cmd->buttons |= attack_button;
    result.requested_shot = true;
  }

  if (is_bow(weapon) && projectile_modifier_enabled(Aim::projectile_mod_charge_weapon) &&
      !charged && same_charge_weapon(weapon) && projectile_charge_state.last_aiming &&
      projectile_charge_state.last_attack && !manual_release_ready) {
    cmd->buttons |= IN_ATTACK;
  }

  if (!has_ammo) {
    cmd->buttons &= ~attack_button;
  }

  const bool firing = (cmd->buttons & attack_button) != 0;
  const bool shot_command = firing || release_requested || manual_bow_release;
  result.attack_ready = can_attack && shot_command;
  result.psilent = aim_mode == Aim::AimMode::PSILENT && shot_command && !manual_attack;

  if (config.aimbot.spread_compensation && shot_command) {
    target_angles = detail::compensate_projectile_spread(
      local, weapon, cmd, info, target_angles);
  }
  cmd->view_angles = aimbot_clamp_angles(target_angles);

  if ((aim_mode != Aim::AimMode::PSILENT || manual_attack) && prediction != nullptr) {
    prediction->set_local_view_angles(cmd->view_angles);
    prediction->set_view_angles(cmd->view_angles);
  }
  if ((aim_mode != Aim::AimMode::PSILENT || manual_attack) && engine != nullptr) {
    engine->set_view_angles(cmd->view_angles);
  }

  projectile_charge_state.weapon = weapon;
  projectile_charge_state.weapon_def_id = weapon->get_def_id();
  projectile_charge_state.last_attack = (cmd->buttons & IN_ATTACK) != 0;
  projectile_charge_state.last_aiming = true;

  return result;
}

}
#endif
