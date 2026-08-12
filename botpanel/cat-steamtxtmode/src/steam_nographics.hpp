#ifndef CAT_STEAM_NOGRAPHICS_HPP
#define CAT_STEAM_NOGRAPHICS_HPP

#include <cstdint>

namespace steam_nographics
{

void initialize();
void on_library_loaded(const char* library_path, void* module_handle);

[[nodiscard]] bool is_enabled();
[[nodiscard]] bool should_hide_windows();
[[nodiscard]] bool should_skip_presentation();
[[nodiscard]] bool should_skip_draw_calls();
[[nodiscard]] int offscreen_coordinate();
[[nodiscard]] unsigned int offscreen_extent();
void limit_present_rate();

char** rewrite_webhelper_argv(const char* executable_path, char* const argv[]);

}

#endif
