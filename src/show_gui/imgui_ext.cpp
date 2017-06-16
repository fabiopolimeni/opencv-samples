#include "imgui_ext.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_PLACEMENT_NEW
#include <ocornut-imgui/imgui_internal.h>

#define IM_F32_TO_INT8_UNBOUND(_VAL)    ((int)((_VAL) * 255.0f + ((_VAL)>=0 ? 0.5f : -0.5f)))   // Unsaturated, for display purpose 
#define IM_F32_TO_INT8_SAT(_VAL)        ((int)(ImSaturate(_VAL) * 255.0f + 0.5f))               // Saturated, always output 0..255

static inline bool      ImIsPowerOfTwo(int v)           { return v != 0 && (v & (v - 1)) == 0; }

#define ENABLE_CUSTOM_WIDGETS 1
#if ENABLE_CUSTOM_WIDGETS

bool ImGui::ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    float col4[4] = { col[0], col[1], col[2], 1.0f };
    if (!ColorEdit4(label, col4, flags & ~ImGuiColorEditFlags_Alpha))
        return false;
    col[0] = col4[0]; col[1] = col4[1]; col[2] = col4[2];
    return true;
}

// Edit colors components (each component in 0.0f..1.0f range)
// Click on colored square to open a color picker (unless ImGuiColorEditFlags_NoPicker is set). Use CTRL-Click to input value and TAB to go to next item.
bool ImGui::ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w_full = CalcItemWidth();
    const float square_sz_with_spacing = (flags & ImGuiColorEditFlags_NoColorSquare) ? 0.0f : (g.FontSize + style.FramePadding.y * 2.0f + style.ItemInnerSpacing.x);

    // If no mode is specified, defaults to RGB
    if (!(flags & ImGuiColorEditFlags_ModeMask_))
        flags |= ImGuiColorEditFlags_RGB;

    // If we're not showing any slider there's no point in querying color mode, nor showing the options menu, nor doing any HSV conversions
    if (flags & ImGuiColorEditFlags_NoSliders)
        flags = (flags & (~ImGuiColorEditFlags_ModeMask_)) | ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_NoOptions;

    // Read back edit mode from persistent storage
    if (!(flags & ImGuiColorEditFlags_NoOptions))
        flags = (flags & (~ImGuiColorEditFlags_ModeMask_)) | (g.ColorEditModeStorage.GetInt(id, (flags & ImGuiColorEditFlags_ModeMask_)) & ImGuiColorEditFlags_ModeMask_);

    // Check that exactly one of RGB/HSV/HEX is set
    IM_ASSERT(ImIsPowerOfTwo((int)(flags & ImGuiColorEditFlags_ModeMask_))); // 

    float f[4] = { col[0], col[1], col[2], col[3] };
    if (flags & ImGuiColorEditFlags_HSV)
        ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);

    int i[4] = { IM_F32_TO_INT8_UNBOUND(f[0]), IM_F32_TO_INT8_UNBOUND(f[1]), IM_F32_TO_INT8_UNBOUND(f[2]), IM_F32_TO_INT8_UNBOUND(f[3]) };

    bool alpha = (flags & ImGuiColorEditFlags_Alpha) != 0;
    bool value_changed = false;
    int components = alpha ? 4 : 3;

    BeginGroup();
    PushID(label);

    if ((flags & (ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_HSV)) != 0 && (flags & ImGuiColorEditFlags_NoSliders) == 0)
    {
        // RGB/HSV 0..255 Sliders
        const float w_items_all = w_full - square_sz_with_spacing;
        const float w_item_one  = ImMax(1.0f, (float)(int)((w_items_all - (style.ItemInnerSpacing.x) * (components-1)) / (float)components));
        const float w_item_last = ImMax(1.0f, (float)(int)(w_items_all - (w_item_one + style.ItemInnerSpacing.x) * (components-1)));

        const bool hide_prefix = (w_item_one <= CalcTextSize("M:999").x);
        const char* ids[4] = { "##X", "##Y", "##Z", "##W" };
        const char* fmt_table[3][4] =
        {
            {   "%3.0f",   "%3.0f",   "%3.0f",   "%3.0f" }, // Short display
            { "R:%3.0f", "G:%3.0f", "B:%3.0f", "A:%3.0f" }, // Long display for RGBA
            { "H:%3.0f", "S:%3.0f", "V:%3.0f", "A:%3.0f" }  // Long display for HSVV
        };
        const char** fmt = hide_prefix ? fmt_table[0] : (flags & ImGuiColorEditFlags_HSV) ? fmt_table[2] : fmt_table[1];

        PushItemWidth(w_item_one);
        for (int n = 0; n < components; n++)
        {
            if (n > 0)
                SameLine(0, style.ItemInnerSpacing.x);
            if (n + 1 == components)
                PushItemWidth(w_item_last);
            value_changed |= DragInt(ids[n], &i[n], 1.0f, 0, 255, fmt[n]);
        }
        PopItemWidth();
        PopItemWidth();
    }
    else if ((flags & ImGuiColorEditFlags_HEX) != 0 && (flags & ImGuiColorEditFlags_NoSliders) == 0)
    {
        // RGB Hexadecimal Input
        const float w_slider_all = w_full - square_sz_with_spacing;
        char buf[64];
        if (alpha)
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X%02X", i[0], i[1], i[2], i[3]);
        else
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", i[0], i[1], i[2]);
        ImGui::PushItemWidth(w_slider_all);
        if (ImGui::InputText("##Text", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
        {
            value_changed |= true;
            char* p = buf;
            while (*p == '#' || ImCharIsSpace(*p))
                p++;
            i[0] = i[1] = i[2] = i[3] = 0;
            if (alpha)
                sscanf(p, "%02X%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2], (unsigned int*)&i[3]); // Treat at unsigned (%X is unsigned)
            else
                sscanf(p, "%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2]);
        }
        PopItemWidth();
    }

    const char* label_display_end = FindRenderedTextEnd(label);

    bool picker_active = false;
    if (!(flags & ImGuiColorEditFlags_NoColorSquare))
    {
        if (!(flags & ImGuiColorEditFlags_NoSliders))
            SameLine(0, style.ItemInnerSpacing.x);

        const ImVec4 col_display(col[0], col[1], col[2], 1.0f);
        if (ColorButton(col_display))
        {
            if (!(flags & ImGuiColorEditFlags_NoPicker))
            {
                OpenPopup("picker");
                SetNextWindowPos(window->DC.LastItemRect.GetBL() + ImVec2(-1,style.ItemSpacing.y));
            }
        }
        else if (!(flags & ImGuiColorEditFlags_NoOptions) && IsItemHovered() && IsMouseClicked(1))
        {
            OpenPopup("context");
        }

        if (BeginPopup("picker"))
        {
            picker_active = true;
            if (label != label_display_end)
                TextUnformatted(label, label_display_end);
            PushItemWidth(256.0f + (alpha ? 2 : 1) * (style.ItemInnerSpacing.x));
            value_changed |= ColorPicker4("##picker", col, (flags & ImGuiColorEditFlags_Alpha) | (ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_HSV | ImGuiColorEditFlags_HEX));
            PopItemWidth();
            EndPopup();
        }
        if (!(flags & ImGuiColorEditFlags_NoOptions) && BeginPopup("context"))
        {
            // FIXME-LOCALIZATION
            if (MenuItem("Edit as RGB", NULL, (flags & ImGuiColorEditFlags_RGB)?1:0)) g.ColorEditModeStorage.SetInt(id, (int)(ImGuiColorEditFlags_RGB));
            if (MenuItem("Edit as HSV", NULL, (flags & ImGuiColorEditFlags_HSV)?1:0)) g.ColorEditModeStorage.SetInt(id, (int)(ImGuiColorEditFlags_HSV));
            if (MenuItem("Edit as Hexadecimal", NULL, (flags & ImGuiColorEditFlags_HEX)?1:0)) g.ColorEditModeStorage.SetInt(id, (int)(ImGuiColorEditFlags_HEX));
            EndPopup();
        }

        // Recreate our own tooltip over's ColorButton() one because we want to display correct alpha here
        if (IsItemHovered())
            SetTooltip("Color:\n(%.2f,%.2f,%.2f,%.2f)\n#%02X%02X%02X%02X", col[0], col[1], col[2], col[3], IM_F32_TO_INT8_SAT(col[0]), IM_F32_TO_INT8_SAT(col[1]), IM_F32_TO_INT8_SAT(col[2]), IM_F32_TO_INT8_SAT(col[3]));
    }

    if (label != label_display_end)
    {
        SameLine(0, style.ItemInnerSpacing.x);
        TextUnformatted(label, label_display_end);
    }

    // Convert back
    if (!picker_active)
    {
        for (int n = 0; n < 4; n++)
            f[n] = i[n] / 255.0f;
        if (flags & ImGuiColorEditFlags_HSV)
            ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
        if (value_changed)
        {
            col[0] = f[0];
            col[1] = f[1];
            col[2] = f[2];
            if (alpha)
                col[3] = f[3];
        }
    }

    PopID();
    EndGroup();

    return value_changed;
}

bool ImGui::ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    float col4[4] = { col[0], col[1], col[2], 1.0f };
    if (!ColorPicker4(label, col4, flags & ~ImGuiColorEditFlags_Alpha))
        return false;
    col[0] = col4[0]; col[1] = col4[1]; col[2] = col4[2];
    return true;
}

// ColorPicker v2.50 WIP 
// see https://github.com/ocornut/imgui/issues/346
// TODO: Missing color square
// TODO: English strings in context menu (see FIXME-LOCALIZATION)
bool ImGui::ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushID(label);
    ImGui::BeginGroup();

    // Setup
    bool alpha = (flags & ImGuiColorEditFlags_Alpha) != 0;
    ImVec2 picker_pos = ImGui::GetCursorScreenPos();
    float bars_width = ImGui::GetWindowFontSize() * 0.5f;                                                           // Arbitrary smallish width of Hue/Alpha picking bars
    float sv_picker_size = ImMax(bars_width * 2, ImGui::CalcItemWidth() - (alpha ? 2 : 1) * (bars_width + style.ItemInnerSpacing.x)); // Saturation/Value picking box
    float bar0_pos_x = picker_pos.x + sv_picker_size + style.ItemInnerSpacing.x;
    float bar1_pos_x = bar0_pos_x + bars_width + style.ItemInnerSpacing.x;

    // Recreate our own tooltip over's ColorButton() one because we want to display correct alpha here
    if (IsItemHovered())
        SetTooltip("Color:\n(%.2f,%.2f,%.2f,%.2f)\n#%02X%02X%02X%02X", col[0], col[1], col[2], col[3], IM_F32_TO_INT8_SAT(col[0]), IM_F32_TO_INT8_SAT(col[1]), IM_F32_TO_INT8_SAT(col[2]), IM_F32_TO_INT8_SAT(col[3]));

    float H,S,V;
    ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], H, S, V);

    // Color matrix logic
    bool value_changed = false, hsv_changed = false;
    ImGui::InvisibleButton("sv", ImVec2(sv_picker_size, sv_picker_size));
    if (ImGui::IsItemActive())
    {
        S = ImSaturate((io.MousePos.x - picker_pos.x) / (sv_picker_size-1));
        V = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size-1));
        value_changed = hsv_changed = true;
    }

    // Hue bar logic
    SetCursorScreenPos(ImVec2(bar0_pos_x, picker_pos.y));
    InvisibleButton("hue", ImVec2(bars_width, sv_picker_size));
    if (IsItemActive())
    {
        H = ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size-1));
        value_changed = hsv_changed = true;
    }

    // Alpha bar logic
    if (alpha)
    {
        SetCursorScreenPos(ImVec2(bar1_pos_x, picker_pos.y));
        InvisibleButton("alpha", ImVec2(bars_width, sv_picker_size));
        if (IsItemActive())
        {
            col[3] = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size-1));
            value_changed = true;
        }
    }

    const char* label_display_end = FindRenderedTextEnd(label);
    if (label != label_display_end)
    {
        SameLine(0, style.ItemInnerSpacing.x);
        TextUnformatted(label, label_display_end);
    }

    // Convert back color to RGB
    if (hsv_changed)
        ColorConvertHSVtoRGB(H >= 1.0f ? H - 10 * 1e-6f : H, S > 0.0f ? S : 10*1e-6f, V > 0.0f ? V : 1e-6f, col[0], col[1], col[2]);

    // R,G,B and H,S,V slider color editor
    if (!(flags & ImGuiColorEditFlags_NoSliders))
    {
        if ((flags & ImGuiColorEditFlags_ModeMask_) == 0)
            flags = ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_HSV | ImGuiColorEditFlags_HEX;
        ImGui::PushItemWidth((alpha ? bar1_pos_x : bar0_pos_x) + bars_width - picker_pos.x);
        ImGuiColorEditFlags sub_flags = (alpha ? ImGuiColorEditFlags_Alpha : 0) | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoColorSquare;
        if (flags & ImGuiColorEditFlags_RGB)
            value_changed |= ImGui::ColorEdit4("##rgb", col, sub_flags | ImGuiColorEditFlags_RGB);
        if (flags & ImGuiColorEditFlags_HSV)
            value_changed |= ImGui::ColorEdit4("##hsv", col, sub_flags | ImGuiColorEditFlags_HSV);
        if (flags & ImGuiColorEditFlags_HEX)
            value_changed |= ImGui::ColorEdit4("##hex", col, sub_flags | ImGuiColorEditFlags_HEX);
        ImGui::PopItemWidth();
    }

    // Try to cancel hue wrap (after ColorEdit), if any
    if (value_changed)
    {
        float new_H, new_S, new_V;
        ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], new_H, new_S, new_V);
        if (new_H <= 0 && H > 0) 
        {
            if (new_V <= 0 && V != new_V)
                ImGui::ColorConvertHSVtoRGB(H, S, new_V <= 0 ? V * 0.5f : new_V, col[0], col[1], col[2]);
            else if (new_S <= 0)
                ImGui::ColorConvertHSVtoRGB(H, new_S <= 0 ? S * 0.5f : new_S, new_V, col[0], col[1], col[2]);
        }
    }

    // Render hue bar
    ImVec4 hue_color_f(1, 1, 1, 1);
    ColorConvertHSVtoRGB(H, 1, 1, hue_color_f.x, hue_color_f.y, hue_color_f.z);
    ImU32 hue_colors[] = { IM_COL32(255,0,0,255), IM_COL32(255,255,0,255), IM_COL32(0,255,0,255), IM_COL32(0,255,255,255), IM_COL32(0,0,255,255), IM_COL32(255,0,255,255), IM_COL32(255,0,0,255) };
    for (int i = 0; i < 6; ++i)
    {
        draw_list->AddRectFilledMultiColor(
            ImVec2(bar0_pos_x, picker_pos.y + i * (sv_picker_size / 6)),
            ImVec2(bar0_pos_x + bars_width, picker_pos.y + (i + 1) * (sv_picker_size / 6)),
            hue_colors[i], hue_colors[i], hue_colors[i + 1], hue_colors[i + 1]);
    }
    float bar0_line_y = (float)(int)(picker_pos.y + H * sv_picker_size + 0.5f);
    draw_list->AddLine(ImVec2(bar0_pos_x - 1, bar0_line_y), ImVec2(bar0_pos_x + bars_width + 1, bar0_line_y), IM_COL32_WHITE);

    // Render alpha bar
    if (alpha)
    {
        float alpha = ImSaturate(col[3]);
        float bar1_line_y = (float)(int)(picker_pos.y + (1.0f-alpha) * sv_picker_size + 0.5f);
        draw_list->AddRectFilledMultiColor(ImVec2(bar1_pos_x, picker_pos.y), ImVec2(bar1_pos_x + bars_width, picker_pos.y + sv_picker_size), IM_COL32_WHITE, IM_COL32_WHITE, IM_COL32_BLACK, IM_COL32_BLACK);
        draw_list->AddLine(ImVec2(bar1_pos_x - 1, bar1_line_y), ImVec2(bar1_pos_x + bars_width + 1, bar1_line_y), IM_COL32_WHITE);
    }

    // Render color matrix
    ImU32 hue_color32 = ColorConvertFloat4ToU32(hue_color_f);
    draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size,sv_picker_size), IM_COL32_WHITE, hue_color32, hue_color32, IM_COL32_WHITE);
    draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size,sv_picker_size), IM_COL32_BLACK_TRANS, IM_COL32_BLACK_TRANS, IM_COL32_BLACK, IM_COL32_BLACK);

    // Render cross-hair
    const float CROSSHAIR_SIZE = 7.0f;
    ImVec2 p((float)(int)(picker_pos.x + S * sv_picker_size + 0.5f), (float)(int)(picker_pos.y + (1 - V) * sv_picker_size + 0.5f));
    draw_list->AddLine(ImVec2(p.x - CROSSHAIR_SIZE, p.y), ImVec2(p.x - 2, p.y), IM_COL32_WHITE);
    draw_list->AddLine(ImVec2(p.x + CROSSHAIR_SIZE, p.y), ImVec2(p.x + 2, p.y), IM_COL32_WHITE);
    draw_list->AddLine(ImVec2(p.x, p.y + CROSSHAIR_SIZE), ImVec2(p.x, p.y + 2), IM_COL32_WHITE);
    draw_list->AddLine(ImVec2(p.x, p.y - CROSSHAIR_SIZE), ImVec2(p.x, p.y - 2), IM_COL32_WHITE);

    EndGroup();
    PopID();

    return value_changed;
}

bool ImGui::ColorSelector(const char* pLabel, ImVec4& oRGBA)
{
	const ImU32 c_oColorGrey = ImGui::ColorConvertFloat4ToU32(ImVec4(0.75f,0.75f,0.75f,1.f));
	const ImU32 c_oColorBlack = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f,0.f,0.f,1.f));
	const ImU32 c_oColorBlackTransparent = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f,0.f,0.f,0.f));
	const ImU32 c_oColorWhite = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f,1.f,1.f,1.f));

	ImGui::PushID(pLabel);
	bool bRet = false;
	ImGuiID iID = ImGui::GetID(pLabel);
	ImGuiWindow* pWindow = ImGui::GetCurrentWindow();

	const ImGuiID iStorageOpen = iID + ImGui::GetID("ColorSelector_Open");

	const ImGuiID iStorageStartColorR = iID + ImGui::GetID("ColorSelector_StartColor_R");
	const ImGuiID iStorageStartColorG = iID + ImGui::GetID("ColorSelector_StartColor_G");
	const ImGuiID iStorageStartColorB = iID + ImGui::GetID("ColorSelector_StartColor_B");
	const ImGuiID iStorageStartColorA = iID + ImGui::GetID("ColorSelector_StartColor_A");

	const ImGuiID iStorageCurrentColorH = iID + ImGui::GetID("ColorSelector_CurrentColor_H");
	const ImGuiID iStorageCurrentColorS = iID + ImGui::GetID("ColorSelector_CurrentColor_S");
	const ImGuiID iStorageCurrentColorV = iID + ImGui::GetID("ColorSelector_CurrentColor_V");
	const ImGuiID iStorageCurrentColorA = iID + ImGui::GetID("ColorSelector_CurrentColor_A");

	
	//pWindow->StateStorage.SetFloat(iID);

	ImDrawList* pDrawList = ImGui::GetWindowDrawList();

	if (ImGui::InvisibleButton("Picker", ImVec2(16,16)))
	{
		pWindow->StateStorage.SetInt(iStorageOpen, 1);
		
		pWindow->StateStorage.SetFloat(iStorageStartColorR, oRGBA.x);
		pWindow->StateStorage.SetFloat(iStorageStartColorG, oRGBA.y);
		pWindow->StateStorage.SetFloat(iStorageStartColorB, oRGBA.z);
		pWindow->StateStorage.SetFloat(iStorageStartColorA, oRGBA.w);

		float fHue, fSat, fVal;
		ImGui::ColorConvertRGBtoHSV( oRGBA.x, oRGBA.y, oRGBA.z, fHue, fSat, fVal );

		pWindow->StateStorage.SetFloat(iStorageCurrentColorH, fHue);
		pWindow->StateStorage.SetFloat(iStorageCurrentColorS, fSat);
		pWindow->StateStorage.SetFloat(iStorageCurrentColorV, fVal);
		pWindow->StateStorage.SetFloat(iStorageCurrentColorA, oRGBA.w);
	}

	for (int iX = 0; iX < 2; ++iX)
	{
		for (int iY = 0; iY < 2; ++iY)
		{
			ImVec2 oA(ImGui::GetItemRectMin().x + iX * 8.f, ImGui::GetItemRectMin().y + iY * 8.f);
			ImVec2 oB(ImGui::GetItemRectMin().x + (1+iX) * 8.f, ImGui::GetItemRectMin().y + (1+iY) * 8.f);
			pDrawList->AddRectFilled( oA, oB, (0 == (iX+iY)%2) ? c_oColorGrey : c_oColorWhite );
		}
	}
	
	pDrawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::ColorConvertFloat4ToU32(oRGBA));

	ImVec2 oRectMin = ImGui::GetItemRectMin();
	ImVec2 oRectMax = ImGui::GetItemRectMax();

	const ImVec2 oPopupSize(175,350);
	//ImGui::SetNextWindowSize(oPopupSize, ImGuiSetCond_Always);
	ImGui::SetNextWindowPos(ImVec2(oRectMin.x, oRectMax.y + 5), ImGuiSetCond_Appearing);
	if (pWindow->StateStorage.GetInt(iStorageOpen, 0) == 1 && ImGui::Begin("Color picker", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		bRet = false;
		const int iCheckboardTileSize = 10;

		ImDrawList* pDrawList = ImGui::GetWindowDrawList();

		ImVec2 oColorPreviewSize(160, 20);
		ImGui::Dummy(oColorPreviewSize);
		ImVec2 oColorAreaMin = ImGui::GetItemRectMin();
		ImVec2 oColorAreaMax = ImGui::GetItemRectMax();

		int iTileHCount = (int)oColorPreviewSize.x / iCheckboardTileSize;
		int iTileVCount = (int)oColorPreviewSize.y / iCheckboardTileSize;

		for (int iX = 0; iX < iTileHCount; ++iX)
		{
			for (int iY = 0; iY < iTileVCount; ++iY)
			{
				pDrawList->AddRectFilled(
					ImVec2(oColorAreaMin.x + iX * iCheckboardTileSize, oColorAreaMin.y + iY * iCheckboardTileSize),
					ImVec2(oColorAreaMin.x + (1+iX) * iCheckboardTileSize, oColorAreaMin.y + (1+iY) * iCheckboardTileSize),
					(0 == (iX+iY)%2) ? c_oColorGrey : c_oColorWhite );
			}
		}

		pDrawList->AddRectFilled(oColorAreaMin, oColorAreaMax, ImGui::ColorConvertFloat4ToU32(oRGBA));

		float fHue = pWindow->StateStorage.GetFloat(iStorageCurrentColorH);
		float fSat = pWindow->StateStorage.GetFloat(iStorageCurrentColorS);
		float fVal = pWindow->StateStorage.GetFloat(iStorageCurrentColorV);

		ImGui::Text("HSV");
		ImGui::Separator();
		{
			//Saturation
			{
				const ImVec2 oSaturationAreaSize(128,128);
				ImGui::InvisibleButton("##SaturationArea", oSaturationAreaSize);
				ImVec2 oSaturationAreaMin = ImGui::GetItemRectMin();
				ImVec2 oSaturationAreaMax = ImGui::GetItemRectMax();

				if (ImGui::IsItemActive())
				{
					bRet = true;
					ImVec2 oCursorPos = ImGui::GetMousePos();
					ImVec2 oNewValue((oCursorPos.x - oSaturationAreaMin.x) / oSaturationAreaSize.x, (oCursorPos.y - oSaturationAreaMin.y) / oSaturationAreaSize.y);
					oNewValue.x = ImClamp(oNewValue.x, 0.f, 1.f);
					oNewValue.y = ImClamp(oNewValue.y, 0.f, 1.f);
					fSat =  oNewValue.x;
					fVal =  1.f - oNewValue.y;
					ImGui::ColorConvertHSVtoRGB( fHue, fSat, fVal, oRGBA.x, oRGBA.y, oRGBA.z );

					ImVec4 oToolTipColor = oRGBA;
					oToolTipColor.w = 1.f;

					ImGui::BeginTooltip();
					ImGui::Dummy(ImVec2(32,32));
					ImVec2 oDummyAreaMin = ImGui::GetItemRectMin();
					ImVec2 oDummyAreaMax = ImGui::GetItemRectMax();
					ImDrawList* pDummyDrawList = ImGui::GetWindowDrawList();
					pDummyDrawList->AddRectFilled( oDummyAreaMin, oDummyAreaMax, ImGui::ColorConvertFloat4ToU32(oToolTipColor));
					ImGui::EndTooltip();
				}

				ImVec2 pos = ImVec2(0, 0);
				ImVec4 c00(1, 1, 1, 1);
				ImVec4 c01(1, 1, 1, 1);
				ImVec4 c10(1, 1, 1, 1);
				ImVec4 c11(1, 1, 1, 1);

				ImVec4 cHueValue(1, 1, 1, 1);
				ImGui::ColorConvertHSVtoRGB(fHue, 1, 1, cHueValue.x, cHueValue.y, cHueValue.z);
				ImU32 oHueColor = ImGui::ColorConvertFloat4ToU32(cHueValue);

				pDrawList->AddRectFilledMultiColor(
					oSaturationAreaMin,
					oSaturationAreaMax,
					c_oColorWhite,
					oHueColor,
					oHueColor,
					c_oColorWhite
					);

				pDrawList->AddRectFilledMultiColor(
					oSaturationAreaMin,
					oSaturationAreaMax,
					c_oColorBlackTransparent,
					c_oColorBlackTransparent,
					c_oColorBlack,
					c_oColorBlack
					);

				pDrawList->AddCircle(ImVec2(oSaturationAreaMin.x + oSaturationAreaSize.x * fSat, oSaturationAreaMin.y + oSaturationAreaSize.y * (1.f - fVal)), 4, c_oColorBlack, 6);
			}
			ImGui::SameLine();
			//Hue
			{
				const ImVec2 oHueAreaSize(20,128);
				ImGui::InvisibleButton("##HueArea", oHueAreaSize);
				//TODO tooltip
				ImVec2 oHueAreaMin = ImGui::GetItemRectMin();
				ImVec2 oHueAreaMax = ImGui::GetItemRectMax();

				if (ImGui::IsItemActive())
				{
					bRet = true;
					fHue = (ImGui::GetMousePos().y - oHueAreaMin.y) / oHueAreaSize.y;
					fHue = ImClamp(fHue, 0.f, 1.f);
					ImGui::ColorConvertHSVtoRGB( fHue, fSat, fVal, oRGBA.x, oRGBA.y, oRGBA.z );

					ImGui::BeginTooltip();
					ImGui::Dummy(ImVec2(32,32));
					ImVec2 oDummyAreaMin = ImGui::GetItemRectMin();
					ImVec2 oDummyAreaMax = ImGui::GetItemRectMax();
					ImDrawList* pDummyDrawList = ImGui::GetWindowDrawList();
					ImVec4 oNewHueRGB;
					oNewHueRGB.w = 1.f;
					ImGui::ColorConvertHSVtoRGB( fSat, 1.f, 1.f, oNewHueRGB.x, oNewHueRGB.y, oNewHueRGB.z );
					pDummyDrawList->AddRectFilled( oDummyAreaMin, oDummyAreaMax, ImGui::ColorConvertFloat4ToU32(oNewHueRGB));
					ImGui::EndTooltip();
				}

				ImVec2 pos = ImVec2(0, 0);
				ImVec4 c0(1, 1, 1, 1);
				ImVec4 c1(1, 1, 1, 1);

				const int iStepCount = 8;
				for (int iStep = 0; iStep < iStepCount; iStep++)
				{
					float h0 = (float)iStep / (float)iStepCount;
					float h1 = (float)(iStep + 1.f) / (float)iStepCount;
					ImGui::ColorConvertHSVtoRGB(h0, 1.f, 1.f, c0.x, c0.y, c0.z);
					ImGui::ColorConvertHSVtoRGB(h1, 1.f, 1.f, c1.x, c1.y, c1.z);

					pDrawList->AddRectFilledMultiColor(
						ImVec2(oHueAreaMin.x, oHueAreaMin.y + oHueAreaSize.y * h0),
						ImVec2(oHueAreaMax.x, oHueAreaMin.y + oHueAreaSize.y * h1),
						ImGui::ColorConvertFloat4ToU32(c0),
						ImGui::ColorConvertFloat4ToU32(c0),
						ImGui::ColorConvertFloat4ToU32(c1),
						ImGui::ColorConvertFloat4ToU32(c1)
						);
				}

				pDrawList->AddLine(
					ImVec2(oHueAreaMin.x, oHueAreaMin.y + oHueAreaSize.y * fHue),
					ImVec2(oHueAreaMax.x, oHueAreaMin.y + oHueAreaSize.y * fHue),
					c_oColorWhite
					);
			}
		}

		//RGBA Sliders
		ImGui::Text("RGBA");
		ImGui::Separator();
		{
			int r = (int)(ImSaturate( oRGBA.x )*255.f);
			int g = (int)(ImSaturate( oRGBA.y )*255.f);
			int b = (int)(ImSaturate( oRGBA.z )*255.f);
			int a = (int)(ImSaturate( oRGBA.w )*255.f);
			bool bChange = false;
			ImGui::PushItemWidth(130.f);
			bChange |= ImGui::SliderInt("R", &r, 0, 255);
			bChange |= ImGui::SliderInt("G", &g, 0, 255);
			bChange |= ImGui::SliderInt("B", &b, 0, 255);
			bChange |= ImGui::SliderInt("A", &a, 0, 255);
			ImGui::PopItemWidth();
			if (bChange)
			{
				bRet = true;
				oRGBA.x = (float)r/255.f;
				oRGBA.y = (float)g/255.f;
				oRGBA.z = (float)b/255.f;
				oRGBA.w = (float)a/255.f;

				ImGui::ColorConvertRGBtoHSV( oRGBA.x, oRGBA.y, oRGBA.z, fHue, fSat, fVal );
			}
		}

		if (bRet)
		{
			pWindow->StateStorage.SetFloat(iStorageCurrentColorH, fHue);
			pWindow->StateStorage.SetFloat(iStorageCurrentColorS, fSat);
			pWindow->StateStorage.SetFloat(iStorageCurrentColorV, fVal);
			pWindow->StateStorage.SetFloat(iStorageCurrentColorA, oRGBA.w);
		}
		
		if (ImGui::Button("Ok"))
		{
			pWindow->StateStorage.SetInt(iStorageOpen, 0);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			pWindow->StateStorage.SetInt(iStorageOpen, 0);
			oRGBA.x = pWindow->StateStorage.GetFloat(iStorageStartColorR);
			oRGBA.y = pWindow->StateStorage.GetFloat(iStorageStartColorG);
			oRGBA.z = pWindow->StateStorage.GetFloat(iStorageStartColorB);
			oRGBA.w = pWindow->StateStorage.GetFloat(iStorageStartColorA);

			bRet = true;
		}
		ImGui::End();
	}

	ImGui::SameLine();

	float fValues[4] = {oRGBA.x, oRGBA.y, oRGBA.z, oRGBA.w};
	if (ImGui::DragFloat4(pLabel, fValues, 0.01f))
	{
		oRGBA.x = fValues[0];
		oRGBA.y = fValues[1];
		oRGBA.z = fValues[2];
		oRGBA.w = fValues[3];
		bRet = true;
	}

	ImGui::PopID();
	return bRet;
}

#endif
