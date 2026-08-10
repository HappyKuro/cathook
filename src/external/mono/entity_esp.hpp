#pragma once

#include "theme.hpp"

#include <unordered_map>
#include <string_view>

namespace mono
{
struct entity_box final
{
	ImVec2 position{};
	ImVec2 size{};
};

struct health_bar_style final
{
	float width{ 3.0f };
	float spacing{ 4.0f };
	int divisions{};
	float animation_speed{ 11.0f };
	color outline{ 0.0f, 0.0f, 0.0f, 0.8f };
};

class entity_esp_renderer final
{
public:
	void draw_box(const entity_box &box, color value, color outline, float alpha = 1.0f) const;
	float draw_health_bar(int entity_id, const entity_box &box, float health, float maximum_health, color value, const health_bar_style &style = {});
	void draw_progress_bar(const entity_box &box, float progress, color value, color outline, float height = 3.0f, float spacing = 4.0f, int divisions = 0, float alpha = 1.0f) const;
	void clear();

private:
	std::unordered_map<int, float> m_health_fractions{};
};

void progress_indicator(const char *id, ImVec2 position, ImVec2 bar_size, float progress, std::string_view label, std::string_view state, color state_color, color accent);
}
