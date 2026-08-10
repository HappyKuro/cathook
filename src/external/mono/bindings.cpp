#include "bindings.hpp"

#include <algorithm>
#include <ranges>

namespace mono
{
uint32_t bindings::add(std::string name, const uint32_t parent_id)
{
	binding item{};
	item.id = m_next_id++;
	item.parent_id = find(parent_id) ? parent_id : 0;
	item.name = std::move(name);
	m_items.push_back(std::move(item));
	return m_items.back().id;
}

bool bindings::remove(const uint32_t id)
{
	if (!find(id)) {
		return false;
	}
	std::vector<uint32_t> removed{ id };
	for (size_t index{}; index < removed.size(); ++index) {
		for (const binding &item : m_items) {
			if (item.parent_id == removed[index]) {
				removed.push_back(item.id);
			}
		}
	}
	std::erase_if(m_items, [&removed](const binding &item) {
		return std::ranges::find(removed, item.id) != removed.end();
	});
	return true;
}

bool bindings::reparent(const uint32_t id, const uint32_t parent_id)
{
	binding *const item{ find(id) };
	if (!item || id == parent_id || (parent_id && !find(parent_id))) {
		return false;
	}
	for (uint32_t cursor{ parent_id }; cursor;) {
		if (cursor == id) {
			return false;
		}
		const binding *const parent{ find(cursor) };
		cursor = parent ? parent->parent_id : 0;
	}
	item->parent_id = parent_id;
	return true;
}

binding *bindings::find(const uint32_t id)
{
	const auto iterator{ std::ranges::find(m_items, id, &binding::id) };
	return iterator == m_items.end() ? nullptr : &*iterator;
}

const binding *bindings::find(const uint32_t id) const
{
	const auto iterator{ std::ranges::find(m_items, id, &binding::id) };
	return iterator == m_items.end() ? nullptr : &*iterator;
}

void bindings::clear()
{
	m_items.clear();
	m_next_id = 1;
}

bool bindings::evaluate(binding &item, const binding_environment &environment)
{
	bool active{};
	if (item.condition == bind_condition::key) {
		const key_state state{ environment.key ? environment.key(item.key) : key_state{} };
		switch (item.key_mode) {
		case bind_key_mode::hold:
			active = state.held;
			break;
		case bind_key_mode::toggle:
			if (state.pressed) item.active = !item.active;
			return item.inverted ? !item.active : item.active;
		case bind_key_mode::double_click:
			if (state.pressed && environment.time) {
				const double now{ environment.time() };
				if (now - item.last_press_time <= environment.double_click_window) {
					item.active = !item.active;
					item.last_press_time = 0.0;
				}
				else {
					item.last_press_time = now;
				}
			}
			return item.inverted ? !item.active : item.active;
		}
	}
	else if (environment.custom_condition) {
		active = environment.custom_condition(item);
	}
	return item.inverted ? !active : active;
}

void bindings::apply_children(const uint32_t parent_id, const binding_environment &environment)
{
	for (binding &item : m_items) {
		if (item.parent_id != parent_id) continue;
		item.active = item.enabled && evaluate(item, environment);
		if (!item.active) continue;
		if (environment.apply) {
			for (const auto &[name, value] : item.overrides) {
				environment.apply(name, value);
			}
		}
		apply_children(item.id, environment);
	}
}

void bindings::update(const binding_environment &environment)
{
	for (binding &item : m_items) {
		item.active = false;
	}
	apply_children(0, environment);
}
}
