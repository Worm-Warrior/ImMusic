//
// Central place for all of the app's ImGui styling.
//
// The goal of this theme is information density: compact rows, a small and
// consistent type scale, low-noise borders, and color used to communicate
// state (playing / selected / disabled) instead of decoration.
//

#ifndef IMMUSIC_THEME_H
#define IMMUSIC_THEME_H

#include "imgui.h"

#include <cstddef>

namespace theme {
    namespace color {
        // Base surfaces, darkest -> lightest.
        inline constexpr ImVec4 bg_deep = ImVec4(0.059f, 0.071f, 0.078f, 1.0f); // #0F1214
        inline constexpr ImVec4 bg = ImVec4(0.078f, 0.090f, 0.102f, 1.0f); // #14171A
        inline constexpr ImVec4 panel = ImVec4(0.102f, 0.118f, 0.133f, 1.0f); // #1A1E22
        inline constexpr ImVec4 frame = ImVec4(0.125f, 0.145f, 0.165f, 1.0f); // #20252A
        inline constexpr ImVec4 hover = ImVec4(0.165f, 0.188f, 0.212f, 1.0f); // #2A3036
        inline constexpr ImVec4 active = ImVec4(0.204f, 0.235f, 0.267f, 1.0f); // #343C44

        inline constexpr ImVec4 border = ImVec4(0.173f, 0.200f, 0.227f, 1.0f); // #2C333A
        inline constexpr ImVec4 border_strong = ImVec4(0.231f, 0.267f, 0.302f, 1.0f); // #3B444D

        // Text hierarchy.
        inline constexpr ImVec4 text = ImVec4(0.863f, 0.886f, 0.910f, 1.0f); // #DCE2E8
        inline constexpr ImVec4 muted = ImVec4(0.596f, 0.635f, 0.675f, 1.0f); // #98A2AC
        inline constexpr ImVec4 disabled = ImVec4(0.373f, 0.412f, 0.451f, 1.0f); // #5F6973

        // Amber is the interaction/focus color. Green only means "playing".
        inline constexpr ImVec4 accent = ImVec4(0.910f, 0.635f, 0.290f, 1.0f); // #E8A24A
        inline constexpr ImVec4 accent_hover = ImVec4(0.949f, 0.725f, 0.408f, 1.0f); // #F2B968
        inline constexpr ImVec4 accent_active = ImVec4(0.784f, 0.525f, 0.212f, 1.0f); // #C88636
        inline constexpr ImVec4 playing = ImVec4(0.490f, 0.827f, 0.627f, 1.0f); // #7DD3A0
        inline constexpr ImVec4 error = ImVec4(0.878f, 0.424f, 0.459f, 1.0f); // #E06C75

        inline constexpr ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        inline constexpr ImVec4 row_alt = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);

        inline constexpr ImVec4 selection = ImVec4(0.910f, 0.635f, 0.290f, 0.18f);
        inline constexpr ImVec4 selection_hovered = ImVec4(0.910f, 0.635f, 0.290f, 0.26f);
        inline constexpr ImVec4 selection_active = ImVec4(0.910f, 0.635f, 0.290f, 0.34f);
    }

    // Apply the full metric + color theme. dpi_scale is the display content
    // scale (the same value previously passed to ScaleAllSizes).
    void apply(float dpi_scale);

    // Load the app font(s). Call once after creating the ImGui context.
    void load_fonts();

    // Small text helpers so secondary metadata is visually consistent.
    void text_muted(const char* fmt, ...) IM_FMTARGS(1);

    void text_dim(const char* fmt, ...) IM_FMTARGS(1);

    // Right-align a single line of text within the remaining content region.
    // Useful for table cells and for values pinned to the right edge.
    void text_right_aligned(const ImVec4& color, const char* text);

    // Format seconds as m:ss (or h:mm:ss). Always writes a valid C string.
    void format_time(int total_seconds, char* out, size_t out_size);
}

#endif //IMMUSIC_THEME_H
