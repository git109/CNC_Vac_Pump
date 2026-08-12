# CNC Vacuum Pump Controller

Controller for a shop CNC vacuum table: reads a vacuum sensor, drives the pump
relay to hold vacuum between two setpoints, and shows the reading on a round LCD
gauge. The active target is an **STM32WB55 (P-NUCLEO-WB55)** running **Zephyr**.

## Current status — EVT1 (tagged + pushed 2026-08)

First calibrated build on real hardware is done, committed, and pushed to
`origin/main` as annotated tag **`EVT1`**. Working tree is clean.

- Real **MPXV4115V** sensor read on `PA0/ADC1_IN5`; `CONFIG_VAC_SIM` compiled out.
- Readout calibrated against the manifold gauge and verified: `0.0 inHg` at
  atmosphere, `~29 inHg` at full vacuum (VB2200 sealed).
- GC9A01 gauge display working; needle tracks the calibrated value.
- Schematic updated: sensor-end supply decoupling + ADC-tap filter cap.

### Open items / next steps
- **Alarm relay is unimplemented** — see the `TODO` in `stm32/zephyrwb/src/main.c`
  (`vac_work_handler`). Hardware pins exist (`alarm`, `PC2/ALARM` = CN7-38).
- Optional: spot-check a **mid-range** vacuum point (~15 inHg) against the gauge
  to confirm linearity; add a breakpoint only if it's off.
- Optional hardware review: atmosphere sits at ~98.8% of ADC full-scale (little
  headroom at 0 inHg — practically harmless for a vacuum-only gauge). The fitted
  divider is 3.94k/6.8k, not the 4.7k/10k originally planned.
- KiCad: pre-existing ERC violations (~40, unrelated to our work). The new
  `Sensor_End` sheet's 3-pin header `J8` shows a benign `lib_symbol_mismatch` —
  right-click → *Update Symbol from Library* to clear.

## Tech stack

- **MCU firmware:** Zephyr 4.4 (RTOS), C, board `nucleo_wb55rg`. LVGL 9.5 UI.
- **Display:** GC9A01 240×240 round LCD, RGB565, over MIPI-DBI-SPI on SPI1.
- **Sensor:** MPXV4115V absolute pressure sensor (5 V supply), datasheet in
  `Docs/MPXV4115V-3138976.pdf`.
- **Pump:** OMT VB2200 rotary-vane, 14.4 CFM (~29 inHg ultimate).
- **UI source:** SquareLine Studio project → generated LVGL C in `src/ui/`.
- **PCB:** KiCad 10 project in `PCB/Vac_Pump_Controller/`.

## Repository layout

```
stm32/zephyrwb/     ← ACTIVE firmware (Zephyr, WB55). Work here.
  src/main.c          control loop + LVGL UI update
  src/vac_filter.c    15-sample median + running average on the raw ADC
  src/encoder.c       rotary encoder (setpoints)
  src/app_state.h     app_red / app_grn setpoints (inHg*10)
  src/ui/             SquareLine-generated LVGL 9 widgets
  boards/nucleo_wb55rg.overlay   devicetree: ADC ch, SPI/LCD, encoder pins
  prj.conf, Kconfig
zephyr/             ← ABANDONED ESP32-S3 attempt. Do NOT build/edit.
source/             ← SquareLine project + original ESP32 firmware (Vac_Gauge_V2)
PCB/Vac_Pump_Controller/   ← KiCad 10 schematic/PCB
CAD/  Docs/  images/        ← STEP models, datasheet/BOM, photos
migration.md, PCB/SCHEMATIC_SWAP_WB55.md   ← ESP32→WB55 migration notes
```

## Build / flash / console

Two west workspaces exist on this Mac; the login shell auto-activates the wrong
one (`zephyrproject`). **Always build from the `zephyresp32s3` topdir.**

```bash
cd /Users/robert/Documents/dev-env/zephyresp32s3
source .venv/bin/activate
# Build (add -p always when switching away from the old zephyr/ app):
west build -b nucleo_wb55rg /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/stm32/zephyrwb
# Flash (STM32CubeProgrammer runner — see WARNING below):
export PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin:$PATH"
west flash
```

- **Simulation mode** (synthetic 0→30→0 sweep, no sensor): append
  `-- -DCONFIG_VAC_SIM=y` to `west build`. Omit it for the real sensor.
- **⚠️ Flash with STM32CubeProgrammer, NEVER OpenOCD** (`west flash -r openocd`).
  OpenOCD can mass-erase the FUS / wireless stack on CPU2. CubeProgrammer erases
  only the M4 app (sectors 0–115). This is a hard rule.
- **Console:** USART1 → ST-LINK VCP, **115200 8N1**. The device name varies
  between reconnects — `ls /dev/cu.usbmodem*` first, then
  `screen /dev/cu.usbmodem<NNN> 115200`. If `screen` "can't open the file," it's
  the wrong suffix, not permissions. Healthy boot logs
  `CNC Vac Pump Controller up (LVGL 9.5.0)` (and, in sim, `VAC_SIM enabled`).
- **Non-interactive serial capture** (when `screen` is fussy), from the venv:
  ```bash
  python3 -c "import serial; s=serial.Serial('/dev/cu.usbmodem103',115200,timeout=1); [print(s.readline().decode(errors='replace').rstrip()) for _ in range(30)]"
  ```

## KiCad validation (no GUI needed)

`kicad-cli` 10.0.4 is at
`/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`. Validate edits without
opening the app:
```bash
kicad-cli sch erc     --output /tmp/erc.rpt  PCB/Vac_Pump_Controller/Vac_Pump_Controller.kicad_sch
kicad-cli sch export netlist --output /tmp/net.net PCB/Vac_Pump_Controller/Vac_Pump_Controller.kicad_sch
```
Close KiCad before editing `.kicad_sch` by hand (watch for `~*.lck` lock files).

## Firmware architecture (main.c)

- **Control loop:** a `k_timer` fires every 10 ms → workqueue `vac_work_handler`:
  reads the ADC, filters (`vac_filter_push`), writes `volatile` globals
  `g_vac_val` (vacuum, inHg×10) and `g_vac_raw` (filtered ADC counts), and drives
  the pump GPIO against `app_red`/`app_grn`.
- **UI loop:** `main()`'s `while(1)` runs LVGL ~10 Hz, `ui_update()` pushes the
  globals into the SquareLine widgets, then `lv_timer_handler()` flushes to the
  GC9A01.

### Sensor front-end + calibration (the numbers)
- Chain: MPXV4115V (Vout, 5 V from CN7-18) → **3.94k / 6.8k** divider →
  `PA0/ADC1_IN5` = **CN7-34**. Sensor GND → **CN7-19** (LCD GND uses CN7-20).
- ADC: 12-bit, VREF+ = VDDA. The driver only accepts `ADC_REF_INTERNAL`
  (= VREF+); there is **no** internal-2.5 V reference to change.
- **Two-point calibration** lives in `vac_work_handler` (constants
  `cnt_atm`/`cnt_ref`/`ref_x10`): `4044 counts → 0 inHg`, `709 counts → 29 inHg`,
  linear `vac(inHg*10) = 290*(4044 - filt)/3335`. Both endpoints are measured, so
  the divider ratio and VREF cancel — recalibrate by re-measuring the counts, not
  by modeling the electronics.
- Needle uses `2700 - 9*v` (0 inHg at dial start, 30 under vacuum); the needle
  image is 135° offset from the setpoint-bug frame (`1350 - 9*v`).

## Gotchas / conventions

- `CONFIG_LV_COLOR_16_SWAP=y` is required in `prj.conf` — the GC9A01 wants
  big-endian RGB565; without it the display has a purplish/green cast.
- Zephyr Kconfig int/string values must **not** have trailing inline comments.
- Commit only when asked. This is a solo project — history is linear on `main`;
  milestone tags (e.g. `EVT1` = Engineering Validation Test 1) go on `main`.
