#include "button_manager.h"
#include "config.h"
#include "ir_blaster.h"
#include "led_indicator.h"
#include "network_manager.h"

static bool btnLastState[NBTN];
static uint32_t btnPressTime[NBTN];
static bool btnHandledHold[NBTN];

static inline bool isDown(int b) {
  return digitalRead(BTN_PINS[b]) == LOW;
}

void buttonsInit() {
  for (int b = 0; b < NBTN; b++) {
    pinMode(BTN_PINS[b], INPUT_PULLUP);
    btnLastState[b] = false;
    btnPressTime[b] = 0;
    btnHandledHold[b] = false;
  }
  Serial.println("[Buttons] Khoi tao 6 nut nhan thanh cong! (N1..N4, ON/OFF, FUNC)");
}

void buttonsLoop() {
  uint32_t now = millis();

  for (int b = 0; b < NBTN; b++) {
    bool down = isDown(b);

    // Phát hiện bắt đầu nhấn nút
    if (down && !btnLastState[b]) {
      btnLastState[b] = true;
      btnPressTime[b] = now;
      btnHandledHold[b] = false;
    }
    // Đang giữ nút
    else if (down && btnLastState[b]) {
      uint32_t duration = now - btnPressTime[b];

      if (b < SLOTS) {
        // Nút lệnh N1..N4, ON/OFF: Giữ 1.5s -> Học lệnh cho nút đó
        if (duration >= HOLD_LEARN_MS && !btnHandledHold[b]) {
          btnHandledHold[b] = true;
          Serial.printf("[Buttons] Nut %s GIU tren 1.5s -> Chuyen sang hoc lenh cho Profile %d Slot %d!\n",
                        BTN_NAMES[b], irGetProfile() + 1, b);
          
          // Nháy LED cam 3 lần rồi bắt đầu học
          for (int i = 0; i < 3; i++) {
            ledSetColor(80, 40, 0); delay(50);
            ledSetOff(); delay(50);
          }
          irLearnSlot(irGetProfile(), b, 10000);
        }
      } else {
        // Nút FUNC (b == 5): Giữ 3.0s -> Báo kích hoạt WiFi AP
        if (duration >= HOLD_WIFI_MS && !btnHandledHold[b]) {
          btnHandledHold[b] = true;
          Serial.println("[Buttons] Nut FUNC GIU 3s -> Yeu cau WiFi AP / Che do dac biet!");
          for (int i = 0; i < 4; i++) {
            ledSetColor(0, 100, 100); delay(60);
            ledSetOff(); delay(60);
          }
          networkPublishStatus("func_held_3s");
        }
      }
    }
    // Nhả nút
    else if (!down && btnLastState[b]) {
      btnLastState[b] = false;
      uint32_t duration = now - btnPressTime[b];

      // Nếu nhả ra trước khi đạt ngưỡng giữ -> xem là NHẤN NHANH (Click)
      if (!btnHandledHold[b] && duration > 50) { // Chống rung phím 50ms
        if (b < SLOTS) {
          // Nhấn nhanh N1..N4, ON/OFF -> Phát lệnh trong slot của profile hiện tại
          Serial.printf("[Buttons] Nut %s NHAN NHANH -> Phat IR Slot %d Profile %d\n",
                        BTN_NAMES[b], b, irGetProfile() + 1);
          if (irSendSlot(irGetProfile(), b)) {
            // Nháy nhẹ LED xanh lá báo phát thành công
            ledSetColor(0, 80, 0); delay(30); ledSetOff();
            networkPublishStatus("btn_sent", BTN_NAMES[b]);
          } else {
            // Nháy đỏ 4 lần báo slot trống
            for (int i = 0; i < 4; i++) {
              ledSetColor(80, 0, 0); delay(60);
              ledSetOff(); delay(60);
            }
            networkPublishStatus("btn_slot_empty", BTN_NAMES[b]);
          }
        } else {
          // Nhấn nhanh nút FUNC -> Đổi Profile (0..3)
          uint8_t nextProf = (irGetProfile() + 1) % PROFILES;
          irSetProfile(nextProf);
          Serial.printf("[Buttons] Nut FUNC NHAN NHANH -> Chuyen sang Profile %d\n", nextProf + 1);
          // Chớp LED tím số lần bằng số Profile
          for (int i = 0; i <= nextProf; i++) {
            ledSetColor(80, 0, 80); delay(200);
            ledSetOff(); delay(200);
          }
        }
      }
    }
  }
}
