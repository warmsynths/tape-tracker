import { describe, expect, it } from 'vitest';
import { init, reduce, onsetPos } from './logic';

const SR = 44100;

function makeTone(len: number, start: number): Float32Array {
  const buf = new Float32Array(len);
  for (let i = start; i < Math.min(len, start + 1500); i++) {
    buf[i] = 0.6 * Math.sin(i * 0.12);
  }
  return buf;
}

describe('FourTrack logic', () => {
  it('trims silence on first take and derives BPM', () => {
    let s = init(SR);
    s = reduce(s, { type: 'FIRST_TAKE', samples: makeTone(SR * 4, 2000) });
    expect(s.status).toBe('confirmBars');
    s = reduce(s, { type: 'SET_BARS', bars: 4 });
    s = reduce(s, { type: 'CONFIRM_BARS' });
    expect(s.status).toBe('ready');
    expect(s.bars).toBe(4);
    expect(s.bpm).toBeGreaterThan(200);
  });

  it('snaps a second part inside the alignment window and leaves deliberate late entry in place', () => {
    let s = init(SR);
    s = reduce(s, { type: 'FIRST_TAKE', samples: makeTone(SR * 8, 500) });
    s = reduce(s, { type: 'SET_BARS', bars: 4 });
    s = reduce(s, { type: 'CONFIRM_BARS' });

    const pre = Math.round(0.12 * SR);
    // 25 ms late -> inside 60 ms window
    const p2 = makeTone(s.loopLen + pre, pre + Math.round(0.025 * SR));
    s = reduce(s, { type: 'PART_TAKE', track: 1, raw: p2, preRoll: pre, label: 'Drums' });
    expect(s.tracks[1].part?.aligned).toBe(true);
    expect(onsetPos(s, 1)).toBe(0);

    // 500 ms late -> outside 60 ms window
    const p3 = makeTone(s.loopLen + pre, pre + Math.round(0.5 * SR));
    s = reduce(s, { type: 'PART_TAKE', track: 2, raw: p3, preRoll: pre, label: 'Bass' });
    expect(s.tracks[2].part?.aligned).toBe(false);
    expect(onsetPos(s, 2)).toBeGreaterThan(0);
  });

  it('supports preset BPM first take with count-in snap', () => {
    let s = init(SR);
    s = reduce(s, { type: 'SET_PRESET_BPM', bpm: 120 });
    expect(s.presetBpm).toBe(120);
    const pre = Math.round(0.12 * SR);
    // ~4 bars at 120 BPM = 8.0 s
    const raw = makeTone(pre + SR * 8, pre + Math.round(0.02 * SR));
    s = reduce(s, { type: 'FIRST_TAKE_TIMED', raw, preRoll: pre });
    expect(s.status).toBe('ready');
    expect(s.bars).toBe(4);
    expect(Math.round(s.bpm)).toBe(120);
    expect(s.tracks[0].part?.aligned).toBe(true);
  });

  it('supports nudge and undo', () => {
    let s = init(SR);
    s = reduce(s, { type: 'FIRST_TAKE', samples: makeTone(SR * 8, 500) });
    s = reduce(s, { type: 'SET_BARS', bars: 4 });
    s = reduce(s, { type: 'CONFIRM_BARS' });

    const pre = Math.round(0.12 * SR);
    s = reduce(s, { type: 'PART_TAKE', track: 1, raw: makeTone(s.loopLen + pre, pre + 200), preRoll: pre });
    s = reduce(s, { type: 'PART_TAKE', track: 1, raw: makeTone(s.loopLen + pre, pre + 800), preRoll: pre });
    s = reduce(s, { type: 'NUDGE', track: 1, unit: 'ms', dir: 1, coarse: true });
    expect(s.tracks[1].part?.offset).toBeGreaterThan(0);
    s = reduce(s, { type: 'UNDO', track: 1 });
    expect(s.tracks[1].part?.offset).toBe(0);
  });
});

