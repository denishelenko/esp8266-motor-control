#pragma once

#if defined(DEVICE_ROLE_SINGLE)
#define WEB_SPEED_MIN "20"
#define WEB_SPEED_MAX "60"
#define WEB_SPEED_DEFAULT "40"
#else
#define WEB_SPEED_MIN "25"
#define WEB_SPEED_MAX "100"
#define WEB_SPEED_DEFAULT "55"
#endif

const char WEB_UI[] PROGMEM = R"HTML(
<!doctype html><html lang="uk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<title>Бігунок</title>
<style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;--ink:#1A1F1B;--panel:#2F3A33;--mid:#5E6A61;--accent:#9FA38B;--paper:#E7E6DD;--line:#5E6A61;--muted:#9FA38B}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}html{-webkit-text-size-adjust:100%;overflow-x:hidden}body{margin:0;min-height:100vh;background:#1A1F1B;color:var(--paper);display:grid;place-items:center;padding:14px;overflow-x:hidden}
main{width:min(680px,100%);background:#2F3A33;border:1px solid var(--line);border-radius:24px;padding:20px;box-shadow:0 25px 70px #0009}header{display:flex;justify-content:space-between;gap:12px;align-items:center}h1{font-size:1.25rem;margin:0}.pill{font-size:.78rem;color:var(--muted);display:flex;align-items:center;gap:7px}.dot{width:9px;height:9px;border-radius:50%;background:var(--mid)}.dot.ok{background:var(--accent);box-shadow:0 0 9px #9FA38B88}
.position{text-align:center;margin:25px 0 4px}.position strong{font-size:2.7rem;letter-spacing:-.05em}.position span{color:var(--muted)}
.railWrap{position:relative;padding:22px 0 28px}.rail{height:12px;border-radius:99px;background:#5E6A61;box-shadow:inset 0 2px 5px #0009,0 0 0 1px #5E6A6155}.centre{position:absolute;left:50%;top:16px;width:2px;height:24px;background:#E7E6DD88}.actual{position:absolute;top:12px;width:20px;height:20px;margin-left:-10px;border:4px solid var(--paper);border-radius:50%;background:var(--panel);box-shadow:0 0 10px #E7E6DD55;pointer-events:none;transition:left .12s linear}.ticks{display:flex;justify-content:space-between;color:var(--muted);font-size:.78rem;margin-top:9px}
input[type=range]{position:absolute;left:-2px;top:6px;width:calc(100% + 4px);height:38px;margin:0;opacity:.01;cursor:pointer}.targetLabel{position:absolute;top:0;transform:translate(-50%,-70%);color:var(--paper);font-size:.77rem;font-weight:800;pointer-events:none}
.speed{display:grid;grid-template-columns:auto 1fr 48px;gap:12px;align-items:center;margin:12px 0 18px;color:var(--muted)}.speed input{position:static;width:100%;height:auto;opacity:1;accent-color:var(--accent)}.speed output{color:var(--paper);text-align:right;font-weight:800}
.stop{width:100%;border:1px solid var(--accent);border-radius:15px;min-height:58px;background:#5E6A61;color:var(--paper);font-weight:900;font-size:1.05rem;cursor:pointer}.programs{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:16px}.programs button,.cal button{border:1px solid var(--mid);background:#1A1F1B;color:var(--paper);border-radius:13px;min-height:50px;font-weight:750;cursor:pointer}.programs button.active{border-color:var(--paper);background:var(--mid)}.programs small{display:block;color:var(--accent);font-weight:500;margin-top:3px}
details{margin-top:14px;border-top:1px solid var(--line);padding-top:13px}summary{color:var(--muted);cursor:pointer}.cal{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:12px}.cal .zero{grid-column:1/-1}.note{font-size:.76rem;line-height:1.4;color:var(--muted);margin:13px 2px 0}button:disabled{opacity:.45;filter:grayscale(1)}
@media(max-width:480px){
 body{display:block;min-height:100vh;min-height:100svh;padding:0;background:var(--ink);overflow-y:auto}
 main{width:100%;min-height:100vh;min-height:100svh;border:0;border-radius:0;padding:max(20px,env(safe-area-inset-top)) max(17px,env(safe-area-inset-right)) max(24px,env(safe-area-inset-bottom)) max(17px,env(safe-area-inset-left));box-shadow:none}
 header{min-height:44px}h1{font-size:1.2rem}.pill{font-size:.73rem}
 .position{margin:18px 0 2px}.position strong{font-size:2.45rem}
 .railWrap{padding:30px 2px 32px}.rail{height:15px}.centre{top:22px;height:28px}.actual{top:16px;width:27px;height:27px;margin-left:-13.5px;border-width:5px}.targetLabel{top:5px;font-size:.8rem}
 input[type=range]{top:5px;height:55px;touch-action:pan-x}
 .speed{grid-template-columns:auto 1fr 46px;gap:10px;margin:8px 0 17px}.speed input{height:32px}
 .stop{min-height:64px;border-radius:16px;font-size:1rem}
 .programs{gap:10px;margin-top:14px}.programs button,.cal button{min-height:62px;padding:8px 6px;touch-action:manipulation}
 details{margin-top:15px;padding-top:15px}summary{min-height:44px;display:flex;align-items:center}.cal{gap:10px}.note{font-size:.8rem}
}
@media(max-width:350px){.programs,.cal{grid-template-columns:1fr}.cal .zero{grid-column:auto}.position strong{font-size:2.15rem}}
</style></head><body><main>
<header><h1>Бігунок</h1><div class="pill"><i id="dot" class="dot"></i><span id="status">підключення…</span></div></header>
<div class="position"><strong id="pos">0.00</strong> <span>м від центру</span></div>
<div class="railWrap"><div class="rail"></div><div class="centre"></div><i id="actual" class="actual" style="left:50%"></i><span id="targetLabel" class="targetLabel" style="left:50%">ціль 0.00</span><input id="target" type="range" min="-1.8" max="1.8" step="0.05" value="0"><div class="ticks"><span>−1.8 м</span><span>центр</span><span>+1.8 м</span></div></div>
<div class="speed"><label for="speed">Потужність</label><input id="speed" type="range" min=")HTML" WEB_SPEED_MIN R"HTML(" max=")HTML" WEB_SPEED_MAX R"HTML(" step="1" value=")HTML" WEB_SPEED_DEFAULT R"HTML("><output id="speedOut">)HTML" WEB_SPEED_DEFAULT R"HTML(%</output></div>
<button id="stop" class="stop" disabled>СТОП</button>
<div class="programs">
 <button data-program="1" disabled>Маятник<small>від краю до краю</small></button><button data-program="2" disabled>Драбинка<small>амплітуда зростає</small></button>
 <button data-program="3" disabled>Ривки<small>нерівний маршрут</small></button><button data-program="4" disabled>Хаос<small>випадкові точки</small></button>
</div>
<details><summary>Ручне виставлення і калібрування</summary><div class="cal"><button id="jogLeft" disabled>◀ тримати: вліво</button><button id="jogRight" disabled>тримати: вправо ▶</button><button id="zero" class="zero" disabled>Позначити поточне місце як центр</button></div><p class="note">Для точності спочатку вручну поставте механізм у фізичний центр і натисніть «Позначити…». Швидкості у м/с калібруються в <b>include/config.h</b>. Без енкодера координата є розрахунковою.</p></details>
</main><script>
(()=>{const $=id=>document.getElementById(id),buttons=[...document.querySelectorAll('button')];let ws,jogTimer,drag=false,lastSend=0;
const send=o=>{if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))};
function targetVisual(v){const min=Number($('target').min),max=Number($('target').max),pct=(v-min)/(max-min)*100;$('targetLabel').style.left=pct+'%';$('targetLabel').textContent='ціль '+Number(v).toFixed(2)}
function setOnline(ok,text){$('dot').classList.toggle('ok',ok);$('status').textContent=text;buttons.forEach(b=>b.disabled=!ok)}
function stopJog(){clearInterval(jogTimer);jogTimer=null;send({type:'jog',direction:'stop',speed:0})}
function connect(){ws=new WebSocket(`ws://${location.hostname}:81/`);ws.onopen=()=>{setOnline(true,'сервер онлайн');send({type:'hello',role:'ui'})};ws.onclose=()=>{setOnline(false,'зв’язок втрачено');clearInterval(jogTimer);setTimeout(connect,1000)};ws.onerror=()=>ws.close();ws.onmessage=e=>{let s;try{s=JSON.parse(e.data)}catch(_){return}if(s.type!=='state')return;
const p=Number(s.position)||0,min=Number($('target').min),max=Number($('target').max);$('pos').textContent=p.toFixed(2);$('actual').style.left=((p-min)/(max-min)*100)+'%';if(!drag){$('target').value=s.target;$('speed').value=s.speed||$('speed').value;$('speedOut').textContent=$('speed').value+'%';targetVisual(s.target)}
const motor=s.motorOnline?(s.transport==='esp-now'?'ESP-NOW • мотор онлайн':'мотор онлайн'):'мотор офлайн';$('status').textContent=(s.mode==='idle'?motor:s.mode+' • '+motor);document.querySelectorAll('[data-program]').forEach(b=>b.classList.toggle('active',Number(b.dataset.program)===Number(s.program)))}}
$('target').onpointerdown=()=>drag=true;$('target').oninput=()=>{targetVisual($('target').value);const now=Date.now();if(now-lastSend>30){lastSend=now;send({type:'target',position:Number($('target').value),speed:Number($('speed').value)})}};$('target').onchange=()=>{drag=false;send({type:'target',position:Number($('target').value),speed:Number($('speed').value)})};
$('speed').oninput=()=>$('speedOut').textContent=$('speed').value+'%';$('stop').onclick=()=>send({type:'stop'});$('zero').onclick=()=>send({type:'zero'});document.querySelectorAll('[data-program]').forEach(b=>b.onclick=()=>send({type:'program',id:Number(b.dataset.program),speed:Number($('speed').value)}));
for(const [id,dir] of [['jogLeft','left'],['jogRight','right']]){const b=$(id);b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);send({type:'jog',direction:dir,speed:Number($('speed').value)});jogTimer=setInterval(()=>send({type:'jog',direction:dir,speed:Number($('speed').value)}),200)};b.onpointerup=stopJog;b.onpointercancel=stopJog;b.onlostpointercapture=stopJog}
window.onblur=stopJog;targetVisual(0);connect()})();
</script></body></html>
)HTML";

#undef WEB_SPEED_MIN
#undef WEB_SPEED_MAX
#undef WEB_SPEED_DEFAULT
