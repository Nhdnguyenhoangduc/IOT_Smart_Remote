#include "network_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "ota_manager.h"
#include "ir_blaster.h"

static WiFiClientSecure netClient;
static PubSubClient     mqtt(netClient);

static String devId;
static String topicStatus;
static String topicSensor;
static String topicIrLearned;
static String topicCmdOta;
static String topicCmdIrPrefix;

static uint32_t lastWiFiAttempt = 0;
static uint32_t lastMqttAttempt = 0;
static uint32_t mqttWaitTime    = 3000;

String networkGetDeviceId() {
  return devId;
}

bool networkIsMqttConnected() {
  return mqtt.connected();
}

bool networkIsWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

void networkPublishStatus(const char *state, const char *extra) {
  if (!mqtt.connected()) return;

  JsonDocument d;
  d["state"] = state;
  d["fw"]    = FW_VERSION;
  d["rssi"]  = WiFi.RSSI();
  d["heap"]  = ESP.getFreeHeap();
  if (extra && *extra) d["info"] = extra;

  char buf[256];
  size_t n = serializeJson(d, buf);
  mqtt.publish(topicStatus.c_str(), (const uint8_t *)buf, n, true);
}

void networkPublishSensor(float temperature, float humidity) {
  if (!mqtt.connected()) return;

  JsonDocument d;
  d["temp"] = round(temperature * 10) / 10.0;
  d["hum"]  = round(humidity * 10) / 10.0;
  d["rssi"] = WiFi.RSSI();

  char buf[128];
  size_t n = serializeJson(d, buf);
  mqtt.publish(topicSensor.c_str(), (const uint8_t *)buf, n, false);
}

void networkPublishLearnedIR(const char *protocol, const char *codeHex, uint16_t bits, uint16_t rawLen, const uint16_t *rawArr) {
  if (!mqtt.connected()) return;

  JsonDocument d;
  d["protocol"] = protocol;
  d["code"]     = codeHex;
  d["bits"]     = bits;
  if (rawLen > 0 && rawArr) {
    JsonArray arr = d["raw"].to<JsonArray>();
    for (uint16_t i = 0; i < rawLen; i++) {
      arr.add(rawArr[i]);
    }
  }

  char buf[MQTT_BUFFER_SIZE];
  size_t n = serializeJson(d, buf);
  mqtt.publish(topicIrLearned.c_str(), (const uint8_t *)buf, n, false);
}

void networkDisconnect() {
  if (mqtt.connected()) {
    mqtt.disconnect();
    delay(100);
  }
  WiFi.disconnect(true);
  delay(200);
}

// ============================================================
//  Bo dinh tuyen lenh MQTT (Command Dispatcher)
// ============================================================
static void onMqttMessage(char *topic, byte *payload, unsigned int len) {
  String topStr = String(topic);
  JsonDocument d;
  DeserializationError err = deserializeJson(d, payload, len);
  if (err) {
    Serial.printf("[MQTT] JSON parse loi: %s\n", err.c_str());
    return;
  }

  // 1. Lenh OTA (ir/<devId>/cmd/ota)
  if (topStr == topicCmdOta) {
    const char *url = d["url"] | "";
    const char *ver = d["version"] | "";
    bool force      = d["force"] | false;
    String sha      = String((const char *)(d["sha256"] | ""));
    sha.replace("sha256:", "");
    sha.trim();

    if (!*url || strncmp(url, "https://", 8) != 0) {
      networkPublishStatus("ota_rejected", "url");
      return;
    }
    if (sha.length() != 64) {
      networkPublishStatus("ota_rejected", "sha256");
      return;
    }
    if (!force && *ver && String(ver) == FW_VERSION) {
      networkPublishStatus("ota_skipped", "same_version");
      return;
    }

    otaTrigger(url, sha);
    return;
  }

  // 2. Lenh IR (ir/<devId>/cmd/ir/<action>)
  if (topStr.startsWith(topicCmdIrPrefix)) {
    String subAction = topStr.substring(topicCmdIrPrefix.length());
    irHandleCommand(subAction, d);
    return;
  }
}

// ============================================================
//  Quan ly ket noi mang non-blocking
// ============================================================
static void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWiFiAttempt = millis();

  Serial.println("[WiFi] Dang ket noi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

static void ensureMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;

  if (millis() - lastMqttAttempt < mqttWaitTime) return;
  lastMqttAttempt = millis();

  Serial.println("[MQTT] Dang ket noi TLS...");
  if (mqtt.connect(devId.c_str(), MQTT_USER, MQTT_PASS,
                   topicStatus.c_str(), 1, true, "{\"state\":\"offline\"}")) {
    mqttWaitTime = 3000;

    // Dang ky cac topic nhan lenh
    mqtt.subscribe(topicCmdOta.c_str(), 1);
    String irSubPattern = topicCmdIrPrefix + "#";
    mqtt.subscribe(irSubPattern.c_str(), 1);

    Serial.println("[MQTT] OK -> Da dang ky topics");
    networkPublishStatus("online");

    // Xac nhan firmware moi on dinh -> HUY ROLLBACK
    otaConfirmFirmware();
  } else {
    mqttWaitTime = min<uint32_t>(mqttWaitTime * 2, 30000);
    Serial.printf("[MQTT] Loi rc=%d, thu lai sau %us\n",
                  mqtt.state(), (unsigned)(mqttWaitTime / 1000));
  }
}

void networkInit() {
  // Tao Device ID tu MAC goc cua ESP32
  char id[13];
  snprintf(id, sizeof id, "%012llX", (unsigned long long)ESP.getEfuseMac());
  devId            = String("irb-") + id;
  topicStatus      = "ir/" + devId + "/status";
  topicSensor      = "ir/" + devId + "/dht";
  topicIrLearned   = "ir/" + devId + "/learned";
  topicCmdOta      = "ir/" + devId + "/cmd/ota";
  topicCmdIrPrefix = "ir/" + devId + "/cmd/ir/";

  Serial.printf("[System] Device ID: %s\n", devId.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWiFiAttempt = millis();

  netClient.setCACert(EMQX_CA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
}

void networkLoop() {
  ensureWiFi();
  ensureMqtt();
  mqtt.loop();
}
