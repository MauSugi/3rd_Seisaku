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
float snoozeMinutes = 5.0; 
bool isAlarmActive = false;
bool alreadyTriggered = false; 
float sleepDebt = 0.0;
bool isSleeping = false;
time_t sleepStartTime = 0;

bool isSnoozeMode = false;
unsigned long snoozeStartTime = 0;

// フェードイン用の変数
int currentVolume = 5;
unsigned long lastVolumeUpTime = 0;

WebServer server(80);
HardwareSerial myDFPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
TM1637Display display(CLK_PIN, DIO_PIN);

void saveDebtToROM() {
  File f = LittleFS.open("/debt.txt", "w");
  if (f) { f.print(sleepDebt); f.close(); }
}

void loadDebtFromROM() {
  if (LittleFS.exists("/debt.txt")) {
    File f = LittleFS.open("/debt.txt", "r");
    if (f) { String s = f.readString(); sleepDebt = s.toFloat(); f.close(); }
  }
}

void handleStop() {
  if (!isAlarmActive) { server.send(200, "text/plain", "Ignored"); return; }
  if (isSleeping) {
    time_t now = time(NULL);
    if (now > 1000000) { float hours = (float)(now - sleepStartTime) / 3600.0; sleepDebt += (7.0 - hours); saveDebtToROM(); }
    isSleeping = false;
  }
  isSnoozeMode = false; isAlarmActive = false; myDFPlayer.stop(); display.setBrightness(3, true); 
  server.send(200, "text/plain", "Stopped");
}

void handleRoot() {
  String iconUrl = "https://cdn-icons-png.flaticon.com/512/2972/2972531.png";
  String s = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
  s += "<link rel='apple-touch-icon' href='" + iconUrl + "'>";
  s += "<title>Remote Alarm Clock</title>";
  s += "<style>body{font-family:'Helvetica Neue',Arial,sans-serif; background:#0f172a; color:#f8fafc; margin:0; padding:20px; display:flex; justify-content:center; min-height:100vh;}";
  s += ".c{background:rgba(255,255,255,0.05); backdrop-filter:blur(10px); padding:25px; border-radius:30px; border:1px solid rgba(255,255,255,0.1); width:100%; max-width:380px; text-align:center;}";
  s += ".header{display:flex; align-items:center; justify-content:center; gap:12px; margin-bottom:25px;} .header img{width:32px; height:32px;} .header h1{font-size:1.2rem; margin:0; color:#6366f1;}";
  s += ".status-panel{background:rgba(255,255,255,0.03); border-radius:20px; padding:15px; margin-bottom:20px; text-align:left; border:1px solid rgba(255,255,255,0.05);}";
  s += ".status-item{display:flex; justify-content:space-between; margin:5px 0; font-size:0.85rem; color:#94a3b8;} .status-val{color:#f8fafc; font-weight:600;}";
  s += ".graph-container{background:rgba(255,255,255,0.05); border-radius:10px; height:12px; width:100%; margin:10px 0; overflow:hidden;} #bar{height:100%; width:0%; transition:1s;}";
  s += ".time-container{margin:15px 0;} .time-label{font-size:0.65rem; color:#6366f1; letter-spacing:2px; font-weight:bold; margin-bottom:-10px;}";
  s += "#curr{font-size:4rem; font-weight:200; background:linear-gradient(to bottom, #fff, #6366f1); -webkit-background-clip:text; -webkit-text-fill-color:transparent; margin:0;}";
  s += ".main-box{width:100%; height:56px; display:flex; align-items:center; justify-content:center; border-radius:15px; font-size:1rem; font-weight:600; margin-bottom:10px; border:none; cursor:pointer;} .s-btn{background:linear-gradient(135deg, #6366f1, #4338ca); color:#fff;}";
  s += ".form-group{text-align:left; margin-top:20px; padding-top:20px; border-top:1px solid rgba(255,255,255,0.1);} label{font-size:0.75rem; color:#94a3b8; display:block; margin-bottom:5px;}";
  s += "select, input{width:100%; height:48px; background:rgba(255,255,255,0.05); border:1px solid rgba(255,255,255,0.1); color:#fff; padding:0 12px; border-radius:10px; margin-bottom:15px; font-size:1rem; box-sizing:border-box; appearance:none;}";
  s += ".footer-btns{margin-top:10px; display:flex; justify-content:space-around;} .sub-btn{background:transparent; border:none; color:#475569; font-size:0.7rem;}";
  s += "</style></head><body><div class='c'>";
  s += "<div class='header'><img src='" + iconUrl + "'><h1>Remote Alarm Clock</h1></div>";
  s += "<div style='text-align:left; font-size:0.8rem;'>SLEEP DEBT: <span id='debt'>0.0</span> h</div>";
  s += "<div class='graph-container'><div id='bar'></div></div>";
  s += "<div class='status-panel'>";
  s += "<div class='status-item'><span>ALARM TIME</span><span class='status-val' id='v_time'>--:--</span></div>";
  s += "<div class='status-item'><span>SNOOZE</span><span class='status-val' id='v_snz'>-- min</span></div>";
  s += "<div class='status-item'><span>TRACK</span><span class='status-val' id='v_trk'>--</span></div>";
  s += "</div>";
  s += "<div class='time-container'><div class='time-label'>CURRENT TIME</div><div id='curr'>--:--</div></div>";
  s += "<div id='box_awake'><button class='main-box s-btn' onclick='doSleep()'>START SLEEPING</button></div>";
  s += "<div id='box_sleep' style='display:none;'><div class='main-box' style='color:#475569; background:rgb(255, 255, 255);'>✨ SLEEPING...</div></div>";
  s += "<div class='form-group'><label>Set New Alarm</label><input type='time' id='t' value='07:30'>";
  s += "<label>Snooze (0.5 Step)</label><input type='number' id='snz' value='5.0' min='0.5' max='60' step='0.5'>";
  s += "<label>Select Track</label><select id='trk'>";
  s += "<option value='1'>1.アラーム</option>";
  s += "<option value='2'>2.Morning</option>";
  s += "<option value='3'>3.岐阜高専校歌</option>";
  s += "<option value='4'>4.マツケンサンバ</option>";
  s += "<option value='5'>5.Sunny Daze</option>";
  s += "<option value='6'>6.チャンカパーナ</option>";
  s += "</select><button class='main-box' style='background:#334155; color:#fff;' onclick='setAlarm()'>SAVE SETTINGS</button></div>";
  s += "<div class='footer-btns'><button class='sub-btn' onclick='resetDebt()'>Reset Data</button><button class='sub-btn' onclick='doStop()'>Debug Stop</button></div>";
  s += "</div><script>";
  s += "async function update(){ try{ const r=await fetch('/api/get?v='+Date.now()); const d=await r.json();";
  s += "document.getElementById('debt').innerText = d.debt.toFixed(1);";
  s += "document.getElementById('v_time').innerText = `${d.h.toString().padStart(2,'0')}:${d.m.toString().padStart(2,'0')}`;";
  s += "document.getElementById('v_snz').innerText = d.snz.toFixed(1) + ' min';";
  s += "const sel = document.getElementById('trk'); const name = sel.options[d.trk-1].text; document.getElementById('v_trk').innerText = name;";
  s += "let p = Math.min(d.debt*10, 100); const bar=document.getElementById('bar'); bar.style.width = p + '%';";
  s += "let c = (d.debt > 7) ? '#f87171' : (d.debt > 3) ? '#fbbf24' : '#34d399'; bar.style.background = c;";
  s += "document.getElementById('curr').innerText = `${d.nh.toString().padStart(2,'0')}:${d.nm.toString().padStart(2,'0')}`;";
  s += "document.getElementById('box_awake').style.display = d.isSl ? 'none' : 'block'; document.getElementById('box_sleep').style.display = d.isSl ? 'block' : 'none';";
  s += "}catch(e){}} setInterval(update, 3000); update();";
  s += "async function doSleep(){ await fetch('/api/sleep'); update(); } async function doStop(){ await fetch('/stop'); update(); }";
  s += "async function setAlarm(){ const t=document.getElementById('t').value; const trk=document.getElementById('trk').value; const snz=document.getElementById('snz').value;";
  s += "await fetch(`/api/set?t=${t}&trk=${trk}&snz=${snz}`); alert('Saved!'); update(); }";
  s += "async function resetDebt(){ if(confirm('Reset?')){ await fetch('/api/reset'); location.reload(); } }</script></body></html>";
  server.send(200, "text/html", s);
}

void setup() {
  Serial.begin(115200); LittleFS.begin(true); loadDebtFromROM(); display.setBrightness(3, true);
  myDFPlayerSerial.begin(9600, SERIAL_8N1, MY_RX_PIN, MY_TX_PIN);
  if (!myDFPlayer.begin(myDFPlayerSerial, false)) { Serial.println("DFPlayer Error"); }
  WiFi.config(ip, gateway, subnet, dns); WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  server.on("/", handleRoot); server.on("/stop", handleStop);
  server.on("/api/get", [](){ 
    time_t now = time(NULL); struct tm *ti = localtime(&now);
    String json = "{\"debt\":"+String(sleepDebt)+",\"isSl\":"+(isSleeping?"true":"false")+",\"h\":"+String(alarmHour)+",\"m\":"+String(alarmMinute)+",\"snz\":"+String(snoozeMinutes)+",\"trk\":"+String(alarmTrack)+",\"nh\":"+String(ti->tm_hour)+",\"nm\":"+String(ti->tm_min)+"}"; 
    server.send(200, "application/json", json); 
  });
  server.on("/api/sleep", [](){ sleepStartTime = time(NULL); isSleeping = true; server.send(200, "text/plain", "OK"); });
  server.on("/api/snooze", [](){
    if (isAlarmActive) { isAlarmActive = false; myDFPlayer.stop(); isSnoozeMode = true; snoozeStartTime = millis(); display.setBrightness(3, true); server.send(200, "text/plain", "Snooze OK"); }
    else { server.send(200, "text/plain", "Ignored"); }
  });
  server.on("/api/set", [](){
    if (server.hasArg("t")) { alarmHour = server.arg("t").substring(0, 2).toInt(); alarmMinute = server.arg("t").substring(3, 5).toInt(); }
    if (server.hasArg("trk")) { alarmTrack = server.arg("trk").toInt(); }
    if (server.hasArg("snz")) { snoozeMinutes = server.arg("snz").toFloat(); } 
    alreadyTriggered = false; server.send(200, "text/plain", "OK");
  });
  server.on("/api/reset", [](){ sleepDebt = 0.0; if (LittleFS.exists("/debt.txt")) { LittleFS.remove("/debt.txt"); } saveDebtToROM(); server.send(200, "text/plain", "Reset OK"); });
  server.begin(); configTime(9 * 3600, 0, "ntp.nict.jp");
}

void loop() {
  server.handleClient();
  static unsigned long lastFlash = 0; static bool isVisible = true;
  time_t now = time(NULL); struct tm *ti = localtime(&now);

  // --- スヌーズ/アラーム開始ロジック ---
  if (isSnoozeMode && (millis() - snoozeStartTime > (unsigned long)(snoozeMinutes * 60000.0))) {
    isSnoozeMode = false; isAlarmActive = true; 
    currentVolume = 25; // スヌーズ明けは音量25（固定）
    myDFPlayer.volume(currentVolume);
    delay(500);
    myDFPlayer.play(alarmTrack);
  }

  // --- 音量のフェードイン処理 ---
  if (isAlarmActive) {
    // currentVolumeが25未満の時だけ1秒おきにアップ（スヌーズ明けの25時は実行されない）
    if (currentVolume < 25 && (millis() - lastVolumeUpTime > 1000)) { 
      lastVolumeUpTime = millis();
      currentVolume++;
      myDFPlayer.volume(currentVolume);
    }

    // 点滅ロジック
    if (millis() - lastFlash > 500) { 
      lastFlash = millis(); isVisible = !isVisible; display.setBrightness(isVisible ? 3 : 0, true); 
      if (now > 1000000) display.showNumberDecEx((ti->tm_hour * 100) + ti->tm_min, (ti->tm_sec % 2 == 0 ? 0b01000000 : 0), true);
    }
  } else if (!isVisible) { display.setBrightness(3, true); isVisible = true; }

  // --- 定時チェック ---
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    if (now > 1000000) {
      if (!isAlarmActive) display.showNumberDecEx((ti->tm_hour * 100) + ti->tm_min, (ti->tm_sec % 2 == 0 ? 0b01000000 : 0), true);
      if (ti->tm_hour == alarmHour && ti->tm_min == alarmMinute && !alreadyTriggered) {
        alreadyTriggered = true; 
        currentVolume = 5; // 初回アラームは音量5からスタート
        myDFPlayer.volume(currentVolume);
        delay(200); 
        myDFPlayer.play(alarmTrack); 
        isAlarmActive = true; 
        lastVolumeUpTime = millis(); // フェードイン開始時間をリセット
      }
      if (ti->tm_min != alarmMinute) alreadyTriggered = false;
    }
  }
}