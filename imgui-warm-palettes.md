# Dear ImGui Warm Palettes: NC Coast and Appalachia

Two palette-driven themes built around warm, natural color. Define the roles below once as constants, then derive every `ImGuiCol_*` slot from them so the theme stays coherent.

---

## 1. NC Coast: "Sand and Sea Oats" (light)

Dry dunes, driftwood, Atlantic teal water, and a coral sunrise.

| Role | Hex | Inspiration |
|---|---|---|
| Window bg | `#EADFC8` | Dry sand |
| Panel / child bg | `#E0D3B8` | Damp sand |
| Frame / input bg | `#F3EBD9` | Bleached shell |
| Hover | `#D2C3A3` | Tide line |
| Border | `#BFAF8E` | Dune shadow |
| Text | `#3A3128` | Dark driftwood |
| Muted text | `#7A6D58` | Weathered pier |
| Primary accent | `#2F7F83` | Atlantic teal |
| Secondary accent | `#D9734E` | Sunrise coral |
| Highlight | `#B8893A` | Sea oats |

**Usage:** teal for buttons, tabs, and headers. Coral only for active/pressed states and things that need attention. Sea-oat gold for checkmarks and slider grabs. Use `#F3EBD9` for text on teal buttons.

**Font idea:** a serif such as Lora for headings, with a humanist sans for body text.

---

## 2. Appalachia: "Blue Ridge in October" (dark)

Bark and soil, woodsmoke, maple, moss, and blue haze on far ridges.

| Role | Hex | Inspiration |
|---|---|---|
| Window bg | `#1E1A16` | Forest floor |
| Panel / child bg | `#28221C` | Dark bark |
| Frame / input bg | `#332B22` | Walnut |
| Hover | `#453A2C` | Dry leaf litter |
| Border | `#4B3F31` | Cabin timber |
| Text | `#E6D8BC` | Parchment |
| Muted text | `#9C8F78` | Woodsmoke |
| Primary accent | `#C4622D` | Maple |
| Secondary accent | `#7A8F4E` | Moss and fern |
| Highlight | `#D8A23A` | Goldenrod |
| Info / links | `#6F8799` | Blue ridge haze |

**Usage:** maple is the main interactive color. Moss for success states and selections. Haze blue sparingly, since a little cool color makes the warm tones look richer.

**Font idea:** Source Sans 3 or Fira Sans for a warmer feel than Inter.

---

## 3. Role to `ImGuiCol_*` mapping

Use the same mapping for both themes. "Accent+" means the primary accent lightened about 10%. "Accent-" means darkened about 10%.

| ImGui slot(s) | Role |
|---|---|
| `WindowBg`, `PopupBg` | Window bg (popups can use Panel) |
| `ChildBg`, `MenuBarBg`, `TitleBg`, `TitleBgActive`, `TitleBgCollapsed` | Panel bg |
| `FrameBg` | Frame / input bg |
| `FrameBgHovered`, `FrameBgActive` | Hover |
| `Border`, `Separator`, `BorderShadow` (transparent) | Border |
| `Text` | Text |
| `TextDisabled` | Muted text |
| `Button`, `Header`, `Tab` | Primary accent (Tab can be a dimmer blend) |
| `ButtonHovered`, `HeaderHovered`, `TabHovered` | Accent+ |
| `ButtonActive`, `HeaderActive` | Secondary accent |
| `TabSelected` (`TabActive` in older versions) | Primary accent |
| `CheckMark`, `SliderGrab` | Highlight |
| `SliderGrabActive` | Secondary accent |
| `ScrollbarBg` | Window bg |
| `ScrollbarGrab` | Border |
| `ScrollbarGrabHovered`, `ScrollbarGrabActive` | Hover, then Accent- |
| `TextSelectedBg` | Primary accent at about 35% alpha |
| `ResizeGrip*` | Accent at low alpha |
| `ModalWindowDimBg` | Window bg at about 50% alpha (tune by eye) |

Slot names shift between Dear ImGui versions, so check `imgui.h` for the version you build against.

---

## 4. Style settings that keep it warm, not clinical

```cpp
ImGuiStyle& s = ImGui::GetStyle();
s.WindowRounding    = 6.0f;
s.ChildRounding     = 6.0f;
s.FrameRounding     = 5.0f;
s.PopupRounding     = 6.0f;
s.TabRounding       = 5.0f;
s.ScrollbarRounding = 8.0f;
s.GrabRounding      = 5.0f;

s.WindowPadding     = ImVec2(12, 12);
s.FramePadding      = ImVec2(10, 6);
s.ItemSpacing       = ImVec2(10, 8);
s.ItemInnerSpacing  = ImVec2(8, 6);

s.WindowBorderSize  = 1.0f;
s.FrameBorderSize   = 0.0f;
```

Starting values only. Tune them with `ImGui::ShowStyleEditor()`.

---

## 5. Design rules

- No pure black or white anywhere. Even the darkest and lightest values carry a warm tint.
- Keep borders low contrast and warm. Hard gray borders are a big part of the clinical look.
- Go roomier than default: more padding and spacing, with 4 to 6 px rounding.
- Pick a font with some personality.
- Use 3 to 4 surface levels (window, panel, frame, hover) with subtle steps between them so the UI reads as layered.
- Reserve accents for interactive elements. Overusing them makes a theme look garish.
- Tune the alpha-heavy slots (`ModalWindowDimBg`, `TextSelectedBg`, `Border`) by eye.
- Check contrast on muted and disabled text, especially in the light theme.

---

## 6. Related references

- **Gruvbox:** nearest existing palette to Appalachia.
- **Everforest:** nearest existing palette to Appalachia's mossy side.
- **ImGui "Themes" thread** on the Dear ImGui GitHub issues: community style exports to study.
