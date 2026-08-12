#include "steam_nographics.hpp"

#include <dlfcn.h>
#include <link.h>
#include <sys/types.h>

namespace
{

using x_display = void;
using x_window = unsigned long;
using x_bool = int;
using gl_enum = unsigned int;
using gl_sizei = int;
using egl_display = void*;
using egl_surface = void*;
using egl_boolean = unsigned int;
using glx_drawable = unsigned long;
using sdl_window = void;
using sdl_window_flags = std::uint64_t;

constexpr sdl_window_flags sdl_window_hidden = 0x00000008ULL;

template <typename function_type>
function_type next_symbol(const char* name)
{
  return reinterpret_cast<function_type>(dlsym(RTLD_NEXT, name));
}

}

#define CAT_STEAM_NOGRAPHICS_EXPORT extern "C" __attribute__((visibility("default")))

__attribute__((constructor)) static void steam_nographics_constructor()
{
  steam_nographics::initialize();
}

CAT_STEAM_NOGRAPHICS_EXPORT void* dlopen(const char* path, int flags)
{
  using function_type = void* (*)(const char*, int);
  static const auto original = next_symbol<function_type>("dlopen");
  void* const result = original != nullptr ? original(path, flags) : nullptr;
  if (result != nullptr)
  {
    steam_nographics::on_library_loaded(path, result);
  }
  return result;
}

CAT_STEAM_NOGRAPHICS_EXPORT void* dlmopen(Lmid_t namespace_id, const char* path, int flags)
{
  using function_type = void* (*)(Lmid_t, const char*, int);
  static const auto original = next_symbol<function_type>("dlmopen");
  void* const result = original != nullptr ? original(namespace_id, path, flags) : nullptr;
  if (result != nullptr)
  {
    steam_nographics::on_library_loaded(path, result);
  }
  return result;
}

CAT_STEAM_NOGRAPHICS_EXPORT x_window XCreateWindow(
  x_display* display, x_window parent, int x, int y, unsigned int width, unsigned int height,
  unsigned int border_width, int depth, unsigned int window_class, void* visual,
  unsigned long value_mask, void* attributes)
{
  using function_type = x_window (*)(x_display*, x_window, int, int, unsigned int, unsigned int,
                                     unsigned int, int, unsigned int, void*, unsigned long, void*);
  static const auto original = next_symbol<function_type>("XCreateWindow");
  if (original == nullptr)
  {
    return 0;
  }
  if (steam_nographics::should_hide_windows())
  {
    return original(display, parent, steam_nographics::offscreen_coordinate(), steam_nographics::offscreen_coordinate(),
      steam_nographics::offscreen_extent(), steam_nographics::offscreen_extent(), border_width, depth,
      window_class, visual, value_mask, attributes);
  }
  return original(display, parent, x, y, width, height, border_width, depth, window_class, visual, value_mask, attributes);
}

CAT_STEAM_NOGRAPHICS_EXPORT x_bool XMapRaised(x_display* display, x_window window)
{
  using function_type = x_bool (*)(x_display*, x_window);
  if (steam_nographics::should_hide_windows())
  {
    return 0;
  }
  static const auto original = next_symbol<function_type>("XMapRaised");
  return original != nullptr ? original(display, window) : 0;
}

CAT_STEAM_NOGRAPHICS_EXPORT sdl_window* SDL_CreateWindow(
  const char* title, int width, int height, sdl_window_flags flags)
{
  using function_type = sdl_window* (*)(const char*, int, int, sdl_window_flags);
  static const auto original = next_symbol<function_type>("SDL_CreateWindow");
  if (original == nullptr)
  {
    return nullptr;
  }
  if (steam_nographics::should_hide_windows())
  {
    return original(title, 1, 1, flags | sdl_window_hidden);
  }
  return original(title, width, height, flags);
}

CAT_STEAM_NOGRAPHICS_EXPORT int SDL_GL_SwapWindow(sdl_window* window)
{
  using function_type = int (*)(sdl_window*);
  if (steam_nographics::should_skip_presentation())
  {
    steam_nographics::limit_present_rate();
    return 1;
  }
  static const auto original = next_symbol<function_type>("SDL_GL_SwapWindow");
  return original != nullptr ? original(window) : 0;
}

CAT_STEAM_NOGRAPHICS_EXPORT void glXSwapBuffers(void* display, glx_drawable drawable)
{
  using function_type = void (*)(void*, glx_drawable);
  if (steam_nographics::should_skip_presentation())
  {
    steam_nographics::limit_present_rate();
    return;
  }
  static const auto original = next_symbol<function_type>("glXSwapBuffers");
  if (original != nullptr)
  {
    original(display, drawable);
  }
}

CAT_STEAM_NOGRAPHICS_EXPORT egl_boolean eglSwapBuffers(egl_display display, egl_surface surface)
{
  using function_type = egl_boolean (*)(egl_display, egl_surface);
  if (steam_nographics::should_skip_presentation())
  {
    steam_nographics::limit_present_rate();
    return 1;
  }
  static const auto original = next_symbol<function_type>("eglSwapBuffers");
  return original != nullptr ? original(display, surface) : 0;
}

CAT_STEAM_NOGRAPHICS_EXPORT void glClear(gl_enum mask)
{
  using function_type = void (*)(gl_enum);
  if (steam_nographics::should_skip_draw_calls())
  {
    return;
  }
  static const auto original = next_symbol<function_type>("glClear");
  if (original != nullptr)
  {
    original(mask);
  }
}

CAT_STEAM_NOGRAPHICS_EXPORT void glDrawArrays(gl_enum mode, int first, gl_sizei count)
{
  using function_type = void (*)(gl_enum, int, gl_sizei);
  if (steam_nographics::should_skip_draw_calls())
  {
    return;
  }
  static const auto original = next_symbol<function_type>("glDrawArrays");
  if (original != nullptr)
  {
    original(mode, first, count);
  }
}

CAT_STEAM_NOGRAPHICS_EXPORT void glDrawElements(gl_enum mode, gl_sizei count, gl_enum type, const void* indices)
{
  using function_type = void (*)(gl_enum, gl_sizei, gl_enum, const void*);
  if (steam_nographics::should_skip_draw_calls())
  {
    return;
  }
  static const auto original = next_symbol<function_type>("glDrawElements");
  if (original != nullptr)
  {
    original(mode, count, type, indices);
  }
}

CAT_STEAM_NOGRAPHICS_EXPORT int execve(const char* path, char* const argv[], char* const envp[])
{
  using function_type = int (*)(const char*, char* const[], char* const[]);
  static const auto original = next_symbol<function_type>("execve");
  char** const rewritten = steam_nographics::rewrite_webhelper_argv(path, argv);
  return original != nullptr ? original(path, rewritten != nullptr ? rewritten : argv, envp) : -1;
}

CAT_STEAM_NOGRAPHICS_EXPORT int execvp(const char* file, char* const argv[])
{
  using function_type = int (*)(const char*, char* const[]);
  static const auto original = next_symbol<function_type>("execvp");
  char** const rewritten = steam_nographics::rewrite_webhelper_argv(file, argv);
  return original != nullptr ? original(file, rewritten != nullptr ? rewritten : argv) : -1;
}

CAT_STEAM_NOGRAPHICS_EXPORT int posix_spawn(pid_t* pid, const char* path, const void* actions,
  const void* attributes, char* const argv[], char* const envp[])
{
  using function_type = int (*)(pid_t*, const char*, const void*, const void*, char* const[], char* const[]);
  static const auto original = next_symbol<function_type>("posix_spawn");
  char** const rewritten = steam_nographics::rewrite_webhelper_argv(path, argv);
  return original != nullptr ? original(pid, path, actions, attributes, rewritten != nullptr ? rewritten : argv, envp) : -1;
}
