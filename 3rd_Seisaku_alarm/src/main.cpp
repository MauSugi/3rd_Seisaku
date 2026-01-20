#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <DFRobotDFPlayerMini.h>
#include <TM1637Display.h>
#include <LittleFS.h>

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

int alarmHour = 7; 
int alarmMinute = 30;
int alarmTrack = 1;
bool isAlarmActive = false;
bool alreadyTriggered = false; 
float sleepDebt = 0.0;
bool isSleeping = false;
time_t sleepStartTime = 0;
bool timeSynced = false;

WebServer server(80);
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
TM1637Display display(CLK_PIN, DIO_PIN);

void saveDebtToROM() {
  File f = LittleFS.open("/debt.txt", "w");
  if (f) { f.print(sleepDebt); f.close(); Serial.printf("[ROM] Saved Debt: %.2f\n", sleepDebt); }
}

void loadDebtFromROM() {
  if (LittleFS.exists("/debt.txt")) {
    File f = LittleFS.open("/debt.txt", "r");
    if (f) { String s = f.readString(); sleepDebt = s.toFloat(); f.close(); Serial.printf("[ROM] Loaded Debt: %.2f\n", sleepDebt); }
  }
}

void handleRoot() {
  String s = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  s += "<style>body{font-family:'Segoe UI',sans-serif; background:#0f172a; color:#f8fafc; margin:0; display:flex; align-items:center; justify-content:center; min-height:100vh;}";
  s += ".c{background:rgba(255,255,255,0.05); backdrop-filter:blur(10px); padding:30px; border-radius:30px; border:1px solid rgba(255,255,255,0.1); width:90%; max-width:380px; text-align:center;}";
  s += ".graph-container{background:rgba(255,255,255,0.05); border-radius:10px; height:12px; width:100%; margin:15px 0 5px 0; overflow:hidden;}";
  // バーの初期設定（JavaScriptで色を制御するため背景指定は最小限に）
  s += "#bar{height:100%; width:0%; transition:1s; border-radius:10px;}";
  s += "#curr{font-size:4rem; font-weight:200; margin:5px 0;}";
  s += ".main-box{width:100%; height:60px; display:flex; align-items:center; justify-content:center; border-radius:15px; font-size:1.1rem; font-weight:600; margin-bottom:10px;}";
  s += ".s-btn{background:linear-gradient(135deg, #6366f1, #4338ca); color:#fff; border:none;}";
  s += "select, input{width:100%; background:rgba(255,255,255,0.05); border:1px solid rgba(255,255,255,0.1); color:#fff; padding:12px; border-radius:10px; margin-bottom:15px;}";
  s += ".footer-btns{margin-top:30px; border-top:1px solid rgba(255,255,255,0.05); padding-top:10px; display:flex; justify-content:space-around;}";
  s += ".sub-btn{background:transparent; border:none; color:#475569; font-size:0.7rem; cursor:pointer;}";
  s += "</style></head><body><div class='c'>";
  s += "<div style='text-align:left;'>SLEEP DEBT <span id='debt'>0.0</span> h</div>";
  s += "<div class='graph-container'><div id='bar'></div></div>"; 
  s += "<div id='curr'>--:--</div>";
  s += "<div id='box_awake'><button class='main-box s-btn' onclick='doSleep()'>START SLEEPING</button></div>";
  s += "<div id='box_sleep' style='display:none;'><div class='main-box' style='color:#475569;'>✨ SLEEPING...</div></div>";
  s += "<div style='text-align:left; margin-top:20px;'><input type='time' id='t' value='07:30'>";
  s += "<select id='trk'><option value='1'>Track 1</option><option value='2'>Track 2</option></select>";
  s += "<button style='width:100%; padding:12px; border-radius:10px; background:#334155; color:#fff;' onclick='setAlarm()'>SAVE</button></div>";
  s += "<div class='footer-btns'><button class='sub-btn' onclick='resetDebt()'>Reset Data</button><button class='sub-btn' onclick='doStop()'>Debug Stop</button></div>";
  s += "</div><script>";
  s += "async function update(){ try{ const r=await fetch('/api/get?v='+Date.now()); const d=await r.json();";
  s += "document.getElementById('debt').innerText = d.debt.toFixed(1);";
  s += "let p = Math.min(d.debt*10, 100); const bar=document.getElementById('bar'); bar.style.width = p + '%';";
  // --- 色のロジック: 緑(3h以下) -> 黄(7h以下) -> 赤(それ以上) ---
  s += "let c = '#34d399'; if(d.debt > 7){ c='#f87171'; }else if(d.debt > 3){ c='#fbbf24'; } bar.style.background = c;";
  s += "document.getElementById('curr').innerText = `${d.h.toString().padStart(2,'0')}:${d.m.toString().padStart(2,'0')}`;";
  s += "document.getElementById('box_awake').style.display = d.isSl ? 'none' : 'block'; document.getElementById('box_sleep').style.display = d.isSl ? 'block' : 'none';";
  s += "}catch(e){}} setInterval(update, 3000); update();";
  s += "async function doSleep(){ await fetch('/api/sleep'); update(); } async function doStop(){ await fetch('/stop'); update(); }";
  s += "async function setAlarm(){ const t=document.getElementById('t').value; const trk=document.getElementById('trk').value; await fetch(`/api/set?t=${t}&trk=${trk}`); update(); }";
  s += "async function resetDebt(){ if(confirm('Reset?')){ await fetch('/api/reset'); location.reload(); } }</script></body></html>";
  server.send(200, "text/html", s);
}

void handleStop() {
  if (isSleeping) {
    time_t now = time(NULL);
    if (now > 1000000) { float hours = (float)(now - sleepStartTime) / 3600.0; sleepDebt += (7.0 - hours); saveDebtToROM(); }
    isSleeping = false;
  }
  if (isAlarmActive) { 
    isAlarmActive = false; 
    myDFPlayer.stop(); 
    display.setBrightness(3, true); 
    Serial.println("[Alarm] Stopped."); 
  }
  server.send(200, "text/plain", "Stopped");
}

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); }
  loadDebtFromROM();

  display.setBrightness(3, true);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  if (!myDFPlayer.begin(myDFPlayerSerial, false)) { Serial.println("DFPlayer Error"); }
  
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[WiFi] Connected.");

  server.on("/", handleRoot);
  server.on("/api/get", [](){ String json = "{\"debt\":"+String(sleepDebt)+",\"isSl\":"+(isSleeping?"true":"false")+",\"h\":"+String(alarmHour)+",\"m\":"+String(alarmMinute)+"}"; server.send(200, "application/json", json); });
  server.on("/api/sleep", [](){ Serial.println("[Sleep] Started."); sleepStartTime = time(NULL); isSleeping = true; server.send(200, "text/plain", "OK"); });
  server.on("/stop", handleStop);
  server.on("/api/set", [](){
    if (server.hasArg("t")) { alarmHour = server.arg("t").substring(0, 2).toInt(); alarmMinute = server.arg("t").substring(3, 5).toInt(); }
    if (server.hasArg("trk")) { alarmTrack = server.arg("trk").toInt(); }
    alreadyTriggered = false; Serial.printf("[Settings] Target: %02d:%02d Track: %d\n", alarmHour, alarmMinute, alarmTrack); server.send(200, "text/plain", "OK");
  });
  server.on("/api/reset", [](){ sleepDebt = 0.0; if (LittleFS.exists("/debt.txt")) { LittleFS.remove("/debt.txt"); } saveDebtToROM(); server.send(200, "text/plain", "Reset OK"); });
  server.begin();

  configTime(9 * 3600, 0, "ntp.nict.jp");
}

void loop() {
  server.handleClient();
  
  static unsigned long lastFlash = 0;
  static bool isVisible = true;
  time_t now = time(NULL);
  struct tm *ti = localtime(&now);

  if (isAlarmActive) {
    if (millis() - lastFlash > 500) {
      lastFlash = millis();
      isVisible = !isVisible;
      display.setBrightness(isVisible ? 3 : 0, true); 
      if (now > 1000000) {
        display.showNumberDecEx((ti->tm_hour * 100) + ti->tm_min, (ti->tm_sec % 2 == 0 ? 0b01000000 : 0), true);
      }
    }
  } else if (!isVisible) {
    display.setBrightness(3, true);
    isVisible = true;
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    if (now > 1000000) {
      if (!timeSynced) { Serial.printf("[Time] Sync OK! %02d:%02d:%02d\n", ti->tm_hour, ti->tm_min, ti->tm_sec); timeSynced = true; }
      if (!isAlarmActive) {
        display.showNumberDecEx((ti->tm_hour * 100) + ti->tm_min, (ti->tm_sec % 2 == 0 ? 0b01000000 : 0), true);
      }
      
      if (ti->tm_hour == alarmHour && ti->tm_min == alarmMinute) {
        if (!alreadyTriggered) { 
          Serial.println(">>> !!! ALARM TRIGGERED !!! <<<");
          alreadyTriggered = true; 
          myDFPlayer.volume(20);
          delay(200);
          myDFPlayer.play(alarmTrack);
          delay(200);
          isAlarmActive = true; 
        }
      }
      if (ti->tm_min != alarmMinute) { alreadyTriggered = false; }
    }
  }
}