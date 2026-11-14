/*
 * ESP32 Player Card - a Minecraft-inspired mini portfolio display
 * Board: Freenove ESP32 Display FNK0114B (2.8" ST7789, 320x240, resistive touch)
 *
 * - Player card: glowing pixel name, photo "GIF", hearts, XP bar (fills toward your next birthday),
 *   hotbar of apps with tap-to-open descriptions, twinkling stars, battery indicator
 * - Five username animations
 * - Phone remote: open the board's address in a browser on the same Wi-Fi (2.4 GHz)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>

// ===================== 1. YOUR WI-FI + BIRTHDAY =====================
// Your real details live in secrets.h (next to this file). It is NOT uploaded to GitHub.
// Copy secrets.example.h to secrets.h and fill it in.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #define SECRET_SSID   "YOUR_WIFI_NAME"
  #define SECRET_PASS   "YOUR_WIFI_PASSWORD"
  #define SECRET_BIRTH_YEAR  2000
  #define SECRET_BIRTH_MONTH 1
  #define SECRET_BIRTH_DAY   1
#endif
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PASS = SECRET_PASS;

// ===================== 2. YOUR CONTENT ===================
const char* MY_NAME    = "Your Name";
const char* MY_TITLE   = "Student | Maker | Designer";
const char* ABOUT_TEXT = "Hi! I build things with code and hardware. "
                         "This tiny screen is my pocket portfolio - "
                         "made with an ESP32.";

struct Project { const char* title; const char* desc; };
Project projects[] = {
  { "Mini Portfolio",   "This device! ESP32 + touch screen + phone control." },
  { "Project Two",      "Short description of something you made." },
  { "Project Three",    "Another cool thing you're proud of." },
};
const int NUM_PROJECTS = sizeof(projects) / sizeof(projects[0]);

struct Skill { const char* name; int percent; };
Skill skills[] = {
  { "C++ / Arduino", 70 },
  { "Python",        80 },
  { "SQL",           65 },
  { "UI Design",     75 },
  { "Video / AE",    60 },
};
const int NUM_SKILLS = sizeof(skills) / sizeof(skills[0]);

const char* CONTACT_EMAIL    = "you@example.com";
const char* CONTACT_GITHUB   = "github.com/yourname";
const char* CONTACT_LINKEDIN = "linkedin.com/in/yourname";

// Shown on the animated screens (keep it about 10 characters or less for big text)
const char* USERNAME = "username";

// ===================== PHOTO "GIF" + NAME TAG ===========
const char* TAG_TEXT = "Carol";   // pixel name at the top of the card
#define NAME_SCALE   3            // size of the pixel name
#define FRAME_MS     1200         // time each photo stays on screen (ms)

// Hearts = year of study
#define HEARTS_FULL  6
#define HEARTS_TOTAL 7
#define HEART_GAP    14   // space from one heart to the next

// XP bar fills from one birthday to the next; level = your age
#define BIRTH_YEAR   SECRET_BIRTH_YEAR    // set in secrets.h
#define BIRTH_MONTH  SECRET_BIRTH_MONTH
#define BIRTH_DAY    SECRET_BIRTH_DAY

// ===================== DISPLAY + TOUCH ===================
TFT_eSPI tft = TFT_eSPI();

#define DOUT 39
#define DIN  32
#define DCS  33
#define DCLK 25
TFT_Touch touch = TFT_Touch(DCS, DCLK, DIN, DOUT);

#define W 320
#define H 240

// Colors
uint16_t BG, CARD, TEXT, MUTED, ACCENT, ACCENT2;

// Screens
enum { SCR_HOME, SCR_ABOUT, SCR_PROJECTS, SCR_SKILLS, SCR_CONTACT, NUM_SCREENS, SCR_MESSAGE, SCR_ANIM, SCR_PHOTO, SCR_SLOT };
int currentScreen = SCR_HOME;
bool needsRedraw = true;
String customMessage = "";
unsigned long lastTouch = 0;

WebServer server(80);
String ipText = "";

// ===================== PHONE WEB PAGE ====================
const char PAGE[] = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#0b0b0f">
<title>Carol &middot; Display</title>
<style>
:root{--bg:#0b0b0f;--s1:#15151c;--s2:#1e1e27;--line:rgba(255,255,255,.07);--tx:#f4f4f6;--mu:#8b8b96;--ac:#8b7bff;--ok:#5ee07a;--bad:#ff6b6b;--r:18px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 -apple-system,BlinkMacSystemFont,"SF Pro Text","Segoe UI",Roboto,sans-serif;padding:env(safe-area-inset-top) 0 calc(88px + env(safe-area-inset-bottom))}
header{padding:22px 20px 10px;display:flex;align-items:flex-end;justify-content:space-between}
.eyebrow{font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--mu)}
h1{margin:2px 0 0;font-size:28px;font-weight:700;letter-spacing:-.02em}
.pill{display:flex;align-items:center;gap:7px;font-size:12px;color:var(--mu);background:var(--s1);border:1px solid var(--line);padding:7px 11px;border-radius:99px}
.dot{width:7px;height:7px;border-radius:50%;background:var(--mu)}
.dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
main{padding:6px 16px}
.tab{display:none}.tab.show{display:block;animation:in .25s ease}
@keyframes in{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
.card{background:var(--s1);border:1px solid var(--line);border-radius:var(--r);padding:16px;margin-bottom:14px}
.label{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--mu);margin:0 0 10px;font-weight:600}
.hint{font-size:13px;color:var(--mu);margin:-4px 0 14px}
button{font:inherit;font-weight:600;color:var(--tx);background:var(--s2);border:1px solid var(--line);border-radius:12px;padding:13px 14px;cursor:pointer;transition:transform .08s,background .15s}
button:active{transform:scale(.97)}
.primary{background:var(--ac);border-color:transparent;color:#fff}
.ghost{background:transparent;border-color:transparent}
.danger{color:var(--bad)}
.full{width:100%}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.now{display:flex;align-items:center;gap:14px}
.now .t{flex:1;text-align:center;min-width:0}
.now .big{font-size:20px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.now .sub{font-size:13px;color:var(--mu)}
.round{width:52px;height:52px;border-radius:50%;padding:0;font-size:16px;flex:none}
input[type=text],textarea{width:100%;font:inherit;color:var(--tx);background:var(--s2);border:1px solid var(--line);border-radius:12px;padding:12px 14px;outline:none;-webkit-appearance:none}
input[type=text]:focus,textarea:focus{border-color:var(--ac)}
textarea{min-height:92px;resize:vertical}
.field{margin-bottom:10px}
.row{display:flex;align-items:center;gap:10px;padding:11px 0;border-top:1px solid var(--line)}
#pl .row:first-child{border-top:0;padding-top:0}
.row .name{flex:1;font-weight:500}
.row.off .name{color:var(--mu)}
.step{display:flex;align-items:center;background:var(--s2);border:1px solid var(--line);border-radius:10px}
.step button{border:0;background:none;padding:6px 10px;border-radius:10px;color:var(--mu)}
.step span{min-width:36px;text-align:center;font-variant-numeric:tabular-nums;font-size:14px}
.mv{display:flex;flex-direction:column;gap:3px}
.mv button{padding:1px 7px;font-size:10px;border-radius:6px;color:var(--mu)}
.sw{position:relative;width:46px;height:28px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw i{position:absolute;inset:0;background:var(--s2);border:1px solid var(--line);border-radius:99px;transition:.2s}
.sw i:before{content:"";position:absolute;width:22px;height:22px;left:2px;top:2px;background:#fff;border-radius:50%;transition:.2s}
.sw input:checked+i{background:var(--ok);border-color:transparent}
.sw input:checked+i:before{transform:translateX(18px)}
.hotbar{display:flex;gap:6px;justify-content:center;margin:4px 0 16px}
.slot{width:54px;height:54px;border-radius:4px;background:#2d2d2d;border:3px solid #6f6f6f;display:flex;align-items:center;justify-content:center;overflow:hidden;padding:0}
.slot img{width:100%;height:100%;object-fit:contain;image-rendering:pixelated}
.slot span{color:#777;font-size:13px}
.slot.sel{border-color:#fff;box-shadow:0 0 0 3px var(--ac)}
.thumbs{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-bottom:12px}
.thumbs img{width:100%;aspect-ratio:196/111;object-fit:cover;border-radius:10px;border:1px solid var(--line);display:block}
.empty{color:var(--mu);font-size:13px;grid-column:1/-1;padding:18px 0;text-align:center;border:1px dashed var(--line);border-radius:12px}
.file{display:block;text-align:center;padding:13px;border:1px dashed rgba(255,255,255,.18);border-radius:12px;color:var(--mu);font-weight:600;margin-bottom:10px;cursor:pointer}
.file input{display:none}
.anim{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.anim button{text-align:left;padding:16px}
.anim small{display:block;color:var(--mu);font-weight:400;font-size:12px;margin-top:3px}
nav{position:fixed;left:0;right:0;bottom:0;display:flex;background:rgba(11,11,15,.82);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);border-top:1px solid var(--line);padding:8px 8px calc(8px + env(safe-area-inset-bottom))}
nav button{flex:1;background:none;border:0;color:var(--mu);font-size:11px;font-weight:600;padding:6px 0;display:flex;flex-direction:column;align-items:center;gap:4px}
nav svg{width:22px;height:22px}
nav button.on{color:var(--tx)}
#toast{position:fixed;left:50%;bottom:calc(96px + env(safe-area-inset-bottom));transform:translate(-50%,16px);background:#fff;color:#000;font-size:14px;font-weight:600;padding:10px 18px;border-radius:99px;opacity:0;transition:.25s;pointer-events:none;white-space:nowrap}
#toast.show{opacity:1;transform:translate(-50%,0)}
</style></head><body>
<header><div><div class="eyebrow">Display remote</div><h1>Carol</h1></div>
<div class="pill"><span class="dot" id="dot"></span><span id="conn">Connecting</span></div></header>
<main>

<section class="tab show" id="t-display">
 <div class="card"><div class="label">Now showing</div>
  <div class="now"><button class="round" onclick="hit('/prev')">&#9664;</button>
   <div class="t"><div class="big" id="now">&mdash;</div><div class="sub" id="nowsub">&nbsp;</div></div>
   <button class="round" onclick="hit('/next')">&#9654;</button></div></div>
 <div class="card"><div class="label">Playlist</div>
  <div class="hint">Turn screens on, set the order and how long each one stays up.</div>
  <div id="pl"></div>
  <div class="row"><span class="name">Auto-play</span><label class="sw"><input id="auto" type="checkbox"><i></i></label></div>
  <button class="primary full" style="margin-top:10px" onclick="savePL()">Save playlist</button></div>
</section>

<section class="tab" id="t-card">
 <div class="card"><div class="label">Hotbar</div>
  <div class="hint">Tap a slot to edit it.</div>
  <div class="hotbar" id="hotbar"></div>
  <div class="field"><input type="text" id="dtitle" maxlength="16" placeholder="App name"></div>
  <div class="field"><textarea id="ddesc" maxlength="220" placeholder="How I use it"></textarea></div>
  <label class="file">Choose icon image<input id="icon" type="file" accept="image/*" onchange="setIcon()"></label>
  <div class="two"><button class="primary" onclick="saveDesc()">Save</button><button onclick="hit('/slot?n='+cur)">Preview</button></div>
  <button class="ghost danger full" style="margin-top:6px" onclick="clearSlot()">Remove icon</button></div>
 <div class="card"><div class="label">Photos</div>
  <div class="thumbs" id="thumbs"></div>
  <label class="file">Add photos<input id="pics" type="file" accept="image/*" multiple onchange="upload()"></label>
  <div class="two"><button onclick="hit('/photos')">Show card</button><button class="danger" onclick="clearPhotos()">Delete all</button></div>
  <div class="hint" style="margin:12px 0 0;text-align:center" id="pinfo"></div></div>
</section>

<section class="tab" id="t-anim">
 <div class="card"><div class="label">Username animations</div>
  <div class="anim">
   <button onclick="hit('/anim?a=0')">Rainbow<small>Colour cycle</small></button>
   <button onclick="hit('/anim?a=1')">Wave<small>Letters ripple</small></button>
   <button onclick="hit('/anim?a=2')">Bounce<small>Wall to wall</small></button>
   <button onclick="hit('/anim?a=3')">Typewriter<small>Typed out</small></button>
   <button onclick="hit('/anim?a=4')">Marquee<small>Scrolls across</small></button>
  </div></div>
</section>

<section class="tab" id="t-msg">
 <div class="card"><div class="label">Message</div>
  <div class="hint">Stays on the display until someone taps it.</div>
  <div class="field"><textarea id="msg" maxlength="200" placeholder="Type a message"></textarea></div>
  <button class="primary full" onclick="sendMsg()">Show on display</button></div>
</section>
</main>

<nav>
 <button class="on" data-t="display" onclick="tab(this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/></svg>Display</button>
 <button data-t="card" onclick="tab(this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="9" cy="10" r="2"/><path d="M14 9h4M14 13h4M7 16h10"/></svg>Card</button>
 <button data-t="anim" onclick="tab(this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3l2.2 5.8L20 11l-5.8 2.2L12 19l-2.2-5.8L4 11l5.8-2.2z"/></svg>Animate</button>
 <button data-t="msg" onclick="tab(this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12a8 8 0 0 1-11.6 7.1L4 20l1-4.6A8 8 0 1 1 21 12z"/></svg>Message</button>
</nav>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
let cur=1,tt;
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(tt);tt=setTimeout(()=>e.classList.remove('show'),1700)}
function online(ok){$('dot').className='dot'+(ok?' on':'');$('conn').textContent=ok?'Connected':'Offline'}
function hit(u){return fetch(u).then(r=>{online(true);setTimeout(state,300);return r}).catch(()=>{online(false);toast('Display not reachable')})}
function tab(b){document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x===b));
  document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('show',x.id==='t-'+b.dataset.t));window.scrollTo(0,0)}
function state(){fetch('/state').then(r=>r.text()).then(t=>{online(true);const p=t.split('|');
  $('now').textContent=p[0];$('nowsub').textContent=p[1]||' '}).catch(()=>online(false))}
// Resizes an image on the phone before sending (fill = crop to fill, otherwise fit inside)
function shrink(file,W,H,fill){return new Promise((res,rej)=>{const img=new Image();
  img.onload=()=>{const c=document.createElement('canvas');c.width=W;c.height=H;
    const x=c.getContext('2d');x.fillStyle='#2d2d2d';x.fillRect(0,0,W,H);
    const sc=fill?Math.max(W/img.width,H/img.height):Math.min(W/img.width,H/img.height);
    const w=img.width*sc,h=img.height*sc;x.drawImage(img,(W-w)/2,(H-h)/2,w,h);
    c.toBlob(res,'image/jpeg',0.9);URL.revokeObjectURL(img.src)};
  img.onerror=rej;img.src=URL.createObjectURL(file)})}
// ---- Hotbar ----
function drawHotbar(){const hb=$('hotbar');hb.innerHTML='';
  for(let n=1;n<=5;n++){const b=document.createElement('button');b.className='slot'+(n===cur?' sel':'');b.onclick=()=>pick(n);
    const img=new Image();img.onerror=()=>{img.remove();const s=document.createElement('span');s.textContent=n;b.appendChild(s)};
    img.src='/icon?n='+n+'&v='+Date.now();b.appendChild(img);hb.appendChild(b)}}
function pick(n){cur=n;drawHotbar();loadDesc()}
function loadDesc(){fetch('/getdesc?n='+cur).then(r=>r.text()).then(t=>{const i=t.indexOf('\n');
  $('dtitle').value=i<0?t:t.slice(0,i);$('ddesc').value=i<0?'':t.slice(i+1)}).catch(()=>{})}
function saveDesc(){hit('/setdesc?n='+cur+'&t='+encodeURIComponent($('dtitle').value)+'&d='+encodeURIComponent($('ddesc').value)).then(()=>toast('Slot '+cur+' saved'))}
async function setIcon(){const f=$('icon').files[0];if(!f)return;toast('Uploading icon…');
  const b=await shrink(f,26,26,false);const fd=new FormData();fd.append('f',b,'i.jpg');
  await fetch('/upload?slot='+cur,{method:'POST',body:fd});$('icon').value='';drawHotbar();toast('Icon updated')}
function clearSlot(){hit('/clearslot?n='+cur).then(()=>{drawHotbar();toast('Icon removed')})}
// ---- Photos ----
function photos(){fetch('/info').then(r=>r.json()).then(j=>{let h='';
  for(let i=0;i<j.count;i++)h+='<img src="/photo?i='+i+'&v='+Date.now()+'">';
  $('thumbs').innerHTML=h||'<div class="empty">No photos yet</div>';
  $('pinfo').textContent=j.count+' photo'+(j.count==1?'':'s')+' · '+j.freeKB+' KB free'}).catch(()=>{})}
async function upload(){const files=[...$('pics').files];if(!files.length)return;
  for(let i=0;i<files.length;i++){toast('Uploading '+(i+1)+' of '+files.length+'…');
    const b=await shrink(files[i],196,111,true);const fd=new FormData();fd.append('f',b,'p.jpg');
    await fetch('/upload',{method:'POST',body:fd})}
  $('pics').value='';photos();hit('/photos');toast('Photos added')}
function clearPhotos(){if(confirm('Delete all photos on the display?'))hit('/clearphotos').then(()=>{photos();toast('Photos deleted')})}
// ---- Playlist ----
const NAMES={5:'Player card',6:'Rainbow',7:'Wave',8:'Bounce',9:'Typewriter',10:'Marquee'};const IDS=[5,6,7,8,9,10];
let rows=[];
function loadPL(){fetch('/getplaylist').then(r=>r.text()).then(t=>{
  const p=t.split('|');$('auto').checked=p[0]==='1';
  const used=(p[1]||'').split(',').filter(x=>x).map(x=>{const q=x.split(':');return{id:+q[0],on:true,sec:+q[1]||10}}).filter(r=>IDS.includes(r.id));
  const rest=IDS.filter(i=>!used.some(u=>u.id===i)).map(i=>({id:i,on:false,sec:10}));
  rows=used.concat(rest);drawPL()}).catch(()=>{})}
function drawPL(){$('pl').innerHTML=rows.map((r,k)=>'<div class="row'+(r.on?'':' off')+'">'+
  '<div class="mv"><button onclick="mv('+k+',-1)">&#9650;</button><button onclick="mv('+k+',1)">&#9660;</button></div>'+
  '<span class="name">'+NAMES[r.id]+'</span>'+
  '<div class="step"><button onclick="sec('+k+',-5)">&minus;</button><span>'+r.sec+'s</span><button onclick="sec('+k+',5)">+</button></div>'+
  '<label class="sw"><input type="checkbox" '+(r.on?'checked':'')+' onchange="rows['+k+'].on=this.checked;drawPL()"><i></i></label></div>').join('')}
function sec(k,d){rows[k].sec=Math.min(600,Math.max(5,rows[k].sec+d));drawPL()}
function mv(k,d){const j=k+d;if(j<0||j>=rows.length)return;const t=rows[k];rows[k]=rows[j];rows[j]=t;drawPL()}
function savePL(){const l=rows.filter(r=>r.on).map(r=>r.id+':'+r.sec).join(',');
  if(!l){toast('Turn on at least one screen');return}
  hit('/setplaylist?auto='+($('auto').checked?1:0)+'&list='+l).then(()=>toast('Playlist saved'))}
// ---- Message ----
function sendMsg(){const t=$('msg').value.trim();if(t)hit('/msg?t='+encodeURIComponent(t)).then(()=>toast('Sent to display'))}
drawHotbar();loadDesc();photos();loadPL();state();setInterval(state,4000);
</script></body></html>
)rawliteral";

// ===================== DRAWING HELPERS ===================
void drawHeader(const char* title, uint16_t color) {
  tft.fillScreen(BG);
  tft.fillRect(0, 0, W, 36, color);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(title, 12, 19, 4);
}

void drawDots() {
  int spacing = 14;
  int startX = W / 2 - (NUM_SCREENS - 1) * spacing / 2;
  for (int i = 0; i < NUM_SCREENS; i++) {
    if (i == currentScreen) tft.fillCircle(startX + i * spacing, 230, 4, ACCENT);
    else                    tft.drawCircle(startX + i * spacing, 230, 3, MUTED);
  }
}

// Word-wraps text inside a box. Returns the y position after the last line.
int drawWrapped(const String& text, int x, int y, int maxW, int font, int lineH, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, BG);
  String line = "", word = "";
  for (int i = 0; i <= (int)text.length(); i++) {
    char c = (i < (int)text.length()) ? text[i] : ' ';
    if (c == ' ' || c == '\n') {
      String test = line.length() ? line + " " + word : word;
      if (tft.textWidth(test, font) > maxW && line.length()) {
        tft.drawString(line, x, y, font);
        y += lineH;
        line = word;
      } else {
        line = test;
      }
      word = "";
      if (c == '\n') { tft.drawString(line, x, y, font); y += lineH; line = ""; }
    } else {
      word += c;
    }
  }
  if (line.length()) { tft.drawString(line, x, y, font); y += lineH; }
  return y;
}

// ===================== SCREENS ===========================
void drawHome() {
  tft.fillScreen(BG);
  tft.fillCircle(160, 62, 30, ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, ACCENT);
  tft.drawString(String(MY_NAME).substring(0, 1), 160, 64, 4);

  tft.setTextColor(TEXT, BG);
  tft.drawString(MY_NAME, 160, 118, 4);
  tft.setTextColor(ACCENT2, BG);
  tft.drawString(MY_TITLE, 160, 148, 2);
  tft.drawFastHLine(110, 166, 100, MUTED);

  tft.setTextColor(MUTED, BG);
  tft.drawString("Tap sides or use your phone", 160, 186, 2);
  tft.drawString(ipText, 160, 204, 2);
  drawDots();
}

void drawAbout() {
  drawHeader("About Me", ACCENT);
  drawWrapped(ABOUT_TEXT, 14, 52, W - 28, 2, 20, TEXT);
  drawDots();
}

void drawProjects() {
  drawHeader("Projects", ACCENT2);
  int y = 46;
  for (int i = 0; i < NUM_PROJECTS && i < 3; i++) {
    tft.fillRoundRect(8, y, W - 16, 54, 8, CARD);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(ACCENT2, CARD);
    tft.drawString(projects[i].title, 18, y + 6, 2);
    tft.setTextColor(TEXT, CARD);
    tft.drawString(projects[i].desc, 18, y + 28, 1);
    y += 60;
  }
  drawDots();
}

void drawSkills() {
  drawHeader("Skills", TFT_DARKGREEN);
  int y = 50;
  for (int i = 0; i < NUM_SKILLS && i < 6; i++) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString(skills[i].name, 12, y + 7, 2);
    int barX = 130, barW = 170;
    tft.fillRoundRect(barX, y, barW, 14, 7, CARD);
    tft.fillRoundRect(barX, y, barW * skills[i].percent / 100, 14, 7, TFT_GREEN);
    y += 32;
  }
  drawDots();
}

void drawContact() {
  drawHeader("Contact", TFT_ORANGE);
  tft.setTextDatum(TL_DATUM);
  const char* labels[] = { "Email", "GitHub", "LinkedIn" };
  const char* values[] = { CONTACT_EMAIL, CONTACT_GITHUB, CONTACT_LINKEDIN };
  int y = 52;
  for (int i = 0; i < 3; i++) {
    tft.setTextColor(MUTED, BG);
    tft.drawString(labels[i], 14, y, 2);
    tft.setTextColor(TEXT, BG);
    tft.drawString(values[i], 14, y + 18, 2);
    y += 52;
  }
  drawDots();
}

void drawMessage() {
  tft.fillScreen(BG);
  tft.fillRoundRect(10, 10, W - 20, H - 20, 14, CARD);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ACCENT2, CARD);
  tft.drawString("New message", W / 2, 22, 2);
  // Draw the message on the card background
  uint16_t oldBG = BG; BG = CARD;
  drawWrapped(customMessage, 24, 52, W - 48, 4, 28, TEXT);
  BG = oldBG;
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(MUTED, CARD);
  tft.drawString("tap to go back", W / 2, H - 18, 1);
}

// ===================== USERNAME ANIMATIONS ===============
// 0 = Rainbow, 1 = Wave, 2 = Bounce, 3 = Typewriter
#define NUM_ANIMS 5
#define MARQUEE_SIZE  3   // how big the scrolling name is (1-4)
#define MARQUEE_SPEED 4   // pixels moved per frame (higher = faster)
int animMode = 0;
unsigned long lastFrame = 0;
unsigned long animT = 0;      // frame counter
int nameSize = 2;             // text scale for the username

TFT_eSprite waveSpr   = TFT_eSprite(&tft);  // off-screen buffers = no flicker
TFT_eSprite bounceSpr = TFT_eSprite(&tft);
TFT_eSprite marqSpr   = TFT_eSprite(&tft);
TFT_eSprite nameSpr   = TFT_eSprite(&tft);  // the pulsing "Carol" on the player card

// Marquee state
int mqX, mqW, mqH;

// Bounce state
int bx, by, bvx = 2, bvy = 2, bw, bh, bHue = 0;
// Typewriter state
int twCount = 0, twHue = 0, twX0, twY;
unsigned long twLast = 0;

// Turns a hue (0-359) into a bright rainbow color
uint16_t hue565(int hue) {
  hue = ((hue % 360) + 360) % 360;
  int region = hue / 60;
  int rem = (hue % 60) * 255 / 60;
  uint8_t r, g, b;
  switch (region) {
    case 0:  r = 255;       g = rem;       b = 0;         break;
    case 1:  r = 255 - rem; g = 255;       b = 0;         break;
    case 2:  r = 0;         g = 255;       b = rem;       break;
    case 3:  r = 0;         g = 255 - rem; b = 255;       break;
    case 4:  r = rem;       g = 0;         b = 255;       break;
    default: r = 255;       g = 0;         b = 255 - rem; break;
  }
  return tft.color565(r, g, b);
}

// Width of the username in the current text size
int nameWidth() { return tft.textWidth(USERNAME, 4); }

void stopAnim() {
  if (waveSpr.created())   waveSpr.deleteSprite();
  if (bounceSpr.created()) bounceSpr.deleteSprite();
  if (marqSpr.created())   marqSpr.deleteSprite();
  if (nameSpr.created())   nameSpr.deleteSprite();
  tft.setTextSize(1);
  tft.setSwapBytes(false);
}

void startAnim() {
  stopAnim();
  tft.fillScreen(BG);
  animT = 0;

  // Use big text if the name fits, otherwise normal size
  tft.setTextSize(2);
  nameSize = (nameWidth() <= W - 20) ? 2 : 1;
  tft.setTextSize(nameSize);

  if (animMode == 1) {                       // Wave
    waveSpr.createSprite(W, 100);
    waveSpr.setTextSize(nameSize);
  } else if (animMode == 2) {                // Bounce
    bw = nameWidth() + 8;
    bh = tft.fontHeight(4) + 8;
    bounceSpr.createSprite(bw, bh);
    bounceSpr.setTextSize(nameSize);
    bx = (W - bw) / 2;  by = (H - bh) / 2;
    bHue = random(360);
  } else if (animMode == 3) {                // Typewriter
    twCount = 0;
    twHue = random(360);
    twX0 = (W - nameWidth()) / 2;
    twY = H / 2 - tft.fontHeight(4) / 2;
    twLast = millis();
  } else if (animMode == 4) {                // Marquee
    tft.setTextSize(MARQUEE_SIZE);
    mqW = nameWidth();
    mqH = tft.fontHeight(4);
    marqSpr.createSprite(W, mqH);
    marqSpr.setTextSize(MARQUEE_SIZE);
    mqX = W;                                 // start just off the right edge
  }
}

// ➡️ Big name slides off the left edge and comes back in from the right
void animMarquee() {
  marqSpr.fillSprite(BG);
  marqSpr.setTextDatum(TL_DATUM);
  marqSpr.setTextColor(hue565(animT * 2), BG);
  marqSpr.drawString(USERNAME, mqX, 0, 4);
  marqSpr.pushSprite(0, (H - mqH) / 2);

  mqX -= MARQUEE_SPEED;
  if (mqX < -mqW) mqX = W;                   // fully gone? wrap to the right side
}

// 🌈 Each letter cycles through the rainbow, offset from its neighbour
void animRainbow() {
  int x = (W - nameWidth()) / 2;
  int y = H / 2 - tft.fontHeight(4) / 2;
  tft.setTextDatum(TL_DATUM);
  for (int i = 0; USERNAME[i]; i++) {
    String c = String(USERNAME[i]);
    tft.setTextColor(hue565(animT * 4 + i * 30), BG);
    tft.drawString(c, x, y, 4);
    x += tft.textWidth(c, 4);
  }
}

// 🌊 Letters bob up and down in a wave, with rainbow colors
void animWave() {
  waveSpr.fillSprite(BG);
  waveSpr.setTextDatum(TL_DATUM);
  int charH = waveSpr.fontHeight(4);
  int x = (W - nameWidth()) / 2;
  for (int i = 0; USERNAME[i]; i++) {
    String c = String(USERNAME[i]);
    int offset = (int)(sin(animT * 0.15 + i * 0.6) * 18);
    waveSpr.setTextColor(hue565(i * 35 + animT * 3), BG);
    waveSpr.drawString(c, x, 50 - charH / 2 + offset, 4);
    x += waveSpr.textWidth(c, 4);
  }
  waveSpr.pushSprite(0, (H - 100) / 2);
}

// 🏓 Name bounces around the screen and changes color on every wall hit
void animBounce() {
  bx += bvx;  by += bvy;
  bool hit = false;
  if (bx <= 0 || bx + bw >= W) { bvx = -bvx; bx = constrain(bx, 0, W - bw); hit = true; }
  if (by <= 0 || by + bh >= H) { bvy = -bvy; by = constrain(by, 0, H - bh); hit = true; }
  if (hit) bHue += 60 + random(80);

  bounceSpr.fillSprite(BG);
  bounceSpr.setTextDatum(MC_DATUM);
  bounceSpr.setTextColor(hue565(bHue), BG);
  bounceSpr.drawString(USERNAME, bw / 2, bh / 2, 4);
  bounceSpr.pushSprite(bx, by);
}

// ⌨️ Types the name letter by letter with a blinking cursor, then repeats in new colors
void animTypewriter() {
  int len = strlen(USERNAME);
  int fh = tft.fontHeight(4);
  int cursorW = 4 * nameSize;
  tft.setTextDatum(TL_DATUM);
  int cx = twX0 + tft.textWidth(String(USERNAME).substring(0, twCount), 4);

  if (twCount < len && millis() - twLast > 140) {
    tft.fillRect(cx + 2, twY, cursorW, fh, BG);             // erase cursor
    tft.setTextColor(hue565(twHue + twCount * 20), BG);
    tft.drawString(String(USERNAME[twCount]), cx, twY, 4);
    twCount++;
    twLast = millis();
    cx = twX0 + tft.textWidth(String(USERNAME).substring(0, twCount), 4);
  } else if (twCount >= len && millis() - twLast > 1800) {
    tft.fillRect(0, twY - 4, W, fh + 8, BG);                // clear and start over
    twCount = 0;
    twHue += 70;
    twLast = millis();
    return;
  }

  bool cursorOn = (millis() / 400) % 2;
  tft.fillRect(cx + 2, twY, cursorW, fh, cursorOn ? TEXT : BG);
}

void animStep() {
  switch (animMode) {
    case 0: animRainbow();    break;
    case 1: animWave();       break;
    case 2: animBounce();     break;
    case 3: animTypewriter(); break;
    case 4: animMarquee();    break;
  }
  animT++;
}

// ===================== PLAYER CARD (photo GIF + HUD) =====
// Layout (320 x 240), based on Carol's Figma mockup
#define PH_X 62        // photo box (matches the white box in the Figma frame)
#define PH_Y 45
#define PH_W 196
#define PH_H 111
#define HEART_X 24     // hearts row
#define HEART_Y 168
#define XP_X 24        // XP bar
#define XP_Y 186
#define XP_W 272
#define XP_H 6
#define SLOT 30        // hotbar slot size
#define SLOTS 5
#define HOTBAR_Y 197

std::vector<String> photos;
int photoIdx = 0;
unsigned long lastPhoto = 0;

// XP state
bool  xpReady = false;
float xpTarget = 0, xpShown = 0;
int   xpLevel = -1;
unsigned long lastXPCheck = 0, lastCardFrame = 0, heartWaveStart = 0;

void loadPhotoList() {
  photos.clear();
  File dir = LittleFS.open("/p");
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    String n = f.name();
    if (n.startsWith("/")) n = n.substring(n.lastIndexOf('/') + 1);
    photos.push_back("/p/" + n);
    f = dir.openNextFile();
  }
  std::sort(photos.begin(), photos.end());
}

String nextPhotoName() {
  int maxNum = 0;
  for (auto& p : photos) maxNum = max(maxNum, (int)p.substring(3).toInt());
  char buf[16];
  snprintf(buf, sizeof(buf), "/p/%04d.jpg", maxNum + 1);
  return String(buf);
}

// The JPG decoder hands us small blocks; the viewport clips them to the target box
bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  if (y >= H) return false;
  tft.pushImage(x, y, w, h, bmp);
  return true;
}

// Draws a JPG file from flash into a box (anything outside the box is cut off)
bool drawJpgFile(const String& path, int x, int y, int w, int h) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t n = f.size();
  uint8_t* buf = (uint8_t*)malloc(n);
  if (!buf) { f.close(); return false; }
  f.read(buf, n);
  f.close();
  tft.setViewport(x, y, w, h, false);
  TJpgDec.drawJpg(x, y, buf, n);
  tft.resetViewport();
  free(buf);
  return true;
}

// ---- Name: blocky white letters with a gray shadow that gently glow ----
#define NAME_GLOW_MS 3000     // time for one glow in-and-out (ms, higher = slower)
#define NAME_BOX_W   130      // area kept clear for the name (stars stay out of it)
#define NAME_BOX_H   40
#define NAME_BOX_Y   4
int nameX = 0, nameY = 0;

// Draws the name and its shadow straight onto the screen (once, when the card opens)
void drawCardName() {
  tft.setTextSize(NAME_SCALE);
  tft.setTextDatum(TL_DATUM);
  nameX = (W - tft.textWidth(TAG_TEXT, 1)) / 2;
  nameY = NAME_BOX_Y + (NAME_BOX_H - 8 * NAME_SCALE) / 2;
  tft.setTextColor(tft.color565(63, 63, 63));               // shadow
  tft.drawString(TAG_TEXT, nameX + NAME_SCALE, nameY + NAME_SCALE, 1);
  tft.setTextColor(TFT_WHITE);                               // letters
  tft.drawString(TAG_TEXT, nameX, nameY, 1);
  tft.setTextSize(1);
}

// Re-colours just the letters so the name slowly glows brighter and softer (no extra memory)
void glowCardName() {
  float phase = (millis() % NAME_GLOW_MS) / (float)NAME_GLOW_MS;
  uint8_t v = 170 + (uint8_t)(85 * (0.5f + 0.5f * sin(phase * 2 * PI)));
  tft.setTextSize(NAME_SCALE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(tft.color565(v, v, v));
  tft.drawString(TAG_TEXT, nameX, nameY, 1);
  tft.setTextSize(1);
}

// ---- Hearts (original pixel art, 7x6 grid drawn at 2x) ----
const char* HEART_PX[6] = { ".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..." };

void drawHeart(int x, int y, bool full) {
  uint16_t top  = full ? tft.color565(235, 30, 30)  : tft.color565(75, 30, 30);
  uint16_t low  = full ? tft.color565(170, 10, 10)  : tft.color565(50, 20, 20);
  for (int r = 0; r < 6; r++)
    for (int c = 0; c < 7; c++)
      if (HEART_PX[r][c] == 'X')
        tft.fillRect(x + c * 2, y + r * 2, 2, 2, r >= 3 ? low : top);
  if (full) tft.fillRect(x + 2, y + 2, 2, 2, TFT_WHITE);   // little shine
}

// offsetsOn = true while the "wave" plays: each heart hops up 2px in turn
void drawHearts(bool wave) {
  tft.fillRect(HEART_X, HEART_Y - 3, HEARTS_TOTAL * HEART_GAP + 2, 16, TFT_BLACK);
  unsigned long t = millis() - heartWaveStart;
  for (int i = 0; i < HEARTS_TOTAL; i++) {
    int hop = 0;
    if (wave && t >= (unsigned long)i * 90 && t < (unsigned long)i * 90 + 150) hop = 2;
    drawHeart(HEART_X + i * HEART_GAP, HEART_Y - hop, i < HEARTS_FULL);
  }
}

// ---- XP bar + level number ----
void drawXPBar(float pct) {
  uint16_t empty = tft.color565(25, 40, 20);
  uint16_t green = tft.color565(110, 220, 40);
  uint16_t light = tft.color565(180, 255, 110);
  uint16_t notch = tft.color565(10, 20, 10);
  tft.drawRect(XP_X - 1, XP_Y - 1, XP_W + 2, XP_H + 2, tft.color565(45, 70, 35));
  tft.fillRect(XP_X, XP_Y, XP_W, XP_H, empty);
  int fw = (int)(XP_W * constrain(pct, 0.0f, 1.0f));
  if (fw > 0) {
    tft.fillRect(XP_X, XP_Y, fw, XP_H, green);
    tft.drawFastHLine(XP_X, XP_Y, fw, light);
  }
  for (int i = 1; i < 16; i++) tft.drawFastVLine(XP_X + i * XP_W / 16, XP_Y, XP_H, notch);
}

void drawLevel() {
  tft.fillRect(W / 2 - 20, XP_Y - 19, 40, 18, TFT_BLACK);
  if (xpLevel < 0) return;
  String s = String(xpLevel);
  tft.setTextSize(2);
  tft.setTextDatum(BC_DATUM);
  int cx = W / 2, cy = XP_Y - 2;
  tft.setTextColor(TFT_BLACK);                     // dark outline
  tft.drawString(s, cx - 1, cy, 1); tft.drawString(s, cx + 1, cy, 1);
  tft.drawString(s, cx, cy - 1, 1); tft.drawString(s, cx, cy + 1, 1);
  tft.setTextColor(tft.color565(128, 255, 32));    // green number
  tft.drawString(s, cx, cy, 1);
  tft.setTextSize(1);
}

// Works out level (age) and how far you are to the next birthday, from internet time
bool computeXP() {
  struct tm now;
  if (!getLocalTime(&now, 10)) return false;
  int y = now.tm_year + 1900;
  bool passed = (now.tm_mon + 1 > BIRTH_MONTH) ||
                (now.tm_mon + 1 == BIRTH_MONTH && now.tm_mday >= BIRTH_DAY);
  int lastY = passed ? y : y - 1;
  struct tm a = {0};
  a.tm_year = lastY - 1900; a.tm_mon = BIRTH_MONTH - 1; a.tm_mday = BIRTH_DAY; a.tm_isdst = -1;
  struct tm b = a; b.tm_year += 1;
  time_t ta = mktime(&a), tb = mktime(&b), tn = mktime(&now);
  xpTarget = (float)(tn - ta) / (float)(tb - ta);
  xpLevel = lastY - BIRTH_YEAR;
  return true;
}

// ---- Hotbar: 5 slots, icons uploaded from the phone ----
void drawHotbar() {
  int x0 = (W - SLOTS * SLOT) / 2;
  for (int i = 0; i < SLOTS; i++) {
    int x = x0 + i * SLOT;
    tft.fillRect(x, HOTBAR_Y, SLOT, SLOT, tft.color565(115, 115, 115));       // frame
    tft.fillRect(x + 2, HOTBAR_Y + 2, SLOT - 4, SLOT - 4, tft.color565(45, 45, 45));
    drawJpgFile("/s/" + String(i + 1) + ".jpg", x + 2, HOTBAR_Y + 2, SLOT - 4, SLOT - 4);
  }
}

void drawPhotoPlaceholder() {
  tft.fillRect(PH_X, PH_Y, PH_W, PH_H, tft.color565(234, 221, 192));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(120, 100, 80));
  tft.drawString("Upload photos", PH_X + PH_W / 2, PH_Y + PH_H / 2 - 8, 2);
  tft.drawString("from your phone", PH_X + PH_W / 2, PH_Y + PH_H / 2 + 10, 2);
}

// ---- Battery indicator (top-right corner) ----
#define BAT_PIN     34      // Freenove: battery voltage on GPIO34, halved by a divider
#define BAT_X       290
#define BAT_Y       6
#define BAT_EMPTY_V 3.3f    // volts = empty
#define BAT_FULL_V  4.2f    // volts = full
float batVolts = 0;
int batShown = -2;
unsigned long lastBat = 0;

float readBattery() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(BAT_PIN);   // average out the noise
  return (mv / 16.0f) * 2.0f / 1000.0f;
}

// Pixel battery with 4 bars: green = good, yellow = half, red = low
void drawBattery(bool force) {
  batVolts = readBattery();
  int level;                                 // -1 = no battery plugged in, else 0-4 bars
  if (batVolts < 2.5f) level = -1;
  else level = constrain((int)((batVolts - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V) * 4 + 0.5f), 0, 4);
  if (!force && level == batShown) return;  // only redraw when it changes
  batShown = level;

  uint16_t frame = level == 0 ? tft.color565(235, 30, 30) : tft.color565(200, 200, 200);
  tft.fillRect(BAT_X - 1, BAT_Y - 1, 28, 14, TFT_BLACK);
  tft.drawRect(BAT_X, BAT_Y, 22, 10, frame);          // body
  tft.fillRect(BAT_X + 22, BAT_Y + 3, 2, 4, frame);   // tip
  if (level < 0) {                                    // no battery: grey dash
    tft.drawFastHLine(BAT_X + 7, BAT_Y + 5, 8, tft.color565(120, 120, 120));
    return;
  }
  uint16_t col = level >= 3 ? tft.color565(110, 220, 40)
               : level == 2 ? tft.color565(255, 200, 40)
                            : tft.color565(235, 30, 30);
  for (int i = 0; i < level; i++) tft.fillRect(BAT_X + 2 + i * 5, BAT_Y + 2, 4, 6, col);
}

// ---- Twinkling stars behind the card ----
#define NUM_STARS 45
struct Star { int16_t x, y; uint8_t size; uint16_t speed, phase; uint8_t tint; };
Star stars[NUM_STARS];
bool starsMade = false;

// Stars stay out of these areas so they never sit on top of the name, photo, HUD or battery
bool inKeepOut(int x, int y, int sz) {
  struct R { int x0, y0, x1, y1; };
  const R zones[] = {
    { (W - NAME_BOX_W) / 2 - 2, 0, (W + NAME_BOX_W) / 2 + 2, NAME_BOX_Y + NAME_BOX_H + 2 },
    { PH_X - 3, PH_Y - 3, PH_X + PH_W + 3, PH_Y + PH_H + 3 },
    { HEART_X - 2, HEART_Y - 5, HEART_X + HEARTS_TOTAL * HEART_GAP + 4, HEART_Y + 15 },
    { W / 2 - 22, XP_Y - 21, W / 2 + 22, XP_Y },
    { XP_X - 3, XP_Y - 3, XP_X + XP_W + 3, XP_Y + XP_H + 3 },
    { (W - SLOTS * SLOT) / 2 - 3, HOTBAR_Y - 3, (W + SLOTS * SLOT) / 2 + 3, HOTBAR_Y + SLOT + 3 },
    { BAT_X - 3, BAT_Y - 3, W, BAT_Y + 14 },
  };
  for (auto& z : zones)
    if (x + sz > z.x0 && x < z.x1 && y + sz > z.y0 && y < z.y1) return true;
  return false;
}

void makeStars() {
  for (int i = 0; i < NUM_STARS; i++) {
    Star& s = stars[i];
    do {
      s.size = random(10) < 2 ? 2 : 1;      // a few bigger stars
      s.x = random(W - 2);
      s.y = random(H - 2);
    } while (inKeepOut(s.x, s.y, s.size));
    s.speed = 1500 + random(3500);         // how fast it twinkles (ms)
    s.phase = random(s.speed);
    s.tint  = random(4);                   // white, icy blue, or warm
  }
  starsMade = true;
}

void drawStars() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_STARS; i++) {
    Star& s = stars[i];
    float t = ((now + s.phase) % s.speed) / (float)s.speed;
    float b = 0.15f + 0.85f * (0.5f + 0.5f * sin(t * 2 * PI));   // brightness 15%-100%
    uint8_t r = 255, g = 255, bl = 255;
    if (s.tint == 1)      { r = 170; g = 190; bl = 255; }
    else if (s.tint == 2) { r = 255; g = 210; bl = 160; }
    tft.fillRect(s.x, s.y, s.size, s.size, tft.color565(r * b, g * b, bl * b));
  }
}

void startPhotos() {
  stopAnim();
  loadPhotoList();
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  if (!starsMade) makeStars();

  drawCardName();
  tft.drawRect(PH_X - 1, PH_Y - 1, PH_W + 2, PH_H + 2, tft.color565(90, 90, 90));
  if (photos.empty()) drawPhotoPlaceholder();
  heartWaveStart = millis();
  drawHearts(false);
  xpShown = 0;                       // bar fills up every time the card opens
  xpReady = computeXP();
  drawXPBar(0);
  drawLevel();
  drawHotbar();
  drawBattery(true);
  lastBat = millis();

  photoIdx = 0;
  lastPhoto = 0;                     // show the first photo right away
}

void photoStep() {
  // Next photo frame
  if (!photos.empty() && millis() - lastPhoto >= FRAME_MS) {
    lastPhoto = millis();
    drawJpgFile(photos[photoIdx], PH_X, PH_Y, PH_W, PH_H);
    photoIdx = (photoIdx + 1) % photos.size();
  }

  if (millis() - lastCardFrame < 30) return;
  lastCardFrame = millis();

  // "Carol" gently glows
  glowCardName();

  // Stars twinkle in the background
  drawStars();

  // Check the battery every 5 seconds
  if (millis() - lastBat > 5000) { lastBat = millis(); drawBattery(false); }

  // Keep trying to get the time until it works, then re-check once a minute
  if (millis() - lastXPCheck > (xpReady ? 60000UL : 2000UL)) {
    lastXPCheck = millis();
    int oldLevel = xpLevel;
    if (computeXP()) {
      if (!xpReady || oldLevel != xpLevel) { xpReady = true; drawLevel(); }
    }
  }

  // XP bar fills up smoothly to the real value
  if (xpReady && xpShown < xpTarget - 0.002f) {
    xpShown += max(0.004f, (xpTarget - xpShown) * 0.08f);
    if (xpShown > xpTarget) xpShown = xpTarget;
    drawXPBar(xpShown);
  }

  // Hearts do a little hop wave every 5 seconds
  unsigned long t = millis() - heartWaveStart;
  if (t > 5000) { heartWaveStart = millis(); t = 0; }
  if (t < (unsigned long)HEARTS_TOTAL * 90 + 200) drawHearts(true);
}

// ===================== HOTBAR SLOT INFO (Minecraft tooltip style) =====
int slotIdx = 1;

// Each slot's text is saved on the board as /s/N.txt  (line 1 = name, rest = description)
void readSlotDesc(int n, String& title, String& desc) {
  title = ""; desc = "";
  File f = LittleFS.open("/s/" + String(n) + ".txt", "r");
  if (!f) return;
  title = f.readStringUntil('\n');
  desc = f.readString();
  f.close();
}

void drawSlot() {
  String title, desc;
  readSlotDesc(slotIdx, title, desc);
  uint16_t tipBG = tft.color565(16, 0, 16);     // dark purple-black, like an item tooltip
  uint16_t aqua  = tft.color565(85, 255, 255);
  uint16_t gray  = tft.color565(170, 170, 170);

  tft.fillScreen(TFT_BLACK);
  int bx = 14, by = 14, bw = W - 28, bh = H - 28;
  tft.fillRect(bx, by, bw, bh, tipBG);
  tft.drawRect(bx + 2, by + 2, bw - 4, bh - 4, tft.color565(80, 0, 255));   // purple border
  tft.drawRect(bx + 3, by + 3, bw - 6, bh - 6, tft.color565(40, 0, 127));

  // The app's icon in its own slot
  int ix = bx + 14, iy = by + 14;
  tft.fillRect(ix, iy, SLOT, SLOT, tft.color565(115, 115, 115));
  tft.fillRect(ix + 2, iy + 2, SLOT - 4, SLOT - 4, tft.color565(45, 45, 45));
  tft.setSwapBytes(true);
  drawJpgFile("/s/" + String(slotIdx) + ".jpg", ix + 2, iy + 2, SLOT - 4, SLOT - 4);
  tft.setSwapBytes(false);

  // App name in pixel letters
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(aqua, tipBG);
  tft.drawString(title.length() ? title : "Slot " + String(slotIdx), ix + SLOT + 10, iy + 2, 1);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(85, 85, 255), tipBG);
  tft.drawString("Slot " + String(slotIdx), ix + SLOT + 10, iy + 22, 1);

  // Description
  uint16_t oldBG = BG; BG = tipBG;
  drawWrapped(desc.length() ? desc : String("No description yet. Add one from your phone."),
              bx + 14, iy + SLOT + 14, bw - 28, 2, 18, gray);
  BG = oldBG;

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(tft.color565(90, 90, 90), tipBG);
  tft.drawString("tap to go back", W / 2, by + bh - 8, 1);
}

void drawCurrent() {
  if (currentScreen == SCR_ANIM)  { startAnim();   return; }
  if (currentScreen == SCR_PHOTO) { startPhotos(); return; }
  stopAnim();
  switch (currentScreen) {
    case SCR_HOME:     drawHome();     break;
    case SCR_ABOUT:    drawAbout();    break;
    case SCR_PROJECTS: drawProjects(); break;
    case SCR_SKILLS:   drawSkills();   break;
    case SCR_CONTACT:  drawContact();  break;
    case SCR_MESSAGE:  drawMessage();  break;
    case SCR_SLOT:     drawSlot();     break;
  }
}

// ===================== PLAYLIST (which screens play, in what order, for how long) =====
// Item ids: 0 Home, 1 About, 2 Projects, 3 Skills, 4 Contact, 5 Player card,
//           6 Rainbow, 7 Wave, 8 Bounce, 9 Typewriter, 10 Marquee
// Set it all from the phone page; it's saved on the board in /playlist.txt
#define NUM_ITEMS 11
std::vector<int> plIds, plSecs;
int plPos = 0;
bool autoplay = false;
unsigned long plStart = 0;

void showItem(int id) {
  if (id < NUM_SCREENS)     currentScreen = id;
  else if (id == 5)         currentScreen = SCR_PHOTO;
  else { animMode = id - 6; currentScreen = SCR_ANIM; }
  needsRedraw = true;
}

// Reads a list like "5:10,10:6" (item id : seconds on screen)
void parsePlaylist(const String& list) {
  plIds.clear(); plSecs.clear();
  int start = 0;
  while (start < (int)list.length()) {
    int comma = list.indexOf(',', start);
    if (comma < 0) comma = list.length();
    String item = list.substring(start, comma);
    item.trim();
    int colon = item.indexOf(':');
    int id  = item.substring(0, colon < 0 ? item.length() : colon).toInt();
    int sec = colon < 0 ? 10 : item.substring(colon + 1).toInt();
    if (item.length() && id >= 0 && id < NUM_ITEMS) {
      plIds.push_back(id);
      plSecs.push_back(constrain(sec, 2, 600));
    }
    start = comma + 1;
  }
  plPos = 0;
}

String playlistString() {
  String s;
  for (size_t i = 0; i < plIds.size(); i++) {
    if (i) s += ",";
    s += String(plIds[i]) + ":" + String(plSecs[i]);
  }
  return s;
}

void loadPlaylist() {
  File f = LittleFS.open("/playlist.txt", "r");
  if (f) { autoplay = f.readStringUntil('\n').toInt() == 1; parsePlaylist(f.readString()); f.close(); }
  if (plIds.empty()) { autoplay = false; parsePlaylist("5:10,0:10,1:10,2:10,3:10,4:10"); }  // starts on the player card
}

void savePlaylist() {
  File f = LittleFS.open("/playlist.txt", "w");
  if (f) { f.print(String(autoplay ? 1 : 0) + "\n" + playlistString()); f.close(); }
}

// Jump straight to an item from the phone; if it's in the playlist, carry on from there
void jumpTo(int id) {
  for (size_t i = 0; i < plIds.size(); i++) if (plIds[i] == id) { plPos = i; break; }
  showItem(id);
  plStart = millis();
}

void nextScreen() {
  if (plIds.empty()) return;
  plPos = (plPos + 1) % plIds.size();
  showItem(plIds[plPos]);
  plStart = millis();
}
void prevScreen() {
  if (plIds.empty()) return;
  plPos = (plPos + plIds.size() - 1) % plIds.size();
  showItem(plIds[plPos]);
  plStart = millis();
}
// Back from a message to wherever the playlist was
void backToPlaylist() {
  if (plIds.empty()) { currentScreen = SCR_HOME; needsRedraw = true; }
  else showItem(plIds[plPos]);
  plStart = millis();
}

// ===================== WEB HANDLERS ======================
void setupServer() {
  server.on("/", []() { server.send(200, "text/html", PAGE); });
  server.on("/go", []() {
    int s = server.arg("s").toInt();
    if (s >= 0 && s < NUM_SCREENS) jumpTo(s);
    server.send(200, "text/plain", "ok");
  });
  server.on("/next", []() { nextScreen(); server.send(200, "text/plain", "ok"); });
  server.on("/prev", []() { prevScreen(); server.send(200, "text/plain", "ok"); });
  server.on("/anim", []() {
    int a = server.arg("a").toInt();
    if (a >= 0 && a < NUM_ANIMS) jumpTo(6 + a);
    server.send(200, "text/plain", "ok");
  });
  server.on("/photos", []() { jumpTo(5); server.send(200, "text/plain", "ok"); });
  // What's on the display right now (for the "Now showing" panel)
  server.on("/state", []() {
    String a, b;
    if (currentScreen == SCR_MESSAGE) { a = "Message"; b = "Tap the display to dismiss"; }
    else if (currentScreen == SCR_SLOT) {
      String ti, de;
      readSlotDesc(slotIdx, ti, de);
      a = ti.length() ? ti : "Slot " + String(slotIdx);
      b = "Hotbar app";
    }
    else if (currentScreen == SCR_PHOTO) { a = "Player card"; }
    else if (currentScreen == SCR_ANIM && animMode >= 0 && animMode < NUM_ANIMS) {
      const char* names[NUM_ANIMS] = { "Rainbow", "Wave", "Bounce", "Typewriter", "Marquee" };
      a = names[animMode];
      b = "Username animation";
    }
    else { a = "Classic screen"; }
    if (!b.length()) b = autoplay ? "Auto-play on" : "Manual";
    server.send(200, "text/plain", a + "|" + b);
  });
  // Lets the phone show the real hotbar icons
  server.on("/icon", []() {
    String p = "/s/" + String(server.arg("n").toInt()) + ".jpg";
    File f = LittleFS.open(p, "r");
    if (!f) { server.send(404, "text/plain", "none"); return; }
    server.streamFile(f, "image/jpeg");
    f.close();
  });
  // Lets the phone show photo thumbnails
  server.on("/photo", []() {
    loadPhotoList();
    int i = server.arg("i").toInt();
    if (i < 0 || i >= (int)photos.size()) { server.send(404, "text/plain", "none"); return; }
    File f = LittleFS.open(photos[i], "r");
    if (!f) { server.send(404, "text/plain", "none"); return; }
    server.streamFile(f, "image/jpeg");
    f.close();
  });
  server.on("/getplaylist", []() {
    server.send(200, "text/plain", String(autoplay ? 1 : 0) + "|" + playlistString());
  });
  server.on("/setplaylist", []() {
    parsePlaylist(server.arg("list"));
    if (plIds.empty()) parsePlaylist("0:10");
    autoplay = server.arg("auto") == "1";
    savePlaylist();
    showItem(plIds[0]);          // start the new playlist from the top
    plStart = millis();
    server.send(200, "text/plain", "ok");
  });
  server.on("/info", []() {
    loadPhotoList();
    long freeKB = (LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024;
    server.send(200, "application/json",
                "{\"count\":" + String(photos.size()) + ",\"freeKB\":" + String(freeKB) + "}");
  });
  server.on("/clearphotos", []() {
    loadPhotoList();
    for (auto& p : photos) LittleFS.remove(p);
    photos.clear();
    if (currentScreen == SCR_PHOTO) needsRedraw = true;
    server.send(200, "text/plain", "ok");
  });
  // Receives one photo at a time from the phone and saves it on the board
  static File upFile;
  server.on("/upload", HTTP_POST,
    []() { server.send(200, "text/plain", "ok"); },
    []() {
      HTTPUpload& up = server.upload();
      if (up.status == UPLOAD_FILE_START) {
        loadPhotoList();
        int slot = server.arg("slot").toInt();
        if (slot >= 1 && slot <= SLOTS)                              // hotbar icon
          upFile = LittleFS.open("/s/" + String(slot) + ".jpg", "w");
        else if (LittleFS.totalBytes() - LittleFS.usedBytes() > 60000) // photo (leave some room)
          upFile = LittleFS.open(nextPhotoName(), "w");
      } else if (up.status == UPLOAD_FILE_WRITE) {
        if (upFile) upFile.write(up.buf, up.currentSize);
      } else if (up.status == UPLOAD_FILE_END) {
        if (upFile) upFile.close();
        loadPhotoList();
        if (currentScreen == SCR_PHOTO && server.hasArg("slot")) drawHotbar();
      }
    });
  server.on("/clearslot", []() {
    int slot = server.arg("n").toInt();
    if (slot >= 1 && slot <= SLOTS) LittleFS.remove("/s/" + String(slot) + ".jpg");
    if (currentScreen == SCR_PHOTO) drawHotbar();
    server.send(200, "text/plain", "ok");
  });
  server.on("/slot", []() {
    int n = server.arg("n").toInt();
    if (n >= 1 && n <= SLOTS) { slotIdx = n; currentScreen = SCR_SLOT; needsRedraw = true; }
    server.send(200, "text/plain", "ok");
  });
  server.on("/getdesc", []() {
    String t, d;
    readSlotDesc(server.arg("n").toInt(), t, d);
    server.send(200, "text/plain", t + "\n" + d);
  });
  server.on("/setdesc", []() {
    int n = server.arg("n").toInt();
    if (n >= 1 && n <= SLOTS) {
      String t = server.arg("t"); t.replace("\n", " ");
      File f = LittleFS.open("/s/" + String(n) + ".txt", "w");
      if (f) { f.print(t.substring(0, 16) + "\n" + server.arg("d").substring(0, 220)); f.close(); }
      if (currentScreen == SCR_SLOT && slotIdx == n) needsRedraw = true;
    }
    server.send(200, "text/plain", "ok");
  });
  server.on("/msg", []() {
    customMessage = server.arg("t").substring(0, 200);
    currentScreen = SCR_MESSAGE;
    needsRedraw = true;
    server.send(200, "text/plain", "ok");
  });
  server.begin();
}

// ===================== SETUP + LOOP ======================
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);   // landscape, 320 x 240
  tft.invertDisplay(false);  // colour fix: if colours look inverted, change false to true
  touch.setCal(527, 3552, 683, 3464, 320, 240, 1);  // from Freenove's Touch Draw sketch

  // Photo storage on the board's flash + JPG decoder
  if (!LittleFS.begin(true)) Serial.println("LittleFS failed - check Partition Scheme");
  LittleFS.mkdir("/p");
  LittleFS.mkdir("/s");
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(jpgOutput);
  loadPlaylist();
  pinMode(BAT_PIN, INPUT);   // battery voltage

  BG      = tft.color565(17, 24, 39);
  CARD    = tft.color565(31, 41, 55);
  TEXT    = tft.color565(243, 244, 246);
  MUTED   = tft.color565(140, 150, 165);
  ACCENT  = tft.color565(99, 102, 241);
  ACCENT2 = tft.color565(236, 72, 153);

  // Connecting screen
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TEXT, BG);
  tft.drawString("Connecting to Wi-Fi...", W / 2, H / 2, 2);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    ipText = "http://" + WiFi.localIP().toString();
    MDNS.begin("portfolio");   // also reachable at http://portfolio.local on many devices
    configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");  // for the XP bar
    setupServer();
    Serial.println("Open on your phone: " + ipText);
  } else {
    ipText = "Wi-Fi not connected (touch only)";
    Serial.println("Wi-Fi failed - check name/password (2.4 GHz only)");
  }
  backToPlaylist();   // start on the first screen of the playlist
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) server.handleClient();

  // Touch: right half = next in playlist, left half = previous
  //        player card: tap a hotbar slot to open its description
  if (touch.Pressed()) {
    if (millis() - lastTouch > 400) {
      int x = touch.X();
      int y = touch.Y();
      int x0 = (W - SLOTS * SLOT) / 2;
      if (currentScreen == SCR_MESSAGE) backToPlaylist();
      else if (currentScreen == SCR_SLOT) { currentScreen = SCR_PHOTO; needsRedraw = true; plStart = millis(); }
      else if (currentScreen == SCR_PHOTO && y >= HOTBAR_Y - 6 && x >= x0 && x < x0 + SLOTS * SLOT) {
        slotIdx = (x - x0) / SLOT + 1;
        currentScreen = SCR_SLOT;
        needsRedraw = true;
      }
      else if (x > W / 2) nextScreen();
      else prevScreen();
    }
    lastTouch = millis();
  }

  // Auto-play: go to the next screen when this one's time is up
  // (pauses while a message or an app description is open)
  if (autoplay && plIds.size() > 1 && currentScreen != SCR_MESSAGE && currentScreen != SCR_SLOT &&
      millis() - plStart >= (unsigned long)plSecs[plPos] * 1000UL) nextScreen();

  if (needsRedraw) { needsRedraw = false; drawCurrent(); }

  // Run the animation at about 33 frames per second
  if (currentScreen == SCR_ANIM && millis() - lastFrame >= 30) {
    lastFrame = millis();
    animStep();
  }

  if (currentScreen == SCR_PHOTO) photoStep();
}
