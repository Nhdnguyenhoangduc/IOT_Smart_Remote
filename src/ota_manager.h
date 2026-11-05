#pragma once
#include <Arduino.h>

// ============================================================
//  MODULE QUAN LY OTA & TU DONG ROLLBACK (AN TOAN)
// ============================================================

void otaInit();
void otaTrigger(const String &url, const String &sha);
void otaConfirmFirmware();
void otaCheckRollback();
void otaLoop();
bool otaIsPending();
