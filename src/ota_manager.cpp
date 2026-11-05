#include "ota_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "mbedtls/sha256.h"
#include "esp_ota_ops.h"
#include "config.h"
#include "network_manager.h"
#include "led_indicator.h"

// Bao cho Arduino core khong tu xac nhan firmware; ta tu xac nhan khi MQTT on dinh
extern "C" bool verifyRollbackLater() { return true; }

static bool pendingVerify = false;
static bool otaPending    = false;
static String targetUrl;
static String targetSha;

static String toHex(const uint8_t *h, size_t n) {
  static const char *d = "0123456789abcdef";
  String s;
  for (size_t i = 0; i < n; i++) { s += d[h[i] >> 4]; s += d[h[i] & 15]; }
  return s;
}

static void otaFail(const char *why) {
  Serial.printf("[OTA] That bai: %s\n", why);
  ledOtaFail();
  networkPublishStatus("ota_failed", why);
}

void otaInit() {
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
      st == ESP_OTA_IMG_PENDING_VERIFY) {
    pendingVerify = true;
    Serial.printf("[OTA] Firmware moi, cho xac nhan (%us)\n",
                  (unsigned)(ROLLBACK_TIMEOUT_MS / 1000));
  }
}

void otaTrigger(const String &url, const String &sha) {
  targetUrl  = url;
  targetSha  = sha;
  otaPending = true;
  Serial.println("[OTA] Nhan lenh, san sang chay...");
}

bool otaIsPending() {
  return otaPending;
}

void otaConfirmFirmware() {
  if (pendingVerify) {
    esp_ota_mark_app_valid_cancel_rollback();
    pendingVerify = false;
    Serial.println("[OTA] >>> DA XAC NHAN FIRMWARE MOI (HUY ROLLBACK THANH CONG) <<<");
  }
}

void otaCheckRollback() {
  if (pendingVerify && millis() > ROLLBACK_TIMEOUT_MS) {
    Serial.println("[OTA] Qua thoi gian an toan -> Tu dong Rollback ve ban cu!");
    ledRollbackAlert();
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}

static void executeOta(const String &url, const String &expectSha) {
  Serial.printf("[OTA] Bat dau tai tu: %s\n", url.c_str());
  networkPublishStatus("ota_start");

  WiFiClientSecure client;
  client.setInsecure(); // Khong check root CA cua github cdn, da bao ve bang SHA256

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.setTimeout(15000);

  if (!http.begin(client, url)) { otaFail("begin"); return; }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char s[24]; snprintf(s, sizeof s, "http_%d", code);
    http.end(); otaFail(s); return;
  }

  int total = http.getSize();
  if (total <= 0 || !Update.begin(total)) {
    http.end(); otaFail("size_or_space"); return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  int written = 0, lastPct = -10;
  uint32_t lastData = millis();

  ledSetOtaProgress(0);

  while (written < total) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, min(avail, sizeof(buf)));
      mbedtls_sha256_update_ret(&ctx, buf, n);
      if (Update.write(buf, n) != (size_t)n) { break; }
      written += n;
      lastData = millis();

      int pct = (int)((int64_t)written * 100 / total);

      // Cap nhat do sang LED theo thi giac nguoi
      ledSetOtaProgress(pct);

      if (pct >= lastPct + 10) {
        lastPct = pct;
        Serial.printf("[OTA] %d%%\n", pct);
        char s[8]; snprintf(s, sizeof s, "%d%%", pct);
        networkPublishStatus("ota_progress", s);
      }
    } else {
      if (!http.connected() || millis() - lastData > 20000) break;
      delay(1);
    }
  }
  http.end();

  uint8_t hash[32];
  mbedtls_sha256_finish_ret(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  if (written != total) { Update.abort(); otaFail("incomplete"); return; }
  if (!toHex(hash, 32).equalsIgnoreCase(expectSha)) {
    Update.abort(); otaFail("sha256_mismatch"); return;
  }
  if (!Update.end(true)) { otaFail("end"); return; }

  // Nạp thành công
  ledOtaSuccess();
  Serial.println("[OTA] Hoan tat 100%, ngat ket noi va khoi dong lai...");
  networkPublishStatus("ota_ok_rebooting");
  delay(300);

  // Ngắt kết nối sạch trước khi restart
  networkDisconnect();
  ESP.restart();
}

void otaLoop() {
  otaCheckRollback();

  if (otaPending && networkIsMqttConnected()) {
    otaPending = false;
    executeOta(targetUrl, targetSha);
  }
}
