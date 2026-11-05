#include "led_indicator.h"

static LedMode currentMode = LED_MODE_NORMAL;
static int currentOtaPct = -1;

// ============================================================
//  Hieu chinh do sang theo Dinh luat Fechner / Stevens
//  PWM = 254 * (pct / 100)^2.4 + 1
// ============================================================
static inline uint8_t getPerceivedBrightness(int pct) {
  if (pct <= 0) return 1;
  if (pct >= 100) return 255;
  float normalized = pct / 100.0f;
  return (uint8_t)(roundf(powf(normalized, 2.4f) * 254.0f) + 1.0f);
}

void ledSetColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(PIN_RGB_LED, r, g, b);
}

void ledSetOff() {
  neopixelWrite(PIN_RGB_LED, 0, 0, 0);
}

static void setLedHue(uint8_t hue, uint8_t bri = 80) {
  uint8_t r = 0, g = 0, b = 0;
  uint8_t region = hue / 43;
  uint8_t remainder = (hue - (region * 43)) * 6;

  uint8_t q = (bri * (255 - remainder)) >> 8;
  uint8_t t = (bri * remainder) >> 8;

  switch (region) {
    case 0:  r = bri; g = t;   b = 0;   break;
    case 1:  r = q;   g = bri; b = 0;   break;
    case 2:  r = 0;   g = bri; b = t;   break;
    case 3:  r = 0;   g = q;   b = bri; break;
    case 4:  r = t;   g = 0;   b = bri; break;
    default: r = bri; g = 0;   b = q;   break;
  }
  ledSetColor(r, g, b);
}

struct RGBColor { uint8_t r, g, b; };
static RGBColor getVersionColor() {
  String v = FW_VERSION;
  int last = v.lastIndexOf('.');
  int patch = (last >= 0) ? v.substring(last + 1).toInt() : 0;
  static const RGBColor palette[] = {
    {120, 0, 0},    // 0: Do
    {0, 120, 0},    // 1: Xanh la
    {0, 0, 120},    // 2: Xanh lam
    {100, 100, 0},  // 3: Vang
    {0, 100, 100},  // 4: Cyan
    {100, 0, 100},  // 5: Magenta
    {80, 80, 80},   // 6: Trang
    {120, 40, 0}    // 7: Cam
  };
  return palette[patch % 8];
}

void ledInit() {
  // Khoi dong LED & chay animation chao mung
  ledSetColor(80, 0, 0);   delay(200);
  ledSetColor(0, 80, 0);   delay(200);
  ledSetColor(0, 0, 80);   delay(200);
  ledSetOff();
  currentMode = LED_MODE_NORMAL;
}

void ledSetOtaProgress(int pct) {
  currentMode = LED_MODE_OTA_PROGRESS;
  if (pct != currentOtaPct) {
    currentOtaPct = pct;
    uint8_t bri = getPerceivedBrightness(pct);
    // Mau Xanh ngoc (Cyan): sang tu rat mo (1) den cuc sang (255)
    ledSetColor(0, (uint8_t)((bri * 3) / 4), bri);
  }
}

void ledOtaSuccess() {
  currentMode = LED_MODE_OTA_SUCCESS;
  ledSetColor(0, 255, 0); // Xanh la 100% ruc ro
}

void ledOtaFail() {
  currentMode = LED_MODE_OTA_FAILED;
  ledSetColor(200, 0, 0); // Do bao loi
}

void ledRollbackAlert() {
  for (int i = 0; i < 6; i++) {
    if (i % 2) ledSetColor(120, 0, 0); else ledSetOff();
    delay(200);
  }
}

// --- Cac pattern hoat hoa binh thuong ---
static void patternVersionColor() {
  static uint32_t t = 0;
  static bool on = false;
  if (millis() - t < 500) return;
  t = millis(); on = !on;
  if (on) {
    RGBColor c = getVersionColor();
    ledSetColor(c.r, c.g, c.b);
  } else {
    ledSetOff();
  }
}

static void patternRainbow() {
  static uint32_t t = 0;
  static uint8_t hue = 0;
  if (millis() - t < 25) return;
  t = millis();
  setLedHue(hue++, 70);
}

static void patternStrobe() {
  static uint32_t t = 0;
  static bool on = false;
  uint16_t interval = on ? 50 : 300;
  if (millis() - t < interval) return;
  t = millis(); on = !on;
  if (on) ledSetColor(100, 100, 100);
  else ledSetOff();
}

static void patternBreathing() {
  static uint32_t t = 0;
  static int pct = 1;
  static int dir = 1;
  if (millis() - t < 18) return;
  t = millis();
  pct += dir;
  if (pct >= 100) dir = -1;
  if (pct <= 1)   dir = 1;
  uint8_t bri = getPerceivedBrightness(pct);
  ledSetColor(
    (uint8_t)((uint16_t)DEMO_R * bri / 255),
    (uint8_t)((uint16_t)DEMO_G * bri / 255),
    (uint8_t)((uint16_t)DEMO_B * bri / 255)
  );
}

static void patternTriColor() {
  static uint32_t t = 0;
  static uint8_t phase = 0;
  if (millis() - t < 500) return;
  t = millis();
  if (phase == 0)      ledSetColor(100, 0, 0);
  else if (phase == 1) ledSetColor(0, 100, 0);
  else                 ledSetColor(0, 0, 100);
  phase = (phase + 1) % 3;
}

void ledLoop() {
  if (currentMode != LED_MODE_NORMAL) return;

  switch (DEMO_PATTERN) {
    case PATTERN_VERSION_COLOR: patternVersionColor(); break;
    case PATTERN_RAINBOW:       patternRainbow();      break;
    case PATTERN_STROBE:        patternStrobe();       break;
    case PATTERN_BREATHING:     patternBreathing();    break;
    case PATTERN_TRI_COLOR:     patternTriColor();     break;
    default:                    patternVersionColor(); break;
  }
}
