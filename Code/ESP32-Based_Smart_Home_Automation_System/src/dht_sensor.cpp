#include "dht_sensor.h"

static Adafruit_AHTX0 dht20;
static sensors_event_t humidity, temp;

#define SDA_DHT 21
#define SCL_DHT 22


TwoWire I2C_DHT(0); // 使用 I2C 控制器 0
Adafruit_AHTX0 aht;

void init_dhtsensor() {
  if(!I2C_DHT.begin(SDA_DHT, SCL_DHT)){
    Serial.println("❌ Failed to init I2C_DHT!");
    while(1);
  }; // 初始化 I2C 总线

  if (!dht20.begin(&I2C_DHT)) {
    Serial.println("DHT20 not detected!");
    while (1); // Stop program if sensor not found
  } else {
    //Serial.println("✅ DHT20 ready.");
  }
}

float getTemperature() {
  dht20.getEvent(&humidity, &temp);
  return temp.temperature;
}

float getHumidity() {
  dht20.getEvent(&humidity, &temp);
  return humidity.relative_humidity;
}
