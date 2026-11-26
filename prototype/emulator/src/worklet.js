// Runs inside the audio thread. It holds the Four-Track application compiled to WebAssembly (the
// same code the Tracker firmware runs) and does what the firmware's audio interrupt and main
// loop do: one 128-frame block at a time.
function ascii(str) {
  const out = new Uint8Array(Math.min(str.length, 23));
  for (let i = 0; i < out.length; i++) out[i] = str.charCodeAt(i) & 127;
  return out;
}

class FourTrackProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ready = false;
    this.blocks = 0;
    this.demo = false;
    this.demoPhase = 0;
    this.demoVoices = [];
    this.files = new Map(); // path -> { buf: Uint8Array, len }
    this.changed = new Set();
    this.pending = [];
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  // ---- the SD card: files kept in memory, reached by the firmware code through imports
  str(ptr) {
    const mem = new Uint8Array(this.x.memory.buffer);
    let end = ptr;
    while (mem[end] !== 0) end++;
    let out = '';
    for (let i = ptr; i < end; i++) out += String.fromCharCode(mem[i]); // no TextDecoder in a worklet
    return out;
  }
  hostImports() {
    const self = this;
    return {
      file_write(pathPtr, dataPtr, len, append) {
        const path = self.str(pathPtr);
        let f = self.files.get(path);
        if (!f || !append) { f = { buf: new Uint8Array(Math.max(len, 1024)), len: 0 }; self.files.set(path, f); }
        if (f.len + len > f.buf.length) {
          const nb = new Uint8Array(Math.max(f.buf.length * 2, f.len + len));
          nb.set(f.buf.subarray(0, f.len));
          f.buf = nb;
        }
        f.buf.set(new Uint8Array(self.x.memory.buffer, dataPtr, len), f.len);
        f.len += len;
        self.changed.add(path);
        return 1;
      },
      file_read(pathPtr, offset, bufPtr, len) {
        const f = self.files.get(self.str(pathPtr));
        if (!f || offset >= f.len) return 0;
        const n = Math.min(len, f.len - offset);
        new Uint8Array(self.x.memory.buffer, bufPtr, n).set(f.buf.subarray(offset, offset + n));
        return n;
      },
      file_size(pathPtr) { const f = self.files.get(self.str(pathPtr)); return f ? f.len : 0; },
      file_remove(pathPtr) { const p = self.str(pathPtr); self.changed.add(p); return self.files.delete(p) ? 1 : 0; },
      // The card's samples: folders first, then .wav files. Each entry is 40 bytes: a folder flag,
      // the name (28 bytes at 4), the size at 32 and the number of WAV files inside at 36.
      file_browse(dirPtr, outPtr, max) {
        let dir = self.str(dirPtr);
        if (!dir.endsWith('/')) dir += '/';
        const folders = new Map(), files = [];
        for (const [p, f] of self.files) {
          if (!p.startsWith(dir)) continue;
          const rest = p.slice(dir.length);
          const slash = rest.indexOf('/');
          const isWav = (n) => n.toLowerCase().endsWith('.wav');
          if (slash >= 0) { if (isWav(rest)) { const n = rest.slice(0, slash); folders.set(n, (folders.get(n) || 0) + 1); } }
          else if (isWav(rest)) files.push([rest, f.len]);
        }
        const by = (a, b) => a[0].toLowerCase().localeCompare(b[0].toLowerCase());
        const list = [...[...folders].sort(by).map(([n, c]) => [n, 0, c, 1]), ...files.sort(by).map(([n, l]) => [n, l, 0, 0])].slice(0, max);
        const out = new Uint8Array(self.x.memory.buffer, outPtr, max * 40);
        out.fill(0);
        const dv = new DataView(self.x.memory.buffer, outPtr, max * 40);
        list.forEach(([n, size, count, dir], i) => {
          out[i * 40] = dir;
          out.set(ascii(n).slice(0, 27), i * 40 + 4);
          dv.setUint32(i * 40 + 32, size, true);
          dv.setUint32(i * 40 + 36, count, true);
        });
        return list.length;
      },
      file_list(dirPtr, outPtr, max) {
        const prefix = self.str(dirPtr) + '/';
        const out = new Uint8Array(self.x.memory.buffer, outPtr, max * 24);
        let n = 0;
        for (const p of self.files.keys()) {
          if (p.startsWith(prefix) && n < max) {
            const name = ascii(p.slice(prefix.length));
            out.fill(0, n * 24, n * 24 + 24);
            out.set(name, n * 24);
            n++;
          }
        }
        return n;
      },
    };
  }

  onMessage(m) {
    if (m.type === 'init') {
      WebAssembly.instantiate(m.wasm, { host: this.hostImports() }).then(({ instance }) => {
        this.x = instance.exports;
        const mem = this.x.memory.buffer;
        const pal = new Uint32Array(mem, this.x.ft_palette_ptr(), 21);
        m.palette.forEach((v, i) => (pal[i] = v));
        m.advances.forEach((a, f) => new Uint8Array(mem, this.x.ft_adv_ptr(f), 128).set(a));
        (m.files || []).forEach((f) => this.files.set(f.path, { buf: f.data, len: f.data.length }));
        this.x.ft_init(sampleRate);
        this.inL = new Float32Array(mem, this.x.ft_in_l_ptr(), 128);
        this.inR = new Float32Array(mem, this.x.ft_in_r_ptr(), 128);
        this.outL = new Float32Array(mem, this.x.ft_out_l_ptr(), 128);
        this.outR = new Float32Array(mem, this.x.ft_out_r_ptr(), 128);
        this.cmd = new Int32Array(mem, this.x.ft_cmd_ptr(), 4096 * 7);
        this.pool = new Uint8Array(mem, this.x.ft_pool_ptr(), 32768);
        this.ready = true;
        this.port.postMessage({ type: 'ready', sampleRate });
      });
    } else if (m.type === 'putfile') {
      this.files.set(m.path, { buf: m.data, len: m.data.length });
    } else if (m.type === 'getfile') {
      const f = this.files.get(m.path);
      this.port.postMessage({ type: 'file', path: m.path, data: f ? f.buf.slice(0, f.len) : null, why: m.why });
    } else if (m.type === 'demo') {
      this.demo = m.on; // can arrive before the engine has loaded
    } else if (!this.ready) {
      return;
    } else if (m.type === 'init-fonts') {
      m.advances.forEach((a, f) => new Uint8Array(this.x.memory.buffer, this.x.ft_adv_ptr(f), 128).set(a));
    } else if (m.type === 'key') this.x.ft_key(m.key);
    else if (m.type === 'jog') this.x.ft_jog(m.delta);
    else if (m.type === 'keyup') this.x.ft_key_up(m.key);
    else if (m.type === 'shift') this.x.ft_shift(m.on ? 1 : 0);
    else if (m.type === 'pad') this.x.ft_pad(m.pad, m.down ? 1 : 0);
    else if (m.type === 'sample') {
      // mono 16-bit audio for a pad slot
      const ptr = this.x.ft_sample_prepare(m.slot, m.data.length);
      if (!ptr) { this.port.postMessage({ type: 'sample-result', slot: m.slot, ok: false }); return; }
      new Int16Array(this.x.memory.buffer, ptr, m.data.length).set(m.data);
      const name = ascii(m.name);
      const np = new Uint8Array(this.x.memory.buffer, this.x.ft_sample_name_ptr(m.slot), 24);
      np.fill(0); np.set(name);
      this.port.postMessage({ type: 'sample-result', slot: m.slot, ok: true });
    }
    else if (m.type === 'monitor') this.x.ft_set_monitor(m.on ? 1 : 0);
    else if (m.type === 'palette') {
      const pal = new Uint32Array(this.x.memory.buffer, this.x.ft_palette_ptr(), 21);
      m.palette.forEach((v, i) => (pal[i] = v));
      this.x.ft_set_palette();
    }
  }

  // A made-up instrument for trying the recorder without a microphone: a plucked chord
  // arpeggio, slightly different in the left and right channels.
  demoBlock(outL, outR) {
    const bpm = 100, notes = [220, 261.63, 329.63, 392, 329.63, 261.63, 196, 246.94];
    const beat = (sampleRate * 60) / bpm / 2;
    for (let i = 0; i < 128; i++) {
      if (this.demoPhase % Math.floor(beat) === 0) {
        const n = Math.floor(this.demoPhase / beat) % notes.length;
        this.demoVoices.push({ f: notes[n], t: 0, p: 0 });
        if (this.demoVoices.length > 6) this.demoVoices.shift();
      }
      this.demoPhase++;
      let s = 0;
      for (const v of this.demoVoices) {
        v.p += v.f / sampleRate;
        if (v.p >= 1) v.p -= 1;
        const env = Math.exp(-v.t / (sampleRate * 0.5));
        s += (v.p * 2 - 1) * env * 0.12 + Math.sin(v.p * 6.2832) * env * 0.12;
        v.t++;
      }
      outL[i] = s;
      outR[i] = s * 0.92;
    }
  }

  process(inputs, outputs) {
    const out = outputs[0];
    if (!this.ready) return true;
    const input = inputs[0];
    // The demo instrument only sounds while a take is being recorded (first take or a Part)
    const mode = this.x.ft_state_mode();
    // While the take waits for sound the demo instrument stays quiet for two seconds, then plays
    if (this.x.ft_state_waiting()) this.waitBlocks = (this.waitBlocks || 0) + 1; else this.waitBlocks = 0;
    const quietWait = this.waitBlocks > 0 && this.waitBlocks < (2 * sampleRate) / 128;
    if (this.demo) {
      if ((mode === 1 || mode === 3) && !quietWait) this.demoBlock(this.inL, this.inR);
      else { this.inL.fill(0); this.inR.fill(0); }
    } else if (input && input.length > 0) {
      this.inL.set(input[0]);
      this.inR.set(input.length > 1 ? input[1] : input[0]);
    } else {
      this.inL.fill(0);
      this.inR.fill(0);
    }
    this.x.ft_audio(128);
    out[0].set(this.outL);
    if (out.length > 1) out[1].set(this.outR);

    this.blocks++;
    if (this.blocks % 2 === 0) this.x.ft_tick(Math.floor((this.blocks * 128 * 1000) / sampleRate));
    if (this.blocks % 8 === 0) {
      if (this.changed.size) {
        this.port.postMessage({ type: 'changed', paths: [...this.changed].map((p) => ({ path: p, size: (this.files.get(p) || { len: -1 }).len })) });
        this.changed.clear();
      }
      const n = this.x.ft_frame();
      const used = this.x.ft_pool_used();
      this.port.postMessage(
        { type: 'frame', cmd: this.cmd.slice(0, n * 7), pool: this.pool.slice(0, used), mode: this.x.ft_state_mode(), screen: this.x.ft_state_screen() }
      );
    }
    return true;
  }
}
registerProcessor('four-track', FourTrackProcessor);
