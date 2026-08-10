#include "../mono.hpp"

#include <cassert>

int main()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io{ ImGui::GetIO() };
	io.IniFilename = nullptr;
	io.DisplaySize = { 1280.0f, 720.0f };
	io.DeltaTime = 1.0f / 60.0f;
	io.Fonts->AddFontDefault();
	unsigned char *pixels{};
	int width{};
	int height{};
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	mono::apply_layout();

	ImGui::NewFrame();
	mono::apply_colors({ 0.36f, 0.60f, 1.0f, 1.0f });

	static mono::menu_state menu{ { 40.0f, 40.0f } };
	const bool visible{ mono::begin_menu(
		{ .id = "mono smoke", .title = "mono smoke" },
		menu,
		[] { mono::navbar_entry("General", true, nullptr); },
		[] {}) };
	if (visible) {
		static bool enabled{};
		static int mode{};
		static float amount{ 0.5f };
		static mono::rgba8 tint{ 90, 150, 255, 255 };
		static std::string name{ "mono" };
		mono::toggle("enabled", &enabled);
		mono::select_single("mode", &mode, { { "first", 0 }, { "second", 1 } });
		mono::slider_float("amount", &amount, 0.0f, 1.0f);
		mono::color_picker("tint", &tint);
		mono::input_string("name", &name);
	}
	mono::end_menu(visible);

	std::vector rows{
		mono::indicator_row{ "feature", "hold", "mouse 4", true },
		mono::indicator_row{ "camera", "toggle", "v", false }
	};
	mono::indicator_panel("mono indicator", "keybinds", rows, { 20.0f, 20.0f });
	mono::notifications notices{};
	notices.push("smoke notification", mono::notification_kind::info, 0.0f, 4.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
	notices.render(0.1f, io.DeltaTime);

	mono::bindings binds{};
	const uint32_t root{ binds.add("root") };
	const uint32_t child{ binds.add("child", root) };
	assert(binds.find(child) && binds.find(child)->parent_id == root);
	assert(!binds.reparent(root, child));

	ImGui::EndFrame();
	ImGui::Render();
	ImGui::DestroyContext();
	return 0;
}
