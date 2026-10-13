#include "loggamera_names.h"
// GPL-3.0-or-later; integrates the pinned qix67/comfortzone_heatpump dependency.
#include <Arduino.h>
#include <WiFi.h>
#include <sys/socket.h>
#include <errno.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <comfortzone_heatpump.h>
#include "secrets.h"
#include "measurements.h"
#include "controls.h"

// Waveshare ESP32-S3-RS485-CAN schematic: TX=17, RX=18, DE=21.
// DE is LOW except during one explicit, queued control frame; OTA waits for TX completion.
constexpr int RX_PIN=18, TX_PIN=17, DE_PIN=21;
constexpr uint32_t BAUD=19200;
struct Chunk { uint32_t sequence, ms; uint8_t length; uint8_t data[64]; };
Chunk *chunks=nullptr;
uint32_t capacity=0, nextSequence=1;
portMUX_TYPE ringLock=portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t rxBytes=0, parserFrames=0, parserBadCRC=0, uartErrors=0;
volatile bool otaBusy=false;
String bootId;
constexpr uint32_t WIFI_RETRY_MS=30000;
uint32_t wifiAttempts=0, wifiConnections=0, lastWifiAttempt=0;

void startWifiAttempt() {
  lastWifiAttempt=millis();
  ++wifiAttempts;
  // Cancel a stalled association/DHCP attempt, but retain credentials and STA mode.
  WiFi.disconnect(false,false);
  WiFi.mode(WIFI_STA);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.printf("Wi-Fi attempt %lu (retries have no limit)\n",(unsigned long)wifiAttempts);
}
WebServer web(80);
HardwareSerial bus(1);
comfortzone::Value readings[comfortzone::count];
portMUX_TYPE readingsLock=portMUX_INITIALIZER_UNLOCKED;
uint8_t receivedFrame[256];
uint16_t receivedFrameLength=0;

comfortzone_control::Engine controls;
portMUX_TYPE controlsLock=portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t txMutex;
volatile uint32_t txFrames=0,txBytes=0;

class MonitorBus: public RS485Interface {
  Chunk pending{};
public:
  void begin() override {
    digitalWrite(DE_PIN,LOW); pinMode(DE_PIN,OUTPUT);
    bus.setRxBufferSize(8192);
    bus.begin(BAUD,SERIAL_8N1,RX_PIN,TX_PIN);
    bus.onReceiveError([](hardwareSerial_error_t){ ++uartErrors; });
  }
  void commit() {
    if (!pending.length) return;
    portENTER_CRITICAL(&ringLock);
    pending.sequence=nextSequence++;
    chunks[(pending.sequence-1)%capacity]=pending;
    portEXIT_CRITICAL(&ringLock);
    pending.length=0;
  }
  void tick() { if(pending.length && uint32_t(millis()-pending.ms)>=20) commit(); }
  int available() override { return bus.available(); }
  int read_byte() override {
    int value=bus.read();
    if(value>=0) {
      portENTER_CRITICAL(&controlsLock);controls.byte(millis());portEXIT_CRITICAL(&controlsLock);
      if(!pending.length) pending.ms=millis();
      pending.data[pending.length++]=uint8_t(value); ++rxBytes;
      if(pending.length==sizeof(pending.data)) commit();
    }
    return value;
  }
  int write_bytes(const void*,int) override { return 0; }
  void flush() override {}
  void enable_receiver_mode() override { digitalWrite(DE_PIN,LOW); }
  void enable_sender_mode() override { digitalWrite(DE_PIN,LOW); }
} monitor;
comfortzone_heatpump pump(&monitor);

void captureTask(void*) {
  pump.set_grab_buffer(receivedFrame,sizeof(receivedFrame),&receivedFrameLength);
  pump.begin();
  for(;;) {
    if(!otaBusy) {
      for(int i=0; i<128 && monitor.available(); ++i) {
        auto result=pump.process();
        if(result!=comfortzone_heatpump::PFT_NONE) {
          ++parserFrames;
          if(result==comfortzone_heatpump::PFT_CORRUPTED) ++parserBadCRC;
          else {
            const uint32_t now=millis();
            portENTER_CRITICAL(&readingsLock);
            comfortzone::decode(receivedFrame,receivedFrameLength,now,readings);
            // Readings are only written by this task; HTTP takes snapshots under its lock.
            portEXIT_CRITICAL(&readingsLock);
            portENTER_CRITICAL(&controlsLock);
            controls.observe(receivedFrame,receivedFrameLength,now,readings);
            portEXIT_CRITICAL(&controlsLock);
          }
        }
      }
      monitor.tick();
      if(xSemaphoreTake(txMutex,0)==pdTRUE){
        comfortzone_control::Frame frame;bool send=false;const bool uartEmpty=bus.available()==0;
        portENTER_CRITICAL(&controlsLock);
        if(otaBusy)controls.interrupt(millis());
        else send=controls.tick(millis(),uartEmpty,frame);
        portEXIT_CRITICAL(&controlsLock);
        if(send&&(otaBusy||bus.available()!=0)){
          portENTER_CRITICAL(&controlsLock);controls.interrupt(millis());portEXIT_CRITICAL(&controlsLock);send=false;
        }
        if(send){
          monitor.commit();
          digitalWrite(DE_PIN,HIGH);delayMicroseconds(50);
          size_t written=bus.write(frame.bytes,frame.length);bus.flush(true);
          digitalWrite(DE_PIN,LOW);++txFrames;txBytes+=written;
          if(written!=frame.length){portENTER_CRITICAL(&controlsLock);controls.interrupt(millis());portEXIT_CRITICAL(&controlsLock);}
        }
        xSemaphoreGive(txMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
String statusJson() {
  uint32_t next;
  portENTER_CRITICAL(&ringLock); next=nextSequence; portEXIT_CRITICAL(&ringLock);
  String json="{\"firmware\":\"comfortzone-monitor-0.8.3\",\"boot_id\":\""+bootId+
    "\",\"ip\":\""+WiFi.localIP().toString()+"\",\"uptime_ms\":"+String(millis())+
    ",\"wifi_attempts\":"+String(wifiAttempts)+",\"wifi_connections\":"+String(wifiConnections)+
    ",\"wifi_retry_ms\":"+String(WIFI_RETRY_MS)+
    ",\"ap_bssid\":\""+WiFi.BSSIDstr()+"\",\"wifi_channel\":"+String(WiFi.channel())+
    ",\"ap_selection\":\"strongest-signal-all-channels\""+
    ",\"rssi_dbm\":"+String(WiFi.RSSI())+",\"free_heap\":"+String(ESP.getFreeHeap())+
    ",\"psram_bytes\":"+String(ESP.getPsramSize())+",\"rx_bytes\":"+String(rxBytes)+
    ",\"parser_frames\":"+String(parserFrames)+",\"parser_bad_crc\":"+String(parserBadCRC)+
    ",\"uart_errors\":"+String(uartErrors)+",\"next_sequence\":"+String(next)+
    ",\"ring_capacity\":"+String(capacity)+",\"baud\":19200,\"serial\":\"8N1\",\"transmit_enabled\":true,\"tx_frames\":"+String(txFrames)+",\"tx_bytes\":"+String(txBytes)+","+
    "\"decoder_profile\":\"loggamera-v1.1-mapping-influx-compared\"}";
  return json;
}
void statusPage() {
  web.send(200,"application/json",statusJson());
}
String measurementsJson() {
  static comfortzone::Value snapshot[comfortzone::count]; // HTTP handlers run serially; avoid growing the loop stack.
  portENTER_CRITICAL(&readingsLock);memcpy(snapshot,readings,sizeof(snapshot));portEXIT_CRITICAL(&readingsLock);
  uint32_t now=millis();String json;json.reserve(24000);json="{\"boot_id\":\""+bootId+"\",\"measurements\":[";
  for(size_t i=0;i<comfortzone::count;++i) {
    const auto &d=comfortzone::definitions[i];const auto &v=snapshot[i];if(i)json+=',';
    uint32_t age=now-v.updatedMs;
    const char *state=!v.received?"waiting":v.fault?"sensor_fault":age>30000?"stale":"ok";
    json+="{\"id\":\""+String(d.id)+"\",\"label\":\""+d.label+"\",\"unit\":\""+d.unit+
      "\",\"group\":\""+d.group+"\",\"state\":\""+state+"\",\"value\":";
    json+=v.received&&!v.fault?String(double(v.raw)/d.scale,d.scale==10?1:0):String("null");
    json+=",\"kind\":\""+String(d.kind)+"\",\"source\":\""+d.source+"\",\"evidence\":\""+d.evidence+"\"";
    const char *loggameraName=comfortzone::loggameraName(d.id);
    json+=",\"loggamera_name\":";
    json+=loggameraName ? String("\"")+loggameraName+"\"" : String("null");
    json+=",\"age_ms\":";json+=v.received?String(age):String("null");json+="}";
  }
  return json+"]}";
}
String controlJson() {
  comfortzone_control::Result r;bool ready;
  portENTER_CRITICAL(&controlsLock);r=controls.result;ready=controls.ready(millis());portEXIT_CRITICAL(&controlsLock);
  String json="{\"boot_id\":\""+bootId+"\",\"ready\":"+(ready?"true":"false")+
    ",\"id\":"+String(r.id)+",\"state\":\""+comfortzone_control::stateName(r.state)+"\",\"command\":\""+comfortzone_control::specs[int(r.kind)].name+
    "\",\"requested\":"+String(double(r.requested)/comfortzone_control::specs[int(r.kind)].scale,1)+
    ",\"acknowledged\":"+(r.acknowledged?"true":"false")+",\"readback_matches\":"+(r.readback?"true":"false")+
    ",\"reply_code\":"+String(r.replyCode)+",\"sent_ms\":"+String(r.sentMs)+",\"tx_hex\":\"";
  const char hex[]="0123456789ABCDEF";
  for(size_t i=0;i<r.tx.length;++i){json+=hex[r.tx.bytes[i]>>4];json+=hex[r.tx.bytes[i]&15];}
  return json+"\"}";
}
void controlError(int status,const char *message){web.send(status,"application/json",String("{\"error\":\"")+message+"\"}");}
void controlPost(comfortzone_control::Kind kind) {
  // Custom header prevents cross-origin HTML form submissions; no HTTP password is required.
  // Accept the previous header so existing API clients continue working.
  if(web.header("X-Comfortzone-Control")!="1" && web.header("X-EX50-Control")!="1"){controlError(400,"X-Comfortzone-Control: 1 is required");return;}
  if(web.header("Content-Type")!="application/x-www-form-urlencoded"||web.args()!=1||web.argName(0)!="value"){
    controlError(400,"Send one form field named value");return;
  }
  int raw;
  if(web.arg("value").length()>12||!comfortzone_control::parse(kind,web.arg("value").c_str(),raw)){
    controlError(400,"Invalid value, range or precision");return;
  }
  comfortzone::Value current;
  portENTER_CRITICAL(&readingsLock);current=readings[comfortzone::indexOf(comfortzone_control::specs[int(kind)].reading)];portEXIT_CRITICAL(&readingsLock);
  if(!current.received||current.fault||uint32_t(millis()-current.updatedMs)>30000){controlError(503,"Current setting is unavailable or stale");return;}
  bool busy,ready,accepted=false;
  portENTER_CRITICAL(&controlsLock);busy=controls.active();ready=controls.ready(millis());
  if(!busy&&ready&&!otaBusy)accepted=controls.queue(kind,raw,millis());
  portEXIT_CRITICAL(&controlsLock);
  if(!accepted){controlError(busy?409:503,busy?"Another command is in progress":"Bus not ready or OTA in progress");return;}
  web.send(202,"application/json",controlJson());
}
void trafficPage() {
  uint32_t after=web.hasArg("after")?strtoul(web.arg("after").c_str(),nullptr,10):0;
  // Snapshot briefly under lock; JSON construction and network I/O happen outside it.
  static Chunk snapshot[64]; // WebServer handlers are serial; keep this off the loop stack.
  unsigned count=0; uint32_t oldest,next,dropped=0;
  portENTER_CRITICAL(&ringLock);
  next=nextSequence; oldest=next>capacity?next-capacity:1;
  uint32_t start=after>=next ? next : after+1;
  if(start<oldest) { dropped=oldest-start; start=oldest; }
  for(uint32_t seq=start; seq<next && count<64; ++seq) snapshot[count++]=chunks[(seq-1)%capacity];
  portEXIT_CRITICAL(&ringLock);
  String json; json.reserve(12000);
  json="{\"boot_id\":\""+bootId+"\",\"oldest_sequence\":"+String(oldest)+
       ",\"next_sequence\":"+String(next)+",\"dropped_chunks\":"+String(dropped)+",\"chunks\":[";
  const char hex[]="0123456789ABCDEF";
  for(unsigned i=0;i<count;++i) {
    const auto &c=snapshot[i]; if(i) json+=',';
    json+="{\"sequence\":"+String(c.sequence)+",\"ms\":"+String(c.ms)+",\"hex\":\"";
    for(unsigned j=0;j<c.length;++j) {json+=hex[c.data[j]>>4];json+=hex[c.data[j]&15];}
    json+="\"}";
  }
  json+="]}"; web.send(200,"application/json",json);
}
// Detach sockets from WebServer so an SSE subscriber never occupies
// its single request slot. Nonblocking sends keep slow clients from delaying OTA.
struct Subscriber {
  NetworkClient client;
  uint32_t after=0, lastStatus=0, progress=0;
  String pending;
  size_t sent=0;
};
Subscriber subscribers[3];
void eventsPage() {
  Subscriber *slot=nullptr;
  for(auto &sub:subscribers) if(!sub.client.connected()) {slot=&sub;break;}
  if(!slot) {web.send(503,"text/plain","SSE client limit reached");return;}
  uint32_t after=0;
  String cursor=web.header("Last-Event-ID");
  if(!cursor.length() && web.hasArg("cursor"))cursor=web.arg("cursor");
  int colon=cursor.indexOf(':');
  if(colon>0 && cursor.substring(0,colon)==bootId)
    after=strtoul(cursor.substring(colon+1).c_str(),nullptr,10);
  uint32_t next;
  portENTER_CRITICAL(&ringLock);next=nextSequence;portEXIT_CRITICAL(&ringLock);
  if(after>=next) after=0;
  slot->client.stop(); slot->client=web.client();
  web.client()=NetworkClient();
  slot->after=after;slot->sent=0;slot->lastStatus=millis();slot->progress=millis();
  slot->pending="HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\nX-Accel-Buffering: no\r\n\r\nretry: 2000\n";
  slot->pending+="id: "+bootId+":"+String(after)+"\nevent: status\ndata: "+statusJson()+"\n\n";
  slot->pending+="event: measurements\ndata: "+measurementsJson()+"\n\n";
}
void streamEvents() {
  for(auto &sub:subscribers) {
    if(!sub.client.connected()) {sub.client.stop();sub.pending="";continue;}
    if(!sub.pending.length()) {
      uint32_t next,oldest;Chunk chunk{};bool have=false;
      portENTER_CRITICAL(&ringLock);
      next=nextSequence;oldest=next>capacity?next-capacity:1;
      if(sub.after+1>=oldest && sub.after+1<next) {
        chunk=chunks[sub.after%capacity];have=true;
      }
      portEXIT_CRITICAL(&ringLock);
      if(sub.after+1<oldest) {
        uint32_t lost=oldest-sub.after-1;sub.after=oldest-1;
        sub.pending="id: "+bootId+":"+String(sub.after)+"\nevent: gap\ndata: {\"boot_id\":\""+bootId+"\",\"dropped_chunks\":"+String(lost)+"}\n\n";
      } else if(have) {
        const char hex[]="0123456789ABCDEF";
        sub.pending="id: "+bootId+":"+String(chunk.sequence)+"\nevent: traffic\ndata: {\"boot_id\":\""+bootId+
          "\",\"sequence\":"+String(chunk.sequence)+",\"ms\":"+String(chunk.ms)+",\"hex\":\"";
        for(unsigned j=0;j<chunk.length;++j) {sub.pending+=hex[chunk.data[j]>>4];sub.pending+=hex[chunk.data[j]&15];}
        sub.pending+="\"}\n\n";sub.after=chunk.sequence;
      } else if(uint32_t(millis()-sub.lastStatus)>=5000) {
        sub.pending="event: status\ndata: "+statusJson()+"\n\nevent: measurements\ndata: "+measurementsJson()+"\n\n";sub.lastStatus=millis();
      }
      sub.sent=0;sub.progress=millis();
    }
    if(sub.pending.length()) {
      int n=::send(sub.client.fd(),sub.pending.c_str()+sub.sent,sub.pending.length()-sub.sent,MSG_DONTWAIT);
      if(n>0) {sub.sent+=n;sub.progress=millis();}
      else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)) {sub.client.stop();continue;}
      if(sub.sent==sub.pending.length()) sub.pending="";
      else if(uint32_t(millis()-sub.progress)>10000) sub.client.stop();
    }
  }
}
const char INDEX[] PROGMEM=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Comfortzone · Live readings</title>
<style>
input,select{display:block;width:100%;margin-top:12px;padding:9px;font:inherit;background:#101b1e;color:#e5f1ec;border:1px solid #2b4045;border-radius:6px}button:disabled{opacity:.45}#control-result{font-size:14px;color:#a2b8b5}:root{color-scheme:dark;--bg:#101b1e;--panel:#19282d;--line:#2b4045;--text:#e5f1ec;--muted:#a2b8b5;--accent:#a8d8a3}*{box-sizing:border-box}body{font:16px system-ui,sans-serif;background:var(--bg);color:var(--text);max-width:1120px;margin:auto;padding:32px 22px}header{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px 20px}.header-diagnostics{display:flex;flex-wrap:wrap;gap:10px 24px;flex-basis:100%;margin:0 0 8px;padding:14px 0;border-top:1px solid var(--line);border-bottom:1px solid var(--line);font-size:12px}.header-diagnostics dt{color:var(--muted);margin-bottom:4px}.header-diagnostics dd{margin:0;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.header-diagnostics.stale dd{color:#eac386}h1{font-weight:550;font-size:36px;letter-spacing:-1px;margin:4px 0}.eyebrow{color:var(--accent);font-size:12px;letter-spacing:2px;text-transform:uppercase}.subtitle,#notice{color:var(--muted);font-size:14px}.connection{border:1px solid var(--line);border-radius:30px;padding:9px 14px;font-size:13px}h2{font-size:16px;font-weight:500;color:var(--muted);margin-top:30px}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:20px}.label{font-size:14px;color:var(--muted);min-height:34px}.value{font-size:34px;font-variant-numeric:tabular-nums;font-weight:500}.unit{font-size:16px;color:var(--accent);margin-left:7px}.age{font-size:12px;color:var(--muted);margin-top:12px}.stale{color:#eac386}.fault{color:#f0a69e}details{margin-top:30px;border-top:1px solid var(--line);padding-top:20px}summary{cursor:pointer;color:var(--muted)}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:12px;max-height:350px;overflow:auto;background:#0c1517;padding:16px;border-radius:10px}button{background:var(--panel);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:8px 12px;margin-top:12px}@media(max-width:650px){body{padding:22px 16px}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.card{padding:15px}.value{font-size:29px}header{align-items:flex-start}.connection{max-width:150px}}
</style>
<header><div><div class="eyebrow">Comfortzone</div><h1>Heat-pump monitor</h1><p class="subtitle">Live heat-pump readings · local connection</p></div><div class="connection" id="connection" role="status">Connecting…</div>
<dl class="header-diagnostics" id="header-diagnostics" aria-label="Device diagnostics">
<div><dt>Wi-Fi signal</dt><dd id="diag-wifi">—</dd></div>
<div><dt>Access point</dt><dd id="diag-ap">—</dd></div>
<div><dt>IP address</dt><dd id="diag-ip">—</dd></div>
<div><dt>Uptime</dt><dd id="diag-uptime">—</dd></div>
<div><dt>Firmware</dt><dd id="diag-firmware">—</dd></div>
<div><dt>Free memory</dt><dd id="diag-heap">—</dd></div>
<div><dt>Bus errors</dt><dd id="diag-errors">—</dd></div>
<div><dt>Diagnostics age</dt><dd id="diag-age">Waiting</dd></div>
</dl></header>
<p id="notice">Waiting for measurements from the heat pump.</p><main id="readings"></main>
<section id="controls"><h2>Controls</h2><div class="grid">
<form class="card" data-command="fan" data-reading="fan_mode"><label>Fan mode <select name="value"><option value="1">Low</option><option value="2">Normal</option><option value="3">High</option></select></label><button disabled>Set fan</button></form>
<form class="card" data-command="room_temperature" data-reading="room_setpoint"><label>Room setpoint (°C) <input name="value" type="number" min="10" max="50" step="0.1" required></label><button disabled>Set temperature</button></form>
<form class="card" data-command="hot_water_temperature" data-reading="hot_water_setpoint"><label>Hot-water setpoint (°C) <input name="value" type="number" min="10" max="60" step="0.1" required></label><button disabled>Set temperature</button></form>
<form class="card" data-command="extra_hot_water" data-reading="extra_hot_water_code"><label>Extra hot water <select name="value"><option value="0">Off</option><option value="1">On</option></select></label><button disabled>Set extra hot water</button></form>
</div><p id="control-result" role="status">Waiting for current settings.</p></section>
<details><summary>Diagnostics &amp; raw traffic</summary><pre id="status"></pre><button id="save">Save displayed traffic</button><pre id="traffic"></pre></details>
<script>
let boot='',lines=[],values=[],receivedAt=0,connected=false,lastEvent=0;
let deviceStatus=null,statusReceivedAt=0;
let stream,resume='';const expanded=new Set();let controlPending=false;
function renderDiagnostics(){
 if(!deviceStatus)return;const s=deviceStatus,elapsed=Math.max(0,Date.now()-statusReceivedAt),stale=elapsed>15000;
 const put=(name,value)=>document.querySelector('#diag-'+name).textContent=value;
 put('wifi',s.rssi_dbm===undefined?'—':s.rssi_dbm+' dBm · ch '+s.wifi_channel);
 put('ap',s.ap_bssid||'—');put('ip',s.ip||'—');put('firmware',(s.firmware||'—').replace(/^comfortzone-monitor-/,''));
 const minutes=Math.floor((s.uptime_ms+(stale?0:elapsed))/60000),days=Math.floor(minutes/1440),hours=Math.floor(minutes/60)%24;
 put('uptime',(days?days+'d ':'')+hours+'h '+minutes%60+'m');
 put('heap',s.free_heap===undefined?'—':Math.round(s.free_heap/1024)+' KiB');
 put('errors',(s.parser_bad_crc??'—')+' CRC / '+(s.uart_errors??'—')+' UART');
 document.querySelector('#diag-errors').classList.toggle('fault',s.parser_bad_crc>0||s.uart_errors>0);
 put('age',(stale?'Stale · ':'')+Math.floor(elapsed/1000)+'s');
 document.querySelector('#header-diagnostics').classList.toggle('stale',stale);
}
function updateControls(){
 for(const form of document.querySelectorAll('#controls form')){
  const m=values.find(x=>x.id===form.dataset.reading);
  const valid=m&&m.state==='ok'&&m.value!==null&&m.age_ms+Date.now()-receivedAt<=30000;
  if(valid&&!form.dataset.initialized){form.elements.value.value=form.dataset.command==='extra_hot_water'?(m.value===32?'1':'0'):m.value;form.dataset.initialized='1';}
  form.querySelector('button').disabled=controlPending||!valid;
 }
}
for(const form of document.querySelectorAll('#controls form'))form.addEventListener('submit',async event=>{
 event.preventDefault();if(controlPending)return;controlPending=true;updateControls();
 const note=document.querySelector('#control-result');note.textContent='Sending request…';
 try{
  const response=await fetch('/api/control/'+form.dataset.command,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Comfortzone-Control':'1'},body:new URLSearchParams({value:form.elements.value.value}).toString()});
  const queued=await response.json();if(!response.ok)throw Error(queued.error||'Request failed');
  note.textContent='Waiting for the pump to confirm the setting…';let finished=false;
  for(let i=0;i<30;++i){
   await new Promise(resolve=>setTimeout(resolve,1000));
   const r=await fetch('/api/control');if(!r.ok)throw Error('Cannot read command status');const state=await r.json();
   if(state.boot_id!==queued.boot_id||state.id!==queued.id)throw Error('Command status changed; check the current setting before trying again.');
   if(['queued','verifying'].includes(state.state))continue;
   note.textContent=state.state==='confirmed'?'Setting confirmed by the heat pump.':'Setting not confirmed ('+state.state+'). Check the current reading before trying again.';finished=true;break;
  }
  if(!finished)note.textContent='Confirmation timed out. Check the current reading before trying again.';
 }catch(error){note.textContent=error.message+' No automatic retry was made.';}
 finally{controlPending=false;updateControls();}
});
function showRaw(){lines=lines.slice(-400);document.querySelector('#traffic').textContent=lines.join('\n');}
function render(){renderDiagnostics();updateControls();const root=document.querySelector('#readings');root.replaceChildren();
 for(const group of [...new Set(values.map(x=>x.group))]){
  let parent=root;
  if(['Alarm flags','Acknowledgement bits','Pump clock','Protocol codes'].includes(group)){
   const section=document.createElement('details');section.open=expanded.has(group);
   const title=document.createElement('summary');title.textContent=group;section.append(title);
   section.addEventListener('toggle',()=>{if(section.isConnected){if(section.open)expanded.add(group);else expanded.delete(group);}});root.append(section);parent=section;
  }else{const title=document.createElement('h2');title.textContent=group;root.append(title);}
  const grid=document.createElement('div');grid.className='grid';parent.append(grid);
  for(const m of values.filter(x=>x.group===group)){
   const age=m.age_ms===null?null:m.age_ms+Date.now()-receivedAt;
   const stale=age!==null&&age>30000;const card=document.createElement('article');card.className='card';
   const label=document.createElement('div');label.className='label';label.textContent=m.label;card.append(label);
   const value=document.createElement('div');value.className='value'+(m.state==='sensor_fault'?' fault':stale?' stale':'');
   value.textContent=m.value===null?'—':m.kind==='boolean'?(m.value?'Yes':'No'):m.kind==='flag'?(m.value?'Set':'Clear'):m.kind==='fan'?({1:'Low',2:'Normal',3:'High'}[m.value]||'Code '+m.value):Number(m.value).toFixed(['°C','Hz'].includes(m.unit)?1:0);
   if(m.kind==='flag'&&m.value)value.classList.add('fault');
   const unit=document.createElement('span');unit.className='unit';unit.textContent=m.unit;value.append(unit);card.append(value);
   const stamp=document.createElement('div');stamp.className='age';stamp.textContent=m.state==='sensor_fault'?'Sensor fault':age===null?'Waiting for data':(stale?'Stale · ':'Updated ')+Math.floor(age/1000)+'s ago';card.append(stamp);
   if(m.evidence==='derived'){const note=document.createElement('div');note.className='age';note.textContent='Calculated locally · active-cycle comparison pending';card.append(note);}
   if(m.evidence==='mapped'){const note=document.createElement('div');note.className='age';note.textContent=m.kind==='flag'?'Raw protocol bit · alarm lifecycle unverified':'Protocol mapping · meaning not fully verified';card.append(note);}
   grid.append(card);
  }
 }
 const fresh=connected&&Date.now()-lastEvent<15000;
 document.querySelector('#connection').textContent=fresh?'● Live connection':'Reconnecting…';
 document.querySelector('#notice').textContent=fresh?'Readings update automatically. Use the controls below to change settings.':'Connection interrupted. Last readings remain visible with their age.';
}
function mark(e){lastEvent=Date.now();if(e&&e.lastEventId)resume=e.lastEventId;}
function connectStream(){
stream=new EventSource('/api/events'+(resume?'?cursor='+encodeURIComponent(resume):''));
stream.addEventListener('status',e=>{mark(e);const s=JSON.parse(e.data);deviceStatus=s;statusReceivedAt=Date.now();renderDiagnostics();if(boot!==s.boot_id){boot=s.boot_id;lines=[];values=[];showRaw();}document.querySelector('#status').textContent=JSON.stringify(s,null,2);});
stream.addEventListener('measurements',e=>{mark(e);const d=JSON.parse(e.data);values=d.measurements;receivedAt=Date.now();render();});
stream.addEventListener('traffic',e=>{mark(e);const c=JSON.parse(e.data);lines.push(c.sequence+' '+c.ms+'ms RX '+c.hex);showRaw();});
stream.addEventListener('gap',e=>{mark(e);lines.push('GAP: '+JSON.parse(e.data).dropped_chunks+' chunks overwritten');showRaw();});
stream.onopen=()=>{connected=true;mark();};stream.onerror=()=>{connected=false;render();if(stream.readyState===EventSource.CLOSED)setTimeout(connectStream,2000);};
}
connectStream();setInterval(render,1000);
document.querySelector('#save').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([lines.join('\n')],{type:'text/plain'}));a.download='comfortzone-'+boot+'.log';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};
</script></html>)HTML";
void setup() {
  digitalWrite(DE_PIN,LOW); pinMode(DE_PIN,OUTPUT);
  Serial.begin(115200);
  bootId=String(esp_random(),HEX)+String(esp_random(),HEX);
  capacity=2048;
  chunks=(Chunk*)heap_caps_calloc(capacity,sizeof(Chunk),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!chunks) {capacity=512; chunks=(Chunk*)calloc(capacity,sizeof(Chunk));}
  if(!chunks) {Serial.println("Capture allocation failed");return;}
  txMutex=xSemaphoreCreateMutex();
  if(!txMutex){Serial.println("TX mutex allocation failed");return;}
  if(xTaskCreatePinnedToCore(captureTask,"rs485",8192,nullptr,2,nullptr,1)!=pdPASS) {
    Serial.println("Capture task failed");return;
  }
  WiFi.mode(WIFI_STA); WiFi.setHostname(DEVICE_HOSTNAME); WiFi.setAutoReconnect(true); WiFi.setSleep(false);
  if(strlen(WIFI_SSID)) startWifiAttempt();
  web.on("/",[]{web.send_P(200,"text/html",INDEX);});
  const char *headers[]={"Last-Event-ID","X-Comfortzone-Control","X-EX50-Control","Content-Type"}; web.collectHeaders(headers,4);
  web.on("/api/control",HTTP_GET,[]{web.send(200,"application/json",controlJson());});
  web.on("/api/control/fan",HTTP_POST,[]{controlPost(comfortzone_control::Kind::Fan);});
  web.on("/api/control/room_temperature",HTTP_POST,[]{controlPost(comfortzone_control::Kind::Room);});
  web.on("/api/control/hot_water_temperature",HTTP_POST,[]{controlPost(comfortzone_control::Kind::Water);});
  web.on("/api/control/extra_hot_water",HTTP_POST,[]{controlPost(comfortzone_control::Kind::Extra);});
  web.on("/api/events",eventsPage);
  web.on("/api/values",[]{web.send(200,"application/json",measurementsJson());});
  web.on("/api/status",statusPage); web.on("/api/traffic",trafficPage); web.begin();
  ArduinoOTA.setHostname(DEVICE_HOSTNAME); ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]{
    otaBusy=true;xSemaphoreTake(txMutex,portMAX_DELAY);digitalWrite(DE_PIN,LOW);
    portENTER_CRITICAL(&controlsLock);controls.interrupt(millis());portEXIT_CRITICAL(&controlsLock);
    xSemaphoreGive(txMutex);
  });
  ArduinoOTA.onError([](ota_error_t e){otaBusy=false;Serial.printf("OTA error %u\n",e);});
  Serial.println(strlen(WIFI_SSID) ? "Comfortzone monitor and controls ready. Connecting to configured Wi-Fi." :
                                 "Comfortzone monitor and controls ready. Wi-Fi configuration missing.");
}
void loop() {
  static bool otaStarted=false,wasConnected=false;
  bool connected=WiFi.status()==WL_CONNECTED;
  if(connected) {
    if(!wasConnected) {++wifiConnections;Serial.printf("IP: %s\n",WiFi.localIP().toString().c_str());}
    if(!otaStarted) {ArduinoOTA.begin();otaStarted=true;}
    ArduinoOTA.handle(); web.handleClient(); streamEvents();
  } else {
    if(wasConnected) {
      // Dispose of stale streams; browsers/collectors will reconnect and replay.
      for(auto &sub:subscribers) {sub.client.stop();sub.pending="";}
      // Give automatic reconnection a full interval before a fresh attempt.
      lastWifiAttempt=millis();
      Serial.println("Wi-Fi lost; retrying indefinitely.");
    }
    if(strlen(WIFI_SSID) && uint32_t(millis()-lastWifiAttempt)>=WIFI_RETRY_MS)
      startWifiAttempt();
  }
  wasConnected=connected; delay(1);
}
