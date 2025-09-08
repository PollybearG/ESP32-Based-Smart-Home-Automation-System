#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H
#include <Wire.h>
#include <Adafruit_AHTX0.h>

void init_dhtsensor();
float getTemperature();
float getHumidity();

#endif
