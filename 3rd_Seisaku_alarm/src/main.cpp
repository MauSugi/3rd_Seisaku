#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <DFRobotDFPlayerMini.h>

const char* ssid     = "GlocalNet_0XEONY";
const char* password = "65666475";

// アラーム設定
int alarmHour = 7;
int alarmMinute = 30;
bool isAlarmActive = false; // アラームが現在鳴っているかどうかのフラグ

// Webサーバー
WebServer server(80);

// DFPlayer設定
#define MY_TX_PIN 25 
#define MY_RX_PIN 26
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

// NTP設定
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 9 * 3600;

// HTML画面（設定＋停止ボタン）
void handleRoot() {
  String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif; text-align:center; background:#f4f4f4;} .card{background:white; padding:20px; border-radius:10px; display:inline-block; margin-top:50px; box-shadow:0 2px 5px rgba(0,0,0,0.1);} input{font-size:1.2rem; padding:10px;} .btn{display:block; width:100%; padding:15px; margin-top:20px; background:#e74c3c; color:white; text-decoration:none; border-radius:5px; font-weight:bold;}</style></head><body>";
  html += "<div class='card'><h1>ESP32 アラーム</h1>";
  html += "<p>現在のアラーム時刻: <strong>" + String(alarmHour) + ":" + (alarmMinute < 10 ? "0" : "") + String(alarmMinute) + "</strong></p>";
  
  html += "<form action='/set' method='GET'>";
  html += "時刻変更: <input type='time' name='t' required>";
  html += "<br><input type='submit' value='設定保存' style='margin-top:10px;'>";
  html += "</form>";

  html += "<a href='/stop' class='btn'>【緊急】アラームを止める</a>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// 時刻設定処理
void handleSet() {
  if (server.hasArg("t")) {
    String t = server.arg("t");
    alarmHour = t.substring(0, 2).toInt();
    alarmMinute = t.substring(3, 5).toInt();
    Serial.printf("New Alarm: %02d:%02d\n", alarmHour, alarmMinute);
    
    // HTMLレスポンスの作成
    String html = "<!DOCTYPE html><html><head>";
    // 文字化け防止のメタタグを最初の方に配置
    html += "<meta charset='UTF-8'>"; 
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='3;URL=/'>";
    html += "<style>body{font-family:sans-serif; text-align:center; padding-top:50px; background:#f4f4f4;} ";
    html += ".msg{background:white; display:inline-block; padding:20px; border-radius:10px; box-shadow:0 2px 5px rgba(0,0,0,0.1);}</style></head><body>";
    html += "<div class='msg'><h2>⏰ アラームを " + t + " に設定しました</h2>";
    html += "<p>3秒後に自動で戻ります...</p>";
    html += "<a href='/'>今すぐ戻る</a></div>";
    html += "</body></html>";
    
    // server.send の第2引数を "text/html" に、内容に UTF-8 が含まれることを明示
    server.send(200, "text/html; charset=utf-8", html);
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// アラーム停止処理（ボタンユニットやスマホから呼ばれる）
// アラーム停止処理（文字化け修正版）
void handleStop() {
  isAlarmActive = false;
  myDFPlayer.stop();
  Serial.println("ALARM STOPPED by Remote");

  // HTMLレスポンスの作成
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>"; // 文字化け防止
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3;URL=/'>"; // 3秒後に戻る
  html += "<style>body{font-family:sans-serif; text-align:center; padding-top:50px; background:#f4f4f4;} ";
  html += ".msg{background:white; display:inline-block; padding:20px; border-radius:10px; box-shadow:0 2px 5px rgba(0,0,0,0.1);}</style></head><body>";
  html += "<div class='msg'><h2>✅ アラームを停止しました</h2>";
  html += "<p>3秒後に自動で戻ります...</p>";
  html += "<a href='/'>戻る</a></div>";
  html += "</body></html>";

  // 第2引数に charset=utf-8 を追加
  server.send(200, "text/html; charset=utf-8", html);
}

void setup() {
  Serial.begin(115200);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  
  if (!myDFPlayer.begin(myDFPlayerSerial)) {
    Serial.println("DFPlayer error. Check SD card/connection.");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nIP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/stop", handleStop); // 停止用パスを追加
  server.begin();

  configTime(gmtOffset_sec, 0, ntpServer);
}

void loop() {
  server.handleClient();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // アラーム開始判定（秒が0の時かつフラグがオフの時）
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && timeinfo.tm_sec == 0) {
      if (!isAlarmActive) {
        Serial.println("ALARM START!");
        isAlarmActive = true;
        myDFPlayer.volume(3); // 音量は適宜調整
        myDFPlayer.play(2);    // SDカードの0002.mp3を再生
      }
    }
  }
  
  // アラーム作動中、音楽が終わってしまった場合のループ処理などが必要ならここに追加
  
  delay(100); 
}