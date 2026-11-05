const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '..');
function script(file) {
  const html = fs.readFileSync(path.join(root, file), 'utf8');
  const match = html.match(/<script>([\s\S]*?)<\/script>/);
  assert.ok(match, `${file}: script missing`);
  new vm.Script(match[1], {filename: file});
  return match[1];
}
function page() {
  const elements = new Map();
  const element = id => {
    if (!elements.has(id)) elements.set(id, {
      style: {}, value: '50', className: '', textContent: '', title: '',
      classList: {active: false, toggle(name, active) { this[name] = active; }, add(name) { this[name] = true; }, remove(name) { this[name] = false; }},
      setAttribute() {}, querySelector() { return {textContent: ''}; }
    });
    return elements.get(id);
  };
  const carriages = [1, 2].map(n => ({dataset: {carriage: String(n)}, classList: {toggle(name, active) { this[name] = active; }}}));
  const programs = [1, 2, 3, 4].map(n => ({dataset: {program: String(n)}, classList: {active: false, toggle(name, active) { this[name] = active; }}}));
  const servos = [45, 90].map(n => ({dataset: {servo: String(n)}, classList: {active: false, toggle(name, active) { this[name] = active; }}}));
  const scroller = {scrollWidth: 0, clientWidth: 0, scrollLeft: 0, addEventListener() {}, setAttribute() {}};
  const document = {getElementById: element, querySelector(q) {
    if (q === 'h1') return element('heading');
    if (q === '.carriages') return scroller;
    return null;
  }, querySelectorAll(q) {
    return q === '.carriage' ? carriages : q === '[data-program]' ? programs :
      q === '[data-servo]' ? servos : q === '.carriage .state' ? [element('state1'), element('state2')] : [];
  }};
  return {document, element, carriages, programs, servos};
}

{
  const ui = page();
  const context = {document: ui.document, location: {search: '?node=offline'}, performance: {now: () => 0},
    requestAnimationFrame() {}, setInterval() { return 1; }, clearInterval() {}, Math, Number};
  vm.runInNewContext(script('web-interface-two-carriages-demo.html'), context);
  assert.equal(ui.element('link').classList.offline, true);
  assert.equal(ui.element('state2').className, 'state off level-0');
  assert.match(ui.element('status').textContent, /Сайт онлайн/);
}

{
  const ui = page();
  let frame;
  const context = {document: ui.document, performance: {now: () => 0}, requestAnimationFrame(callback) { frame = callback; },
    setInterval() { return 1; }, clearInterval() {}, Math, Number};
  vm.runInNewContext(script('web-interface-two-carriages-demo.html'), context);
  assert.equal(ui.element('stop').textContent, 'СТАРТ');
  assert.equal(ui.programs[0].classList.active, true);
  const demo = fs.readFileSync(path.join(root, 'web-interface-two-carriages-demo.html'), 'utf8');
  assert.match(demo, /state online level-2/);
  assert.match(demo, /state online level-4/);
  assert.match(demo, /min="-8" max="8"/);
  assert.match(demo, /\.ipsc \.target-outline\{fill:#FF4D00;stroke:#000/);
  assert.match(demo, /\.ipsc \.target-zone\{fill:none;stroke:#000/);
  assert.match(demo, /\.chain\{width:62\.4px;\s*height:27\.3px/);
  assert.match(demo, /viewBox="0 0 400 520"/);
  assert.match(demo, /\.railWrap\{position:relative;\s*padding:44px 3px 38px\}/);
  assert.match(demo, /\.ipsc\{position:absolute;\s*top:51px;/);
  assert.match(demo, /width:82\.368px;\s*height:108\.576px;/);
  assert.match(demo, /\.ticks\{position:relative;\s*height:45px;\s*margin-top:5px;/);
  assert.match(demo, /\.tick\.major\{width:2px;\s*height:15px;/);
  assert.match(demo, /\.targetDot\{position:absolute;\s*top:46px;/);
  assert.match(demo, /<strong id="pos">0\.00<\/strong><strong> м<\/strong>/);
  assert.ok(demo.indexOf('id="left"') < demo.indexOf('id="zero"'));
  assert.ok(demo.indexOf('id="zero"') < demo.indexOf('id="right"'));
  assert.doesNotMatch(demo, /демо без ESP|Демо: рух/);
  ui.programs[1].onclick();
  assert.equal(ui.element('stop').textContent, 'СТОП');
  assert.equal(ui.carriages[0].classList.moving, true);
  ui.servos[1].onclick();
  assert.equal(ui.servos[1].classList.active, true);
  ui.element('stop').onclick();
  assert.equal(ui.element('stop').textContent, 'СТАРТ');
  assert.equal(ui.carriages[0].classList.moving, false);
  assert.equal(ui.element('servoOff').classList.active, false);
  frame(2500);
  assert.match(ui.element('servoState').textContent, /відкривається|відкрито/);
  ui.element('servoOff').onclick();
  assert.equal(ui.element('servoOff').classList.active, true);
  ui.element('target').value = '2';
  ui.element('target').oninput();
  assert.match(ui.element('servoState').textContent, /вимкнено/);
}

{
  const ui = page();
  const sent = [];
  let socket;
  const context = {document: ui.document, window: {}, location: {hostname: '192.168.4.1'}, performance: {now: () => 0}, Math,
    WebSocket: class { constructor() { socket = this; this.readyState = 1; } send(message) { sent.push(JSON.parse(message)); } },
    setInterval() { return 1; }, clearInterval() {}, setTimeout() {}, Date, JSON, Number};
  vm.runInNewContext(script('ArduinoIDE/12_TwoCarriages_ESP32_Controller/web_ui.h'), context);
  const base = {position: 0, target: 0, speed: 50, program: 1, mode: 'стоп', servoAngle: 45, servoPosition: 0, outputPwm: 0, online: true};
  socket.onmessage({data: JSON.stringify({type: 'state', linked: false, nodeOnline: true,
    carriages: [{...base, rssi: -73}, {...base, rssi: -50}]})});
  assert.equal(ui.element('stop').textContent, 'СТАРТ');
  assert.equal(ui.programs[0].classList.active, true);
  assert.equal(ui.element('state1').className, 'state online level-2');
  assert.equal(ui.element('state2').className, 'state online level-4');
  ui.element('stop').onclick();
  assert.equal(sent.at(-1).type, 'start');
  ui.programs[2].onclick();
  assert.equal(sent.at(-1).type, 'program');
  assert.equal(sent.at(-1).id, 3);
  ui.servos[1].onclick();
  assert.equal(sent.at(-1).angle, 90);
  assert.equal(sent.at(-1).enabled, true);
  ui.element('servoOff').onclick();
  assert.equal(sent.at(-1).enabled, false);
  ui.element('speed').value = '80';
  ui.element('speed').oninput();
  assert.equal(sent.at(-1).type, 'speed');
  socket.onmessage({data: JSON.stringify({type: 'state', linked: false, nodeOnline: true,
    carriages: [{...base, target: 1, mode: 'програма', servoEnabled: true, servoPosition: 45, outputPwm: 200, rssi: -73}, {...base, rssi: -50}]})});
  assert.equal(ui.element('stop').textContent, 'СТОП');
  assert.equal(ui.carriages[0].classList.moving, true);
  assert.equal(ui.carriages[1].classList.moving, false);
  ui.element('stop').onclick();
  assert.equal(sent.at(-1).type, 'stop');
  socket.onmessage({data: JSON.stringify({type: 'state', linked: false, nodeOnline: true,
    carriages: [{...base, mode: 'стоп', servoEnabled: true, servoPosition: 45, rssi: -73}, {...base, rssi: -50}]})});
  assert.equal(ui.element('stop').textContent, 'СТАРТ');
  assert.equal(ui.carriages[0].classList.moving, false);
  assert.equal(ui.element('servoOff').classList.active, false);
  assert.doesNotMatch(ui.element('servoState').textContent, /вимкнено/);
  socket.onmessage({data: JSON.stringify({type: 'state', linked: true, nodeOnline: false,
    carriages: [{...base, rssi: -73}, {...base, online: false, rssi: -127}]})});
  assert.equal(ui.element('link').classList.offline, true);
  assert.match(ui.element('status').textContent, /Сайт онлайн/);
  assert.equal(ui.element('state2').className, 'state off level-0');
  assert.equal(ui.carriages[1].classList.moving, false);
  socket.onmessage({data: JSON.stringify({type: 'state', linked: false, nodeOnline: true,
    carriages: [{...base, rssi: -73}, {...base, rssi: -50}]})});
  assert.equal(ui.element('link').classList.offline, false);
  ui.element('target').value = '2';
  ui.element('target').onchange();
  assert.equal(sent.at(-1).type, 'target');
}

{
  const embedded = fs.readFileSync(path.join(root, 'ArduinoIDE/12_TwoCarriages_ESP32_Controller/web_ui.h'), 'utf8');
  const demo = fs.readFileSync(path.join(root, 'web-interface-two-carriages-demo.html'), 'utf8');
  assert.match(embedded, /\.ipsc \.target-outline\{fill:#FF4D00;stroke:#000/);
  assert.match(embedded, /\.ipsc \.target-zone\{stroke:#000/);
  assert.match(embedded, /\.readout strong\{font-size:1rem;letter-spacing:normal\}/);
  assert.match(embedded, /\.chain\{width:62\.4px;height:27\.3px\}/);
  assert.match(embedded, /\.railWrap\{padding:44px 3px 38px\}\.centre\{top:38px\}/);
  assert.match(embedded, /\.ipsc\{top:51px;width:82\.368px;height:108\.576px;transform:translate\(-50%,-50%\)\}/);
  assert.match(embedded, /\.ticks\{position:relative;height:45px;margin-top:5px;font-size:\.7rem\}/);
  assert.match(embedded, /\.tick\.major\{width:2px;height:15px;/);
  assert.match(embedded, /input\[type=range\]\.target\{top:16px;height:70px\}/);
  assert.match(embedded, /viewBox="0 0 400 520"/);
  const linkedIcon = html => html.match(/<g class="linkedIcon"[\s\S]*?<\/g>/)[0];
  assert.equal(linkedIcon(embedded), linkedIcon(demo));
  assert.equal((linkedIcon(demo).match(/<path /g) || []).length, 3);
  const scale = html => html.match(/<div class="ticks">([\s\S]*?)<\/div>/)[1].replace(/\s+/g, '');
  assert.equal(scale(embedded), scale(demo));
  assert.equal((scale(demo).match(/class="tick/g) || []).length, 17);
  assert.equal((scale(demo).match(/class="tickmajor"/g) || []).length, 9);
  assert.match(scale(demo), /left:62\.5%"><span>\+2<\/span>/);
  assert.match(scale(demo), /left:100%"><span>\+8м<\/span>/);
  const targetPaths = html => [...html.matchAll(/<path class="target-(?:outline|rim|zone)" d="[^"]+"\/>/g)].map(match => match[0]);
  assert.deepEqual(targetPaths(embedded), targetPaths(demo));
  const cartSvg = html => [...html.matchAll(/<svg class="cart"[\s\S]*?<\/svg>/g)].map(match => match[0]);
  assert.equal(cartSvg(demo).length, 2);
  assert.deepEqual(cartSvg(embedded), cartSvg(demo));
  assert.match(cartSvg(demo)[0], /<path class="cable" d="M4 78H206"\/>/);
  assert.match(cartSvg(demo)[0], /class="led-core"/);
  assert.match(embedded, /\.carriage\.moving \.cart \.led-core\{animation:cartLed/);
  assert.match(embedded, /<strong id="pos">0\.00<\/strong><strong> м<\/strong>/);
  assert.match(embedded, /\.chain\.offline::before,\.chain\.offline::after/);
  assert.match(demo, /\.chain\.offline::before,\.chain\.offline::after/);
}

{
  const firmware = fs.readFileSync(path.join(root, 'ArduinoIDE/12_TwoCarriages_ESP32_Controller/firmware_impl.h'), 'utf8');
  const node = fs.readFileSync(path.join(root, 'ArduinoIDE/13_TwoCarriages_NodeMCU/firmware_impl.h'), 'utf8');
  const controllerConfig = fs.readFileSync(path.join(root, 'ArduinoIDE/12_TwoCarriages_ESP32_Controller/config.h'), 'utf8');
  const nodeConfig = fs.readFileSync(path.join(root, 'ArduinoIDE/13_TwoCarriages_NodeMCU/config.h'), 'utf8');
  assert.match(firmware, /stopCarriage\(i, true\)/);
  assert.match(firmware, /if \(c\.servoEnabled && c\.nextServoAt/);
  assert.match(firmware, /c\["servoEnabled"\] = carriages\[i\]\.servoEnabled;/);
  assert.match(controllerConfig, /PACKET_VERSION = 3/);
  assert.match(nodeConfig, /PACKET_VERSION = 3/);
  for (const source of [firmware, node]) {
    assert.match(source, /uint32_t session;\s*uint32_t sequence;/);
    assert.match(source, /static_assert\(sizeof\(CarriagePacket\) == 19/);
  }
  assert.match(firmware, /nodeAcked &&\s*millis\(\) - lastAckAt < NODE_ACK_TIMEOUT_MS/);
  assert.match(firmware, /confirmed \? carriages\[1\]\.outputPwm : int16_t\(0\)/);
  assert.match(firmware, /void updateLocalMotor\(\)/);
  assert.match(node, /statusPending = true;/);
  assert.match(node, /udp\.remoteIP\(\) != CONTROLLER_IP/);
  assert.match(node, /if \(packet\.pwm != 0 \|\| packet\.servoPosition != 0\) return;/);
  assert.match(node, /sendPacket\(PacketType::Status\);  \/\/ ACK/);
}

console.log('PASS: two-carriage demo and firmware UI defaults, controls, servo, signal levels.');
