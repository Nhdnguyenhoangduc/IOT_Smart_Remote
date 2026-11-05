#pragma once
#include <Arduino.h>

// ============================================================
//  MODULE CAM BIEN NHIET DO & DO AM DHT11
// ============================================================

void sensorInit();
void sensorLoop();

// Ham lay gia tri gan nhat
float sensorGetTemperature();
float sensorGetHumidity();
bool sensorIsValid();
