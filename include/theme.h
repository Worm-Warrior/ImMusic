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
    // ---------------------------------------------------------------------
    // UI SCALE
    // ---------------------------------------------------------------------
    // The whole interface is authored in logical pixels at this scale. Change
    // this single value to make text and controls larger/smaller; the display
    // DPI scale is applied on top of it. Use theme::px() for any explicit size
    // in UI code so it follows this value.
    inline constexpr float kUiScale = 1.5f;

    // Base body font size in logical pixels (rendered at kUiScale * DPI).
    inline constexpr float base_font_size = 14.0f;

    // Build a color from an 0xRRGGBB value so palette entries can be copied
    // straight from a color sheet. Optional alpha for overlays/tints.
    constexpr ImVec4 hex(unsigned int rgb, float alpha = 1.0f) {
        return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                      static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                      static_cast<float>(rgb & 0xFF) / 255.0f,
                      alpha);
    }

    // ---------------------------------------------------------------------
    // PALETTE
    // ---------------------------------------------------------------------
    // Every color used by the app lives in this single table. When replacing
    // the palette, only edit the values below; UI code refers to the semantic
    // names in theme::color (bg, text, accent, ...).
    //
    // Roles:
    //   surfaces  darkest -> lightest (bg_deep, bg, panel, frame, hover, active)
    //   lines     quiet edge / emphasized edge (border, border_strong)
    //   text      primary / metadata / unavailable (text, muted, disabled)
    //   accent    interaction, focus and selection
    //   playing   playback state only
    //   error     failure messages only
    // Roles mirror imgui-warm-palettes.md so the light theme can be dropped in
    // later by replacing this table. Extra roles (disabled, error, highlights)
    // are called out where the sheet does not define them.
    struct palette_t {
        ImVec4 bg_deep;
        ImVec4 bg;
        ImVec4 panel;
        ImVec4 frame;
        ImVec4 hover;
        ImVec4 active;

        ImVec4 border;
        ImVec4 border_strong;

        ImVec4 text;
        ImVec4 muted;
        ImVec4 disabled;

        ImVec4 accent;
        ImVec4 accent_hover;
        ImVec4 accent_active;
        ImVec4 playing;
        ImVec4 highlight;
        ImVec4 info;
        ImVec4 error;

        ImVec4 overlay;
        ImVec4 overlay_strong;
        ImVec4 row_alt;

        ImVec4 transparent;
    };

    inline constexpr palette_t palette = {
        // Appalachia "Blue Ridge in October" (dark).
        // Surfaces, darkest -> lightest.
        .bg_deep = hex(0x1E1A16), // window bg (also used as text on accent)
        .bg = hex(0x1E1A16), // window bg
        .panel = hex(0x28221C), // panel / child / menu bar / title bg
        .frame = hex(0x332B22), // frame / input bg
        .hover = hex(0x453A2C), // dry leaf litter
        .active = hex(0x453A2C), // frame active shares hover per the mapping

        // Borders.
        .border = hex(0x4B3F31), // cabin timber
        .border_strong = hex(0x5A4C3B), // derived: border lifted for emphasis

        // Text hierarchy.
        .text = hex(0xE6D8BC), // parchment
        .muted = hex(0x9C8F78), // woodsmoke
        .disabled = hex(0x6F6554), // derived: muted dimmed toward window bg

        // Accents + states.
        .accent = hex(0xC4622D), // maple: interaction / focus
        .accent_hover = hex(0xD9773E), // derived: maple lightened ~10%
        .accent_active = hex(0x7A8F4E), // moss: pressed states
        .playing = hex(0x7A8F4E), // moss: playback / success only
        .highlight = hex(0xD8A23A), // goldenrod: checkmarks / slider grabs
        .info = hex(0x6F8799), // blue ridge haze: links / info
        .error = hex(0xC9564A), // not in the sheet: warm brick for errors

        // Dimming behind modal windows / inactive content (window bg at alpha).
        .overlay = hex(0x1E1A16, 0.50f),
        .overlay_strong = hex(0x1E1A16, 0.65f),

        // Zebra striping (very subtle lightening of text color).
        .row_alt = hex(0xE6D8BC, 0.03f),

        .transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
    };

    // Stable semantic names used throughout the UI.
    namespace color {
        inline const ImVec4 &bg_deep = palette.bg_deep;
        inline const ImVec4 &bg = palette.bg;
        inline const ImVec4 &panel = palette.panel;
        inline const ImVec4 &frame = palette.frame;
        inline const ImVec4 &hover = palette.hover;
        inline const ImVec4 &active = palette.active;

        inline const ImVec4 &border = palette.border;
        inline const ImVec4 &border_strong = palette.border_strong;

        inline const ImVec4 &text = palette.text;
        inline const ImVec4 &muted = palette.muted;
        inline const ImVec4 &disabled = palette.disabled;

        inline const ImVec4 &accent = palette.accent;
        inline const ImVec4 &accent_hover = palette.accent_hover;
        inline const ImVec4 &accent_active = palette.accent_active;
        inline const ImVec4 &playing = palette.playing;
        inline const ImVec4 &highlight = palette.highlight;
        inline const ImVec4 &info = palette.info;
        inline const ImVec4 &error = palette.error;

        inline const ImVec4 &overlay = palette.overlay;
        inline const ImVec4 &overlay_strong = palette.overlay_strong;
        inline const ImVec4 &row_alt = palette.row_alt;

        inline const ImVec4 &transparent = palette.transparent;
    }

    // Apply the full metric + color theme. dpi_scale is the display content
    // scale (the same value previously passed to ScaleAllSizes).
    void apply(float dpi_scale);

    // Load the app font(s). Call once after creating the ImGui context.
    void load_fonts();

    // Total UI scale currently in effect (kUiScale * DPI).
    float scale();

    // Convert a logical pixel value to actual pixels. Use this for explicit
    // window sizes, column widths, gaps, etc. so they follow the UI scale.
    float px(float logical_px);

    // Small text helpers so secondary metadata is visually consistent.
    void text_muted(const char *fmt, ...) IM_FMTARGS(1);

    void text_dim(const char *fmt, ...) IM_FMTARGS(1);

    // Right-align a single line of text within the remaining content region.
    // Useful for table cells and for values pinned to the right edge.
    void text_right_aligned(const ImVec4 &color, const char *text);

    // Format seconds as m:ss (or h:mm:ss). Always writes a valid C string.
    void format_time(int total_seconds, char *out, size_t out_size);
}

#endif //IMMUSIC_THEME_H
