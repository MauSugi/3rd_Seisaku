#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --- ここを書き換えてください ---
const char* ssid     = "GlocalNet_0XEONY";
const char* password = "65666475";
const char* alarmUnitIp = "192.168.x.x"; // 親機のシリアルモニターに出たIPアドレス

#define BUTTON_PIN 14 // ボタンを繋ぐピン（14番とGND）

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // WiFi接続
  WiFi.begin(ssid, password);
  Serial.print("WiFi接続中");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi接続完了！");
  Serial.print("子機のIPアドレス: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // ボタンが押されたか確認
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("ボタンが押されました。通信開始...");

    HTTPClient http;
    // 親機のURLへリクエスト（テストとしてルートページにアクセス）
    String url = "http://" + String(alarmUnitIp) + "/"; 
    
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("通信成功！ 親機からのレスポンスコード: %d\n", httpCode);
    } else {
      Serial.printf("通信失敗... エラー: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    delay(500); // チャタリング・連打防止
  }
}