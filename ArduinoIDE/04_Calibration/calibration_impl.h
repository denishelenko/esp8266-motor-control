#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "config.h"

ESP8266WebServer server(HTTP_PORT);
DNSServer dns;

bool testRunning = false;
bool testRight = true;
uint8_t testPwmPercent = 100;
uint32_t testStartedAt = 0;
uint32_t testDurationMs = 1000;
int16_t targetPwm = 0;
int16_t appliedPwm = 0;
uint32_t lastRampAt = 0;

const char CALIBRATION_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="uk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Калібрування мотора</title><style>
:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#07111f;color:#eff6ff;padding:14px}main{width:min(560px,100%);background:#101d31;border:1px solid #29405f;border-radius:22px;padding:22px}h1{margin-top:0;font-size:1.35rem}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}label{display:grid;gap:6px;color:#a7bad3;font-size:.85rem}input,select,button{font:inherit;border-radius:11px;border:1px solid #385476;background:#0a1526;color:white;padding:12px}input[type=range]{padding:0;accent-color:#38bdf8}.wide{grid-column:1/-1}.buttons{display:grid;grid-template-columns:2fr 1fr;gap:10px;margin:18px 0}button{font-weight:800;cursor:pointer}.run{background:#0284c7}.stop{background:#dc2626}.result{background:#081426;border-radius:14px;padding:15px;margin-top:16px}.big{font-size:2rem;font-weight:900;color:#38bdf8}.code{font-family:monospace;word-break:break-all;color:#fbbf24}.warn{color:#fca5a5;font-size:.82rem;line-height:1.45}.muted{color:#91a6c2;font-size:.82rem;line-height:1.45}.status{font-weight:800;margin:12px 0}
</style></head><body><main><h1>Калібрування швидкості</h1>
<p class="warn">Підніміть механізм або звільніть траєкторію. Натискання «Запустити тест» одразу вмикає мотор.</p>
<div class="grid"><label>Напрямок<select id="dir"><option value="right">Вправо</option><option value="left">Вліво</option></select></label><label>Час тесту, секунд<input id="seconds" type="number" min="0.2" max="10" step="0.1" value="1.0"></label><label class="wide">PWM: <b id="pwmText">100%</b><input id="pwm" type="range" min="25" max="100" step="1" value="100"></label></div>
<div class="buttons"><button class="run" id="run">▶ Запустити тест</button><button class="stop" id="stop">■ STOP</button></div><div id="status" class="status">Готово до тесту</div>
<label>Виміряна відстань, сантиметрів<input id="distance" type="number" min="0" step="0.1" placeholder="Наприклад: 83.5"></label><button class="wide" id="calculate" style="width:100%;margin-top:10px">Обчислити швидкість</button>
<div class="result"><div class="muted">Швидкість / відстань за 1 секунду</div><div id="mps" class="big">— м/с</div><div id="detail" class="muted">Проведіть тест і введіть фактичну відстань.</div><p id="code" class="code"></p></div>
<p class="muted">Для максимальної швидкості виберіть 100%. Для таблиці повторіть тест на 25, 40, 55, 70, 85 і 100% окремо вправо та вліво. Краще зробити 3 повтори й узяти середнє.</p>
<script>
const $=id=>document.getElementById(id);let lastDuration=1,lastPwm=100,lastDirection='right';const results={};
$('pwm').oninput=()=>$('pwmText').textContent=$('pwm').value+'%';
$('run').onclick=async()=>{lastDuration=Math.max(.2,Math.min(10,Number($('seconds').value)||1));lastPwm=Number($('pwm').value);lastDirection=$('dir').value;$('distance').value='';$('mps').textContent='— м/с';$('code').textContent='';await fetch(`/run?direction=${lastDirection}&pwm=${lastPwm}&ms=${Math.round(lastDuration*1000)}`)};
$('stop').onclick=()=>fetch('/stop');
$('calculate').onclick=()=>{const cm=Number($('distance').value);if(!(cm>0)){alert('Введіть виміряну відстань у сантиметрах');return}const speed=cm/100/lastDuration;$('mps').textContent=speed.toFixed(3)+' м/с';$('detail').textContent=`За ${lastDuration.toFixed(1)} с пройдено ${cm.toFixed(1)} см. За 1 с: ${(speed*100).toFixed(1)} см.`;results[lastPwm]??={};results[lastPwm][lastDirection]=speed;const r=results[lastPwm];$('code').textContent=r.right&&r.left?`{${lastPwm}, ${r.right.toFixed(3)}f, ${r.left.toFixed(3)}f},`:`PWM ${lastPwm}% збережено для напрямку «${lastDirection==='right'?'вправо':'вліво'}». Тепер виміряйте інший напрямок.`};
setInterval(async()=>{try{const s=await(await fetch('/status')).json();$('status').textContent=s.running?`МОТОР ПРАЦЮЄ • залишилось ${(s.remainingMs/1000).toFixed(1)} с`:'Мотор зупинено / готово';$('status').style.color=s.running?'#fbbf24':'#86efac'}catch(e){$('status').textContent='Немає зв’язку'}},150);
</script></main></body></html>
)HTML";

void writeMotor(int16_t pwm) {
  analogWrite(MOTOR_IN1_PIN, pwm > 0 ? pwm : 0);
  analogWrite(MOTOR_IN2_PIN, pwm < 0 ? -pwm : 0);
}

void stopTest() {
  testRunning = false;
  targetPwm = 0;
  appliedPwm = 0;
  writeMotor(0);
}

void updateRamp() {
  if (millis() - lastRampAt < RAMP_INTERVAL_MS) return;
  lastRampAt = millis();
  if (appliedPwm < targetPwm) {
    appliedPwm += RAMP_STEP;
    if (appliedPwm > targetPwm) appliedPwm = targetPwm;
  } else if (appliedPwm > targetPwm) {
    appliedPwm -= RAMP_STEP;
    if (appliedPwm < targetPwm) appliedPwm = targetPwm;
  }
  writeMotor(appliedPwm);
}

void handleRun() {
  if (!server.hasArg("direction") || !server.hasArg("pwm") || !server.hasArg("ms")) {
    server.send(400, "application/json", "{\"error\":\"missing arguments\"}");
    return;
  }
  testRight = server.arg("direction") != "left";
  testPwmPercent = constrain(server.arg("pwm").toInt(), 25, 100);
  testDurationMs = constrain(server.arg("ms").toInt(), 200, 10000);
  const int16_t magnitude = map(testPwmPercent, 0, 100, 0, PWM_MAX);
  targetPwm = testRight ? magnitude : -magnitude;
  appliedPwm = 0;
  testStartedAt = millis();
  testRunning = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus() {
  uint32_t remaining = 0;
  if (testRunning) {
    const uint32_t elapsed = millis() - testStartedAt;
    remaining = elapsed < testDurationMs ? testDurationMs - elapsed : 0;
  }
  String json = String("{\"running\":") + (testRunning ? "true" : "false") +
                ",\"remainingMs\":" + remaining +
                ",\"pwm\":" + testPwmPercent + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteRange(PWM_MAX);
  analogWriteFreq(PWM_FREQUENCY_HZ);
  stopTest();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setOutputPower(20.5f);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP("Motor-Calibration", WIFI_PASSWORD, 6, false, 2);
  dns.start(53, "*", IPAddress(192,168,4,1));
  MDNS.begin("motor");

  server.on("/", [](){ server.send_P(200, "text/html; charset=utf-8", CALIBRATION_PAGE); });
  server.on("/run", handleRun);
  server.on("/stop", [](){ stopTest(); server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/status", handleStatus);
  server.onNotFound([](){ server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
  Serial.println("Join Motor-Calibration and open http://motor.local");
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  MDNS.update();
  if (testRunning && millis() - testStartedAt >= testDurationMs) stopTest();
  updateRamp();
}


