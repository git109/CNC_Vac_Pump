# SquareLine UI — per-widget rebuild spec (LVGL-9 fresh project, SLS 1.6.1)

Every value below is pulled from your existing project
(`source/Vac_Gauge_V2/src/ui_lvgl/ui_Screen1.c` + `Sm_Disp.spj`). Recreate the widgets
in this order (top → bottom = back → front z-order) and keep the **names exact**.

## How the SLS 1.6.1 Inspector is laid out
A selected widget's Inspector has these sections, top → bottom. Values below are tagged
with which section they go in:

- **[Props]** — *Widget Properties* (top block): the **Name**, plus **widget-specific**
  fields. Image → **Asset, Rotation, Pivot X/Y, Scale**. Label → **Text**. Spinner →
  **Time, Angle**.
- **[Transform]** — **X, Y, Width, Height, Align** (position/size; X/Y are in px).
- **[Style]** — *Style Settings* (bottom): add a local style to a **Part** (Main /
  Indicator / …) then set **Background color, Radius, Text (font/color/spacing)**, etc.

Because every widget uses **Align = CENTER**, the Transform **X/Y are offsets from screen
center** (negative Y = up, negative X = left). "Size = content" → leave Width/Height on
the **content** setting so the image keeps its PNG size.

---

## Screen: `Screen1`
- **[Style]** Main part → Background **color `#000000`** (black), opacity 255.

## 1. `GaugeFace` — Image
- **[Props]** Asset = `Small_Gauge_Scale.png`
- **[Transform]** Align **CENTER**, X **0**, Y **-6**, size **content**

## 2. `Spinner1` — Spinner  *(decorative — firmware never drives it; optional)*
- **[Transform]** Align **CENTER**, X **20**, Y **49**, Width **30**, Height **30**
- **[Style]** **Main** part → Arc → **Width 5**; **Indicator** part → Arc → **Width 5**
- **[Flags]** Clickable **off**
- **No Time/Angle fields in SLS 1.6.1:** LVGL 9 moved the spin time/arc out of the
  spinner constructor, so SquareLine no longer exposes them — the animation just runs at
  its built-in defaults. Place the widget and move on. (You can even skip this widget
  entirely; nothing in the firmware references it.)

## 3. `ledDarkImage` — Image
- **[Props]** Asset = `Dark_LED.png`
- **[Transform]** Align **CENTER**, X **42**, Y **5**, size **content**

## 4. `ledRedImage` — Image  *(create AFTER ledDarkImage so it stacks on top)*
- **[Props]** Asset = `Red_LED.png`
- **[Transform]** Align **CENTER**, X **42**, Y **5**, size **content**

## 5. `Needle` — Image
- **[Props]** Asset = `Small_Gauge_Needle.png`; **Rotation 0**; **Pivot X 78 / Pivot Y 15**
- **[Transform]** Align **CENTER**, X **-32**, Y **35**, size **content**

## 6. `vacLabel` — Label
- **[Props]** Text = `30.0`
- **[Transform]** Align **CENTER**, X **0**, Y **-24**, size **content**
- **[Style]** Main part:
  - Text → **Font `courierBold`**, **Color `#BEDBBE`**, **Letter space -2**, Line space 0
  - Background → **Color `#454545`**, opacity 255
  - **Radius 5**

## 7. `redBugImg` — Image
- **[Props]** Asset = `red_bug_.png` *(trailing underscore)*; **Rotation -45.0** (°);
  **Pivot X 8 / Pivot Y 119**
- **[Transform]** Align **CENTER**, X **0**, Y **-107**, size **content**

## 8. `grnBugImg` — Image
- **[Props]** Asset = `grn_bug_.png` *(trailing underscore)*; **Rotation -90.0** (°);
  **Pivot X 8 / Pivot Y 119**
- **[Transform]** Align **CENTER**, X **0**, Y **-107**, size **content**

---

## Font: `courierBold`  — two steps (this trips people up)

**Copying the TTF into Assets is NOT enough** — it only makes it a *source*. You must
then **generate** the font, or it won't appear in the label's Font dropdown.

1. **Add the source:** drop `Courier_BOLD.ttf` into the project's `assets/` (Assets →
   Fonts). This makes it selectable in the Font Manager — it does *not* create a usable
   font yet.
2. **Generate it:** open **Panels → Font Manager → Create New Font** and set:

| Field | Value | Note |
|---|---|---|
| Font Name | `courierBold` | |
| Select Font Asset | `Courier_BOLD.ttf` | from the dropdown |
| Font Size | **30** | |
| **Bpp** | **4** | ⚠️ original was **8 bpp** — LVGL 9 rejects 8-bpp; 8 also blocks generation |
| Range | `0x20-0x7F` | ASCII printable |

3. Click **Create Font** → it appears under **Created Fonts**.
4. Assign it: **vacLabel → [Style] → Text → Font → `courierBold`**.

## Images to import (**Assets → Images → Add**, from `source/SquareLine_VacDisp/assets/`)
Use exactly these 6; ignore the near-duplicates (`Red_Bug.png`, `Small_Gauge_Needle copy.png`,
`*_.png` scale variant, `Red_Bug-17X26.png`):

| Widget | PNG |
|---|---|
| GaugeFace | `Small_Gauge_Scale.png` |
| ledDarkImage | `Dark_LED.png` |
| ledRedImage | `Red_LED.png` |
| Needle | `Small_Gauge_Needle.png` |
| redBugImg | `red_bug_.png` |
| grnBugImg | `grn_bug_.png` |

---

## Notes
- **Rotation units:** SLS shows degrees with one decimal (`-45.0`) — matches the old
  code's 0.1° integer (`-450`). Enter the degree value.
- **Pivot** only matters on rotating images (`Needle`, `redBugImg`, `grnBugImg`); leave it
  default on the rest.
- The firmware drives, at runtime: `vacLabel` text, `Needle` rotation, `redBugImg` /
  `grnBugImg` rotation, and `ledRedImage` show/hide — all by the exact names above; the
  values here are just the starting layout.

Once exported to `stm32/zephyrwb/src/ui/`, ping me and I'll wire `ui_init()` + those
runtime calls into `main.c` for LVGL 9.
