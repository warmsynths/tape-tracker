// Web emulator of the Four-Track recorder. The screen logic and drawing are the firmware's own
// C++ (compiled to WebAssembly and run in the audio thread). This page draws what it produces,
// with the Tracker's real font bitmaps, and turns clicks and keys into button presses.
(function () {
  'use strict';
  const FONTS = JSON.parse(document.getElementById('fonts-data').textContent);
  const WASM = Uint8Array.from(atob(document.getElementById('wasm-data').textContent), (c) => c.charCodeAt(0));
  const WORKLET = document.getElementById('worklet-src').textContent;

  // Key ids, same order as core/ft/App.h
  const K = { B1: 0, B2: 1, B3: 2, B4: 3, B5: 4, B6: 5, B7: 6, B8: 7, Up: 8, Down: 9, Left: 10, Right: 11, Rec: 12, Play: 13, Stop: 14, Jog: 15, Sample: 16, Perf: 17, Master: 18, File: 19, Song: 20, Pattern: 21, Params: 22, SamplePlay: 23, SampleLoad: 24 };

  // Colours, in the order of the Palette in core/ft/Gfx.h: bg, top, topText, frame, text, dim,
  // cell, sel, panel, rec, meter, track, wave, waveBg, waveOff, marker, warn
  const THEMES = {
    design: [0x15130f, 0x221f19, 0xd3c8ae, 0x3a342a, 0xe6dcc3, 0x91876f, 0x211e18, 0x332e25, 0x1c1a15, 0xc4452e, 0x93a66a, 0x2c2922, 0x8d826c, 0x191712, 0x575043, 0xd8873a, 0xd2a94f],
    original: [0x000000, 0x222222, 0xffffff, 0x3f3f3f, 0xffffff, 0x575757, 0x222222, 0x3f3f3f, 0x222222, 0xcd1818, 0x30cb3f, 0x222222, 0x777777, 0x0c0c0c, 0x3f3f3f, 0xd67022, 0xd67022],
  };
  let PALETTE = THEMES.design;

  let fontSet = 'liberation';
  let glyphs = {}; // cache: set|font|ch|color -> canvas
  const canvas = document.getElementById('screen');
  const ctx = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  let frame = null;
  let node = null, audio = null, started = false;

  function css(rgb) { return '#' + ('000000' + (rgb >>> 0).toString(16)).slice(-6); }

  function glyph(font, ch, color) {
    const key = fontSet + '|' + font + '|' + ch + '|' + color;
    let c = glyphs[key];
    if (c) return c;
    const f = FONTS[fontSet][font];
    const b64 = f.glyphs[ch];
    c = document.createElement('canvas');
    c.width = f.width; c.height = f.height;
    if (b64) {
      const raw = atob(b64);
      const g = c.getContext('2d');
      const img = g.createImageData(f.width, f.height);
      const r = (color >> 16) & 255, gr = (color >> 8) & 255, b = color & 255;
      for (let i = 0; i < raw.length; i++) {
        const a = raw.charCodeAt(i) * 17;
        img.data[i * 4] = r; img.data[i * 4 + 1] = gr; img.data[i * 4 + 2] = b; img.data[i * 4 + 3] = a;
      }
      g.putImageData(img, 0, 0);
    }
    glyphs[key] = c;
    return c;
  }

  function advances() { return FONTS[fontSet].map((f) => f.adv); }

  function draw(f) {
    ctx.fillStyle = css(PALETTE[0]);
    ctx.fillRect(0, 0, 800, 480);
    let color = 0xffffff;
    const cmd = f.cmd, pool = f.pool;
    for (let i = 0; i < cmd.length; i += 7) {
      const op = cmd[i], a = cmd[i + 1], b = cmd[i + 2], c = cmd[i + 3], d = cmd[i + 4], e = cmd[i + 5], n = cmd[i + 6];
      if (op === 1) { color = a; ctx.fillStyle = css(a); }
      else if (op === 2) ctx.fillRect(a, b, c, d);
      else if (op === 3) {
        if (b === d) ctx.fillRect(Math.min(a, c), b, Math.abs(c - a) + 1, 1);
        else ctx.fillRect(a, Math.min(b, d), 1, Math.abs(d - b) + 1);
      } else if (op === 4) { ctx.beginPath(); ctx.arc(a + 0.5, b + 0.5, c, 0, 6.2832); ctx.fill(); }
      else if (op === 5) {
        let x = a;
        for (let k = 0; k < e; k++) {
          const ch = pool[d + k];
          const g = glyph(c, ch, color);
          ctx.drawImage(g, x, b);
          x += FONTS[fontSet][c].adv[ch & 127];
        }
      } else if (op === 6) {
        for (let k = 0; k < n; k++) {
          const h = pool[e + k];
          ctx.fillRect(a + k * c, b - (h >> 1), d, h);
        }
      }
    }
  }

  function loop() {
    if (frame) { draw(frame); frame = null; }
    requestAnimationFrame(loop);
  }

  function send(m) { if (node) node.port.postMessage(m); }

  async function start() {
    if (started) return;
    started = true;
    document.getElementById('overlay').style.display = 'none';
    audio = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
    // A blob URL works when the page is served; a page opened from a file has an opaque origin
    // that blocks blob URLs for worklets, so fall back to a data URL.
    try {
      const url = URL.createObjectURL(new Blob([WORKLET], { type: 'application/javascript' }));
      await audio.audioWorklet.addModule(url);
    } catch (err) {
      await audio.audioWorklet.addModule('data:text/javascript;base64,' + btoa(unescape(encodeURIComponent(WORKLET))));
    }
    node = new AudioWorkletNode(audio, 'four-track', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
    node.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'frame') frame = m;
      else if (m.type === 'changed') filesChanged(m.paths);
      else if (m.type === 'file') fileArrived(m);
      else if (m.type === 'sample-result') sampleDone(m);
      else if (m.type === 'ready') document.getElementById('rate').textContent = m.sampleRate + ' Hz';
    };
    node.port.postMessage({ type: 'init', wasm: WASM, palette: PALETTE, advances: advances(), files: (await loadSaved()).concat(demoCard()) });
    node.connect(audio.destination);
    setInput('demo');
    if (audio.state === 'suspended') await audio.resume();
  }

  let micSource = null;
  async function setInput(kind) {
    document.querySelectorAll('[name=input]').forEach((r) => (r.checked = r.value === kind));
    if (micSource) { micSource.disconnect(); micSource = null; }
    if (kind === 'demo') { send({ type: 'demo', on: true }); return; }
    send({ type: 'demo', on: false });
    if (kind === 'mic') {
      try {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 2 } });
        micSource = audio.createMediaStreamSource(stream);
        micSource.connect(node);
      } catch (err) {
        document.getElementById('note').textContent = 'Microphone not available: ' + err.message + '. Using the demo instrument.';
        setInput('demo');
      }
    }
  }

  // ------------------------------------------------------------------------------------------
  // The device: screen scaling, buttons, jog wheel, pads
  function press(key) { start().then(() => send({ type: 'key', key })); }
  function release(key) { send({ type: 'keyup', key }); }

  let shiftOn = false;
  function setShift(on) {
    if (shiftOn === on) return;
    shiftOn = on;
    document.getElementById('shift').classList.toggle('on', on);
    send({ type: 'shift', on });
  }
  document.getElementById('shift').addEventListener('click', () => start().then(() => setShift(!shiftOn)));

  document.getElementById('overlay').addEventListener('click', start);
  document.querySelectorAll('[data-key]').forEach((el) => {
    const k = K[el.dataset.key];
    el.addEventListener('pointerdown', (e) => { e.preventDefault(); press(k); });
    el.addEventListener('pointerup', () => release(k));
    el.addEventListener('pointerleave', () => { if (el.matches(':active')) release(k); });
  });

  const jog = document.getElementById('jog');
  let jogAngle = 0;
  function turn(d) { jogAngle += d * 18; jog.style.transform = 'rotate(' + jogAngle + 'deg)'; start().then(() => send({ type: 'jog', delta: d })); }
  jog.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY > 0 ? 1 : -1); }, { passive: false });
  canvas.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY > 0 ? 1 : -1); }, { passive: false });
  // drag round the wheel to turn it
  let dragAngle = null, dragAcc = 0;
  jog.addEventListener('pointerdown', (e) => { jog.setPointerCapture(e.pointerId); const r = jog.getBoundingClientRect(); dragAngle = Math.atan2(e.clientY - (r.top + r.height / 2), e.clientX - (r.left + r.width / 2)); dragAcc = 0; });
  jog.addEventListener('pointermove', (e) => {
    if (dragAngle === null) return;
    const r = jog.getBoundingClientRect();
    const a = Math.atan2(e.clientY - (r.top + r.height / 2), e.clientX - (r.left + r.width / 2));
    let d = a - dragAngle; if (d > Math.PI) d -= 2 * Math.PI; if (d < -Math.PI) d += 2 * Math.PI;
    dragAngle = a; dragAcc += d;
    while (dragAcc > 0.31) { turn(1); dragAcc -= 0.31; }
    while (dragAcc < -0.31) { turn(-1); dragAcc += 0.31; }
  });
  jog.addEventListener('pointerup', () => { dragAngle = null; });

  const padGrid = document.getElementById('pads');
  for (let i = 0; i < 48; i++) {
    const b = document.createElement('button');
    b.className = 'pad';
    b.dataset.pad = i;
    const down = (e) => { e.preventDefault(); b.classList.add('on'); start().then(() => send({ type: 'pad', pad: i, down: true })); };
    const up = () => { if (b.classList.contains('on')) { b.classList.remove('on'); send({ type: 'pad', pad: i, down: false }); } };
    b.addEventListener('pointerdown', down);
    b.addEventListener('pointerup', up);
    b.addEventListener('pointerleave', up);
    padGrid.appendChild(b);
  }

  // Buttons under the screen sit exactly under the eight columns of the screen
  const KEYMAP = { '1': 'B1', '2': 'B2', '3': 'B3', '4': 'B4', '5': 'B5', '6': 'B6', '7': 'B7', '8': 'B8',
    ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right', r: 'Rec', R: 'Rec', ' ': 'Play', s: 'Stop', S: 'Stop' };
  window.addEventListener('keyup', (e) => {
    if (e.key === 'Shift') { setShift(false); return; }
    const k = KEYMAP[e.key];
    if (k) release(K[k]);
  });
  window.addEventListener('keydown', (e) => {
    if (e.repeat || e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
    if (e.key === 'Shift') { start().then(() => setShift(true)); return; }
    if (e.key === 'Enter' || e.key === 'i') { press(K.Jog); return; }
    if (e.key === 'c') { press(K.Song); return; }
    if (e.key === 'h') { press(K.Pattern); return; }
    if (e.key === 'p') { press(K.Params); return; }
    if (e.key === 'l') { press(K.SamplePlay); return; }
    if (e.key === 'o') { press(K.SampleLoad); return; }
    if (e.key === 'e') { press(K.Sample); return; }
    if (e.key === 'g') { press(K.Perf); return; }
    if (e.key === 'm') { press(K.Master); return; }
    if (e.key === 'f') { press(K.File); return; }
    if (e.key === '[') { turn(-1); return; }
    if (e.key === ']') { turn(1); return; }
    const k = KEYMAP[e.key];
    if (k) { e.preventDefault(); press(K[k]); }
  });

  document.querySelectorAll('[name=input]').forEach((r) => r.addEventListener('change', () => start().then(() => setInput(r.value))));
  const soundBtn = document.getElementById('sound');
  soundBtn.addEventListener('click', async () => {
    if (!audio) return;
    if (audio.state === 'running') { await audio.suspend(); soundBtn.textContent = 'Sound off'; }
    else { await audio.resume(); soundBtn.textContent = 'Sound on'; }
  });

  document.getElementById('theme').addEventListener('change', (e) => {
    PALETTE = THEMES[e.target.value];
    send({ type: 'palette', palette: PALETTE });
  });

  // ------------------------------------------------------------------------------------------
  // Files: the project files are kept in browser storage; exports are downloaded from the list
  const DB = 'four-track-emulator';
  function db() {
    return new Promise((res, rej) => {
      const r = indexedDB.open(DB, 1);
      r.onupgradeneeded = () => r.result.createObjectStore('files');
      r.onsuccess = () => res(r.result);
      r.onerror = () => rej(r.error);
    });
  }
  async function dbPut(path, data) { try { const d = await db(); d.transaction('files', 'readwrite').objectStore('files').put(data, path); } catch (e) { /* storage unavailable */ } }
  // A few sounds on the pretend SD card so Load samples has something to browse. Not kept in
  // browser storage; files you add yourself are.
  function wavBytes(chans, rate) {
    const ch = chans.length, n = chans[0].length, out = new Uint8Array(44 + n * ch * 2), dv = new DataView(out.buffer);
    const w4 = (at, t) => { for (let i = 0; i < 4; i++) out[at + i] = t.charCodeAt(i); };
    w4(0, 'RIFF'); dv.setUint32(4, 36 + n * ch * 2, true); w4(8, 'WAVE'); w4(12, 'fmt '); dv.setUint32(16, 16, true);
    dv.setUint16(20, 1, true); dv.setUint16(22, ch, true); dv.setUint32(24, rate, true); dv.setUint32(28, rate * ch * 2, true);
    dv.setUint16(32, ch * 2, true); dv.setUint16(34, 16, true); w4(36, 'data'); dv.setUint32(40, n * ch * 2, true);
    for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++) dv.setInt16(44 + (i * ch + c) * 2, Math.max(-1, Math.min(1, chans[c][i])) * 30000, true);
    return out;
  }
  function demoCard() {
    const R = 44100;
    let seed = 7;
    const noise = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 2147483648 - 1; };
    const make = (sec, fn) => { const n = Math.floor(R * sec), a = new Float32Array(n); let ph = 0, lp = 0; for (let i = 0; i < n; i++) { const e = 1 - i / n; a[i] = fn(i / R, e, () => noise(), (f) => (ph += f / R), (v, k) => (lp = lp * k + v * (1 - k))); } return a; };
    const tri = (p) => 4 * Math.abs(p - Math.floor(p + 0.5)) - 1;
    const kick = (len, lo, hi) => make(len, (t, e, nz, ph) => tri(ph(lo + hi * e * e * e)) * e * e);
    const hat = (len) => make(len, (t, e, nz, ph, lp) => { const x = nz(); return (x - lp(x, 0.5)) * e * e * e; });
    const snare = (len) => make(len, (t, e, nz, ph) => (tri(ph(190)) * 0.4 + nz() * 0.6) * e * e);
    const tom = (len, f) => make(len, (t, e, nz, ph) => tri(ph(f * (0.7 + 0.3 * e))) * e * e);
    const clap = (len) => make(len, (t, e, nz) => nz() * e * e * (Math.floor(t * 100) % 3 === 2 ? 1 : 0.5) * 0.7);
    const rim = (len) => make(len, (t, e, nz, ph) => (tri(ph(1700)) * 0.6 + nz() * 0.3) * e * e * e);
    const tone = (len, f) => make(len, (t, e, nz, ph) => Math.sin(2 * Math.PI * ph(f)) * e * 0.8);
    const files = [
      ['Drums/Clap 01.wav', [clap(0.42)]], ['Drums/Hat closed.wav', [hat(0.18)]], ['Drums/Hat open.wav', [hat(0.86)]],
      ['Drums/Kick 04.wav', [kick(0.64, 42, 120)]], ['Drums/Rim.wav', [rim(0.12)]], ['Drums/Snare 02.wav', [snare(0.51)]],
      ['Drums/Tom low.wav', [tom(0.92, 90)]],
      ['Drums/808/Kick 808.wav', [kick(0.9, 38, 90)]], ['Drums/808/Snare 808.wav', [snare(0.35)]], ['Drums/808/Hat 808.wav', [hat(0.1)]],
      ['Drums/Acoustic/Kick wood.wav', [kick(0.4, 60, 70)]], ['Drums/Acoustic/Snare rim.wav', [snare(0.28)]], ['Drums/Acoustic/Tom hi.wav', [tom(0.5, 180)]],
      ['Keys/Sine A3.wav', [tone(1.5, 220)]], ['Keys/Sine C4 stereo.wav', [tone(1.2, 261.63), tone(1.2, 262.5)]],
    ];
    return files.map(([p, ch]) => ({ path: '/Samples/' + p, data: wavBytes(ch, R) }));
  }
  const addToCard = async (e) => {
    await start();
    for (const f of e.target.files) {
      const rel = f.webkitRelativePath || f.name;
      const path = '/Samples/' + rel;
      const data = new Uint8Array(await f.arrayBuffer());
      send({ type: 'putfile', path, data });
      dbPut(path, data);
    }
    document.getElementById('note').textContent = e.target.files.length + ' file(s) added to the card under Samples.';
    e.target.value = '';
  };
  document.getElementById('sdFiles').addEventListener('change', addToCard);
  document.getElementById('sdFolder').addEventListener('change', addToCard);
  async function dbDel(path) { try { const d = await db(); d.transaction('files', 'readwrite').objectStore('files').delete(path); } catch (e) { /* ignore */ } }
  async function loadSaved() {
    try {
      const d = await db();
      return await new Promise((res) => {
        const out = [];
        const c = d.transaction('files').objectStore('files').openCursor();
        c.onsuccess = () => { const cur = c.result; if (cur) { out.push({ path: cur.key, data: cur.value }); cur.continue(); } else res(out); };
        c.onerror = () => res([]);
      });
    } catch (e) { return []; }
  }
  const known = new Map(); // exported files and projects by path
  let timers = {};
  function filesChanged(list) {
    list.forEach(({ path, size }) => {
      if (path.startsWith('undo/')) return;
      if (size < 0) { known.delete(path); dbDel(path); renderFiles(); return; }
      known.set(path, size);
      clearTimeout(timers[path]);
      timers[path] = setTimeout(() => send({ type: 'getfile', path, why: path.startsWith('Projects/') ? 'save' : 'list' }), 700);
    });
    renderFiles();
  }
  function fileArrived(m) {
    if (!m.data) return;
    known.set(m.path, m.data.length);
    if (m.why === 'save') dbPut(m.path, m.data);
    else if (m.why === 'download') {
      const a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([m.data], { type: m.path.endsWith('.wav') ? 'audio/wav' : 'application/octet-stream' }));
      a.download = m.path.split('/').pop();
      a.click();
    }
    renderFiles();
  }
  function renderFiles() {
    const box = document.getElementById('files');
    box.textContent = '';
    [...known.keys()].filter((p) => p.startsWith('Exports/') || p.startsWith('Projects/')).sort().forEach((p) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.textContent = p + ' (' + Math.round(known.get(p) / 1024) + ' KB)';
      b.addEventListener('click', () => send({ type: 'getfile', path: p, why: 'download' }));
      box.appendChild(b);
    });
    if (!box.children.length) box.textContent = 'Nothing saved yet.';
  }
  renderFiles();

  // ------------------------------------------------------------------------------------------
  // Samples for the pads
  document.getElementById('sampleFile').addEventListener('change', async (e) => {
    await start();
    const slot = Math.max(0, Math.min(47, (parseInt(document.getElementById('sampleSlot').value, 10) || 1) - 1));
    for (const file of e.target.files) {
      try {
        const buf = await audio.decodeAudioData(await file.arrayBuffer());
        const n = buf.length, mono = new Int16Array(n);
        for (let i = 0; i < n; i++) {
          let s = 0;
          for (let c = 0; c < buf.numberOfChannels; c++) s += buf.getChannelData(c)[i];
          s /= buf.numberOfChannels;
          mono[i] = Math.max(-1, Math.min(1, s)) * 32767;
        }
        send({ type: 'sample', slot, data: mono, name: file.name.replace(/\.[^.]+$/, '') });
        document.getElementById('sampleSlot').value = slot + 2;
        return;
      } catch (err) { document.getElementById('note').textContent = 'Could not read ' + file.name; }
    }
  });
  function sampleDone(m) { document.getElementById('note').textContent = m.ok ? 'Sample loaded.' : 'No room left in sample memory.'; }

  // Stock screens: the dimmed buttons show a picture of what the Tracker shows there
  const STOCK = JSON.parse(document.getElementById('stock-data').textContent);
  const STOCK_TEXT = {
    'params': ['Params', 'The stock instrument parameters screen.'],
    'config': ['Config', 'The stock settings screen: the device, audio, MIDI, firmware update.'],
    'sample-load': ['Sample Load', 'The stock file browser for loading samples and instruments.'],
    'sample-play': ['Sample Play', 'The stock sample player and play-mode screen.'],
    'sample-rec': ['Sample Rec', 'The stock sample recorder.'],
  };
  const stockBox = document.getElementById('stock');
  function closeStock() { stockBox.hidden = true; }
  document.querySelectorAll('[data-stock]').forEach((b) => b.addEventListener('click', () => {
    const k = b.dataset.stock;
    const img = document.getElementById('stockImg'), card = document.getElementById('stockCard');
    if (STOCK[k]) { img.src = STOCK[k]; img.style.display = 'block'; card.style.display = 'none'; }
    else { img.style.display = 'none'; card.style.display = 'block'; card.innerHTML = '<b>' + STOCK_TEXT[k][0] + '</b>' + STOCK_TEXT[k][1] + '<br><br><small>The picture has not been added yet.</small>'; }
    stockBox.hidden = false;
  }));
  stockBox.addEventListener('click', closeStock);
  document.querySelectorAll('[data-key], #shift').forEach((b) => b.addEventListener('pointerdown', closeStock));
  window.addEventListener('keydown', closeStock);

  // The device is drawn at its real size and scaled to fit the window
  function fit() {
    const st = document.getElementById('scaleStage');
    st.style.transform = '';
    const w = 1200, h = st.offsetHeight + 16;
    const s = Math.min(window.innerWidth / w, window.innerHeight / h, 1.6);
    st.style.transform = 'scale(' + s + ')';
    document.getElementById('app').style.minHeight = (h * s) + 'px';
  }
  window.addEventListener('resize', fit);
  fit();
  const drawer = document.getElementById('drawer');
  const gear = document.getElementById('gear');
  function toggleDrawer(show) { drawer.hidden = !show; gear.setAttribute('aria-expanded', String(show)); }
  gear.addEventListener('click', () => toggleDrawer(drawer.hidden));
  document.getElementById('drawerClose').addEventListener('click', () => toggleDrawer(false));
  // the last button pressed, in the strip under the device
  document.querySelectorAll('.hw-btn[title], .hw-fn-row .hw-btn').forEach((b) => b.addEventListener('pointerdown', () => {
    const t = (b.title || '').replace(/ \(.*\)$/, '') || b.textContent;
    const le = document.getElementById('lastEvent'); if (le) le.innerHTML = '<span>Last:</span>' + (b.classList.contains('dim') ? t : (b.textContent.replace(/\s+/g, ' ')));
  }));

  // Initial picture before audio starts: a blank device screen
  ctx.fillStyle = css(PALETTE[0]); ctx.fillRect(0, 0, 800, 480);
  requestAnimationFrame(loop);
})();
