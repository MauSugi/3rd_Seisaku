#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <DFRobotDFPlayerMini.h>
#include <TM1637Display.h>

const char* ssid     = "AiR-WiFi_0V3MP5";
const char* password = "62153483";

int alarmHour = 7;
int alarmMinute = 30;
int alarmTrack = 1;
bool isAlarmActive = false;

float sleepDebt = 0.0;
bool isSleeping = false;
time_t sleepStartTime = 0;
const float IDEAL_SLEEP = 7.0; 

#define BUTTON_PIN 27
#define CLK_PIN 18
#define DIO_PIN 19
#define MY_TX_PIN 25 
#define MY_RX_PIN 26

WebServer server(80);
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
TM1637Display display(CLK_PIN, DIO_PIN);

const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 9 * 3600;

void handleRoot() {
  String s = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  s += "<link rel='icon' href='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>🔔</text></svg>'>";
  s += "<title>Smart Alarm</title>";
  s += "<style>";
  s += "body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif; background:#f0f2f5; margin:0; display:flex; align-items:center; justify-content:center; min-height:100vh; color:#1c1e21;}";
  s += ".c{background:#fff; padding:40px 30px; border-radius:24px; box-shadow:0 20px 40px rgba(0,0,0,0.08); width:90%; max-width:360px; text-align:center;}";
  s += "#dbx{padding:15px; border-radius:15px; margin-bottom:15px; color:#fff; font-weight:bold; transition:0.5s;}";
  s += "h3{margin:0 0 10px; font-weight:600; color:#4b4b4b;}";
  s += "#curr{font-size:4rem; font-weight:700; margin:10px 0; background:linear-gradient(45deg, #4a90e2, #9b51e0); -webkit-background-clip:text; -webkit-fill-color:transparent; -webkit-text-fill-color:transparent;}";
  s += ".field{text-align:left; margin-top:15px;}";
  s += "label{display:block; margin-bottom:5px; font-size:0.85rem; font-weight:bold; color:#8e8e93;}";
  s += "input, select{width:100%; padding:12px; border:2px solid #f0f0f0; border-radius:12px; font-size:1rem; box-sizing:border-box; background:#f9f9f9;}";
  s += "button{width:100%; padding:16px; margin-top:10px; border:none; border-radius:14px; background:#1c1e21; color:#fff; font-size:1.1rem; font-weight:bold; cursor:pointer; transition:all 0.2s ease;}";
  s += ".s-btn{background:#1a237e; margin-top:10px;}";
  s += ".d-btn{background:#ff5252; color:#fff; margin-top:10px; font-size:0.9rem;}";
  s += ".status{margin-top:10px; font-size:0.8rem; color:#4a90e2; opacity:0;}";
  s += "</style></head>";
  s += "<body onload='g()'><div class='c'>";
  s += "<div id='dbx'>睡眠負債: <span id='debt'>0.0</span>h</div>";
  s += "<h3>🔔 Smart Alarm</h3>";
  s += "<div id='curr'>00:00</div>";
  s += "<button onclick='sl()' id='slB' class='s-btn'>🌙 今から寝る</button>";
  s += "<button onclick='stopDemo()' class='d-btn'>⚡ [DEMO] STOP</button>";
  s += "<div class='field'><label>⏰ Time</label><input type='time' id='t'></div>";
  s += "<div class='field'><label>🎵 Track</label><select id='m'><option value='1'>Track 1</option><option value='2'>Track 2</option></select></div>";
  s += "<button onclick='s()' id='btn'>SET ALARM</button>";
  s += "<div id='st' class='status'>Saved!</div></div>";
  s += "<script>";
  
  // 最新データ取得 (キャッシュ対策+表示リセットロジック追加)
  s += "async function g(){try{const r=await fetch('/api/get?t='+new Date().getTime());const d=await r.json();";
  s += "document.getElementById('curr').innerText=`${d.h.toString().padStart(2,'0')}:${d.m.toString().padStart(2,'0')}`;";
  s += "document.getElementById('t').value=`${d.h.toString().padStart(2,'0')}:${d.m.toString().padStart(2,'0')}`;";
  s += "document.getElementById('m').value=d.t;";
  s += "document.getElementById('debt').innerText=d.debt.toFixed(1);";
  s += "const b=document.getElementById('dbx'); if(d.debt>5){b.style.background='#ff5252';}else if(d.debt>2){b.style.background='#ffa726';}else{b.style.background='#66bb6a';}";
  s += "const sb=document.getElementById('slB');";
  s += "if(d.isSl){sb.innerText='💤 就寝中...'; sb.style.opacity='0.6';}else{sb.innerText='🌙 今から寝る'; sb.style.opacity='1';}";
  s += "}catch(e){}}";

  // ★ デモ用停止関数（確実に更新を反映させるフロー）
  s += "async function stopDemo(){";
  s += "  if(!confirm('アラームを強制停止しますか？'))return;";
  s += "  await fetch('/stop');"; 
  s += "  await new Promise(res => setTimeout(res, 500));"; // ESP32側の計算完了を待つ
  s += "  await g();"; // 最新状態を再取得
  s += "  alert('Stopped & Updated!');";
  s += "}";

  s += "async function sl(){await fetch('/api/sleep'); g();}";
  s += "async function s(){const b=document.getElementById('btn');const t=document.getElementById('t').value;const m=document.getElementById('m').value;if(!t)return;";
  s += "b.innerText='...'; await fetch(`/api/set?t=${t}&m=${m}`); location.reload();}";
  s += "</script></body></html>";
  server.send(200, "text/html", s);
}

void handleGetSettings() {
  String json = "{\"h\":" + String(alarmHour) + ",\"m\":" + String(alarmMinute) + ",\"t\":" + String(alarmTrack) + 
                ",\"debt\":" + String(sleepDebt) + ",\"isSl\":" + (isSleeping?"true":"false") + "}";
  server.send(200, "application/json", json);
}

void handleSleep() {
  sleepStartTime = time(NULL);
  isSleeping = true;
  server.send(200, "text/plain", "OK");
}

void handleSet() {
  if (server.hasArg("t")) {
    String t = server.arg("t");
    alarmHour = t.substring(0, 2).toInt();
    alarmMinute = t.substring(3, 5).toInt();
  }
  if (server.hasArg("m")) alarmTrack = server.arg("m").toInt();
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  server.send(200, "text/plain", "Stopped");
  if (isAlarmActive) {
    if (isSleeping) {
      time_t now = time(NULL);
      float hours = (float)(now - sleepStartTime) / 3600.0;
      sleepDebt += (IDEAL_SLEEP - hours);
      isSleeping = false;
    }
    isAlarmActive = false;
    myDFPlayer.stop();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  display.setBrightness(3);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  myDFPlayer.begin(myDFPlayerSerial);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.on("/", handleRoot);
  server.on("/api/get", handleGetSettings);
  server.on("/api/set", handleSet);
  server.on("/api/sleep", handleSleep);
  server.on("/stop", handleStop);
  server.begin();

  configTime(gmtOffset_sec, 0, ntpServer);
}

void loop() {
  server.handleClient();
  if (isAlarmActive && digitalRead(BUTTON_PIN) == LOW) {
    handleStop();
    delay(500);
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int currentTime = (timeinfo.tm_hour * 100) + timeinfo.tm_min;
    display.showNumberDecEx(currentTime, (timeinfo.tm_sec % 2 == 0 ? 0b01000000 : 0), true);
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && timeinfo.tm_sec == 0) {
      if (!isAlarmActive) {
        isAlarmActive = true;
        myDFPlayer.volume(20);
        myDFPlayer.play(alarmTrack);
      }
    }
  }
  delay(100); 
}