#include "WebConfigPage.h"

namespace WebConfigPage {
const char* css() {
    return R"CSS(:root{color-scheme:dark;--bg:#101827;--surface:#1f2937;--surface-alt:#111827;--text:#e5e7eb;--strong:#fff;--muted:#9ca3af;--accent:#65d46e;--border:#374151;--ok:#14532d;--err:#7f1d1d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,sans-serif}
.page,.nav{width:min(720px,calc(100% - 20px));margin:auto}header{padding:12px 0 5px}h1{font-size:22px;margin:0;color:var(--strong)}h2{font-size:16px;margin:0 0 9px;color:var(--strong)}p{margin:5px 0}.muted,.key,footer{color:var(--muted)}
.nav{display:flex;gap:4px;flex-wrap:wrap;padding:4px 0}.nav a{flex:1 1 auto;text-align:center;color:var(--text);text-decoration:none;padding:7px;border:1px solid var(--border);border-radius:8px;background:var(--surface)}.nav a.active{background:var(--ok);border-color:var(--ok);font-weight:700}
main{padding:4px 0 12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:8px}.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:12px;margin:8px 0;min-width:0}.grid .card{margin:0}
.row{display:grid;grid-template-columns:minmax(118px,36%) minmax(0,1fr);gap:8px;padding:6px 0;border-top:1px solid var(--border);align-items:center}.row:first-of-type{border:0}.key{font-size:13px}.value{overflow-wrap:anywhere;color:var(--strong)}
input,select,button{font:inherit}input,select{width:100%;min-height:36px;padding:6px 8px;background:var(--surface-alt);color:var(--strong);border:1px solid #4b5563;border-radius:7px}button{min-height:36px;padding:7px 12px;background:var(--border);color:var(--strong);border:1px solid #4b5563;border-radius:8px;font-weight:700;cursor:pointer}button.primary{background:var(--accent);border-color:var(--accent);color:#102018}button.danger{background:var(--err);border-color:var(--err)}button:disabled{opacity:.5;cursor:wait}button:focus-visible,input:focus-visible,select:focus-visible,a:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.actions{display:flex;gap:6px;flex-wrap:wrap;margin:8px 0}.actions button{flex:1 1 110px}.tag{display:inline-block;border-radius:999px;padding:2px 8px;background:var(--border);font-size:12px;font-weight:700}.tag.ok,.notice.ok{background:var(--ok);color:#bbf7d0}.tag.err,.notice.err{background:var(--err);color:#fecaca}.notice{border-radius:9px;padding:9px 11px;margin:8px 0}.hint{color:var(--muted);font-size:12px}.planned{color:#facc15;font-size:12px}.section-title{font-size:12px;color:var(--muted);margin:12px 0 0}footer{padding:10px 0;border-top:1px solid var(--border);font-size:12px;text-align:center}[hidden]{display:none!important}
@media(max-width:460px){.page,.nav{width:calc(100% - 16px)}.grid{grid-template-columns:1fr}.row{grid-template-columns:minmax(100px,36%) minmax(0,1fr);gap:6px}.card{padding:11px}}
)CSS";
}

const char* html() {
    return R"HTML(<!doctype html><html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/assets/voxone.css"><title>VoxOne</title></head><body>
<header class="page"><h1>VoxOne</h1><p class="muted">Lokalny panel konfiguracji <span class="tag ok">GOTOWY</span></p></header>
<nav class="nav" aria-label="Nawigacja"><a href="#status" data-tab="status">Status</a><a href="#settings" data-tab="settings">Ustawienia</a><a href="#system" data-tab="system">System</a></nav>
<main class="page">
<section id="status"><p class="section-title">Status</p><div class="grid">
<div class="card"><h2>Audio</h2><div class="row"><span class="key">Źródło</span><strong class="value" data-status="audio_source">-</strong></div><div class="row"><span class="key">Właściciel I2S</span><strong class="value" data-status="audio_owner">-</strong></div><div class="row"><span class="key">Głośność</span><strong class="value" data-status="volume">-</strong></div><div class="row"><span class="key">Bluetooth</span><strong class="value" data-status="bluetooth_state">-</strong></div><div class="row"><span class="key">Radio</span><strong class="value" data-status="radio_state">-</strong></div></div>
<div class="card"><h2>Sieć i system</h2><div class="row"><span class="key">Wi-Fi</span><strong class="value" data-status="wifi_status">-</strong></div><div class="row"><span class="key">IP</span><strong class="value" data-status="ip">-</strong></div><div class="row"><span class="key">Hostname</span><strong class="value" data-status="hostname">-</strong></div><div class="row"><span class="key">RSSI</span><strong class="value" data-status="wifi_rssi">-</strong></div><div class="row"><span class="key">Uptime</span><strong class="value" data-status="uptime_s">-</strong></div><div class="row"><span class="key">Heap</span><strong class="value" data-status="free_heap">-</strong></div></div></div>
<section class="card"><h2>Sterowanie</h2><div class="actions"><button type="button" data-action="/previous">Poprzedni</button><button type="button" data-action="/play">PLAY</button><button type="button" data-action="/pause">PAUSE</button><button type="button" data-action="/next">Następny</button><button type="button" data-action="/stop">STOP</button></div><div class="row"><label class="key" for="liveVolume">Głośność</label><input id="liveVolume" type="number" min="0" max="100"></div><button type="button" id="setVolume">Ustaw głośność</button><p class="hint">Sterowanie działa na bieżąco; konfiguracja poniżej wymaga restartu.</p></section></section>
<section id="settings" hidden><p class="section-title">Ustawienia trwałe · obowiązują po restarcie</p><p class="hint">Opcje oznaczone „Obsługa runtime planowana” są zapisywane i walidowane, ale jeszcze nie działają. Dotyczy to także MAX98357A i SSD1306.</p><form id="configForm"><div id="configFields"></div><div class="actions"><button class="primary" type="submit">ZAPISZ</button></div></form></section>
<section id="system" hidden><p class="section-title">System</p><div class="card"><h2>Informacje</h2><div class="row"><span class="key">Firmware</span><strong class="value" data-status="fw">-</strong></div><div class="row"><span class="key">Schema</span><strong class="value" data-status="config_schema">-</strong></div><div class="row"><span class="key">Build</span><strong class="value" data-status="build">-</strong></div><div class="row"><span class="key">Powód restartu</span><strong class="value" data-status="reset_reason">-</strong></div></div><div class="card"><h2>Administracja</h2><div class="actions"><button type="button" id="reboot">Restart</button><button type="button" id="clearWifi">Usuń Wi-Fi</button><button type="button" class="danger" id="reset">Przywróć ustawienia</button></div><p class="hint">Reset zapisuje domyślne ustawienia i uruchamia VoxOne ponownie.</p></div></section>
<p id="notice" class="notice" role="status" hidden></p>
</main><footer class="page">VoxOne · lokalny interfejs WWW</footer>
<script>
(() => {
const $=s=>document.querySelector(s), all=s=>document.querySelectorAll(s), note=$('#notice');
const show=(ok,msg)=>{note.hidden=false;note.className='notice '+(ok?'ok':'err');note.textContent=msg};
const boolOpts=[['0','OFF'],['1','ON']];
const groups=[
['Ogólne','',[
['device.name','Nazwa urządzenia','text',48]]],
['Funkcje','',[
['features.bluetoothEnabled','Bluetooth','bool'],['features.radioEnabled','Radio','bool'],['features.playMediaEnabled','PLAY_MEDIA','bool','planned'],['features.displayEnabled','Display','bool'],['features.encoderEnabled','Encoder','bool'],['features.buttonsEnabled','Buttons','bool','planned'],['features.mqttEnabled','MQTT','bool','planned'],['features.yoRadioWsEnabled','yoRadio WebSocket','bool','planned'],['features.haDiscoveryEnabled','HA Discovery','bool','planned']]],
['Audio','',[
['audio.volume','Głośność','number','0:100'],['audio.maxVolume','Maksymalna głośność logiczna','number','1:100'],['audio.startVolume','Głośność startowa','number','0:100','planned'],['audio.maxOutputVolume','Limit wyjścia fizycznego','number','0:100','planned'],['audio.defaultSource','Źródło domyślne','select',[['0','STOP'],['1','RADIO'],['2','BT']]],['audio.outputType','Typ wyjścia','select',[['0','PCM5102A'],['1','MAX98357A']]],['audio.i2sBclk','I2S BCLK','number','0:39'],['audio.i2sLrclk','I2S LRCLK','number','0:39'],['audio.i2sDout','I2S DOUT','number','0:39']]],
['PLAY_MEDIA','features.playMediaEnabled',[
['playMedia.volumeMode','Tryb głośności','select',[['0','CURRENT'],['1','FIXED']],'planned'],['playMedia.fixedVolume','Stała głośność','number','0:100','planned','playMedia.volumeMode=1']]],
['Bluetooth','features.bluetoothEnabled',[
['bluetooth.deviceName','Nazwa Bluetooth','text',48,'planned'],['bluetooth.autoReconnect','Reconnect','bool'],['bluetooth.reconnectDelayMs','Grace period (ms)','number','0:60000'],['bluetooth.discoverableEnabled','Widoczność','bool','planned'],['bluetooth.discoverableSec','Czas widoczności (s)','number','1:3600','planned'],['bluetooth.rememberLastPeer','Zapamiętaj ostatni telefon','bool','planned']]],
['Radio','features.radioEnabled',[
['radio.defaultStation','Stacja domyślna','number','0:9999','planned'],['radio.autostart','Autostart','bool','planned'],['radio.reconnectEnabled','Reconnect','bool','planned'],['radio.streamTimeoutMs','Timeout streamu (ms)','number','1000:60000','planned'],['radio.icyMetadataEnabled','ICY metadata','bool','planned']]],
['Display','features.displayEnabled',[
['display.type','Typ display','select',[['0','ST7789'],['1','SSD1306']]],['display.brightness','Jasność','number','0:100','planned'],['display.screensaverEnabled','Screensaver','bool','planned'],['display.screensaverTimeoutSec','Timeout screensavera (s)','number','10:86400','planned']]],
['Piny ST7789','display.type=0',[
['display.st7789.sck','SCK','number','0:39'],['display.st7789.mosi','MOSI','number','0:39'],['display.st7789.cs','CS','number','0:39'],['display.st7789.dc','DC','number','0:39'],['display.st7789.rst','RST (-1 = brak)','number','-1:39']]],
['Piny SSD1306','display.type=1',[
['display.ssd1306.sda','SDA','number','0:39','planned'],['display.ssd1306.scl','SCL','number','0:39','planned'],['display.ssd1306.address','Adres I2C','number','8:119','planned']]],
['Encoder','features.encoderEnabled',[
['encoder.pinA','Pin A','number','0:39'],['encoder.pinB','Pin B','number','0:39'],['encoder.pinButton','Przycisk','number','0:39'],['encoder.direction','Kierunek','select',[['0','NORMAL'],['1','REVERSED']]],['encoder.volumeStep','Krok głośności','number','1:10'],['encoder.accelerationEnabled','Przyspieszenie','bool','planned']]],
['MQTT','features.mqttEnabled',[
['mqtt.host','Host','text',128,'planned'],['mqtt.port','Port','number','1:65535','planned'],['mqtt.username','Użytkownik','text',128,'planned'],['mqtt.password','Hasło (puste = zachowaj)','password',128,'planned'],['mqtt.rootTopic','Root topic','text',128,'planned']]],
['yoRadio','features.yoRadioWsEnabled',[
['yoRadio.playlistCompat','Zgodność playlist','bool','planned'],['yoRadio.extensionsEnabled','Rozszerzenia','bool','planned']]],
['Sieć','',[
['network.wifiSsid','Wi-Fi SSID','text',32],['network.wifiPassword','Hasło Wi-Fi (puste = zachowaj)','password',64],['network.hostname','Hostname','text',63,'planned'],['network.mdnsEnabled','mDNS','bool','planned'],['network.dhcpEnabled','DHCP','bool','planned'],['network.ntpEnabled','NTP','bool','planned'],['network.timezone','Strefa czasowa','text',64,'planned']]]
];
let csrf='';
function inputFor(f){const [key,label,type,meta,planned,condition]=f,row=document.createElement('div'),lab=document.createElement('label');row.className='row';row.dataset.condition=condition||'';lab.className='key';lab.textContent=label;let input;if(type==='bool'||type==='select'){input=document.createElement('select');(type==='bool'?boolOpts:meta).forEach(x=>{const opt=document.createElement('option');opt.value=x[0];opt.textContent=x[1];input.append(opt)})}else{input=document.createElement('input');input.type=type;if(type==='number'){const lim=meta.split(':');input.min=lim[0];input.max=lim[1];input.step='1'}else input.maxLength=meta}input.name=key;input.id=key.replaceAll('.','_');lab.htmlFor=input.id;const box=document.createElement('span');box.className='value';box.append(input);if(planned==='planned'||(type==='bool'&&meta==='planned')){const p=document.createElement('small');p.className='planned';p.textContent='Obsługa runtime planowana';box.append(p)}row.append(lab,box);return row}
for(const [title,condition,fields] of groups){const section=document.createElement('section'),h=document.createElement('h2');section.className='card';section.dataset.condition=condition;h.textContent=title;section.append(h);fields.forEach(f=>section.append(inputFor(f)));$('#configFields').append(section)}
const form=$('#configForm');
function visible(expr){if(!expr)return true;const [key,expected]=expr.split('='),input=form.elements.namedItem(key);return input&&(expected===undefined?input.value==='1':input.value===expected)}
function conditions(){all('#configFields [data-condition]').forEach(e=>{const condition=e.dataset.condition;e.hidden=!visible(condition)||(condition.startsWith('display.type=')&&!visible('features.displayEnabled'))})}
form.addEventListener('change',conditions);
function tab(){const current=['settings','system'].includes(location.hash.slice(1))?location.hash.slice(1):'status';['status','settings','system'].forEach(t=>{$('#'+t).hidden=t!==current;$('.nav [data-tab="'+t+'"]').classList.toggle('active',t===current)});note.hidden=true}
window.addEventListener('hashchange',tab);tab();
async function loadConfig(){try{const r=await fetch('/api/v1/config',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const data=await r.json();csrf=data.csrf;for(const input of form.elements){if(input.name&&Object.hasOwn(data,input.name))input.value=String(data[input.name])}conditions()}catch(e){show(false,'Nie można odczytać konfiguracji: '+e.message)}}
async function post(url,body){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});let data;try{data=await r.json()}catch(_){throw Error('Nieprawidłowa odpowiedź serwera')}if(!r.ok||!data.ok)throw Error(data.error||'HTTP '+r.status);return data}
form.addEventListener('submit',async e=>{e.preventDefault();const btn=form.querySelector('button[type=submit]');btn.disabled=true;try{const body=new URLSearchParams(new FormData(form));body.set('_token',csrf);const data=await post('/api/v1/config',body);show(true,data.message||'Ustawienia zapisane. VoxOne zostanie uruchomiony ponownie.')}catch(err){show(false,err.message);btn.disabled=false}});
all('[data-action]').forEach(b=>b.onclick=async()=>{try{await post(b.dataset.action,new URLSearchParams({_token:csrf}));refreshStatus()}catch(e){show(false,e.message)}});
$('#setVolume').onclick=async()=>{const v=$('#liveVolume').value;try{await post('/volume',new URLSearchParams({v,_token:csrf}));refreshStatus()}catch(_){show(false,'Nie można ustawić głośności')}};
async function admin(url,word,code){if(!confirm(word))return;try{const data=await post(url,new URLSearchParams({_token:csrf,confirm:code}));show(true,data.message||'Restart zaplanowany.')}catch(e){show(false,e.message)}}
$('#reboot').onclick=()=>admin('/reboot','Uruchomić VoxOne ponownie?','YES');
$('#clearWifi').onclick=()=>admin('/wifi/clear','Usunąć zapisane Wi-Fi?','YES');
$('#reset').onclick=()=>admin('/api/v1/config/reset','Przywrócić ustawienia domyślne?','RESET');
async function refreshStatus(){try{const r=await fetch('/api/v1/status',{cache:'no-store'});if(!r.ok)return;const d=await r.json();d.wifi_status=d.wifi_connected?'Połączono':'Offline';d.wifi_rssi=d.wifi_connected?d.wifi_rssi+' dBm':'-';d.uptime_s=d.uptime_s+' s';d.free_heap=d.free_heap+' B';d.bluetooth_state=d.bluetooth_connected?(d.bluetooth_playing?'PLAYING':'CONNECTED'):'DISCONNECTED';all('[data-status]').forEach(el=>{el.textContent=d[el.dataset.status]??'-'});$('#liveVolume').max=d.max_volume;if(document.activeElement!==$('#liveVolume'))$('#liveVolume').value=d.volume}catch(_){}}
loadConfig();refreshStatus();setInterval(refreshStatus,5000);
})();
</script></body></html>)HTML";
}
}
