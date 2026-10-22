const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const uiFile = process.argv[2] || 'include/web_ui.h';
function scripts(file) {
  const text = fs.readFileSync(path.join(root, file), 'utf8');
  return [...text.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
}
for (const file of ['include/web_ui.h','src/calibration.cpp','web-interface-demo.html']) {
  for (const js of scripts(file)) new vm.Script(js, {filename:file});
}
const elements = new Map();
function element(id) {
  if (!elements.has(id)) elements.set(id, {
    style:{}, dataset:{}, classList:{toggle(){}}, value:'50', min:'-1.8', max:'1.8',
    setPointerCapture(){}, setAttribute(){}, disabled:true, hidden:false
  });
  return elements.get(id);
}
const program = [1,2,3,4].map(n => ({...element('program'+n),dataset:{program:String(n)}}));
const angles = [45,90].map(n => ({...element('angle'+n),dataset:{servoAngle:String(n)}}));
const sent = [], timers = new Map(), events = {};
let timerId = 0, socket;
const document = {
  hidden:false, getElementById:element, addEventListener:(name,fn)=>events[name]=fn,
  querySelector:element,
  querySelectorAll(selector){
    if (selector==='[data-program]')return program;
    if (selector==='[data-servo-angle]')return angles;
    if (selector==='button')return [...program,...angles,element('stop'),element('jogLeft'),element('jogRight')];
    return [];
  }
};
const window = {};
const context = {
  document, window, location:{hostname:'192.168.4.1'},
  WebSocket:class{
    constructor(){this.readyState=1;socket=this}
    send(text){sent.push(JSON.parse(text))}
    close(){this.readyState=3}
  },
  setInterval(fn){const id=++timerId;timers.set(id,fn);return id},
  clearInterval(id){timers.delete(id)},setTimeout(){},Date,JSON,Number,Boolean
};
vm.runInNewContext(scripts(uiFile)[0],context);
socket.onopen();
assert.equal(sent.at(-1).type,'hello');
if (uiFile.includes('standalone-led')) {
  element('light').onclick();
  assert.equal(sent.at(-1).type,'light');
  assert.equal(sent.at(-1).enabled,true);
  socket.onmessage({data:JSON.stringify({type:'state',position:0,target:0,limit:1.8,
    speed:50,motorOnline:true,transport:'local',mode:'ручний рух',servoAvailable:true,
    lightOn:true,lightStrobe:true,lightOutput:true})});
  assert.equal(element('light').textContent,'Світло: строб');
  socket.onmessage({data:JSON.stringify({type:'state',position:0,target:0,limit:1.8,
    speed:50,motorOnline:true,transport:'local',mode:'idle',servoAvailable:true,
    lightOn:true,lightStrobe:false,lightOutput:true})});
  assert.equal(element('light').textContent,'Світло: увімкнено');
  element('light').onclick();
  assert.equal(sent.at(-1).enabled,false);
}
sent.length=0;
window.onblur();
assert.equal(sent.length,0,'observer blur must not stop another client');
for (const fn of [...timers.values()])fn();
assert.equal(sent.at(-1).type,'ping');
element('speed').value='100';element('speed').onchange();
assert.equal(sent.at(-1).speed,100);
element('speed').value='1';element('speed').onchange();
assert.equal(sent.at(-1).speed,1);
socket.onmessage({data:JSON.stringify({type:'state',position:0,target:.5,limit:3,speed:50,
  motorOnline:true,transport:'local',mode:'idle',program:0,servoAvailable:false})});
assert.equal(element('target').min,-3);
assert.equal(element('target').max,3);
assert.equal(element('.servoPanel').hidden,true);
element('jogLeft').onpointerdown({preventDefault(){},pointerId:1});
assert.equal(sent.at(-1).direction,'left');
element('jogLeft').onpointerup();
assert.equal(sent.at(-1).direction,'stop');
const count=sent.length;
element('jogLeft').onlostpointercapture();
assert.equal(sent.length,count,'stop only once');
document.hidden=true;events.visibilitychange();
assert.equal(sent.at(-1).type,'release');
sent.length=0;
for(const fn of [...timers.values()])fn();
assert.equal(sent.length,0,'hidden tab must not renew a motion lease');
element('jogRight').onpointerdown({preventDefault(){},pointerId:2});
socket.onclose();window.onblur();
assert.equal(sent.at(-1).direction,'right','disconnect clears jog without stale stop');
console.log('PASS: JS syntax (firmware/calibration/demo), heartbeat, multi-client blur, 1/100% speed, track bounds, jog release, background/disconnect.');
