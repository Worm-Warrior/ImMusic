//
// ImGui theme implementation. See theme.h for the palette and scaling intent.
//

#include "theme.h"

#include <cstdarg>
#include <cstdio>

namespace theme {
    namespace {
        float current_scale = kUiScale;

        ImVec4 with_alpha(const ImVec4 &color, float alpha) {
            return ImVec4(color.x, color.y, color.z, alpha);
        }
    }

    void apply(float dpi_scale) {
        ImGuiStyle &style = ImGui::GetStyle();

        // Total scale for explicit sizes (window/column widths etc.). Style
        // metrics are scaled below; fonts use the ImGui scale factors.
        current_scale = dpi_scale * kUiScale;

        // Start from the stock metrics so repeated calls (e.g. hot reload /
        // theme previews) do not compound the scaling.
        style = ImGuiStyle();

        // --- Metrics -----------------------------------------------------
        // Authored in logical pixels at a 14px base font and scaled by
        // current_scale. Tight, but not cramped: enough padding to separate
        // rows without wasting vertical space in long track lists.
        style.WindowPadding = ImVec2(8.0f, 6.0f);
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowBorderHoverPadding = 6.0f;
        style.WindowMinSize = ImVec2(240.0f, 120.0f);
        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;

        style.ChildRounding = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 3.0f;
        style.PopupBorderSize = 1.0f;

        style.FramePadding = ImVec2(6.0f, 3.0f);
        style.FrameRounding = 2.0f;
        style.FrameBorderSize = 1.0f;

        style.ItemSpacing = ImVec2(6.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 3.0f);
        style.IndentSpacing = 16.0f;
        style.CellPadding = ImVec2(6.0f, 3.0f);

        style.ScrollbarSize = 12.0f;
        style.ScrollbarRounding = 2.0f;
        style.ScrollbarPadding = 2.0f;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 2.0f;

        style.TabRounding = 2.0f;
        style.TabBorderSize = 0.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f;
        style.TabMinWidthBase = 48.0f;
        style.TabMinWidthShrink = 32.0f;

        style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesFull;
        style.TreeLinesSize = 1.0f;
        style.TreeLinesRounding = 0.0f;

        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextBorderSize = 1.0f;
        style.SeparatorTextPadding = ImVec2(6.0f, 3.0f);

        style.DockingSeparatorSize = 2.0f;
        style.DisabledAlpha = 0.45f;
        style.MouseCursorScale = 1.0f;

        // --- Colors ------------------------------------------------------
        style.Colors[ImGuiCol_Text] = color::text;
        style.Colors[ImGuiCol_TextDisabled] = color::muted;
        style.Colors[ImGuiCol_WindowBg] = color::bg;
        style.Colors[ImGuiCol_ChildBg] = color::panel;
        style.Colors[ImGuiCol_PopupBg] = color::panel;
        style.Colors[ImGuiCol_Border] = color::border;
        style.Colors[ImGuiCol_BorderShadow] = color::transparent;

        style.Colors[ImGuiCol_FrameBg] = color::frame;
        style.Colors[ImGuiCol_FrameBgHovered] = color::hover;
        style.Colors[ImGuiCol_FrameBgActive] = color::active;

        style.Colors[ImGuiCol_TitleBg] = color::panel;
        style.Colors[ImGuiCol_TitleBgActive] = color::panel;
        style.Colors[ImGuiCol_TitleBgCollapsed] = color::panel;
        style.Colors[ImGuiCol_MenuBarBg] = color::panel;

        style.Colors[ImGuiCol_ScrollbarBg] = color::bg;
        style.Colors[ImGuiCol_ScrollbarGrab] = color::border;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = color::hover;
        style.Colors[ImGuiCol_ScrollbarGrabActive] = color::accent;

        style.Colors[ImGuiCol_CheckMark] = color::highlight;
        style.Colors[ImGuiCol_SliderGrab] = color::highlight;
        style.Colors[ImGuiCol_SliderGrabActive] = color::accent_active;

        style.Colors[ImGuiCol_Button] = color::frame;
        style.Colors[ImGuiCol_ButtonHovered] = color::hover;
        style.Colors[ImGuiCol_ButtonActive] = color::active;

        // Selection tints derive from the primary accent so a palette swap
        // keeps them in sync. HeaderActive uses moss, per the sheet mapping.
        style.Colors[ImGuiCol_Header] = with_alpha(color::accent, 0.18f);
        style.Colors[ImGuiCol_HeaderHovered] = with_alpha(color::accent, 0.26f);
        style.Colors[ImGuiCol_HeaderActive] = with_alpha(color::accent_active, 0.28f);

        style.Colors[ImGuiCol_Separator] = color::border;
        style.Colors[ImGuiCol_SeparatorHovered] = color::accent;
        style.Colors[ImGuiCol_SeparatorActive] = color::accent;

        style.Colors[ImGuiCol_ResizeGrip] = with_alpha(color::accent, 0.25f);
        style.Colors[ImGuiCol_ResizeGripHovered] = with_alpha(color::accent, 0.40f);
        style.Colors[ImGuiCol_ResizeGripActive] = with_alpha(color::accent, 0.55f);
        style.Colors[ImGuiCol_InputTextCursor] = color::accent;

        style.Colors[ImGuiCol_TabHovered] = color::hover;
        style.Colors[ImGuiCol_Tab] = color::bg;
        style.Colors[ImGuiCol_TabSelected] = color::panel;
        style.Colors[ImGuiCol_TabSelectedOverline] = color::accent;
        style.Colors[ImGuiCol_TabDimmed] = color::bg;
        style.Colors[ImGuiCol_TabDimmedSelected] = color::panel;
        style.Colors[ImGuiCol_TabDimmedSelectedOverline] = color::border_strong;

        style.Colors[ImGuiCol_DockingPreview] = with_alpha(color::accent, 0.34f);
        style.Colors[ImGuiCol_DockingEmptyBg] = color::transparent;

        style.Colors[ImGuiCol_PlotLines] = color::accent;
        style.Colors[ImGuiCol_PlotLinesHovered] = color::accent_hover;
        style.Colors[ImGuiCol_PlotHistogram] = color::accent;
        style.Colors[ImGuiCol_PlotHistogramHovered] = color::accent_hover;

        style.Colors[ImGuiCol_TableHeaderBg] = color::panel;
        style.Colors[ImGuiCol_TableBorderStrong] = color::border_strong;
        style.Colors[ImGuiCol_TableBorderLight] = color::border;
        style.Colors[ImGuiCol_TableRowBg] = color::transparent;
        style.Colors[ImGuiCol_TableRowBgAlt] = color::row_alt;

        style.Colors[ImGuiCol_TextLink] = color::info;
        style.Colors[ImGuiCol_TextSelectedBg] = with_alpha(color::accent, 0.35f);
        style.Colors[ImGuiCol_TreeLines] = color::border_strong;

        style.Colors[ImGuiCol_DragDropTarget] = color::accent;
        style.Colors[ImGuiCol_DragDropTargetBg] = with_alpha(color::accent, 0.18f);
        style.Colors[ImGuiCol_UnsavedMarker] = color::accent;
        style.Colors[ImGuiCol_NavCursor] = color::accent;

        style.Colors[ImGuiCol_NavWindowingHighlight] = color::text;
        style.Colors[ImGuiCol_NavWindowingDimBg] = color::overlay;
        style.Colors[ImGuiCol_ModalWindowDimBg] = color::overlay_strong;

        // Scale metrics for the UI scale + high-DPI displays. Fonts use the
        // separate ImGui scale factors so they stay crisp.
        style.ScaleAllSizes(current_scale);
        style.FontSizeBase = base_font_size;
        style.FontScaleMain = kUiScale;
        style.FontScaleDpi = dpi_scale;
    }

    void load_fonts() {
        ImGuiIO &io = ImGui::GetIO();

        // M PLUS 1 Code keeps track metadata and durations aligned, and it
        // includes Japanese kana/kanji. Build a range that also covers common
        // simplified Chinese so international libraries render instead of
        // turning into '?'.
        static ImVector<ImWchar> glyph_ranges;
        if (glyph_ranges.empty()) {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
            builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
            builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            builder.BuildRanges(&glyph_ranges);
        }

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;

        // base_font_size is in logical pixels; FontScaleMain applies kUiScale.
        ImFont *body = io.Fonts->AddFontFromFileTTF(
            "../res/MPLUS1Code-Regular.ttf", base_font_size, &cfg, glyph_ranges.Data);

        if (body) {
            io.FontDefault = body;
        } else {
            io.Fonts->AddFontDefault();
        }
    }

    float scale() {
        return current_scale;
    }

    float px(float logical_px) {
        return logical_px * current_scale;
    }

    void text_muted(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextColoredV(color::muted, fmt, args);
        va_end(args);
    }

    void text_dim(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextColoredV(color::disabled, fmt, args);
        va_end(args);
    }

    void text_right_aligned(const ImVec4 &color, const char *text) {
        const float text_width = ImGui::CalcTextSize(text).x;
        const float available = ImGui::GetContentRegionAvail().x;
        if (text_width < available) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - text_width);
        }
        ImGui::TextColored(color, "%s", text);
    }

    void format_time(int total_seconds, char *out, size_t out_size) {
        if (!out || out_size == 0) return;
        if (total_seconds < 0) total_seconds = 0;

        const int hours = total_seconds / 3600;
        const int minutes = (total_seconds % 3600) / 60;
        const int seconds = total_seconds % 60;

        if (hours > 0) {
            snprintf(out, out_size, "%d:%02d:%02d", hours, minutes, seconds);
        } else {
            snprintf(out, out_size, "%d:%02d", minutes, seconds);
        }
    }
}
