#include <WiFi.h>
#include <HTTPClient.h>
#include "globals.h"
#include "esp_camera.h"
#include "camera_module.h"

#define CAMERA_MODEL_WROVER_KIT  // Has PSRAM
#include "camera_pins.h"

const char* telegramBotToken = "YOUR_TOKEN_HERE"; // bot_token is the access token for Telegram Bot API, It needs the user to insert their own API to build server
const char* telegramChatId = "7533611600";
const char* flaskServerUrl = "http://10.0.0.8:8080/upload";
 
void startCameraServer();
void initCamera(){
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 4;
  config.pin_d1 = 5;
  config.pin_d2 = 18;
  config.pin_d3 = 19;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 21;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

  //startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}






const char* flaskServerHost = "10.0.0.8"; // 🟡 Flask服务器 IP 地址
const uint16_t flaskServerPort = 8080;         // 🟡 Flask端口
const char* flaskPath = "/upload";             // Flask 路由路径

void sendCmdToServer() {
  const char* serverUrl = "http://10.0.0.8:8080/record"; // 可配置的服务器 URL
  const int maxRetries = 3;
  const int retryDelayMs = 2000; // 每次重试间隔 2 秒

  for (int attempt = 0; attempt < maxRetries; attempt++) {
    HTTPClient http;
    http.setTimeout(5000); // 设置 5 秒超时
    http.begin(serverUrl);
    
    int httpCode = http.POST(""); // 空 body
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        Serial.printf("✅ 成功发送录制命令，服务器响应: %s\n", http.getString().c_str());
        http.end();
        return;
      } else {
        Serial.printf("⚠️ 服务器返回错误状态码: %d\n", httpCode);
      }
    } else {
      Serial.printf("❌ HTTP 请求失败: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    if (attempt < maxRetries - 1) {
      Serial.printf("重试 %d/%d，等待 %d 毫秒...\n", attempt + 1, maxRetries, retryDelayMs);
      delay(retryDelayMs);
    }
  }
  
  Serial.println("❌ 无法连接到服务器，放弃重试");
}

void captureAndSendPhotoDirect() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
 
 WiFiClient client;

  if (!client.connect(flaskServerHost, flaskServerPort)) {
    Serial.println("❌ 无法连接到 Flask 服务器");
    return;
  }

  // 构造 multipart/form-data 边界
  String boundary = "----ESP32FormBoundary";
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
  bodyStart += "ESP32 Camera Upload\r\n";

  bodyStart += "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  size_t contentLength = bodyStart.length() + fb->len + bodyEnd.length();

  // 构造 HTTP 请求头
  String requestHeader = "";
  requestHeader += "POST " + String(flaskPath) + " HTTP/1.1\r\n";
  requestHeader += "Host: " + String(flaskServerHost) + "\r\n";
  requestHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  requestHeader += "Content-Length: " + String(contentLength) + "\r\n";
  requestHeader += "Connection: close\r\n\r\n";

  // 发送 Header + multipart 起始部分
  client.print(requestHeader);
  client.print(bodyStart);

  // 发送 JPEG 图像数据（分块更省内存）
  const size_t bufferSize = 1024;
  for (size_t i = 0; i < fb->len; i += bufferSize) {
    size_t chunkSize = min(bufferSize, fb->len - i);
    client.write(fb->buf + i, chunkSize);
  }

  // 发送 multipart 尾部
  client.print(bodyEnd);

  // 读取响应
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();
  esp_camera_fb_return(fb);
}

void sendTriggerRequest(){
  HTTPClient http;

  http.begin(FLASK_SERVER_URL_RECORD_VIDEO);
  int httpResponseCode = http.GET();
  if(httpResponseCode >0){
    String payload = http.getString();
    Serial.printf("✅ Response received: %d\nContent: %s\n", httpResponseCode, payload.c_str());
  } else {
    Serial.printf("❌ Request failed. Error Code: %d\n", httpResponseCode);
  }

  http.end();
}