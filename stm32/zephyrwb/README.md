# CNC Vacuum Pump Controller — Zephyr application (STM32WB55)

**Active** Zephyr port, targeting the **P-NUCLEO-WB55** (MB1355, board
`nucleo_wb55rg`). Replaces the abandoned ESP32-S3 attempt (see
[../../zephyr/](../../zephyr/)). Full background: [../../migration.md](../../migration.md).

## Why this board is nicer than the ESP32-S3
- **One USB cable** does *both* flashing (ST-LINK/SWD) and console (USART1 VCP) —
  no download-mode button dance, no USB-Serial-JTAG hacks, no dual-port confusion.
- **Real onboard LEDs** — the green LD2 is the firmware heartbeat (no WS2812 needed).
- **No pin traps** — no non-existent GPIOs, no flash/PSRAM-shared pins. Wiring goes to
  the **ST Morpho** headers (CN7/CN10), which expose every MCU pin.

## Build & flash

```bash
cd /Users/robert/Documents/dev-env/zephyresp32s3
source .venv/bin/activate
west build -b nucleo_wb55rg /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump/stm32/zephyrwb
west flash
```

Host tools: `cmake`, `ninja`, `dtc` (`brew install cmake ninja dtc`). Flashing uses the
onboard ST-LINK via OpenOCD (bundled with the Zephyr SDK).

### Console / monitor
Console is USART1 on the ST-LINK Virtual COM Port — same cable as flashing:

```bash
west build -t run              # not used; use a serial terminal:
ls /dev/cu.usbmodem*           # the ST-LINK VCP
screen /dev/cu.usbmodemXXXX 115200     # exit: Ctrl-A k y
```

A healthy boot prints: `CNC Vac Pump Controller up (LVGL 9.5.0)` and the green LD2
blinks ~5x/sec.

## Simulate mode (no sensor required)
Exercise the display, needle, setpoint bugs, and pump-relay logic before the analog
front-end is wired. Enable `CONFIG_VAC_SIM` — the control loop feeds a synthetic
triangle sweep (0 → 30.0 → 0 inHg, ~2 s per direction) instead of the ADC:

```bash
west build -b nucleo_wb55rg <this dir> -- -DCONFIG_VAC_SIM=y
west flash
```
Or uncomment `# CONFIG_VAC_SIM=y` in `prj.conf`. Boot logs `VAC_SIM enabled` and the
on-screen value continuously sweeps while the pump relay toggles at the setpoints.

## Status: what works today
- Boots, brings up the GC9A01 over MIPI-DBI-SPI (Arduino SPI1), backlight on.
- 10 ms ADC control loop with median+average filter → pump relay switching.
- Rotary-encoder thread with the two-bug setpoint logic.
- Green-LED heartbeat.
- **Placeholder** LVGL 9 UI (value + setpoint labels).

## TODO to reach feature parity
1. **Re-export the SquareLine UI for LVGL 9** → step-by-step in
   [SQUARELINE_LVGL9_EXPORT.md](SQUARELINE_LVGL9_EXPORT.md). Exports from
   `source/SquareLine_VacDisp/Sm_Disp.spj` into `src/ui/`, then enable the `file(GLOB…)`
   block in `CMakeLists.txt` and replace `ui_placeholder()`/`ui_update()` in `main.c`
   with `ui_init()` + the `ui_*` widget calls. The 8.3 export in
   `../../source/Vac_Gauge_V2/src/ui_lvgl/` will **not** compile against LVGL 9.
2. **Implement the alarm relay** (the original never did — see `main.c` TODO).
3. Optionally move the polling encoder thread to `gpio-qdec` + `gpio-keys` feeding an
   LVGL encoder indev.

## Wiring — ST Morpho headers (nucleo_wb55rg)

Readable version with color swatches: **[WIRING.html](WIRING.html)**. "Silk" = the label
printed on the GC9A01 module; "Morpho" = the CN7/CN10 connector-pin you land the wire on;
"Wire" = the JXI pigtail color. The Arduino-header alias (Dx/Ax) is kept as a cross-reference
— it's the *same* MCU pin, so nothing in the overlay or firmware changes.

| Function            | Module silk | Morpho (CN-pin) | STM32 pin      | (Arduino) | Wire  |
|---------------------|:-----------:|:---------------:|:--------------:|:---------:|:-----:|
| Vac sensor (ADC)    | —           | CN7-34          | PA0 (ADC1_IN5) | A3        |       |
| Pump relay (act-low)| —           | CN7-36          | PC3            | A4        |       |
| Alarm relay (act-low)| —          | CN7-38          | PC2            | A5        |       |
| Heartbeat LED       | —           | onboard LD2     | PB0 (green)    | —         |       |
| Encoder A           | —           | CN10-33         | PC6            | D2        |       |
| Encoder B           | —           | CN10-31         | PA10           | D3        |       |
| Encoder switch      | —           | CN10-29         | PC10           | D4        |       |
| LCD SCLK            | **SCL**     | CN10-11         | PA5            | D13       | Wht   |
| LCD MOSI (Din)      | **SDA**     | CN10-15         | PA7            | D11       | Yel   |
| LCD CS              | **CS**      | CN10-17         | PA4            | D10       | Blu   |
| LCD DC              | **DC**      | CN10-19         | PA9            | D9        | Grn   |
| LCD RST             | **RES**     | CN10-21         | PC12           | D8        | Org   |
| LCD backlight       | **BLK**     | CN7-32          | PA1            | A2        | Pur   |
| LCD VCC             | **VCC**     | CN7-16 (**3V3**)| **3V3**        | —         | Red   |
| LCD GND             | **GND**     | CN7-20 (GND)    | GND            | —         | Blk   |
| Sensor Vs (supply)  | —           | CN7-18 (**5V**) | 5 V            | —         |       |

> Encoder A/B/SW use internal pull-ups (idle high, switch to GND). The GC9A01 is a
> **3.3 V** module — power VCC from **3V3**, never 5 V. MISO is intentionally unused
> (write-only panel), so CS is a GPIO and the SPI bus is SCK+MOSI only.
>
> **Vac sensor (CN7-34 / PA0):** MPXV4115V (powered from **5V**, CN7-18) → R1 5k / R2 10k
> divider → ~0.13–3.07 V into the ADC. Divider + cap details and schematic in
> [WIRING.html](WIRING.html). The firmware's raw→inHg map still needs STM32 calibration
> (see the `TODO` in `src/main.c`).

### Cautions when using the ST Morpho headers
The Morpho headers bring out **all** MCU pins, so they're less foolproof than the Arduino
header. Watch for:
- **Count carefully.** CN7 and CN10 are each 2×19 = 38 pins, odd column (1,3,5…) on one
  side and even (2,4,6…) on the other. Pin 1 is silk-marked; miscounting by one row is the
  most common wiring mistake. Analog + power (A0–A5, 5 V, 3V3) live on **CN7**; the digital
  D0–D15 pins live on **CN10**.
- **Don't touch the debug/system pins** the Arduino header used to hide: **PA13/PA14 (SWD)**,
  **NRST**, **PH3/BOOT0**. Wiring to these can break flashing and the console.
- **Don't touch PA2/PA3** (CN10-35 D1 / CN10-37 D0) — they're the ST-LINK Virtual COM Port
  (console). Using them as GPIO kills your serial monitor.
- **Solder-bridge-shared pins:** three of our signals share a Morpho pad with an alternate
  MCU pin — **PA7** (CN10-15, shared w/ PA10), **PA4** (CN10-17, shared w/ PB10), and **PA1**
  (CN7-32, shared w/ PB6). The board ships with the default (the pin we use), so **leave the
  solder bridges as-is** — no rework needed.
- Electrically, plugging into Morpho vs the Arduino header is identical (same MCU pin), so
  the devicetree overlay and firmware are unchanged.
