#pragma once
#include <Arduino.h>

// ============================================================
//  MODULE QUAN LY WIFI, MQTT (TLS) & DINH TUYEN LENH (ROUTER)
// ============================================================

void networkInit();
void networkLoop();
void networkDisconnect();

// Ham publish telemetry trang thai he thong
void networkPublishStatus(const char *state, const char *extra = "");

// Ham publish du lieu cam bien DHT11
void networkPublishSensor(float temperature, float humidity);

// Ham publish ket qua hoc lenh IR
void networkPublishLearnedIR(const char *protocol, const char *codeHex, uint16_t bits, uint16_t rawLen = 0, const uint16_t *rawArr = nullptr);

String networkGetDeviceId();
bool networkIsMqttConnected();
bool networkIsWiFiConnected();
