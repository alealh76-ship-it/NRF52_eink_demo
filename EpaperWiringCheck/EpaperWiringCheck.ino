/*
 * EpaperWiringCheck — find out WHERE a dead e-paper setup is dead
 * ---------------------------------------------------------------------------
 * Run this when the hello-world sketch "does nothing". It replaces one useless
 * symptom with a specific answer, in five stages, each reporting before the
 * next one can hide it:
 *
 *   1. LED blink       — is the sketch running at all?
 *   2. Serial banner   — is the serial link real?
 *   3. BUSY idle level — is the panel powered and the BUSY line connected?
 *   4. Reset response  — does the panel react to RES?
 *   5. Solid red fill  — does the whole chain work, with no fonts involved?
 *
 * Read the FIRST stage that misbehaves. Everything after it is noise.
 *
 * Board: as written, Arduino Pro Mini (ATmega328P) with the pins below.
 * For the nRF52840 SuperMini, swap the pin block for the one in comments and
 * add SPI.setPins(36, 32, 11) as the first line of setup().
 *
 * Libs: GxEPD2 >= 1.6.6, Adafruit GFX.
 */

#include <GxEPD2_4C.h>
#include <SPI.h>

// ---- Pro Mini pins ---------------------------------------------------------
#define EPD_BUSY  A3
#define EPD_RST   A2
#define EPD_DC    A1
#define EPD_CS    9
// SDA -> D11 (MOSI), SCL -> D13 (SCK): fixed by the ATmega328P hardware SPI.
//
// ---- nRF52840 SuperMini equivalents (Nordic nRF52840 DK board entry) -------
// #define EPD_BUSY  17   // P0.17      #define EPD_DC  22  // P0.22
// #define EPD_RST   20   // P0.20      #define EPD_CS  24  // P0.24
// SCL -> P1.00 (32), SDA -> P0.11 (11), and SPI.setPins(36, 32, 11) in setup().

#define PAGE_HEIGHT 10

GxEPD2_4C<GxEPD2_154c_GDEM0154F51H, PAGE_HEIGHT> display(
  GxEPD2_154c_GDEM0154F51H(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

#if defined(__AVR__)
static int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

// Sample BUSY and report how it sat. BUSY is ACTIVE LOW on this panel:
// HIGH means idle, LOW means the controller is working.
static uint8_t sampleBusy(uint8_t samples)
{
  uint8_t highs = 0;
  for (uint8_t i = 0; i < samples; i++)
  {
    if (digitalRead(EPD_BUSY) == HIGH) highs++;
    delay(5);
  }
  return highs;
}

void setup()
{
  // --- Stage 1: proof of life -----------------------------------------------
  // Do this BEFORE any SPI call: on a Pro Mini the on-board LED is D13, which
  // is also SCK, so once SPI.begin() runs the LED belongs to the SPI bus.
  // Six blinks here means power, bootloader and sketch are all fine. NO blink
  // means nothing is running: wrong board or Processor entry, a failed upload,
  // or no power — and no amount of display debugging will help until it does.
  pinMode(LED_BUILTIN, OUTPUT);
  for (uint8_t i = 0; i < 6; i++)
  {
    digitalWrite(LED_BUILTIN, (i & 1) ? HIGH : LOW);
    delay(120);
  }
  digitalWrite(LED_BUILTIN, LOW);

  // --- Stage 2: serial ------------------------------------------------------
  // 9600 because at 8 MHz the ATmega328P's baud divisor puts 115200 ~3.5% out.
  // Blinks but no text below => baud mismatch or the USB-serial adapter, NOT
  // the display.
  Serial.begin(9600);
  delay(200);
  Serial.println();
  Serial.println(F("=== EpaperWiringCheck ==="));
  Serial.print(F("F_CPU     : ")); Serial.println(F_CPU);
  Serial.print(F("pins BUSY/RST/DC/CS: "));
  Serial.print(EPD_BUSY); Serial.print('/');
  Serial.print(EPD_RST);  Serial.print('/');
  Serial.print(EPD_DC);   Serial.print('/');
  Serial.println(EPD_CS);
  Serial.print(F("pages     : ")); Serial.println(display.pages());
#if defined(__AVR__)
  Serial.print(F("free RAM  : ")); Serial.println(freeRam());
  // Under ~250 bytes here and the stack is living dangerously: lower
  // PAGE_HEIGHT before believing anything else this sketch tells you.
#endif

  // --- Stage 3: BUSY idle level --------------------------------------------
  pinMode(EPD_BUSY, INPUT);
  const uint8_t idleHighs = sampleBusy(20);
  Serial.print(F("BUSY idle : ")); Serial.print(idleHighs);
  Serial.println(F("/20 samples HIGH"));
  if (idleHighs == 0)
  {
    Serial.println(F("  -> stuck LOW. Panel unpowered, BUSY miswired, or the"));
    Serial.println(F("     controller is held busy. Check VCC and GND first."));
  }
  else if (idleHighs < 20)
  {
    Serial.println(F("  -> unstable. Likely a floating pin: BUSY not actually"));
    Serial.println(F("     connected, or a bad joint."));
  }
  else
  {
    Serial.println(F("  -> idle HIGH, as expected. NOTE: a disconnected pin"));
    Serial.println(F("     can also read HIGH, so stage 4 is the real test."));
  }

  // --- Stage 4: does the panel answer a reset? ------------------------------
  // Pulse RES low and watch BUSY. A live panel dips BUSY low while it restarts.
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, HIGH); delay(20);
  digitalWrite(EPD_RST, LOW);  delay(20);
  digitalWrite(EPD_RST, HIGH);

  bool sawBusy = false;
  unsigned long tStart = millis(), tFirstLow = 0, tReleased = 0;
  while (millis() - tStart < 3000)
  {
    if (!sawBusy && digitalRead(EPD_BUSY) == LOW)
    {
      sawBusy = true;
      tFirstLow = millis() - tStart;
    }
    else if (sawBusy && digitalRead(EPD_BUSY) == HIGH)
    {
      tReleased = millis() - tStart;
      break;
    }
  }
  if (sawBusy)
  {
    Serial.print(F("reset     : BUSY went low at ")); Serial.print(tFirstLow);
    Serial.print(F(" ms, released at ")); Serial.print(tReleased);
    Serial.println(F(" ms -> panel is alive and RES/BUSY are wired"));
  }
  else
  {
    Serial.println(F("reset     : BUSY never moved."));
    Serial.println(F("  -> suspect RES or BUSY wiring, or no panel power."));
    Serial.println(F("     Not conclusive on its own: some controllers stay"));
    Serial.println(F("     quiet until commanded. Stage 5 decides."));
  }

  // --- Stage 5: solid red, no fonts, no layout ------------------------------
  // If this fills the screen red, wiring and library are fine and the fault is
  // in the drawing code. If it stays blank while the timing below looks
  // healthy (~25 s), suspect the panel type: GxEPD2 has no support for the
  // 152x152 GDEY0154F51, and driving one produces exactly this - a well-behaved
  // refresh with nothing to show for it.
  Serial.println(F("init...   (library diagnostics follow)"));
  display.init(9600, true, 20, false);

  Serial.println(F("filling red; expect ~25 s"));
  const unsigned long t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_RED);
  }
  while (display.nextPage());
  const unsigned long elapsed = millis() - t0;

  Serial.print(F("refresh   : ")); Serial.print(elapsed); Serial.println(F(" ms"));
  if (elapsed < 3000)
  {
    Serial.println(F("  -> far too quick. The panel never really refreshed;"));
    Serial.println(F("     BUSY is probably not connected, so the library"));
    Serial.println(F("     never waited for it."));
  }
  else
  {
    Serial.println(F("  -> a plausible four-colour refresh."));
    Serial.println(F("     Red screen? wiring is good, debug the drawing."));
    Serial.println(F("     Blank screen? suspect panel type or SDA/SCL."));
  }

  display.hibernate();
  Serial.println(F("=== done ==="));
}

void loop()
{
}
