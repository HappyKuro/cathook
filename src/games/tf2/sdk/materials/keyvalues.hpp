/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/games/tf2/sdk/materials/keyvalues.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#ifndef KEYVALUES_HPP
#define KEYVALUES_HPP

#include <cstddef>
#include <new>
#include <string.h>

enum types_t {
  TYPE_NONE = 0,
  TYPE_STRING,
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_PTR,
  TYPE_WSTRING,
  TYPE_COLOR,
  TYPE_UINT64,
  TYPE_NUMTYPES,
};

class KeyValues;
class key_values_system_interface {
public:
  void* alloc_key_values_memory(const int size) {
    void** vtable = *(void***)this;
    auto alloc_fn = (void* (*)(void*, int))vtable[1];
    return alloc_fn(this, size);
  }

  void free_key_values_memory(void* memory) {
    void** vtable = *(void***)this;
    auto free_fn = (void (*)(void*, void*))vtable[2];
    free_fn(this, memory);
  }
};

static key_values_system_interface* (*key_values_system_original)();
static KeyValues* (*key_values_constructor_original)(void*, const char*);
static void* (*key_values_set_int_original)(void*, const char*, int);
static bool (*key_values_load_from_buffer_original)(void*, const char*, const char*, void*, const char*);
static void (*key_values_delete_this_original)(void*);

class KeyValues {
public:
  static void* operator new(const std::size_t size) {
    if (key_values_system_original != nullptr) {
      if (auto* system = key_values_system_original(); system != nullptr) {
        if (void* memory = system->alloc_key_values_memory(static_cast<int>(size)); memory != nullptr) {
          return memory;
        }
      }
    }
    throw std::bad_alloc{};
  }

  static void operator delete(void* memory) noexcept {
    if (memory == nullptr) return;
    if (key_values_system_original != nullptr) {
      if (auto* system = key_values_system_original(); system != nullptr) {
        system->free_key_values_memory(memory);
        return;
      }
    }
    ::operator delete(memory);
  }

  KeyValues(const char* name) {
    key_values_constructor_original(this, name);
  }

  KeyValues() {

  }

  void set_int(const char* key_name, int value) {
    (void)key_values_set_int_original(this, key_name, value);
  }

  bool load_from_buffer(const char* resource_name, const char* buffer) {
    return key_values_load_from_buffer_original(this, resource_name, buffer, nullptr, nullptr);
  }

  void delete_this() {
    if (key_values_delete_this_original != nullptr) {
      key_values_delete_this_original(this);
    } else {
      delete this;
    }
  }

private:
  int m_iKeyName;
  char* m_sValue;
  wchar_t* m_wsValue;

  union {
    int m_iValue;
    float m_flValue;
    void* m_pValue;
    unsigned char m_Color[4];
  };

  char m_iDataType;
  char m_bHasEscapeSequences;
  char m_bEvaluateConditionals;
  char unused[1];

  KeyValues* m_pPeer;
  KeyValues* m_pSub;
  KeyValues* m_pChain;
};

static_assert(sizeof(KeyValues) == 64, "KeyValues ABI layout changed");

#endif
