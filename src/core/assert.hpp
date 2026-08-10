/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/core/assert.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#include <SDL2/SDL.h>

#include "core/logger.hpp"

#define error_box(message) \
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,			\
			   "Fatal Error",				\
			   message,					\
			   nullptr);

#define error_assert(expression, message)				\
  if (expression) {							\
    cathook::core::log_raw("fatal error: ");				\
    cathook::core::log_raw(message);					\
    error_box(message);							\
    exit(1);								\
  }
