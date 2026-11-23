# Audit E: stock v1.0.0 capabilities we may have missed or wrongly ruled out

Scope: read-only sweep of `core/`, `platforms/mk66/`, `CHANGELOG.md`, `docs/`, plus `wayfinder/tickets/01-core-workflow-and-device.md` and `docs/readiness-matrix.md`. Nothing was built, run or measured. There is no git history to diff against stock v1.0.0 (one commit), so "ours" is identified by reading: `core/interface/fourTrack/`, `core/source/FourTrackSession.*`, `core/libs/Audio/play_loops.*`, and the `setFirstBlockCallback` hook in `core/source/Recorder.*`.

Confidence tags: **[V]** read directly in the source and cited. **[I]** inferred from the code, not confirmed. **[?]** could not be determined. All paths are relative to the repo root.

Impact labels: **REUSE** (use it instead of building), **RELEVANT** (changes a decision), **NO IMPACT**.

---

## Top ten things we would have missed (ranked by how much they change the plan)

1. **Streaming does not make the pad bank free, and it also does not stop us from streaming the Parts.** The ticket 01 correction ("memory is therefore not the limit for a pad bank") over-corrects. [V]
   - Only samples of 256 KB or more, in One-Shot or the three Loop modes, stream (`core/source/mtStructs.h:775-791`, `core/FileManager/fileManagerInstrument.cpp:144-146`). Anything under 256 KB (about 2.97 s of mono) is fully resident, and so are Slice, Beat Slice, Granular and Wavetable.
   - A streamed sample still takes a 32,768-frame pre-cache (64 KB) in the sample pool (`mtStructs.h:776`, `fileManagerInstrument.cpp:148-152`). 48 streamed pads is 3 MiB of pool. Four 30 s Parts leave about 1.15 MiB (`docs/research/cpu-ram-budget.md:99`), roughly 18 streamed pads or about 13 s of short hits in total. The 24 s test build (`fFourTrack.cpp:27-29`, 26 s regions) leaves about 2.6 MB.
   - The opposite opportunity: the same machinery could play the Parts themselves from the SD card (64 KB of RAM each), so "four Parts, about 30 s each, all in RAM" is a choice, not a stock limit. Only the take being recorded has to be in RAM. Limits that do apply: mono only on the Original (`core/AudioEngine/VoiceStreamManager.h:17`), 8 streaming voices (`:12`), and SD speed, which nobody has measured. Whether a plain WAV Part can be given to the streamer without going through the `.pti` instrument loader is not determined [?].
   - Plan change: write the pool budget as "Parts + 64 KB per streamed pad + all short hits", and decide explicitly whether Parts stream.

2. **Stock already has a transport and clock, and our player bypasses it.** [V]
   - `Sequencer` tempo is a float, 40 to 800 BPM (`core/source/mtSequencer.cpp:48-49`, `mtSequencer.cpp:2215-2231`), so a fractional derived BPM is representable. The sequencer sends MIDI clock, Start and Stop (`mtSequencer.cpp:347`, `core/source/mtMidi.cpp:379-392`), follows external clock, Continue and Song Position (CHANGELOG, MIDI Overhaul), and the master delay has tempo sync.
   - MIDI clock out is only emitted from the sequencer tick. Our `AudioPlayLoops` and `FourTrackSession` never touch it, so "MIDI clock out", "delay synced to the loop" and "click" are all unbuilt unless we run the sequencer (even with an empty pattern) at the derived tempo as the loop clock. That is a real architectural option we have not weighed.
   - The click already exists: `engine.makeMetronomeTick(accent)` (`core/AudioEngine/mtAudioEngine.cpp:1420-1445`), on its own mixer input, with an embedded 8 kHz click. But the sequencer only drives it when `SR->recordInProgressFlag == 1` or a pre-roll is active (`mtSequencer.cpp:183-185`), so it is tied to the Sample Recorder screen. We must call `makeMetronomeTick` ourselves.
   - The stock metronome has a time signature of 1 to 12 beats by 1 to 12 steps (`configMenu.cpp:179-180`, `mtSequencer.cpp:3408-3414`). "4/4 only" is our choice, not a limit.

3. **A first-take auto-trim already exists, and "Recording Threshold" is not an audio threshold.** [V]
   - `cSampleRecorder::calculateStartPointFromRecording` and `...EndPoint...` (`core/interface/sampleRecorder/fSampleRec.cpp:3026-3130`, called from `calculatePointsAfterRecording`, `:3198`) scan for the first and last sample above -70 dBFS with a 1 ms attack. `Recorder::crop` and a 5-level crop undo (`undoCount = 5`, `sampleRecorder.h:49`, `fSampleRec.cpp:1645-1710`) sit beside them. Ticket 04's research looked only at `mtSliceDetector` and concluded nothing like it existed (`docs/research/onset-detection.md:120-137`).
   - Limits: the result is a 16-bit fraction of the length (about 20 samples of resolution on a 30 s take), and -70 dBFS will trigger on hum. It is a fit for "trim the first take to the first note"; it is not a replacement for the Alignment Window detector.
   - Config items named in the brief: "Recording Threshold" is the timing window for live pattern-note recording during a pre-roll, in micro-timing units (`configMenuDefs.h:267-268`, `mtSequencer.cpp:2650`). It has nothing to do with the audio level. "Pre roll" is the Metronome count-in switch (`configMenu.cpp:188-192`, `mtSequencer.cpp:1653-1680`). There is no stock threshold-triggered audio start. The only auto-stop is memory full (`Recorder.cpp:149-162`).

4. **The audio codec has a hardware compressor/limiter that v1.0.0 wires up and never turns on.** [V for the code, ? for whether it works]
   - `mtAutoVolumeControl` (`core/AudioEngine/MasterEnhancer/mtAutoVolumeControl.h`) drives the SGTL5000 AVC: threshold -96 to 0 dB, attack and decay 2 to 32 dB/s, response 0/25/50/100 ms, compressor or hard limiter. The MK66 codec impl exists (`platforms/mk66/source/Sgtl5000Impl.cpp:45-65`). Nothing calls it (`mtMasterEnhancer::enable()` only sets bass enhance and disables surround). It costs no MCU time.
   - Caveats found in the code: `Sgtl5000Impl::configureAutoVolume` passes `maxExpanderGainType` as both the max-gain and the response-time argument (`Sgtl5000Impl.cpp:59-60`), and the driver stores threshold, attack and decay in `uint8_t` variables (`platforms/mk66/3rdparty/Audio/control_sgtl5000.cpp:967-969`), which looks like truncation of values that need 16 bits. Whether it is musical, and whether it sits on the headphone and line-out paths, needs a device test.
   - Ticket 05 says glue compression "must be written". It may not have to be, or this can at least be tried first.
   - Related: the master limiter already has a gain-reduction readout (`fMasterParams.cpp:1602-1630`, `AudioEffectLimiter::getAverageGainChange`). Readiness matrix findings 11 and 25 ("needs a gain-reduction readout") are already met by stock.

5. **Stock sample voices already give live, per-voice lo-fi, so "wow/flutter, bit-crush, saturation must be written" is only true for line-in and our custom player.** [V that they exist, ? on depth and rate ranges]
   - Per instrument: overdrive, bit depth (4 to 16, `AudioBitDepth`), filter, reverb and delay send, and automation with envelopes, LFOs and (new in 1.0.0) wavetable-shaped LFOs on volume, pan, cutoff and **finetune** (`mtStructs.h:244`, `:421-455`, CHANGELOG item 3). A pitch LFO on finetune is a wow/flutter generator. I did not read its depth or speed limits.
   - Master mixer saturation has five curves (already known). Offline bit-crush already has bits and sample-rate (`effector/mtEffectBitcrusher.cpp:20-80`). The matrix note that sample-rate reduction is missing is only true for a live version.
   - If Parts were played as stock loop instruments instead of `AudioPlayLoops`, they would also get per-track volume, pan, sends, mute, the 8-track mixer CC map, and export stems. The cost is the stock voice path (8 voices, 16-bit loop points, see item 6). This is a design fork we have not stated.

6. **MIDI and pad facts that change the controls plan.** [V]
   - Pads send no velocity and no aftertouch: `onPadPush` always passes velocity 0 (`core/source/mtActions.cpp:56`). MIDI velocity is only honoured if Config > Recording options includes Velocity (`mtSequencer.cpp:2557-2560`); otherwise every MIDI note plays at 127.
   - Stock has no "pad n plays instrument n" trigger. In the pattern editor a pad can select instrument n (`fPatternEdit.cpp:4634`), and Instrument Edit moves the list cursor (`fIinstrEdit.cpp:2891-2897`); other screens play `lastUsedInstrument` at a pitch from the pad scale (`fIinstrEdit.cpp:2905-2912`, `fSampleImport.cpp:1597-1607`). The ticket 01 line "instrument slots 0 to 47 map one to one onto the 48 pads for Pad mode" overstates this: Pad mode needs new trigger code (small: set `lastUsedInstrument = pad`, then `handleNoteOn`).
   - MIDI notes in go to `lastUsedInstrument`, transposed by Middle C, through a mode switch (`mtMidi.cpp:90-134`, modes at `mtSequencer.h:111-116`). With our screen open the mode is the sequencer default, which writes to the current pattern selection. Our module has to set its own mode or the controller does odd things [I].
   - Stock CC map (`configMenuDefs.h:380-382`, `fMasterParams.cpp:3055-3098`): CC 71-78 track levels 1-8, 79 delay, 80 reverb, 81 dry mix, 82 line-in level; CC 41-48 and 51-62 performance mode; CC 1/3/5/7/9-15 etc. edit the last-used instrument. CC 71-78 move the stock track volumes, not our four Parts.
   - The Original build is USB MIDI **device** only (`platforms/mk66/makefile:44` has `-DUSB_MIDI`; I found no USB host code). A controller therefore has to come in through the MIDI In jack, not the USB port [I, from the build flags and absence of host code; the hardware itself I could not check].
   - Pad lights: 48 LEDs, front layer, back layer and blink, brightness 0 to 100 (`core/source/mtPadsBacklight.h:17-24`). No per-button LEDs found. Useful for track-state lights.

7. **Export is sequencer-bound and always stereo, 16-bit, labelled 44100 Hz.** [V]
   - `mtAudioExport::runSequence` starts the pattern or song (`core/Exporter/mtAudioExport.cpp:266-300`) and renders faster than real time, one master file or master plus track1-8, reverb and delay stems (`mtAudioExport.h:57-72`). The WAV header says 44100 Hz stereo (`mtAudioExport.cpp:364-368`) while the audio clock really runs 44117.65 Hz (`AudioStream.h:52`). The 1.0.0 change "exact 44100 Hz export" makes the file match real-time pitch. Result: a file from this device is labelled 44100 Hz and plays about 0.04% slow in a DAW relative to real time, so a DAW grid will drift by about 24 ms per minute. Mono export is not offered [I].
   - The loop player feeds the same mixer that the master tap reads (`mtAudioEngine.cpp:150-157,163-165`), so a master capture would include it, but the exporter's driver is the sequencer. "Export with 1 to 16 repeats" needs either a sequencer-driven loop or a small new render loop. REUSE the WAV header and block-writing code, not the driver.
   - Related, known to the team already: the audio rate is not exactly 44.1 kHz (`cpu-ram-budget.md:29`). Any BPM derived from frame counts must use 44117.647 (our test build uses 44118, `fFourTrack.cpp:23`).

8. **Power loss and battery behaviour will lose RAM-only Parts.** [V]
   - Holding power runs a power-off sequence that saves the stock project to the Workspace (`core/interface/core/interface.cpp:283-300`). A battery below 20% (not charging) only shows a popup (`core/source/mtActions.cpp:85-88`, `platforms/mk66/source/BatteryImpl.cpp:131-137`). Empty battery calls `goLowPower()` immediately, with no save (`mtActions.cpp:79-83`).
   - Stock also autosaves the project struct every 10 s (`core/FileManager/fileManager.cpp:346-353`). That autosave writes stock project data, not our Parts, and it is background SD traffic during recording and streaming [I]. Our hooks go in `onBatteryLow`, `onBatteryEmpty` and `processPowerOffSequence`; the SD-removed screen already exists (`interface.cpp:309-330`).

9. **Stock sample point positions are 16-bit fractions of the sample length.** [V]
   - Start, end and loop points in playback are `uint16_t` positions (0 to 65535) turned into sample indexes, with optional snap to zero crossing (`core/libs/Audio/play_memory.cpp:701-720`, project option "Snap to zero", `configMenu.cpp:84`). On a 30 s Part this is about 20 samples (0.5 ms) per step. If Parts are played through stock voices, sample-accurate Offset cannot be expressed as a point; it has to be done by moving the buffer pointer, as `AudioPlayLoops` does. Not a blocker, but it argues against "just use loop points".

10. **Stock tools that fit a recording to a tempo exist, but they are coarse and use the memory we are taking.** [V]
   - Sample Editor "Timestretch Beat" (steps and BPM, 40 to 800, cast to an **integer** BPM, `core/effector/mtEffectTimestretch.cpp:21`) and "Timestretch Note" (target length 0.1 to 8 s, `mtSampleEditorEngine.cpp:100-110`) are offline SOLA stretchers. They run in the edit buffers, which live in the upper 7.5 MiB of the sample pool (`mtStructs.h:85`, `Tracker.cpp:28`) and cap input at `EDIT_BUFF_SIZE/2` samples (about 42.7 s, `fSampleEdit.cpp:90`). The fractional tempo we derive cannot be typed into Beat stretch exactly. Useful only for fitting loaded drum loops to a loop; not for the Parts. This matches the "no audio is stretched" decision, and the edit buffers remain in conflict with Part memory (known).

---

## 1. Capability table

### 1.1 Recording

| Capability | What v1.0.0 does | Where | Impact |
|---|---|---|---|
| Threshold-triggered start | Not present for audio. "Recording Threshold" is note-timing for live pattern recording (-0 to -60, 16 steps). | `configMenu.cpp:149-153`, `configMenuDefs.h:130-147,267`, `mtSequencer.cpp:2650` | RELEVANT (name trap; do not reuse it) |
| Pre-roll / metronome count-in | "Pre roll" On/Off in Config > Metronome. Count-in is one bar (`numerator x denominator` steps) at project tempo, with a "Counting in" popup. In the Sample Recorder, recording starts at the end of the count-in via a callback. | `configMenu.cpp:188-192`, `mtSequencer.cpp:183-236,1653-1680`, `fSampleRec.cpp:105-125,1354-1366,2910-2913` | REUSE for the starting-tempo count-in (matrix rows 3, 21), with the screen-flag coupling noted in item 2 |
| Auto-stop | Only when memory is full, then a callback (our code uses it). | `Recorder.cpp:149-162` | NO IMPACT |
| Crop | `Recorder::crop` plus on-screen start/end points. | `Recorder.cpp:204-218`, `fSampleRec.cpp:1645-1685` | RELEVANT (item 3) |
| Undo levels | 5 levels of crop undo in RAM (addresses only, no audio copy). Take-level undo does not exist. | `sampleRecorder.h:49`, `fSampleRec.cpp:1688-1712` | NO IMPACT (our SD undo stands) |
| Auto-trim at start and end | -70 dBFS, 1 ms attack, 16-bit result. | `fSampleRec.cpp:3026-3207` | REUSE for first take (item 3) |
| Input source | Line in, FM radio, Mic low gain, Mic high gain (Original). Switching to mic mutes the headphones for a short timer. USB source is IMX only. | `sampleRecorder.h:6-60`, `fSampleRec.cpp:2660-2720`, `:253-261` | NO IMPACT (radio was dropped) |
| Gain | Stored 0 to 100, mapped to codec line-in level 0 to 15, mic low 0 to 25 dB, mic high 0 to 44 dB. Saved in the stock project values. | `fSampleRec.cpp:529-576`, `:2447-2488` | REUSE (matches ticket 01 ranges) |
| Stereo to mono | Config > General > "Line in channel": Stereo (L+R averaged), Mono Left, Mono Right. Recorder averages stereo to mono if the source is stereo. Mic sources are mono. | `configMenuDefs.h:99-103`, `Recorder.cpp:62-75,110-126` | REUSE (a guitar on one channel needs Mono Left or Right) |
| Monitoring | Software monitor via the line-in mixer input, On/Off in the recorder. A hardware-direct headphone source (`kAudioInput`) exists in the codec API but is only used in the factory test. | `fSampleRec.cpp:578-590`, `mtTest.cpp:1426`, `Platform.h:258` | RELEVANT (zero-latency headphone bypass is available, but I believe the codec headphone switch is either-or, so it cannot also play the other tracks [?]) |
| Level meter | RMS analyzers on input and per channel. | `AudioInput.cpp:7-48`, `fSampleRec.cpp` (levelBar) | REUSE (already used in `fFourTrack.cpp:489`) |
| Input gate | A noise gate (`AudioEffectDynamics`) on line-in; defaults compression and limit are also enabled [?, not verified]. | `AudioInput.cpp:48`, `mtAudioEngine.cpp:265-266` | RELEVANT (explains line-in monitor gating) |
| Sample-accurate start | Not stock. Block-rounded (128 samples), as ticket 03 says. Our callback arms at the first block. | `Recorder.cpp:127-140` | known |

### 1.2 Tempo and timing

| Capability | What v1.0.0 does | Where | Impact |
|---|---|---|---|
| Project tempo | `globalTempo`, float, 40 to 800 BPM (tempo FX max 200). Step = a 16th, 576 ticks per step. | `mtSequencer.cpp:48-53,2215-2272`, `mtStructs.h:889` | RELEVANT (range limits derived tempo: very short or very long loops fall outside) |
| External MIDI clock sync | Clock in from USB or jack, tempo measured over 48 pulses (10 to 999 BPM), Start/Continue/Stop/SPP, "Clock sync correction". | `mtSequencer.h:943-1050`, `mtMidi.cpp:395-515`, `mtStructs.h:50-51`, CHANGELOG MIDI Overhaul | RELEVANT (item 2) |
| MIDI clock out | Sent from the sequencer tick only. | `mtSequencer.cpp:347`, `mtMidi.cpp:379` | RELEVANT (item 2) |
| Tempo from sample length / beats in a sample | Not present. Beat Slice and Timestretch Beat take a tempo from the project or the user, not from the sample. | `mtEffectTimestretch.cpp:12-36`, `fSamplePlay.cpp` | NO IMPACT |
| Timestretch | Offline SOLA, Beat (integer BPM, steps, micro-step) and Note (0.1 to 8 s). | `effector/mtEffectTimestretch.*` | RELEVANT (item 10) |
| Snap to zero crossing | Project option, playback points only, threshold +/-10. Not a recording tool. | `play_memory.h:225`, `play_memory.cpp:63-96,701-720,813`, `mtAudioEngine.cpp:1613` | REUSE the 10-line zero-crossing test for the detector (ticket 04 already notes this) |
| Time signature | 1 to 12 / 1 to 12 in Config > Metronome. | `configMenu.cpp:179-198` | RELEVANT (4/4-only is a choice) |
| Pattern length | Up to 128 steps per pattern (`MAXSTEP = 127`), song chain up to 255 patterns. | `mtSequencer.h:69,332`, `mtStructs.h:146` | relevant only if the sequencer is used as the loop clock: a 32-bar loop is four patterns |

### 1.3 Metronome and click

See item 2 and the pre-roll row. The click is `makeMetronomeTick(accent)` with volume `mtConfig.metronome.volume` (0 to 100) and an accent by pitch. Beat = every `denominator` steps, bar = `numerator x denominator` steps. State On/Off applies during recording and playback. Route: its own `finalMixer` input, after the limiter (`mtAudioEngine.cpp:721-724`, `:1420-1445`), so the click is **not** limited and, per `pad-recording-path.md:45`, is present on the master queue (it would be in an export or master capture) [I].

### 1.4 Loop playback, slicing

| Capability | What v1.0.0 does | Where | Impact |
|---|---|---|---|
| Loop modes | Forward, Backward, Ping-pong, One-shot (instrument play modes). Streamed loops pre-fetch the wrap point. | `mtStructs.h:386-396`, `VoiceStreamManager.h:19-21`, `docs/developer_guide.md` section 3 | RELEVANT (Part playback via stock voices is possible, item 5) |
| Seamless looping of RAM samples | Forward loop in `play_memory_loop_forward.cpp`; I did not verify click-free behaviour at the wrap. | `libs/Audio/play_memory_loop_forward.cpp` | [?] |
| Slicing | Auto-slice (`mtSliceDetector`, up to 48 slices), Slice and Beat Slice play modes, "Play Thru" (new). Slice modes stay fully in RAM on the Original. | `mtSliceDetector.h`, `mtStructs.h:339,780-790`, CHANGELOG 7 | NO IMPACT |

### 1.5 MIDI and pads

See item 6. Other facts: MIDI out can send notes, CC, pitch bend, aftertouch, program change over USB and the jack (`mtMidi.cpp:520-728`); Middle C offset is configurable; MIDI note in has a channel filter (`mtMidi.cpp:85-88`); USB and jack in/out modes are separate for notes and CC. Pads: `onPadHold` exists (long press), so a pad long-press gesture is available (`mtActions.cpp:64`).

### 1.6 Live effects and master chain

Verified and consistent with `docs/research/master-fx-availability.md`; additions in bold.

| Item | Status on MK66 | Where | Impact |
|---|---|---|---|
| Master reverb, delay (ping-pong, **tempo sync**, filter), limiter (Subtle or Extreme, sidechain, **gain readout**), 5 saturation types | Live, on the master bus; all editable in the Master screen | `masterParams.h:49-75`, `mtAudioEngine.cpp:503-515,1550-1556`, `configMenu.cpp:81` | REUSE |
| Bass boost, stereo widening, 5-band EQ | Hardware, in the codec | `MasterEnhancer/*`, `Sgtl5000Impl.cpp:67-120` | REUSE (zero CPU) |
| **Codec AVC compressor/limiter** | Code present, not enabled | item 4 | RELEVANT |
| Chorus, flanger, compressor, bit-crusher, EQ, limiter, overdrive, delay, normalize, reverse, fades | Offline only, in the Sample Editor, non-live | `mtSampleEditorEngine.cpp:21-38` | NO IMPACT live (templates only) |
| `.ptfx` plugins | Offline, MK66 code cap 32 KB | `docs/modular_effects_system.md` | NO IMPACT |
| Per-voice live effects | Overdrive, bit depth, filter, sends, finetune LFO, WT LFOs | item 5 | RELEVANT |
| "Extra headroom" and "Limiter mode" | Project settings that change loudness on playback; they live in the stock project values. | `configMenu.cpp:81-84`, `mtAudioEngine.cpp:1610` | RELEVANT: our own project format must set them or levels will differ from the stock default |
| Performance mode | Per-track live FX on sample tracks (reverse, filters, etc.) and global FX, MIDI CC control | `core/interface/performanceMode/*` | NO IMPACT (we drop it); I did not read the FX list [?] |

### 1.7 WAV import and export, projects, USB, power, screenshots

| Capability | What v1.0.0 does | Where | Impact |
|---|---|---|---|
| WAV import | 8, 16, 24, 32-bit PCM and 32-bit float, mono or stereo, any rate. Converted to mono 16-bit at the device rate; channel choice Left, Right or L+R; optional half-rate "lo-fi" import (tune -12). | `AudioDataUtilities.cpp:125-157`, `SampleImport.cpp:13,370-405,454-545`, `FileManagerOperation.h:19-24` | REUSE for pad samples (no format work needed) |
| Sample export | "Export samples" writes project samples as WAV. | `fileManagerExport.cpp:292-473` | REUSE for single-sample export |
| Mix export, stems, "bounce" | Render master or stems from the sequencer (item 7). No bounce or resample of a track into a sample, except the Sample Editor's offline apply. | `mtAudioExport.*` | RELEVANT |
| Recording save | Saves the take into a `.pti` instrument, not a plain WAV. | `RecordingSave.cpp:15-50` | RELEVANT: our "mono WAV Part" plan means new write code (the `strWavFileHeader` writer is reusable) |
| Project save/load | Header-only copy-on-write instruments, incremental save, autosave every 10 s. Stock structs are size-pinned (ticket 16). | `developer_guide.md` section 3, `fileManager.cpp:346` | known; autosave interplay in item 8 |
| USB audio, USB storage | **Not available on the Original in this tree.** `UsbAudioImpl` is an empty stub; `UsbImpl` methods are empty; the Config USB menu exists only for IMX; the build defines `USB_MIDI` only. | `platforms/mk66/source/UsbAudioImpl.h`, `UsbImpl.h`, `configMenu.cpp:49`, `makefile:44` | RELEVANT: ticket 01 and the brief do not promise it, but do not plan any USB audio or storage feature |
| Headphone vs line out | Separate volume and mute settings and line-out levels; no jack-detect logic found. | `mtStructs.h:483-484,1031-1040`, `mtAudioEngine.cpp:1019-1030` | NO IMPACT |
| Low battery, power off | Item 8. | | RELEVANT |
| Screenshots | Hold Shift and press Power: saves `Snapshots/Snapshot_NNN.bmp` (800x480, 32-bit) on the SD card, from any screen including ours. | `interfaceInputs.cpp:62-71`, `libs/NewDisplay/snapshots.cpp:1-60` | REUSE: easy way for the owner to send real device screens back to the design AI |
| Sample Editor effect list | 18 entries incl. Timestretch, Bitcrusher, Compressor; offline only | `sampleEditor.h:11-21` | NO IMPACT |
| Image viewer, themes, fonts | Present; not relevant | CHANGELOG 4, 11 | NO IMPACT |

---

## 2. Hard statements in ticket 01 and the readiness matrix: does v1.0.0 actually impose them?

### 2.1 `wayfinder/tickets/01-core-workflow-and-device.md`

| Statement | Imposed by v1.0.0 code? | Evidence |
|---|---|---|
| "Four mono tracks, ~30 s each, all in RAM" (line 12) | **No.** It is our design. The pool is 11.25 MiB but the top 7.5 MiB doubles as the edit buffers (see 2.3); the arithmetic (about 2.52 MiB per 30 s Part) is right. Streaming could hold Parts on SD (item 1). | `mtStructs.h:65,85`, `VoiceStreamManager.h:12-17` |
| "No synths, no pitch changes, no bounce in v1" | **No.** Stock voices pitch by note; bounce is simply not built. Our choices. | |
| "No audio is stretched" | **No.** Stock has offline stretch (item 10); the decision stands on its own merits. | |
| "4/4 only for v1" | **No.** Stock metronome supports 1 to 12 over 1 to 12 steps. | `configMenu.cpp:179-198` |
| "Bar choices 1, 2, 3, 4, 6, 8, then 12 to 32" | **Partly.** Tempo must land in 40 to 800 BPM for the stock clock and metronome, so a 1-bar 8-second loop (30 BPM) or a 1-bar 0.25 s loop (960 BPM) cannot be a stock tempo. Not checked against the bar list. | `mtSequencer.cpp:48-49` |
| "Samples of 256 KB or more keep only about 0.74 s in memory (64 KB, mono on the Original) in One-Shot and Loop modes. Slice, Beat Slice, Granular and Wavetable stay fully in memory" | **Yes, correct.** | `mtStructs.h:775-791`, `fileManagerInstrument.cpp:144-146` |
| "Memory is therefore not the limit for a pad bank" | **Overstated.** Short samples and the 64 KB pre-caches use pool memory (item 1). | |
| "The limits are 48 instrument slots, 8 simultaneous streaming voices, SD speed (not measured)" | **Slots and voices: yes** (`kMaxInstruments = 48`, `kVoiceCount = 8`, `kVoices = 8` at `mtAudioEngine.h:40`). The 8 voices are shared with all other sample voices, not a separate pool [I]. Missing from the list: pool use, 16-bit points, mono-only streaming. | `VoiceStreamManager.h:11-12` |
| "Stock instrument slots 0 to 47 map one to one onto the 48 pads for Pad mode" | **Partly.** True for selecting a slot in pattern edit; there is no one-pad-one-sample trigger (item 6). | `fPatternEdit.cpp:4634` |
| "Line gain about 0 to +22 dB in 16 steps; mic low 0 to +25; mic high 0 to +44" | **Yes for mic and for 16 steps** (`gain * 15 / 100`). The +22 dB figure was derived by the ticket, not stated in the repo [?]. | `fSampleRec.cpp:529-562` |
| "Feedback protection: ... mic monitoring starts off; line monitoring starts on" | **Our design, not stock.** Stock monitor defaults to Off (`defaultRecorderConfig.monitor = 1`, list index 1 is "Off", and monitor=1 sets the line-in monitor volume to its minimum). Stock mutes the headphones briefly on switching to a mic. No stock feedback guard beyond that. | `sampleRecorder.h:37-56`, `fSampleRec.cpp:578-581,253-261` |
| "Undo keeps the previous take on the SD card because memory cannot hold it" | **Consequence of our layout**, not stock. Stock has 5 crop-undo levels but no take undo. | `sampleRecorder.h:49` |
| "MIDI input must trigger the same sounds as the pads" | **Not automatic.** MIDI notes go to `lastUsedInstrument` through a screen-dependent switch, and velocity is flattened without Record Velocity (item 6). Pads have no velocity at all, so "same sound" is easy, "same dynamics" is not. | `mtMidi.cpp:90-134`, `mtSequencer.cpp:2557` |
| "No bounce" / "Export: 1 to 16 repeats" | Export is stock-driven by the sequencer (item 7). | |
| "Part Editor: trim start and end, offset, fades, normalize" | Stock Sample Editor has Cut, Fade In, Fade Out and Normalizer, but offline and in the edit buffers (item 10). Reusable as algorithms. | `mtSampleEditorEngine.cpp:21-38` |
| "Lo-fi ... per-track effects only if the CPU allows" | Per-voice overdrive and bit depth already run on every stock voice; that CPU is already in stock's cost [I]. | item 5 |
| "Small text readouts may update about ten times a second" | Not a v1.0.0 limit; see `docs/research/display-pipeline-and-motion.md`. Not re-checked here. | |

### 2.2 `docs/readiness-matrix.md`

| Statement | Imposed by v1.0.0? | Notes |
|---|---|---|
| Row 3 and 21: "Needs a click sound source" | **No. A click exists** (item 2). | `mtAudioEngine.cpp:1420` |
| Row 11 and 25, finding in row 11: "needs a gain-reduction readout from the limiter" | **No. It exists.** | `fMasterParams.cpp:1602-1630` |
| Row 17: "Lo-fi master chain: mostly to be written" | **Mostly true for the master; the codec AVC and per-voice lo-fi are not accounted for** (items 4 and 5). | |
| Row 14, 15: "Stock WAV export exists" | **Yes, but sequencer-driven, stereo only** (item 7). | |
| Row 19: "SD-missing is not in the simulator" | Stock has the screen and the removal hook. | `interface.cpp:309-331` |
| Evidence note 1: "recording is rounded to a 128-sample block, triggered from the screen loop" | **Yes**: `recorder.update()` is called from the engine update (main loop), pulling 128-sample blocks. | `mtAudioEngine.cpp:288`, `Recorder.cpp:111-140` |
| Evidence note 3: "Four 30 s Parts take about 10.1 of 11.25 MiB" | Arithmetic is right. The 11.25 MiB pool is shared with the edit buffers. | `cpu-ram-budget.md:98-101` |
| Evidence note 4: "only a gate-style compressor, crude saturation and bit-crush exist" | Incomplete: add codec AVC, per-voice effects, tempo-synced delay (items 4, 5). | |
| Finding 2: "Four 30 s Parts use 10.1 of 11.25, so an undo copy does not fit in RAM" | **Yes, if all Parts are RAM-resident.** Not true if Parts stream (item 1). | |
| Finding 4: "the recorder has no internal source today" | **Yes** (sources are line, radio, mic; internal pad capture needs new work). | `sampleRecorder.h:6-60` |
| "Stock effects: only a gate-style compressor ..." | See above. | |
| Row 22: "4/4 only" | Our choice (above). | |
| "Six readiness checks" and "no measurement on device" | A self-description, true. | |

### 2.3 One conflict inside the docs, not resolved

`docs/developer_guide.md:19-30` says the Original's 11.25 MiB is "a 7.5 MB resident mono pool plus a 3.75 MB FX edit buffer". The code reads the other way: `EDIT_BUFF_OFFSET = SAMPLE_MEMORY_SIZE - 2 * SAMPLE_CHANNELS * EDIT_BUFF_SIZE` is 3.75 MiB (`mtStructs.h:85`), so the edit buffers occupy the **upper 7.5 MiB** and anything above 3.75 MiB is overwritten by an edit, as `cpu-ram-budget.md:100` says. I could not determine what stock does with samples that sit above 3.75 MiB when the Sample Editor opens. It does not change our plan (we remove the editor), but the developer guide should not be quoted for the pool size.

---

## 3. What I could not determine

- Whether a plain WAV Part file can be handed to the streaming path without becoming a `.pti` instrument (item 1). The setup call is `VoiceStreamManager::setupInstrumentStream(instr, path, dataOffset, frames, stereo)`, but I traced only its callers in the instrument loader.
- Whether the SGTL5000 AVC acts on the final output and is audible (item 4). The driver's `uint8_t` truncation was read, not tested.
- The depth and speed limits of the instrument finetune LFO and whether the sound is usable as wow/flutter.
- Whether the codec headphone source switch can mix, or only choose between DAC and line-in. I believe it is exclusive, from general knowledge of the part, not from this tree.
- Whether the stock autosave (10 s) writes anything while the four-track screen is open and nothing changed. `saveProjectToWorkspace` was not read in full.
- Whether the Original's USB port has host hardware at all. No host code is present; that is all I can say.
- Whether a click at the loop wrap occurs in stock forward-loop playback. Not checked.
- Everything is unmeasured: CPU, SD throughput, latency. No stock behaviour here was run.
- Performance mode FX list, Config persistence of MIDI settings between firmwares, headphone jack detection: not read in depth.

## 4. Stock quirks that would bite if reused

- `Sgtl5000Impl::configureAutoVolume` passes `maxExpanderGainType` twice (`Sgtl5000Impl.cpp:59-60`).
- AVC driver `uint8_t` register values (`control_sgtl5000.cpp:967-969`).
- Timestretch Beat tempo is `uint16_t` (`mtEffectTimestretch.cpp:21`).
- Metronome click timing depends on `SR->recordInProgressFlag`, a stock Sample Recorder flag (`mtSequencer.cpp:183`), and a pre-roll end callback that only fires if that flag is set (`:217-222`). Our screen never sets it.
- Autosave every 10 s uses the stock project struct (`fileManager.cpp:346-353`).
- Recorder monitor polarity is easy to misread: `monitor = 1` is the label "Off" and sets the monitor volume to the minimum (`sampleRecorder.h:37-40`, `fSampleRec.cpp:580`). Wire a quick-monitor toggle against the label, not the number.
