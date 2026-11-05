#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  MODULE DIEU KHIEN LED RGB ONBOARD (GPIO 10)
//  Tich hop Dinh luat Fechner / Stevens (Chuan thi giac mat nguoi)
// ============================================================

enum LedMode {
  LED_MODE_NORMAL,       // Chay cac pattern DEMO theo cau hinh
  LED_MODE_OTA_PROGRESS, // Dang hien thi tien do tai OTA (tang do sang)
  LED_MODE_OTA_SUCCESS,  // Xanh la ruc ro
  LED_MODE_OTA_FAILED,   // Do bao loi
  LED_MODE_OFF
};

void ledInit();
void ledSetColor(uint8_t r, uint8_t g, uint8_t b);
void ledSetOff();
void ledSetOtaProgress(int pct);
void ledOtaSuccess();
void ledOtaFail();
void ledRollbackAlert();
void ledLoop();
