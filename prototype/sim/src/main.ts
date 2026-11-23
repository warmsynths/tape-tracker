import './style.css';
import { AudioEngine, Hit, NoteSpec, renderHits, SR, synth } from './audio';
import { Action, choicesFor, detectFirstOnset, init, MAX_TRACK_SEC, reduce, State } from './logic';
import { CHARS, fmtPos, LiveRefs, PROJECTS, RecState, renderScreen, UIState } from './screen';

interface ActiveRec extends RecState {
  hits: Hit[];
}

const app = document.getElementById('app')!;
app.innerHTML = `
<div class="scale-stage" id="scaleStage">
  <div class="topbar-meta">
    <div><b>ORIGINAL TRACKER · FOUR-TRACK RECORDER</b> <span style="margin-left:8px">MK66 Limits: 30s/track · 600-col waveforms · 10 Hz readouts</span></div>
    <div class="meta-btns">
      <button type="button" id="bTheme">Theme: Original</button>
      <button type="button" id="bReset">Power / Reset</button>
    </div>
  </div>

  <main class="tracker" id="stage" tabindex="0" aria-label="Original Polyend Tracker Hardware Simulator">
    <div class="left-col">
      <div class="screen-frame">
        <div class="screen" id="screen"></div>
      </div>
      <div class="hw-fn-row" id="hwFnRow">
        <button type="button" class="hw-btn">1</button>
        <button type="button" class="hw-btn">2</button>
        <button type="button" class="hw-btn">3</button>
        <button type="button" class="hw-btn">4</button>
        <button type="button" class="hw-btn">5</button>
        <button type="button" class="hw-btn">6</button>
        <button type="button" class="hw-btn">7</button>
        <button type="button" class="hw-btn">8</button>
      </div>
      <div class="pad-grid" id="pads"></div>
    </div>

    <div class="right-col">
      <div class="brand">
        <span>Polyend Tracker</span>
        <span class="limit-pill" id="ramPill">RAM 0.0 / 30s</span>
      </div>

      <div class="jog-box">
        <div class="jog" id="jog" title="Scroll wheel to turn Jog">
          <div class="jog-notch" id="jogNotch"></div>
          <button type="button" class="jog-center" id="bOk" title="Enter / Confirm">OK</button>
        </div>
        <div class="jog-btns">
          <button type="button" class="hw-btn" id="bJogL">↺ Jog −</button>
          <button type="button" class="hw-btn" id="bJogR">Jog + ↻</button>
        </div>
      </div>

      <!-- 15 labelled buttons (3 rows x 5 cols, matching interfaceDefs.h 8..22) -->
      <div class="bank-15">
        <button type="button" class="hw-btn dim" id="bParams">Params</button>
        <button type="button" class="hw-btn dim" id="bPerform">Perform</button>
        <button type="button" class="hw-btn" id="bFile">File</button>
        <button type="button" class="hw-btn" id="bConfig">Config</button>
        <button type="button" class="hw-btn" id="bMaster">Master</button>

        <button type="button" class="hw-btn dim" id="bSLoad">Sample<br>Load</button>
        <button type="button" class="hw-btn dim" id="bSPlay">Sample<br>Play</button>
        <button type="button" class="hw-btn dim" id="bSEdit">Sample<br>Edit</button>
        <button type="button" class="hw-btn" id="bSRec">Sample<br>Rec</button>
        <button type="button" class="hw-btn dim" id="bSong">Song</button>

        <button type="button" class="hw-btn col-g" id="bNote" title="Select Track 1">Note</button>
        <button type="button" class="hw-btn col-y" id="bInstr" title="Select Track 2">Instr</button>
        <button type="button" class="hw-btn col-p" id="bFx1" title="Select Track 3">FX1</button>
        <button type="button" class="hw-btn col-b" id="bFx2" title="Select Track 4">FX2</button>
        <button type="button" class="hw-btn" id="bPattern">Pattern</button>
      </div>

      <!-- 10 labelled buttons (2 rows x 5 cols, matching interfaceDefs.h 23..32) -->
      <div class="bank-10">
        <button type="button" class="hw-btn" id="bPlay">Play</button>
        <button type="button" class="hw-btn dim" id="bCopy">Copy</button>
        <button type="button" class="hw-btn" id="bInsert">Insert</button>
        <button type="button" class="hw-btn" id="bUp">▲</button>
        <button type="button" class="hw-btn" id="bDelete">Delete</button>

        <button type="button" class="hw-btn rec-led" id="bRec">Rec</button>
        <button type="button" class="hw-btn" id="bShift">Shift</button>
        <button type="button" class="hw-btn" id="bLeft">◀</button>
        <button type="button" class="hw-btn" id="bDown">▼</button>
        <button type="button" class="hw-btn" id="bRight">▶</button>
      </div>
    </div>
  </main>

  <div class="footer-strip">
    <div class="last-event" id="lastEvent"><span>Status:</span>Ready. Press Rec (or Enter), play pads/keys, then press Rec again to set Loop.</div>
    <div class="hint">Enter=Rec/OK · Space=Play · 1–4=Track · Arrows=Nav · Wheel=Jog · Backspace=Undo · Esc=Back</div>
  </div>
</div>
`;

const $ = <T extends HTMLElement = HTMLElement>(id: string): T => document.getElementById(id) as T;

function fitToViewport(): void {
  const st = $('scaleStage');
  st.style.transform = 'scale(1)';
  const w = st.scrollWidth;
  const h = st.scrollHeight;
  const s = Math.min(1, (window.innerWidth - 16) / w, (window.innerHeight - 16) / h);
  st.style.transform = `scale(${s.toFixed(4)})`;
}
window.addEventListener('resize', fitToViewport);

const audio = new AudioEngine();
let S: State = init(SR);
let rec: ActiveRec | null = null;
let refs: LiveRefs | null = null;
let offsetOrig = 0;
let jogAngle = 0;
let last10Hz = 0;
let masterMeter = 0;
const meterVals = [0, 0, 0, 0];

const ui: UIState = {
  theme: 'Original',
  view: 'main',
  popup: null,
  shiftHeld: false,
  editingBpm: false,
  offsetField: 'ms',
  setRow: 3,
  inRow: 1,
  projRow: 0,
  nameTitle: 'Save as',
  nameStr: 'BASEMENT 04',
  nameCur: 10,
  projName: 'Basement 04',
  dirty: false,
  inp: { src: 0, gain: 18, mon: true, level: -48, peakDb: -48, clip: false, clipUntil: 0 },
};

const KEYMAP: Record<string, NoteSpec> = {};
const padsEl = $('pads');

const guitarNotes: Array<[string, number, string]> = [
  ['A3', 220, 'a'], ['C4', 261.63, 's'], ['D4', 293.66, 'd'], ['E4', 329.63, 'f'],
  ['G4', 392, 'g'], ['A4', 440, 'h'], ['C5', 523.25, 'j'], ['D5', 587.33, 'k'],
  ['E5', 659.25, 'l'], ['G5', 783.99, ';'], ['A5', 880, ''], ['C6', 1046.5, ''],
];
const bassNotes: Array<[string, number, string]> = [
  ['A1', 55, 'q'], ['C2', 65.41, 'w'], ['D2', 73.42, 'e'], ['E2', 82.41, 'r'],
  ['G2', 98, 't'], ['A2', 110, 'y'], ['C3', 130.81, 'u'], ['D3', 146.83, 'i'],
  ['E3', 164.81, 'o'], ['G3', 196, 'p'], ['A3', 220, ''], ['C4', 261.63, ''],
];
const drumNotes: Array<[string, string, string]> = [
  ['KICK', 'kick', 'z'], ['SNR', 'snare', 'x'], ['CH', 'chat', 'c'], ['OH', 'ohat', 'v'],
  ['CLAP', 'clap', 'b'], ['TOML', 'tomL', 'n'], ['TOMH', 'tomH', 'm'], ['RIM', 'rim', ','],
  ['SHK', 'shaker', '.'], ['COW', 'cowbell', '/'], ['KICK2', 'kick', ''], ['SNR2', 'snare', ''],
];
const padRows: Array<Array<{ label: string; note: NoteSpec; key: string }>> = [
  guitarNotes.map(([name, f, key]) => ({ label: name, note: { kind: 'guitar', f, label: 'Guitar' }, key })),
  bassNotes.map(([name, f, key]) => ({ label: name, note: { kind: 'bass', f, label: 'Bass' }, key })),
  drumNotes.map(([name, kind, key]) => ({ label: name, note: { kind, f: 0, label: 'Drums' }, key })),
  guitarNotes.map(([name, f]) => ({ label: name, note: { kind: 'guitar', f: f * 0.5, label: 'Guitar' }, key: '' })),
];

padRows.forEach((row) => {
  row.forEach((cell) => {
    if (cell.key) KEYMAP[cell.key] = cell.note;
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'pad';
    b.textContent = cell.label;
    if (cell.key) b.title = `Key: ${cell.key.toUpperCase()}`;
    b.onpointerdown = (e) => {
      e.preventDefault();
      b.classList.add('hit');
      hit(cell.note);
      window.setTimeout(() => b.classList.remove('hit'), 100);
      $('stage').focus();
    };
    padsEl.appendChild(b);
  });
});

function ensureCtx(): AudioContext {
  return audio.ensure(onTick, onFrame);
}

function dispatch(a: Action): void {
  S = reduce(S, a);
  if (['FIRST_TAKE', 'CONFIRM_BARS', 'PART_TAKE', 'NUDGE', 'SET_OFFSET', 'UNDO'].includes(a.type)) ui.dirty = true;
  if (a.type === 'RESET') {
    ui.dirty = false;
    audio.stopPlayback();
  }
  if (['PART_TAKE', 'NUDGE', 'SET_OFFSET', 'UNDO', 'CONFIRM_BARS', 'MUTE', 'RESET'].includes(a.type) || (a.type === 'SET_SETTING' && a.key === 'loopMode')) {
    audio.refresh(S);
  }
  redraw();
}

function stopFirstTake(memFull: boolean): void {
  if (!rec || rec.kind !== 'first' || !audio.ctx) return;
  const now = audio.ctx.currentTime;
  const label = rec.label || 'Track 1';
  if (rec.T0 != null) {
    const pre = rec.pre ?? 0;
    const ws = rec.windowStart ?? rec.T0;
    const len = Math.min(Math.round(MAX_TRACK_SEC * SR) + pre, Math.max(pre + SR, Math.round((now - ws) * SR)));
    const raw = renderHits(rec.hits, ws, len);
    rec = null;
    const hadNoSound = detectFirstOnset(raw, SR) < 0;
    S.tracks[0].name = label;
    dispatch({ type: 'FIRST_TAKE_TIMED', raw, preRoll: pre, memFull });
    if (hadNoSound) {
      ui.popup = { kind: 'nosound' };
      redraw();
    } else if (S.status === 'ready') {
      audio.startPlayback(S, audio.ctx.currentTime + 0.05);
      redraw();
    }
    return;
  }
  const len = Math.min(Math.round(MAX_TRACK_SEC * SR), Math.round((now - (rec.t0 ?? now)) * SR));
  const samples = renderHits(rec.hits, rec.t0 ?? now, len);
  rec = null;
  const hadNoSound = detectFirstOnset(samples, SR) < 0;
  S.tracks[0].name = label;
  dispatch({ type: 'FIRST_TAKE', samples, memFull });
  if (hadNoSound) {
    ui.popup = { kind: 'nosound' };
    redraw();
  }
}

function finishPart(): void {
  if (!rec || rec.kind !== 'part') return;
  const r = rec;
  rec = null;
  const pre = r.pre ?? 0;
  const raw = renderHits(r.hits, r.windowStart ?? 0, S.loopLen + pre);
  const hadNoSound = detectFirstOnset(raw, SR) < 0;
  dispatch({ type: 'PART_TAKE', track: r.track ?? 0, raw, preRoll: pre, label: r.label ?? undefined });
  if (hadNoSound) {
    ui.popup = { kind: 'nosound' };
    redraw();
  }
}

function startPartArm(track: number): void {
  const ctx = ensureCtx();
  const loopSec = S.loopLen / SR;
  if (loopSec > MAX_TRACK_SEC) {
    ui.popup = { kind: 'memfull', track };
    redraw();
    return;
  }
  const now = ctx.currentTime;
  const beat = 60 / S.bpm;
  const pre = Math.round((S.settings.preRollMs / 1000) * SR);
  let T0: number;
  if (audio.playing && S.settings.loopMode === 'repeat') {
    T0 = audio.epoch + Math.ceil((now + 0.05 - audio.epoch) / loopSec) * loopSec;
  } else {
    T0 = now + 4 * beat + 0.1;
    for (let k = 4; k >= 1; k--) audio.clickAt(T0 - k * beat, k === 4);
    audio.startPlayback(S, T0);
  }
  rec = {
    kind: 'part',
    track,
    T0,
    windowStart: T0 - pre / SR,
    windowEnd: T0 + loopSec,
    pre,
    hits: [],
    started: false,
    label: null,
  };
  redraw();
}

function doRecord(): void {
  ui.editingBpm = false;
  const ctx = ensureCtx();
  const now = ctx.currentTime;
  if (S.status === 'empty' && !rec) {
    if (S.presetBpm) {
      const beat = 60 / S.presetBpm;
      const pre = Math.round((S.settings.preRollMs / 1000) * SR);
      const T0 = now + 4 * beat + 0.1;
      for (let k = 4; k >= 1; k--) audio.clickAt(T0 - k * beat, k === 4);
      rec = {
        kind: 'first',
        track: 0,
        T0,
        windowStart: T0 - pre / SR,
        pre,
        started: false,
        hits: [],
        label: null,
      };
    } else {
      rec = { kind: 'first', track: 0, t0: now, windowStart: now, started: true, hits: [], label: null };
    }
    redraw();
    return;
  }
  if (rec && rec.kind === 'first') {
    if (rec.T0 != null && now < rec.T0) {
      cancelRec();
      return;
    }
    stopFirstTake(false);
    return;
  }
  if (S.status === 'confirmBars') {
    dispatch({ type: 'CONFIRM_BARS' });
    audio.startPlayback(S, ctx.currentTime + 0.1);
    redraw();
    return;
  }
  if (S.status === 'ready' && !rec) {
    if (S.tracks[S.selected].part) {
      ui.popup = { kind: 'replace', track: S.selected };
      redraw();
      return;
    }
    startPartArm(S.selected);
  }
}

function cancelRec(): void {
  if (rec && rec.kind === 'part') {
    rec = null;
    audio.stopPlayback();
    redraw();
  } else if (rec && rec.kind === 'first') {
    rec = null;
    redraw();
  }
}

function hit(n: NoteSpec): void {
  const ctx = ensureCtx();
  const when = ctx.currentTime;
  if (ui.inp.mon || ui.view !== 'input') audio.playBuf(synth(n.kind, n.f), 0, 0.9);
  const boost = Math.min(0, -14 + (ui.inp.gain - 12) * 0.5);
  ui.inp.level = Math.max(ui.inp.level, boost);
  ui.inp.peakDb = Math.max(ui.inp.peakDb, boost);
  if (boost >= 0) {
    ui.inp.clip = true;
    ui.inp.clipUntil = performance.now() + 2000;
  }
  meterVals[S.selected] = 0.85;
  masterMeter = Math.max(masterMeter, 0.78);
  if (rec && when >= (rec.windowStart ?? 0) && (rec.kind === 'first' || when < (rec.windowEnd ?? 0))) {
    rec.hits.push({ t: when, n });
    if (!rec.label && n.label) rec.label = n.label;
  }
}

function startExport(): void {
  ui.popup = { kind: 'exporting', pct: 0 };
  redraw();
  const iv = window.setInterval(() => {
    if (!ui.popup || ui.popup.kind !== 'exporting') {
      window.clearInterval(iv);
      return;
    }
    ui.popup.pct = Math.min(1, ui.popup.pct + 0.15);
    if (ui.popup.pct >= 1) {
      window.clearInterval(iv);
      ui.popup = null;
    }
    redraw();
  }, 120);
}

function onTick(): void {
  if (!audio.ctx) return;
  const now = audio.ctx.currentTime;
  const loopSec = S.loopLen / SR;
  if (audio.playing && S.bpm && S.settings.click) {
    const beat = 60 / S.bpm;
    while (audio.epoch + audio.nextClick * beat < now + 0.12) {
      const t = audio.epoch + audio.nextClick * beat;
      if (t >= now - 0.01) audio.clickAt(t, audio.nextClick % 4 === 0);
      audio.nextClick++;
    }
  }
  if (audio.playing && S.settings.loopMode === 'once' && S.loopLen && now - audio.epoch > loopSec + 0.05 && !rec) {
    audio.stopPlayback();
    redraw();
  }
  if (rec && rec.kind === 'first' && rec.started && now - (rec.T0 ?? rec.t0 ?? now) >= MAX_TRACK_SEC) stopFirstTake(true);
  if (rec && rec.kind === 'part' && now >= (rec.windowEnd ?? 0) + 0.03) finishPart();
}

function onFrame(ts: number): void {
  requestAnimationFrame(onFrame);
  const now = audio.ctx ? audio.ctx.currentTime : 0;
  for (let i = 0; i < 4; i++) {
    meterVals[i] = Math.max(audio.playing && S.tracks[i].part && !S.tracks[i].mute ? 0.25 + 0.2 * Math.abs(Math.sin(now * 7 + i)) : 0, meterVals[i] * 0.9);
    if (refs?.trackMeterEls[i]) refs.trackMeterEls[i].style.width = `${Math.round(114 * meterVals[i])}px`;
  }
  masterMeter = Math.max(audio.playing ? 0.35 + 0.2 * Math.abs(Math.sin(now * 6)) : 0, masterMeter * 0.9);
  if (refs?.masterFillEl) {
    const mh = ui.view === 'offset' ? 297 : 378;
    const fh = Math.round(mh * masterMeter);
    refs.masterFillEl.style.top = `${58 + mh - fh}px`;
    refs.masterFillEl.style.height = `${fh}px`;
  }
  if (ui.inp.level > -48) {
    ui.inp.level = Math.max(-48, ui.inp.level - 0.6);
    if (refs?.inputFillEl) refs.inputFillEl.style.width = `${Math.max(0, Math.round(((ui.inp.level + 48) / 48) * 720))}px`;
  }
  if (ui.inp.clip && performance.now() > ui.inp.clipUntil) {
    ui.inp.clip = false;
    if (ui.view === 'input') redraw();
  }
  if (refs?.playheadEl && S.loopLen && audio.playing && now >= audio.epoch) {
    const loopSec = S.loopLen / SR;
    const p = S.settings.loopMode === 'repeat' ? (now - audio.epoch) % loopSec : Math.min(loopSec, now - audio.epoch);
    refs.playheadEl.style.left = `${168 + Math.round((p / loopSec) * 600)}px`;
    if (refs.posValEl && ts - last10Hz >= 100) {
      last10Hz = ts;
      refs.posValEl.textContent = fmtPos(S, p);
    }
  } else if (rec && rec.kind === 'first' && rec.started && refs?.posValEl && ts - last10Hz >= 100) {
    last10Hz = ts;
    refs.posValEl.textContent = `${Math.max(0, now - (rec.T0 ?? rec.t0 ?? now)).toFixed(1)} s`;
  }
  if (rec) {
    if (!rec.started && now >= (rec.windowStart ?? 0)) {
      rec.started = true;
      redraw();
    } else if (refs?.armedMsgEl && rec.T0 != null && now < rec.T0 && ts - last10Hz >= 60) {
      last10Hz = ts;
      const activeBpm = S.bpm || S.presetBpm || 120;
      const beatsLeft = Math.min(4, Math.max(1, Math.ceil((rec.T0 - now) / (60 / activeBpm))));
      refs.armedMsgEl.textContent =
        rec.T0 - now <= (4 * 60) / activeBpm + 0.15
          ? `Count-in ${beatsLeft}`
          : `Waiting for loop start · ${(rec.T0 - now).toFixed(1)} s`;
    }
  }
}

function redraw(): void {
  const now = audio.ctx ? audio.ctx.currentTime : 0;
  const isArmed = Boolean(rec && rec.kind === 'part' && now < (rec.windowStart ?? 0));
  const isRec = Boolean((rec && rec.kind === 'first') || (rec && rec.kind === 'part' && now >= (rec.windowStart ?? 0)));

  $('bRec').className = 'hw-btn rec-led' + (isRec || isArmed ? ' active' : '');
  $('bPlay').className = 'hw-btn' + (audio.playing ? ' on' : '');
  $('bShift').className = 'hw-btn' + (ui.shiftHeld ? ' on' : '');
  $('ramPill').textContent = `RAM ${S.loopLen ? (S.loopLen / SR).toFixed(1) : '0.0'} / ${MAX_TRACK_SEC.toFixed(0)}s`;

  refs = renderScreen($('screen'), $('hwFnRow'), S, ui, rec, audio.playing, audio.epoch, now, meterVals, {
    onSelectTrack: (i) => dispatch({ type: 'SELECT', track: i }),
    onMuteTrack: (i) => dispatch({ type: 'MUTE', track: i }),
    onSetSetting: (key, val) => dispatch({ type: 'SET_SETTING', key, value: val }),
    onSetBars: (b) => dispatch({ type: 'SET_BARS', bars: b }),
    onCancelBars: () => dispatch({ type: 'CANCEL_BARS' }),
    onConfirmBars: () => {
      const ctx = ensureCtx();
      dispatch({ type: 'CONFIRM_BARS' });
      audio.startPlayback(S, ctx.currentTime + 0.1);
      redraw();
    },
    onToggleBpmEdit: () => {
      if (S.status === 'empty' && !rec) {
        ui.editingBpm = !ui.editingBpm;
        if (ui.editingBpm && S.presetBpm == null) dispatch({ type: 'SET_PRESET_BPM', bpm: 120 });
        else redraw();
      }
    },
    onNudge: (unit, dir) => dispatch({ type: 'NUDGE', track: S.selected, unit, dir, coarse: ui.shiftHeld }),
    onSetOffset: (offset) => dispatch({ type: 'SET_OFFSET', track: S.selected, offset }),
    onResetOffset: () => { dispatch({ type: 'SET_OFFSET', track: S.selected, offset: offsetOrig }); ui.view = 'main'; redraw(); },
    onUndo: () => dispatch({ type: 'UNDO', track: S.selected }),
    onRecord: doRecord,
    onStopFirstTake: () => stopFirstTake(false),
    onFinishPart: finishPart,
    onCancelRec: cancelRec,
    onStopPlayback: () => { audio.stopPlayback(); redraw(); },
    onStartPartArm: startPartArm,
    onStartExport: startExport,
    onNewProject: () => { dispatch({ type: 'RESET' }); ui.projName = 'Untitled 01'; ui.view = 'main'; redraw(); },
    onRender: redraw,
    onFocusStage: () => $('stage').focus(),
  });

  if (S.log.length) $('lastEvent').innerHTML = `<span>Status:</span>${S.log[0]}`;
}

function jogStep(dir: number, coarse: boolean): void {
  jogAngle = (jogAngle + dir * 24) % 360;
  $('jogNotch').style.transform = `rotate(${jogAngle}deg)`;
  if (ui.popup && ui.popup.kind === 'export') {
    ui.popup.repeats = Math.max(1, Math.min(16, ui.popup.repeats + dir));
    redraw();
    return;
  }
  if (ui.view === 'settings') {
    const keys: Array<keyof State['settings'] | 'ts'> = ['loopMode', 'click', 'ts', 'windowMs', 'preRollMs', 'rounding'];
    const opts: Record<string, Array<State['settings'][keyof State['settings']]>> = {
      loopMode: ['repeat', 'once'],
      click: [false, true],
      windowMs: [0, 30, 60, 100, 150],
      preRollMs: [0, 60, 120, 250, 500],
      rounding: ['off', '0.1', '1'],
    };
    const key = keys[ui.setRow];
    const arr = opts[key];
    if (!arr) return;
    const idx = arr.indexOf(S.settings[key as keyof State['settings']]);
    const nxt = Math.max(0, Math.min(arr.length - 1, idx + dir));
    dispatch({ type: 'SET_SETTING', key: key as keyof State['settings'], value: arr[nxt] });
  } else if (ui.view === 'input') {
    if (ui.inRow === 0) {
      ui.inp.src = Math.max(0, Math.min(2, ui.inp.src + dir));
      ui.inp.mon = ui.inp.src === 0;
      ui.inp.gain = Math.min(ui.inp.gain, [22, 25, 44][ui.inp.src]);
    } else if (ui.inRow === 1) {
      ui.inp.gain = Math.max(0, Math.min([22, 25, 44][ui.inp.src], ui.inp.gain + dir * (coarse ? 4 : 1.5)));
    } else {
      ui.inp.mon = dir > 0;
    }
    redraw();
  } else if (ui.view === 'project') {
    ui.projRow = (ui.projRow + dir + PROJECTS.length) % PROJECTS.length;
    redraw();
  } else if (ui.view === 'name') {
    const idx = Math.max(0, CHARS.indexOf(ui.nameStr[ui.nameCur] || 'A'));
    const nxt = (idx + dir + CHARS.length) % CHARS.length;
    const arr = ui.nameStr.padEnd(ui.nameCur + 1, ' ').split('');
    arr[ui.nameCur] = CHARS[nxt];
    ui.nameStr = arr.join('').trimEnd();
    redraw();
  } else if (S.status === 'confirmBars' && S.pending) {
    const list = choicesFor(S.pending.trimmed.length, SR);
    const i = list.indexOf(S.pending.bars);
    const j = Math.max(0, Math.min(list.length - 1, i + dir));
    dispatch({ type: 'SET_BARS', bars: list[j] });
  } else if (ui.view === 'offset') {
    dispatch({ type: 'NUDGE', track: S.selected, unit: ui.offsetField, dir, coarse });
  } else if (ui.editingBpm && S.status === 'empty' && !rec) {
    const step = coarse ? 10 : 1;
    if (S.presetBpm == null) {
      dispatch({ type: 'SET_PRESET_BPM', bpm: dir > 0 ? 120 : null });
    } else {
      const next = S.presetBpm + dir * step;
      dispatch({ type: 'SET_PRESET_BPM', bpm: next < 40 ? null : next });
    }
  } else {
    dispatch({ type: 'SELECT', track: (S.selected + dir + 4) % 4 });
  }
}

function arrowDir(dx: number, dy: number, coarse: boolean): void {
  if (ui.view === 'settings') {
    if (dy) {
      do { ui.setRow = (ui.setRow + dy + 6) % 6; } while (ui.setRow === 2);
      redraw();
    }
    if (dx) jogStep(dx, coarse);
  } else if (ui.view === 'input') {
    if (dy) { ui.inRow = (ui.inRow + dy + 3) % 3; redraw(); }
    if (dx) jogStep(dx, coarse);
  } else if (ui.view === 'project') {
    if (dy) { ui.projRow = (ui.projRow + dy + PROJECTS.length) % PROJECTS.length; redraw(); }
  } else if (ui.view === 'name') {
    if (dx) { ui.nameCur = Math.max(0, Math.min(15, ui.nameCur + dx)); redraw(); }
    if (dy) jogStep(dy, false);
  } else if (S.status === 'confirmBars') {
    if (dx) jogStep(dx, false);
  } else if (ui.view === 'offset') {
    if (dx) { ui.offsetField = ui.offsetField === 'ms' ? 'bars' : 'ms'; redraw(); }
    if (dy) dispatch({ type: 'NUDGE', track: S.selected, unit: ui.offsetField, dir: -dy, coarse });
  } else if (ui.editingBpm && S.status === 'empty' && !rec && dx) {
    jogStep(dx, coarse);
  } else if (dy) {
    dispatch({ type: 'SELECT', track: (S.selected + dy + 4) % 4 });
  }
}

function pressOk(): void {
  if (ui.popup) {
    if (ui.popup.kind === 'replace') {
      const tr = ui.popup.track;
      ui.popup = null;
      startPartArm(tr);
    } else if (ui.popup.kind === 'nosound' || ui.popup.kind === 'memfull') {
      ui.popup = null;
      redraw();
    } else if (ui.popup.kind === 'export') {
      startExport();
    }
    return;
  }
  if (ui.view === 'offset') { ui.view = 'main'; redraw(); }
  else if (ui.editingBpm) { ui.editingBpm = false; redraw(); }
  else if (ui.view === 'project') { ui.projName = PROJECTS[ui.projRow][0]; ui.dirty = false; ui.view = 'main'; redraw(); }
  else if (ui.view === 'name') { ui.projName = ui.nameStr.trim() || 'UNTITLED'; ui.dirty = false; ui.view = 'project'; redraw(); }
  else if (S.status === 'confirmBars') {
    const ctx = ensureCtx();
    dispatch({ type: 'CONFIRM_BARS' });
    audio.startPlayback(S, ctx.currentTime + 0.1);
    redraw();
  } else {
    doRecord();
  }
}

function pressEsc(): void {
  if (ui.popup) { ui.popup = null; redraw(); }
  else if (ui.editingBpm) { ui.editingBpm = false; redraw(); }
  else if (ui.view !== 'main') {
    if (ui.view === 'offset') dispatch({ type: 'SET_OFFSET', track: S.selected, offset: offsetOrig });
    ui.view = 'main';
    redraw();
  } else {
    cancelRec();
  }
}

Array.from($('hwFnRow').children).forEach((b, i) => {
  (b as HTMLElement).onclick = () => {
    $('stage').focus();
    if (ui.shiftHeld && ui.view === 'main' && i === 5) {
      ui.inp.mon = !ui.inp.mon;
      redraw();
      return;
    }
    if (ui.view === 'main' && i === 3 && S.tracks[S.selected].part) {
      offsetOrig = S.tracks[S.selected].part!.offset;
    }
    refs?.bottomHandlers[i]?.();
  };
});

$('jog').addEventListener('wheel', (e) => {
  e.preventDefault();
  jogStep(e.deltaY > 0 ? 1 : -1, e.shiftKey || ui.shiftHeld);
}, { passive: false });
$('bJogL').onclick = () => jogStep(-1, ui.shiftHeld);
$('bJogR').onclick = () => jogStep(1, ui.shiftHeld);
$('bOk').onclick = pressOk;
$('bInsert').onclick = pressOk;
$('bUp').onclick = () => arrowDir(0, -1, ui.shiftHeld);
$('bDown').onclick = () => arrowDir(0, 1, ui.shiftHeld);
$('bLeft').onclick = () => arrowDir(-1, 0, ui.shiftHeld);
$('bRight').onclick = () => arrowDir(1, 0, ui.shiftHeld);
$('bShift').onclick = () => { ui.shiftHeld = !ui.shiftHeld; redraw(); };
$('bRec').onclick = doRecord;
$('bPlay').onclick = () => {
  const ctx = ensureCtx();
  if (S.status === 'ready') {
    if (audio.playing) { audio.stopPlayback(); redraw(); }
    else { audio.startPlayback(S, ctx.currentTime + 0.1); redraw(); }
  }
};
$('bDelete').onclick = () => dispatch({ type: 'UNDO', track: S.selected });
$('bFile').onclick = () => { ui.view = ui.view === 'project' ? 'main' : 'project'; redraw(); };
$('bPattern').onclick = () => { ui.view = 'main'; redraw(); };
$('bSRec').onclick = () => { ui.view = ui.view === 'input' ? 'main' : 'input'; redraw(); };
$('bConfig').onclick = () => { ui.view = ui.view === 'settings' ? 'main' : 'settings'; redraw(); };
$('bMaster').onclick = () => { ui.view = ui.view === 'settings' ? 'main' : 'settings'; redraw(); };
$('bNote').onclick = () => dispatch({ type: 'SELECT', track: 0 });
$('bInstr').onclick = () => dispatch({ type: 'SELECT', track: 1 });
$('bFx1').onclick = () => dispatch({ type: 'SELECT', track: 2 });
$('bFx2').onclick = () => dispatch({ type: 'SELECT', track: 3 });

$('bTheme').onclick = () => {
  ui.theme = ui.theme === 'Original' ? 'Monochrome' : 'Original';
  $('bTheme').textContent = `Theme: ${ui.theme}`;
  redraw();
};
$('bReset').onclick = () => {
  audio.stopPlayback();
  rec = null;
  ui.view = 'main';
  ui.popup = null;
  dispatch({ type: 'RESET' });
};
$('stage').addEventListener('pointerdown', () => $('stage').focus());

document.addEventListener('keydown', (e) => {
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (KEYMAP[k] && !e.repeat && ui.view !== 'name') {
    e.preventDefault();
    hit(KEYMAP[k]);
    return;
  }
  if (e.repeat && k !== 'ArrowUp' && k !== 'ArrowDown') return;

  if (ui.view === 'name') {
    if (k === 'Backspace') {
      e.preventDefault();
      if (ui.nameStr.length) {
        ui.nameStr = ui.nameStr.slice(0, Math.max(0, ui.nameCur - 1)) + ui.nameStr.slice(ui.nameCur);
        ui.nameCur = Math.max(0, ui.nameCur - 1);
        redraw();
      }
      return;
    }
    if (k.length === 1 && CHARS.includes(k.toUpperCase())) {
      e.preventDefault();
      const arr = ui.nameStr.padEnd(ui.nameCur + 1, ' ').split('');
      arr[ui.nameCur] = k.toUpperCase();
      ui.nameStr = arr.join('').slice(0, 16).trimEnd();
      ui.nameCur = Math.min(15, ui.nameCur + 1);
      redraw();
      return;
    }
  }
  if (k === 'Enter') { e.preventDefault(); pressOk(); }
  else if (k === 'Escape') { e.preventDefault(); pressEsc(); }
  else if (k === ' ') {
    e.preventDefault();
    if (S.status === 'ready') {
      if (audio.playing) { audio.stopPlayback(); redraw(); }
      else { const ctx = ensureCtx(); audio.startPlayback(S, ctx.currentTime + 0.1); redraw(); }
    }
  } else if (k >= '1' && k <= '6' && S.status === 'confirmBars' && S.pending) {
    const list = choicesFor(S.pending.trimmed.length, SR);
    dispatch({ type: 'SET_BARS', bars: list[Number(k) - 1] });
  } else if (k >= '1' && k <= '4' && ui.view === 'main') {
    dispatch({ type: 'SELECT', track: Number(k) - 1 });
  } else if (k === 'ArrowLeft') { e.preventDefault(); arrowDir(-1, 0, e.shiftKey || ui.shiftHeld); }
  else if (k === 'ArrowRight') { e.preventDefault(); arrowDir(1, 0, e.shiftKey || ui.shiftHeld); }
  else if (k === 'ArrowUp') { e.preventDefault(); arrowDir(0, -1, e.shiftKey || ui.shiftHeld); }
  else if (k === 'ArrowDown') { e.preventDefault(); arrowDir(0, 1, e.shiftKey || ui.shiftHeld); }
  else if (k === 'Backspace') { e.preventDefault(); dispatch({ type: 'UNDO', track: S.selected }); }
});

redraw();
fitToViewport();
