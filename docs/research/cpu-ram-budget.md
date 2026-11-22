# CPU and RAM budget: four-track looping recorder on the Original Tracker (MK66)

Ticket question: can the trimmed stock engine run 4 looping mono Parts from RAM, one recording, a live
line-in monitor, pad sample hits and the master delay/reverb/limiter without dropouts on MK66?

Method: read-only source reading (`core/AudioEngine`, `core/libs/Audio`, `platforms/mk66`, `docs/memory_map.md`,
`docs/developer_guide.md`, `UPCOMING.md`). Nothing was built or run. No cycle counts or CPU percentages for
individual voices exist anywhere in the repo, so every CPU number below is my own estimate and is labelled as such.
Line numbers are approximate to a few lines.

## Headline

* RAM: **fits, but only if the Sample Editor / `.ptfx` machinery is removed or relocated.** 4 x 30 s mono is
  about 10.1 MiB of the 11.25 MiB sample pool. The stock Sample Editor scratch region overlaps everything above
  the first 3.75 MiB of that pool, so it cannot coexist with the Parts. Free SDRAM outside the pool is only about 1 MiB.
* CPU: **very likely fits, confidence medium-low.** The target workload is a strict subset of what stock can
  already run at once (8 sample tracks, each possibly stereo, plus all master FX and line-in). Stock does not
  promise dropout-free playback at its own maximum (`UPCOMING.md:57`), so "subset of stock" is a
  reassurance, not proof. My estimate is 35-60% of the audio ISR budget, dominated by the reverb.
* Two stock behaviours will bite the design and need changing (both small): recording freezes per-voice updates
  (`mtAudioEngine.cpp:277`), and files of 256 KB or more default to SD streaming at import.
* The final answer needs a hardware run. See the last sections.

## Fixed facts (high confidence)

| Fact | Value | Source |
| --- | --- | --- |
| Audio block size | 128 samples per update; one mono block is 256 B of samples plus a 4 B header | `platforms/mk66/config/config.h:14`, `core/libs/Audio/AudioStream.h:56,64-71` |
| Real sample rate | 44117.647 Hz (180 MHz x 16 / 255 / 256), not exactly 44.1 kHz | `core/source/Tracker.cpp:198-205` |
| Block period | 128 / 44117.647 = **2.901 ms = about 522,000 CPU cycles at 180 MHz** (about 4,080 cycles per sample, for everything) | derived from the two rows above and `-DF_CPU=180000000` at `platforms/mk66/makefile:44` |
| Where audio runs | The whole graph runs inside the I2S output DMA interrupt, priority level 5. Main loop (UI, SD, display) gets whatever is left. SDHC is level 6, USB is 3, input DMA is 4 | `platforms/mk66/config/interruptsConfig.h:16-23`, `platforms/mk66/3rdparty/Audio/output_i2s.cpp:67-68`, `platforms/mk66/source/TrackerMain.cpp:20-30` |
| What the ISR does per block | MIDI update, sequencer tick, `engine.update()` (which includes `recorder.update()` and 8x `instrumentPlayer[i].update()`), then `AudioStream::update_all()` (every active node, in creation order) | `core/source/Tracker.cpp:180-210`, `core/AudioEngine/mtAudioEngine.cpp:273-322`, `core/libs/Audio/AudioStream.cpp:342-358` |
| Idle nodes are skipped | `AudioStream::active` gates `update()`; a null block means silence and many nodes then do no work (`AUDIO_NULL_IS_SILENCE`) | `AudioStream.cpp:349-353`, `AudioStream.h:46-48`, `UPCOMING.md:108-114` |
| Stock voice count | 8 sample voices (`kVoices = 8`, `kSampleTracks = 8`, `playerEngine instrumentPlayer[8]`). No synth on MK66: `FEATURE_SYNTH` is only defined under `PLATFORM_IMX` | `core/AudioEngine/mtAudioEngine.h:40,827`, `core/source/mtStructs.h:89`, `core/makefile:55-57` |
| Block pool | `AudioMemory(70)` on MK66 = 70 blocks x 260 B = about 18 KB in `.dmabuffers` (internal SRAM). Running out of blocks gives silent dropouts | `mtAudioEngine.cpp:180-182`, `AudioStream.h:115-120` |
| Master FX location | Reverb, delay and limiter run on the MCU. The SGTL5000 EQ, bass enhance, surround and auto volume are codec hardware, so they cost no MCU time | `core/AudioEngine/MasterEnhancer/*`, `mtAudioEngine.cpp:256-257` |
| Built-in profiler | `Profiler` brackets MIDI + sequencer + engine update + `update_all`, and reports min/max/avg/last as a percent of the block period. Needs an empty `/.develop` file on the SD card; Play/Stop on the Config screen resets it. The title bar warns at 85 / 93 / 99 % | `core/source/Tracker.cpp:191,209`, `platforms/mk66/source/Profiler.h:30`, `core/interface/core/titleBar.h:34-36`, `docs/developer_guide.md:259` |

## Per-voice chain in stock (what one Part costs)

Each of the 8 voices owns, per channel, a memory player, bit-depth crusher, state-variable filter, amplifier,
two panning amplifiers, two RMS meters and a stereo mirror (`core/AudioEngine/AudioVoice.cpp:30-39`). Its output
then goes to the track mixers, to the delay send mixer and to the reverb send mixer (L and R each), and to the
limiter sidechain mux (`mtAudioEngine.cpp:334-369,414-430`). Mono samples run only channel 0 and stop channel 1
(`mtAudioNoteOnHandle.cpp:28-30,59-63`; `UPCOMING.md:50`), with the mirror handing the same block to both
sides. The filter can be disconnected when disabled (`mtAudioEngine.cpp:2011-2019`).

The loop players have a fast path for non-streamed, non-glide playback that keeps the playhead in locals
(`play_memory_loop_forward.cpp:165-176`). Streamed voices take a slower per-sample path (`currentIsStreamed`).
That is another reason to keep Parts out of streaming (see hazards).

Besides the audio graph, each voice also runs a control-rate update every block: end-of-play detection,
up to 5 envelopes (pan and finetune are always on) and parameter refresh
(`core/AudioEngine/mtAudioUpdateHandle.cpp:7-47`). Trimming those is a likely cheap win.

## Budget table: CPU (per 128-sample block, ISR budget about 522,000 cycles)

All "est." values are my own instruction-count reasoning for a 180 MHz Cortex-M4F, not measurements. Treat
them as plus or minus a factor of 2. Cycle ranges are for one block.

| Item | Stock behaviour | Est. cost | Confidence | Basis |
| --- | --- | --- | --- | --- |
| One mono looping Part (player, bit-depth, filter if on, amp, 2 pan amps, 2 RMS, send/mix inputs) | Stock already runs 8 of these, each possibly stereo | 13k-26k cycles = 2.5-5% | Low | Guess from per-sample work: interpolating loop about 25-40 cycles/sample, the rest of the chain about 70-150. The repo has no per-voice figure |
| 4 mono looping Parts | Half the stock voice count and, against stereo stock, a quarter of the channel chains | 10-20% | Low | 4 x the row above |
| Parts' SDRAM reads | 16-bit reads from SDRAM at 90 MHz on a 32-bit bus; about 350 KB/s total for 4 voices at normal pitch. Bandwidth is no problem; latency per uncached read is the open question | Included above, could add several % | Low | SDRAM clock `platforms/mk66/3rdparty/SDRAM/sdram.cpp` (`clockSrc = 90000000`). I found no cache setup in `platforms/mk66`, so whether `0x70000000` is cached is unknown to me |
| Live line-in monitor (stereo-to-mono, 2 gates, 2 RMS, mixer inputs, route to final mix) | Stock monitor path | 3-6% | Low | `AudioInput.cpp:4-41`, `mtAudioEngine.cpp:371-430`. Guess |
| One recording | `AudioRecordQueue` (10 blocks) plus a 256 B `memcpy` per block into SDRAM, done inside the ISR. Trivial | under 1% | Medium | `core/libs/Audio/record_queue.h:31`, `core/source/Recorder.cpp:84-160` |
| Pad sample hit (a 5th voice) | One more voice of the same type; `noteOn` disables audio interrupts briefly while it sets up | 2.5-5% while sounding | Low | as row 1; `mtAudioNoteOnHandle.cpp:19-23` |
| Master delay (`AudioEffectShortDelay`) | Integer maths, 1.5 MB in SDRAM, sleeps on silence | 1-3% | Low | `effect_shortDelay.cpp`, `effect_shortDelay.h:5-7`. Guess |
| Master reverb (`AudioEffectPolyverb`) | Floating-point Griesinger reverb, about 23 delay-line accesses per sample. On MK66 the 32 KB "buffer E" is in SDRAM (8 of 12 reads and 8 of 11 writes) rather than in internal RAM as on i.MX | **10-20%, likely the single largest item** | Low | `effect_polyverb.cpp:7-13`, `Polyverb/reverb.h:72-140`. It sleeps when input is silent and the tail has decayed (`effect_polyverb.cpp:52-79`), so the worst case is with sound going in |
| Limiter L+R (float, look-ahead) | Per-sample float, optional sidechain | 2-4% | Low | `effect_limiter.cpp:98-270`. Guess |
| Final mix, output stream, USB output stream (if USB audio is on) | Fixed overhead | 2-5% | Low | `AudioOutput.cpp`. The maintainers call USB audio a CPU risk, `UPCOMING.md:57` |
| Sequencer tick + 8x `playerEngine::update` + MIDI | Control-rate, every block, in the ISR | 2-5% | Low | `Tracker.cpp:190-206`. Guess |
| **Total, everything in the ticket on at once** | | **35-60% of ISR budget** | **Low** | Sum of the rows above |

Reference points that are not my guesses:

* The maintainers' own statement: "Heavy use of stereo samples, sample streaming, or USB audio streaming can
  max out the CPU and cause audio stuttering or UI lag" on this hardware (`UPCOMING.md:57`). So stock at its
  maximum (8 stereo voices) is not guaranteed clean. Our 4-mono-voice load is a quarter of that channel count.
* A recent optimisation pass cut average audio CPU by about 36% on the Original Tracker (`UPCOMING.md:109`),
  mainly by letting silent tracks and idle FX sleep. No absolute baseline is given. That sleeping helps us
  less than it helps stock: four loops that never stop never sleep.
* The SD card and display work run in the main loop and are starved if the ISR runs hot. Pattern redraws
  already cost about 20-35 ms per step (`docs/developer_guide.md:43`), so pad/encoder responsiveness is the
  thing to watch with a new UI, not only audio dropouts.

## Budget table: RAM

Units: MiB = 1,048,576 B. "30 s" means 30 x 44,100 x 2 B = 2,646,000 B (2.52 MiB) mono 16-bit; at the real
hardware rate (44,117.6 Hz) it is 2,647,059 B, which makes no practical difference.

| Item | Size | Where | Confidence | Source |
| --- | --- | --- | --- | --- |
| SDRAM total | 16 MiB at `0x70000000` | external | High | `core/source/mtStructs.h:61-65`, `platforms/mk66/3rdparty/teensy_core/mk66fx1m0.ld:4-8` |
| SDRAM used by stock firmware | about 15,317 KB (93.5%) | external | Medium (doc figure from a baseline build, not re-measured) | `docs/memory_map.md:30` |
| SDRAM free after stock | **about 1,067 KB (1.04 MiB)** | external | Medium | `docs/memory_map.md:30` |
| Sample pool (`externalRamSampleMemory`) | 11,520 KB = **11.25 MiB** | external | High | `mtStructs.h:65`, `docs/memory_map.md:44` |
| 4 Parts x 30 s mono | 4 x 2.52 = **10.10 MiB**; about 1.15 MiB of the pool left over | in the pool | High (arithmetic) | derived |
| Sample Editor scratch (`externalRamEffectsBuff`) | 7.5 MiB at the top of the pool: `EDIT_BUFF_OFFSET = 11.25 - 4 x 1.875 = 3.75 MiB`. Anything above 3.75 MiB in the pool is overwritten by an edit | in the pool | High | `mtStructs.h:82,87`, `docs/memory_map.md:46,56-58` |
| Consequence | Parts only fit in the pool if the editor/effects path is removed or its buffer moved. With it present, only 3.75 MiB (one 30 s Part and a bit) is safe | | High | the rows above |
| Recording buffer | Stock recorder writes straight into the **start of the whole sample pool**, with the full `SAMPLE_MEMORY_SIZE` as its limit. It allocates no separate buffer. Its queue is 10 audio blocks (about 2.6 KB per channel) in internal RAM | pool | High | `core/interface/sampleRecorder/fSampleRec.cpp:1573-1575`, `core/source/Recorder.cpp:20-28,84-160`, `record_queue.h:31` |
| Consequence | The recorder must be pointed at the target Part's own 30 s region (or the free tail of the pool). A separate 30 s staging buffer (2.52 MiB) does not fit in the 1.04 MiB free space | | High | arithmetic |
| Master delay | 1.5 MiB (3 lines x 262,144 samples x 2 B). Max delay 3.5 s is about 154,000 samples, so a line cannot be halved to 131,072 without cutting max time | external | High | `effect_shortDelay.h:5-7`, `mtStructs.h:75` |
| Reverb | 32 KB `.reverb_memory` + 32 KB `internalRamReverbBuff` (both in SDRAM on MK66) | external | High | `effect_polyverb.cpp:13`, `mtStructs.h:76` |
| Shared scratch (`externalRamScratchBuff`) | 768 KB: `.ptfx` plugin area, SD preview stream, file browser, exporter | external | High | `mtStructs.h:77`, `docs/memory_map.md:56-72` |
| SD streaming (`VoiceStreamManager`) | Per-voice ring of 16,384 frames (32 KB) x 8 voices, plus pre-caches and loop-wrap buffers (MK66 is mono only: `kMaxChannels = 1`). Removable if streaming is dropped | external | Medium (I did not sum the total) | `VoiceStreamManager.h:12-23` |
| `g_wtEnvelopeBank` | 64 KB | external | High | `docs/memory_map.md:50` |
| Free if editor, `.ptfx`, streaming and file manager are all removed | 1.04 MiB + 0.75 MiB scratch + stream buffers + cold UI buffers. My guess is roughly 2-3 MiB, enough for a 5th 30 s buffer (2.52 MiB) only at the high end | external | **Low (guess)** | only a real linker report can tell |
| Internal SRAM free (heap + stack) | about 80.6 KB of 256 KB. `CHANGELOG.md:106` still says about 128 KB, an older figure; trust `memory_map.md` | internal | Medium | `docs/memory_map.md:29` |
| Audio block pool | 70 x 260 B = about 18 KB, internal. Enough for stock's 8 voices; a trimmed 4-voice graph needs fewer | internal | Medium | `mtAudioEngine.cpp:182` |
| Flash free | about 144 KB of 1 MiB. Removing the old UI and editor frees flash, but a new UI uses some back | flash | Medium | `docs/memory_map.md:28` |

## Design hazards found in stock (things the plan must change)

1. **Recording freezes per-voice updates.** `audioEngine::update()` only runs `instrumentPlayer[i].update()`
   when `!recorder.isRecordingActive()` (`mtAudioEngine.cpp:275-304`). Stock stops the sequencer before it
   records (`fSampleRec.cpp:306,460-462,1431`), so it never plays and records together. In our product, Parts
   that keep looping while a new take is recorded would lose envelope, end-of-play and parameter updates for the
   whole take. The loop player nodes themselves keep running (they are in `update_all`), but this guard has to go.
   High confidence that the guard exists; medium on how audible the effect would be.
2. **Entering the stock recorder clears delay and reverb tails** (`fSampleRec.cpp:464-465`). That would cut the
   master FX if the code is reused as is.
3. **Auto streaming.** Samples of 256 KB or more are streamed from SD by default at import
   (`mtStructs.h:781`, `fSampleImport.cpp:1561`, `SampleImport.cpp:485`, `fileManagerInstrument.cpp:563`).
   A 30 s Part is 2.5 MiB, so any path through stock instrument creation must force "resident".
   Parts recorded straight into RAM do not go through import; anything loaded from SD does.
4. **Instrument model is heavy.** Voices are driven through `mtProject.instrument[]` and the sequencer
   (`playerEngine::noteOn` reads `mtProject.sampleMemory[idx].isStereo` and sets up many envelopes). Four Parts
   plus pad hits will still need a minimal instrument record per Part. I did not trace every dependency.
5. **Voice count is not a problem.** 4 Parts + pad hits + a metronome node (`mtAudioEngine.h:115`) fits in the
   8 voices.

## Ranked cost drivers (my estimate, to be verified)

1. Master reverb (float, SDRAM-resident delay lines on MK66). Never sleeps while audio is going in.
2. The four Part chains (per-voice filter, pan amps, RMS meters, send mixers). The two RMS meters and the
   bit-depth crusher per voice are the easiest to delete.
3. Display/UI work in the main loop. Not audio CPU, but it competes for the same core.
4. Limiter, then delay, then line-in gate/meters.
5. SDRAM read latency on the Part loops (unknown; could move item 2 up).

## What can only be measured on real hardware

Everything in the CPU table is a guess until these are done. Use an SD card with an empty `/.develop` file and read
min/max/avg/last on the Config screen (`docs/developer_guide.md:259`).

1. Audio ISR percent with the full target scene: 4 looping mono Parts, line-in monitor on, a recording
   running, pad hits sounding, delay and reverb on at high settings, limiter on. Max and avg, over several
   minutes. Anything near 85% (the firmware's own yellow level, `titleBar.h:34`) is too tight.
2. The same scene with reverb off, to get the true reverb cost on MK66.
3. One Part alone vs four, to get a real per-voice cost, with filter on and off, and pitch 1.0 vs other.
4. Parts in SDRAM vs a copy in internal RAM (if a short one fits) to find what SDRAM read latency costs.
   Whether `0x70000000` goes through any cache is unknown to me.
5. Peak audio-block use (`AudioMemoryUsageMax()`, `AudioStream.h:122-124`) to confirm 70 blocks is enough.
6. Main-loop responsiveness (pad latency, display redraw time, SD writes at the end of a take) while the ISR is
   at its measured maximum.
7. Real linker output (`make all` prints it, `docs/memory_map.md:10`) after the editor, `.ptfx`, file browser
   and streaming code are removed, to see how much SDRAM and flash are really free.
8. Recording into a region that is also being played (a Part overdub): glitches, SDRAM bus contention, and any
   click where the loop wrap meets the recorder's write head.
9. Line-in monitoring latency. Input DMA block plus output DMA block puts a floor of about 2 x 2.9 ms in the
   path (reasoned from the double-buffered 128-sample DMA, `platforms/mk66/3rdparty/Audio/input_i2s.cpp:30`,
   `output_i2s.h:60-61`); not measured, and it may matter for a live monitor. The codec can also route line-in
   to the headphones in hardware; I did not check whether this firmware exposes that.
10. USB audio on vs off during the test. The maintainers say it can max out the CPU (`UPCOMING.md:57`).

## Where I am guessing, plainly

* Every cycle and percent figure in the CPU table, including the 35-60% total.
* That reverb is the biggest item (reasoned from its code, not measured).
* How much extra SDRAM removing the editor, `.ptfx` and streaming would free (2-3 MiB is a guess).
* Whether SDRAM reads are slow enough to hurt four looping voices.
* That a trimmed graph can drop the per-voice RMS and bit-depth nodes without side effects. Other code reads the
  RMS values for the UI, which is being replaced anyway, but I did not trace every consumer.
