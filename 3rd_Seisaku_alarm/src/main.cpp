#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <DFRobotDFPlayerMini.h>
#include <TM1637Display.h>
#include <LittleFS.h> // ファイルシステム用ライブラリ

// --- WiFi & 固定IP設定 ---
const char* ssid     = "AiR-WiFi_0V3MP5";
const char* password = "62153483";
IPAddress ip(192, 168, 43, 10);      
IPAddress gateway(192, 168, 43, 1);   
IPAddress subnet(255, 255, 255, 0);   
IPAddress dns(8, 8, 8, 8);            

#define CLK_PIN 18
#define DIO_PIN 19
#define MY_TX_PIN 25 
#define MY_RX_PIN 26

// 管理変数
int alarmHour = 7; 
int alarmMinute = 30;
int alarmTrack = 1;
bool isAlarmActive = false;
bool alreadyTriggered = false; 
float sleepDebt = 0.0;
bool isSleeping = false;
time_t sleepStartTime = 0;

WebServer server(80);
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
TM1637Display display(CLK_PIN, DIO_PIN);

// --- 内部ROM 操作関数 ---
void saveDebtToROM() {
  File f = LittleFS.open("/debt.txt", "w");
  if (f) {
    f.print(sleepDebt);
    f.close();
    Serial.printf("[ROM] Saved Debt: %.2f\n", sleepDebt);
  }
}

void loadDebtFromROM() {
  if (LittleFS.exists("/debt.txt")) {
    File f = LittleFS.open("/debt.txt", "r");
    if (f) {
      String s = f.readString();
      sleepDebt = s.toFloat();
      f.close();
      Serial.printf("[ROM] Loaded Debt: %.2f\n", sleepDebt);
    }
  } else {
    Serial.println("[ROM] No saved data found.");
  }
}

// --- Webサーバー ハンドラ ---
// --- Webサーバー ハンドラ ---
void handleRoot() {
  String s = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  
  // --- iPhoneホーム画面用アイコン設定 ---
  s += "<meta name='apple-mobile-web-app-title' content='SleepDebt'>";
  // アイコンを「月(🌙)」の絵文字画像に設定
  s += "<link rel='apple-touch-icon' href='https://fonts.gstatic.com/s/i/short-term/release/googlestylesheet/mountain_city_and_sun/default/24px.svg'>";
  s += "<link rel='apple-touch-icon' href='https://img.icons8.com/emoji/160/000000/crescent-moon-emoji.png'>"; 

  s += "<style>body{font-family:'Segoe UI',sans-serif; background:#0f172a; color:#f8fafc; margin:0; display:flex; align-items:center; justify-content:center; min-height:100vh;}";
  s += ".c{background:rgba(255,255,255,0.05); backdrop-filter:blur(10px); padding:30px; border-radius:30px; border:1px solid rgba(255,255,255,0.1); width:90%; max-width:380px; text-align:center;}";
  s += ".graph-container{background:rgba(255,255,255,0.05); border-radius:10px; height:12px; width:100%; margin:15px 0 5px 0; overflow:hidden; border:1px solid rgba(255,255,255,0.05);}";
  s += "#bar{height:100%; width:0%; transition:1s cubic-bezier(0.4, 0, 0.2, 1); border-radius:10px;}";
  s += ".graph-label{display:flex; justify-content:space-between; font-size:0.65rem; color:#64748b; margin-bottom:20px;}";
  s += "#curr{font-size:4rem; font-weight:200; margin:5px 0; color:#fff;}";
  s += ".main-box{width:100%; height:60px; display:flex; align-items:center; justify-content:center; border-radius:15px; font-size:1.1rem; font-weight:600; margin-bottom:10px;}";
  s += ".s-btn{background:linear-gradient(135deg, #6366f1, #4338ca); color:#fff; border:none; cursor:pointer;}";
  s += ".sl-label{background:rgba(255,255,255,0.05); color:#475569; border:1px solid rgba(255,255,255,0.1); letter-spacing:2px;}";
  s += "select, input{width:100%; background:rgba(255,255,255,0.05); border:1px solid rgba(255,255,255,0.1); color:#fff; padding:12px; border-radius:10px; margin-bottom:15px; outline:none;}";
  s += ".footer-btns{margin-top:30px; border-top:1px solid rgba(255,255,255,0.05); padding-top:10px; display:flex; justify-content:space-around;}";
  s += ".sub-btn{background:transparent; border:none; color:#475569; font-size:0.7rem; cursor:pointer; padding:10px;}";
  s += "</style></head><body><div class='c'>";
  
  s += "<div style='text-align:left;'><span style='font-size:0.75rem; color:#94a3b8; letter-spacing:1px;'>SLEEP DEBT</span>";
  s += " <span id='debt' style='font-size:1.2rem; font-weight:600;'>0.0</span> <small style='color:#64748b;'>h</small></div>";
  s += "<div class='graph-container'><div id='bar'></div></div>";
  s += "<div class='graph-label'><span>GOOD</span><span>CAUTION</span><span>DANGER</span></div>";
  s += "<div id='curr'>07:30</div>";
  s += "<div id='box_awake'><button class='main-box s-btn' onclick='doSleep()'>START SLEEPING</button></div>";
  s += "<div id='box_sleep' style='display:none;'><div class='main-box sl-label'>✨ SLEEPING...</div></div>";
  s += "<div style='text-align:left; margin-top:20px;'><label style='font-size:0.7rem; color:#64748b;'>ALARM TIME</label><input type='time' id='t' value='07:30'>";
  s += "<label style='font-size:0.7rem; color:#64748b;'>TRACK</label><select id='trk'><option value='1'>Track 1</option><option value='2'>Track 2</option><option value='3'>Track 3</option></select>";
  s += "<button style='width:100%; padding:12px; border-radius:10px; border:none; background:#334155; color:#fff; font-weight:600;' onclick='setAlarm()'>SAVE SETTINGS</button></div>";
  s += "<div class='footer-btns'><button class='sub-btn' onclick='resetDebt()'>Reset Data</button><button class='sub-btn' onclick='doStop()'>Debug Stop</button></div>";
  
  s += "</div><script>";
  s += "async function update(){ try{ const r=await fetch('/api/get?v='+Date.now()); const d=await r.json();";
  s += "document.getElementById('debt').innerText = d.debt.toFixed(1);";
  s += "let p = Math.min(Math.max(d.debt * 10, 0), 100); const b = document.getElementById('bar');";
  s += "b.style.width = p + '%';";
  s += "if(d.debt > 5){ b.style.background = '#f43f5e'; b.style.boxShadow = '0 0 10px #f43f5e'; }";
  s += "else if(d.debt > 2.5){ b.style.background = '#fbbf24'; b.style.boxShadow = '0 0 10px #fbbf24'; }";
  s += "else { b.style.background = '#10b981'; b.style.boxShadow = 'none'; }";
  s += "document.getElementById('curr').innerText = `${d.h.toString().padStart(2,'0')}:${d.m.toString().padStart(2,'0')}`;";
  s += "document.getElementById('box_awake').style.display = d.isSl ? 'none' : 'block';";
  s += "document.getElementById('box_sleep').style.display = d.isSl ? 'block' : 'none';";
  s += "}catch(e){}}";
  s += "async function doSleep(){ await fetch('/api/sleep'); update(); }";
  s += "async function doStop(){ await fetch('/stop'); update(); }";
  s += "async function setAlarm(){ const t=document.getElementById('t').value; const trk=document.getElementById('trk').value; await fetch(`/api/set?t=${t}&trk=${trk}`); update(); }";
  s += "async function resetDebt(){ if(confirm('Reset all data?')){ await fetch('/api/reset'); location.reload(); } }";
  s += "setInterval(update, 3000); update();</script></body></html>";
  server.send(200, "text/html", s);
}

void handleGetSettings() {
  String json = "{\"debt\":" + String(sleepDebt) + ",\"isSl\":" + (isSleeping?"true":"false") + ",\"h\":" + String(alarmHour) + ",\"m\":" + String(alarmMinute) + "}";
  server.send(200, "application/json", json);
}

void handleSleep() {
  sleepStartTime = time(NULL);
  isSleeping = true;
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  server.send(200, "text/plain", "Stopped"); // ブラウザへ即時返信
  if (isSleeping) {
    time_t now = time(NULL);
    if (now > 1000000) {
      float hours = (float)(now - sleepStartTime) / 3600.0;
      sleepDebt += (7.0 - hours);
      saveDebtToROM(); // 負債をROMに保存
    }
    isSleeping = false;
  }
  if (isAlarmActive) {
    isAlarmActive = false;
    myDFPlayer.stop();
  }
}

void handleSet() {
  if (server.hasArg("t")) {
    alarmHour = server.arg("t").substring(0, 2).toInt();
    alarmMinute = server.arg("t").substring(3, 5).toInt();
  }
  if (server.hasArg("trk")) { alarmTrack = server.arg("trk").toInt(); }
  alreadyTriggered = false;
  server.send(200, "text/plain", "OK");
}

void handleReset() {
  sleepDebt = 0.0;
  if (LittleFS.exists("/debt.txt")) { LittleFS.remove("/debt.txt"); }
  saveDebtToROM();
  server.send(200, "text/plain", "Reset OK");
}

void setup() {
  Serial.begin(115200);
  
  // ファイルシステム初期化
  if (!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); }
  loadDebtFromROM(); // 起動時に読み込み

  display.setBrightness(3);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  myDFPlayer.begin(myDFPlayerSerial, false);
  
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  server.on("/", handleRoot);
  server.on("/api/get", handleGetSettings);
  server.on("/api/sleep", handleSleep);
  server.on("/stop", handleStop);
  server.on("/api/set", handleSet);
  server.on("/api/reset", handleReset);
  server.begin();

  configTime(9 * 3600, 0, "ntp.nict.jp");
}

void loop() {
  server.handleClient();
  
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    time_t now = time(NULL);
    struct tm *ti = localtime(&now);
    if (now > 100000) {
      display.showNumberDecEx((ti->tm_hour * 100) + ti->tm_min, (ti->tm_sec % 2 == 0 ? 0b01000000 : 0), true);
      
      if (ti->tm_hour == alarmHour && ti->tm_min == alarmMinute) {
        if (!alreadyTriggered) { 
          isAlarmActive = true;
          alreadyTriggered = true; 
          myDFPlayer.volume(20);
          myDFPlayer.play(alarmTrack);
        }
      } else {
        alreadyTriggered = false;
      }
    }
  }
}