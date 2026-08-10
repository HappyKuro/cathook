#ifndef CATHOOK_CONSOLE_PRINT_HPP
#define CATHOOK_CONSOLE_PRINT_HPP

#include <cstdarg>
#include <cstdio>
#include <dlfcn.h>
#include <string_view>
#include <vector>

#include "core/print.hpp"
#include "games/tf2/sdk/interfaces/steam_runtime.hpp"

namespace cathook::core
{

namespace detail
{

using console_msg_fn = void (*)(const char*, ...);

inline console_msg_fn resolve_console_msg()
{
  static const console_msg_fn message = []
  {
    void* tier0 = open_loaded_library("libtier0.so");
    if (tier0 == nullptr) {
      return static_cast<console_msg_fn>(nullptr);
    }

    auto* symbol = reinterpret_cast<console_msg_fn>(dlsym(tier0, "Msg"));
    dlclose(tier0);
    return symbol;
  }();

  return message;
}

}

[[gnu::format(printf, 1, 2)]] inline void console_print(const char* format, ...)
{
  if (format == nullptr) {
    return;
  }

  va_list args;
  va_start(args, format);

  va_list length_args;
  va_copy(length_args, args);
  const int length = std::vsnprintf(nullptr, 0, format, length_args);
  va_end(length_args);

  if (length < 0) {
    va_end(args);
    return;
  }

  std::vector<char> buffer(static_cast<std::size_t>(length) + 1);
  std::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);

  log_raw(std::string_view{buffer.data(), static_cast<std::size_t>(length)});

  if (const auto message = detail::resolve_console_msg(); message != nullptr) {
    message("%s", buffer.data());
  }
}

}

#endif
