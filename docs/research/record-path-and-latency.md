# Record path and latency on MK66 (original Tracker)

Ticket: how does recording work end to end, where does input-to-output latency come from, can recording start sample-accurately on a loop boundary, can a rolling pre-roll be kept, how can the monitor delay be compensated, and can the live monitor feed back.

Method: read the firmware source only (read-only). No hardware was available, and the SGTL5000 datasheet could not be fetched (network blocked), so every codec-internal number below is a labelled guess. All paths are relative to the repo root.

## 1. Short answers

| Question | Answer | Confidence |
|---|---|---|
| Input-to-output latency from the firmware's own buffering | 2 blocks = 256 samples = 5.80 ms best case; 3 blocks = 384 samples = 8.70 ms if the RX interrupt lands after the TX interrupt (boot/load dependent, see 3.1) | Medium |
| Codec ADC + DAC filters and I2S FIFOs on top | Unknown from the code. Guess 1 to 2 ms (about 45 to 90 samples). Not from a datasheet. | Low |
| Total input to headphone through the software monitor | About 300 to 480 samples, about 7 to 11 ms | Low-Medium |
| Sample-accurate start on a loop boundary with the current code | No. Start is quantised to a 128-sample block and `record()` is called from the UI loop, not the audio interrupt. | High |
| Sample-accurate start achievable | Yes, by arming inside the audio interrupt and trimming the first block by an offset (section 6.1) | Medium-High |
| Rolling pre-roll | Not present (the existing "pre-roll" is a metronome count-in). Easy to add as a free-running ring of input blocks. Cost is SDRAM, which is tight on MK66. | High |
| Compensation | Treat the whole loop as a fixed delay: recorded position = loop position + round-trip latency. Shift recorded data earlier by a measured constant. | High for the method, Low for the number |
| Existing monitor can feed back | Not digitally. The record queue and the monitor both tap the input only; nothing from the output returns to the input. Acoustic or cabled loops can. | High |

## 2. Data path, step by step

Clocks first. The MK66 is I2S master; the SGTL5000 is slave.

- MCLK = 180 MHz x 16 / 255 = 11.2941 MHz (`platforms/mk66/3rdparty/Audio/output_i2s.cpp:118-121`). Sample rate = MCLK / 256 = 44117.647 Hz, not exactly 44100. The firmware says so itself (`core/source/Tracker.cpp:194-200`).
- Block = 128 frames (`platforms/mk66/config/config.h:14`, `core/libs/Audio/AudioStream.h:51`). Block period T = 128 / 44117.647 = 2.9013 ms.
- Codec registers (`platforms/mk66/3rdparty/Audio/control_sgtl5000.cpp:545-548`): CHIP_CLK_CTRL 0x0004 (44.1 kHz, 256 Fs), CHIP_I2S_CTRL 0x0130 (slave, SCLK 32 Fs, 16-bit, I2S), CHIP_SSS_CTRL 0x0010 (I2S_DOUT = ADC, DAC = I2S_IN). ADC input is line-in or mic via `CHIP_ANA_CTRL` bit 2 (`:552`, `:241-246`).

Receive (record) side:

1. Analog input, SGTL5000 ADC and digital filters, I2S_DOUT to the MCU (delay unknown, see 3).
2. SAI/I2S0 receiver, synchronous to the transmitter clocks (`output_i2s.cpp:174-181`, `RCR2 SYNC(1)`). FIFO watermark `RFW(1)` (`:176`).
3. eDMA reads one 16-bit value (upper half of each 32-bit slot, `SADDR = &I2S0_RDR0 + 2`) per request into a ring of 512 int16 = 256 stereo frames = two 128-frame halves (`platforms/mk66/3rdparty/Audio/input_i2s.cpp:30,49-59`). Half-complete and major-complete both raise an interrupt (`:59`).
4. RX ISR (priority 4, `platforms/mk66/config/interruptsConfig.h:16`) works out which half just finished and calls `updateInputBuffers(ptr)` with a pointer to it (`input_i2s.cpp:70-94`). It copies nothing.
5. `updateInputBuffers` only stores the pointer in the engine (`platforms/mk66/source/TrackerMain.cpp:31-39`, `core/AudioEngine/mtAudioEngine.cpp:324-333`, `core/libs/Audio/Input.cpp:76-79`).
6. The copy happens later, in the output ISR: `AudioInputStream::update()` deinterleaves the DMA half into two `audio_block_t` blocks (`core/libs/Audio/Input.cpp:4-70`), inside `AudioStream::update_all()` (`core/libs/Audio/AudioStream.cpp:342-358`).
7. Graph: `i2sStream -> stereoToMono` (pass-through for stereo, L or R duplicated for mono) `-> recordingQueueI2s[L/R]` and `-> recordingRms`, plus `-> gate -> mixers` for the live monitor (`core/AudioEngine/AudioInput.cpp:5-20, 40-47`).
8. `AudioRecordQueue::update()` queues the block pointer in a 10-slot ring (9 usable) if `enabled` (`core/libs/Audio/record_queue.cpp:84-124`, `record_queue.h:31`). If the ring is full the block is dropped silently, with no counter (`record_queue.cpp:115-118`).
9. Draining: `Recorder::update()` is called at the top of `audioEngine::update()` (`core/AudioEngine/mtAudioEngine.cpp:273-275`), which `TrackerImpl::updateAudioBuffers` calls every audio ISR (`core/source/Tracker.cpp:181-208`). It copies one block per queue per ISR into SDRAM (`externalRamSampleMemory`) (`core/source/Recorder.cpp:84-145`). So draining is in the audio interrupt, not the main loop, and the queue does not grow in normal operation. SD card writes are not in this path.
10. UI: `functActionRecord()` (`core/interface/sampleRecorder/fSampleRec.cpp:1566-1601`) runs from the UI loop; it calls `recorder.setConfig()` and `recorder.record()`, which calls `queue->begin()` (clears the queue, sets `enabled`) (`core/source/Recorder.cpp:42-77`, `record_queue.h:38-44`). Stop: `recorder.stop()` ends the queue, drains what is left, clears (`Recorder.cpp:154-189`). Recording auto-stops near the end of the buffer (`:147-158`).

Playback (output) side:

1. Output ISR (priority 5, `interruptsConfig.h:18`) fires at each half of a 2 x 128-frame TX ring (`output_i2s.cpp:35,71-99`). It calls `TrackerImpl::updateAudioBuffers`, which runs `midiUpdate`, `sequencer.tick()`, `engine.update()` (includes `recorder.update()`), then `update_all()` (`core/source/Tracker.cpp:181-208`, `TrackerMain.cpp:19-29`).
2. `AudioOutputStream::update()` copies the final mix into staging buffers (`core/libs/Audio/Output.cpp:6-45`). The ISR then interleaves them into the half of the TX ring that is not currently being sent (`output_i2s.cpp:92-98`).
3. TX eDMA feeds the SAI TX FIFO (`TFW(1)`), then I2S to the SGTL5000 DAC (`DAC_SELECT = I2S_IN`), then the analog output stage.

Monitor path (software): `i2sStream -> stereoToMono -> input 3 of the tracks-and-sends mixer` (`mtAudioEngine.cpp:414-420`), gain from `forceLineInStreamVolume` / `refreshLineInStreamVolume` (`mtAudioEngine.cpp:853-872`). The recorder's Monitor On/Off option sets that gain to DEFAULT (86) or MIN (0) (`fSampleRec.cpp:615-622`, labels `sampleRecorder.h:24-28`). The same input also goes to delay and reverb mixers on channels 8 and 9 (`mtAudioEngine.cpp:371-405`). So monitoring is fully through the DSP chain with all the buffering below. Nothing in the firmware uses a codec-internal hardware monitor during recording (`setHeadphoneSource` is called only from `core/source/mtTest.cpp:1406-1422`).

Update order: `AudioStream` objects register in construction order (`AudioStream.h:143-154`) and `AudioConnection::connect` flags a destination with `hasLaterSource` if its source updates after it (`AudioStream.cpp:238-253`). In `audioEngine` the members are `input`, then `mixerAndFxs`, then `output` (`mtAudioEngine.h:231-247`), so the monitor chain runs forward in list order and should add no extra block delay. I did not check every internal node (Medium confidence).

## 3. Where latency comes from (arithmetic)

Notation: T = 128 samples = 128 / 44117.647 = 2.9013 ms.

Timeline, taking RX ISR and TX ISR as occurring together at times k*T:

- An input sample captured in [(k-1)T, kT) sits in the RX half that completes at kT.
- At kT the output ISR runs `update_all`, reads that half, and mixes it into the TX half that has just finished being sent (`output_i2s.cpp:71-99`). The TX ring is two halves long, so a half filled at kT is sent during [(k+1)T, (k+2)T) (the other half is being sent during [kT, (k+1)T)).
- So an input sample at offset j within its block is output at offset j within the block sent during [(k+1)T, (k+2)T). Delay from capture to output = (k+1)T - (k-1)T = **2T = 256 samples = 256 / 44.1176 = 5.80 ms.**

Breakdown: one block (T) waiting for the RX half to fill, plus one block (T) waiting for the TX half to be sent after being filled. ISR processing time does not add as long as `update_all` finishes in under T.

Extras not visible in the code:

- I2S FIFOs. TX DMA writes ahead of the shifter and RX DMA reads behind it (watermark 1; K66 SAI FIFO is 8 words, from memory of the reference manual). Guess: a few samples. Low confidence.
- Codec ADC decimation and DAC interpolation filters. I could not get SGTL5000 numbers. A comparable TI codec lists about 17/fs for the ADC alone (TI AIC310x datasheet, found by web search, not SGTL5000). **Guess for SGTL5000 ADC + DAC: 1 to 2 ms (45 to 90 samples). Not a datasheet or measured value.**
- Analog stages: negligible by comparison.

| Case | Samples | ms (samples / 44.1176) |
|---|---|---|
| Firmware buffering, best case (2 blocks) | 256 | 5.80 |
| Firmware buffering, 3-block case | 384 | 8.70 |
| Plus FIFOs and codec (guess 45 to 100) | 300 to 484 | 6.8 to 11.0 |

Planning figure: **about 300 to 480 samples (7 to 11 ms) input to output through the DSP path. Must be measured.**

### 3.1 Two-or-three-block uncertainty (matters for alignment)

RX and TX share one SAI frame clock, but their DMA rings started at different moments: TX in `AudioOutputI2S::begin` (`output_i2s.cpp:65-66`), RX later in `AudioInputI2S::begin` (`platforms/mk66/source/AudioHardware.cpp:14-15`, `input_i2s.cpp:64-65`). So there is a fixed phase offset d between the two rings (microseconds to a few hundred microseconds, set at boot).

- If the RX half-complete interrupt arrives before the TX one, the output ISR sees the newly completed RX half: 2T.
- If it arrives after, and the output ISR reaches `AudioInputStream::update` before it, the ISR still sees the previous half: 3T. RX has higher priority (4 vs 5), so it can also preempt the output ISR midway and change the pointer, so the block read depends on ISR timing.

I could not determine from the code which one happens, and it may vary from boot to boot. That is the main reason to measure, and to read the DMA position registers (6.3) rather than assume.

Hazard: the engine only holds a pointer into the DMA ring. If the output ISR runs more than about one block late (for example held off by a higher priority interrupt such as USB at priority 3 or MIDI at 0, `interruptsConfig.h:11-21`), DMA overwrites the half before it is copied. That corrupts monitor and recording silently.

## 4. Sample accuracy of the start (current code)

- `record()` runs on the UI loop, so `begin()` takes effect at an arbitrary point relative to the audio ISR.
- The first block recorded is the first one `AudioRecordQueue::update` sees after `enabled` is set, so the start is quantised to a 128-sample (2.90 ms) block boundary of the RX ring at best.
- The sequencer is block-quantised too: `sequencer.tick(length / fs)` advances per block and note-ons are applied before that block is rendered (`Tracker.cpp:202-208`, `core/source/mtSequencer.cpp:92-100`). Only MIDI clock output gets a sub-block offset (`midiSetClockSubBlockOffsetUs`). A note triggered "at loop start" therefore sounds from sample 0 of a block, with up to 128 samples of jitter against the true loop position.
- The existing pre-roll is a metronome count-in, not audio (`fSampleRec.cpp:1397-1408`).

## 5. Compensation model for Parts recorded while monitoring

Define p(k) as the loop position of the first sample rendered at ISR k.

- Loop position p(k)+j is heard at time (k+1)T + j (plus DAC delay).
- A player in time with it makes sound then; the input captures it later by the ADC delay, in the RX block read at ISR k+2.
- If the recorder naively stores "the block read at ISR m at loop position p(m)", the hit lands at p(k) + 256 + j, so a constant late offset of 2 blocks plus (DAC + ADC delays + FIFOs).
- Correct approach: store the block read at ISR m at loop position p(m-2) (256 samples earlier). The residual is codec and FIFO delay only (guess 45 to 100 samples), removed by a measured constant. In the 3-block case use 384 instead of 256.

So alignment is one constant, RTL (round trip latency in samples), applied by shifting recorded data earlier relative to loop position. It depends on the hardware path, not on the player. Monitor and record queue tap the same point (`stereoToMono` output), so the delay the player hears in the monitor is the delay recorded.

Consequences:

- Shifting earlier by RTL needs RTL extra samples captured after loop end, and a hit played just before loop start needs pre-roll (6.2).
- Players may anticipate by feel; that is musical, not technical, and the constant still corrects the hardware part.

## 6. What a four-track recorder can do

### 6.1 Sample-accurate arming
- Arm in the audio ISR (`TrackerImpl::updateAudioBuffers`), not the UI loop. The UI sets a flag; the ISR consumes it at the loop boundary.
- Keep a global frame counter incremented by 128 per ISR and tag each queued RX block with its sequence number.
- Loop positions per render block are known exactly, so a loop start inside a block can be handled by copying from offset `o = loopStart - blockStart`. Same at the end. This gives one-sample resolution.

### 6.2 Rolling pre-roll
- Not present today. A free-running ring of the last N input samples is cheap in CPU (one block copy per ISR).
- Cost is SDRAM. MK66 external RAM is 16 MB with about 1 MB free (`docs/memory_map.md`, EXTERNAL_RAM row; `core/source/mtStructs.h:63-65`). One second of mono 16-bit = 44117.6 x 2 B = 88 KB. A 150 ms pre-roll (more than the RTL plus an early hit) is about 13 KB mono: affordable. Full loops are the problem: 8 s stereo = 8 x 44117.6 x 2 B x 2 ch = 1.41 MB per Part, so four Parts = 5.6 MB, which would have to come out of `SAMPLE_MEMORY_SIZE` (11.25 MB, `mtStructs.h:65`). Mono halves that.
- Keep the pre-roll in an ISR-owned ring, not in `AudioRecordQueue` (10 slots, about 29 ms).

### 6.3 Measuring RX/TX phase in firmware
- The RX ISR already reads `dma.TCD->DADDR` (`input_i2s.cpp:74`) and the TX ISR reads `SADDR` (`output_i2s.cpp:73`). Stamping both with a free-running cycle counter would reveal the exact phase and which ISR fires first, removing the 2-versus-3-block uncertainty at run time. Not implemented today.

## 7. Existing monitor and feedback

- The monitor is software only (section 2). The codec can do ADC to DAC internally via `CHIP_SSS_CTRL` DAC_SELECT = ADC (`control_sgtl5000.cpp:650-664`, used by the factory test at `mtTest.cpp:1422`), and `CHIP_ANA_CTRL` bit 6 can route line-in straight to the headphone amp (`control_sgtl5000.cpp:241`). Either gives near-zero latency but replaces the DAC source, so loop playback would not be heard at the same time. The codec register map also has a DAP mixer (`DAP_MIX_SELECT`, `:122`; `DAP_MIX_CHAN`, `:489`) that might mix I2S loop audio with the live ADC inside the codec for a zero-MCU-latency monitor. **That last point is my reading of register names, untested and unchecked against the datasheet: a guess.**
- Digital feedback: none. Record queues and the monitor are sinks off `stereoToMono`; nothing from the output returns to the input. Codec I2S_DOUT is the ADC, not the DAC mix (`CHIP_SSS_CTRL = 0x0010`, `:548`).
- Real feedback routes: mic near a speaker, line-out cabled to line-in, the radio source (`fSampleRec.cpp:2725-2735`). Monitor On with a mic and speakers will howl; the recorder mutes the headphone when selecting a mic source (`fSampleRec.cpp:2745-2750`).
- The monitor also feeds delay and reverb sends (`mtAudioEngine.cpp:371-405`); those have their own internal feedback but are not an I/O loop.

## 8. What must be measured on hardware

Goal: one number, RTL in samples, and its boot-to-boot and load-to-load stability.

Simple loopback method: patch line-out (or headphone out through a suitable attenuator) to line-in. In a test build, at a known frame counter N inside the audio ISR, render a single short full-scale click at sample 0 of a block, while recording the input into RAM with the frame counter tagged per block. Find the first input sample above a threshold and compute `RTL = input_index - N*128`. Report samples. Repeat about 50 times across about 10 power cycles, and with the system busy (display updates, SD activity, USB connected), to see jitter and whether the 2-block or 3-block case occurs. No extra equipment beyond a cable. With an oscilloscope on the output and a GPIO toggled in the ISR when the click is rendered, the output-side delay can be separated from the input side.

Also measure: (a) RX versus TX ISR timestamp phase (6.3), (b) worst-case `update_all` duration against T = 2.90 ms (the existing `Profiler::startMeasuringAudio`, `Tracker.cpp:191`, may already log this), (c) whether the input pointer is ever stale at copy time.

## 9. Confidence summary

- Data path, ISR order, ring sizes, priorities, queue behaviour: High (read directly from source).
- 2 blocks = 256 samples buffering latency: Medium (depends on RX/TX phase, 3.1).
- Codec and FIFO share: Low (no datasheet, no measurement).
- Which ISR fires first at a given boot: Low (needs hardware).
- Hardware monitor via the DAP mixer: Low (untested reading of register names).
- Memory feasibility numbers: Medium (from `docs/memory_map.md` baselines, which change per build).
