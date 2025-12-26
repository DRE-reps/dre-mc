#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===================
// CAMERA MODEL
// ===================
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ===================
// WiFi AP
// ===================
const char* ssid = "ESP32_CAM_AP";
const char* password = "12345678";

IPAddress local_IP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

// ===================
WebServer server(81);

// ===================
// Camera setup
// ===================
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA;   // 320x240
  config.jpeg_quality = 12;             // 10–15
  config.fb_count = 2;

  esp_camera_init(&config);
}

// ===================
// MJPEG handler
// ===================
void streamMJPEG() {
  WiFiClient client = server.client();

  String response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";

  client.print(response);

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) continue;

    client.printf(
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n\r\n",
      fb->len
    );

    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);


  }
}

// ===================
// Setup
// ===================
void setup() {
  Serial.begin(115200);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  setupCamera();

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  server.on("/stream", HTTP_GET, streamMJPEG);
  server.begin();

  Serial.println("MJPEG stream started:");
  Serial.println("http://192.168.4.1:81/stream");
}



void loop() {
  server.handleClient();
}
