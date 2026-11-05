// ============================================================
//  Universal IR Blaster - ESP32-C3 Super Mini
//  Phase 1: Secure FOTA & Visual Telemetry (Fechner Curve)
//  Phase 2: IR Transmission & Signal Learning (100% E:\ir-remote)
//           + 6 Nút nhấn vật lý (N1..N4, ON/OFF, FUNC)
//           + Cảm biến nhiệt độ & độ ẩm DHT11
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "led_indicator.h"
#include "network_manager.h"
#include "ota_manager.h"
#include "ir_blaster.h"
#include "button_manager.h"
#include "dht_sensor.h"

void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 3000) delay(10);
  delay(100);

  Serial.printf("\n========================================\n");
  Serial.printf("   UNIVERSAL IR BLASTER - FW %s\n", FW_VERSION);
  Serial.printf("   Fit 100%% Hardware E:\\ir-remote + DHT11\n");
  Serial.printf("========================================\n");

  // 1. Khoi tao phan cung hien thi LED
  ledInit();

  // 2. Kiem tra trang thai OTA / Rollback
  otaInit();

  // 3. Khoi tao ket noi WiFi & MQTT TLS
  networkInit();

  // 4. Khoi tao phan cung Hong ngoai IR (TX, RX, RX_PWR LittleFS)
  irInit();

  // 5. Khoi tao 6 nut nhan vat ly (GPIO 4, 3, 2, 1, 5, 0)
  buttonsInit();

  // 6. Khoi tao cam bien Nhiet do - Do am DHT11 (GPIO 9)
  sensorInit();
}

void loop() {
  // Quan ly ket noi mang & nhan lenh MQTT
  networkLoop();

  // Xu ly tien trinh OTA hoac kiem tra timeout rollback
  otaLoop();

  // Xu ly 6 nut nhan vat ly (N1..N4, ON/OFF, FUNC)
  buttonsLoop();

  // Cap nhat hieu ung LED onboard
  ledLoop();

  // Vong lap lang nghe / xu ly tin hieu IR (hoc lenh)
  irLoop();

  // Doc va publish dinh ky cam bien DHT11
  sensorLoop();
}
