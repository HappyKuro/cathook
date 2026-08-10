#include "skybox_changer.hpp"

#include <algorithm>

#include "core/print.hpp"
#include "core/shared/sigs.hpp"
#include "features/menu/config.hpp"
#include "games/tf2/sdk/interfaces/convar_system.hpp"
#include "libsigscan/libsigscan.h"

namespace skybox_changer
{

namespace
{

struct option
{
  const char* sky_name;
  const char* label;
};

constexpr option options[] = {
  { nullptr, "Off" },
  { "sky_tf2_04", "TF2 04" },
  { "sky_upward", "Upward" },
  { "sky_dustbowl_01", "Dustbowl" },
  { "sky_goldrush_01", "Goldrush" },
  { "sky_granary_01", "Granary" },
  { "sky_well_01", "Well" },
  { "sky_gravel_01", "Gravel Pit" },
  { "sky_badlands_01", "Badlands" },
  { "sky_hydro_01", "Hydro" },
  { "sky_night_01", "Night 01" },
  { "sky_nightfall_01", "Nightfall" },
  { "sky_trainyard_01", "Trainyard" },
  { "sky_stormfront_01", "Stormfront" },
  { "sky_morningsnow_01", "Morning Snow" },
  { "sky_alpinestorm_01", "Alpine Storm" },
  { "sky_harvest_01", "Harvest" },
  { "sky_harvest_night_01", "Harvest Night" },
  { "sky_halloween", "Halloween" },
  { "sky_halloween_night_01", "Halloween Night" },
  { "sky_halloween_night2014_01", "Halloween Night 2014" },
  { "sky_island_01", "Island" },
  { "sky_jungle_01", "Jungle" },
  { "sky_invasion2fort_01", "Invasion 2Fort" },
  { "sky_well_02", "Well 02" },
  { "sky_outpost_01", "Outpost" },
  { "sky_coastal_01", "Coastal" },
  { "sky_rainbow_01", "Rainbow" },
  { "sky_badlands_pyroland_01", "Badlands Pyroland" },
  { "sky_pyroland_01", "Pyroland 01" },
  { "sky_pyroland_02", "Pyroland 02" },
  { "sky_pyroland_03", "Pyroland 03" },
};

constexpr int option_count_value = sizeof(options) / sizeof(options[0]);
using load_named_skys_fn = bool (*)(const char*);

load_named_skys_fn load_named_skys = nullptr;
Convar* sky_name = nullptr;
int applied_index = -1;

const char* const* labels()
{
  static const char* values[option_count_value]{};
  static bool initialized = false;
  if (!initialized) {
    for (int index = 0; index < option_count_value; ++index) {
      values[index] = options[index].label;
    }
    initialized = true;
  }
  return values;
}

bool load_default_sky()
{
  if (sky_name == nullptr && convar_system != nullptr) {
    sky_name = convar_system->find_var("sv_skyname");
  }

  if (sky_name == nullptr) {
    return false;
  }

  const char* name = sky_name->get_string();
  return name != nullptr && name[0] != '\0' && load_named_skys(name);
}

}

int option_count()
{
  return option_count_value;
}

const char* const* option_names()
{
  return labels();
}

void resolve_load_named_skys()
{
  load_named_skys = reinterpret_cast<load_named_skys_fn>(
    sigscan_module("engine.so", sigs::load_named_skys));

  if (load_named_skys == nullptr) {
    print("Failed to find LoadNamedSkys; skybox changer disabled\n");
  }
}

void update()
{
  if (load_named_skys == nullptr) {
    return;
  }

  const int index = std::clamp(config.visuals.skybox_changer_index, 0, option_count_value - 1);
  if (index == applied_index) {
    return;
  }

  const bool loaded = index == 0 ? load_default_sky() : load_named_skys(options[index].sky_name);
  if (!loaded) {
    print("Failed to load skybox option %d\n", index);
  }

  applied_index = index;
}

void invalidate()
{
  applied_index = -1;
}

}
