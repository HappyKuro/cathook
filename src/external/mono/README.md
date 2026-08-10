# mono

`mono` is the self-contained ImGui UI framework used by SEO64. It owns the
visual language and reusable behavior; it contains no TF2, SDK, entity,
configuration-registry, DirectX, or Win32 code.

## Included

- backend-neutral ImGui context and frame runtime
- layout and accent-driven theme
- menu window, header, draggable shell, navbar entries, and body layout
- toggle, single/multi select, integer/float slider, color, string, multiline,
  key-capture, spacing, separator, and disabled-scope widgets
- hierarchical hold/toggle/double-click keybind model with custom conditions
  and typed setting overrides
- draggable multi-column indicator panels
- animated info/warning notifications

SEO64's `app/features/ui` and `app/features/notifs` directories are host
adapters and concrete menu content. They translate `Color`, `ConfigVar`, TF2
conditions, input, persistence, fonts, and DX9 calls into mono's small public
types.

## Copying mono

Copy this directory into the target project and add these sources:

```text
bindings.cpp
menu_shell.cpp
overlays.cpp
runtime.cpp
theme.cpp
widgets.cpp
```

Then include `mono.hpp`. CMake consumers can use:

```cmake
set(MONO_IMGUI_INCLUDE_DIR "/path/to/imgui")
# Optional when the public header is not named imgui.h:
set(MONO_IMGUI_HEADER "imgui.hpp")
add_subdirectory(path/to/mono)
target_link_libraries(your_target PRIVATE mono)
```

SEO64 calls its ImGui header `imgui.hpp`. A normal upstream checkout usually
calls it `imgui.h`; CMake selects that automatically. For another layout,
define `MONO_IMGUI_HEADER` to a quoted include path or change only
`config.hpp`.

## Host contract

The renderer/platform integration is four callbacks:

```cpp
mono::runtime ui;
ui.initialize({
	.initialize = [] { return platform_init() && renderer_init(); },
	.shutdown = [] { renderer_shutdown(); platform_shutdown(); },
	.new_frame = [] { renderer_new_frame(); platform_new_frame(); },
	.render = [](ImDrawData *data) { renderer_render(data); }
}, [](ImGuiIO &io) {
	return io.Fonts->AddFontDefault() != nullptr;
});
```

At frame time:

```cpp
ui.begin_frame({ 0.36f, 0.60f, 1.0f, 1.0f });


ui.end_frame();
```

Key capture is renderer-independent. Supply the target's key state and display
name once:

```cpp
mono::set_input_adapter({
	.state = [](int key) { return read_key(key); },
	.name = [](int key) { return key_name(key); }
});
```

## Menu and overlays

`begin_menu`/`end_menu` is convenient for small menus. For very large host
menus, keep `ImGui::Begin` and `ImGui::End` in one lexical scope and use mono's
theme, navbar, header, and child primitives individually. This makes ImGui
stack ownership obvious.

```cpp
static mono::menu_state state{ { 40.0f, 40.0f } };
const bool visible = mono::begin_menu(
	{ .id = "example", .title = "example menu" },
	state,
	[] { mono::navbar_entry("General", true, nullptr); },
	[] {});
if (visible) {
	static bool enabled{};
	mono::toggle("enabled", &enabled);
}
mono::end_menu(visible);
```

Indicators consume plain rows, so they do not know about the source of the
data:

```cpp
std::vector rows{
	mono::indicator_row{ "aimbot", "hold", "mouse 4", true },
	mono::indicator_row{ "third person", "toggle", "v", false }
};
position = mono::indicator_panel("binds", "keybinds", rows, position);
```

Notifications own their animation queue:

```cpp
mono::notifications notices;
notices.push("configuration saved", mono::notification_kind::info,
	now, 4.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
notices.render(now, delta_time);
```

The target project remains responsible for serializing its settings and
bindings. This avoids forcing a JSON library or storage format onto mono.
