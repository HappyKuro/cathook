/*
/^-----^\   data: 2026-08-10
V  o o  V  file: src/features/misc/removals.hpp
 |  Y  |   author: HappyKuro
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#ifndef REMOVALS_HPP
#define REMOVALS_HPP

namespace removals
{

void on_create_move();

[[nodiscard]] bool should_skip_model(const char* model_name);

}

#endif
