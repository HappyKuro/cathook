#include "steam_nographics.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C"
{
void* XCreateWindow(void*, unsigned long, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, void*, unsigned long, void*);
int XMapRaised(void*, unsigned long);
void* SDL_CreateWindow(const char*, int, int, std::uint64_t);
int SDL_GL_SwapWindow(void*);
void glClear(unsigned int);
}

namespace steam_nographics
{

#if defined(__i386__)

namespace
{

constexpr std::uint8_t pinned_steamui_build_id[] = {
  0xaa, 0xc6, 0x9f, 0x07, 0xc9, 0x75, 0x5b, 0x26, 0xc1, 0x00,
  0x16, 0x53, 0xe7, 0xd5, 0xf8, 0x49, 0x04, 0xac, 0xc0,
};

struct hook_spec
{
  const char* symbol;
  void* replacement;
};

const hook_spec steamui_hooks[] = {
  { "XCreateWindow", reinterpret_cast<void*>(XCreateWindow) },
  { "XMapRaised", reinterpret_cast<void*>(XMapRaised) },
  { "SDL_CreateWindow", reinterpret_cast<void*>(SDL_CreateWindow) },
  { "SDL_GL_SwapWindow", reinterpret_cast<void*>(SDL_GL_SwapWindow) },
  { "glClear", reinterpret_cast<void*>(glClear) },
};

const char* basename(const char* path)
{
  if (path == nullptr)
  {
    return "";
  }
  const char* const slash = std::strrchr(path, '/');
  return slash != nullptr ? slash + 1 : path;
}

std::uintptr_t dynamic_address(std::uintptr_t image_base, Elf32_Addr value)
{
  const auto pointer = static_cast<std::uintptr_t>(value);
  return pointer < image_base ? image_base + pointer : pointer;
}

bool is_pinned_steamui(std::uintptr_t image_base)
{
  const auto* const elf = reinterpret_cast<const Elf32_Ehdr*>(image_base);
  if (elf == nullptr || std::memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0 || elf->e_ident[EI_CLASS] != ELFCLASS32)
  {
    return false;
  }

  const auto* const program_headers = reinterpret_cast<const Elf32_Phdr*>(image_base + elf->e_phoff);
  for (std::uint16_t index = 0; index < elf->e_phnum; ++index)
  {
    const Elf32_Phdr& program_header = program_headers[index];
    if (program_header.p_type != PT_NOTE)
    {
      continue;
    }

    const auto* note = reinterpret_cast<const std::uint8_t*>(image_base + program_header.p_vaddr);
    const auto* const note_end = note + program_header.p_memsz;
    while (note + sizeof(Elf32_Nhdr) <= note_end)
    {
      const auto* const header = reinterpret_cast<const Elf32_Nhdr*>(note);
      note += sizeof(*header);
      const auto* const name = note;
      note += (header->n_namesz + 3U) & ~3U;
      const auto* const descriptor = note;
      note += (header->n_descsz + 3U) & ~3U;
      if (note > note_end)
      {
        break;
      }
      if (header->n_type == NT_GNU_BUILD_ID && header->n_namesz == 4 && std::memcmp(name, "GNU", 4) == 0
        && header->n_descsz == sizeof(pinned_steamui_build_id)
        && std::memcmp(descriptor, pinned_steamui_build_id, sizeof(pinned_steamui_build_id)) == 0)
      {
        return true;
      }
    }
  }
  return false;
}

int page_protection(const void* address)
{
  FILE* const maps = std::fopen("/proc/self/maps", "r");
  if (maps == nullptr)
  {
    return PROT_READ;
  }

  const std::uintptr_t target = reinterpret_cast<std::uintptr_t>(address);
  char line[512]{};
  while (std::fgets(line, sizeof(line), maps) != nullptr)
  {
    unsigned long begin = 0;
    unsigned long end = 0;
    char permissions[5]{};
    if (std::sscanf(line, "%lx-%lx %4s", &begin, &end, permissions) != 3
      || target < begin || target >= end)
    {
      continue;
    }

    std::fclose(maps);
    int result = 0;
    if (permissions[0] == 'r') result |= PROT_READ;
    if (permissions[1] == 'w') result |= PROT_WRITE;
    if (permissions[2] == 'x') result |= PROT_EXEC;
    return result;
  }

  std::fclose(maps);
  return PROT_READ;
}

bool write_got_slot(void** slot, void* replacement)
{
  const long page_size = sysconf(_SC_PAGESIZE);
  if (slot == nullptr || page_size <= 0)
  {
    return false;
  }

  const std::uintptr_t page = reinterpret_cast<std::uintptr_t>(slot)
    & ~static_cast<std::uintptr_t>(page_size - 1);
  const int original_protection = page_protection(slot);
  if (mprotect(reinterpret_cast<void*>(page), static_cast<size_t>(page_size), original_protection | PROT_WRITE) != 0)
  {
    return false;
  }

  *slot = replacement;
  __sync_synchronize();
  return mprotect(reinterpret_cast<void*>(page), static_cast<size_t>(page_size), original_protection) == 0;
}

const hook_spec* find_hook(const char* symbol)
{
  for (const hook_spec& hook : steamui_hooks)
  {
    if (std::strcmp(symbol, hook.symbol) == 0)
    {
      return &hook;
    }
  }
  return nullptr;
}

void install_steamui_got_hooks(void* module_handle)
{
  if (!is_enabled() || module_handle == nullptr)
  {
    return;
  }

  link_map* map = nullptr;
  if (dlinfo(module_handle, RTLD_DI_LINKMAP, &map) != 0 || map == nullptr || std::strcmp(basename(map->l_name), "steamui.so") != 0)
  {
    return;
  }

  const std::uintptr_t base = static_cast<std::uintptr_t>(map->l_addr);
  if (!is_pinned_steamui(base))
  {
    return;
  }

  const Elf32_Sym* symbols = nullptr;
  const char* strings = nullptr;
  const Elf32_Rel* relocations = nullptr;
  size_t relocation_bytes = 0;
  for (Elf32_Dyn* dynamic = map->l_ld; dynamic != nullptr && dynamic->d_tag != DT_NULL; ++dynamic)
  {
    switch (dynamic->d_tag)
    {
      case DT_SYMTAB: symbols = reinterpret_cast<const Elf32_Sym*>(dynamic_address(base, dynamic->d_un.d_ptr)); break;
      case DT_STRTAB: strings = reinterpret_cast<const char*>(dynamic_address(base, dynamic->d_un.d_ptr)); break;
      case DT_JMPREL: relocations = reinterpret_cast<const Elf32_Rel*>(dynamic_address(base, dynamic->d_un.d_ptr)); break;
      case DT_PLTRELSZ: relocation_bytes = dynamic->d_un.d_val; break;
      default: break;
    }
  }

  if (symbols == nullptr || strings == nullptr || relocations == nullptr || relocation_bytes == 0)
  {
    return;
  }

  unsigned int installed = 0;
  const size_t count = relocation_bytes / sizeof(Elf32_Rel);
  for (size_t index = 0; index < count; ++index)
  {
    const Elf32_Rel& relocation = relocations[index];
    const char* const name = strings + symbols[ELF32_R_SYM(relocation.r_info)].st_name;
    const hook_spec* const hook = find_hook(name);
    if (hook != nullptr)
    {
      auto** const slot = reinterpret_cast<void**>(base + relocation.r_offset);
      if (write_got_slot(slot, hook->replacement))
      {
        ++installed;
      }
    }
  }

  std::fprintf(stderr, "[steam-nographics] steamui build=aac6c9f07c9755b26c100165b3e7d5f84904acc0 GOT hooks=%u\n", installed);
  std::fflush(stderr);
}

}

void install_module_hooks(const char* library_path, void* module_handle)
{
  if (library_path != nullptr && std::strcmp(basename(library_path), "steamui.so") == 0)
  {
    install_steamui_got_hooks(module_handle);
  }
}

#else

void install_module_hooks(const char*, void*)
{
}

#endif

}
