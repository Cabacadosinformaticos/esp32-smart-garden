// The dashboard page of the smart garden, served at /. It is one complete HTML
// document with its CSS and JavaScript in a single raw string literal; the page
// is kept in flash and sent with send_P, so no heap is used to build it.

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h> // Needed for PROGMEM

const char dashboardHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>Smart Garden</title>
<style>
/* Theme: light, dark follows the system setting */
:root{--bg:#f2f5f1;--cd:#fff;--tx:#14201a;--mu:#5d6f64;--ln:#e2e9e3;--tr:#e9efe9;--sh:0 1px 2px #0f1e140d,0 8px 22px #0f1e1410;--ok:#127a45;--wn:#a26200;--al:#c0342a;--ac:#1d8a4e}
@media (prefers-color-scheme:dark){:root{--bg:#0a1410;--cd:#13211a;--tx:#e7f0ea;--mu:#93a89b;--ln:#213429;--tr:#1d3026;--sh:0 1px 2px #0008,0 8px 22px #0006;--ok:#4ad07f;--wn:#f0b13e;--al:#ff6f61;--ac:#4ad07f}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.45 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif;-webkit-text-size-adjust:100%}
.w{max-width:1100px;margin:0 auto;padding:16px 14px 28px}
/* Header */
.hd{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:14px}
.ct svg,.lg{width:17px;height:17px;flex:0 0 auto;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.lg{width:34px;height:34px;color:var(--ac)}
h1{margin:0;font-size:21px;font-weight:700}
.sb{margin:1px 0 0;font-size:13px;color:var(--mu)}
.pl{margin-left:auto;display:inline-flex;align-items:center;gap:7px;padding:6px 13px;border:1px solid var(--ln);border-radius:999px;background:var(--cd);font-size:13px;font-weight:600;color:var(--mu)}
.pl i{width:8px;height:8px;border-radius:50%;background:currentColor}
.pl.live{color:var(--ok);border-color:var(--ok)}
.pl.live i{animation:p 1.8s ease-out infinite}
.pl.off{color:var(--al);border-color:var(--al)}
@keyframes p{0%{box-shadow:0 0 0 0 var(--ok)}to{box-shadow:0 0 0 8px #0000}}
/* Status banner */
.bn{--bc:var(--mu);display:flex;gap:10px;align-items:baseline;flex-wrap:wrap;background:var(--cd);border:1px solid var(--ln);border-left:4px solid var(--bc);border-radius:16px;padding:12px 16px;margin-bottom:16px;box-shadow:var(--sh)}
.bn h2{margin:0;font-size:15px;font-weight:700;color:var(--bc)}
.bn p{margin:0;font-size:13.5px;color:var(--mu)}
.bn p:empty{display:none}
/* Cards */
.g{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px}
.c{background:var(--cd);border:1px solid var(--ln);border-radius:16px;padding:15px 16px 16px;box-shadow:var(--sh);min-width:0}
.ct{display:flex;align-items:center;gap:8px;margin:0 0 12px;font-size:12.5px;font-weight:700;letter-spacing:.05em;text-transform:uppercase;color:var(--mu)}
.b{font-size:30px;font-weight:700;line-height:1.1;font-variant-numeric:tabular-nums}
.u{font-size:15px;font-weight:600;color:var(--mu);margin-left:2px}
.br{height:8px;border-radius:99px;background:var(--tr);overflow:hidden;margin:10px 0 6px}
.fl{height:100%;width:0;border-radius:99px;background:var(--ac);transition:width .5s ease}
.rg{display:flex;justify-content:space-between;font-size:12px;color:var(--mu)}
/* Soil ring */
.gw{position:relative;width:148px;max-width:100%;margin:2px auto 8px}
.gw svg{display:block;width:100%;height:auto}
.gw .b{position:absolute;inset:0;display:flex;align-items:center;justify-content:center}
.gt{fill:none;stroke:var(--tr);stroke-width:10}
.gr{fill:none;stroke-width:10;stroke-linecap:round;stroke-dasharray:289.03;stroke-dashoffset:289.03;stroke:var(--mu);transition:stroke-dashoffset .5s ease}
.tk{stroke:var(--mu);stroke-width:3;stroke-linecap:round}
.tr{display:flex;justify-content:center;gap:18px;font-size:12px;color:var(--mu)}
.tr b{color:var(--tx);font-weight:600}
/* Tank and pump */
.tw{display:flex;justify-content:center;margin:2px 0 6px}
.tw svg{width:104px;height:104px}
.bd{fill:none;stroke:var(--mu);stroke-width:3}
.cp{fill:var(--mu)}
.wt{fill:currentColor;opacity:.9}
.ts{text-align:center;font-size:16px;font-weight:700}
.ps{display:flex;align-items:center;gap:9px;font-size:22px;font-weight:700}
.pd{width:11px;height:11px;border-radius:50%;background:var(--mu);flex:0 0 auto}
.pd.run{background:var(--ok);animation:p 1.4s ease-out infinite}
.pd.wait{background:var(--wn)}
.rs{margin-top:12px;border-top:1px solid var(--ln);padding-top:10px}
.rw{display:flex;justify-content:space-between;gap:12px;font-size:13.5px;padding:3px 0;color:var(--mu)}
.rw b{color:var(--tx);font-weight:600;text-align:right;overflow-wrap:anywhere}
/* Footer */
.ft{display:flex;flex-wrap:wrap;align-items:center;gap:6px 20px;margin-top:18px;padding:14px 4px 0;border-top:1px solid var(--ln);font-size:12.5px;color:var(--mu)}
.fi{display:inline-flex;align-items:center;gap:6px}
.sg{width:19px;height:17px;flex:0 0 auto}
.sg rect{fill:currentColor}
.c,.bn,.ft{transition:.3s}
.off .c,.off .bn,.off .ft{opacity:.45;filter:grayscale(.35)}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style>
</head>
<body>
<div class="w">
<header class="hd">
<svg class="lg" viewBox="0 0 24 24" aria-hidden="true"><path d="M4.5 19.5C4.5 11.6 10.6 5 19.5 4.5c.5 8.9-5.9 15-15 15z"/><path d="M4.5 19.5C8 15 12.4 11.6 16.6 9.6"/></svg>
<div><h1>Smart Garden</h1><p class="sb">Garden at <span id="ipText">--</span></p></div>
<span id="pill" class="pl"><i></i><span id="pillText">Connecting</span></span>
</header>
<main>
<div id="banner" class="bn" role="status" aria-live="polite"><h2 id="bTitle">Reading the garden</h2><p id="bReasons"></p></div>
<div class="g">

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3.5s6.2 6.4 6.2 10.6a6.2 6.2 0 0 1-12.4 0C5.8 9.9 12 3.5 12 3.5z"/></svg>Soil moisture</h2>
<div class="gw">
<svg viewBox="0 0 130 130" aria-hidden="true">
<circle class="gt" cx="65" cy="65" r="46"/>
<circle class="gr" id="soilRing" cx="65" cy="65" r="46" transform="rotate(-90 65 65)"/>
<line class="tk" id="dryTick" x1="65" y1="13" x2="65" y2="7"/>
<line class="tk" id="wetTick" x1="65" y1="13" x2="65" y2="7"/>
</svg>
<div class="b" id="soilVal">--</div>
</div>
<div class="tr"><span>Dry <b id="dryVal">--</b></span><span>Wet <b id="wetVal">--</b></span></div>
</article>

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M6 9h12v10a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2z"/><path d="M9 3h6v6H9z"/><path d="M6 15c2-1.5 4-1.5 6 0s4 1.5 6 0"/></svg>Water tank</h2>
<div class="tw">
<svg id="tankSvg" viewBox="0 0 80 80" aria-hidden="true">
<defs><clipPath id="tankClip"><rect x="19" y="21" width="42" height="48" rx="9"/></clipPath></defs>
<rect class="bd" x="19" y="21" width="42" height="48" rx="9"/>
<rect class="wt" id="tankWater" x="19" y="21" width="42" height="48" clip-path="url(#tankClip)"/>
<rect class="bd" x="33" y="9" width="14" height="13"/>
<rect class="cp" x="29" y="4" width="22" height="6" rx="3"/>
</svg>
</div>
<div class="ts" id="tankText">--</div>
</article>

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0z"/><path d="M12 9v6"/></svg>Temperature</h2>
<div class="b"><span id="tempVal">--</span><span class="u">&deg;C</span></div>
<div class="br"><div class="fl" id="tempBar"></div></div>
<div class="rg"><span id="tempMin">--</span><span id="tempMax">--</span></div>
</article>

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 8c3-2.6 6 2.6 9 0s6 2.6 9 0"/><path d="M3 13c3-2.6 6 2.6 9 0s6 2.6 9 0"/><path d="M3 18c3-2.6 6 2.6 9 0s6 2.6 9 0"/></svg>Air humidity</h2>
<div class="b"><span id="humVal">--</span><span class="u">%</span></div>
<div class="br"><div class="fl" id="humBar"></div></div>
<div class="rg"><span id="humMin">--</span><span id="humMax">--</span></div>
</article>

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.2 5.2l1.4 1.4M17.4 17.4l1.4 1.4M18.8 5.2l-1.4 1.4M6.6 17.4l-1.4 1.4"/></svg>Light</h2>
<div class="b"><span id="lightVal">--</span><span class="u">%</span></div>
<div class="br"><div class="fl" id="lightBar"></div></div>
</article>

<article class="c">
<h2 class="ct"><svg viewBox="0 0 24 24" aria-hidden="true"><rect x="5" y="9" width="14" height="10" rx="3"/><path d="M12 9V4M9 4h6"/></svg>Pump</h2>
<div class="ps"><span class="pd" id="pumpDot"></span><span id="pumpTxt">Idle</span></div>
<div class="rs"><div class="rw"><span>Mode</span><b id="pumpMode">Auto</b></div><div class="rw"><span>Reason</span><b id="pumpReason">--</b></div></div>
<div id="pumpControls"></div>
</article>

</div>
</main>
<footer class="ft">
<span id="uptime">--</span>
<span class="fi"><svg class="sg" id="sig" viewBox="0 0 26 24" aria-hidden="true"><rect x="1" y="16" width="4" height="6" rx="1"/><rect x="8" y="12" width="4" height="10" rx="1"/><rect x="15" y="8" width="4" height="14" rx="1"/><rect x="22" y="3" width="4" height="19" rx="1"/></svg><span id="rssi">--</span></span>
<span id="fIp">--</span>
<span id="mem">--</span>
<span id="ago">Waiting for data</span>
</footer>
</div>
<script>
'use strict';
var M=Math,C=289.03,cfg=null,lastR=null,tOk=0;
function el(i){return document.getElementById(i);}
function set(i,t){el(i).textContent=t;}
function N(v){return typeof v=='number'&&!isNaN(v);}
function num(v,d){return N(v)?(d?v.toFixed(d):String(M.round(v))):'--';}
function clamp(v){return N(v)?M.max(0,M.min(100,v)):0;}
function kb(v){return N(v)?M.round(v/1024)+' KB':'--';}
function fetchJson(url,o){
 var a=new AbortController(),t=setTimeout(function(){a.abort();},4000);
 o=o||{};o.cache='no-store';o.signal=a.signal;
 return fetch(url,o).then(function(r){if(!r.ok){throw Error('http '+r.status);}return r.json();}).then(function(d){clearTimeout(t);return d;},function(e){clearTimeout(t);throw e;});
}
function online(ok){
 document.body.classList.toggle('off',!ok);
 el('pill').className='pl '+(ok?'live':'off');
 set('pillText',ok?'Live':'Offline');
}
function agoText(){
 if(!tOk){set('ago','Waiting for data');return;}
 var s=M.round((Date.now()-tOk)/1000);
 set('ago',s<1?'Updated just now':'Updated '+s+' s ago');
}
function uptime(t){
 if(!N(t)){return '--';}
 var d=M.floor(t/86400),h=M.floor(t%86400/3600),m=M.floor(t%3600/60);
 return d>0?d+'d '+h+'h '+m+'m':h>0?h+'h '+m+'m':m>0?m+'m':M.floor(t%60)+'s';
}
function signal(r){
 var n=r>=-60?4:r>=-70?3:r>=-80?2:r>=-95?1:0,bs=el('sig').children,i;
 for(i=0;i<bs.length;i++){bs[i].setAttribute('opacity',i<n?1:.25);}
}
function tickAt(id,p){
 var e=el(id);
 if(p==null){e.style.display='none';return;}
 e.style.display='';
 var a=((M.max(0,M.min(100,p))*3.6-90)*M.PI)/180,c=M.cos(a),s=M.sin(a);
 e.setAttribute('x1',M.round(65+52*c));e.setAttribute('y1',M.round(65+52*s));
 e.setAttribute('x2',M.round(65+58*c));e.setAttribute('y2',M.round(65+58*s));
}
function banner(r,s){
 var l=[],tx=[],i,lv='',ti='Reading the garden';
 if(s){
  if(!r.soil_ok){l.push(['al','Soil sensor not answering']);}
  else if(N(r.soil)&&r.soil<s.soil_dry){l.push(['wn','Soil is dry']);}
  if(r.tank_empty){l.push(['al','Water tank is empty']);}
  if(!r.dht_ok){l.push(['al','Temperature sensor not answering']);}
  else{
   if(N(r.temperature)&&(r.temperature<s.temp_min||r.temperature>s.temp_max)){l.push(['wn','Temperature outside '+num(s.temp_min)+' to '+num(s.temp_max)+' C']);}
   if(N(r.humidity)&&(r.humidity<s.hum_min||r.humidity>s.hum_max)){l.push(['wn','Air humidity outside '+num(s.hum_min)+' to '+num(s.hum_max)+' %']);}
  }
  if((r.pump.blocked_remaining_s||0)>0){l.push(['wn','Pump paused after a safety stop']);}
  for(i=0;i<l.length;i++){if(l[i][0]=='al'){lv='al';break;}}
  if(!lv){lv=l.length?'wn':'ok';}
  ti=lv=='al'?'Alert':lv=='wn'?'Needs attention':'All good';
 }
 el('banner').style.setProperty('--bc',lv?'var(--'+lv+')':'var(--mu)');
 set('bTitle',ti);
 for(i=0;i<l.length;i++){tx.push(l[i][1]);}
 set('bReasons',tx.join(' | '));
}
function soil(r,s){
 var ok=!!r.soil_ok&&N(r.soil),v=ok?r.soil:null;
 var dr=s&&N(s.soil_dry)?s.soil_dry:null,we=s&&N(s.soil_wet)?s.soil_wet:null;
 var g=el('soilRing'),val=el('soilVal');
 if(v==null){val.textContent='--';g.style.strokeDashoffset=C;}
 else{var p=clamp(v);g.style.strokeDashoffset=M.round(C*(1-p/100));val.textContent=M.round(p)+'%';}
 var st=v==null?'mu':(dr!=null&&v<dr?'wn':'ok');
 g.style.stroke='var(--'+st+')';
 val.style.color=st=='ok'?'':'var(--'+st+')';
 set('dryVal',num(dr));set('wetVal',num(we));
 tickAt('dryTick',dr);tickAt('wetTick',we);
}
function tank(r){
 var e=r.tank_empty,w=el('tankWater'),t=el('tankText');
 w.setAttribute('y',e?66:21);w.setAttribute('height',e?3:48);
 el('tankSvg').style.color=e?'var(--al)':'var(--ac)';
 t.textContent=e?'Tank empty':'Water available';
 t.style.color=e?'var(--al)':'var(--ok)';
}
function metric(p,v,mn,mx){
 set(p+'Val',num(v,1));set(p+'Min',num(mn));set(p+'Max',num(mx));
 var pc=v==null||mn==null||mx==null||mx<=mn?0:M.max(0,M.min(100,((v-mn)/(mx-mn))*100));
 var ok=v!=null&&mn!=null&&mx!=null&&mx>mn&&v>=mn&&v<=mx,st=v==null?'mu':(ok?'ok':'wn');
 var b=el(p+'Bar');
 b.style.width=M.round(pc)+'%';b.style.background='var(--'+st+')';
 el(p+'Val').style.color=st=='ok'?'':'var(--'+st+')';
}
function pump(p){
 var run=p.running,w=M.round(p.blocked_remaining_s||0),st='',ti='Idle';
 if(run){st='ok';ti='Running';}else if(w>0){st='wn';ti='Paused '+w+' s';}
 el('pumpDot').className='pd'+(run?' run':w>0?' wait':'');
 var e=el('pumpTxt');
 e.textContent=ti;e.style.color=st?'var(--'+st+')':'';
 set('pumpMode',p.mode=='manual'?'Manual':'Auto');
 set('pumpReason',p.reason?p.reason:'--');
}
function render(r,s){
 set('ipText',r.ip?r.ip:'--');
 banner(r,s);soil(r,s);tank(r);
 metric('temp',r.dht_ok&&N(r.temperature)?r.temperature:null,s?s.temp_min:null,s?s.temp_max:null);
 metric('hum',r.dht_ok&&N(r.humidity)?r.humidity:null,s?s.hum_min:null,s?s.hum_max:null);
 set('lightVal',num(r.light));
 el('lightBar').style.width=clamp(r.light)+'%';
 pump(r.pump);
 set('uptime',uptime(r.uptime_s));
 set('rssi',N(r.wifi_rssi)?r.wifi_rssi+' dBm':'--');
 signal(r.wifi_rssi);
 set('fIp',r.ip?r.ip:'--');
 set('mem','Memory '+kb(r.heap_free)+' free, low '+kb(r.heap_min));
}
function poll(){
 fetchJson('/api/readings').then(function(r){
  lastR=r;tOk=Date.now();online(true);render(r,cfg);
 },function(){online(false);}).catch(function(){}).then(function(){setTimeout(poll,3000);});
}
function loadCfg(){
 fetchJson('/api/settings').then(function(s){cfg=s;if(lastR){render(lastR,cfg);}},function(){});
}
loadCfg();setInterval(loadCfg,30000);setInterval(agoText,1000);poll();
</script>
</body>
</html>
)rawliteral";

#endif
