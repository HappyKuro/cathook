/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/core/memory/memory.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <string>
#include <fstream>
#include <sstream>

#include "../print.hpp"

static void* get_module_base_address(std::string module_name) {
  std::ifstream file;
  file.open("/proc/self/maps");

  bool found = false;

  std::string line;
  std::string concat_line;
  while (std::getline(file, line)) {
    for (int i = 0; i < line.length(); i++) {
      concat_line = "";
      for (int h = 0; i+h < line.length() && h < module_name.length(); h++) {
	concat_line += line[i+h];
      }
      if (concat_line == module_name) { found = true; break; }
    }

    if (found == true) {break;}
  }

  concat_line = "";
  for (int i = 0; i <= line.length(); i++) {
    if (line[i] == '-') { break; }
    concat_line += line[i];
  }

  std::stringstream ss;
  ss << std::hex << "0x"+concat_line;

  unsigned long result;
  ss >> result;

  return (void*)result;
}

#endif
