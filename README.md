# WeAct 1.54" BWRY e-paper demos

"Hello World" in black, red and yellow on a **WeAct 1.54" four-colour
e-paper panel**, driven from two very different microcontrollers. Both
sketches produce the same screen; what differs is how they get there.

| Sketch | Board | Notes |
|---|---|---|
| [`EpaperHelloWorld`](EpaperHelloWorld/EpaperHelloWorld.ino) | nRF52840 SuperMini | Full 10 kB frame in RAM, one pass |
| [`EpaperHelloWorld_ProMini`](EpaperHelloWorld_ProMini/EpaperHelloWorld_ProMini.ino) | Arduino Pro Mini (ATmega328P) | 2 kB SRAM, so 20 banded passes |
| [`EpaperWiringCheck`](EpaperWiringCheck/EpaperWiringCheck.ino) | either | Run this when nothing happens — see below |

## When nothing happens

`EpaperWiringCheck` walks five stages and reports each before the next can
mask it: LED blink (is the sketch running?), serial banner (is the link real?),
BUSY idle level (is the panel powered and BUSY connected?), reset response
(does the panel react to RES?), and a solid red fill with no fonts involved.
Read the first stage that misbehaves and ignore everything after it.

Both need **GxEPD2 ≥ 1.6.6** (Library Manager) — that release added this
panel — plus Adafruit GFX, which comes with it.

## The panel

200×200, black / white / red / yellow, driven as Good Display
**GDEM0154F51H** (JD79660 controller), GxEPD2 class
`GxEPD2_154c_GDEM0154F51H`.

**Check the resolution before blaming your wiring.** Two 1.54" four-colour
panels are in circulation: the 200×200 GDEM0154F51H these sketches drive, and
a 152×152 GDEY0154F51 (JD79661) that GxEPD2 1.6.9 does not support at all. A
152×152 panel still compiles and still gets clocked data — it just comes back
blank or scrambled.

## Power and logic levels

From WeAct's own schematic (`Hardware/WeAct-EpaperModule SchDoc.pdf` in
[WeActStudio/WeActStudio.EpaperModule](https://github.com/WeActStudio/WeActStudio.EpaperModule)):

- **VCC feeds an ME6216A33 LDO.** The header takes 3.3 V or 5 V and the panel
  always runs from the regulated 3.3 V rail. Supply voltage is not the worry.
- **The signals have no level shifter.** BUSY / RES / D-C / CS / SCL / SDA
  reach the panel through 100 Ω series resistors (R3–R8) and nothing else.

So a **3.3 V microcontroller wires straight through**. A **5 V one** (a 5 V/16 MHz
Pro Mini, an Uno) drives 5 V logic into 3.3 V panel inputs, current-limited to
about 11 mA by those resistors — it often appears to work and it is still out
of spec, so shift the five lines the MCU drives. BUSY is the one line the panel
drives and needs no help: 3.3 V clears a 5 V part's 3.0 V input threshold.

The module also has SB1/SB2 solder bridges selecting 3-wire vs 4-wire SPI.
GxEPD2 needs 4-wire (the D/C pin), which is how the boards ship.

**Pin order** is the same on both the 2.54 mm header (P1) and the 1.25 mm wafer
connector (J2), per WeAct's schematic:

`1 BUSY · 2 RES · 3 D/C · 4 CS · 5 SCL · 6 SDA · 7 GND · 8 VCC`

Still confirm against your module's silkscreen before powering it — swapping
GND and VCC is the one mistake that kills the board.

## nRF52840 SuperMini

Sold since ~2023 as "ProMicro nRF52840" — a nice!nano-compatible clone with
the Adafruit UF2 bootloader.

1. Preferences → *Additional Boards Manager URLs*:
   `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`
2. Boards Manager → **Adafruit nRF52 Boards**
3. Tools → Board → **Nordic nRF52840 DK**

**Why the DK entry.** That package ships no SuperMini/nice!nano variant, and
the DK variant (`pca10056`) is the honest stand-in for two reasons: it numbers
Arduino pins exactly like the chip's own GPIOs — pin `n` is `P0.n`, `P1.n` is
`32 + n` — so the P-numbers on the silkscreen translate straight across; and it
uploads by nrfutil serial DFU after a 1200 bps touch, which is what this
board's bootloader speaks. Double-tap RST if no port appears. The DK's own LED
and button pins don't exist on the SuperMini, so `LED_BUILTIN` is meaningless
here; the sketch reports over USB serial instead.

| module pin | SuperMini pad | Arduino pin no. |
|---|---|---|
| BUSY | P0.17 | 17 |
| RES | P0.20 | 20 |
| D/C | P0.22 | 22 |
| CS | P0.24 | 24 |
| SCL (clock) | P1.00 | 32 |
| SDA (MOSI) | P0.11 | 11 |
| GND / VCC | GND / 3V3 | — |

MISO goes nowhere (the panel never talks back), but the nRF52 SPI peripheral
needs a pin assigned, so P1.04 is parked there and left unconnected. P0.09 and
P0.10 are avoided throughout: they default to NFC antenna function.

### Deep sleep (nRF52840 only)

`EpaperHelloWorld` finishes by putting both halves to sleep: `display.hibernate()`
sends the panel's deep-sleep command (`0x07` / `0xA5`), and then
`SLEEP_MCU_AFTER_DRAW` puts the nRF52840 into **System OFF**, its deepest stop —
single-digit microamps, RAM lost, waking restarts from `setup()`. The panel
holds its image with no power, so an idle picture frame costs essentially
nothing to keep displayed.

Set `WAKE_PIN` to a pad (e.g. `P0(6)`) to wake on that pad being pulled to GND;
leave it `-1` and RST or a power cycle is the only way back. Set
`SLEEP_MCU_AFTER_DRAW` to `0` to keep the MCU awake and idling.

Three things worth knowing:

- **It refuses to sleep while USB is attached.** Entering System OFF with VBUS
  present tends to wake the chip straight back up, which looks exactly like a
  boot loop. The sketch checks `USBREGSTATUS` and stays awake on USB, so you
  only get real sleep on battery — by design, not by accident.
- **`display.end()` is deliberately not called.** It switches CS, DC and RST to
  inputs, floating the sleeping panel's control lines. GPIO output levels are
  latched through System OFF, so leaving them driven is simpler and safer.
- **The LDO on the display module probably dominates your power budget**, not
  the sleeping MCU or panel. Measure the whole board before optimising either.

Waking the panel needs nothing special: the driver checks `_hibernating` at the
top of `_InitDisplay()` and pulses RES for you, so the next draw just works.

## Arduino Pro Mini (ATmega328P)

Tools → Board → **Arduino Pro or Pro Mini**, then Tools → Processor →
**ATmega328P (3.3V, 8 MHz)**. Prefer the 3.3 V variant — see logic levels
above. The Processor entry must match the board you own or the upload fails
and every delay is off by 2×. There is no USB: upload through an FTDI/CH340
adapter with DTR wired to RESET.

| module pin | Pro Mini |
|---|---|
| BUSY | A3 |
| RES | A2 |
| D/C | A1 |
| CS | D9 |
| SCL (clock) | D13 |
| SDA (MOSI) | D11 |
| GND / VCC | GND / VCC |

D11 and D13 are not a free choice — they're MOSI and SCK on the ATmega328P's
hardware SPI. D12 (MISO) is unused, and `SPI.begin()` forces D10 to an output,
so D10 isn't available as an input.

**Why this sketch is paged.** A 200×200 four-colour frame is 2 bits/pixel =
10,000 bytes; the ATmega328P has 2,048. GxEPD2 renders it in horizontal bands
instead, each band streamed into the panel's own memory, with one refresh after
the last. Measured cost of that trade:

| PAGE_HEIGHT | buffer | display object | passes |
|---|---|---|---|
| 8 | 400 B | 560 B | 25 |
| **10** | **500 B** | **664 B** | **20** ← the sketch |
| 20 | 1000 B | 1160 B | 10 |
| 200 | — | 10160 B | 1 (needs a bigger MCU) |

The consequence: **your drawing code runs once per pass**, 20 times over. Keep
what you add inside the `do/while` cheap, and keep string literals in `F()`.

Serial is at 9600 rather than 115200: at 8 MHz the baud divisor puts 115200
about 3.5 % out, which is where a link starts dropping characters.

## Reading GxEPD2's diagnostics

Passing a baud rate to `display.init()` makes GxEPD2 print a timing line per
step. **Those numbers are microseconds**, and comparing them against the
driver's own measured figures is the fastest way to find a wiring fault:

| step | healthy | meaning if far shorter |
|---|---|---|
| `_PowerOn` | ~157,485 µs | BUSY never seen asserted |
| `_refresh` | ~21,428,295 µs | ditto — and the refresh gets aborted |
| `_PowerOff` | ~81,338 µs | ditto |

**An unread BUSY line does not just skip a wait — it blanks the screen.**
`_waitWhileBusy()` returns immediately, the library believes a 21-second
refresh finished in milliseconds, and the `hibernate()` that follows puts the
panel into deep sleep part-way through painting. The result looks exactly like
"the sketch does nothing", while the serial log claims success.

Both sketches now catch this themselves: a refresh reported in under 10
seconds prints a warning naming BUSY as the suspect.

To test it, set `USE_BUSY_PIN` to `0` near the top of the sketch. GxEPD2 then
ignores the pin and uses the datasheet's fixed delays instead (25 s for a full
refresh), so the panel is left alone to finish. If the image appears, BUSY is
your fault to fix; if it stays blank, BUSY is exonerated and the problem is on
CS / D/C / SDA / SCL or the panel type.

## Refresh timing

A four-colour full refresh takes **about 25 seconds**, flashing through its
colour passes throughout. That's normal for BWRY e-paper, not a hang. Both
sketches draw once and hibernate — Good Display asks for ~180 s between full
refreshes on colour panels.

**Using fewer colours does not make it faster.** The refresh is one command
(`0x12`) after which the host just waits on BUSY; the controller runs a fixed
waveform out of its OTP table, and the passes that move red and yellow pigment
happen whether or not any pixel ends up red or yellow. GxEPD2 declares
`full_refresh_time` and `partial_refresh_time` both at 25000 ms. Data transfer
is irrelevant either way — 10 kB at 4 MHz is ~20 ms of the 25 s.

If update speed matters, change the panel, not the code: the B/W 1.54" 200×200
module (SSD1681, `GxEPD2_154_D67`) is the same size, wires identically, and
does ~2.6 s full / ~0.5 s partial refreshes with genuine fast partial update.
Colour on e-paper is bought with time.
