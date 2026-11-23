export const NT = 4;
export const BPB = 4;
export const MAX_TRACK_SEC = 30.0;
export const BAR_CHOICES = [1, 2, 3, 4, 6, 8] as const;
export const LONG_BAR_CHOICES = [12, 16, 20, 24, 28, 32] as const;

export type LoopMode = 'repeat' | 'once';
export type TempoRounding = 'off' | '0.1' | '1';

export interface Part {
  raw: Float32Array;
  preRoll: number;
  onsetRel: number;
  aligned: boolean;
  alignShift: number;
  offset: number;
  reason: string;
}

export interface Track {
  name: string;
  part: Part | null;
  prev: Part | null;
  mute: boolean;
}

export interface Settings {
  loopMode: LoopMode;
  windowMs: number;
  preRollMs: number;
  rounding: TempoRounding;
  click: boolean;
}

export interface PendingTake {
  trimmed: Float32Array;
  bars: number;
  memFull: boolean;
}

export interface State {
  sr: number;
  status: 'empty' | 'confirmBars' | 'ready';
  presetBpm: number | null;
  loopLen: number;
  bars: number;
  bpm: number;
  pending: PendingTake | null;
  selected: number;
  tracks: Track[];
  settings: Settings;
  log: string[];
}

export type Action =
  | { type: 'RESET' }
  | { type: 'SELECT'; track: number }
  | { type: 'SET_PRESET_BPM'; bpm: number | null }
  | { type: 'SET_SETTING'; key: keyof Settings; value: Settings[keyof Settings] }
  | { type: 'MUTE'; track: number }
  | { type: 'FIRST_TAKE'; samples: Float32Array; memFull?: boolean }
  | { type: 'FIRST_TAKE_TIMED'; raw: Float32Array; preRoll: number; memFull?: boolean }
  | { type: 'SET_BARS'; bars: number }
  | { type: 'CANCEL_BARS' }
  | { type: 'CONFIRM_BARS' }
  | { type: 'PART_TAKE'; track: number; raw: Float32Array; preRoll: number; label?: string }
  | { type: 'NUDGE'; track: number; unit: 'ms' | 'bars' | '16th'; dir: number; coarse?: boolean }
  | { type: 'SET_OFFSET'; track: number; offset: number }
  | { type: 'UNDO'; track: number };

const mod = (a: number, n: number): number => ((a % n) + n) % n;
const amp = (db: number): number => Math.pow(10, db / 20);
const resize = (buf: Float32Array, len: number): Float32Array => {
  const o = new Float32Array(len);
  o.set(buf.subarray(0, Math.min(len, buf.length)));
  return o;
};

export function detectFirstOnset(x: Float32Array, sr: number): number {
  const cell = 32;
  const n = Math.floor(x.length / cell);
  if (n < 8) return -1;
  const env = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    let m = 0;
    for (let j = i * cell; j < (i + 1) * cell; j++) {
      const a = Math.abs(x[j]);
      if (a > m) m = a;
    }
    env[i] = m;
  }
  const sorted = Array.from(env).sort((a, b) => a - b);
  const floor = sorted[Math.floor(n * 0.1)];
  const thr = Math.max(amp(-50), floor * 4);
  const conf = Math.max(1, Math.round((0.005 * sr) / cell));
  for (let i = 0; i < n - conf; i++) {
    if (env[i] < thr) continue;
    let above = 0;
    for (let k = 0; k <= conf; k++) if (env[i + k] >= thr) above++;
    if (above < Math.ceil((conf + 1) / 2)) continue;
    const s = i * cell;
    const lim = Math.max(0, s - Math.round(0.015 * sr));
    let p = s;
    while (p > lim && Math.abs(x[p]) >= thr * 0.25) p--;
    let q = p;
    while (q < s + cell && Math.abs(x[q]) < thr * 0.25) q++;
    const zc = Math.round(0.001 * sr);
    let z = q;
    for (let k = q; k > Math.max(1, q - zc); k--) {
      if ((x[k - 1] <= 0 && x[k] > 0) || (x[k - 1] >= 0 && x[k] < 0)) {
        z = k;
        break;
      }
    }
    return z;
  }
  return -1;
}

export const bpmFor = (len: number, bars: number, sr: number): number =>
  (bars * BPB * 60) / (len / sr);

export function choicesFor(len: number, sr: number): readonly number[] {
  return len / sr > 18 ? LONG_BAR_CHOICES : BAR_CHOICES;
}

export function pickBars(len: number, sr: number): number {
  const list = choicesFor(len, sr);
  let best = list[Math.floor(list.length / 2)];
  let bd = 1e9;
  for (const b of list) {
    const bpm = bpmFor(len, b, sr);
    if (bpm < 60 || bpm > 180) continue;
    const d = Math.abs(bpm - 105);
    if (d < bd) {
      bd = d;
      best = b;
    }
  }
  return best;
}

export function roundedLen(len: number, bars: number, sr: number, rounding: TempoRounding): number {
  if (rounding === 'off') return len;
  const step = rounding === '1' ? 1 : 0.1;
  const bpm = Math.round(bpmFor(len, bars, sr) / step) * step;
  return Math.round(((bars * BPB * 60) / bpm) * sr);
}

export const windowSamples = (s: State): number =>
  !s.bpm ? 0 : Math.min((s.settings.windowMs / 1000) * s.sr, (60 / s.bpm / 4 / 2) * s.sr);

export const barSamples = (s: State): number => s.loopLen / s.bars;

export const onsetPos = (s: State, t: number): number => {
  const p = s.tracks[t].part;
  return p ? mod(p.onsetRel + p.alignShift + p.offset, s.loopLen) : 0;
};

export function renderTrack(s: State, t: number): Float32Array {
  const p = s.tracks[t].part;
  const L = s.loopLen;
  const out = new Float32Array(L || 0);
  if (!p || !L) return out;
  const shift = p.alignShift + p.offset;
  const once = s.settings.loopMode === 'once';
  for (let i = 0; i < p.raw.length; i++) {
    const pos = i - p.preRoll + shift;
    // Play once: anything pushed past either end is cut off. Repeat: it wraps like a tape loop.
    if (once && (pos < 0 || pos >= L)) continue;
    out[mod(pos, L)] += p.raw[i];
  }
  return out;
}

export function init(sr: number): State {
  return {
    sr,
    status: 'empty',
    presetBpm: null,
    loopLen: 0,
    bars: 0,
    bpm: 0,
    pending: null,
    selected: 0,
    tracks: Array.from({ length: NT }, (_, i) => ({
      name: `Track ${i + 1}`,
      part: null,
      prev: null,
      mute: false,
    })),
    settings: {
      loopMode: 'repeat',
      windowMs: 60,
      preRollMs: 120,
      rounding: '0.1',
      click: false,
    },
    log: [],
  };
}

export const fmtMs = (s: State, n: number): string => {
  const ms = (n / s.sr) * 1000;
  const a = Math.abs(ms);
  return (ms < 0 ? '−' : '+') + (a >= 1000 ? `${(a / 1000).toFixed(2)} s` : `${a.toFixed(0)} ms`);
};

const say = (s: State, m: string): void => {
  s.log = [m, ...s.log].slice(0, 20);
};

const wrapHalf = (x: number, L: number): number => Math.round(mod(x + L / 2, L) - L / 2);

export function reduce(state: State, a: Action): State {
  const s: State = {
    ...state,
    tracks: state.tracks.map((t) => ({ ...t })),
    settings: { ...state.settings },
    log: state.log,
  };

  switch (a.type) {
    case 'RESET':
      return init(state.sr);
    case 'SELECT':
      s.selected = a.track;
      return s;
    case 'SET_PRESET_BPM':
      s.presetBpm = a.bpm == null ? null : Math.max(40, Math.min(200, Math.round(a.bpm)));
      return s;
    case 'SET_SETTING':
      (s.settings as unknown as Record<string, unknown>)[a.key] = a.value;
      return s;
    case 'MUTE':
      s.tracks[a.track].mute = !s.tracks[a.track].mute;
      return s;
    case 'FIRST_TAKE': {
      const onset = detectFirstOnset(a.samples, s.sr);
      if (onset < 0) {
        say(s, 'No sound in this take. Nothing was kept.');
        return s;
      }
      const trimmed = a.samples.subarray(onset);
      s.pending = { trimmed, bars: pickBars(trimmed.length, s.sr), memFull: !!a.memFull };
      s.status = 'confirmBars';
      say(
        s,
        `First take: ${(trimmed.length / s.sr).toFixed(2)} s kept (trimmed ${((onset / s.sr) * 1000).toFixed(0)} ms). Choose bar count.`
      );
      return s;
    }
    case 'FIRST_TAKE_TIMED': {
      const bpm = s.presetBpm ?? 120;
      const onset = detectFirstOnset(a.raw, s.sr);
      if (onset < 0) {
        say(s, 'No sound in this take. Nothing was kept.');
        return s;
      }
      const barSec = (BPB * 60) / bpm;
      const rawPlayedSec = Math.max(0.5, (a.raw.length - a.preRoll) / s.sr);
      const list = choicesFor(rawPlayedSec * s.sr, s.sr);
      let bars = list[0];
      let bestDiff = 1e9;
      for (const b of list) {
        const d = Math.abs(rawPlayedSec - b * barSec);
        if (d < bestDiff) {
          bestDiff = d;
          bars = b;
        }
      }
      const len = Math.min(Math.round(MAX_TRACK_SEC * s.sr), Math.round(bars * barSec * s.sr));
      const onsetRel = onset - a.preRoll;
      const win = Math.min((s.settings.windowMs / 1000) * s.sr, (60 / bpm / 4 / 2) * s.sr);
      let aligned = false;
      let shift = 0;
      let reason = `First Part · ${bpm.toFixed(1)} BPM`;
      if (win > 0 && Math.abs(onsetRel) <= win) {
        aligned = true;
        shift = -onsetRel;
        reason = `First Part · snapped ${fmtMs(s, onsetRel).replace('+', '').replace('−', '')} ${onsetRel < 0 ? 'early' : 'late'}`;
      }
      const out = new Float32Array(len);
      for (let i = 0; i < a.raw.length; i++) {
        const idx = i - a.preRoll + shift;
        if (idx >= 0 && idx < len) out[idx] += a.raw[i];
      }
      s.loopLen = len;
      s.bars = bars;
      s.bpm = bpmFor(len, bars, s.sr);
      s.status = 'ready';
      s.tracks[0].part = { raw: out, preRoll: 0, onsetRel: aligned ? 0 : Math.max(0, onsetRel), aligned, alignShift: 0, offset: 0, reason };
      s.selected = 0;
      say(s, `Loop set at ${s.bpm.toFixed(1)} BPM (${bars} bar${bars > 1 ? 's' : ''}, ${(len / s.sr).toFixed(2)} s).`);
      return s;
    }
    case 'SET_BARS':
      if (s.pending) s.pending = { ...s.pending, bars: a.bars };
      return s;
    case 'CANCEL_BARS':
      s.pending = null;
      s.status = 'empty';
      say(s, 'First take discarded.');
      return s;
    case 'CONFIRM_BARS': {
      if (!s.pending) return s;
      const bars = s.pending.bars;
      const raw0 = s.pending.trimmed;
      const len = roundedLen(raw0.length, bars, s.sr, s.settings.rounding);
      const raw = len === raw0.length ? raw0 : resize(raw0, len);
      s.loopLen = len;
      s.bars = bars;
      s.bpm = bpmFor(len, bars, s.sr);
      s.pending = null;
      s.status = 'ready';
      s.tracks[0].part = {
        raw,
        preRoll: 0,
        onsetRel: 0,
        aligned: true,
        alignShift: 0,
        offset: 0,
        reason: 'First Part · sets Loop',
      };
      s.selected = 0;
      say(s, `Loop set: ${bars} bar${bars > 1 ? 's' : ''} (${(len / s.sr).toFixed(2)} s, ${s.bpm.toFixed(1)} BPM).`);
      return s;
    }
    case 'PART_TAKE': {
      if (s.status !== 'ready') return s;
      const t = a.track;
      const onset = detectFirstOnset(a.raw, s.sr);
      if (onset < 0) {
        say(s, `Track ${t + 1}: no sound in this take. Nothing was kept.`);
        return s;
      }
      const win = windowSamples(s);
      const onsetRel = onset - a.preRoll;
      let aligned = false;
      let shift = 0;
      let reason: string;
      if (win <= 0) {
        reason = 'Snap off · kept as played';
      } else if (Math.abs(onsetRel) <= win) {
        aligned = true;
        shift = -onsetRel;
        reason = `Snapped ${fmtMs(s, onsetRel).replace('+', '').replace('−', '')} ${onsetRel < 0 ? 'early' : 'late'}`;
      } else {
        reason = `Left in place (${fmtMs(s, onsetRel)})`;
      }
      s.tracks[t].prev = s.tracks[t].part;
      s.tracks[t].part = { raw: a.raw, preRoll: a.preRoll, onsetRel, aligned, alignShift: shift, offset: 0, reason };
      if (a.label) s.tracks[t].name = a.label;
      s.selected = t;
      say(s, `Track ${t + 1}: ${reason}`);
      return s;
    }
    case 'NUDGE': {
      const p = s.tracks[a.track].part;
      if (!p || !s.loopLen) return s;
      const delta =
        a.unit === 'ms'
          ? Math.round(((a.coarse ? 10 : 1) / 1000) * s.sr) * a.dir
          : a.unit === '16th'
            ? Math.round(barSamples(s) / 16) * a.dir
            : Math.round(barSamples(s) * (a.coarse ? 0.25 : 1)) * a.dir;
      s.tracks[a.track].part = { ...p, offset: wrapHalf(p.offset + delta, s.loopLen) };
      return s;
    }
    case 'SET_OFFSET': {
      const p = s.tracks[a.track].part;
      if (!p) return s;
      s.tracks[a.track].part = { ...p, offset: wrapHalf(a.offset, s.loopLen) };
      say(s, `Track ${a.track + 1}: offset ${fmtMs(s, s.tracks[a.track].part!.offset)}.`);
      return s;
    }
    case 'UNDO': {
      const t = s.tracks[a.track];
      if (!t.prev) {
        say(s, `Track ${a.track + 1}: nothing to undo.`);
        return s;
      }
      t.part = t.prev;
      t.prev = null;
      say(s, `Track ${a.track + 1}: previous Part restored.`);
      return s;
    }
  }
}

