import {
  barSamples,
  choicesFor,
  fmtMs,
  MAX_TRACK_SEC,
  onsetPos,
  renderTrack,
  State,
} from './logic';

export type ThemeName = 'Original' | 'Monochrome';
export type ViewMode = 'main' | 'offset' | 'settings' | 'input' | 'project' | 'name';

export type PopupState =
  | null
  | { kind: 'replace'; track: number }
  | { kind: 'nosound' }
  | { kind: 'memfull'; track: number }
  | { kind: 'export'; repeats: number }
  | { kind: 'exporting'; pct: number };

export interface InputState {
  src: number;
  gain: number;
  mon: boolean;
  level: number;
  peakDb: number;
  clip: boolean;
  clipUntil: number;
}

export interface RecState {
  kind: 'first' | 'part';
  t0?: number;
  track?: number;
  T0?: number;
  windowStart?: number;
  windowEnd?: number;
  pre?: number;
  started?: boolean;
  label?: string | null;
}

export interface UIState {
  theme: ThemeName;
  view: ViewMode;
  popup: PopupState;
  shiftHeld: boolean;
  editingBpm: boolean;
  offsetField: 'ms' | 'bars';
  setRow: number;
  inRow: number;
  projRow: number;
  nameTitle: string;
  nameStr: string;
  nameCur: number;
  projName: string;
  dirty: boolean;
  inp: InputState;
}

export interface Palette {
  bg: string;
  topBarBg: string;
  topBarText: string;
  topBarFrame: string;
  labelActive: string;
  labelInactive: string;
  button: string;
  red: string;
  list: string;
  listSel: string;
  barActive: string;
  barInactive: string;
  waveform: string;
  waveformBg: string;
  waveformProgress: string;
  marker: string;
  orange: string;
  grey: string;
}

export const PAL: Record<ThemeName, Palette> = {
  Original: {
    bg: '#15130f',
    topBarBg: '#221f19',
    topBarText: '#d3c8ae',
    topBarFrame: '#3a342a',
    labelActive: '#e6dcc3',
    labelInactive: '#91876f',
    button: '#211e18',
    red: '#c4452e',
    list: '#1c1a15',
    listSel: '#332e25',
    barActive: '#93a66a',
    barInactive: '#2c2922',
    waveform: '#8d826c',
    waveformBg: '#191712',
    waveformProgress: '#d2a94f',
    marker: '#d8873a',
    orange: '#d8873a',
    grey: '#575043',
  },
  Monochrome: {
    bg: '#000000',
    topBarBg: '#000000',
    topBarText: '#ffffff',
    topBarFrame: '#7f7f7f',
    labelActive: '#ffffff',
    labelInactive: '#8a8a8a',
    button: '#000000',
    red: '#ffffff',
    list: '#000000',
    listSel: '#262626',
    barActive: '#ffffff',
    barInactive: '#3a3a3a',
    waveform: '#7f7f7f',
    waveformBg: '#000000',
    waveformProgress: '#ffffff',
    marker: '#ffffff',
    orange: '#ffffff',
    grey: '#4a4a4a',
  },
};

export const PROJECTS: Array<[string, string, string, string, string]> = [
  ['Basement 04', 'open', '3 Parts', '4 bars · 115.0 BPM', 'Today'],
  ['Basement 03', '', '4 Parts', '8 bars · 92.0 BPM', 'Yesterday'],
  ['Dusk Riff', '', '2 Parts', '2 bars · 76.5 BPM', '12 Nov'],
  ['Sunday Bass', '', '1 Part', '4 bars · 101.0 BPM', '9 Nov'],
];

export const CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. '.split('');

export interface LiveRefs {
  playheadEl: HTMLElement | null;
  posValEl: HTMLElement | null;
  masterFillEl: HTMLElement | null;
  trackMeterEls: HTMLElement[];
  inputFillEl: HTMLElement | null;
  armedMsgEl: HTMLElement | null;
  bottomHandlers: Array<(() => void) | null>;
}

export interface ScreenCallbacks {
  onSelectTrack: (i: number) => void;
  onMuteTrack: (i: number) => void;
  onSetSetting: (key: keyof State['settings'], val: State['settings'][keyof State['settings']]) => void;
  onSetBars: (b: number) => void;
  onCancelBars: () => void;
  onConfirmBars: () => void;
  onToggleBpmEdit: () => void;
  onNudge: (unit: 'ms' | 'bars' | '16th', dir: number) => void;
  onSetOffset: (offset: number) => void;
  onResetOffset: () => void;
  onUndo: () => void;
  onRecord: () => void;
  onStopFirstTake: () => void;
  onFinishPart: () => void;
  onCancelRec: () => void;
  onStopPlayback: () => void;
  onStartPartArm: (track: number) => void;
  onStartExport: () => void;
  onNewProject: () => void;
  onRender: () => void;
  onFocusStage: () => void;
}

const el = <K extends keyof HTMLElementTagNameMap>(
  tag: K,
  style: string,
  text: string | null,
  parent: HTMLElement
): HTMLElementTagNameMap[K] => {
  const d = document.createElement(tag);
  if (style) d.style.cssText = style;
  if (text != null) d.textContent = text;
  parent.appendChild(d);
  return d;
};

export function fmtPos(s: State, sec: number): string {
  if (!s.loopLen || !s.bpm) return '1.1.00';
  const beatSec = 60 / s.bpm;
  const totalBeats = sec / beatSec;
  const bar = Math.floor(totalBeats / 4) + 1;
  const beat = Math.floor(totalBeats % 4) + 1;
  const sx = Math.floor((totalBeats * 4) % 4) * 25;
  return `${bar}.${beat}.${String(sx).padStart(2, '0')}`;
}

function drawLaneWave(canvas: HTMLCanvasElement, c: Palette, s: State, i: number, H: number, splitPx: number): void {
  canvas.width = 600;
  canvas.height = H;
  const g = canvas.getContext('2d');
  if (!g) return;
  g.fillStyle = c.waveformBg;
  g.fillRect(0, 0, 600, H);
  const t = s.tracks[i];
  const p = t.part;
  if (!p || !s.loopLen) return;
  const buf = renderTrack(s, i);
  let pk = 0.0001;
  for (let k = 0; k < buf.length; k += 7) pk = Math.max(pk, Math.abs(buf[k]));
  const cy = H / 2;
  const a = (H - 10) / 2;
  for (let x = 0; x < 600; x++) {
    const i0 = Math.floor((x / 600) * buf.length);
    const i1 = Math.max(i0 + 1, Math.floor(((x + 1) / 600) * buf.length));
    let mn = 0;
    let mx = 0;
    for (let k = i0; k < i1; k += 3) {
      const v = buf[k];
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    const y1 = Math.round(cy - (mx / pk) * a);
    const y2 = Math.round(cy - (mn / pk) * a) + 1;
    g.fillStyle = t.mute ? c.grey : x < splitPx ? c.waveformProgress : c.waveform;
    g.fillRect(x, y1, 1, Math.max(1, y2 - y1));
  }
}

export function renderScreen(
  scr: HTMLElement,
  hwFnRow: HTMLElement,
  s: State,
  ui: UIState,
  rec: RecState | null,
  playing: boolean,
  epoch: number,
  now: number,
  meterVals: number[],
  cb: ScreenCallbacks
): LiveRefs {
  const c = PAL[ui.theme];
  scr.innerHTML = '';
  scr.style.background = c.bg;

  const refs: LiveRefs = {
    playheadEl: null,
    posValEl: null,
    masterFillEl: null,
    trackMeterEls: [],
    inputFillEl: null,
    armedMsgEl: null,
    bottomHandlers: [],
  };

  const renderBottom = (
    items: Array<string | { t: string; on?: boolean; red?: boolean }>,
    handlers: Array<(() => void) | null>
  ): void => {
    refs.bottomHandlers = handlers;
    el('div', `position:absolute;left:0;top:440px;width:800px;height:40px;background:${c.topBarFrame};`, null, scr);
    const hwBtns = hwFnRow.children;
    items.forEach((it, i) => {
      const o = typeof it === 'string' ? { t: it } : it;
      const w = i === 7 ? 100 : 99;
      const bg = o.on ? c.listSel : c.button;
      const line = o.on ? c.labelActive : c.button;
      const fg = o.red ? c.red : o.t ? c.labelActive : c.labelInactive;
      const wt = o.on || o.red ? '700' : '400';
      const b = el(
        'div',
        `position:absolute;left:${i * 100}px;top:441px;width:${w}px;height:39px;background:${bg};color:${fg};font-size:14px;font-weight:${wt};line-height:39px;text-align:center;white-space:nowrap;cursor:${o.t ? 'pointer' : 'default'};`,
        o.t || '',
        scr
      );
      el('div', `position:absolute;left:0;top:0;width:${w}px;height:2px;background:${line};`, null, b);
      if (o.t && handlers[i]) {
        b.onclick = (e) => {
          e.stopPropagation();
          cb.onFocusStage();
          handlers[i]?.();
        };
      }
      const hw = hwBtns[i] as HTMLElement | undefined;
      if (hw) {
        hw.textContent = o.t || String(i + 1);
        hw.className = 'hw-btn' + (o.red ? ' rec-led' : o.on ? ' on' : '');
      }
    });
  };

  const renderTopBar = (
    title: string,
    fields: Array<{ label: string; value: string; w: number; active?: boolean; onClick?: () => void }>,
    boxes: Array<{ glyph?: string; text: string; fg?: string; glyphColor?: string }>
  ): void => {
    el('div', `position:absolute;left:0;top:0;width:800px;height:36px;background:${c.topBarBg};`, null, scr);
    el('div', `position:absolute;left:0;top:35px;width:800px;height:1px;background:${c.topBarFrame};`, null, scr);
    el('div', `position:absolute;left:16px;top:8px;font-size:17px;font-weight:700;line-height:20px;color:${c.topBarText};white-space:nowrap;`, title, scr);
    const fw = el('div', 'position:absolute;left:176px;top:0;height:36px;display:flex;', null, scr);
    fields.forEach((f) => {
      const d = el(
        'div',
        `position:relative;width:${f.w}px;display:flex;align-items:baseline;justify-content:center;gap:6px;padding-top:9px;background:${f.active ? c.listSel : 'transparent'};cursor:${f.onClick ? 'pointer' : 'default'};`,
        null,
        fw
      );
      if (f.active) el('div', `position:absolute;left:0;top:0;width:${f.w}px;height:2px;background:${c.labelActive};`, null, d);
      if (f.onClick) d.onclick = (e) => { e.stopPropagation(); cb.onFocusStage(); f.onClick!(); };
      el('div', `position:absolute;left:0;top:10px;width:1px;height:16px;background:${c.topBarFrame};`, null, d);
      if (f.label) el('span', `font-size:12px;line-height:14px;color:${f.active ? c.labelActive : c.labelInactive};`, f.label, d);
      const val = el('span', `font-size:17px;font-weight:700;line-height:20px;color:${c.topBarText};`, f.value, d);
      if (f.label === 'POS') refs.posValEl = val;
    });
    const bw = el('div', 'position:absolute;right:10px;top:6px;height:24px;display:flex;gap:14px;', null, scr);
    boxes.forEach((b) => {
      const d = el('div', 'height:24px;display:flex;align-items:center;gap:6px;white-space:nowrap;', null, bw);
      if (b.glyph) el('span', `font-size:17px;font-weight:700;line-height:20px;color:${b.glyphColor || c.topBarText};`, b.glyph, d);
      el('span', `font-size:14px;font-weight:700;line-height:17px;color:${b.fg || c.topBarText};`, b.text, d);
    });
  };

  const monBox = { text: ui.inp.mon ? 'MON IN' : 'MON OFF', fg: ui.inp.mon ? c.labelInactive : c.orange };
  const isArmed = Boolean(rec && now < (rec.windowStart ?? 0));
  const isRec = Boolean(rec && now >= (rec.windowStart ?? 0));
  const loopSec = s.loopLen / s.sr;

  if (ui.view === 'settings') {
    renderTopBar(ui.projName, [{ label: '', value: 'Settings', w: 200 }], [monBox]);
    const rows: Array<{
      k: keyof State['settings'] | 'ts';
      label: string;
      opts: Array<[State['settings'][keyof State['settings']] | string, string]>;
      unit?: string;
      fixed?: boolean;
      scope: string;
      help?: string;
    }> = [
      { k: 'loopMode', label: 'Loop Mode', opts: [['repeat', 'Repeat'], ['once', 'Play once']], scope: 'This project', help: 'Repeat loops forever. Play once stops at the end of one pass.' },
      { k: 'click', label: 'Click in playback', opts: [[false, 'Off'], [true, 'On']], unit: 'Count-in always plays', scope: 'This project', help: 'Plays a click during playback. Count-in before recording always clicks.' },
      { k: 'ts', label: 'Time signature', opts: [['4/4', '4/4']], unit: 'Fixed', fixed: true, scope: '' },
      { k: 'windowMs', label: 'Snap to loop start', opts: [[0, 'Off'], [30, '30'], [60, '60'], [100, '100'], [150, '150']], unit: 'ms', scope: 'This device', help: 'If the first note of a new Part lands within this window of loop start, it snaps onto loop start.' },
      { k: 'preRollMs', label: 'Keep before start', opts: [[0, 'None'], [60, '60'], [120, '120'], [250, '250'], [500, '500']], unit: 'ms', scope: 'This device', help: 'Audio captured before loop start so early first notes can be aligned cleanly.' },
      { k: 'rounding', label: 'Round tempo', opts: [['off', 'Off'], ['0.1', '0.1 BPM'], ['1', '1 BPM']], scope: 'This device', help: 'Rounds the BPM derived from your first Part by trimming or padding silence at the end.' },
    ];
    rows.forEach((r, i) => {
      const active = i === ui.setRow;
      const bg = active ? c.listSel : c.bg;
      const row = el('div', `position:absolute;left:0;top:${44 + i * 52}px;width:800px;height:52px;background:${bg};cursor:${r.fixed ? 'default' : 'pointer'};`, null, scr);
      if (!r.fixed) row.onclick = () => { ui.setRow = i; cb.onRender(); };
      if (active) el('div', `position:absolute;left:16px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', row);
      el('div', `position:absolute;left:40px;top:16px;font-size:17px;color:${r.fixed ? c.labelInactive : c.labelActive};`, r.label, row);
      let x = 290;
      const curVal = r.fixed ? '4/4' : s.settings[r.k as keyof State['settings']];
      r.opts.forEach(([val, txt]) => {
        const w = Math.max(56, Math.round(txt.length * 8) + 28);
        const sel = curVal === val;
        const o = el('div', `position:absolute;left:${x}px;top:10px;width:${w}px;height:32px;cursor:pointer;`, null, row);
        el('div', `position:absolute;left:0;top:0;width:${w}px;height:2px;background:${sel ? (r.fixed ? c.labelInactive : c.labelActive) : bg};`, null, o);
        el('div', `position:absolute;left:0;top:2px;width:${w}px;font-size:14px;font-weight:${sel && !r.fixed ? '700' : '400'};line-height:30px;text-align:center;color:${sel && !r.fixed ? c.labelActive : c.labelInactive};`, txt, o);
        if (!r.fixed) {
          o.onclick = (e) => {
            e.stopPropagation();
            ui.setRow = i;
            cb.onSetSetting(r.k as keyof State['settings'], val as State['settings'][keyof State['settings']]);
          };
        }
        x += w + 6;
      });
      if (r.unit) el('div', `position:absolute;left:${x + 6}px;top:19px;font-size:12px;color:${c.labelInactive};`, r.unit, row);
      if (r.scope) el('div', `position:absolute;right:16px;top:19px;font-size:12px;color:${c.labelInactive};`, r.scope, row);
      el('div', `position:absolute;left:40px;top:51px;width:744px;height:1px;background:${c.barInactive};`, null, row);
    });
    const hr = rows[ui.setRow];
    el('div', `position:absolute;left:40px;top:370px;font-size:14px;font-weight:700;color:${c.labelActive};`, `${hr.label} · ${hr.scope}`, scr);
    el('div', `position:absolute;left:40px;top:392px;width:720px;font-size:14px;line-height:19px;color:${c.labelInactive};`, hr.help || '', scr);
    renderBottom(['Reset', '', '', '', '', '', '', 'Close'], [
      () => {
        const defs: Partial<Record<keyof State['settings'], State['settings'][keyof State['settings']]>> = {
          loopMode: 'repeat', click: false, windowMs: 60, preRollMs: 120, rounding: '0.1',
        };
        const k = rows[ui.setRow].k as keyof State['settings'];
        if (defs[k] !== undefined) cb.onSetSetting(k, defs[k]!);
      },
      null, null, null, null, null, null,
      () => { ui.view = 'main'; cb.onRender(); },
    ]);
  } else if (ui.view === 'input') {
    renderTopBar(ui.projName, [{ label: '', value: 'Input', w: 200 }], [monBox]);
    const maxG = [22, 25, 44][ui.inp.src];
    const srcRow = el('div', `position:absolute;left:0;top:44px;width:800px;height:52px;background:${ui.inRow === 0 ? c.listSel : c.bg};cursor:pointer;`, null, scr);
    srcRow.onclick = () => { ui.inRow = 0; cb.onRender(); };
    if (ui.inRow === 0) el('div', `position:absolute;left:16px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', srcRow);
    el('div', `position:absolute;left:40px;top:16px;font-size:17px;color:${c.labelActive};`, 'Source', srcRow);
    let sx = 290;
    ['Line', 'Mic low gain', 'Mic high gain'].forEach((t, j) => {
      const w = Math.max(56, Math.round(t.length * 8) + 28);
      const sel = ui.inp.src === j;
      const o = el('div', `position:absolute;left:${sx}px;top:10px;width:${w}px;height:32px;cursor:pointer;`, null, srcRow);
      el('div', `position:absolute;left:0;top:0;width:${w}px;height:2px;background:${sel ? c.labelActive : ui.inRow === 0 ? c.listSel : c.bg};`, null, o);
      el('div', `position:absolute;left:0;top:2px;width:${w}px;font-size:14px;font-weight:${sel ? '700' : '400'};line-height:30px;text-align:center;color:${sel ? c.labelActive : c.labelInactive};`, t, o);
      o.onclick = (e) => {
        e.stopPropagation();
        ui.inRow = 0;
        ui.inp.src = j;
        ui.inp.mon = j === 0;
        ui.inp.gain = Math.min(ui.inp.gain, [22, 25, 44][j]);
        cb.onRender();
      };
      sx += w + 6;
    });
    el('div', `position:absolute;left:40px;top:51px;width:744px;height:1px;background:${c.barInactive};`, null, srcRow);

    const gRow = el('div', `position:absolute;left:0;top:96px;width:800px;height:52px;background:${ui.inRow === 1 ? c.listSel : c.bg};cursor:pointer;`, null, scr);
    gRow.onclick = () => { ui.inRow = 1; cb.onRender(); };
    if (ui.inRow === 1) el('div', `position:absolute;left:16px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', gRow);
    el('div', `position:absolute;left:40px;top:16px;font-size:17px;color:${c.labelActive};`, 'Gain', gRow);
    el('div', `position:absolute;left:92px;top:19px;font-size:12px;color:${c.labelInactive};`, `0 to +${maxG} dB`, gRow);
    const bw = Math.round((340 * ui.inp.gain) / maxG);
    el('div', `position:absolute;left:290px;top:23px;width:340px;height:6px;background:${c.barInactive};`, null, gRow);
    el('div', `position:absolute;left:290px;top:23px;width:${bw}px;height:6px;background:${c.labelActive};`, null, gRow);
    el('div', `position:absolute;left:${290 + bw - 1}px;top:17px;width:2px;height:18px;background:${c.labelActive};`, null, gRow);
    el('div', `position:absolute;left:650px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, `+${ui.inp.gain.toFixed(1)} dB`, gRow);
    el('div', `position:absolute;left:40px;top:51px;width:744px;height:1px;background:${c.barInactive};`, null, gRow);

    const mRow = el('div', `position:absolute;left:0;top:148px;width:800px;height:52px;background:${ui.inRow === 2 ? c.listSel : c.bg};cursor:pointer;`, null, scr);
    mRow.onclick = () => { ui.inRow = 2; cb.onRender(); };
    if (ui.inRow === 2) el('div', `position:absolute;left:16px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', mRow);
    el('div', `position:absolute;left:40px;top:16px;font-size:17px;color:${c.labelActive};`, 'Monitoring', mRow);
    ['Off', 'On'].forEach((t, j) => {
      const sel = (ui.inp.mon ? 1 : 0) === j;
      const o = el('div', `position:absolute;left:${290 + j * 62}px;top:10px;width:56px;height:32px;cursor:pointer;`, null, mRow);
      el('div', `position:absolute;left:0;top:0;width:56px;height:2px;background:${sel ? c.labelActive : ui.inRow === 2 ? c.listSel : c.bg};`, null, o);
      el('div', `position:absolute;left:0;top:2px;width:56px;font-size:14px;font-weight:${sel ? '700' : '400'};line-height:30px;text-align:center;color:${sel ? c.labelActive : c.labelInactive};`, t, o);
      o.onclick = (e) => { e.stopPropagation(); ui.inRow = 2; ui.inp.mon = j === 1; cb.onRender(); };
    });
    el('div', `position:absolute;left:40px;top:51px;width:744px;height:1px;background:${c.barInactive};`, null, mRow);

    const dbX = (db: number) => Math.round(40 + ((Math.max(-48, Math.min(0, db)) + 48) / 48) * 720);
    el('div', `position:absolute;left:40px;top:226px;font-size:12px;color:${c.labelInactive};`, 'Input level', scr);
    const pkBox = el('div', 'position:absolute;right:40px;top:222px;display:flex;align-items:baseline;gap:8px;', null, scr);
    el('span', `font-size:14px;font-weight:700;padding:0 6px;color:${c.bg};background:${ui.inp.clip ? c.orange : c.bg};`, 'CLIP', pkBox);
    el('span', `font-size:12px;color:${c.labelInactive};`, 'Peak', pkBox);
    el('span', `font-size:14px;font-weight:700;color:${c.labelActive};`, `${Math.round(ui.inp.peakDb)} dB`, pkBox);
    el('div', `position:absolute;left:40px;top:246px;width:720px;height:18px;background:${c.barInactive};`, null, scr);
    refs.inputFillEl = el('div', `position:absolute;left:40px;top:246px;width:${Math.max(0, dbX(ui.inp.level) - 40)}px;height:18px;background:${c.barActive};`, null, scr);
    el('div', `position:absolute;left:${dbX(ui.inp.peakDb) - 1}px;top:246px;width:2px;height:18px;background:${c.labelActive};`, null, scr);
    el('div', `position:absolute;left:490px;top:242px;width:1px;height:26px;background:${c.labelInactive};`, null, scr);
    el('div', `position:absolute;left:670px;top:242px;width:1px;height:26px;background:${c.labelInactive};`, null, scr);
    el('div', `position:absolute;left:490px;top:284px;width:181px;font-size:12px;text-align:center;color:${c.labelInactive};`, 'aim here', scr);
    [-48, -36, -24, -18, -12, -6, 0].forEach((d) =>
      el('div', `position:absolute;left:${d === -48 ? 40 : d === 0 ? 752 : dbX(d) - 9}px;top:270px;font-size:12px;color:${c.labelInactive};`, String(d).replace('-', '−'), scr)
    );
    el('div', `position:absolute;left:0;top:300px;width:800px;height:1px;background:${c.topBarFrame};`, null, scr);
    el('div', `position:absolute;left:40px;top:318px;font-size:12px;color:${c.labelInactive};`, 'Feedback protection', scr);
    el('div', `position:absolute;left:40px;top:341px;width:14px;height:14px;border:1px solid ${ui.inp.mon ? c.barActive : c.labelInactive};background:${ui.inp.mon ? c.barActive : c.bg};`, null, scr);
    el('div', `position:absolute;left:64px;top:338px;font-size:17px;font-weight:700;color:${c.labelActive};`, ui.inp.mon ? 'On' : 'Off', scr);
    el('div', `position:absolute;left:64px;top:364px;width:340px;font-size:14px;line-height:19px;color:${c.labelInactive};`, ui.inp.mon ? 'Watching the monitor for feedback. Nothing is being turned down.' : 'Monitoring is off, so nothing can feed back. Protection switches on with monitoring.', scr);
    el('div', `position:absolute;left:470px;top:318px;font-size:12px;color:${c.labelInactive};`, 'Monitor level reduced by', scr);
    el('div', `position:absolute;left:470px;top:344px;width:200px;height:6px;background:${c.barInactive};`, null, scr);
    el('div', `position:absolute;left:686px;top:338px;font-size:17px;font-weight:700;color:${c.labelActive};`, '0 dB', scr);
    el('div', `position:absolute;left:470px;top:364px;width:290px;font-size:12px;line-height:16px;color:${c.labelInactive};`, 'Only the monitor is turned down. The recording is not affected.', scr);

    renderBottom(
      [{ t: 'Line', on: ui.inp.src === 0 }, { t: 'Mic low', on: ui.inp.src === 1 }, { t: 'Mic high', on: ui.inp.src === 2 }, { t: 'Monitor', on: ui.inp.mon }, '', '', '', 'Close'],
      [
        () => { ui.inp.src = 0; ui.inp.mon = true; ui.inp.gain = Math.min(ui.inp.gain, 22); cb.onRender(); },
        () => { ui.inp.src = 1; ui.inp.mon = false; ui.inp.gain = Math.min(ui.inp.gain, 25); cb.onRender(); },
        () => { ui.inp.src = 2; ui.inp.mon = false; ui.inp.gain = Math.min(ui.inp.gain, 44); cb.onRender(); },
        () => { ui.inp.mon = !ui.inp.mon; cb.onRender(); },
        null, null, null,
        () => { ui.view = 'main'; cb.onRender(); },
      ]
    );
  } else if (ui.view === 'project') {
    renderTopBar(ui.projName, [{ label: '', value: 'Project', w: 200 }], [monBox].concat(ui.dirty ? [{ text: 'Unsaved', fg: c.orange }] : []));
    el('div', `position:absolute;left:40px;top:48px;width:16px;height:12px;border:2px solid ${c.labelInactive};`, null, scr);
    el('div', `position:absolute;left:40px;top:45px;width:7px;height:4px;background:${c.labelInactive};`, null, scr);
    el('div', `position:absolute;left:64px;top:45px;font-size:14px;color:${c.labelActive};`, 'SD / Projects', scr);
    el('div', `position:absolute;right:40px;top:47px;font-size:12px;color:${c.labelInactive};`, `${PROJECTS.length} projects · 3.2 GB free`, scr);
    ([['Name', 40], ['Parts', 340], ['Loop', 440], ['Saved', 620]] as const).forEach(([t, x]) =>
      el('div', `position:absolute;left:${x}px;top:76px;font-size:12px;color:${c.labelInactive};`, t, scr)
    );
    PROJECTS.forEach((d, i) => {
      const r = el('div', `position:absolute;left:0;top:${94 + i * 38}px;width:780px;height:38px;background:${i === ui.projRow ? c.listSel : c.bg};cursor:pointer;`, null, scr);
      r.onclick = () => { ui.projRow = i; cb.onRender(); };
      if (i === ui.projRow) el('div', `position:absolute;left:16px;top:9px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', r);
      const nm = el('div', 'position:absolute;left:40px;top:9px;display:flex;align-items:baseline;gap:8px;', null, r);
      el('span', `font-size:17px;font-weight:700;color:${c.labelActive};`, d[0], nm);
      if (d[0] === ui.projName) el('span', `font-size:12px;color:${c.labelInactive};`, 'open', nm);
      el('div', `position:absolute;left:340px;top:11px;font-size:14px;color:${c.labelActive};`, d[2], r);
      el('div', `position:absolute;left:440px;top:11px;font-size:14px;color:${c.labelActive};`, d[3], r);
      el('div', `position:absolute;left:620px;top:11px;font-size:14px;color:${c.labelInactive};`, d[4], r);
      el('div', `position:absolute;left:40px;top:37px;width:740px;height:1px;background:${c.barInactive};`, null, r);
    });
    renderBottom(['New', 'Open', 'Save', 'Save as', 'Rename', 'Export WAV', '', 'Close'], [
      cb.onNewProject,
      () => { ui.projName = PROJECTS[ui.projRow][0]; ui.dirty = false; ui.view = 'main'; cb.onRender(); },
      () => { ui.dirty = false; cb.onRender(); },
      () => { ui.nameTitle = 'Save as'; ui.nameStr = ui.projName.toUpperCase().slice(0, 16); ui.nameCur = Math.min(15, ui.nameStr.length); ui.view = 'name'; cb.onRender(); },
      () => { ui.nameTitle = 'Rename'; ui.nameStr = ui.projName.toUpperCase().slice(0, 16); ui.nameCur = Math.min(15, ui.nameStr.length); ui.view = 'name'; cb.onRender(); },
      () => { ui.popup = { kind: 'export', repeats: 1 }; cb.onRender(); },
      null,
      () => { ui.view = 'main'; cb.onRender(); },
    ]);
  } else if (ui.view === 'name') {
    renderTopBar(ui.projName, [{ label: '', value: ui.nameTitle, w: 200 }], [monBox]);
    el('div', `position:absolute;left:40px;top:60px;font-size:12px;color:${c.labelInactive};`, 'Name', scr);
    el('div', `position:absolute;right:40px;top:60px;font-size:12px;color:${c.labelInactive};`, `${ui.nameStr.length} / 16`, scr);
    for (let i = 0; i < 16; i++) {
      const cell = el('div', `position:absolute;left:${40 + i * 42}px;top:84px;width:42px;height:48px;background:${i === ui.nameCur ? c.listSel : c.bg};cursor:pointer;`, null, scr);
      cell.onclick = () => { ui.nameCur = i; cb.onRender(); };
      el('div', `position:absolute;left:0;top:0;width:42px;height:2px;background:${i === ui.nameCur ? c.labelActive : c.bg};`, null, cell);
      el('div', `position:absolute;left:0;top:46px;width:42px;height:2px;background:${c.barInactive};`, null, cell);
      el('div', `position:absolute;left:0;top:14px;width:42px;font-size:17px;font-weight:700;text-align:center;color:${c.labelActive};`, ui.nameStr[i] || '', cell);
    }
    el('div', `position:absolute;left:40px;top:166px;font-size:12px;color:${c.labelInactive};`, 'Letter at the cursor', scr);
    CHARS.forEach((ch, i) => {
      const sel = ui.nameStr[ui.nameCur] === ch;
      const sc = el('div', `position:absolute;left:${40 + i * 18}px;top:188px;width:18px;height:34px;background:${sel ? c.listSel : c.bg};cursor:pointer;`, null, scr);
      el('div', `position:absolute;left:0;top:0;width:18px;height:2px;background:${sel ? c.labelActive : c.bg};`, null, sc);
      el('div', `position:absolute;left:0;top:8px;width:18px;font-size:14px;font-weight:${sel ? '700' : '400'};text-align:center;color:${sel ? c.labelActive : c.labelInactive};`, ch, sc);
      sc.onclick = () => {
        const arr = ui.nameStr.padEnd(ui.nameCur + 1, ' ').split('');
        arr[ui.nameCur] = ch;
        ui.nameStr = arr.join('').trimEnd();
        cb.onRender();
      };
    });
    el('div', `position:absolute;left:40px;top:250px;font-size:14px;color:${c.labelInactive};`, 'Jog picks the letter. Left/Right move cursor. Or type on keyboard.', scr);
    el('div', `position:absolute;left:0;top:300px;width:800px;height:1px;background:${c.topBarFrame};`, null, scr);
    el('div', `position:absolute;left:40px;top:318px;font-size:12px;color:${c.labelInactive};`, 'Saves to', scr);
    el('div', `position:absolute;left:40px;top:338px;font-size:14px;color:${c.labelActive};`, `SD / Projects / ${ui.nameStr || 'UNTITLED'}`, scr);
    renderBottom(['Delete', 'Space', 'Clear', '', '', '', 'Cancel', { t: 'Save', on: true }], [
      () => { if (ui.nameStr.length) { ui.nameStr = ui.nameStr.slice(0, ui.nameCur) + ui.nameStr.slice(ui.nameCur + 1); ui.nameCur = Math.min(ui.nameCur, ui.nameStr.length); } cb.onRender(); },
      () => { const arr = ui.nameStr.padEnd(ui.nameCur + 1, ' ').split(''); arr[ui.nameCur] = ' '; ui.nameStr = arr.join('').slice(0, 16); ui.nameCur = Math.min(15, ui.nameCur + 1); cb.onRender(); },
      () => { ui.nameStr = ''; ui.nameCur = 0; cb.onRender(); },
      null, null, null,
      () => { ui.view = 'project'; cb.onRender(); },
      () => { ui.projName = ui.nameStr.trim() || 'UNTITLED'; ui.dirty = false; ui.view = 'project'; cb.onRender(); },
    ]);
  } else {
    const isOffset = ui.view === 'offset';
    const H = isOffset ? 72 : 93;
    const G = isOffset ? 3 : 2;
    const T0 = 58;
    const bottomY = T0 + 4 * H + 3 * G;

    const stBox = isRec
      ? { glyph: '●', text: 'REC', fg: c.red, glyphColor: c.red }
      : isArmed
        ? { glyph: '●', text: 'ARMED', fg: c.topBarText, glyphColor: c.red }
        : playing
          ? { glyph: '▶', text: 'PLAY', fg: c.topBarText }
          : { glyph: '■', text: 'STOP', fg: c.topBarText };

    renderTopBar(
      ui.projName,
      [
        {
          label: 'BPM',
          value: s.status === 'confirmBars' ? '—' : s.bpm ? s.bpm.toFixed(1) : s.presetBpm ? `${s.presetBpm}` : 'Free',
          w: 104,
          active: ui.editingBpm && s.status === 'empty',
          onClick: s.status === 'empty' ? cb.onToggleBpmEdit : undefined,
        },
        { label: 'BARS', value: s.status === 'confirmBars' ? '?' : s.bars ? String(s.bars) : '—', w: 80 },
        { label: 'POS', value: '1.1.00', w: 110 },
      ],
      [monBox, stBox]
    );

    const lm = el('div', 'position:absolute;left:16px;top:36px;height:20px;display:flex;align-items:center;gap:6px;', null, scr);
    el('span', `font-size:17px;font-weight:700;color:${c.labelInactive};`, s.settings.loopMode === 'repeat' ? '↻' : '→', lm);
    el('span', `font-size:12px;color:${c.labelInactive};`, s.settings.loopMode === 'repeat' ? 'Repeat' : 'Play once', lm);

    const rul = el('div', 'position:absolute;left:168px;top:36px;width:600px;height:20px;', null, scr);
    if (s.status !== 'confirmBars' && s.bars) {
      const totalBeats = s.bars * 4;
      for (let j = 0; j <= totalBeats; j++) {
        const x = Math.min(Math.round((j / totalBeats) * 600), 599);
        const isBar = j % 4 === 0;
        el('div', `position:absolute;left:${x}px;top:${isBar ? 0 : 14}px;width:1px;height:${isBar ? 20 : 6}px;background:${c.barInactive};`, null, rul);
      }
      for (let b = 0; b < s.bars; b++) {
        el('div', `position:absolute;left:${Math.round((b / s.bars) * 600) + 5}px;top:2px;font-size:12px;color:${c.labelInactive};`, String(b + 1), rul);
      }
    } else {
      el(
        'div',
        `position:absolute;left:8px;top:2px;font-size:12px;color:${c.labelInactive};`,
        s.status === 'confirmBars' && s.pending
          ? `Loop ${(s.pending.trimmed.length / s.sr).toFixed(2)} s · bars not set`
          : ui.editingBpm
            ? 'Turn Jog (or ◀ ▶) to set starting BPM · Press OK or Tempo when done'
            : s.presetBpm
              ? `Starting tempo ${s.presetBpm} BPM (1-bar count-in) · Press Rec to start`
              : 'Free tempo (press Tempo button 5 to set) · Press Rec to start',
        rul
      );
    }
    el('div', `position:absolute;left:0;top:18px;width:600px;height:1px;background:${c.marker};`, null, rul);
    el('div', `position:absolute;left:0;top:11px;width:2px;height:8px;background:${c.marker};`, null, rul);
    el('div', `position:absolute;left:598px;top:11px;width:2px;height:8px;background:${c.marker};`, null, rul);

    const nameTop = Math.round((H - 57) / 2);
    const splitPx = s.loopLen && playing && now >= epoch ? Math.round((((now - epoch) % loopSec) / loopSec) * 600) : 0;

    for (let i = 0; i < 4; i++) {
      const t = s.tracks[i];
      const p = t.part;
      const recTrack = rec?.kind === 'first' ? 0 : (rec?.track ?? -1);
      const trArmed = Boolean(rec && recTrack === i && now < (rec.windowStart ?? 0));
      const trRec = Boolean(rec && recTrack === i && now >= (rec.windowStart ?? 0));
      const hot = s.selected === i || trArmed || trRec;
      const lane = el('div', `position:absolute;left:0;top:${T0 + i * (H + G)}px;width:800px;height:${H}px;`, null, scr);
      const hdr = el('div', `position:absolute;left:0;top:0;width:164px;height:${H}px;background:${hot ? c.listSel : c.list};cursor:pointer;`, null, lane);
      hdr.onclick = () => { cb.onSelectTrack(i); cb.onFocusStage(); };
      el('div', `position:absolute;left:16px;top:${nameTop}px;width:16px;font-size:17px;font-weight:700;color:${hot ? c.labelActive : c.labelInactive};`, String(i + 1), hdr);
      el('div', `position:absolute;left:40px;top:${nameTop}px;font-size:17px;font-weight:700;color:${t.mute ? c.labelInactive : c.labelActive};`, t.name, hdr);
      const mBox = el('div', `position:absolute;left:134px;top:${nameTop}px;width:20px;height:20px;border:1px solid ${t.mute ? c.orange : c.barInactive};background:${t.mute ? c.orange : hot ? c.listSel : c.list};color:${t.mute ? c.bg : c.labelInactive};font-size:14px;font-weight:700;line-height:18px;text-align:center;cursor:pointer;`, 'M', hdr);
      mBox.onclick = (e) => { e.stopPropagation(); cb.onMuteTrack(i); };

      const infoLine = el('div', `position:absolute;left:40px;top:${nameTop + 26}px;height:20px;display:flex;align-items:center;white-space:nowrap;`, null, hdr);
      if (trArmed || trRec) el('span', `font-size:17px;font-weight:700;color:${c.red};margin-right:5px;`, '●', infoLine);
      const infoTxt = trRec
        ? 'Recording'
        : trArmed
          ? 'Armed'
          : p && p.offset
            ? `Offset ${fmtMs(s, p.offset)}`
            : p
              ? `Part 1 · ${s.bars} bars`
              : hot
                ? 'Input'
                : '';
      el('span', `font-size:12px;color:${c.labelInactive};`, infoTxt, infoLine);

      el('div', `position:absolute;left:40px;top:${nameTop + 54}px;width:114px;height:3px;background:${c.barInactive};`, null, hdr);
      const mFill = el('div', `position:absolute;left:40px;top:${nameTop + 54}px;width:${Math.round(114 * meterVals[i])}px;height:3px;background:${trRec ? c.red : c.barActive};`, null, hdr);
      refs.trackMeterEls.push(mFill);
      el('div', `position:absolute;left:137px;top:${nameTop + 52}px;width:1px;height:7px;background:${c.labelInactive};`, null, hdr);

      const wCan = el('canvas', `position:absolute;left:168px;top:0;width:600px;height:${H}px;`, null, lane);
      drawLaneWave(wCan, c, s, i, H, splitPx);

      if (trRec || trArmed || !p) {
        const activeBpm = s.bpm || s.presetBpm || 120;
        const beatsLeft = trArmed ? Math.min(4, Math.max(1, Math.ceil(((rec?.T0 ?? 0) - now) / (60 / activeBpm)))) : 0;
        const msg = trRec
          ? rec?.kind === 'first'
            ? 'Recording first take · press Rec to stop'
            : 'Recording · stops at Loop end'
          : trArmed
            ? (rec?.T0 ?? 0) - now <= (4 * 60) / activeBpm + 0.15
              ? `Count-in ${beatsLeft}`
              : `Waiting for loop start · ${Math.max(0, (rec?.T0 ?? 0) - now).toFixed(1)} s`
            : 'No Part';
        const mel = el('div', `position:absolute;left:184px;top:${Math.round(H / 2 - 9)}px;font-size:${trArmed ? 17 : 14}px;font-weight:${trArmed ? 700 : 400};color:${trArmed ? c.labelActive : c.labelInactive};`, msg, lane);
        if (trArmed) refs.armedMsgEl = mel;
      }
      if (p && p.reason && !trRec && !trArmed && !isOffset) {
        el('div', `position:absolute;right:40px;top:4px;font-size:12px;color:${p.aligned && i > 0 ? c.barActive : c.labelInactive};`, p.reason, lane);
      }
      if (trArmed || trRec) {
        el('div', `position:absolute;left:168px;top:0;width:2px;height:${H}px;background:${c.red};`, null, lane);
      } else if (p && s.loopLen) {
        const ox = 168 + Math.round((onsetPos(s, i) / s.loopLen) * 600);
        el('div', `position:absolute;left:${ox}px;top:0;width:2px;height:${H}px;background:${c.marker};`, null, lane);
      }
    }

    el('div', `position:absolute;left:782px;top:${T0}px;width:6px;height:${bottomY - T0}px;background:${c.barInactive};`, null, scr);
    refs.masterFillEl = el('div', `position:absolute;left:782px;top:${bottomY}px;width:6px;height:0px;background:${c.barActive};`, null, scr);

    if (s.status !== 'confirmBars') {
      const px = 168 + splitPx;
      refs.playheadEl = el('div', `position:absolute;left:${px}px;top:36px;width:1px;height:${bottomY - 36}px;background:${c.labelActive};pointer-events:none;`, null, scr);
      el('div', `position:absolute;left:-3px;top:0;width:7px;height:5px;background:${c.labelActive};`, null, refs.playheadEl);
    }

    if (isOffset) {
      const p = s.tracks[s.selected].part;
      const bs = barSamples(s);
      const band = el('div', `position:absolute;left:0;top:358px;width:800px;height:78px;background:${c.list};`, null, scr);
      el('div', `position:absolute;left:0;top:0;width:800px;height:1px;background:${c.topBarFrame};`, null, band);
      el('div', `position:absolute;left:16px;top:12px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'Offset', band);
      el('div', `position:absolute;left:16px;top:38px;font-size:12px;color:${c.labelInactive};`, `Track ${s.selected + 1} · ${s.tracks[s.selected].name} · Part 1`, band);
      el('div', `position:absolute;left:16px;top:55px;font-size:12px;color:${c.labelInactive};`, `Loop ${Math.round(loopSec * 1000)} ms · ${s.bars} bars`, band);
      el('div', `position:absolute;left:200px;top:29px;font-size:17px;font-weight:700;color:${c.labelActive};`, '→', band);

      const fTime = el('div', `position:absolute;left:224px;top:11px;width:196px;height:56px;background:${ui.offsetField === 'ms' ? c.listSel : c.list};cursor:pointer;`, null, band);
      fTime.onclick = () => { ui.offsetField = 'ms'; cb.onRender(); };
      el('div', `position:absolute;left:0;top:0;width:196px;height:${ui.offsetField === 'ms' ? 2 : 1}px;background:${ui.offsetField === 'ms' ? c.labelActive : c.barInactive};`, null, fTime);
      el('div', `position:absolute;left:12px;top:10px;font-size:12px;color:${c.labelInactive};`, 'Time', fTime);
      if (ui.offsetField === 'ms') el('div', `position:absolute;right:12px;top:9px;font-size:14px;font-weight:700;color:${c.labelActive};`, 'JOG', fTime);
      el('div', `position:absolute;left:12px;top:29px;font-size:17px;font-weight:700;color:${c.labelActive};`, p ? fmtMs(s, p.offset) : '0 ms', fTime);
      el('div', `position:absolute;right:12px;top:32px;font-size:12px;color:${c.labelInactive};`, '±1 ms', fTime);

      const fBars = el('div', `position:absolute;left:436px;top:11px;width:196px;height:56px;background:${ui.offsetField === 'bars' ? c.listSel : c.list};cursor:pointer;`, null, band);
      fBars.onclick = () => { ui.offsetField = 'bars'; cb.onRender(); };
      el('div', `position:absolute;left:0;top:0;width:196px;height:${ui.offsetField === 'bars' ? 2 : 1}px;background:${ui.offsetField === 'bars' ? c.labelActive : c.barInactive};`, null, fBars);
      el('div', `position:absolute;left:12px;top:10px;font-size:12px;color:${c.labelInactive};`, 'Bars', fBars);
      if (ui.offsetField === 'bars') el('div', `position:absolute;right:12px;top:9px;font-size:14px;font-weight:700;color:${c.labelActive};`, 'JOG', fBars);
      const bFrac = p ? p.offset / bs : 0;
      el('div', `position:absolute;left:12px;top:29px;font-size:17px;font-weight:700;color:${c.labelActive};`, `${bFrac >= 0 ? '+' : ''}${bFrac.toFixed(3)}`, fBars);
      el('div', `position:absolute;right:12px;top:32px;font-size:12px;color:${c.labelInactive};`, `${(bFrac * 4).toFixed(1)} beats`, fBars);

      el('div', `position:absolute;left:652px;top:14px;font-size:12px;color:${c.labelInactive};`, 'Part starts at', band);
      el('div', `position:absolute;left:652px;top:31px;font-size:17px;font-weight:700;color:${c.marker};`, p ? fmtPos(s, onsetPos(s, s.selected) / s.sr) : '1.1.00', band);
      el('div', `position:absolute;left:652px;top:55px;font-size:12px;color:${c.labelInactive};`, s.settings.loopMode === 'repeat' ? 'Wraps at Loop end' : 'Loop Mode: Play once', band);

      renderBottom(
        [{ t: 'Time', on: ui.offsetField === 'ms' }, { t: 'Bars', on: ui.offsetField === 'bars' }, 'Snap 1/16', 'Zero', '− 1/16', '+ 1/16', 'Cancel', 'Done'],
        [
          () => { ui.offsetField = 'ms'; cb.onRender(); },
          () => { ui.offsetField = 'bars'; cb.onRender(); },
          () => { const p0 = s.tracks[s.selected].part; if (p0) { const step = barSamples(s) / 16; cb.onSetOffset(Math.round(p0.offset / step) * step); } },
          () => cb.onSetOffset(0),
          () => cb.onNudge('16th', -1),
          () => cb.onNudge('16th', 1),
          cb.onResetOffset,
          () => { ui.view = 'main'; cb.onRender(); },
        ]
      );
    } else if (s.status === 'confirmBars' && s.pending) {
      const len = s.pending.trimmed.length;
      const choices = choicesFor(len, s.sr);
      const pr = el('div', `position:absolute;left:0;top:228px;width:800px;height:212px;background:${c.list};`, null, scr);
      el('div', `position:absolute;left:0;top:0;width:800px;height:1px;background:${c.topBarFrame};`, null, pr);
      el('div', `position:absolute;left:16px;top:16px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'How many bars?', pr);
      el('div', `position:absolute;right:16px;top:19px;font-size:12px;color:${c.labelInactive};`, `Track 1 · Part 1 · ${(len / s.sr).toFixed(3)} s`, pr);
      const sub = el('div', 'position:absolute;left:16px;top:42px;display:flex;gap:8px;', null, pr);
      if (s.pending.memFull) el('span', `font-size:14px;font-weight:700;color:${c.orange};`, 'Memory full.', sub);
      el('span', `font-size:14px;color:${c.labelInactive};`, s.pending.memFull ? `Recording stopped at ${MAX_TRACK_SEC} s. Everything was kept.` : 'This Part sets the Loop. Its length in bars sets the tempo.', sub);
      choices.forEach((b, i) => {
        const sel = s.pending?.bars === b;
        const cell = el('div', `position:absolute;left:${i * 100}px;top:78px;width:99px;height:96px;background:${sel ? c.listSel : c.button};cursor:pointer;`, null, pr);
        cell.onclick = () => cb.onSetBars(b);
        el('div', `position:absolute;left:0;top:0;width:99px;height:2px;background:${sel ? c.labelActive : c.button};`, null, cell);
        const hd = el('div', 'position:absolute;left:16px;top:12px;display:flex;align-items:baseline;gap:5px;', null, cell);
        el('span', `font-size:17px;font-weight:700;color:${c.labelActive};`, String(b), hd);
        el('span', `font-size:12px;color:${c.labelInactive};`, b === 1 ? 'bar' : 'bars', hd);
        el('div', `position:absolute;left:16px;top:52px;font-size:17px;color:${c.labelActive};`, (b * 4 * 60 / (len / s.sr)).toFixed(1), cell);
        el('div', `position:absolute;left:16px;top:74px;font-size:12px;color:${c.labelInactive};`, 'BPM', cell);
      });
      el('div', `position:absolute;left:616px;top:90px;font-size:12px;color:${c.labelInactive};`, 'Result', pr);
      el('div', `position:absolute;left:616px;top:108px;font-size:17px;font-weight:700;color:${c.labelActive};`, `${(s.pending.bars * 4 * 60 / (len / s.sr)).toFixed(1)} BPM`, pr);
      el('div', `position:absolute;left:616px;top:134px;font-size:12px;color:${c.labelInactive};`, `${(len / s.sr / s.pending.bars).toFixed(3)} s per bar · 4/4`, pr);
      el('div', `position:absolute;left:616px;top:152px;font-size:12px;color:${c.labelInactive};`, `Loop Mode: ${s.settings.loopMode === 'repeat' ? 'Repeat' : 'Play once'}`, pr);
      el('div', `position:absolute;left:16px;top:188px;font-size:12px;color:${c.labelInactive};`, 'Jog or buttons 1–6 to choose, OK to confirm', pr);

      renderBottom(
        choices.map((b) => ({ t: String(b), on: s.pending?.bars === b })).concat(['Cancel', { t: 'OK', on: true }] as never),
        [...choices.map((b) => () => cb.onSetBars(b)), cb.onCancelBars, cb.onConfirmBars]
      );
    } else if (isArmed) {
      renderBottom(['Disarm', 'Mute', { t: 'Monitor', on: ui.inp.mon }, '', '', '', '', 'Stop'], [
        cb.onCancelRec,
        () => cb.onMuteTrack(s.selected),
        () => { ui.inp.mon = !ui.inp.mon; cb.onRender(); },
        null, null, null, null,
        () => { cb.onCancelRec(); cb.onStopPlayback(); },
      ]);
    } else if (isRec) {
      renderBottom([{ t: 'Stop Rec', red: true }, 'Mute', { t: 'Monitor', on: ui.inp.mon }, '', '', '', '', 'Stop'], [
        () => { if (rec?.kind === 'first') cb.onStopFirstTake(); else if (rec?.kind === 'part') cb.onFinishPart(); },
        () => cb.onMuteTrack(s.selected),
        () => { ui.inp.mon = !ui.inp.mon; cb.onRender(); },
        null, null, null, null,
        () => { cb.onCancelRec(); cb.onStopPlayback(); },
      ]);
    } else {
      renderBottom(
        [
          s.status === 'empty' ? 'Rec' : 'Arm',
          s.status === 'empty' ? '' : 'Mute',
          s.tracks[s.selected].prev ? 'Undo' : '',
          s.tracks[s.selected].part ? 'Offset' : '',
          s.status === 'empty' ? { t: 'Tempo', on: ui.editingBpm } : '',
          'Input',
          'Settings',
          'Project',
        ],
        [
          cb.onRecord,
          s.status === 'empty' ? null : () => cb.onMuteTrack(s.selected),
          cb.onUndo,
          () => { if (s.tracks[s.selected].part) { ui.view = 'offset'; cb.onRender(); } },
          s.status === 'empty' ? cb.onToggleBpmEdit : null,
          () => { ui.view = 'input'; cb.onRender(); },
          () => { ui.view = 'settings'; cb.onRender(); },
          () => { ui.view = 'project'; cb.onRender(); },
        ]
      );
    }
  }

  if (ui.popup) {
    const p = ui.popup;
    const box = el('div', `position:absolute;left:150px;top:138px;width:500px;height:200px;border:1px solid ${c.topBarFrame};background:${c.list};`, null, scr);
    el('div', `position:absolute;left:-1px;top:-1px;width:500px;height:2px;background:${c.labelActive};`, null, box);
    if (p.kind === 'replace') {
      const t = s.tracks[p.track];
      el('div', `position:absolute;left:24px;top:22px;font-size:17px;font-weight:700;color:${c.labelActive};`, '● Replace Part?', box);
      el('div', `position:absolute;left:24px;top:56px;width:450px;font-size:14px;line-height:20px;color:${c.labelActive};`, `Track ${p.track + 1} already has a Part. Recording replaces it after the count-in.`, box);
      el('div', `position:absolute;left:24px;top:116px;font-size:12px;color:${c.labelInactive};`, `${t.name} · Part 1 · ${s.bars} bars`, box);
      renderBottom(['', '', '', '', '', '', 'Cancel', { t: 'Replace', red: true }], [
        null, null, null, null, null, null,
        () => { ui.popup = null; cb.onRender(); },
        () => { const tr = p.track; ui.popup = null; cb.onStartPartArm(tr); },
      ]);
    } else if (p.kind === 'nosound') {
      el('div', `position:absolute;left:24px;top:22px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'No sound in this take', box);
      el('div', `position:absolute;left:24px;top:56px;width:450px;font-size:14px;line-height:20px;color:${c.labelActive};`, 'Nothing was kept. Play the pads or keys while recording.', box);
      el('div', `position:absolute;left:24px;top:116px;font-size:12px;color:${c.labelInactive};`, 'Check the source and gain in Input.', box);
      renderBottom(['', '', '', '', '', '', 'Input', 'OK'], [
        null, null, null, null, null, null,
        () => { ui.popup = null; ui.view = 'input'; cb.onRender(); },
        () => { ui.popup = null; cb.onRender(); },
      ]);
    } else if (p.kind === 'memfull') {
      el('div', `position:absolute;left:24px;top:22px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'Not enough memory', box);
      el('div', `position:absolute;left:24px;top:56px;width:450px;font-size:14px;line-height:20px;color:${c.labelActive};`, `Track ${p.track + 1} was not armed. A new Part needs room for one full Loop (${loopSec.toFixed(1)} s).`, box);
      el('div', `position:absolute;left:24px;top:116px;font-size:12px;color:${c.labelInactive};`, 'Remove a Part, or save and start a new project.', box);
      renderBottom(['', '', '', '', '', '', 'Project', 'OK'], [
        null, null, null, null, null, null,
        () => { ui.popup = null; ui.view = 'project'; cb.onRender(); },
        () => { ui.popup = null; cb.onRender(); },
      ]);
    } else if (p.kind === 'export') {
      el('div', `position:absolute;left:24px;top:22px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'Export WAV', box);
      el('div', `position:absolute;left:24px;top:56px;width:450px;font-size:14px;color:${c.labelActive};`, `${ui.projName}.wav to SD / Exports`, box);
      const fld = el('div', `position:absolute;left:24px;top:90px;width:450px;height:52px;background:${c.listSel};`, null, box);
      el('div', `position:absolute;left:0;top:0;width:450px;height:2px;background:${c.labelActive};`, null, fld);
      el('div', `position:absolute;left:12px;top:17px;font-size:14px;color:${c.labelInactive};`, 'Repeats', fld);
      el('div', `position:absolute;left:120px;top:15px;font-size:17px;font-weight:700;color:${c.labelActive};`, String(p.repeats), fld);
      el('div', `position:absolute;left:180px;top:18px;font-size:12px;color:${c.labelInactive};`, `× Loop · ${(loopSec * p.repeats).toFixed(1)} s file`, fld);
      el('div', `position:absolute;right:12px;top:16px;font-size:14px;font-weight:700;color:${c.labelActive};`, 'JOG', fld);
      renderBottom(['', '', '', '', '', '', 'Cancel', { t: 'Export', on: true }], [
        null, null, null, null, null, null,
        () => { ui.popup = null; cb.onRender(); },
        cb.onStartExport,
      ]);
    } else if (p.kind === 'exporting') {
      el('div', `position:absolute;left:24px;top:22px;font-size:17px;font-weight:700;color:${c.labelActive};`, 'Exporting mix', box);
      el('div', `position:absolute;left:24px;top:56px;width:450px;font-size:14px;color:${c.labelActive};`, `${ui.projName}.wav · to SD / Exports`, box);
      el('div', `position:absolute;left:24px;top:104px;width:390px;height:6px;background:${c.barInactive};`, null, box);
      el('div', `position:absolute;left:24px;top:104px;width:${Math.round(390 * p.pct)}px;height:6px;background:${c.labelActive};`, null, box);
      el('div', `position:absolute;right:24px;top:98px;font-size:14px;font-weight:700;color:${c.labelActive};`, `${Math.round(p.pct * 100)}%`, box);
      renderBottom(['', '', '', '', '', '', '', 'Cancel'], [
        null, null, null, null, null, null, null,
        () => { ui.popup = null; cb.onRender(); },
      ]);
    }
  }

  return refs;
}

