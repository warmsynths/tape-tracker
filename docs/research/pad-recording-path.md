# Recording live pad playing as audio (original Tracker, MK66)

Ticket: `wayfinder/tickets/15-research-pad-recording-path.md`. How do pad samples reach the output, can the recorder take the internal mix or only the pad voices as a source, what must change, what does it cost, does timing stay sample-accurate against the loop, and does any code already resample internal audio.

Method: read the firmware source only. Nothing was built, run or measured. All paths are relative to the repo root. Anything marked **GUESS** is my own estimate, not something the code or a measurement says. Line numbers are as read in this checkout.

## 1. Short answers

| Question | Answer | Confidence |
|---|---|---|
| Can the stock recorder take an internal source? | No. `Recorder::record()` only knows two sources, USB and I2S (`core/source/Recorder.cpp:49-62`, enum at `core/source/Recorder.h:14-18`). Both are external inputs. The UI list has line, radio, mic and USB only (`core/interface/sampleRecorder/sampleRecorder.h:9-13`). | High |
| Does the audio graph already have internal taps? | Yes. The offline exporter wired a record queue onto every voice, the reverb, the delay and the final mix, for stems (`core/AudioEngine/mtAudioEngine.cpp:679-746`, `core/AudioEngine/ExportQueues.h:7-23`). They are the same `AudioRecordQueue` class the recorder reads. | High |
| So what has to change? | Small. Add a third `Source` value to the recorder that reads one of those queues (or a new one), and stop the recorder freezing the voices while it runs. No new DSP is required for the simplest option. See section 4. | High that it is small, Medium on exact size |
| Cost | RAM: one to a few 260-byte blocks from the block pool, plus the loop buffer you need anyway. CPU: well under 1% of the audio interrupt for the capture itself (**GUESS**, no cycle counts exist). The real cost is the update-freeze fix, which is stock behaviour you would be turning off. | Medium-Low |
| Sample-accurate against the loop? | Yes for the capture, if the start is armed inside the audio interrupt (the same fix ticket 03 already needs). A pad hit itself is placed to the nearest 128-sample block, and its press-to-sound delay is set by the main loop, which was not measured. | Medium |
| Existing resample or bounce feature? | No bounce or freeze feature. The only offline rendering is the WAV exporter, which writes files to the SD card, not into sample memory. The only real resampler is a cubic interpolator used when importing external WAVs. | Medium-High |

## 2. How a pad press becomes sound (step by step)

Which "pad" means what. `core/interface/performanceMode/fPerformance.cpp` is the Performance FX screen: its pad handler selects FX slots and does not start sample notes (`fPerformance.cpp:872-912`). Pads that play samples are handled on the pattern, sample playback, instrument and import screens, which all end up in `Sequencer::handleNoteOn`. `padToBeatNoteConverter` is only a note-number table for beat-slice instruments: pad 0-47 maps to `FIRST_BEAT_SLICE_NOTE + pad` (`core/AudioEngine/padToBeatNoteConverter.cpp:7-22`, used from `core/source/mtPadBoard.cpp:~200-207`). It does no audio.

1. A pad press arrives as a UI event on the main loop (`TrackerImpl::update()`, `core/source/Tracker.cpp:165-179`) and calls the screen's pad handler, for example `functPads` in the pattern editor, which calls `sequencer.handleNoteOn(GRID_INSIDE_PATTERN, note, -1, pad)` (`core/interface/patternEditor/fPatternEdit.cpp:4716`). Other screens do the same with `GRID_OUTSIDE_PATTERN` (`core/interface/samplePlayback/fSamplePlay.cpp:641`, `core/interface/instrumentEditor/fIinstrEdit.cpp:2967`). The press is not handled in an interrupt.
2. `Sequencer::handleNoteOn` (`core/source/mtSequencer.cpp:2754`) picks a sample track and, for a sample instrument, calls `instrumentPlayer[track].noteOff()` then `instrumentPlayer[track].noteOn(instrument, note, velocity, ...)` (`mtSequencer.cpp:2965-2978` for preview during edit, `:3021-3033` for play mode). So a live pad hit uses one of the eight track voices, the same ones the sequencer uses. A second path, `mtPadBoard.startNoteWithoutInstrument`, takes a free voice for previewing raw buffers in the sample editor and recorder (`core/source/mtPadBoard.cpp:28-42`, callers `fSampleRec.cpp:686`, `fSampleEdit.cpp:784,3321`).
3. `playerEngine::noteOn` (`core/AudioEngine/mtAudioNoteOnHandle.cpp:84-`) sets up envelopes, filter, gain, pan, bit depth and sends under `AudioNoInterrupts()`, then starts the sample in the voice's `AudioPlayMemory`. It runs on the main loop, between audio interrupts.
4. Each voice is an `AudioVoice`: `memoryPlayer -> bitDepth -> filter -> amplifier -> stereoMirror -> panningAmplifier` (and an RMS meter), one chain per channel (`core/AudioEngine/AudioVoice.cpp:32-44`).
5. Voice outputs fan out in `audioEngine::audioEngine()` (`core/AudioEngine/mtAudioEngine.cpp:131-151`):
   - `panningAmplifier` L/R into `tracksAndSynthMixer[L/R]` input `idx` (the dry sum of all voices) (`mtAudioEngine.cpp:334-346`).
   - `filter` L/R into the reverb and delay send mixers (`:348-368`). Sends are taken before pan and before the dry mix, so effects are not in the dry sum.
   - `stereoMirror` into the limiter sidechain mux (`:428-440`).
   - `panningAmplifier` L/R into the per-voice export queues (`:679-694`).
6. `MixerAndFxs` (`core/AudioEngine/MixerAndFxs.cpp:6-33`): `tracksAndSynthMixer -> tracksAndSendsMixer` input 0, delay output into input 1, reverb output into input 2. Input 3 is the live line-in monitor (`mtAudioEngine.cpp:413-425`).
7. `tracksAndSendsMixer -> limiter -> finalMixer` input 0 (`mtAudioEngine.cpp:502-514`, `core/AudioEngine/AudioOutput.cpp:16-20`). `finalMixer` also takes the SD preview player, test waveform and metronome tick (`AudioOutput.h:19-26`, `mtAudioEngine.cpp:657-677`).
8. `finalMixer -> outputStream` (and USB out) (`AudioOutput.cpp:22-26,55-58`). The output interrupt interleaves it into the I2S transmit ring (see `docs/research/record-path-and-latency.md` section 2 for that half).

All of this runs in the output-DMA interrupt, once per 128-sample block (2.90 ms), via `engine.update()` then `AudioStream::update_all()` (`core/source/Tracker.cpp:181-209`).

## 3. What sources already exist inside the graph

| Tap | Contains | Already has a record queue? | Notes |
|---|---|---|---|
| Per voice, after pan (`panningAmplifier`) | One voice, dry, panned, no reverb or delay | Yes, 16 queues (8 voices x L/R) (`mtAudioEngine.cpp:679-694`) | Cleanest "pad voices only" source if pads and Parts use different voices |
| `tracksAndSynthMixer` L/R | All 8 voices summed, dry | No (only an RMS meter, `MixerAndFxs.cpp:28-29`) | Includes the other Parts playing back |
| Reverb return, delay return | Wet only | Yes (`mtAudioEngine.cpp:696-727`) | Not useful as a source |
| `tracksAndSendsMixer` | Voices plus FX plus live line-in monitor | No | Includes the input monitor |
| `finalMixer` L/R (post limiter) | Everything: voices, FX, line-in monitor, SD preview, metronome, test tone | Yes, the master stem queue (`mtAudioEngine.cpp:729-738`) | Anything audible is in it, including the metronome click |

So the answer to "internal mix, or only the pad voices?" is: **both are reachable**. The internal mix (master) already has a queue, but it also contains the metronome tick and the live input monitor, which you do not want inside a pad capture. Pad voices alone are available as per-voice queues, and a dry sum of all voices needs one new pair of queues.

Queue behaviour that matters (`core/libs/Audio/record_queue.cpp:84-124`, `record_queue.h:31-44`):
- A queue only keeps blocks after `begin()` and drops them again after `end()`. The export queues are permanently connected but idle, so they cost almost nothing until started.
- The ring is 10 slots, 9 usable. If it fills, new blocks are dropped silently with no counter (`record_queue.cpp:115-118`).
- With `AUDIO_NULL_IS_SILENCE` on (it is, `core/libs/Audio/AudioStream.h:54-56`), an empty input still produces a zero block while recording (`record_queue.cpp:92-103`), so silent stretches are captured as silence, not gaps.
- The queue stores the block pointer, it does not copy. The copy happens in `Recorder::update()`, one block per queue per audio interrupt (`core/source/Recorder.cpp:84-145`), which runs at the top of the same interrupt (`mtAudioEngine.cpp:273-275`).

## 4. Options

**A. Pad bus: record the pad voices only (recommended).**
Reserve some voices for pads (for example voices 5-8, Parts on 1-4). Add a recorder source, say `kInternalPads`, that reads the per-voice export queues for those voices, sums them to mono in `Recorder::update()` (it already averages L and R for mono, `Recorder.cpp:130-141`; summing a few voices is one more loop), and writes into the Part's loop buffer. Alternatively add one small mixer node summing those voices into one queue pair, which makes the recorder trivial and keeps the sum clean of clipping logic.
- Changes: `Recorder.h` Config enum (+1 value), `Recorder::record()` branch, voice reservation in the pad/sequencer note path, fix the update-freeze (below). No change to the stock mixer graph if the existing per-voice queues are used.
- Result: dry pad audio, panned and volume-scaled, without reverb, delay, line-in monitor or metronome.

**B. Dry mix of all voices.** Add queues on `tracksAndSynthMixer` L/R. Gives everything that is playing, including the other Parts. Useful later as a "bounce"; wrong for pad capture because it would re-record the loop that is playing.

**C. Master mix.** Use the existing `stem_master` queues. Zero new graph code, but includes metronome and input monitor, and sits after the limiter. One more difference: `output` is constructed after `exportQueues` (`mtAudioEngine.h:246-247`), so by the update-list rule in `AudioStream::connect` (`core/libs/Audio/AudioStream.cpp:239-254`) the master queue most likely reads the previous block, one block (2.9 ms) late (**GUESS**, I did not trace the whole update order). Not recommended for pads.

**D. Offline render of pad events** (the exporter way). The exporter already renders the sequencer to WAV faster than real time by switching on `AudioStream::renderMode`, virtual timers, and stepping `sequencer.tick`, `engine.update`, `artificial_isr` in a loop (`core/Exporter/mtAudioExport.cpp:72-101,724-758`, `core/libs/Audio/AudioStream.cpp:342-371`). That is a bounce of a pattern, not a live capture. Not suited to "play pads live and hear it recorded". Mention only because it is the only existing path that renders internal audio.

## 5. The one stock behaviour that breaks pad capture

`audioEngine::update()` only calls `instrumentPlayer[i].update()` when the recorder is **not** recording (`core/AudioEngine/mtAudioEngine.cpp:277-302`). That per-voice update handles end-of-note detection, release, envelopes, LFOs and parameter refresh (`core/AudioEngine/mtAudioUpdateHandle.cpp:7-70`). While recording is active these stop, so a pad hit during a recording would start (the note-on itself is done on the main loop) but would not release, end or follow its envelope correctly. It was presumably made to save CPU while recording from a line input.
Change: remove or narrow the `if (!recorder.isRecordingActive())` guard for voices used by pads and Parts. This is already flagged in `docs/research/cpu-ram-budget.md:20-21`. Confidence it is needed: High (the code is plain). Its CPU cost is the same per-voice update stock already runs when not recording; I did not measure it.

## 6. Cost

RAM (internal SRAM and SDRAM):
- Queue objects: tiny, one more pair is a few hundred bytes (**GUESS**).
- Blocks in flight: each is 260 bytes from the shared pool of 70 (`docs/research/cpu-ram-budget.md`, "Block pool" row). In normal operation the recorder drains one per interrupt, so about 1 to 3 per queue. Pool exhaustion causes silent dropouts, so confirm headroom on the device with four Parts, delay and reverb running.
- Destination: the loop buffer. Mono 16-bit is 88 KB per second. A 30 s Part is about 2.6 MB. The 4 x 30 s total (10.1 of 11.25 MiB) is already accounted in the cpu-ram-budget note. Pad capture adds no extra buffer if it records straight into the Part's buffer, as line capture does.
- The exporter's 768 KB scratch buffer is only used by offline render and is not involved in option A.

CPU (all **GUESS**, no cycle counts exist in the repo; 522,000 cycles per block):
- Queue plus copy plus mono sum for one source: a few thousand cycles per block, under 1%.
- Option A with a new 4-input sum node, stereo: another small amount per block; call it 1 to 2% as a placeholder.
- Re-enabling `instrumentPlayer[i].update()` during recording: already part of the stock non-recording budget.
Measure with the built-in profiler (needs `/.develop` on the SD card; see the cpu-ram-budget note).

## 7. Timing against the loop

What is sample-accurate and what is not:
- **Capture is on the same clock as playback.** The tap, the mixer and the loop player all run inside the same audio interrupt, so a recorded sample stays at a fixed offset to loop playback for the whole take. This is better than line-in capture, which also carries codec and DMA input delay (about 7 to 11 ms planning figure in `docs/research/record-path-and-latency.md`). Internal capture has no input round trip, so the round-trip compensation constant would not apply to pad parts (**inference**, Medium).
- **Start of recording** is still block-rounded and called from the main loop today (`fSampleRec.cpp:1566-1601`, `Recorder.cpp:46-82`). For a start exactly on the loop boundary it has to be armed in the audio interrupt and the first block trimmed, as already concluded in ticket 03. The same fix serves both sources.
- **The pad hit itself** is not sample-accurate. `noteOn` runs on the main loop, so the sound begins at the next block boundary after the press is processed, up to one block (2.9 ms) later plus however long the main loop took to see the pad. The loop clock is not affected, only where the hit lands inside the loop. This delay is the same for every pad hit, so a fixed offset can be removed, but jitter (main-loop timing) cannot. Pad scan rate and main-loop period were not read and not measured.
- **Quantising pad hits** to the grid would have to be done by delaying the note-on to a chosen block inside the audio interrupt (the sequencer already does this on `tick()`, `Tracker.cpp:204`). Not done today for live pads.

## 8. Existing resampling or bounce code

- **No bounce, freeze or resample-to-sample feature.** A search for resample, bounce, freeze and decimate across `core` found no mix-to-sample path. Sample Editor effects in `core/effector` work on sample memory in place and were not read in detail (so "none" for effector is Medium confidence).
- **Exporter** (`core/Exporter/mtAudioExport.cpp`): renders pattern or song to WAV on the SD card, faster than real time, using `renderMode` (`:72-101,724-758`). Output goes to files, not to an instrument. Its header writes 44100 Hz although the real rate is 44117.6 Hz (`mtAudioExport.cpp:383`, `Tracker.cpp:194-201`); harmless for a firmware that only replays at its own rate, but note it if exported files ever have to match a computer's timeline.
- **Rate converter on import**: `convertAudioData` does cubic interpolation from the file's sample rate to the target (`core/FileManager/AudioDataUtilities.cpp:60-80,82-`), called from the import code (`core/FileManager/fileTransfer.cpp:283,728` and others). It works on bytes read from files, not on live audio.
- **Playback pitch** uses interpolation in `AudioPlayMemory` (`setInterpolationEnable`, `mtAudioEngine.cpp:2040-2045`). That is pitch-shifting on read, not rendering to a new buffer.

## 9. Recommendation

Go with option A, with these choices:
1. Add one recorder source for the pad bus and record it into the Part buffer exactly as line-in does. Reuse the per-voice export queues, or a dedicated 4-input sum node, whichever is easier to get right in review. I lean towards the dedicated sum node because it removes any mismatch between the recorder's two-channel assumption and N voices (`Recorder.cpp:96-99` requires both channels available).
2. Reserve voices for pads, keep Parts on separate voices, so the capture never contains the loop that is playing.
3. Remove the update freeze during recording (section 5). Required.
4. Arm the recording start inside the audio interrupt (ticket 03). Required for sample-accurate start for every source.
5. Decide separately whether captured pads should carry reverb and delay. As wired they will not (sends are before the dry sum and not in the pad bus). Printing them dry suits a lo-fi master effect applied later, but that is a product choice, not a finding.

Do not build a general "internal mix" recorder source first. Master or dry-mix capture is a bounce feature, which the map lists as out of scope.

## 10. Confidence

Overall: **Medium-High** that the routing is as described and the change is small. **Medium-Low** on every CPU number, and **Medium** on the timing section, because the pad-to-sound delay depends on the main loop, which I did not read in full.

What I did not check: the full update order of every node (`hasLaterSource` for the new queues), the pad hardware scan path and rate, the effector directory in detail, and whether the 70-block pool has headroom with four Parts running.

## 11. What only the device can confirm

1. Audio-interrupt CPU with four Parts playing, a pad bus recording, delay and reverb on (profiler, `/.develop` file).
2. Whether the 70-block pool runs dry (silent dropouts) when the extra queue is running.
3. Press-to-sound delay and its jitter for a pad hit, measured with a pad and a recorded take compared against a click.
4. Whether the update-freeze fix causes any new glitch or hang when voices keep updating during recording.
5. Whether a recorded pad hit lands at a constant offset from the loop start across many takes (look for zero drift over a long loop).
6. Whether the master queue is one block late compared with the voice queues (only matters if option C is ever used).
7. Whether a stereo pad sample summed to a mono Part sounds right (the recorder's mono sum is a plain average, `Recorder.cpp:136-137`).
8. Whether the metronome click and the line-in monitor leak into the pad capture in the final graph (they should not in option A).
