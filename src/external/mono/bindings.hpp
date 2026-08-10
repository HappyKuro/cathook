#pragma once

#include "widgets.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mono
{
using bind_value = std::variant<bool, int, float, std::string, rgba8>;

enum class bind_condition : uint8_t
{
	key,
	custom
};

enum class bind_key_mode : uint8_t
{
	hold,
	toggle,
	double_click
};

enum class bind_visibility : uint8_t
{
	always,
	while_active,
	hidden
};

struct binding final
{
	uint32_t id{};
	uint32_t parent_id{};
	std::string name{ "new bind" };
	bind_condition condition{ bind_condition::key };
	bind_key_mode key_mode{ bind_key_mode::hold };
	std::string condition_id{};
	int condition_value{};
	int key{};
	bool enabled{ true };
	bool inverted{};
	bool active{};
	bind_visibility visibility{ bind_visibility::always };
	double last_press_time{};
	std::unordered_map<std::string, bind_value> overrides{};
};

struct binding_environment final
{
	std::function<key_state(int)> key{};
	std::function<double()> time{};
	std::function<bool(const binding &)> custom_condition{};
	std::function<void(std::string_view, const bind_value &)> apply{};
	double double_click_window{ 0.25 };
};

class bindings final
{
public:
	uint32_t add(std::string name = "new bind", uint32_t parent_id = 0);
	bool remove(uint32_t id);
	bool reparent(uint32_t id, uint32_t parent_id);
	binding *find(uint32_t id);
	const binding *find(uint32_t id) const;
	void clear();

	void update(const binding_environment &environment);

	const std::vector<binding> &items() const { return m_items; }
	std::vector<binding> &items() { return m_items; }
	uint32_t next_id() const { return m_next_id; }

private:
	bool evaluate(binding &item, const binding_environment &environment);
	void apply_children(uint32_t parent_id, const binding_environment &environment);

	std::vector<binding> m_items{};
	uint32_t m_next_id{ 1 };
};
}
