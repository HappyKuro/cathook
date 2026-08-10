/*
/^-----^\   data: 2026-05-08
V  o o  V  file: src/core/hooks/host_is_secure_server_allowed.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include <cstdint>
#include "core/shared/sigs.hpp"
#include "features/menu/config.hpp"
#include "libsigscan/libsigscan.h"

using host_is_secure_server_allowed_fn = bool (*)();

host_is_secure_server_allowed_fn host_is_secure_server_allowed_original = nullptr;

bool* get_allow_secure_servers_flag()
{
  static bool* allow_secure_servers = nullptr;
  static int frames_until_retry = 0;

  if (allow_secure_servers != nullptr)
  {
    return allow_secure_servers;
  }

  if (frames_until_retry > 0)
  {
    --frames_until_retry;
    return nullptr;
  }

  auto* match = reinterpret_cast<std::uint8_t*>(sigscan_module("engine.so", sigs::allow_secure_servers_flag_ref));
  if (match == nullptr)
  {
    frames_until_retry = 128;
    return nullptr;
  }

  const auto displacement = *reinterpret_cast<std::int32_t*>(match + 3);
  allow_secure_servers = reinterpret_cast<bool*>(match + 7 + displacement);
  return allow_secure_servers;
}

bool host_is_secure_server_allowed_hook()
{
  CATHOOK_HOOK_GUARD();
  if (config.misc.exploits.vac_bypass)
  {
    if (auto* allow_secure_servers = get_allow_secure_servers_flag(); allow_secure_servers != nullptr)
    {
      *allow_secure_servers = true;
    }

    return true;
  }

  if (host_is_secure_server_allowed_original == nullptr)
  {
    return true;
  }

  return host_is_secure_server_allowed_original();
}
