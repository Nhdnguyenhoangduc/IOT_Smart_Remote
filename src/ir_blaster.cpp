#include "ir_blaster.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "config.h"
#include "network_manager.h"
#include "led_indicator.h"

static IRsend irsend(PIN_IR_TX);
static IRrecv irrecv(PIN_IR_RX, 1024, 50, true);
static Preferences pref;

static uint8_t currentProfile = 0;
static Sig sigBuffer;

static bool learningActive = false;
static uint32_t learnStartTime = 0;
static uint32_t learnTimeout = 10000;
static int learnTargetProfile = -1;
static int learnTargetSlot = -1;

void irInit() {
  pinMode(PIN_RX_PWR, OUTPUT);
  digitalWrite(PIN_RX_PWR, LOW); // Mặc định tắt nguồn mắt thu để tiết kiệm điện và chống nhiễu

  irsend.begin();
  LittleFS.begin(true);
  pref.begin("ir", false);
  currentProfile = pref.getUChar("p", 0) % PROFILES;

  Serial.printf("[IR] Khoi tao thanh cong! TX=GPIO%d, RX=GPIO%d, RX_PWR=GPIO%d | Profile hien tai=%d\n",
                PIN_IR_TX, PIN_IR_RX, PIN_RX_PWR, currentProfile + 1);
}

uint8_t irGetProfile() {
  return currentProfile;
}

void irSetProfile(uint8_t p) {
  currentProfile = p % PROFILES;
  pref.putUChar("p", currentProfile);
  Serial.printf("[IR] Da chuyen sang Profile %d\n", currentProfile + 1);
  networkPublishStatus("profile_changed", String(currentProfile + 1).c_str());
}

String irGetSlotPath(int p, int b) {
  return "/" + String(p) + "_" + String(b);
}

bool irLoadSlot(int p, int b, Sig &s) {
  File f = LittleFS.open(irGetSlotPath(p, b), "r");
  if (!f) return false;
  memset(&s, 0, sizeof(Sig));
  f.read((uint8_t*)&s, min((size_t)f.size(), sizeof(Sig)));
  f.close();
  return true;
}

void irSaveSlot(int p, int b, const Sig &s) {
  File f = LittleFS.open(irGetSlotPath(p, b), "w");
  if (f) {
    f.write((uint8_t*)&s, offsetof(Sig, raw) + s.len * 2);
    f.close();
    Serial.printf("[IR Storage] Da luu slot p=%d, b=%d\n", p, b);
  }
}

bool irDeleteSlot(int p, int b) {
  String pt = irGetSlotPath(p, b);
  if (LittleFS.exists(pt)) {
    return LittleFS.remove(pt);
  }
  return false;
}

String irGetSlotDesc(int p, int b) {
  Sig s;
  if (!irLoadSlot(p, b, s)) return "";
  if (s.len) return "RAW " + String(s.len);
  return typeToString((decode_type_t)s.type) + " 0x" + uint64ToString(s.value, 16);
}

bool irSendSlot(int p, int b) {
  Sig s;
  if (!irLoadSlot(p, b, s)) {
    Serial.printf("[IR Send] Slot p=%d, b=%d dang trong!\n", p, b);
    return false;
  }

  ledSetColor(0, 0, 100); // Nháy xanh dương báo phát
  if (s.len) {
    irsend.sendRaw(s.raw, s.len, 38);
    Serial.printf("[IR Send] Phat RAW len=%d tu slot p=%d, b=%d\n", s.len, p, b);
  } else {
    irsend.send((decode_type_t)s.type, s.value, s.bits);
    Serial.printf("[IR Send] Phat %s 0x%s bits=%d tu slot p=%d, b=%d\n",
                  typeToString((decode_type_t)s.type).c_str(),
                  uint64ToString(s.value, 16).c_str(), s.bits, p, b);
  }
  delay(50);
  ledSetOff();
  return true;
}

bool irSendDirect(const char *protocol, uint64_t code, uint16_t bits, uint16_t repeat) {
  decode_type_t pType = strToDecodeType(protocol);
  if (pType == UNKNOWN) return false;

  ledSetColor(0, 0, 100);
  bool ok = irsend.send(pType, code, bits, repeat);
  delay(50);
  ledSetOff();
  return ok;
}

bool irSendRawDirect(const uint16_t *rawBuf, uint16_t len, uint16_t freq) {
  if (!rawBuf || len == 0) return false;
  ledSetColor(0, 0, 100);
  irsend.sendRaw(rawBuf, len, freq);
  delay(50);
  ledSetOff();
  return true;
}

bool irLearnSlot(int p, int b, uint32_t timeoutMs) {
  digitalWrite(PIN_RX_PWR, HIGH); // Bật nguồn mắt thu IR
  delay(150);
  irrecv.enableIRIn();

  decode_results r;
  uint32_t t = millis();
  bool ok = false;

  ledSetColor(80, 40, 0); // Cam = đang chờ tín hiệu

  while (millis() - t < timeoutMs) {
    if (irrecv.decode(&r)) {
      bool simple = (r.decode_type != UNKNOWN && r.bits <= 64 && !hasACState(r.decode_type));
      uint16_t n = getCorrectedRawLength(&r);
      if (r.overflow || r.rawlen < 6 || r.value == kRepeat || (!simple && n > MAXRAW)) {
        irrecv.resume();
        continue;
      }

      memset(&sigBuffer, 0, sizeof(Sig));
      sigBuffer.type = r.decode_type;
      sigBuffer.bits = r.bits;
      sigBuffer.value = r.value;
      if (!simple) {
        uint16_t *a = resultToRawArray(&r);
        memcpy(sigBuffer.raw, a, n * 2);
        sigBuffer.len = n;
        delete[] a;
      }
      irSaveSlot(p, b, sigBuffer);
      ok = true;
      break;
    }
    delay(5);
  }

  irrecv.disableIRIn();
  digitalWrite(PIN_RX_PWR, LOW); // Tắt nguồn mắt thu IR sau khi xong

  if (ok) {
    ledSetColor(0, 100, 0); delay(200); ledSetOff();
  } else {
    ledSetColor(100, 0, 0); delay(200); ledSetOff();
  }

  return ok;
}

void irStartLearning(uint32_t timeoutMs) {
  learningActive = true;
  learnTimeout = timeoutMs;
  learnStartTime = millis();

  digitalWrite(PIN_RX_PWR, HIGH);
  delay(150);
  irrecv.enableIRIn();

  ledSetColor(80, 40, 0);
  networkPublishStatus("ir_learning_start");
  Serial.printf("[IR] Che do hoc lenh bat dau (%d ms). Huong remote vao mat thu!\n", timeoutMs);
}

void irStopLearning() {
  if (learningActive) {
    irrecv.disableIRIn();
    digitalWrite(PIN_RX_PWR, LOW);
    learningActive = false;
    learnTargetProfile = -1;
    learnTargetSlot = -1;
    ledSetOff();
    Serial.println("[IR] Dung che do hoc lenh.");
  }
}

bool irIsLearning() {
  return learningActive;
}

void irHandleCommand(const String &subTopic, const JsonDocument &doc) {
  // 1. Phat slot da luu: ir/<devId>/cmd/ir/slot  {"p":0, "b":1}
  if (subTopic == "slot") {
    int p = doc["p"] | currentProfile;
    int b = doc["b"] | 0;
    if (p >= 0 && p < PROFILES && b >= 0 && b < SLOTS) {
      if (irSendSlot(p, b)) {
        networkPublishStatus("ir_slot_sent", String(irGetSlotDesc(p, b)).c_str());
      } else {
        networkPublishStatus("ir_slot_empty");
      }
    }
    return;
  }

  // 2. Chon Profile: ir/<devId>/cmd/ir/profile  {"p":1}
  if (subTopic == "profile") {
    int p = doc["p"] | 0;
    irSetProfile(p);
    return;
  }

  // 3. Phat ma truc tiep: ir/<devId>/cmd/ir/send  {"protocol":"NEC", "value":"0xFFA25D", "bits":32}
  if (subTopic == "send") {
    const char *protocol = doc["protocol"] | "NEC";
    int bits             = doc["bits"] | 32;
    int repeat           = doc["repeat"] | 0;
    uint64_t code = 0;
    if (doc["value"].is<const char*>()) {
      code = strtoull(doc["value"].as<const char*>(), NULL, 0);
    } else {
      code = doc["value"].as<uint64_t>();
    }

    if (irSendDirect(protocol, code, bits, repeat)) {
      networkPublishStatus("ir_sent", protocol);
    } else {
      networkPublishStatus("ir_send_failed", protocol);
    }
    return;
  }

  // 4. Phat ma RAW truc tiep: ir/<devId>/cmd/ir/raw  {"freq":38, "raw":[...]}
  if (subTopic == "raw") {
    JsonArrayConst arr = doc["raw"].as<JsonArrayConst>();
    uint16_t freq = doc["freq"] | 38;
    if (arr.size() > 0 && arr.size() <= MAXRAW) {
      uint16_t rawData[arr.size()];
      for (size_t i = 0; i < arr.size(); i++) {
        rawData[i] = arr[i].as<uint16_t>();
      }
      irSendRawDirect(rawData, arr.size(), freq);
      networkPublishStatus("ir_raw_sent");
    }
    return;
  }

  // 5. Hoc ma va luu vao slot hoac gui len MQTT: ir/<devId>/cmd/ir/learn {"p":0, "b":1, "timeout":10000}
  if (subTopic == "learn") {
    uint32_t timeoutMs = doc["timeout"] | 10000;
    learnTargetProfile = doc["p"] | -1;
    learnTargetSlot    = doc["b"] | -1;
    irStartLearning(timeoutMs);
    return;
  }

  // 6. Xoa slot da luu: ir/<devId>/cmd/ir/del {"p":0, "b":1}
  if (subTopic == "del") {
    int p = doc["p"] | -1;
    int b = doc["b"] | -1;
    if (p >= 0 && p < PROFILES && b >= 0 && b < SLOTS) {
      irDeleteSlot(p, b);
      networkPublishStatus("ir_slot_deleted");
    }
    return;
  }

  // 7. Dung hoc: ir/<devId>/cmd/ir/stop_learn
  if (subTopic == "stop_learn") {
    irStopLearning();
    networkPublishStatus("ir_learning_stopped");
    return;
  }
}

void irLoop() {
  if (!learningActive) return;

  if (millis() - learnStartTime > learnTimeout) {
    irStopLearning();
    networkPublishStatus("ir_learning_timeout");
    return;
  }

  decode_results r;
  if (irrecv.decode(&r)) {
    if (r.overflow || r.rawlen < 6 || r.value == kRepeat) {
      irrecv.resume();
      return;
    }

    bool simple = (r.decode_type != UNKNOWN && r.bits <= 64 && !hasACState(r.decode_type));
    uint16_t n = getCorrectedRawLength(&r);

    String protoStr = typeToString(r.decode_type);
    String codeHex = "0x" + uint64ToString(r.value, 16);

    Serial.printf("[IR LEARNED] Protocol: %s | Code: %s | Bits: %d | RawLen: %d\n",
                  protoStr.c_str(), codeHex.c_str(), r.bits, n);

    ledSetColor(0, 100, 0); // Xanh la bao thanh cong

    // Neu co slot muc tieu thi luu vao LittleFS
    if (learnTargetProfile >= 0 && learnTargetSlot >= 0) {
      memset(&sigBuffer, 0, sizeof(Sig));
      sigBuffer.type = r.decode_type;
      sigBuffer.bits = r.bits;
      sigBuffer.value = r.value;
      if (!simple) {
        uint16_t *a = resultToRawArray(&r);
        memcpy(sigBuffer.raw, a, n * 2);
        sigBuffer.len = n;
        delete[] a;
      }
      irSaveSlot(learnTargetProfile, learnTargetSlot, sigBuffer);
      Serial.printf("[IR LEARNED] Da tu dong luu vao Slot Profile %d - Nut %d\n",
                    learnTargetProfile + 1, learnTargetSlot + 1);
    }

    // Publish len MQTT
    if (simple) {
      networkPublishLearnedIR(protoStr.c_str(), codeHex.c_str(), r.bits);
    } else {
      uint16_t *rawArr = resultToRawArray(&r);
      networkPublishLearnedIR(protoStr.c_str(), codeHex.c_str(), r.bits, n, rawArr);
      delete[] rawArr;
    }

    delay(300);
    irStopLearning();
    networkPublishStatus("ir_learning_success", protoStr.c_str());
  }
}
