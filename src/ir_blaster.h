#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstddef>
#include "config.h"

// ============================================================
//  MODULE HONG NGOAI (IR BLASTER & LEARNER & STORAGE)
//  Tich hop 100% tinh nang tu E:\ir-remote\ir-remote
// ============================================================

// Cau truc luu tin hieu giong E:\ir-remote
struct Sig {
  uint8_t type;
  uint16_t bits;
  uint64_t value;
  uint16_t len;
  uint16_t raw[MAXRAW];
};

void irInit();
void irLoop();

// Quan ly profile & slot
uint8_t irGetProfile();
void irSetProfile(uint8_t p);
String irGetSlotPath(int p, int b);
bool irLoadSlot(int p, int b, Sig &s);
void irSaveSlot(int p, int b, const Sig &s);
bool irDeleteSlot(int p, int b);
String irGetSlotDesc(int p, int b);

// Phat tin hieu
bool irSendSlot(int p, int b);
bool irSendDirect(const char *protocol, uint64_t code, uint16_t bits, uint16_t repeat = 0);
bool irSendRawDirect(const uint16_t *rawBuf, uint16_t len, uint16_t freq = 38);

// Hoc tin hieu (co bat/tat nguon RX_PWR GPIO8)
bool irLearnSlot(int p, int b, uint32_t timeoutMs = 10000);
void irStartLearning(uint32_t timeoutMs = 10000);
void irStopLearning();
bool irIsLearning();

// Xu ly lenh MQTT
void irHandleCommand(const String &subTopic, const JsonDocument &doc);
