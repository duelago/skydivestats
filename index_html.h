#pragma once
#include <Arduino.h>

// Webbsidan ligger här eftersom Arduino IDE:s preprocessor inte klarar
// raw string-literaler med JavaScript i .ino-filer.
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="sv"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lavinmätare</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--fg:#1b1b1b;--mut:#777;--bd:#d6d6d6}
@media(prefers-color-scheme:dark){:root{--bg:#141414;--card:#1f1f1f;--fg:#eee;--mut:#999;--bd:#3a3a3a}}
body{font-family:system-ui,-apple-system,sans-serif;margin:0;background:var(--bg);color:var(--fg)}
main{max-width:560px;margin:auto;padding:16px}
h1{font-size:1.4rem;margin:.2em 0 .7em}
section{background:var(--card);border-radius:12px;padding:14px 16px;margin-bottom:14px;box-shadow:0 1px 3px #0002}
h2{font-size:.8rem;margin:0 0 .7em;text-transform:uppercase;letter-spacing:.08em;color:var(--mut)}
.big{font-size:2.8rem;font-weight:700;line-height:1;margin-bottom:.15em}
.l1{color:#6fb12e}.l2{color:#d4b000}.l3{color:#f08a00}.l4,.l5{color:#d40000}.l0{color:var(--mut)}
table{width:100%;border-collapse:collapse;font-size:.92rem}td{padding:3px 0;vertical-align:top}td:first-child{color:var(--mut);width:42%}
button{font:inherit;padding:8px 12px;border-radius:8px;border:1px solid var(--bd);background:var(--bg);color:var(--fg);cursor:pointer}
button.p{background:#c8102e;border-color:#c8102e;color:#fff}
input{font:inherit;padding:7px;border:1px solid var(--bd);border-radius:8px;width:90px;background:var(--bg);color:var(--fg)}
label{display:flex;align-items:center;justify-content:space-between;gap:8px;margin:6px 0}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:6px;margin:8px 0}
.dot{display:inline-block;width:12px;height:12px;border-radius:50%;background:#999;vertical-align:-1px;margin-right:6px}
.dot.on{background:#d40000;box-shadow:0 0 6px #d40000}
.mut{color:var(--mut);font-size:.85rem}.err{color:#d40000;font-weight:600}
ol{padding-left:1.2em;margin:.4em 0}li{margin:.2em 0}
a{color:inherit}
</style></head><body><main>
<h1>Lavinmätare · Glemmtal</h1>

<section>
<h2>Aktuell lavinfara</h2>
<div id="lvl" class="big l0">–</div>
<div id="lvlName" class="mut"></div>
<table style="margin-top:10px">
<tr><td>Ratings i bulletinen</td><td id="raw">–</td></tr>
<tr><td>Gäller till</td><td id="valid">–</td></tr>
<tr><td>Bulletin (datum)</td><td id="src">–</td></tr>
<tr><td>Senaste hämtning</td><td id="last">–</td></tr>
<tr><td>Status</td><td id="fstat">–</td></tr>
<tr><td>Nästa hämtning</td><td id="next">–</td></tr>
<tr><td>Läge</td><td id="mode">–</td></tr>
</table>
<div class="row"><button onclick="post('/api/fetch')">Hämta nu</button></div>
</section>

<section>
<h2>Test</h2>
<div class="row">
<input id="testLvl" type="number" min="0" max="5" value="3">
<button class="p" onclick="post('/api/test?level='+document.getElementById('testLvl').value)">Visa nivå</button>
<button onclick="post('/api/live')">Tillbaka till live</button>
</div>
<div class="mut">1–5 = lavinnivå, 0 = "ingen bulletin". Testläget släpper automatiskt efter 10 min.</div>
</section>

<section>
<h2>Kalibrering</h2>
<table>
<tr><td>Hallsensor</td><td><span id="hdot" class="dot"></span><span id="htxt">–</span></td></tr>
<tr><td>Motor</td><td id="mstate">–</td></tr>
<tr><td>Position (steg från magnet)</td><td id="pos">–</td></tr>
<tr><td>Magnetzon</td><td id="mw">–</td></tr>
<tr><td>Nollpunkt (offset)</td><td id="zero">–</td></tr>
<tr><td>Visaren står på</td><td id="shown">–</td></tr>
</table>
<div id="merr" class="err"></div>
<ol class="mut">
<li>Tryck <b>Hitta magnet &amp; gå till 0</b>.</li>
<li>Finjustera med knapparna tills visaren pekar exakt på 0. Avsluta gärna med framåt-steg (+) så glappet tas upp.</li>
<li>Tryck <b>Spara som 0</b>.</li>
</ol>
<div class="row"><button class="p" onclick="post('/api/home')">Hitta magnet &amp; gå till 0</button></div>
<div class="row">
<button onclick="jog(-100)">−100</button><button onclick="jog(-10)">−10</button><button onclick="jog(-1)">−1</button>
<button onclick="jog(1)">+1</button><button onclick="jog(10)">+10</button><button onclick="jog(100)">+100</button>
</div>
<div class="row"><button onclick="post('/api/setzero')">Spara som 0</button></div>
</section>

<section>
<h2>Inställningar</h2>
<label>Steg per varv <input id="spr" type="number"></label>
<label>Hastighet (steg/s) <input id="speed" type="number"></label>
<label>Region-ID <input id="region" style="width:110px"></label>
<label>Parkering utan bulletin (0–4) <input id="offs" type="number" step="0.1" min="0" max="4"></label>
<label>Vänd rotationsriktning (omstart) <input id="inv" type="checkbox" style="width:auto"></label>
<label>Hallsensor aktiv LÅG <input id="hlow" type="checkbox" style="width:auto"></label>
<div class="row"><button class="p" onclick="saveCfg()">Spara inställningar</button></div>
</section>

<section class="mut">
<a href="/update">Firmwareuppdatering (OTA)</a> ·
<a href="https://lawine.salzburg.at/" target="_blank">lawine.salzburg.at</a><br>
<span id="sys"></span>
</section>

</main>
<script>
const $=id=>document.getElementById(id);
const names=['Ingen bulletin','gering','mäßig','erheblich','groß','sehr groß'];
let cfgLoaded=false;
async function post(u){try{const r=await fetch(u,{method:'POST'});const t=await r.text();if(!r.ok)alert(t);}catch(e){alert(e)}refresh();}
function jog(n){post('/api/jog?steps='+n)}
function fmt(iso){if(!iso)return'–';const d=new Date(iso);return isNaN(d)?iso:d.toLocaleString('sv-SE',{dateStyle:'short',timeStyle:'short'})}
async function refresh(){
 try{
  const s=await (await fetch('/api/status')).json();
  const l=s.level;
  $('lvl').textContent=l<0?'?':(l==0?'–':l);
  $('lvl').className='big l'+Math.max(l,0);
  $('lvlName').textContent=l<0?'Okänd (inte hämtat ännu)':names[l]+(l>=4?' → visas på 0':'');
  $('raw').textContent=s.raw||'–';
  $('valid').textContent=fmt(s.validUntil);
  $('src').textContent=s.source||'–';
  $('last').textContent=s.lastFetch||'–';
  $('fstat').textContent=s.fetchStatus;
  $('next').textContent='om '+s.nextFetchMin+' min';
  $('mode').textContent=s.manual?(s.test?'TEST – visar nivå '+s.testLevel:'Kalibrering'):'Live';
  $('hdot').className='dot'+(s.hall?' on':'');
  $('htxt').textContent=s.hall?'magnet detekterad':'ingen magnet';
  $('mstate').textContent=s.state+(s.homed?'':' · ej homad');
  $('pos').textContent=s.homed?s.pos:'– (kör homing)';
  $('mw').textContent=s.magnetWidth?s.magnetWidth+' steg':'–';
  $('zero').textContent=s.zero+' steg';
  $('shown').textContent=s.shown<0?'okänt':s.shown;
  $('merr').textContent=s.motorError||'';
  $('sys').textContent='v'+s.fw+' · '+s.ip+' · '+s.rssi+' dBm · heap '+s.heap;
  if(!cfgLoaded){$('spr').value=s.spr;$('speed').value=s.speed;$('region').value=s.region;
   $('offs').value=s.offseason;$('inv').checked=s.invert;$('hlow').checked=s.hallLow;cfgLoaded=true;}
 }catch(e){}
}
async function saveCfg(){
 const p=new URLSearchParams({spr:$('spr').value,speed:$('speed').value,region:$('region').value,
  offseason:$('offs').value,invert:$('inv').checked?1:0,hallLow:$('hlow').checked?1:0});
 const r=await fetch('/api/config',{method:'POST',body:p});alert(await r.text());cfgLoaded=false;refresh();
}
setInterval(refresh,1000);refresh();
</script></body></html>)rawliteral";
