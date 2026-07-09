# CNC Vacuum Pump Controller — Zephyr application (ESP32-S3)

> ⚠️ **ARCHIVED / NOT MAINTAINED.** This ESP32-S3 attempt was abandoned due to board
> quality / hardware issues during bring-up. The **active** port targets the STM32WB55 —
> see [../stm32/zephyrwb/](../stm32/zephyrwb/). This directory is kept for reference and
> for the ESP32-S3-specific lessons below.

ESP32-S3 / Zephyr 4.4 / LVGL 9.5 port of `source/Vac_Gauge_V2/`.
See the top-level [migration.md](../migration.md) for the full analysis.

## Build & flash

This app is **freestanding** — it lives in this repo, not inside the Zephyr
workspace. Build it from within your existing workspace so `west` can find the
SDK. Do **not** run `west init` (the workspace already exists).

Use the **qualified** board target `esp32s3_devkitc/esp32s3/procpu` — the bare
`esp32s3_devkitc` is rejected by this Zephyr ("Please specify a valid board
target"). Host tools required: `cmake`, `ninja`, `dtc` (`brew install cmake ninja dtc`).

> The wiring lives in `boards/esp32s3_devkitc_esp32s3_procpu.overlay`. That exact
> name (board **plus qualifiers**, `/` → `_`) is what Zephyr auto-applies; a file named
> just `esp32s3_devkitc.overlay` is silently ignored and the build fails on undeclared
> devicetree nodes.

```bash
cd /Users/robert/Documents/dev-env/zephyresp32s3
source .venv/bin/activate
west build -b esp32s3_devkitc/esp32s3/procpu /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/zephyr
west flash
```

### Console / monitor (single cable)
The overlay routes the console to the ESP32-S3 built-in **USB-Serial-JTAG**, so the
one native-USB cable (`/dev/cu.usbmodemXXXX`) carries flashing, monitor, and console:

```bash
west espressif monitor        # or: screen /dev/cu.usbmodem1101 115200  (exit: Ctrl-A k y)
```

A healthy boot prints: `CNC Vac Pump Controller up (LVGL 9.5.0)`.

To use the DevKitC "UART" (CP2102) port instead, delete the two
`zephyr,console`/`zephyr,shell-uart` lines in
`boards/esp32s3_devkitc_esp32s3_procpu.overlay` and monitor `/dev/cu.usbserial-*`.

### Flashing won't connect ("No serial data received")
The S3's native-USB auto-reset can fail to enter download mode while an app is
running. Force it manually: **hold BOOT, tap RESET, release BOOT**, then `west flash`.

### Fast display bring-up (optional)
Zephyr ships `waveshare/esp32s3_touch_lcd_1_28` — the *same* GC9A01 round panel
on an S3. To verify the display stack with zero wiring, build for that board
(its devicetree already defines the panel, so the overlay's display node is
ignored):
```bash
west build -b esp32s3_touch_lcd_1_28/esp32s3/procpu <this dir>
```

## Status: what works today
- Boots, brings up the GC9A01 over MIPI-DBI-SPI, backlight on.
- 10 ms ADC control loop with median+average filter → pump relay switching.
- Rotary-encoder thread with the two-bug setpoint logic.
- **Placeholder** LVGL 9 UI (value + setpoint labels).

## TODO to reach feature parity
1. **Re-export the SquareLine UI for LVGL 9** from
   `source/SquareLine_VacDisp/Sm_Disp.spj` into `src/ui/`, enable the `file(GLOB…)`
   block in `CMakeLists.txt`, and replace `ui_placeholder()`/`ui_update()` in
   `main.c` with `ui_init()` + the `ui_*` widget calls (needle angle, red/green
   bugs, red LED). The 8.3 export in `../source/Vac_Gauge_V2/src/ui_lvgl/` will
   **not** compile against LVGL 9.
2. **Implement the alarm relay** (the original never did — see `main.c` TODO).
3. Optionally replace the polling encoder thread with `gpio-qdec` + `gpio-keys`
   feeding an LVGL encoder indev (`CONFIG_LV_Z_ENCODER_INPUT`).

## ESP32-S3 pin remap (why pins moved)

Two S3 facts break the original classic-ESP32 wiring:
- **GPIO22–25 do not exist** on the ESP32-S3.
- **GPIO26–37** are the in-package flash/PSRAM bus on WROOM-1 modules — unusable.

| Function            | Classic ESP32 | ESP32-S3 (this overlay) | Reason |
|---------------------|:-------------:|:-----------------------:|--------|
| Vac sensor (ADC)    | 35            | **4** (ADC1_CH3)        | GPIO35 isn't an ADC pin on S3 |
| Pump relay          | 26            | **6**                   | 26 = flash/PSRAM |
| Alarm relay         | 21            | **7**                   | 21 reused for backlight |
| Debug LED           | 2             | 2                       | ok |
| Encoder A           | 32            | **40**                  | 32 = flash/PSRAM |
| Encoder B           | 25            | **41**                  | GPIO25 doesn't exist |
| Encoder switch      | 27            | **42**                  | 27 = flash/PSRAM |
| LCD SCLK (Wht)      | 18            | 18                      | ok |
| LCD MOSI (Din)(Yel) | 23            | **11**                  | GPIO23 doesn't exist |
| LCD MISO             | 19            | **none**                | GPIO19 = USB_D-; display is write-only so MISO is dropped |
| LCD CS (Blu)        | 5             | 5                       | ok |
| LCD DC (Grn)        | 17            | 17                      | ok |
| LCD RST (Org)       | 16            | 16                      | ok |
| LCD backlight (Pur) | 21            | 21                      | ok |

> Adjust to match your actual PCB. If your module has **octal PSRAM** (N8R8),
> also keep GPIO35–37 clear. All pins above already avoid that range.
