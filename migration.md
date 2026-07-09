# CNC Vacuum Pump Controller — Zephyr Migration

Porting the original Arduino firmware (`source/Vac_Gauge_V2/`) to Zephyr RTOS.
This top-level doc is the **overview + platform history**; the board-specific detail
lives in each app's own README.

---

## Platform history

| Stage | Platform | Status | Location |
|---|---|---|---|
| Original | Arduino / classic ESP32 (WeMos D1 Mini32) | Reference | [source/Vac_Gauge_V2/](source/Vac_Gauge_V2/) |
| Attempt 1 | ESP32-S3 (Lonely Binary N16R8) + Zephyr | **Abandoned** — board quality / hardware failure during bring-up | [zephyr/](zephyr/) *(archived)* |
| **Current** | **STM32WB55 (P-NUCLEO-WB55, MB1355)** + Zephyr | **Active** | [stm32/zephyrwb/](stm32/zephyrwb/) |

**Why the switch:** the ESP32-S3 bring-up hit repeated hardware issues (native-USB
flashing quirks, and finally a board that stopped enumerating entirely after the LCD
was connected). The STM32WB55 Nucleo is a cleaner target — onboard ST-LINK (one cable
for flash + console), real onboard LEDs, and no pin traps. The ESP32-S3 app is kept
for reference but is not maintained.

---

## What carried over unchanged

The port is mostly board-agnostic C; only the devicetree/board glue differs per target:

- **Business logic** — `vac_filter.c` (median+average filter, with the original's
  ring-buffer out-of-bounds bug fixed) and `encoder.c` (the two-"bug" setpoint logic)
  are identical across both ports.
- **Control structure** — Arduino `setup()/loop()` → a `k_timer`+workqueue 10 ms control
  loop plus the LVGL handler loop. `millis()` → `k_uptime_get_32()`; `xTaskCreate` →
  `k_thread`; `analogRead` → the Zephyr ADC API; `pinMode/digitalWrite` → `gpio_dt_spec`.
- **Display** — GC9A01 over the `zephyr,mipi-dbi-spi` + `galaxycore,gc9x01x` binding
  (`display-inversion` on, RGB565). The hand-written TFT_eSPI `my_disp_flush` is gone —
  Zephyr's display driver + LVGL module own it.

## The one cross-cutting decision: LVGL version

The workspace ships **LVGL 9.5**; the SquareLine export in
`source/Vac_Gauge_V2/src/ui_lvgl/` is **LVGL 8.3.11** and will **not** compile against
9.x (renamed APIs: `lv_disp_drv_*` → `lv_display_*`, `lv_img_set_angle` →
`lv_image_set_rotation`, etc.). Both ports therefore ship a small **placeholder LVGL 9
UI** and defer the real UI to a **re-export from SquareLine targeting LVGL 9** (open
`source/SquareLine_VacDisp/Sm_Disp.spj`, set the export target to LVGL 9, regenerate into
the app's `src/ui/`). `lv_conf.h` is discarded — Zephyr configures LVGL via `CONFIG_LV_*`.

---

## Active target: STM32WB55 — see [stm32/zephyrwb/README.md](stm32/zephyrwb/README.md)

Build and flash over the single ST-LINK cable:

```bash
cd /Users/robert/Documents/dev-env/zephyresp32s3
source .venv/bin/activate
west build -b nucleo_wb55rg /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/stm32/zephyrwb
west flash
```

Highlights vs. the ESP32-S3 attempt:
- **One USB cable** = flash (ST-LINK/SWD) + console (USART1 VCP). No download-mode dance.
- **Onboard green LED (LD2)** is the heartbeat — no WS2812 driver needed.
- **No pin traps** — wiring goes to the **ST Morpho** headers (CN7/CN10). Full pin map
  and Morpho cautions in the app README / [stm32/zephyrwb/WIRING.html](stm32/zephyrwb/WIRING.html).

Verified: configures + compiles + links clean (FLASH ~33%, RAM ~39% of 192 KB).

### KiCad schematic
The `PCB/` schematic still shows the original WeMos/ESP32. To move it to the WB55, follow
the beginner swap kit: **[PCB/SCHEMATIC_SWAP_WB55.html](PCB/SCHEMATIC_SWAP_WB55.html)**
([md](PCB/SCHEMATIC_SWAP_WB55.md)). Because the design connects by named nets, only the MCU
symbol changes — the LCD, relays, encoder, and sensor divider stay put.

---

## Archived target: ESP32-S3 — see [zephyr/README.md](zephyr/README.md)

Kept for reference only. Notable ESP32-S3 lessons captured there, in case the platform
is ever revisited:
- GPIO22–25 don't exist; GPIO26–37 are the flash/PSRAM bus on WROOM/N-R8 modules.
- GPIO19/20 are the native-USB D-/D+ lines — a display MISO there kills USB.
- Console-over-USB-Serial-JTAG and the BOOT/RESET download-mode dance.
- Onboard RGB LED is a WS2812 (GPIO48), needing the `ws2812-spi` driver.

---

## Shared build environment

- Zephyr workspace: `~/Documents/dev-env/zephyresp32s3` (Zephyr **4.4.99**, LVGL **9.5.0**).
  **Do not `west init`** again — build the freestanding apps from inside it.
- Host tools: `cmake`, `ninja`, `dtc` (`brew install cmake ninja dtc`).
- Devicetree overlays auto-apply when named after the board target
  (`nucleo_wb55rg.overlay`, `esp32s3_devkitc_esp32s3_procpu.overlay`).
