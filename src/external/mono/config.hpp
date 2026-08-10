#pragma once

// Override MONO_IMGUI_HEADER from the target build when ImGui uses another path.
#ifndef MONO_IMGUI_HEADER
#define MONO_IMGUI_HEADER "imgui/imgui.h"
#endif

#ifndef MONO_IMGUI_INTERNAL_HEADER
#define MONO_IMGUI_INTERNAL_HEADER "imgui/imgui_internal.h"
#endif

#include MONO_IMGUI_HEADER
#include MONO_IMGUI_INTERNAL_HEADER
