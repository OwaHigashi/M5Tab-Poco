// webpage.h
// HTML+CSS+JS served by the M5Tab-Poco device for external browsers.
// Stored in PROGMEM to save RAM. External media (video/audio) pulled from
// public CDNs so the page itself is also a flashy remote FX show.

#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Tab-Poco Remote</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  html,body { margin:0; padding:0; height:100%; background:#0a0a0f; color:#eee;
              font-family: -apple-system, "Hiragino Sans", "Noto Sans JP", sans-serif; }
  header { padding: 14px 20px; display:flex; justify-content:space-between; align-items:center;
           background: linear-gradient(90deg, #111, #222); border-bottom: 1px solid #333; }
  header h1 { margin:0; font-size:20px; letter-spacing:2px; }
  #status { font-size: 13px; opacity:.75; }
  main { display:grid; grid-template-columns: repeat(3, 1fr); gap:12px; padding:16px; }
  button.fx {
    position:relative; overflow:hidden; border:none; border-radius: 16px;
    aspect-ratio: 3 / 2; font-size: clamp(18px, 4vw, 32px); font-weight: 800;
    color:#fff; cursor:pointer; letter-spacing: 2px;
    transition: transform .08s ease, box-shadow .2s ease;
    box-shadow: 0 6px 18px rgba(0,0,0,.55), inset 0 0 40px rgba(255,255,255,.08);
  }
  button.fx:active { transform: scale(.97); }
  button.fx[disabled] { opacity:.35; cursor:not-allowed; }
  .bomb { background: radial-gradient(circle at 30% 30%, #ff6a00, #8a0303 60%, #200 100%); }
  .clap { background: radial-gradient(circle at 30% 30%, #ffe066, #ff3ea5 50%, #5a00a0 100%); }
  .talk { background: radial-gradient(circle at 30% 30%, #3ee8ff, #0066aa 50%, #002040 100%); }
  .marquee-box { grid-column: 1 / -1; display: grid; gap: 8px;
                 padding: 14px; border-radius: 18px; margin-top: 4px;
                 background: linear-gradient(135deg, #1b1b24, #23103a);
                 box-shadow: 0 8px 28px rgba(0,0,0,.6); }
  .marquee-box textarea { width: 100%; min-height: 66px; resize: vertical;
                          padding: 10px; border-radius: 10px;
                          border: 1px solid #444; background: #0a0a0f;
                          color: #eee; font-size: 15px;
                          font-family: inherit; }
  .marquee-row { display: flex; gap: 4px; flex-wrap: wrap; align-items: center; }
  .marquee-row button { padding: 4px 8px; border-radius: 6px;
                        border: 1px solid #555; background: #222;
                        color: #fff; cursor: pointer; font-weight: 700;
                        font-size: 13px; min-width: 32px; line-height: 1.2; }
  .marquee-row button.send { background: #1b7f3a; border-color: #2a9a4a;
                             padding: 4px 12px; }
  .marquee-row button.stop { background: #7f1b1b; border-color: #9a2a2a;
                             padding: 4px 12px; }
  .marquee-row .sep { width:1px; height:22px; background:#444; margin:0 4px; }
  .mkp-speed.on { background: #0a66aa !important; border-color:#1a8acc !important; }
  .mkp-speed.on::after { content:' ●'; }
  /* Hidden-but-clickable button (for the overflow-permission "<sp>" tag).
     Visually invisible, no border/background/text, still occupies a small
     click region so it can be triggered as a secret button. */
  .mkp-hidden {
    background: transparent !important;
    border: 1px solid transparent !important;
    color: transparent !important;
    width: 28px; padding: 4px 0 !important;
    min-width: 0 !important;
    box-shadow: none !important;
  }
  .mkp-hidden:hover { background: transparent !important; }
  .mkp-hidden:focus { outline: none !important; }
  .mkp-r { background:#7a1a1a !important; }
  .mkp-g { background:#1a7a1a !important; }
  .mkp-b { background:#1a1a7a !important; }
  .mkp-y { background:#7a7a1a !important; }
  .mkp-c { background:#1a7a7a !important; }
  .mkp-m { background:#7a1a7a !important; }
  .mkp-w { background:#555   !important; }
  .mkp-o { background:#b55a00 !important; }
  .mkp-sz { background:#2d2d40 !important; }
  .mkp-u  { background:#333    !important; text-decoration: underline; }
  .mkp-hl { background:#222    !important; box-shadow: inset 0 0 0 2px #888; }
  .marquee-row .spd { margin-left: auto; display:flex; gap:4px; align-items:center; }
  .marquee-row .spd label { font-size:13px; opacity:.75; }
  .marquee-row .spd select { background:#0a0a0f; color:#eee;
                             border:1px solid #444; border-radius:6px;
                             padding:4px 8px; }
  #mqCount { font-size:13px; opacity:.7; }
  .talk.recording {
    background: radial-gradient(circle at 30% 30%, #ff6a6a, #a00000 50%, #300 100%);
    animation: rec-pulse 1s ease-in-out infinite;
  }
  @keyframes rec-pulse {
    0%,100% { box-shadow: 0 8px 28px rgba(255,0,0,.4), inset 0 0 60px rgba(255,255,255,.08); }
    50%     { box-shadow: 0 8px 48px rgba(255,0,0,.9), inset 0 0 80px rgba(255,255,255,.15); }
  }
  footer { text-align:center; padding:14px; opacity:.5; font-size:12px; }
  .overlay { position:fixed; inset:0; pointer-events:none; z-index:9999; }
  .overlay video { width:100%; height:100%; object-fit:cover; }
  .flash { position:fixed; inset:0; background:#fff; opacity:0; pointer-events:none;
           transition: opacity .08s; z-index:9998; }
  .pill { display:inline-block; padding:3px 10px; border-radius:999px; background:#333; margin-left:6px; }
  .on  { background:#1b7f3a; }
  .off { background:#b22525; }
</style>
</head>
<body>
<header>
  <h1>M5Tab-Poco</h1>
  <div id="status">
    受付:<span id="acc" class="pill">...</span>
    音量:<span id="vol" class="pill">...</span>
  </div>
</header>

<main>
  <button class="fx bomb" id="btnBomb">BOMB</button>
  <button class="fx clap" id="btnClap">CHEER</button>
  <button class="fx talk" id="btnTalk">🎙 TALK</button>

  <div class="marquee-box">
    <div class="marquee-row">
      <strong>📢 横スクロール</strong>
      <span id="mqCount" style="margin-left:auto">0 / 100</span>
    </div>
    <textarea id="mqText" maxlength="600"
      placeholder="例: <r>おしらせ</r> です! <big>19時</big>から開始"></textarea>
    <div class="marquee-row">
      <button class="mkp mkp-r" data-tag="r">赤</button>
      <button class="mkp mkp-y" data-tag="y">黄</button>
      <button class="mkp mkp-g" data-tag="g">緑</button>
      <button class="mkp mkp-c" data-tag="c">水</button>
      <button class="mkp mkp-b" data-tag="b">青</button>
      <button class="mkp mkp-m" data-tag="m">紫</button>
      <button class="mkp mkp-o" data-tag="o">橙</button>
      <button class="mkp mkp-w" data-tag="w">白</button>
      <span class="sep"></span>
      <button class="mkp mkp-sz" data-tag="small">小</button>
      <button class="mkp mkp-sz" data-tag="s2">中</button>
      <button class="mkp mkp-sz" data-tag="big">大</button>
      <span class="sep"></span>
      <button class="mkp mkp-u"  data-tag="u">下線</button>
      <button class="mkp mkp-hl" data-tag="hl">強調</button>
      <button class="mkp" data-tag="/">リセット</button>
      <span class="sep"></span>
      <button class="mkp-speed on" data-speed="1">x1</button>
      <button class="mkp-speed" data-speed="2">x2</button>
      <button class="mkp-speed" data-speed="3">x3</button>
      <button class="mkp-speed" data-speed="4">x4</button>
      <button class="mkp-speed" data-speed="5">x5</button>
      <span class="sep"></span>
      <button class="send" id="btnMqSend">流す</button>
      <button class="stop" id="btnMqStop">停止</button>
      <button class="mkp mkp-hidden" data-tag="sp"
              title="" aria-label="割込"></button>
    </div>
  </div>
</main>

<footer>Tap BOMB / CHEER for FX, TALK to speak through the Tab5 speaker.</footer>

<div class="flash" id="flash"></div>
<div class="overlay" id="overlay"></div>

<audio id="sndBomb" preload="auto" crossorigin="anonymous"
  src="https://actions.google.com/sounds/v1/explosions/explosion.ogg"></audio>
<audio id="sndClap" preload="auto" crossorigin="anonymous"
  src="https://actions.google.com/sounds/v1/human_voices/human_applause.ogg"></audio>

<script>
// Derive API base from current doc location so the app works whether it is
// served directly (http://10.1.4.10/) or under a reverse-proxy subpath
// (https://host/pm/). All fetches go through api(path).
const API_BASE = (() => {
  let p = location.pathname;
  if (p.endsWith('/')) return p;
  const i = p.lastIndexOf('/');
  return i >= 0 ? p.substring(0, i + 1) : '/';
})();
function api(path) {
  return API_BASE + path.replace(/^\/+/, '');
}

async function refreshStatus() {
  try {
    const r = await fetch(api('/status'), { cache: 'no-store' });
    const j = await r.json();
    const acc = document.getElementById('acc');
    acc.textContent = j.accept ? 'ON' : 'OFF';
    acc.className = 'pill ' + (j.accept ? 'on' : 'off');
    document.getElementById('vol').textContent = j.volume + '/255';
    document.getElementById('btnBomb').disabled = !j.accept;
    document.getElementById('btnClap').disabled = !j.accept;
    // Don't disable TALK while recording (would strand the active session).
    if (!talkActive) document.getElementById('btnTalk').disabled = !j.accept;
  } catch (e) { /* network blip */ }
}

function flashScreen(color) {
  const f = document.getElementById('flash');
  f.style.background = color || '#fff';
  f.style.opacity = '1';
  setTimeout(() => { f.style.opacity = '0'; }, 120);
}

async function trigger(path, sfxId) {
  try { document.getElementById(sfxId).play().catch(() => {}); } catch(e) {}
  try {
    const r = await fetch(api(path), { method: 'POST' });
    if (!r.ok) console.log('server rejected', r.status);
  } catch(e) { console.log(e); }
}

document.getElementById('btnBomb').onclick = () => {
  flashScreen('#fff');
  trigger('/bomb', 'sndBomb');
};
document.getElementById('btnClap').onclick = () => {
  flashScreen('#ffe066');
  trigger('/clap', 'sndClap');
};

// ======================================================
//  TALK: capture mic, downsample to 16 kHz Int16 mono,
//  POST /talk?sr=16000 every ~500ms until stopped.
// ======================================================
const TALK_TARGET_SR = 16000;
const TALK_CHUNK_MS  = 500;
let talkActive = false;
let talkCtx = null, talkStream = null, talkProc = null, talkSrc = null;
let talkFrames = [];      // Float32Array[] at ctx.sampleRate
let talkSendTimer = null;

function updateTalkUi() {
  const btn = document.getElementById('btnTalk');
  if (talkActive) {
    btn.classList.add('recording');
    btn.textContent = '🔴 REC — tap to stop';
    btn.disabled = false;
  } else {
    btn.classList.remove('recording');
    btn.textContent = '🎙 TALK';
  }
}

function drainAndSend() {
  if (talkFrames.length === 0) return;
  let total = 0;
  for (const f of talkFrames) total += f.length;
  const merged = new Float32Array(total);
  let p = 0;
  for (const f of talkFrames) { merged.set(f, p); p += f.length; }
  talkFrames = [];

  // Resample to TALK_TARGET_SR (linear interpolation — adequate for voice).
  const srcSr = talkCtx ? talkCtx.sampleRate : TALK_TARGET_SR;
  let resampled;
  if (Math.abs(srcSr - TALK_TARGET_SR) < 1) {
    resampled = merged;
  } else {
    const ratio = srcSr / TALK_TARGET_SR;
    const outLen = Math.floor(merged.length / ratio);
    resampled = new Float32Array(outLen);
    for (let i = 0; i < outLen; i++) {
      const x = i * ratio;
      const i0 = Math.floor(x);
      const frac = x - i0;
      const s0 = merged[i0] || 0;
      const s1 = merged[i0 + 1] || 0;
      resampled[i] = s0 + (s1 - s0) * frac;
    }
  }

  // Float32 [-1,1] → Int16 LE
  const pcm = new Int16Array(resampled.length);
  for (let i = 0; i < resampled.length; i++) {
    let s = resampled[i];
    if (s > 1) s = 1; else if (s < -1) s = -1;
    pcm[i] = (s * 32767) | 0;
  }
  if (pcm.length === 0) return;

  fetch(api('/talk?sr=' + TALK_TARGET_SR), {
    method: 'POST',
    headers: {'Content-Type': 'application/octet-stream'},
    body: pcm.buffer
  }).catch(() => {});
}

async function talkStart() {
  try {
    talkStream = await navigator.mediaDevices.getUserMedia({
      audio: {
        channelCount: 1,
        noiseSuppression: true,
        echoCancellation: true,
        autoGainControl: true
      }
    });
  } catch (e) {
    alert('マイクを開けませんでした: ' + e.message +
          '\n(HTTPS 経由でアクセスしているか確認してください)');
    return;
  }
  const Ctx = window.AudioContext || window.webkitAudioContext;
  // Hint target rate; browsers that honor it give us 16kHz directly.
  try { talkCtx = new Ctx({sampleRate: TALK_TARGET_SR}); }
  catch (e) { talkCtx = new Ctx(); }

  talkSrc  = talkCtx.createMediaStreamSource(talkStream);
  talkProc = talkCtx.createScriptProcessor(4096, 1, 1);
  talkProc.onaudioprocess = (ev) => {
    if (!talkActive) return;
    // Input buffer is recycled — clone.
    talkFrames.push(new Float32Array(ev.inputBuffer.getChannelData(0)));
  };
  talkSrc.connect(talkProc);
  // ScriptProcessor needs a sink to actually run — use a muted gain node
  // instead of ctx.destination so the user doesn't hear their own mic.
  const mute = talkCtx.createGain();
  mute.gain.value = 0;
  talkProc.connect(mute);
  mute.connect(talkCtx.destination);

  talkActive = true;
  talkSendTimer = setInterval(drainAndSend, TALK_CHUNK_MS);
  updateTalkUi();
}

async function talkStop() {
  talkActive = false;
  if (talkSendTimer) { clearInterval(talkSendTimer); talkSendTimer = null; }
  drainAndSend();   // flush last partial chunk
  try { talkProc && talkProc.disconnect(); } catch(e){}
  try { talkSrc  && talkSrc.disconnect();  } catch(e){}
  if (talkStream) { talkStream.getTracks().forEach(t => t.stop()); }
  if (talkCtx)    { try { await talkCtx.close(); } catch(e){} }
  talkCtx = talkStream = talkProc = talkSrc = null;
  updateTalkUi();
}

document.getElementById('btnTalk').onclick = () => {
  if (talkActive) talkStop(); else talkStart();
};

// ======================================================
//  MARQUEE: post text with <r>/<y>/... markup to /marquee.
//  Character count is over plain-text (markup stripped).
// ======================================================
const mqTextEl  = document.getElementById('mqText');
const mqCountEl = document.getElementById('mqCount');
const MQ_PLAIN_MAX = 100;

function mqPlainLen(s) {
  return s.replace(/<[^>]*>/g, '').length;
}
function mqUpdateCount() {
  const n = mqPlainLen(mqTextEl.value);
  mqCountEl.textContent = n + ' / ' + MQ_PLAIN_MAX;
  mqCountEl.style.color = (n > MQ_PLAIN_MAX) ? '#ff6a6a' : '';
}
mqTextEl.addEventListener('input', mqUpdateCount);
mqUpdateCount();

// Color tags (r,g,b,y,c,m,w,o) are closed by </> (the server's </> resets
// the color only). Everything else must be closed explicitly with </tag>.
const MQ_COLOR_TAGS = ['r','g','b','y','c','m','w','o'];
function mqInsert(tag) {
  const el = mqTextEl;
  const s = el.selectionStart, e = el.selectionEnd;
  const v = el.value;
  if (tag === '/') {
    const insert = '</>';
    el.value = v.slice(0, s) + insert + v.slice(e);
    const p = s + insert.length;
    el.focus(); el.setSelectionRange(p, p);
  } else {
    const open = '<' + tag + '>';
    const close = MQ_COLOR_TAGS.includes(tag) ? '</>' : '</' + tag + '>';
    el.value = v.slice(0, s) + open + v.slice(s, e) + close + v.slice(e);
    const p = (s === e) ? s + open.length : e + open.length + close.length;
    el.focus(); el.setSelectionRange(p, p);
  }
  mqUpdateCount();
}
document.querySelectorAll('.mkp').forEach(btn => {
  btn.addEventListener('click', () => mqInsert(btn.dataset.tag));
});

let mqSpeed = 1;
document.querySelectorAll('.mkp-speed').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.mkp-speed').forEach(x => x.classList.remove('on'));
    btn.classList.add('on');
    mqSpeed = parseInt(btn.dataset.speed, 10);
  });
});

document.getElementById('btnMqSend').onclick = async () => {
  const text = mqTextEl.value.trim();
  if (!text) return;
  if (mqPlainLen(text) > MQ_PLAIN_MAX) {
    alert('本文が長すぎます (最大 ' + MQ_PLAIN_MAX + ' 文字)');
    return;
  }
  try {
    const r = await fetch(api('/marquee?speed=' + mqSpeed), {
      method: 'POST',
      headers: {'Content-Type': 'text/plain; charset=utf-8'},
      body: text
    });
    if (!r.ok) {
      let reason = r.status;
      try {
        const j = await r.json();
        if (j.reason === 'lanes_full')
          reason = '5本の通常レーンが満杯です。<sp>テキスト</sp> で囲めば割込レーン (6〜9本目) を使えます。';
        else if (j.reason === 'all_lanes_full')
          reason = '9レーン全部使用中です。どれかが流れ終わるまで待ってください。';
        else if (j.reason === 'empty')
          reason = '本文が空です。';
        else if (j.reason)
          reason = r.status + ': ' + j.reason;
      } catch(e) {}
      alert('送信失敗: ' + reason);
    }
  } catch(e) { alert(e.message); }
};
document.getElementById('btnMqStop').onclick = async () => {
  try { await fetch(api('/marquee/stop'), { method: 'POST' }); } catch(e) {}
};


refreshStatus();
setInterval(refreshStatus, 2000);
</script>
</body>
</html>
)HTML";
