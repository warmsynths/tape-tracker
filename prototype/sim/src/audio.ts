import { renderTrack, State } from './logic';

export const SR = 44100;

export interface NoteSpec {
  kind: string;
  f: number;
  label: string;
}

export interface Hit {
  t: number;
  n: NoteSpec;
}

const cache: Record<string, Float32Array> = {};

function rnd(o: { s: number }): number {
  o.s = (o.s * 1664525 + 1013904223) >>> 0;
  return (o.s / 4294967296) * 2 - 1;
}

function ks(f: number, dur: number, damp: number, soft: number): Float32Array {
  const n = Math.floor(SR * dur);
  const N = Math.max(2, Math.round(SR / f));
  const ring = new Float32Array(N);
  const o = new Float32Array(n);
  const r = { s: 12345 + N };
  for (let i = 0; i < N; i++) ring[i] = rnd(r);
  for (let p = 0; p < soft; p++) for (let i = 0; i < N; i++) ring[i] = (ring[i] + ring[(i + 1) % N]) / 2;
  let pk = 0;
  for (let i = 0; i < N; i++) pk = Math.max(pk, Math.abs(ring[i]));
  let idx = 0;
  for (let i = 0; i < n; i++) {
    const a = ring[idx];
    const b = ring[(idx + 1) % N];
    ring[idx] = (a + b) * 0.5 * damp;
    o[i] = (a / pk) * 0.5;
    idx = (idx + 1) % N;
  }
  return o;
}

export function synth(kind: string, f: number): Float32Array {
  const key = kind + f;
  if (cache[key]) return cache[key];
  const r = { s: 777 };
  const mk = (d: number, fn: (t: number) => number): Float32Array => {
    const o = new Float32Array(Math.floor(SR * d));
    for (let i = 0; i < o.length; i++) o[i] = fn(i / SR);
    return o;
  };
  let out: Float32Array;
  if (kind === 'guitar') out = ks(f, 1.8, 0.9965, 1);
  else if (kind === 'bass') out = ks(f, 1.3, 0.9985, 4);
  else if (kind === 'kick') {
    let ph = 0;
    out = mk(0.4, (t) => {
      ph += (2 * Math.PI * (45 + 105 * Math.exp(-t * 30))) / SR;
      return Math.sin(ph) * Math.exp(-t * 8) * 0.85;
    });
  } else if (kind === 'snare') {
    out = mk(0.25, (t) => rnd(r) * Math.exp(-t * 22) * 0.45 + Math.sin(2 * Math.PI * 185 * t) * Math.exp(-t * 28) * 0.4);
  } else if (kind === 'chat' || kind === 'ohat') {
    let pv = 0;
    const dk = kind === 'chat' ? 90 : 14;
    out = mk(kind === 'chat' ? 0.08 : 0.4, (t) => {
      const x = rnd(r);
      const y = x - pv;
      pv = x;
      return y * Math.exp(-t * dk) * 0.3;
    });
  } else if (kind === 'clap') {
    out = mk(
      0.3,
      (t) =>
        rnd(r) *
        (Math.exp(-t * 60) +
          Math.exp(-(t - 0.012) * 60) * (t > 0.012 ? 1 : 0) +
          Math.exp(-(t - 0.024) * 25) * (t > 0.024 ? 1 : 0)) *
        0.3
    );
  } else if (kind === 'tomL' || kind === 'tomH') {
    const f0 = kind === 'tomL' ? 120 : 190;
    let ph = 0;
    out = mk(0.4, (t) => {
      ph += (2 * Math.PI * (f0 * 0.65 + f0 * 0.35 * Math.exp(-t * 14))) / SR;
      return Math.sin(ph) * Math.exp(-t * 9) * 0.7;
    });
  } else if (kind === 'rim') {
    out = mk(0.08, (t) => Math.sin(2 * Math.PI * 820 * t) * Math.exp(-t * 120) * 0.5 + rnd(r) * Math.exp(-t * 200) * 0.2);
  } else if (kind === 'shaker') {
    let pv = 0;
    out = mk(0.14, (t) => {
      const x = rnd(r);
      const y = x - pv;
      pv = x;
      return y * Math.min(1, t / 0.015) * Math.exp(-t * 28) * 0.25;
    });
  } else {
    out = mk(0.35, (t) => ((Math.sin(2 * Math.PI * 540 * t) > 0 ? 1 : -1) + (Math.sin(2 * Math.PI * 800 * t) > 0 ? 1 : -1)) * Math.exp(-t * 12) * 0.12);
  }
  cache[key] = out;
  return out;
}

export function renderHits(hits: Hit[], startSec: number, len: number): Float32Array {
  const buf = new Float32Array(len);
  for (const h of hits) {
    const idx = Math.round((h.t - startSec) * SR);
    if (idx < 0 || idx >= len) continue;
    const s = synth(h.n.kind, h.n.f);
    const m = Math.min(s.length, len - idx);
    for (let i = 0; i < m; i++) buf[idx + i] += s[i];
  }
  return buf;
}

export class AudioEngine {
  ctx: AudioContext | null = null;
  private master: GainNode | null = null;
  playing = false;
  epoch = 0;
  nextClick = 0;
  private nodes: Array<{ n: AudioBufferSourceNode; g: GainNode } | null> = [null, null, null, null];

  ensure(onTick: () => void, onFrame: (ts: number) => void): AudioContext {
    if (!this.ctx) {
      const AC = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      this.ctx = new AC({ sampleRate: SR });
      this.master = this.ctx.createGain();
      this.master.gain.value = 0.8;
      const comp = this.ctx.createDynamicsCompressor();
      this.master.connect(comp);
      comp.connect(this.ctx.destination);
      window.setInterval(onTick, 25);
      requestAnimationFrame(onFrame);
    }
    if (this.ctx.state === 'suspended') void this.ctx.resume();
    return this.ctx;
  }

  playBuf(buf: Float32Array, when = 0, gain = 1): void {
    if (!this.ctx || !this.master) return;
    const ab = this.ctx.createBuffer(1, buf.length, SR);
    ab.copyToChannel(buf as Float32Array<ArrayBuffer>, 0);
    const n = this.ctx.createBufferSource();
    n.buffer = ab;
    const g = this.ctx.createGain();
    g.gain.value = gain;
    n.connect(g);
    g.connect(this.master);
    n.start(when);
  }

  clickAt(t: number, accent: boolean): void {
    if (!this.ctx || !this.master) return;
    const o = this.ctx.createOscillator();
    const g = this.ctx.createGain();
    o.frequency.value = accent ? 1600 : 1100;
    o.type = 'square';
    g.gain.setValueAtTime(0.14, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + 0.03);
    o.connect(g);
    g.connect(this.master);
    o.start(t);
    o.stop(t + 0.04);
  }

  stopTrack(i: number): void {
    const o = this.nodes[i];
    if (o) {
      try {
        o.n.stop();
      } catch {
        // already stopped
      }
      o.n.disconnect();
      this.nodes[i] = null;
    }
  }

  startTrack(s: State, i: number): void {
    this.stopTrack(i);
    if (!this.ctx || !this.master || !s.tracks[i].part || !this.playing || !s.loopLen) return;
    const loopSec = s.loopLen / SR;
    const now = this.ctx.currentTime + 0.01;
    let when: number;
    let phase: number;
    if (now < this.epoch) {
      when = this.epoch;
      phase = 0;
    } else {
      when = now;
      phase = (now - this.epoch) % loopSec;
    }
    if (s.settings.loopMode === 'once' && now - this.epoch >= loopSec) return;
    const ab = this.ctx.createBuffer(1, s.loopLen, SR);
    ab.copyToChannel(renderTrack(s, i) as Float32Array<ArrayBuffer>, 0);
    const n = this.ctx.createBufferSource();
    n.buffer = ab;
    n.loop = s.settings.loopMode === 'repeat';
    const g = this.ctx.createGain();
    g.gain.value = s.tracks[i].mute ? 0 : 1;
    n.connect(g);
    g.connect(this.master);
    n.start(when, phase);
    this.nodes[i] = { n, g };
  }

  refresh(s: State): void {
    if (this.ctx && this.playing) {
      for (let i = 0; i < 4; i++) this.startTrack(s, i);
    }
  }

  startPlayback(s: State, at: number): void {
    for (let i = 0; i < 4; i++) this.stopTrack(i);
    this.playing = true;
    this.epoch = at;
    for (let i = 0; i < 4; i++) this.startTrack(s, i);
    if (this.ctx && s.bpm) {
      this.nextClick = Math.max(0, Math.ceil((this.ctx.currentTime - at) / (60 / s.bpm)));
    }
  }

  stopPlayback(): void {
    this.playing = false;
    for (let i = 0; i < 4; i++) this.stopTrack(i);
  }
}

