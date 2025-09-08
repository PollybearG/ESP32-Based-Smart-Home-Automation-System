#define BLYNK_PRINT Serial

/* Fill in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID          "TMPL2OPgoeGSk"
#define BLYNK_TEMPLATE_NAME        "EmergingTech"
#define BLYNK_AUTH_TOKEN           "Yd7-hGMooJo81lufQeUHP9cHcubN245z"

#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

#include "esp_camera.h"
#include "dht_sensor.h"
#include "distance_sensor.h"
#include "camera_module.h"


// Your WiFi credentials.
// Set password to "" for open networks.
 char ssid[] = "128Gao";
 char pass[] = "Gao5965678$";
//char ssid[] = "Mattiphone";
//char pass[] = "asdf1234";

#define LED_PIN_RED 4 
#define MOTOR_PIN 5
#define LDR_PIN 34


void setup();
void loop();
 
// BlynkTimer timer;
hw_timer_t* timer = NULL;
volatile bool routine_flag = false;
volatile bool ledState = false;
float lux=0.0;
Servo myServo;
int ldrValue;
int lumin_threshold =  3000;  
float temperature_threshold = 23.0;
float distance;
float distance_threshold = 28;
unsigned long last_event_time= 0;
unsigned long curr_time ;
unsigned int init_angle = 0;
bool manual_close = false;

// This function will be called every time Slider Widget
// in Blynk app writes values to the Virtual Pin 1
BLYNK_WRITE(V3)
{
  int angle = param.asInt(); // assigning incoming value from pin V1 to a variable
  myServo.write(angle);
  Serial.print("Angle: ");
  Serial.println(angle);  
}

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  if(pinValue==0){
    digitalWrite(LED_PIN_RED, LOW);
    Serial.print("LED : LOW ");
  }else {
     digitalWrite(LED_PIN_RED, HIGH);
     Serial.print("LED : HIGH ");
  }
}

void routine(){
    char message[64];

    float temperature = getTemperature();
    snprintf(message, sizeof(message), "%.2f °C, threshold: %.2f °C", temperature, temperature_threshold);
    Serial.println(message);
    // V0 ---> Temperature
    Blynk.virtualWrite(V0, message);

    float humidity = getHumidity();
    snprintf(message, sizeof(message), "Humidity is: %.2f %%", humidity);
    Serial.println(message);
    // V1 ---> Humidity
    Blynk.virtualWrite(V1, humidity);
 
    ldrValue = analogRead(LDR_PIN);
    int lightPercentage = map(ldrValue, 0, 4095, 0, 100);
    snprintf(message, sizeof(message), "%d lumin, threshold: %d lumin", ldrValue, lumin_threshold);
    Serial.println(message);
    // V2 ---> Lumin
    Blynk.virtualWrite(V2, message);

    distance = measureDistanceCM();
    snprintf(message, sizeof(message), "%.2f cm, threshold: %.2f cm", distance, distance_threshold);
    Serial.println(message);
    // V3 ---> Distance from the Door
    Blynk.virtualWrite(V3, message);

    // Deal with lumin threshold
    if(ldrValue < lumin_threshold){
      digitalWrite(LED_PIN_RED, HIGH);
      Blynk.virtualWrite(V7, 1);
    } else{
      digitalWrite(LED_PIN_RED, LOW);
      Blynk.virtualWrite(V7, 0);
    }

    if(temperature < temperature_threshold){
      Blynk.virtualWrite(V6, 0);
    } else{
      Blynk.virtualWrite(V6, 1);
    }

    curr_time = millis();
    Serial.printf("Time: %d\n", curr_time - last_event_time);
    if(distance < distance_threshold){
      if(curr_time - last_event_time > 60000){
        Serial.println("Camera loading.....");
        last_event_time = curr_time;
        Blynk.virtualWrite(V8, 1);
        sendTriggerRequest();
      }
    }else {
      Blynk.virtualWrite(V8, 0);
      Serial.println("Camera down.....");
    }
    
    
    
}

void IRAM_ATTR onTimer(){
  routine_flag = true;
}


 
void setup(){
    Serial.begin(115200);
    Serial.println("Init Blynk ...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
 
 
    delay(2000);

    // Init DHT20 Sensor
    init_dhtsensor();
    // Distance sensor setup
    initDistanceSensor();

 
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 100000, true);
    timerAlarmEnable(timer);


    // LED setup
    pinMode(LED_PIN_RED, OUTPUT);
    digitalWrite(LED_PIN_RED, LOW);

  
    // Init Motor
    myServo.attach(MOTOR_PIN);
    //myServo.write(0);

    // Init Camera. should move to another device.
    //initCamera();
}

void loop() {
    Blynk.run();
    if(routine_flag){
      routine_flag = false;
      routine();
    }
    // if(ldrValue<LuminThreshold){
    //   Blynk.virtualWrite(V6, 1);
    // }else {
    //    Blynk.virtualWrite(V6, 0);
    // }



  if (Serial.available()) {
      char c = Serial.read();
      if (c == 'p') {
        sendTriggerRequest();
        //sendCmdToServer();

      }

      if (c == 'a'){
        myServo.write(0);
      }
      if (c == 's'){
        myServo.write(90);
      }
      if (c == 'd'){
        myServo.write(180);
      }
    }

    delay(2000);
}

