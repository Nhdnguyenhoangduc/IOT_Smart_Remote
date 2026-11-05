#pragma once
#include <Arduino.h>

// ============================================================
//  CAU HINH PHAN CUNG CHINH XAC TU E:\ir-remote\ir-remote
//  Khop 100% voi bo phan cung ESP32-C3-Zero / Super Mini da build
// ============================================================

// --- Phien ban firmware ---
#ifndef FW_VERSION
  #define FW_VERSION "1.0.5"
#endif

// --- 6 NUT NHAN (GPIO 4, 3, 2, 1, 5, 0) ---
// Thu tu trong mang BTN: N1=4, N2=3, N3=2, N4=1, ON/OFF=5, FUNC=0
#define NBTN         6
#define SLOTS        5     // 5 nut lenh moi profile (N1..N4 + ON/OFF)
#define BTN_FUNC_IDX 5     // Nut FUNC nam o chi so 5 (GPIO 0)
const uint8_t BTN_PINS[NBTN] = {4, 3, 2, 1, 5, 0};

// Ten goi than thien cua cac nut
const char* const BTN_NAMES[NBTN] = {"N1", "N2", "N3", "N4", "ON/OFF", "FUNC"};

// --- CHÂN HỒNG NGOẠI & LED ---
#define PIN_IR_TX    6     // Qua tro 1k -> chan B cua transistor NPN C828
#define PIN_IR_RX    7     // Chan OUT (Signal) cua mat thu IR
#define PIN_RX_PWR   8     // Cap nguon (VCC) cho mat thu IR, chi bat khi hoc lenh!
#define PIN_RGB_LED  10    // LED RGB WS2812 onboard

// --- CHÂN CẢM BIẾN NHIỆT ẨM DHT11 ---
#define PIN_DHT      9     // Chân DATA của DHT11 (GPIO9 khong vuong nut/IR/LED)

// --- PROFILES & THỜI GIAN NHẤN GIỮ ---
#define PROFILES            4       // 4 Profile thiet bi (TV, Dieu hoa, Quat, ...)
#define HOLD_LEARN_MS       1500    // Giu nut 1.5s = Hoc lenh cho nut do
#define HOLD_WIFI_MS        3000    // Giu nut FUNC 3s = Bat WiFi Web Server cau hinh
#define WEB_IDLE_TIMEOUT_MS 300000  // 5 phut Web khong dung -> Tu tat Web/WiFi

#define MAXRAW              700     // Do dai toi da cua xung RAW

// --- Thoi gian an toan Rollback FOTA ---
#define ROLLBACK_TIMEOUT_MS 180000UL // 180s cho ket noi on dinh sau OTA

// --- Thong so WiFi / MQTT FOTA Hub ---
#define WIFI_RETRY_INTERVAL_MS 15000UL
#define MQTT_BUFFER_SIZE       1024
#define MQTT_KEEPALIVE_SEC     30
#define DHT_READ_INTERVAL_MS   10000UL // Doc DHT11 dinh ky 10s

// --- LED Patterns ---
enum LedPattern {
  PATTERN_VERSION_COLOR = 0,
  PATTERN_RAINBOW       = 1,
  PATTERN_STROBE        = 2,
  PATTERN_BREATHING     = 3,
  PATTERN_TRI_COLOR     = 4
};

#ifndef DEMO_PATTERN
  #define DEMO_PATTERN PATTERN_VERSION_COLOR
#endif
#ifndef DEMO_R
  #define DEMO_R 0
#endif
#ifndef DEMO_G
  #define DEMO_G 150
#endif
#ifndef DEMO_B
  #define DEMO_B 255
#endif
