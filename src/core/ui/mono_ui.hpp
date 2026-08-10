#pragma once

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

bool mono_ui_initialize_opengl(SDL_Window *window);
bool mono_ui_initialize_vulkan(SDL_Window *window);
bool mono_ui_backend_matches(bool vulkan);
void mono_ui_shutdown(bool release_graphics_resources);
bool mono_ui_initialized();
void mono_ui_apply_theme();
bool mono_ui_should_block_input();
bool mono_ui_build_opengl_frame();
void mono_ui_lock();
void mono_ui_unlock();
void mono_ui_process_event(const SDL_Event *event);

bool mono_ui_begin_frame();
void mono_ui_end_frame();
void mono_ui_release_frame();
