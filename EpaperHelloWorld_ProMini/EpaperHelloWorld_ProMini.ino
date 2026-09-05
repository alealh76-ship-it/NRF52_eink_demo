/*
 * EpaperHelloWorld_ProMini — three-colour "Hello World" on a WeAct 1.54" BWRY
 * ---------------------------------------------------------------------------
 * Board : Arduino Pro Mini (ATmega328P) — 2 kB SRAM, 32 kB flash
 *
 *   Arduino IDE setup
 *     Tools -> Board -> Arduino AVR Boards -> "Arduino Pro or Pro Mini"
 *     Tools -> Processor -> "ATmega328P (3.3V, 8 MHz)"   <- see the volts note
 *     Tools -> Port -> your USB-serial adapter
 *   The Pro Mini has no USB. Upload through an FTDI/CH340 adapter with DTR
 *   wired to RESET, otherwise you have to hit reset by hand at exactly the
 *   right moment. The Processor menu entry must match the board you actually
 *   own: pick the wrong one and the upload fails or every delay is off by 2x.
 *
 * Libs  : GxEPD2 >= 1.6.6   (Library Manager — 1.6.6 is where this panel landed)
 *         Adafruit GFX Library (installed as a GxEPD2 dependency)
 *
 * Display: WeAct 1.54" four-colour e-paper, black / white / red / yellow,
 *          200x200 px, driven as Good Display GDEM0154F51H (JD79660).
 *
 * ===========================================================================
 * VOLTS — read this before connecting anything
 * ===========================================================================
 * Use a 3.3 V / 8 MHz Pro Mini if you have the choice. Then every line below
 * is a direct connection and there is nothing further to think about.
 *
 * On a 5 V / 16 MHz Pro Mini the module's *supply* is fine but its *signals*
 * are not. From WeAct's own schematic (WeAct-EpaperModule SchDoc):
 *   - VCC feeds an ME6216A33 LDO, so the header happily takes 3.3 V or 5 V
 *     and the panel itself always runs from the regulated 3.3 V rail.
 *   - BUSY / RES / D/C / CS / SCL / SDA reach the panel through 100 R series
 *     resistors (R3-R8) and NOTHING ELSE. There is no level shifter on board.
 * So a 5 V board drives 5 V logic into 3.3 V panel inputs, current-limited to
 * about 11 mA by those resistors. It often appears to work and it is still
 * out of spec — put a level shifter (or a divider) on the five lines the AVR
 * drives. BUSY is the one line the panel drives, and it needs no help: 3.3 V
 * clears a 5 V AVR's 3.0 V input threshold.
 *
 * The module also has SB1/SB2 solder bridges selecting 3-wire vs 4-wire SPI.
 * GxEPD2 needs 4-wire (the D/C pin), which is how the boards ship. Only worry
 * about this if someone has been at yours with an iron.
 *
 * Wiring. WeAct's schematic gives both the 2.54 mm header (P1) and the 1.25 mm
 * wafer connector (J2) the same order, pin 1 to pin 8:
 *     1 BUSY   2 RES   3 D/C   4 CS   5 SCL   6 SDA   7 GND   8 VCC
 * Still confirm against your own module's silkscreen before powering it:
 * swapping GND and VCC is the one mistake that kills the board.
 *
 *     module pin | Pro Mini | note
 *     -----------+----------+----------------------------------------------
 *     BUSY       | A3       | panel drives this
 *     RES        | A2       | reset, active low
 *     D/C        | A1       | data / command select
 *     CS         | D9       | chip select, active low
 *     SCL        | D13      | SPI clock  — fixed by the AVR's hardware SPI
 *     SDA        | D11      | SPI data in — likewise fixed (MOSI)
 *     GND        | GND      |
 *     VCC        | VCC      | 3.3 V board: VCC. 5 V board: VCC or RAW, the
 *                |          | on-board LDO regulates it either way.
 *
 *   SDA/SCL here are SPI, not I2C — the names come from the panel datasheet.
 *   D11 and D13 are not a free choice: they are MOSI and SCK on the ATmega328P
 *   and the hardware SPI peripheral is wired to them. D12 (MISO) stays unused,
 *   and note that SPI.begin() forces D10 to an output, so D10 is not available
 *   as an input while this sketch runs.
 *
 * ===========================================================================
 * WHY THIS SKETCH IS BUILT THE WAY IT IS — 2 kB of RAM
 * ===========================================================================
 * A 200x200 four-colour frame is 2 bits per pixel = 10,000 bytes. The
 * ATmega328P has 2,048. The frame cannot be held in RAM, so GxEPD2 renders it
 * in horizontal bands: PAGE_HEIGHT rows at a time into a small buffer, each
 * band shipped straight into the panel's own memory, and one refresh at the
 * end once all bands have landed.
 *
 * PAGE_HEIGHT below is that trade, measured (sizeof on the instantiated
 * template; AVR is a little smaller still, its pointers being 2 bytes):
 *
 *     PAGE_HEIGHT | buffer | whole display object | passes
 *     ------------+--------+----------------------+-------
 *          8      |  400 B |        560 B         |   25
 *         10      |  500 B |        664 B         |   20   <- this sketch
 *         20      | 1000 B |       1160 B         |   10
 *         25      | 1250 B |       1408 B         |    8
 *        200      |    —   |      10160 B         |    1   (needs a bigger MCU)
 *
 * At 10 rows the display object costs about a third of SRAM and leaves the
 * rest for the stack, the Serial buffers and everything you add later. The
 * cost of more passes is that everything in the drawing loop — every
 * fillScreen, every character, every bounds calculation — runs once PER PASS,
 * 20 times over. On an 8 MHz AVR that is still a fraction of a second, and it
 * is invisible beside the panel's own 25-second refresh, so the small buffer
 * is clearly the right end of this trade.
 *
 * Keep new drawing code inside the do/while and keep it cheap. And keep string
 * literals wrapped in F() so they stay in flash: a handful of forgotten
 * strings is enough to exhaust what little RAM is left.
 *
 * Timing: a four-colour full refresh takes about 25 seconds, flashing through
 *   its colour passes the whole time. That is normal for BWRY e-paper, not a
 *   hang. The sketch draws once and stops — Good Display asks for ~180 s
 *   between full refreshes on colour panels.
 */

#include <GxEPD2_4C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPI.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// ---- pins (see the wiring table above) ------------------------------------
#define EPD_BUSY  A3
#define EPD_RST   A2
#define EPD_DC    A1
#define EPD_CS    9
// SDA -> D11 (MOSI) and SCL -> D13 (SCK) are fixed by the ATmega328P's
// hardware SPI and are not named here; the SPI library owns them.

// Set to 0 to ignore the BUSY line entirely and use the datasheet's fixed
// delays instead (GxEPD2 does this whenever the busy pin is -1). Slower and
// less robust, but immune to a broken, floating or miswired BUSY connection --
// which is worth knowing, because an unread BUSY line does not merely slow
// things down: the library then believes each step finished instantly and
// puts the panel to sleep part-way through its refresh, leaving a blank
// screen. Flip this to 0 to find out whether BUSY is your problem.
#define USE_BUSY_PIN 1

// ---- MCU deep sleep --------------------------------------------------------
// 1 -> after drawing, put the ATmega328P into SLEEP_MODE_PWR_DOWN, its deepest
//      stop: all clocks halted, sub-microamp on the bare chip.
// 0 -> stay awake and idle in loop().
//
// READ THIS BEFORE EXPECTING A LOW NUMBER. On a stock Pro Mini the chip is not
// what drains your battery. The board's power LED burns roughly a milliamp,
// and the on-board regulator adds its own quiescent draw -- together some
// thousands of times more than the sleeping ATmega328P. Sleeping the MCU on an
// unmodified board buys you almost nothing measurable. To make this setting
// worth having: remove the power LED, and either remove the regulator or feed
// regulated power straight to VCC instead of RAW. Then the display module's
// own LDO becomes the next thing to argue with.
#define SLEEP_MCU_AFTER_DRAW 1

// Pin that wakes the board, or -1 for none (then only RESET or a power cycle
// brings it back, which is the closest match to the nRF52 sketch's behaviour).
// Must be D2 (INT0) or D3 (INT1): waking from power-down needs a LOW-LEVEL
// interrupt, because edge detection runs off the I/O clock and that clock is
// stopped. Both pins are free in this wiring.
#define WAKE_PIN (-1)   // e.g. 2

// ---- paged rendering ------------------------------------------------------
// Rows per pass. 10 rows -> 500 bytes of buffer, 20 passes over the screen.
// Raising this trades SRAM for fewer passes: buffer bytes = PAGE_HEIGHT * 50,
// plus roughly 100 bytes of bookkeeping. See the table in the header.
#define PAGE_HEIGHT 10

GxEPD2_4C<GxEPD2_154c_GDEM0154F51H, PAGE_HEIGHT> display(
  GxEPD2_154c_GDEM0154F51H(EPD_CS, EPD_DC, EPD_RST, USE_BUSY_PIN ? EPD_BUSY : -1));

// Fail at compile time rather than with mystifying runtime corruption if
// someone raises PAGE_HEIGHT past what an ATmega328P can spare.
static_assert(PAGE_HEIGHT * (200 / 4) <= 900,
              "PAGE_HEIGHT too large: the page buffer will not leave the "
              "ATmega328P enough SRAM for the stack");

static const char HELLO[] = "Hello World";

// Baselines for the three lines, in pixels from the top of the 200 px panel.
static const int16_t LINE_BLACK_Y  = 78;
static const int16_t LINE_RED_Y    = 118;
static const int16_t LINE_YELLOW_Y = 158;

// Draw text horizontally centred, with `y` as the baseline.
static void drawCentred(const char* text, int16_t y, uint16_t colour)
{
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((display.width() - int16_t(w)) / 2 - x1, y);
  display.setTextColor(colour);
  display.print(text);
}

// Same, but on a filled bar. Yellow ink on white paper is genuinely faint on
// these panels, so the yellow line gets a black bar behind it — otherwise the
// third colour reads as "nothing printed" from any distance.
static void drawCentredOnBar(const char* text, int16_t y, uint16_t colour, uint16_t barColour)
{
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  const int16_t x = (display.width() - int16_t(w)) / 2 - x1;
  display.fillRoundRect(x + x1 - 8, y1 - 6, int16_t(w) + 16, int16_t(h) + 12, 6, barColour);
  display.setCursor(x, y);
  display.setTextColor(colour);
  display.print(text);
}

static void drawHelloWorld()
{
  display.setFullWindow();
  display.firstPage();
  // Everything in here runs once per band — 20 times over. GxEPD2 clips each
  // pass to the band being rendered, so the code is written as if it were
  // drawing the whole screen.
  do
  {
    display.fillScreen(GxEPD_WHITE);

    // A red double frame, so the border proves the red channel independently
    // of the text.
    display.drawRect(0, 0, display.width(), display.height(), GxEPD_RED);
    display.drawRect(1, 1, display.width() - 2, display.height() - 2, GxEPD_RED);

    // Caption in the built-in 5x7 font.
    display.setFont(NULL);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    {
      const char* caption = "WeAct 1.54\" BWRY";
      int16_t  x1, y1;
      uint16_t w, h;
      display.getTextBounds(caption, 0, 0, &x1, &y1, &w, &h);
      display.setCursor((display.width() - int16_t(w)) / 2, 26);
      display.print(caption);
    }

    // The same words in each of the three inks the panel can lay down.
    display.setFont(&FreeMonoBold9pt7b);
    drawCentred(HELLO, LINE_BLACK_Y, GxEPD_BLACK);
    drawCentred(HELLO, LINE_RED_Y, GxEPD_RED);
    drawCentredOnBar(HELLO, LINE_YELLOW_Y, GxEPD_YELLOW, GxEPD_BLACK);
  }
  while (display.nextPage());
}

#if SLEEP_MCU_AFTER_DRAW

#if WAKE_PIN >= 0
static void wakeISR()
{
  // Detach immediately: the interrupt is level-triggered, so it would fire
  // continuously for as long as the pin is held low.
  detachInterrupt(digitalPinToInterrupt(WAKE_PIN));
}
#endif

static void mcuPowerDown()
{
  Serial.println(F("entering power-down"));
  Serial.flush();   // the USART stops the moment the clocks do
  delay(20);

#if WAKE_PIN >= 0
  pinMode(WAKE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeISR, LOW);
#endif

  // The ADC keeps its analogue front end powered even in power-down unless
  // ADEN is cleared, and that alone is worth a few hundred microamps -- far
  // more than the sleeping core. Note there is no point calling
  // power_all_disable() here: the PRR registers save current in active and
  // idle modes, but power-down has already stopped every clock.
  ADCSRA &= ~_BV(ADEN);

  // Control pins are left driven on purpose, exactly as in the nRF52 sketch:
  // pin states are retained through power-down, so the sleeping panel's CS, DC
  // and RES stay held rather than floating. Do not call display.end() here.

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  // Brown-out detection costs ~20 uA and is pointless while asleep. The
  // hardware only honours this within a few cycles of sleep_cpu(), so it has
  // to sit here, between the disable and the sleep, with interrupts off.
  sleep_bod_disable();
  sei();
  sleep_cpu();

  // ---- Execution resumes HERE on wake, NOT at setup(). This is the one real
  // difference from the nRF52 sketch, where System OFF restarts the program.
  // With WAKE_PIN at -1 nothing can wake it, so control never gets this far.
  sleep_disable();
  ADCSRA |= _BV(ADEN);
  Serial.println(F("woken"));
}
#endif

void setup()
{
  // 9600 rather than something brisker: at 8 MHz the ATmega328P's baud
  // divisor puts 115200 about 3.5% out, which is where a serial link starts
  // dropping characters. This sketch prints four lines; slow is fine.
  Serial.begin(9600);
  Serial.println(F("EpaperHelloWorld_ProMini: start"));
  Serial.print(F("pages: "));
  Serial.println(display.pages());

  // 9600 -> GxEPD2 prints its own diagnostics to Serial.
  // true  -> full init (cold boot, not a wake from deep sleep).
  // 20    -> reset pulse in ms, the library default.
  // false -> drive RST normally rather than the pull-down trick some 5 V
  //          Waveshare boards need.
  display.init(9600, true, 20, false);
  display.setRotation(0);
  display.setTextWrap(false);

  Serial.println(F("refreshing; ~25 s"));
  const unsigned long refreshStart = millis();
  drawHelloWorld();
  const unsigned long elapsed = millis() - refreshStart;
  Serial.print(F("refresh done in "));
  Serial.print(elapsed);
  Serial.println(F(" ms"));
#if USE_BUSY_PIN
  if (elapsed < 10000)
  {
    // A four-colour refresh cannot finish this fast. If we get here, every
    // _waitWhileBusy returned instantly, which means the BUSY line was never
    // seen asserted -- so hibernate() below lands in the middle of the panel's
    // refresh and aborts it. A blank screen with a fast "done" is this bug.
    Serial.println(F("WARNING: too fast for a real refresh."));
    Serial.println(F("BUSY is not being read: check the A3 wire, or set"));
    Serial.println(F("USE_BUSY_PIN to 0 to fall back to fixed delays."));
  }
#endif

  // Park the panel in deep sleep. The image stays on screen with no power.
  display.hibernate();
  Serial.println(F("panel hibernating; image is retained"));

#if SLEEP_MCU_AFTER_DRAW
  // Panel asleep, so stop the MCU too. With WAKE_PIN at -1 this never returns.
  mcuPowerDown();
#endif
}

void loop()
{
  // Reached only with SLEEP_MCU_AFTER_DRAW at 0, or after a WAKE_PIN wake.
  // E-paper holds the last image without power, and this panel does not want
  // another full refresh for a few minutes anyway.
}
