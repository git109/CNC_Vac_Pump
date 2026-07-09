# Re-exporting the SquareLine UI for LVGL 9.5

The firmware ships a placeholder UI because the existing SquareLine export
(`source/Vac_Gauge_V2/src/ui_lvgl/`) targets **LVGL 8.3** and will not compile against
the **LVGL 9.5** in this Zephyr workspace. This guide walks you — assuming no prior
SquareLine experience — through regenerating the UI for LVGL 9 and dropping it into
`stm32/zephyrwb/src/ui/`.

Your project (`source/SquareLine_VacDisp/Sm_Disp.spj`) is small: **one screen, 8
widgets, 1 custom font, ~7 image assets** — comfortably inside SquareLine's free tier.

---

## 0. What we're producing

SquareLine exports a folder of C files (`ui.c`, `ui.h`, `ui_Screen1.c`, image `.c`
files, the font, `ui_helpers.*`, `filelist.txt`). We only want **those UI files** —
not a full template project — and we copy them into `stm32/zephyrwb/src/ui/`.

**Name contract (do not rename these widgets):** the firmware refers to
`ui_Screen1`, `ui_GaugeFace`, `ui_Spinner1`, `ui_ledDarkImage`, `ui_ledRedImage`,
`ui_Needle`, `ui_vacLabel`, `ui_redBugImg`, `ui_grnBugImg`, and the font
`ui_font_courier_Bold`. Keep the widget names exactly as they are so the generated
`ui_*` symbols still match.

---

## 1. Install SquareLine Studio

1. Go to **https://squareline.io/** → **Downloads**. Grab the **macOS** installer
   (Apple Silicon build for your M-series Mac).
2. Open the `.dmg` and drag **SquareLine Studio** to Applications. First launch:
   right-click → **Open** (to clear the Gatekeeper warning for an unsigned app).
3. **Create a free account** and sign in inside the app (it requires login even for the
   free tier).
4. In the license dialog choose the **Personal** license — free, non-commercial, limited
   to **10 screens / 150 widgets**. This project uses 1 screen / 8 widgets, so it fits.
   (A 30-day **Trial** unlocks Business features if you ever need them, but you don't
   here.)

There are no other tools to install — SquareLine bundles its own font/image
converters. You do **not** need the LVGL source; we don't use SquareLine's compiler.

---

## 2. Back up, then open the project

1. **Back up first.** In a terminal:
   ```bash
   cp -R source/SquareLine_VacDisp source/SquareLine_VacDisp.lvgl8.bak
   ```
   (There's also a `backup/` folder inside it, but a full copy is safest — the upgrade
   edits the project in place.)
2. In SquareLine: **File → Open Project…** → select
   `source/SquareLine_VacDisp/Sm_Disp.spj`. It opens as an **LVGL 8.3.11** project
   (board: "Arduino with TFT_eSPI", 240×240).

You'll see the round gauge in the canvas, a **Screens/Widgets** tree on the left, an
**Inspector** on the right, and an **Assets** panel (fonts + images).

---

## 3. Prepare the project for LVGL 9 (do these BEFORE switching the version)

LVGL 9 dropped/renamed some things SquareLine 8 projects rely on. Fix them first:

1. **Fonts.** Open the **Assets** panel → **Fonts** → select `ui_font_courier_Bold` →
   click **Modify**. In the font dialog make sure **Bpp** is **4 bit** or lower (LVGL 9
   removed 8-bpp fonts), then **Modify/Update**. This regenerates the font `.c` with the
   LVGL-9-compatible version macro.
2. **Animation Time.** Select each widget and, in the Inspector, **uncheck "Animation
   Time"** wherever it appears (notably on `ui_Spinner1`). A left-over animation-time
   value can crash LVGL 9. (The spinner keeps spinning; only the checkbox is the issue.)
3. **Color format.** Open **Project Settings** (gear icon / **File → Project Settings**)
   → **Display**. If **Color depth** is set to **16-bit swap**, change it to plain
   **16-bit**. Zephyr's display driver handles byte order; a swapped format here will
   give you wrong colors. Leave resolution **240 × 240**.

---

## 4. Get onto LVGL 9 — the version is locked to the board template

**Important:** the LVGL version is **bound to the board/UI-kit template**, not a free
dropdown. This project's board is **"Arduino with TFT_eSPI"**, which is an
**LVGL-8.3-only** template — so its version list only shows **8.3.11**, and there is no
way to pick 9.x for it. (If you *also* see only 8.3 for new projects, your SquareLine
Studio is too old — you need **≥ v1.4.1** for LVGL 9; check **Help → About** and update
from squareline.io if needed.)

You therefore get LVGL 9 one of two ways:

- **Recommended — create a fresh LVGL-9 project (see §8).** At **File → Create → New
  Project**, choose a template that offers **LVGL 9.x** (a **Desktop** / generic project
  is simplest and is LVGL-9-capable — confirm the **LVGL version** selector shows 9.x
  before creating), set **240 × 240 / 16-bit**, then rebuild the 8 widgets. This is the
  reliable path and, for a 1-screen UI, only ~15 minutes.
- **If available — swap the board template in place.** Some SquareLine versions let you
  change an existing project's **Board/template** to an LVGL-9 board in **Project
  Settings**, after which the LVGL version follows to 9.x. This preserves your layout,
  but the option is version-dependent and often missing — if you don't see it, use the
  fresh-project route above.

Any **9.x** export is source-compatible with the **9.5** runtime; SquareLine doesn't
list 9.5 specifically. After you're on a 9.x project, **Save** (**File → Save**).

---

## 5. Set the export path and options

1. **Project Settings → File export:**
   - **UI Files Export Path** → set to your app's UI folder:
     `…/CNC_Vac_Pump/stm32/zephyrwb/src/ui`
     (Create the `ui` folder if it doesn't exist. You can also export elsewhere and copy
     it in — see step 6.)
   - **Object naming**: leave the **default** (produces `ui_<WidgetName>`, e.g.
     `ui_Needle`). Do **not** switch to `[Screen]_Name` — that would rename symbols and
     break the firmware contract.
   - **Flat export**: optional. The Zephyr build globs `src/ui/**.c` recursively, so
     either flat or with a `components/` subfolder works.
2. **Export → Export UI Files** (menu bar). **Do _not_** use "Create Template Project" —
   that dumps an Arduino/PlatformIO scaffold we don't want.

You should now have `ui.c`, `ui.h`, `ui_Screen1.c`, the `ui_img_*.c` files,
`ui_font_courier_Bold.c`, `ui_helpers.*`, and `filelist.txt` in `stm32/zephyrwb/src/ui/`.

---

## 6. Wire it into the Zephyr build

1. If you exported outside the repo, copy the files in:
   ```bash
   # example, if you exported to ~/SquareLineExport:
   mkdir -p stm32/zephyrwb/src/ui && cp -R ~/SquareLineExport/* stm32/zephyrwb/src/ui/
   ```
2. Enable the UI sources in the build — uncomment the two lines in
   [CMakeLists.txt](CMakeLists.txt):
   ```cmake
   file(GLOB_RECURSE UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.c)
   target_sources(app PRIVATE ${UI_SOURCES})
   ```
3. In [src/main.c](src/main.c): `#include "ui/ui.h"`, replace the `ui_placeholder()`
   call with `ui_init()`, and replace the placeholder `ui_update()` body with the real
   `ui_*` widget calls (label text, needle rotation, bug rotations, red-LED show/hide).
   **Tell me when the files are in `src/ui/` and I'll write this integration for you** —
   the LVGL-9 API names differ from the 8.3 originals (see next section).

---

## 7. LVGL 8.3 → 9 API changes you'll see

The generated UI mostly "just works," but the hand-written calls that drive it change:

| Purpose | LVGL 8.3 (old firmware) | LVGL 9 |
|---|---|---|
| Rotate needle/bug image | `lv_img_set_angle(obj, a)` | `lv_image_set_rotation(obj, a)` |
| Set label text | `lv_label_set_text(obj, s)` | *(unchanged)* |
| Show/hide the red LED | `lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN)` | `lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN)` |
| Event target (only if you add events) | `e->target` | `lv_event_get_target(e)` |

Your `ui_events.h` was empty (no custom events), so the `e->target` change won't bite.
`lv_conf.h` is irrelevant here — Zephyr configures LVGL via `CONFIG_LV_*`.

---

## 8. Fallback: build a fresh LVGL 9 project

If §4's conversion corrupts the project, recreating this small UI is straightforward:

1. **File → Create → New Project.** Board/UI-kit: any (e.g. **"Arduino with
   TFT_eSPI"** again — the board only affects the template we ignore). Set **LVGL
   version = 9.x**, **Resolution 240 × 240**, **Color depth 16-bit** (not swap),
   shape as you like.
2. Recreate the widgets with the **exact same names** (see §0). Every position, size,
   image, font, angle, and pivot value is in the checklist
   **[UI_WIDGET_SPEC.md](UI_WIDGET_SPEC.md)**. For each:
   - Add an **Image** widget for `GaugeFace`, `Needle`, `ledDarkImage`, `ledRedImage`,
     `redBugImg`, `grnBugImg`; add a **Label** `vacLabel`; add a **Spinner** `Spinner1`.
   - Import the PNGs from `source/SquareLine_VacDisp/assets/` via the **Assets → Images →
     Add** button, and assign each to its image widget.
   - Import the font `assets/ui_font_courier_Bold` (or re-add `Courier_BOLD.ttf`) and set
     it on `vacLabel`.
   - Match positions/alignment to the originals (all centered; the needle/bugs are
     rotated at runtime by the firmware, so their static angle is 0).
3. Set the **UI Files Export Path** (§5) and **Export → Export UI Files**.

For a 1-screen UI this takes ~15 minutes and avoids all conversion quirks.

---

## Sources
- [SquareLine: update a project from LVGL 8.x to 9.x (forum)](https://forum.squareline.io/t/how-to-update-a-project-from-lvgl-8-x-to-9-x/6035)
- [SquareLine: upgrade project LVGL major version (forum)](https://forum.squareline.io/t/update-upgrade-project-lvgl-major-version/3858)
- [SquareLine Project Settings docs](https://docs.squareline.io/docs/dev_env/project_settings/)
- [SquareLine Licenses (free Personal tier: 10 screens / 150 widgets)](https://docs.squareline.io/docs/introduction/licences/)
