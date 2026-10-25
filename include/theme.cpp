//
// ImGui theme implementation. See theme.h for the intent behind the palette.
//

#include "theme.h"

#include <cstdarg>
#include <cstdio>

namespace theme {
    namespace {
        constexpr float body_font_size = 14.0f;
    }

    void apply(float dpi_scale) {
        ImGuiStyle& style = ImGui::GetStyle();

        // Start from the stock metrics so repeated calls (e.g. hot reload /
        // theme previews) do not compound the scaling.
        style = ImGuiStyle();

        // --- Metrics -----------------------------------------------------
        // Tight, but not cramped: enough padding to separate rows without
        // wasting vertical space in long track lists.
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
        style.Colors[ImGuiCol_ChildBg] = color::transparent;
        style.Colors[ImGuiCol_PopupBg] = color::panel;
        style.Colors[ImGuiCol_Border] = color::border;
        style.Colors[ImGuiCol_BorderShadow] = color::transparent;

        style.Colors[ImGuiCol_FrameBg] = color::frame;
        style.Colors[ImGuiCol_FrameBgHovered] = color::hover;
        style.Colors[ImGuiCol_FrameBgActive] = color::active;

        style.Colors[ImGuiCol_TitleBg] = color::bg_deep;
        style.Colors[ImGuiCol_TitleBgActive] = color::panel;
        style.Colors[ImGuiCol_TitleBgCollapsed] = color::bg_deep;
        style.Colors[ImGuiCol_MenuBarBg] = color::bg_deep;

        style.Colors[ImGuiCol_ScrollbarBg] = color::transparent;
        style.Colors[ImGuiCol_ScrollbarGrab] = color::border_strong;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = color::muted;
        style.Colors[ImGuiCol_ScrollbarGrabActive] = color::accent;

        style.Colors[ImGuiCol_CheckMark] = color::accent;
        style.Colors[ImGuiCol_SliderGrab] = color::accent;
        style.Colors[ImGuiCol_SliderGrabActive] = color::accent_hover;

        style.Colors[ImGuiCol_Button] = color::frame;
        style.Colors[ImGuiCol_ButtonHovered] = color::hover;
        style.Colors[ImGuiCol_ButtonActive] = color::active;

        style.Colors[ImGuiCol_Header] = color::selection;
        style.Colors[ImGuiCol_HeaderHovered] = color::selection_hovered;
        style.Colors[ImGuiCol_HeaderActive] = color::selection_active;

        style.Colors[ImGuiCol_Separator] = color::border;
        style.Colors[ImGuiCol_SeparatorHovered] = color::accent;
        style.Colors[ImGuiCol_SeparatorActive] = color::accent;

        style.Colors[ImGuiCol_ResizeGrip] = color::border;
        style.Colors[ImGuiCol_ResizeGripHovered] = color::accent;
        style.Colors[ImGuiCol_ResizeGripActive] = color::accent;
        style.Colors[ImGuiCol_InputTextCursor] = color::accent;

        style.Colors[ImGuiCol_TabHovered] = color::hover;
        style.Colors[ImGuiCol_Tab] = color::bg_deep;
        style.Colors[ImGuiCol_TabSelected] = color::panel;
        style.Colors[ImGuiCol_TabSelectedOverline] = color::accent;
        style.Colors[ImGuiCol_TabDimmed] = color::bg_deep;
        style.Colors[ImGuiCol_TabDimmedSelected] = color::panel;
        style.Colors[ImGuiCol_TabDimmedSelectedOverline] = color::border_strong;

        style.Colors[ImGuiCol_DockingPreview] = color::selection_active;
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

        style.Colors[ImGuiCol_TextLink] = color::accent;
        style.Colors[ImGuiCol_TextSelectedBg] = color::selection_active;
        style.Colors[ImGuiCol_TreeLines] = color::border_strong;

        style.Colors[ImGuiCol_DragDropTarget] = color::accent;
        style.Colors[ImGuiCol_DragDropTargetBg] = color::selection;
        style.Colors[ImGuiCol_UnsavedMarker] = color::accent;
        style.Colors[ImGuiCol_NavCursor] = color::accent;

        style.Colors[ImGuiCol_NavWindowingHighlight] = color::text;
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);

        // Scale metrics (not fonts) for high-DPI displays.
        style.ScaleAllSizes(dpi_scale);
        style.FontScaleDpi = dpi_scale;
        style.FontSizeBase = body_font_size;
    }

    void load_fonts() {
        ImGuiIO& io = ImGui::GetIO();

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

        ImFont* body = io.Fonts->AddFontFromFileTTF(
            "../res/MPLUS1Code-Regular.ttf", body_font_size, &cfg, glyph_ranges.Data);

        if (body) {
            io.FontDefault = body;
        } else {
            io.Fonts->AddFontDefault();
        }
    }

    void text_muted(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextColoredV(color::muted, fmt, args);
        va_end(args);
    }

    void text_dim(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextColoredV(color::disabled, fmt, args);
        va_end(args);
    }

    void text_right_aligned(const ImVec4& color, const char* text) {
        const float text_width = ImGui::CalcTextSize(text).x;
        const float available = ImGui::GetContentRegionAvail().x;
        if (text_width < available) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - text_width);
        }
        ImGui::TextColored(color, "%s", text);
    }

    void format_time(int total_seconds, char* out, size_t out_size) {
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
