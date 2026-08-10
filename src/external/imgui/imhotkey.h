#pragma once
#include "imgui.h"

#include <string>

using ImHotkeyFlags = int;

enum ImHotkeyFlags_
{
    ImHotkeyFlags_None = 0,
    ImHotkeyFlags_NoModifiers = 1,
    ImHotkeyFlags_NoKeyboard = 1 << 1,
    ImHotkeyFlags_NoMouse = 1 << 2,

    ImHotkeyFlags_Default = ImHotkeyFlags_None
};

enum ImHotkeyModifiers_
{
    ImHotkeyModifier_Shift = 0x1,
    ImHotkeyModifier_Alt = 0x10,
    ImHotkeyModifier_Ctrl = 0x100,
};

namespace ImGui
{

    struct ImHotkeyData_t
    {
    public:
        ImHotkeyData_t(unsigned short t_scanCode, unsigned short t_vkCode,
                       unsigned short t_mouseButtons = 0, unsigned short t_modifiers = 0);

        unsigned short scanCode = 0;

        unsigned short vkCode = 0;

        unsigned short mouseButton = 0;

        unsigned short modifiers = 0;

        [[nodiscard]] const char* GetLabel();

        void Reset();

    private:

        std::string label_;

        int32_t id_;

        int32_t labelCacheSum_ = -1;

        static inline int32_t instanceCount = 0;
    };

    IMGUI_API bool ImHotkey(ImHotkeyData_t* v);

    IMGUI_API bool ImHotkey(ImHotkeyData_t* v, ImHotkeyFlags flags);

    IMGUI_API bool ImHotkey(ImHotkeyData_t* v, const ImVec2& size);

    IMGUI_API bool ImHotkey(ImHotkeyData_t* v, const ImVec2& size, ImHotkeyFlags flags);
}
