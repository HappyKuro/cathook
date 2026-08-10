#include "entity_esp.hpp"

#include <algorithm>
#include <cmath>

namespace mono
{
namespace
{
ImU32 to_u32(const color value, const float alpha)
{
	return ImGui::ColorConvertFloat4ToU32({ value.r, value.g, value.b, std::clamp(value.a * alpha, 0.0f, 1.0f) });
}

color brighten(const color value, const float amount)
{
	return { std::lerp(value.r, 1.0f, amount), std::lerp(value.g, 1.0f, amount), std::lerp(value.b, 1.0f, amount), value.a };
}
}

void entity_esp_renderer::draw_box(const entity_box &box, const color value, const color outline, const float alpha) const
{
	ImDrawList *const draw_list{ ImGui::GetWindowDrawList() };
	const ImVec2 minimum{ std::round(box.position.x), std::round(box.position.y) };
	const ImVec2 maximum{ std::round(box.position.x + box.size.x), std::round(box.position.y + box.size.y) };
	draw_list->AddRect(minimum, maximum, to_u32(outline, alpha), 0.0f, 0, 3.0f);
	draw_list->AddRect(minimum, maximum, to_u32(value, alpha), 0.0f, 0, 1.0f);
}

float entity_esp_renderer::draw_health_bar(const int entity_id, const entity_box &box, const float health, const float maximum_health, const color value, const health_bar_style &style)
{
	if (maximum_health <= 0.0f) {
		return 0.0f;
	}

	const float target{ std::clamp(health / maximum_health, 0.0f, 1.0f) };
	float &shown{ m_health_fractions.try_emplace(entity_id, target).first->second };
	const float amount{ 1.0f - std::exp(-style.animation_speed * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f)) };
	shown = std::lerp(shown, target, amount);
	if (std::abs(shown - target) < 0.001f) {
		shown = target;
	}

	const float height{ std::max(std::round(box.size.y), 1.0f) };
	const float x{ std::round(box.position.x - style.spacing - style.width - 2.0f) };
	const float y{ std::round(box.position.y) };
	const float fill_y{ std::round(y + height * (1.0f - shown)) };
	ImDrawList *const draw_list{ ImGui::GetWindowDrawList() };
	draw_list->AddRectFilled({ x - 1.0f, y - 1.0f }, { x + style.width + 1.0f, y + height + 1.0f }, to_u32(style.outline, 1.0f), 1.5f);
	draw_list->AddRectFilledMultiColor({ x, fill_y }, { x + style.width, y + height }, to_u32(brighten(value, 0.22f), 1.0f), to_u32(brighten(value, 0.22f), 1.0f), to_u32(value, 1.0f), to_u32(value, 1.0f));
	if (height * shown > 0.5f) {
		draw_list->AddLine({ x, fill_y }, { x + style.width, fill_y }, to_u32(brighten(value, 0.42f), 1.0f));
	}
	for (int index{ 1 }; index < style.divisions; ++index) {
		const float division_y{ y + height * (static_cast<float>(index) / static_cast<float>(style.divisions)) };
		draw_list->AddLine({ x, division_y }, { x + style.width, division_y }, to_u32(style.outline, 1.0f));
	}
	return fill_y;
}

void entity_esp_renderer::draw_progress_bar(const entity_box &box, const float progress, const color value, const color outline, const float height, const float spacing, const int divisions, const float alpha) const
{
	const float width{ box.size.x };
	const float y{ box.position.y + box.size.y + height + spacing };
	const float filled{ std::clamp(progress, 0.0f, 1.0f) * width };
	ImDrawList *const draw_list{ ImGui::GetWindowDrawList() };
	draw_list->AddRectFilled({ box.position.x - 1.0f, y - 1.0f }, { box.position.x + width + 1.0f, y + height + 1.0f }, to_u32(outline, alpha), 1.0f);
	draw_list->AddRectFilled({ box.position.x, y }, { box.position.x + filled, y + height }, to_u32(value, alpha), 1.0f);
	for (int index{ 1 }; index < divisions; ++index) {
		const float division_x{ box.position.x + width * (static_cast<float>(index) / static_cast<float>(divisions)) };
		draw_list->AddLine({ division_x, y }, { division_x, y + height }, to_u32(outline, alpha));
	}
}

void progress_indicator(const char *const id, const ImVec2 position, const ImVec2 bar_size, const float progress, const std::string_view label, const std::string_view state, const color state_color, const color accent)
{
	// Background draw lists are cached per viewport.  When SDL recreates the
	// renderer/context that cache can briefly retain an invalid draw-list
	// pointer; progress indicators used it directly and crashed in
	// ImDrawList::AddRectFilled.  A regular window draw list is owned and
	// rebuilt by the active ImGui frame, so use a transparent, fixed-position
	// window for this overlay instead.
	if (ImGui::GetCurrentContext() == nullptr) {
		return;
	}

	const float label_height{ ImGui::GetFontSize() + 3.0f };
	const ImVec2 window_size{ bar_size.x, label_height + bar_size.y };
	ImGui::SetNextWindowPos(position, ImGuiCond_Always);
	ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;
	ImGui::Begin(id, nullptr, flags);

	ImDrawList *const draw_list{ ImGui::GetWindowDrawList() };
	const ImVec2 window_position{ ImGui::GetWindowPos() };
	const ImVec2 bar_start{ window_position.x, window_position.y + label_height };
	const ImVec2 bar_end{ bar_start.x + bar_size.x, bar_start.y + bar_size.y };
	const float filled{ std::clamp(progress, 0.0f, 1.0f) * bar_size.x };
	const ImU32 accent_u32{ to_u32(accent, 1.0f) };
	draw_list->AddRectFilled(bar_start, bar_end, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
	if (filled > 0.0f) {
		const ImDrawFlags corners{ filled >= bar_size.x ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft };
		draw_list->AddRectFilled(bar_start, { bar_start.x + filled, bar_end.y }, accent_u32, 4.0f, corners);
	}
	draw_list->AddRect(bar_start, bar_end, accent_u32, 4.0f);
	draw_list->AddText(window_position, ImGui::GetColorU32(ImGuiCol_Text), label.data(), label.data() + label.size());
	const ImVec2 state_size{ ImGui::CalcTextSize(state.data(), state.data() + state.size()) };
	draw_list->AddText({ window_position.x + bar_size.x - state_size.x, window_position.y }, to_u32(state_color, 1.0f), state.data(), state.data() + state.size());
	ImGui::End();
	ImGui::PopStyleVar();
}

void entity_esp_renderer::clear()
{
	m_health_fractions.clear();
}
}
