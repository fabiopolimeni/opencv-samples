#include "imgui/imgui.h"

// Enumeration for ColorEdit3() / ColorEdit4() / ColorPicker3() / ColorPicker4()
enum ImGuiColorEditFlags_
{
    ImGuiColorEditFlags_Alpha           = 1 << 0,   // ColorEdit/ColorPicker: show/edit Alpha component. Must be 0x01 for compatibility with old API taking bool
    ImGuiColorEditFlags_RGB             = 1 << 1,   // ColorEdit: Choose one among RGB/HSV/HEX. User can still use the options menu to change. ColorPicker: Choose any combination or RGB/HSX/HEX.
    ImGuiColorEditFlags_HSV             = 1 << 2,
    ImGuiColorEditFlags_HEX             = 1 << 3,
    ImGuiColorEditFlags_NoPicker        = 1 << 4,   // ColorEdit: Disable picker when clicking on colored square
    ImGuiColorEditFlags_NoOptions       = 1 << 5,   // ColorEdit: Disable toggling options menu when right-clicking colored square
    ImGuiColorEditFlags_NoColorSquare   = 1 << 6,   // ColorEdit: Disable colored square
    ImGuiColorEditFlags_NoSliders       = 1 << 7,   // ColorEdit: Disable sliders, show only a button. ColorPicker: Disable all RGB/HSV/HEX sliders.
    ImGuiColorEditFlags_ModeMask_       = ImGuiColorEditFlags_RGB|ImGuiColorEditFlags_HSV|ImGuiColorEditFlags_HEX
};

typedef int ImGuiColorEditFlags; // color edit mode for ColorEdit*()     // enum ImGuiColorEditFlags_

namespace ImGui
{
    IMGUI_API bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags);  // click on colored squared to open a color picker, right-click for options. Hint: 'float col[3]' function argument is same as 'float* col'. You can pass address of first element out of a contiguous set, e.g. &myvector.x
    IMGUI_API bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags);  // 0x01 = ImGuiColorEditFlags_Alpha = very dodgily backward compatible with 'bool show_alpha=true'

    IMGUI_API bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    IMGUI_API bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0x01);

    IMGUI_API bool ColorSelector(const char* pLabel, ImVec4& oRGBA);
}