# KiCad schematic swap kit — WeMos D1 Mini32 → P-NUCLEO-WB55 (ST Morpho)

**Goal:** replace the ESP32 (`WeMos_D1_Mini32`) symbol in
`PCB/Vac_Pump_Controller/Vac_Pump_Controller.kicad_sch` with the STM32
**P-NUCLEO-WB55** dev board, wired to its **ST Morpho** headers (CN7 / CN10) —
and change **nothing** in the peripheral subcircuits (LCD, relays, encoder, and
the R1/R2/C1/C3 vacuum-sensor divider are all correct as-is).

This is written for **KiCad 10.0.4** and assumes **no prior KiCad experience**.
It's schematic-only — the PCB file is empty, so there is no layout or footprint
work. Readable HTML version: **[SCHEMATIC_SWAP_WB55.html](SCHEMATIC_SWAP_WB55.html)**.

> **Why connectors instead of a board symbol?** Your custom parts (the WeMos
> symbol, the relay module, the LCD) live *embedded inside the schematic file* —
> there is no separate symbol-library file to add to. Rather than hand-build a
> new Nucleo symbol (a lot for a first KiCad task) or hand-edit the 12,000-line
> schematic text (risky), we represent the dev board the honest way: as its **two
> Morpho connectors**, using KiCad's **built-in** generic connector symbol. Nothing
> to install, and it matches how you'll physically wire it.

---

## How the connection actually works (read this once)

Your schematic is a **single flat sheet** that connects things by **net-label name**,
not by drawing a wire all the way across. Every peripheral already has its labels —
e.g. the LCD's chip-select wire carries a label `LCD_CS`, the encoder carries
`ENC_ChanA`, the divider tap carries `Vacuum_Sig`. **Any two points with the same
label name are the same electrical net.**

So the whole job is: **put a label with the *matching name* on the correct Morpho
pin.** You do **not** need to route wires from the connectors over to the LCD/relays.
Place the label, and the net is joined by name.

---

## The swap map (this is the whole spec)

Place a short wire off each listed pin and attach a **net label** with the **exact
name** in the last column. Names are **case-sensitive** — copy them verbatim.

### CN10 — digital header (place a `Conn_02x19_Odd_Even`, name it **CN10**)

| Morpho pin | MCU pin | Net label to place | Signal              |
|:----------:|:-------:|:-------------------|:--------------------|
| **11**     | PA5     | `LCD_SClk`         | LCD SPI clock       |
| **15**     | PA7     | `LCD_MOSI`         | LCD SPI data-in     |
| **17**     | PA4     | `LCD_CS`           | LCD chip-select     |
| **19**     | PA9     | `LCD_DC`           | LCD data/command    |
| **21**     | PC12    | `LCD_RST`          | LCD reset           |
| **33**     | PC6     | `ENC_ChanA`        | Encoder A           |
| **31**     | PA10    | `ENC_ChanB`        | Encoder B           |
| **29**     | PC10    | `ENC_PB`           | Encoder push-button |
| **20**     | GND     | *(GND power symbol)* | Ground            |

### CN7 — analog + power header (place a second `Conn_02x19_Odd_Even`, name it **CN7**)

| Morpho pin | MCU pin  | Net label to place | Signal                         |
|:----------:|:--------:|:-------------------|:-------------------------------|
| **34**     | PA0      | `Vacuum_Sig`       | Divider tap → ADC (ADC1_IN5)   |
| **36**     | PC3      | `Vac_Pump`         | Pump-relay control             |
| **38**     | PC2      | `Alarm`            | Alarm-relay control            |
| **32**     | PA1      | `LCD_BackLt`       | LCD backlight enable           |
| **16**     | 3V3      | `LCD_Vcc`          | 3.3 V rail → LCD VCC           |
| **18**     | 5V (E5V) | `5Vdc`             | 5 V rail (see power caution)   |
| **20**     | GND      | *(GND power symbol)* | Ground                       |

**Two nets that intentionally change:**
- **`LCD_MISO` is dropped.** The panel is write-only, so the MCU has no MISO pin
  here. Leave the LCD symbol's MISO pin **unconnected** (put a No-Connect flag on
  it — step 7) and delete the old floating `LCD_MISO` label if one remains.
- **`Vac_Sensor` stays on the divider only.** The MCU connects to **`Vacuum_Sig`**
  (the *divided* tap), never to the raw `Vac_Sensor` node. Nothing to do — just
  don't wire `Vac_Sensor` to the board.

Everything else in the schematic (R1 5k, R2 10k, C1 10µF, C3 100pF, relay module,
LCD module, encoder, AC/DC supply) is **untouched**.

---

## Step-by-step in KiCad 10 (beginner)

### 0. Back up first
In Terminal:
```bash
cd /Users/robert/Documents/shop-dev/shop-tools/CNC_Vac_Pump
git add -A && git commit -m "snapshot before WB55 schematic swap"
```
(Or just copy the `PCB/Vac_Pump_Controller/` folder in Finder.) If anything goes
sideways you can `git restore` or delete your copy.

### 1. Open the schematic
- Launch **KiCad 10**. **File → Open Project…** →
  `PCB/Vac_Pump_Controller/Vac_Pump_Controller.kicad_pro`.
- In the project window, double-click **Vac_Pump_Controller.kicad_sch** to open the
  **Schematic Editor**.

**Navigation basics:** scroll wheel = zoom, press-and-drag middle mouse = pan,
`Esc` = cancel the current tool, `Ctrl+Z` = undo. Hover a part and press the letter
keys below (KiCad "hover hotkeys" act on whatever the cursor is over).

### 2. Find and delete the old ESP32 symbol
- Locate the **WeMos D1 Mini32** block (a tall symbol with pins named `IO27`, `IO25`,
  `Txd`, `Rxd`, …). Use **View → Zoom to Fit** (`Home`) to see the whole sheet.
- Click its body once to select (it highlights), press **Delete**.
- You'll see short wire stubs and net labels where it used to connect — that's fine,
  leave the labels; we'll re-use those names. Delete any wire stub that now dangles
  in mid-air if it bothers you (click it, Delete) — optional.

### 3. Place the two Morpho connectors
- Press **A** (Add Symbol) — or the op-amp/AND toolbar icon on the right.
- In the search box type **`Conn_02x19_Odd_Even`**, pick it (library
  *Connector_Generic*), click **OK**, click an empty area to drop it.
- Press **A** again, place a **second** one nearby.
- These are 2×19 = 38-pin symbols. **Pin numbers 1–38 match the Morpho pin numbers
  exactly**: odd numbers (1,3,…37) down one column, even (2,4,…38) down the other.

### 4. Name the connectors
- Hover the first connector, press **E** (Edit Properties). Set **Reference** =
  `CN10`, **Value** = `ST_Morpho_CN10`. OK.
- Hover the second, press **E**. **Reference** = `CN7`, **Value** = `ST_Morpho_CN7`.

> If KiCad complains later about a missing footprint, ignore it — there's no PCB.
> (Advanced/optional: assign `PinHeader_2x19_P2.54mm_Vertical` if you ever lay out
> a board.)

### 5. Add a wire stub to each used pin
For every row in the two tables above:
- Press **W** (Add Wire), click on the connector **pin endpoint** (the little circle
  at the pin tip), drag out ~2 grid squares into open space, click to finish, `Esc`.
- Tip: zoom in close so you click the exact pin number you want. The pin number is
  printed next to each pin.

### 6. Attach the net labels
For every row (except the GND rows — those get a power symbol in step 8):
- Hover the free end of that pin's wire, press **L** (Add Label).
- Type the **exact** net name from the table (e.g. `LCD_SClk`), click to place it on
  the wire end. `Esc`.
- **Copy names exactly**, including capitalization: `LCD_SClk` (capital S, C, l… as
  shown), `ENC_ChanA`, `Vacuum_Sig`, `LCD_BackLt`, `Vac_Pump`, `LCD_Vcc`, `5Vdc`.
  A mistyped name silently makes a *new, disconnected* net — ERC (step 9) will catch
  it as an unconnected/undriven pin.

### 7. Handle the dropped MISO
- Find the **LCD module** symbol, locate its **MISO** pin (it used to carry the
  `LCD_MISO` label).
- Hover that pin, press **Q** to drop a **No-Connect** (×) flag on it. This tells
  KiCad "intentionally unused" so ERC stays quiet.
- If a lone `LCD_MISO` label is left floating from the old wiring, click it, Delete.

### 8. Add ground (and check power)
- Press **P** (Add Power Symbol), search **`GND`**, place it on a wire stub off
  **CN7 pin 20** and again off **CN10 pin 20**. GND power symbols all join the same
  `GND` net automatically.
- The `LCD_Vcc` label on **CN7-16** is your 3.3 V feed to the LCD; the `5Vdc` label on
  **CN7-18** is the 5 V rail. (See the **power caution** below — how the board is fed
  in the field vs on the bench.)

### 9. Run the Electrical Rules Checker (your safety net)
- **Inspect → Electrical Rules Checker → Run**.
- **Expected, harmless:** ~60 "Pin not connected" warnings on the *unused* connector
  pins (we only wire ~17 of the two connectors' 76). That's normal for a dev-board-as-module symbol —
  you can ignore them, or select each unused pin and press **Q** for No-Connect if you
  want a spotless report.
- **Must fix:** any warning naming **one of our nets** — e.g. "Label `LCD_SCl k` not
  connected" or "input pin not driven" on an LCD/encoder/relay net. That means a
  **name typo** or a label placed off the wire end. Re-open the map and correct the
  spelling/placement.

### 10. Save & (optional) eyeball a PDF
- **Ctrl+S**.
- **File → Plot…** → Format **PDF** → **Plot** to render a page you can scan for
  stray labels. (Or ask me to run `kicad-cli sch erc` / `kicad-cli sch export pdf`
  on the saved file to double-check it electrically.)

---

## Cautions

**Schematic**
- **Label names are exact and case-sensitive.** The single most common mistake.
  Match the tables character-for-character.
- **Connector pin number = Morpho pin number.** Go by the printed pin *number*
  (1–38), not by physical top/bottom position on screen. Odd column = pins
  1,3,…37; even column = 2,4,…38.
- **`LCD_MISO` is deliberately gone** (write-only panel). No-Connect it.
- **Unused connector-pin warnings are expected** — don't chase them.

**Hardware / power (matches the ST Morpho wiring doc)**
- **Powering the board in the field:** this product's 5 V comes from the external
  **HS-40005 AC/DC** supply (`5Vdc` rail), which you feed into the Nucleo's **5V (E5V)
  pin, CN7-18**. Feeding the Nucleo from external 5 V requires setting the board's
  power-source jumper to **E5V** (see UM2435 §6.4 "Power supply"). On the bench, the
  **ST-LINK USB** powers the board instead — don't drive external 5 V into CN7-18 and
  USB at the same time unless the jumper is set correctly, or you'll back-feed the
  ST-LINK.
- **3.3 V comes from the Nucleo's on-board regulator** (`LCD_Vcc`, CN7-16). Power the
  GC9A01 from this, never 5 V.
- **Don't wire the Morpho debug/system pins** the Arduino header used to hide —
  `PA13/PA14` (SWD), `NRST`, `PH3/BOOT0`, and the console `PA2/PA3` (D0/D1). They're
  exposed on the Morpho and easy to grab by accident.
- **Solder-bridge-shared pads** (`PA7`=CN10-15, `PA4`=CN10-17, `PA1`=CN7-32 each share
  a pad with an alternate MCU pin). The board defaults to the pin we use — leave the
  solder bridges alone. The schematic just records the default.

**PCB** — the `.kicad_pcb` is empty; this change is schematic-only. If you ever lay
out a real board, assign footprints to CN7/CN10 (and the other parts) first.

---

## Want me to do it instead?
I can attempt the edit directly on the `.kicad_sch` text and **validate it with
`kicad-cli sch erc`** before handing it back (Option A from earlier). The trade-off:
I can't *see* the result, so wire/label placement may be visually messy even when
electrically correct. Say the word and I'll produce a validated draft for you to open
and tidy.
