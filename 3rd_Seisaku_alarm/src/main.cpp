#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h> // Webサーバー用ライブラリ
#include <time.h>
#include <DFRobotDFPlayerMini.h>

const char* ssid     = "GlocalNet_0XEONY";
const char* password = "65666475";

// アラーム設定（初期値）
int alarmHour = 22;
int alarmMinute = 46;

// Webサーバー（ポート80）
WebServer server(80);

// DFPlayer設定
#define MY_TX_PIN 25 
#define MY_RX_PIN 26
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

// NTP設定
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 9 * 3600;

// HTML画面の作成
void handleRoot() {
  String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif; text-align:center;} input{font-size:1.2rem; padding:10px;}</style></head><body>";
  html += "<h1>ESP32 目覚まし設定</h1>";
  html += "<p>現在のアラーム時刻: " + String(alarmHour) + ":" + (alarmMinute < 10 ? "0" : "") + String(alarmMinute) + "</p>";
  html += "<form action='/set' method='GET'>";
  html += "時刻: <input type='time' name='t' required>";
  html += "<br><br><input type='submit' value='設定保存'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

// 時刻を受け取る処理
void handleSet() {
  if (server.hasArg("t")) {
    String t = server.arg("t"); // "HH:MM" 形式で届く
    alarmHour = t.substring(0, 2).toInt();
    alarmMinute = t.substring(3, 5).toInt();
    Serial.printf("New Alarm: %02d:%02d\n", alarmHour, alarmMinute);
  }
  server.send(200, "text/html", "<h1>設定しました</h1><a href='/'>戻る</a>");
}

void setup() {
  Serial.begin(115200);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  myDFPlayer.begin(myDFPlayerSerial);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nIP Address: ");
  Serial.println(WiFi.localIP()); // ここに表示されるIPをスマホで入力

  // サーバーのURL割り当て
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();

  configTime(gmtOffset_sec, 0, ntpServer);
}

void loop() {
  server.handleClient(); // スマホからのアクセスを待ち受ける

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // アラーム判定
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && timeinfo.tm_sec == 0) {
      Serial.println("ALARM START!");
      myDFPlayer.volume(5);
      myDFPlayer.play(2);
      myDFPlayer.volume(5);
      delay(500);
    }
  }
  delay(100); 
}