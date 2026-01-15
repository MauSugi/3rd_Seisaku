#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid     = "AiR-WiFi_0V3MP5";
const char* password = "62153483";

// アラーム本体（親機）のIPアドレスをここに入力
const char* serverUrl = "http://192.168.43.10/stop"; 

#define REMOTE_BUTTON_PIN 27 // WROVERなのでここでも16は避けます

void setup() {
  Serial.begin(115200);
  pinMode(REMOTE_BUTTON_PIN, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
}

void loop() {
  // ボタンが押されたらリクエスト送信
  if (digitalRead(REMOTE_BUTTON_PIN) == LOW) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      int httpCode = http.GET(); // 親機の /stop を叩く

      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    delay(1000); // 連続送信防止
  }
}