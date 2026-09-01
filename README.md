# nRF52840 e-paper demos

Arduino IDE sketches driving e-paper panels from an **nRF52840 SuperMini**
(sold since ~2023 as "ProMicro nRF52840" — a nice!nano-compatible clone
carrying the Adafruit UF2 bootloader).

## Sketches

| Sketch | Panel | What it does |
|---|---|---|
| [`EpaperHelloWorld`](EpaperHelloWorld/EpaperHelloWorld.ino) | WeAct 1.54" BWRY, 200x200 | "Hello World" in black, red and yellow, inside a red frame |

## Arduino IDE setup

1. File → Preferences → *Additional Boards Manager URLs*:
   `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`
2. Tools → Board → Boards Manager → install **Adafruit nRF52 Boards**
3. Tools → Board → Adafruit nRF52 Boards → **Nordic nRF52840 DK**
4. Library Manager → **GxEPD2** (1.6.6 or newer) — Adafruit GFX comes with it

**Why the DK board entry.** That package ships no SuperMini/nice!nano variant,
and the DK variant (`pca10056`) is the honest stand-in for two reasons. It
numbers Arduino pins exactly like the chip's own GPIOs — Arduino pin `n` is
`P0.n`, and `P1.n` is `32 + n` — so the P-numbers silkscreened on the SuperMini
translate straight across. And it uploads by nrfutil serial DFU after a 1200 bps
touch, which is what this board's bootloader already speaks, so plain Upload
works. If no port appears, double-tap RST to force the bootloader.

The DK's own LED and button pins do not exist on the SuperMini, so
`LED_BUILTIN` is meaningless here; the sketches report status over USB serial.

## Wiring — WeAct 1.54" BWRY

The module's 8-pin header, in its printed order:

| module pin | SuperMini pad | Arduino pin no. |
|---|---|---|
| 1 BUSY | P0.17 | 17 |
| 2 RES | P0.20 | 20 |
| 3 D/C | P0.22 | 22 |
| 4 CS | P0.24 | 24 |
| 5 SCL (SPI clock) | P1.00 | 32 |
| 6 SDA (MOSI) | P0.11 | 11 |
| 7 GND | GND | — |
| 8 VCC | 3V3 | — |

`SDA`/`SCL` here are SPI, not I2C — the names come from the panel datasheet.
The display never talks back, so MISO goes nowhere; the nRF52 SPI peripheral
still needs a pin assigned, so P1.04 is parked there and left unconnected.

Both parts are 3.3 V, so this is a direct connection with no level shifting.
Power the module from **3V3**, never 5V/RAW — the WeAct board has no regulator.
P0.09 and P0.10 are avoided throughout: they default to NFC antenna function
and need a chip-level config change to become plain GPIO.

## Two things that look like wiring faults but aren't

**Resolution.** Two 1.54" four-colour panels are in circulation: the 200x200
GDEM0154F51H (JD79660) these sketches drive, and a 152x152 GDEY0154F51
(JD79661) that GxEPD2 1.6.9 does not support at all. A 152x152 panel still
compiles and still gets clocked data — it just comes back blank or scrambled.
Check the resolution in the vendor listing first.

**Speed.** A four-colour full refresh takes about 25 seconds, flashing through
its colour passes the whole time. That is normal for BWRY e-paper, not a hang.
Good Display asks for at least ~180 s between full refreshes on colour panels,
so these sketches draw once and hibernate rather than looping.
