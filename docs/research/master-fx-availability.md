# Master FX availability on MK66 (lo-fi master chain)

Ticket: which stock effects on the original Tracker (MK66, Cortex-M4F @ 180 MHz, no `FEATURE_SYNTH`) can serve a lo-fi master chain of glue compressor, tape saturation, wow/flutter and bit-crush?

All paths are relative to the repo root. Line numbers refer to this checkout. Nothing outside `docs/research/` was modified.

## Short answer

| Need | Stock answer on MK66 | Verdict |
| --- | --- | --- |
| Glue compressor | No dedicated real-time compressor. `AudioEffectDynamics` is a real-time, block-RMS compressor/gate/limiter, but mono, 30 ms minimum attack/release, and only used on line-in today. The master limiter is a peak limiter, not a compressor. | Reusable with caveats (two instances, unlinked), or write a small stereo-linked variant |
| Tape saturation | Master mixer already has five selectable saturation curves (`SaturationType`), integer, essentially free. | Usable as-is for a crude version |
| Wow / flutter | Nothing real-time. `AudioEffectShortDelay` has a fixed delay time (no modulation). The offline flanger has the right LFO + interpolated delay structure but is not a live object. | Must be written; a template exists |
| Bit-crush | `AudioBitDepth` is real-time and trivial (bit-depth only, no sample-rate reduction). Offline `mtEffectBitcrusher` has both bits and rate but is offline. | Bit-depth part reusable; sample-rate part must be written |

## Platform facts used for budgets

- Audio block is 128 samples at 44117.65 Hz: `core/libs/Audio/AudioStream.h:51,57`. One block is about 2.90 ms, which is about 522,000 CPU cycles at 180 MHz (arithmetic, not measured).
- Core build for MK66 is `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16` (single-precision FPU only, so `double` math such as `exp()` is software): `core/makefile:53-54`. The core makefile compiles everything under `libs/`, `effector/`, `AudioEngine/` with a `find` wildcard (`core/makefile:14-21`), so every file listed below is compiled for MK66 unless it carries its own `#ifdef`. `FEATURE_SYNTH` is defined only for IMX (`core/makefile:57`).
- Free resources on MK66: about 144 KB flash, about 80.6 KB internal RAM, about 1,067 KB SDRAM (`docs/memory_map.md` section 1). The 1.5 MB master delay buffer is already fully used by the existing delay (`docs/memory_map.md` section 2; `core/source/mtStructs.h:75`).
- No per-effect CPU or RAM numbers are documented anywhere in the repo. I searched `docs/` and the audio sources for cycle, CPU-load and microsecond figures and found only the generic `/.develop` profiler (live audio ISR CPU % on the Config screen; `docs/developer_guide.md:258`, `docs/memory_map.md:150`, `AUDIO_PERFORMANCE_INFO_ENABLED` in `core/makefile` DEFINES and `core/interface/configEditor/gConfigEdit.cpp:234`). Every cost in the table below is therefore a source-reading estimate, marked "est." and not measured.

## Master signal path today

`tracksAndSendsMixer[L/R]` (dry + delay + reverb sends) goes to `AudioEffectLimiter[L/R]`, then `finalMixer[L/R]`, then the output stream:

- `core/AudioEngine/mtAudioEngine.cpp:503-515` (mixer to limiter connection)
- `core/AudioEngine/AudioOutput.cpp:18-28` (limiter to finalMixer to outputStream)
- `core/AudioEngine/MixerAndFxs.cpp:19-28` (dry/delay/reverb into `tracksAndSendsMixer`)

The natural insertion point for a custom master chain is the connection at `mtAudioEngine.cpp:503-515`, between the sends mixer and the limiter, so the existing limiter stays as the final safety stage.

## Findings table

Cost column: "est." is my reading of the code, not a measurement. "Guess" means I have no basis beyond general Cortex-M4 experience.

### A. Real-time effects (AudioStream objects in the audio graph)

| Effect | RT or offline | On MK66? | Cost | Controls | Citations |
| --- | --- | --- | --- | --- | --- |
| `AudioEffectLimiter` (master peak limiter) | Real-time, already on the master bus, one per channel | Yes. Compiled, in use. Methods live in `.audio_ram_functions` (`AUDIO_CORE_RAM`) | Per-sample float loop with a 5-sample look-ahead, no divide when under threshold. Est. a few tens of cycles/sample, roughly 1-2% of the block budget per channel (guess). RAM: object plus two 5-sample buffers, negligible | Threshold (101-step table), attack 1-1000, release 1-1000, "Subtle" vs "Hard" mode, optional sidechain source (mux) | `core/libs/Audio/effect_limiter.h:8-50`; `effect_limiter.cpp:20-50` (setters), `:71-79` (update), `:98` (sidechain loop), `:199` (main loop); ranges `core/source/mtStructs.h:325-328`; modes `:565-568`; defaults `:876-878,954`; wiring `mtAudioEngine.cpp:1021-1047` |
| `AudioEffectDynamics` (gate + soft-knee compressor + limiter, auto makeup gain) | Real-time | Yes. Compiled. Instantiated only as the line-in gate, one per input channel | Light. Detector is one 128-sample RMS per block (4x unrolled MAC), a handful of float ops, a few `expf`/`log`-class calls per block, then one gain ramp across the block via `applyGain`. Est. well under 1% of block budget per instance (guess) | `gate(threshold, attack, release, hysteresis)`, `compression(threshold, attack, release, ratio, kneeWidth)`, `limit(...)`, `autoMakeupGain(headroom)` / `makeupGain(dB)`. Threshold -110..0 dB, ratio 1..60, knee 0..32 dB, attack/release clamped to 0.03-4.0 s | `core/libs/Audio/effect_dynamics.h:41-45` (limits), `:67,92,122,140,150` (setters), `:233` (time-to-alpha, block rate); `effect_dynamics.cpp:35-52` (RMS), `:59-173` (update), `:109-133` (compressor), `:167` (gain apply); only user `core/AudioEngine/AudioInput.h:40`, `mtAudioEngine.cpp:265-266` |
| `AudioBitDepth` | Real-time (per voice today) | Yes. Compiled, in use (one per voice channel) | Very cheap: 64 masked 32-bit ANDs per block per channel. Est. under 0.2% per channel (guess) | `setBitDepth(4..16)`; no sample-rate reduction | `core/libs/Audio/effect_bitDepth.cpp:3-37`, `.h:5-19`; per-voice wiring `core/AudioEngine/AudioVoice.cpp:36-37`; engine setter `mtAudioEngine.cpp:1745-1750` |
| `AudioFilterStateVariable` overdrive stage | Real-time (per voice today) | Yes. Compiled, in use | `update_overdrive` is integer, one piecewise curve per sample. Est. 15-25 cycles/sample, around 1% per channel (guess). Object is tiny | `setOverdriveAmount(0..255)` (0 = off), then the SVF (cutoff/resonance/type) after it | `core/libs/Audio/filter_variable.cpp:70-108` (curve), `:244-351` (update; filter can be bypassed with `disconnect()`), `filter_variable.h:50-61`; use `mtAudioEngine.cpp:1741-1744` |
| Mixer output saturation (`SaturationType`) | Real-time, built into `AudioMixer`, already on the master bus | Yes. Compiled, in use, user-selectable in Master Params | Integer-only. Soft clip is one cubic per sample; the stage already exists in the mix path, so adding a curve costs almost nothing. Any non-hard-clip type disables the single-contributor fast path (`mixer.cpp:65`) so a few extra cycles per block (est.) | `kHardClip, kSoftClip, kSaturate, kOverdrive, kOverdrive2`. Applied to the 32-bit mix sum, so drive is set by upstream track and master gain | `core/source/mtStructs.h:585-594`; `core/libs/Audio/dspApplyGain.cpp:172-290`; `mixer.h:73,89`; `mixer.cpp:65-110`; applied to both track and sends mixers `mtAudioEngine.cpp:1550-1556`; UI `core/interface/masterParams/fMasterParams.cpp:1093-1102` |
| `AudioEffectShortDelay` (master delay) | Real-time | Yes. Compiled, in use | Per-sample integer loop with a one-pole filter in the feedback path. Mono or ping-pong; already skips silent blocks. RAM: 3 lines x 512 KB in SDRAM = 1.5 MB, all of the existing delay buffer, none spare | Time (ms, up to 3500), feedback (capped 0.99), ping-pong, tempo sync, feedback filter frequency | `core/libs/Audio/effect_shortDelay.h:5-65`; `.cpp:15-31` (begin), `:55-62` (setTime), `:98-113` (filter); fixed delay per block `:387,446,497`; buffer `core/source/mtStructs.h:75`, `docs/memory_map.md` section 2 |
| `AudioEffectPolyverb` (master reverb) | Real-time | Yes. Compiled; on MK66 both internal buffers are placed in SDRAM to save SRAM | Heaviest of the stock effects (est., guess); skips silent blocks. RAM 32 KB + 32 KB SDRAM | Size/time, damp, predelay, diffusion, amount | `core/libs/Audio/effect_polyverb.cpp:6-14` (MK66 placement), `:25-152`; `effect_polyverb.h:24-36`; `docs/memory_map.md` section 2 |
| `AudioAmplifier` | Real-time | Yes | Trivial; passes through at unity | `gain(float)`, ramped across the block | `core/libs/Audio/amplifier.h:33-52`; `amplifier.cpp:31-71` |
| `MasterEnhancer` (bass enhance, surround/stereo width, 5-band graphic EQ, auto-volume) | Not DSP in the MCU. It programs the SGTL5000 codec's hardware processor over I2C | Yes. `Sgtl5000Impl` exists for MK66, so bass enhance and surround are wired. `mtMasterEnhancer::enable()` is called at boot | Zero MCU audio cost (codec does the work) | Bass boost 0-100, stereo boost, 5 EQ bands | `core/AudioEngine/MasterEnhancer/mtMasterEnhancer.cpp:1-49`; `mtAudioEngine.cpp:257-261`; `platforms/mk66/source/Sgtl5000Impl.cpp:67-120`; UI `fMasterParams.cpp:583-584` |

### B. Offline Sample Editor effects (not usable live)

All run as `mtEffect` subclasses in 8192-sample chunks over the Sample Editor buffers (`confirmed` to `processed`), not inside the audio graph. They are compiled for MK66 (`core/makefile` globs `effector/`) and registered in `core/effector/mtSampleEditorEngine.cpp:21-38`. Their only value for the master chain is as algorithm templates.

| Effect | RT or offline | On MK66? | Cost | Controls | Citations | Template value |
| --- | --- | --- | --- | --- | --- | --- |
| `mtEffectCompresor` | Offline | Yes | Per-sample float with `powf`, `log10f`, `powf` per channel (est. several hundred cycles/sample, guess). Far too heavy to run live as written | Threshold (dB), ratio, attack, release | `core/effector/mtEffectCompresor.cpp:6-14,28-110` (`powf`/`log10f` at `:69,71,77`) | Shows an intended per-sample feed-forward RMS compressor. Do not port as-is |
| `mtEffectLimiter` | Offline | Yes | Not read in depth | Threshold (dB) and others | `core/effector/mtEffectLimiter.cpp:4-16` | Duplicate of what the live limiter does |
| `mtEffectBitcrusher` | Offline | Yes | Trivial per sample | Bits 1-16, rate (sample-and-hold step 1-64 samples) | `core/effector/mtEffectBitcrusher.cpp:6-14,25-40,50-100` | Source of the sample-rate-reduction logic (hold each sample N times) to copy into a live version |
| `mtEffectOverdrive` | Offline | Yes | Quadratic is integer. Exponential uses double `exp()` (slow on M4F). Reciprocal is one float divide | Gain, type (quadratic / exponential / reciprocal) | `core/effector/mtEffectOverdrive.cpp:20-120` (`exp(` at `:98`) | Reciprocal curve `x/(1+|x|)` is the cheap, tape-like soft saturator to copy |
| `mtEffectFlanger` | Offline | Yes | Per-sample integer + one multiply. Works in 128-sample blocks, matching the audio block size. Buffer 4096 samples x 2 ch in SDRAM | Delay length, offset %, depth, LFO rate | `core/effector/mtEffectFlanger.cpp:8` (buffer), `:10-22`, `:99-155` (Q15 sine LFO modulating a circular delay line with interpolation); `.h:8` | Closest existing code to wow/flutter (LFO-modulated delay line). Its interpolation uses low bits of the phase accumulator as the fraction rather than the true fractional read position, so it is an approximation (moderate confidence on this reading) |
| `mtEffectChorus` | Offline | Yes | Not read in depth | Delay length, voice count | `core/effector/mtEffectChorus.cpp:10-18` | Same family as flanger |
| `mtEffectDelay`, `mtEffectEQ`, `mtEffectAmplifier`, `mtEffectNormalizer`, fade/cut/reverse, timestretch, wavetable smoother | Offline | Yes | Not read | n/a | `core/effector/mtSampleEditorEngine.cpp:21-38`; `mtEffectDelay.cpp:9-20` | Not relevant to the chain |
| `.ptfx` plugins (Sample Editor SDK) | Offline | Yes; MK66 code cap 32 KB, scratch up to 384 KB | Plugin-defined | Up to 12 params | `docs/modular_effects_system.md` (bundled plugins table, section 9); `docs/memory_map.md` section 3. Bundled: haas, stereo_delay, multimode_filter (with drive modes), `transient_shaper.c`, `convolution_reverb.c` in `editor-fx-sdk/effects/` | Offline only. Cannot be a live master insert |

## Answers to the specific questions

1. **Is there a usable real-time compressor for master glue?** Not a purpose-built one.
   - `AudioEffectDynamics` is a real compressor with a soft knee, ratio up to 60:1, makeup gain and a block-rate RMS detector, and it is compiled for MK66 (`effect_dynamics.h:92-120`). For glue it is plausibly adequate: attack/release are clamped to a 30 ms minimum (`MIN_T`, `effect_dynamics.h:41`; time constants are computed at block rate, `:233`), which suits slow glue settings but not fast transient control.
   - Limitations: it is mono (one input, no sidechain input), so using one per channel gives unlinked left/right gain and a wandering stereo image. It has no dry/wet mix.
   - It is exercised in the shipped firmware only as the line-in gate. Its constructor also calls `compression()`, `limit()` and `autoMakeupGain()` with defaults (`effect_dynamics.h:50-62`), and the engine calls only `gate(GATE_DB)` (`mtAudioEngine.cpp:265-266`), so the compressor and limiter appear to be enabled at defaults on line-in. I did not verify that behaviour or the resulting makeup gain. Treat the compressor path as little-tested.
   - The master `AudioEffectLimiter` is a peak limiter with a 5-sample look-ahead, not a compressor. With a low threshold and slow release it will squash, but it is not a ratio-based glue compressor.
2. **Tape saturation?** No tape-specific model. The stock master mixer offers five saturation curves, user-selectable, essentially free CPU. `kSoftClip` (cubic, `out = in - in^3/6.72`) and `kSaturate` (`3x/2 - x^3/2`) are the gentlest (`dspApplyGain.cpp:184-224`). Drive comes from the level feeding the mixer. This is a static waveshaper only: no frequency-dependent saturation, no hysteresis, no head-bump. Good enough for a crude lo-fi colour.
3. **Wow/flutter?** Nothing real-time. `AudioEffectShortDelay` uses a constant integer `timeInSamples` per block (`effect_shortDelay.cpp:55-62,387`), with no LFO, no fractional read and no interpolation. It also owns the whole 1.5 MB delay buffer.

## Gap list (needs writing from scratch)

Written as design notes only. I have not written or built any code.

1. **Wow/flutter delay (required, no stock equivalent).** A new `AudioStream` per channel: a short circular buffer (about 8-30 ms; 1024-2048 samples of `int16` is about 2-4 KB per channel, so about 4-8 KB total, internal RAM is fine) and a read pointer modulated by a slow LFO (wow around 0.5-2 Hz, flutter around 5-12 Hz) with linear interpolation. Copy the structure from `mtEffectFlanger::calculate` (128-sample blocks, Q15 sine) but use a true fractional position. Est. 30-60 cycles/sample (guess), so about 1-2% of the budget per channel.
2. **Stereo-linked glue compressor (strongly recommended).** Adapt `AudioEffectDynamics::update` to take two inputs, compute the RMS of both blocks (or of the sum), apply one gain to both. About 60-100 lines. The alternative of two unlinked `AudioEffectDynamics` instances works with no new code but gives image shift. Faster attack than 30 ms would need changing `MIN_T`.
3. **Sample-rate reduction.** `AudioBitDepth` only truncates bits. Add a sample-and-hold step (logic exists in offline `mtEffectBitcrusher.cpp:50-100`) either into a new master-bus object or a new class. Tiny cost.
4. **Master-bus instances of `AudioBitDepth`.** The class is reusable as-is, but there is none on the master bus; two instances (L/R) must be created and wired. Note a robustness quirk: `setBitDepth` computes its mask from the unclamped argument (`effect_bitDepth.cpp:33-36`), so keep input inside 4-16.
5. **Better tape-style saturation (optional).** Stock curves are static and fairly hard. A softer `x/(1+|x|)` (the offline reciprocal curve at `mtEffectOverdrive.cpp:108-114`) with a pre-gain and a gentle high-frequency roll-off would sound more tape-like. A few lines, one float divide per sample (est., guess).
6. **Master-chain wiring and priority control.** The audio graph is static (`AudioConnection` objects built once). Nothing in the repo implements "degrade per-track FX first if CPU runs short"; that logic must be written. The existing profiler (`/.develop`) can measure ISR load but I found no runtime load-shedding hook.
7. **UI and project-file fields** for any new parameters. Out of scope for this ticket.

## Likely CPU picture (all estimates)

The block budget is about 522,000 cycles. Estimated cost for the master chain if built from the parts above: stock saturation (about 0), two unlinked or one linked dynamics (about 1-2%), master `AudioBitDepth` x2 (under 0.5%), new wow/flutter x2 (about 2-4%), new sample-and-hold (under 1%). That suggests about 5-8% in total, but this is a guess to be replaced by a measurement with the `/.develop` profiler on real hardware; the dominant unknown is the existing load from 8 voices, delay and reverb, which I did not estimate.

## Confidence

- **High:** which effects exist, whether they are real-time or offline, that they are compiled for MK66 (build globs, no MK66-specific guards, MK66 `Sgtl5000Impl` present), the signal path, and that there is no stock wow/flutter or dedicated stereo compressor. These come straight from the source.
- **Medium:** that `AudioEffectDynamics` is musically adequate as a glue compressor (I read the algorithm but did not hear or run it), and the line-in default-enable observation.
- **Low (guesses):** every CPU percentage and cycle count. The repo contains no measurements, I did not build or profile, and I did not read `reverb.h`, `effect_shortDelay` loops, the chorus or most offline effects in depth.
- Not checked: whether the 80 KB internal RAM / 144 KB flash headroom still holds after the new code (the new objects above are small, but flash for code is unmeasured), and anything about the Tracker+/Mini firmware paths beyond the build flags.
