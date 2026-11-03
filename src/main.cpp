// ============================================================
//  Universal IR Blaster - Giai doan 1: FOTA toan cau
//  ESP32-C3 Super Mini | HTTPS Pull OTA kich hoat qua MQTT
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "secrets.h"

#ifndef FW_VERSION
#define FW_VERSION "1.0.0"
#endif

// Bao cho Arduino core KHONG tu xac nhan firmware moi;
// ta tu xac nhan sau khi WiFi + MQTT chay on dinh (co che rollback)
extern "C" bool verifyRollbackLater() { return true; }

WiFiClientSecure netClient;
PubSubClient mqtt(netClient);

String devId, topicStatus, topicOta;
bool   pendingVerify = false;  // firmware vua OTA, dang cho xac nhan
bool   otaPending    = false;  // co yeu cau OTA cho xu ly (chay trong loop, khong chay trong callback)
String otaUrl;

// ---------- Cong bo trang thai (retained) len MQTT ----------
void publishStatus(const char *state, const char *extra = "") {
  JsonDocument d;
  d["state"] = state;
  d["fw"]    = FW_VERSION;
  d["rssi"]  = WiFi.RSSI();
  d["heap"]  = ESP.getFreeHeap();
  if (*extra) d["info"] = extra;
  char buf[200];
  size_t n = serializeJson(d, buf);
  mqtt.publish(topicStatus.c_str(), (const uint8_t *)buf, n, true);
}

// ---------- OTA: tai qua HTTPS, ghi thang xuong slot OTA khong chay ----------
void doOta(const String &url) {
  Serial.printf("[OTA] Bat dau: %s\n", url.c_str());
  publishStatus("ota_start");

  esp_http_client_config_t http = {};
  http.url               = url.c_str();
  http.crt_bundle_attach = esp_crt_bundle_attach;  // xac thuc TLS bang CA bundle cua IDF
  http.timeout_ms        = 20000;
  http.keep_alive_enable = true;
  http.buffer_size       = 4096;  // GitHub redirect co header dai -> can buffer lon
  http.buffer_size_tx    = 2048;

  esp_https_ota_config_t cfg = {};
  cfg.http_config = &http;

  esp_https_ota_handle_t h = nullptr;
  if (esp_https_ota_begin(&cfg, &h) != ESP_OK) {
    Serial.println("[OTA] Loi: khong mo duoc URL / sai TLS");
    publishStatus("ota_failed", "begin");
    return;
  }

  int total = esp_https_ota_get_image_size(h);
  int lastPct = -10;
  esp_err_t err;
  while ((err = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    int pct = total > 0 ? esp_https_ota_get_image_len_read(h) * 100 / total : 0;
    if (pct >= lastPct + 10) {  // bao tien do moi 10%
      lastPct = pct;
      Serial.printf("[OTA] %d%%\n", pct);
      char s[8]; snprintf(s, sizeof s, "%d%%", pct);
      publishStatus("ota_progress", s);
    }
    mqtt.loop();  // giu keepalive MQTT
  }

  // finish() kiem tra checksum/SHA256 cua image roi moi doi boot partition
  if (err == ESP_OK && esp_https_ota_is_complete_data_received(h) &&
      esp_https_ota_finish(h) == ESP_OK) {
    Serial.println("[OTA] Thanh cong, khoi dong lai...");
    publishStatus("ota_ok_rebooting");
    mqtt.loop(); delay(500);
    ESP.restart();
  } else {
    esp_https_ota_abort(h);  // bo do: slot dang chay khong bi anh huong
    Serial.printf("[OTA] That bai (err=0x%x), van chay ban cu\n", err);
    publishStatus("ota_failed", "download/verify");
  }
}

// ---------- MQTT callback: chi parse lenh, KHONG chay OTA o day ----------
void onMqtt(char *topic, byte *payload, unsigned int len) {
  if (String(topic) != topicOta) return;
  JsonDocument d;
  if (deserializeJson(d, payload, len)) { Serial.println("[MQTT] JSON loi"); return; }

  const char *url = d["url"] | "";
  const char *ver = d["version"] | "";
  bool force      = d["force"] | false;
  if (!*url || strncmp(url, "https://", 8) != 0) {  // chi chap nhan HTTPS
    publishStatus("ota_rejected", "url");
    return;
  }
  if (!force && *ver && String(ver) == FW_VERSION) {  // da dung phien ban thi bo qua
    publishStatus("ota_skipped", "same_version");
    return;
  }
  otaUrl = url; otaPending = true;
}

// ---------- WiFi / MQTT: tu ket noi lai co backoff ----------
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  static uint32_t last = 0;
  if (last && millis() - last < 10000) return;  // thu lai moi 10s, khong chan loop
  last = millis();
  Serial.println("[WiFi] Dang ket noi lai...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void ensureMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  static uint32_t last = 0, wait = 5000;
  if (last && millis() - last < wait) return;
  last = millis();

  Serial.println("[MQTT] Dang ket noi...");
  // LWT: broker tu dat trang thai "offline" neu thiet bi rot dot ngot
  if (mqtt.connect(devId.c_str(), MQTT_USER, MQTT_PASS,
                   topicStatus.c_str(), 1, true, "{\"state\":\"offline\"}")) {
    wait = 5000;
    mqtt.subscribe(topicOta.c_str(), 1);
    publishStatus("online");
    Serial.println("[MQTT] OK");
    if (pendingVerify) {  // firmware moi chay on -> xac nhan, huy rollback
      esp_ota_mark_app_valid_cancel_rollback();
      pendingVerify = false;
      Serial.println("[OTA] Da xac nhan firmware moi");
    }
  } else {
    wait = min<uint32_t>(wait * 2, 60000);  // backoff 5s -> 60s
    Serial.printf("[MQTT] That bai rc=%d, thu lai sau %us\n", mqtt.state(), (unsigned)(wait / 1000));
  }
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 3000) delay(10);  // cho USB CDC toi da 3s, KHONG cho vo han
  delay(200);
  Serial.printf("\n=== IR Blaster FW %s ===\n", FW_VERSION);

  // Firmware hien tai co dang o trang thai "cho xac nhan" (vua OTA) khong?
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
      st == ESP_OTA_IMG_PENDING_VERIFY) {
    pendingVerify = true;
    Serial.println("[OTA] Firmware moi, cho xac nhan (90s)");
  }

  char id[13];
  snprintf(id, sizeof id, "%012llX", (unsigned long long)ESP.getEfuseMac());
  devId       = String("irb-") + id;
  topicStatus = "ir/" + devId + "/status";
  topicOta    = "ir/" + devId + "/cmd/ota";
  Serial.printf("Device ID: %s\n", devId.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Super Mini hay loi WiFi neu de cong suat mac dinh
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  netClient.setCACert(EMQX_CA);        // TLS toi EMQX: ghim CA cua EMQX
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
}

void loop() {
  ensureWiFi();
  ensureMqtt();
  mqtt.loop();

  if (otaPending && mqtt.connected()) {
    otaPending = false;
    doOta(otaUrl);
  }

  // Qua 90s ma firmware moi khong len duoc WiFi+MQTT -> tu rollback ve ban cu
  if (pendingVerify && millis() > 90000) {
    Serial.println("[OTA] Firmware moi khong on dinh -> rollback");
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}
