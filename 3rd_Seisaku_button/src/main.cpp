#include <WiFi.h>
#include <HTTPClient.h>

// --- 設定項目 ---
const char* ssid     = "AiR-WiFi_0V3MP5";
const char* password = "62153483";
const char* host     = "http://192.168.43.10"; 

#define REMOTE_BUTTON_PIN 27
#define STATUS_LED_PIN    4

// --- LED演出の微調整 ---
const int MAX_BRIGHTNESS = 150; // 0〜255の間で設定（150は元の約60%）

void setup() {
  Serial.begin(115200);
  pinMode(REMOTE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  analogWrite(STATUS_LED_PIN, 0);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected!");
}

void sendRequest(String endpoint) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(host) + endpoint;
    http.begin(url);
    int httpCode = http.GET();
    http.end();
    Serial.printf("[HTTP] Sent %s, code: %d\n", endpoint.c_str(), httpCode);
  }
}

void loop() {
  static unsigned long pressStartTime = 0;
  static bool isPressing = false;
  static unsigned long lastPrintTime = 0;
  
  bool currentState = (digitalRead(REMOTE_BUTTON_PIN) == LOW);

  if (currentState) {
    if (!isPressing) {
      pressStartTime = millis();
      isPressing = true;
      lastPrintTime = 0;
      Serial.println("\nButton Down... Start timing.");
    }

    unsigned long duration = millis() - pressStartTime;

    // シリアルモニタ表示
    unsigned long elapsedSec = duration / 1000;
    if (elapsedSec > lastPrintTime) {
      Serial.printf("Holding... %d sec\n", elapsedSec);
      lastPrintTime = elapsedSec;
    }

    // --- LED演出（3次関数カーブ） ---
    if (duration < 3000) {
      // 3乗にすることで、前半2秒間をさらに暗く保ちます
      float progress = (float)duration / 3000.0;
      int brightness = (int)(progress * progress * progress * (float)MAX_BRIGHTNESS); 
      analogWrite(STATUS_LED_PIN, brightness);
    } 
    else if (duration < 5000) {
      // 3-5秒: 高速点滅
      if ((duration / 100) % 2 == 0) {
        analogWrite(STATUS_LED_PIN, MAX_BRIGHTNESS);
      } else {
        analogWrite(STATUS_LED_PIN, 0);
      }
    } 
    else {
      // 5秒以上: 消灯
      analogWrite(STATUS_LED_PIN, 0);
    }
  } 
  else {
    if (isPressing) {
      unsigned long duration = millis() - pressStartTime;
      isPressing = false;
      analogWrite(STATUS_LED_PIN, 0);
      Serial.printf("Button Released. Duration: %d ms\n", duration);

      if (duration >= 5000) {
        sendRequest("/api/snooze");
      } 
      else if (duration >= 3000) {
        sendRequest("/stop");
      } else {
        Serial.println("Ignored.");
      }
    }
  }
  delay(20); 
}