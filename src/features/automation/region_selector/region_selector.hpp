/*
/^-----^\   data: 2026-05-06
V  o o  V  file: src/features/automation/region_selector/region_selector.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef REGION_SELECTOR_HPP
#define REGION_SELECTOR_HPP
#include <array>
#include <cstdint>
#include <string_view>
#include "games/tf2/sdk/interfaces/steam_networking_utils.hpp"

namespace automation::region_selector
{

struct data_center
{
  const char* continent;
  const char* label;
  const char* code;
  std::uint64_t bit;
};

constexpr std::array<data_center, 31> data_centers{{

  {"Europe", "frankfurt (fra)", "fra", 1ull << 1},
  {"Europe", "falkenstein (fsn)", "fsn", 1ull << 2},
  {"Europe", "helsinki (hel)", "hel", 1ull << 3},
  {"Europe", "london (lhr)", "lhr", 1ull << 4},
  {"Europe", "madrid (mad)", "mad", 1ull << 5},
  {"Europe", "paris (par)", "par", 1ull << 6},
  {"Europe", "stockholm 1 (sto)", "sto", 1ull << 7},
  {"Europe", "stockholm 2 (sto2)", "sto2", 1ull << 8},
  {"Europe", "vienna (vie)", "vie", 1ull << 13},
  {"Europe", "warsaw (waw)", "waw", 1ull << 9},

  {"North America", "atlanta (atl)", "atl", 1ull << 14},
  {"North America", "dallas (dfw)", "dfw", 1ull << 22},
  {"North America", "moses lake (eat)", "eat", 1ull << 15},
  {"North America", "virginia (iad)", "iad", 1ull << 17},
  {"North America", "los angeles (lax)", "lax", 1ull << 18},
  {"North America", "chicago (ord)", "ord", 1ull << 20},
  {"North America", "seattle (sea)", "sea", 1ull << 21},

  {"South America", "sao paulo (gru)", "gru", 1ull << 23},
  {"South America", "lima (lim)", "lim", 1ull << 24},
  {"South America", "santiago (scl)", "scl", 1ull << 25},
  {"South America", "buenos aires (eze)", "eze", 1ull << 26},

  {"Asia", "mumbai (bom2)", "bom2", 1ull << 27},
  {"Asia", "dubai (dxb)", "dxb", 1ull << 28},
  {"Asia", "hong kong (hkg)", "hkg", 1ull << 30},
  {"Asia", "chennai/ambattur (maa2)", "maa2", 1ull << 32},
  {"Asia", "seoul (seo)", "seo", 1ull << 39},
  {"Asia", "singapore (sgp)", "sgp", 1ull << 35},
  {"Asia", "tokyo (tyo)", "tyo", 1ull << 36},

  {"Oceania", "sydney (syd)", "syd", 1ull << 45},
  {"Oceania", "guam (gum)", "gum", 1ull << 47},

  {"Africa", "johannesburg (jnb)", "jnb", 1ull << 46},
}};

constexpr std::uint64_t all_region_bits = [] {
  std::uint64_t bits = 0;
  for (const auto& data_center : data_centers)
  {
    bits |= data_center.bit;
  }
  return bits;
}();
constexpr int blocked_region_ping = 69420;
constexpr int preferred_region_ping = 1;

bool is_region_allowed(std::string_view region);
void set_region_allowed(std::uint64_t bit, bool allowed);
bool is_region_bit_allowed(std::uint64_t bit);
bool are_all_continent_regions_allowed(std::string_view continent);
void set_continent_regions_allowed(std::string_view continent, bool allowed);
int adjust_ping(int original_ping, steam_networking_pop_id pop_id);
void refresh_ping_data();

}
#endif
