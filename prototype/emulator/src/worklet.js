// Runs inside the audio thread. It holds the Four-Track application compiled to WebAssembly (the
// same code the Tracker firmware runs) and does what the firmware's audio interrupt and main
// loop do: one 128-frame block at a time.
class FourTrackProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ready = false;
    this.blocks = 0;
    this.demo = false;
    this.demoPhase = 0;
    this.demoVoices = [];
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  onMessage(m) {
    if (m.type === 'init') {
      WebAssembly.instantiate(m.wasm, {}).then(({ instance }) => {
        this.x = instance.exports;
        const mem = this.x.memory.buffer;
        const pal = new Uint32Array(mem, this.x.ft_palette_ptr(), 12);
        m.palette.forEach((v, i) => (pal[i] = v));
        m.advances.forEach((a, f) => new Uint8Array(mem, this.x.ft_adv_ptr(f), 128).set(a));
        this.x.ft_init(sampleRate);
        this.inL = new Float32Array(mem, this.x.ft_in_l_ptr(), 128);
        this.inR = new Float32Array(mem, this.x.ft_in_r_ptr(), 128);
        this.outL = new Float32Array(mem, this.x.ft_out_l_ptr(), 128);
        this.outR = new Float32Array(mem, this.x.ft_out_r_ptr(), 128);
        this.cmd = new Int32Array(mem, this.x.ft_cmd_ptr(), 4096 * 7);
        this.pool = new Uint8Array(mem, this.x.ft_pool_ptr(), 32768);
        this.x.ft_set_monitor(1);
        this.ready = true;
        this.port.postMessage({ type: 'ready', sampleRate });
      });
    } else if (m.type === 'demo') {
      this.demo = m.on; // can arrive before the engine has loaded
    } else if (!this.ready) {
      return;
    } else if (m.type === 'init-fonts') {
      m.advances.forEach((a, f) => new Uint8Array(this.x.memory.buffer, this.x.ft_adv_ptr(f), 128).set(a));
    } else if (m.type === 'key') this.x.ft_key(m.key);
    else if (m.type === 'jog') this.x.ft_jog(m.delta);
    else if (m.type === 'pad') this.x.ft_pad(m.pad, m.down ? 1 : 0);
    else if (m.type === 'monitor') this.x.ft_set_monitor(m.on ? 1 : 0);
    else if (m.type === 'palette') {
      const pal = new Uint32Array(this.x.memory.buffer, this.x.ft_palette_ptr(), 12);
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
    if (this.demo) {
      this.demoBlock(this.inL, this.inR);
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
      const n = this.x.ft_frame();
      const used = this.x.ft_pool_used();
      this.port.postMessage(
        { type: 'frame', cmd: this.cmd.slice(0, n * 7), pool: this.pool.slice(0, used), mode: this.x.ft_state_mode() }
      );
    }
    return true;
  }
}
registerProcessor('four-track', FourTrackProcessor);
