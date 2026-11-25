// Web emulator of the Four-Track recorder. The screen logic and drawing are the firmware's own
// C++ (compiled to WebAssembly and run in the audio thread). This page draws what it produces,
// with the Tracker's real font bitmaps, and turns clicks and keys into button presses.
(function () {
  'use strict';
  const FONTS = JSON.parse(document.getElementById('fonts-data').textContent);
  const WASM = Uint8Array.from(atob(document.getElementById('wasm-data').textContent), (c) => c.charCodeAt(0));
  const WORKLET = document.getElementById('worklet-src').textContent;

  // Key ids, same order as core/ft/App.h
  const K = { B1: 0, B2: 1, B3: 2, B4: 3, B5: 4, B6: 5, B7: 6, B8: 7, Up: 8, Down: 9, Left: 10, Right: 11, Rec: 12, Play: 13, Stop: 14, Shift: 15 };

  // Colours: the Tracker's "Original" theme roles, in the order of the Palette in core/ft/Gfx.h
  const PALETTE = [0x000000, 0x777777, 0xffffff, 0x575757, 0x222222, 0x3f3f3f, 0x3f3f3f, 0x222222, 0x777777, 0xcd1818, 0xd67022, 0x30cb3f];

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
      else if (m.type === 'ready') document.getElementById('rate').textContent = m.sampleRate + ' Hz';
    };
    node.port.postMessage({ type: 'init', wasm: WASM, palette: PALETTE, advances: advances() });
    node.connect(audio.destination);
    setInput('demo');
    if (audio.state === 'suspended') await audio.resume();
  }

  let micSource = null;
  async function setInput(kind) {
    document.querySelectorAll('[name=input]').forEach((r) => (r.checked = r.value === kind));
    if (micSource) { micSource.disconnect(); micSource = null; }
    // Hearing a microphone through speakers feeds back, so monitoring starts off for it.
    const mon = document.getElementById('monitor');
    mon.checked = kind === 'demo';
    send({ type: 'monitor', on: mon.checked });
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

  document.getElementById('overlay').addEventListener('click', start);
  document.querySelectorAll('[data-key]').forEach((el) => {
    el.addEventListener('click', () => press(K[el.dataset.key]));
  });

  const jog = document.getElementById('jog');
  let jogAngle = 0;
  function turn(d) { jogAngle += d * 18; jog.style.transform = 'rotate(' + jogAngle + 'deg)'; start().then(() => send({ type: 'jog', delta: d })); }
  jog.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY > 0 ? 1 : -1); }, { passive: false });
  canvas.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY > 0 ? 1 : -1); }, { passive: false });
  document.getElementById('jogL').addEventListener('click', () => turn(-1));
  document.getElementById('jogR').addEventListener('click', () => turn(1));

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
  window.addEventListener('keydown', (e) => {
    if (e.repeat || e.target.tagName === 'INPUT') return;
    if (e.key === '[') { turn(-1); return; }
    if (e.key === ']') { turn(1); return; }
    const k = KEYMAP[e.key];
    if (k) { e.preventDefault(); press(K[k]); }
  });

  document.querySelectorAll('[name=input]').forEach((r) => r.addEventListener('change', () => start().then(() => setInput(r.value))));
  const soundBtn = document.getElementById('sound');
  soundBtn.addEventListener('click', async () => {
    if (!audio) return;
    if (audio.state === 'running') { await audio.suspend(); soundBtn.textContent = 'Sound off (click to turn on)'; }
    else { await audio.resume(); soundBtn.textContent = 'Sound on (click to turn off)'; }
  });
  document.getElementById('monitor').addEventListener('change', (e) => send({ type: 'monitor', on: e.target.checked }));
  document.getElementById('font').addEventListener('change', (e) => { fontSet = e.target.value; glyphs = {}; if (node) node.port.postMessage({ type: 'init-fonts', advances: advances() }); });

  // Initial picture before audio starts: a blank device screen
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, 800, 480);
  requestAnimationFrame(loop);
})();
