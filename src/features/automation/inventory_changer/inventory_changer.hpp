#ifndef INVENTORY_CHANGER_HPP
#define INVENTORY_CHANGER_HPP

class Entity;

namespace inventory_changer
{

#if 0 // Inventory changer temporarily disabled.

enum class item_category {
  weapon,
  wearable,
  crate,
  key,
};

struct schema_item_option {
  std::uint16_t definition = 0;
  std::string label;
};

const std::vector<schema_item_option>& item_options(item_category category);
const std::vector<schema_item_option>& effect_options();
const std::vector<schema_item_option>& paintkit_options();

void set_client_module_address(const void* address);

void on_frame_stage(int stage);

std::uint32_t redirect_item_definition(std::uint32_t item_definition);

float attribute_hook_value_float_hook(float value, const char* attribute_name,
                                      Entity* entity, void* context, bool is_global);

#endif

}

#endif
