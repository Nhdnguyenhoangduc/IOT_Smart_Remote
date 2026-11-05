#include "dht_sensor.h"
#include <DHT.h>
#include "config.h"
#include "network_manager.h"

static DHT dht(PIN_DHT, DHT11);
static uint32_t lastReadTime = 0;
static float currentTemp = 0.0f;
static float currentHum = 0.0f;
static bool dataValid = false;

void sensorInit() {
  dht.begin();
  Serial.printf("[Sensor] DHT11 khoi tao tren GPIO %d\n", PIN_DHT);
}

float sensorGetTemperature() {
  return currentTemp;
}

float sensorGetHumidity() {
  return currentHum;
}

bool sensorIsValid() {
  return dataValid;
}

void sensorLoop() {
  // Doc moi DHT_READ_INTERVAL_MS (10s)
  if (millis() - lastReadTime < DHT_READ_INTERVAL_MS) return;
  lastReadTime = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("[Sensor] Doc DHT11 that bai!");
    dataValid = false;
    return;
  }

  currentTemp = t;
  currentHum = h;
  dataValid = true;

  Serial.printf("[Sensor] Nhiet do: %.1f *C | Do am: %.1f %%\n", t, h);

  // Gui du lieu qua MQTT len topic: ir/<devId>/dht
  networkPublishSensor(currentTemp, currentHum);
}
