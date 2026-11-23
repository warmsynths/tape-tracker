# Audit C: pads, samples, streaming, MIDI, memory and the pad-bus claim, checked against v1.0.0

Read-only audit. Nothing in the repository was changed. All paths are relative to `/home/user/tape-tracker`. Line numbers are from the files as they are now (v1.0.0 plus our additions).

## How this was checked

- The repository is one commit (`f81bfed`) on top of the tag `v1.0.0`. `git diff v1.0.0 -- core platforms` lists exactly 18 files that are ours (Four-Track screen, `FourTrackSession`, `play_loops`, `loop_fill`, small edits in `Recorder`, `mtAudioEngine`, `MixerAndFxs.h`, `mixer.cpp`, `Audio.h`, `interface.cpp`). Everything else is stock v1.0.0. I used this to separate stock behaviour from ours.
- I did not use the build output folders (`core/build-*`, `platforms/mk66/build/*.lss`). They are git-ignored and were built from other source: they contain code that the source tree does not (for example `AUDIO_NULL_IS_SILENCE`, `isInputSilenced`). They are not evidence for v1.0.0.
- Nothing was built or run. Everything below is from reading the code. Where I reason about behaviour without being able to run it, I say so.
- Status words: VERIFIED IN v1.0.0, DIFFERS IN v1.0.0, NOT FOUND IN v1.0.0, ESTIMATE. In the tables I shorten them to VERIFIED / DIFFERS / NOT FOUND / ESTIMATE.
- "Line numbers wrong" is noted where the claim is right but the cited lines are not. In `pad-recording-path.md` nearly every citation is off (for example `Sequencer::handleNoteOn` is cited at `mtSequencer.cpp:2754` but is at `:2551`). That file clearly cites another tree.

## 1. Direct answers to the questions

### 1.1 How pads trigger sounds, screen by screen

Pad press path: `padPressed` (`core/source/mtActions.cpp:54-57`) calls `Interface::padPressed` (`core/interface/core/interfaceInputs.cpp:78-81`), which calls `InputProcessor::processPadsInput` (`core/interface/core/modulesBase.cpp:306-317`) and then whatever handler the current screen registered with `setPadsGlobal`. The pad hardware interrupt only sets a flag (`platforms/mk66/source/PadsImpl.cpp:19-22`); the dispatch itself runs in the main loop (`core/source/Tracker.cpp:152-168`, `mtInterface.update()` at `:166`). I did not trace the I2C scan code, so "main loop" for the scan step is by inference (ESTIMATE, medium).

- The Original passes no velocity: `onPadPush` calls `padPressed(n, 0, 0, 0)` (`mtActions.cpp:56`). Every pad handler also ignores or overrides it (`-1`, `127` or `100`). So pads are not velocity sensitive.
- On module switch the old handler is cleared (`core/interface/core/interface.cpp:415,437` call `ip->clearGlobalPads()`). Our Four-Track screen registers none (`fFourTrack.cpp:138` only clears lights), so pads are silent on it. `setPadsGlobal` has 21 call sites (count matches the survey).

| Screen | Pad handler | Instrument that plays | Note | Voice path |
|---|---|---|---|---|
| Pattern editor, play mode (green) | `fPatternEdit.cpp:4622-4647` | `lastUsedInstrument`, except in the Instrument column, where `lastUsedInstrument = pad` (`:4631-4636`) | `getNoteFromPad` (`:4630`); in the Instrument column the note at the cursor step (`getSelectionFirstNote`, `mtSequencer_patternEdit.cpp:1681-1690`, default 60) | `handleNoteOn(GRID_INSIDE_PATTERN, ..., pad)` (`:4640`) |
| Pattern editor, record | `fPatternEdit.cpp:4684-4689` (note column), `:4719-4741` (instrument column) | note column: `lastUsedInstrument`; instrument column: **the pad number is the instrument** (`GRID_INSIDE_PATTERN_INSTRUMENT`, `mtSequencer.cpp:2575-2579`, `:2713` `padInstrumentEdit ? source : lastUsedInstrument`) | same | same |
| Pattern editor, edit | `fPatternEdit.cpp:4743-4757` | Instrument column: `setSelectionInstrument(pad)` (or `48 + pad % 16` on MIDI tracks) | | edits the pattern, no sound |
| Sample Playback | `fSamplePlay.cpp:675-704` via `SP->noteOnHandle` (`:593-595`) | `lastUsedInstrument` | `getNoteFromPad(pad, isBeatSlice)` | `handleNoteOn(GRID_OUTSIDE_PATTERN, ..., 127, pad)` |
| Instrument editor | `fIinstrEdit.cpp:2892-2923` | `lastUsedInstrument` | same | same. In the "Instrument MIDI Mapping" page a pad only selects a list row (`:2896-2901`, silent) |
| Master, Song editor, note popup | `fMasterParams.cpp:2943-2957`, `fSongEdit.cpp:1147-1157`, `interfacePopups.cpp:1184-1228` | `lastUsedInstrument` | same | same |
| Sample Loader | `fSampleImport.cpp:1998-2011`: **disabled** (only the text keyboard uses pads; the `playFromPads` call is commented out, "playback wylaczony na zlecenie Piotrka") | none | none | only the Preview button plays the selected slot, note 60 (`:1577-1606`, `:1643-1660`, `mtPadBoard.cpp:207-208`) |
| Sample Recorder, not recording | `fSampleRec.cpp:626-662`, `:1487-1499` | none: plays the raw recorded buffer | `getNoteFromPad` | `mtPadBoard.startNoteWithoutInstrument` (`mtPadBoard.cpp:28-42`) |
| Sample Recorder, recording | same handler, falls to `handleNoteOn` (`:657`) | `lastUsedInstrument` | | but `engine.blockAllNoteOn()` (`:317`, undone at `:430`) makes `noteOn` return 0 for every instrument (`mtAudioNoteOnHandle.cpp:13,86`), so silent |
| Sample Editor | `fSampleEdit.cpp:624-661`, `:1594-1620` | none: raw buffer of the edited sample | | `startNoteWithoutInstrument` |
| Performance | `fPerformance.cpp:835-870` | none | none | FX slot select only, no `handleNoteOn` |
| Four-Track (ours) | none | none | none | silent |

How `handleNoteOn` picks the voice in a screen like ours (not edit mode, not live-record): `mtSequencer.cpp:2782-2825`. It starts at the selection's first track and takes the first sample track (0 to 7) that is not muted (`isTrackEngineMuted`, `mtSequencer_patternEdit.cpp:2087-2091`; that is `trackMute`, a project value) and has `noteOpen == 0`. It then calls `instrumentPlayer[track].noteOff()` and `.noteOn(lastUsedInstrument, note, volume, ...)`. Voice index equals track index. There is no voice stealing: with 8 pads held, a 9th press finds no free track and does nothing. A released pad frees its track at once, and the next press takes the lowest free track again, so a new hit cuts the tail of the previous one on that voice (by reading; not heard on a device).

How Config notes scale, root note and layout apply:
- Values are project values: `padBoardScale` (default 0, Chromatic), `padBoardNoteOffset` (default 11), `padBoardRootNote` (default 24) in `mtStructs.h:848-850`. Menu items `configMenu.cpp:66-68`, `:77-79`; the change handler `setPadBoardValues` (`connfigMenuActions.cpp:102-107`) calls `mtPadBoard.setPadNotes`. It is also called at project load (`fProjEdit.cpp:382-385`).
- `setPadNotes` (`mtPadBoard.cpp:133-178`) fills `padNotes[48]`: start at root + 12, step up note by note skipping notes outside the scale, and at each row start apply the layout offset. `padBoard[]` (`:9-12`) maps pad number to that table: pad 0 to 11 (top row) are table entries 36 to 47, pad 36 to 47 (bottom row) are entries 0 to 11.
- With the defaults (chromatic, layout 11, root 24) the bottom-left pad is note 36 and each pad is one semitone up, row after row. Unity pitch is note 60 (`play_memory.cpp:23`, `kMiddleNoteIdx = 60`; `STEP_NOTE_DEFAULT = 60`, `mtSequencer.h:44`). With these defaults that is pad 12, my arithmetic. Notes clamp to 0 to 120 (`mtStructs.h:151-152`).
- Exception: when the instrument's play mode is Beat Slice, the pad maps straight to slice note `24 + pad` (`mtPadBoard.cpp:202-205`, `padToBeatNoteConverter.cpp:6-12`, `FIRST_BEAT_SLICE_NOTE = 24`, `mtStructs.h:360`). The scale, root and layout settings do not apply to it.
- Scale, root and layout do not apply to MIDI notes (see 1.3).

### 1.2 Is there truly no way for pad N to play instrument N?

There is. It is not a table or a mode, but the Pattern editor already does it (`fPatternEdit.cpp:4631-4636`, `:4719-4741`, `:4743-4757`, and `mtSequencer.cpp:2575-2579, 2713`): the pad number is used directly as the instrument index 0 to 47. Calling the same primitives from any screen works with no engine change:
`mtProject.values.lastUsedInstrument = pad; sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, 60, -1, pad);` (and `handleNoteOff` on release). An empty slot is silent (`mtAudioNoteOnHandle.cpp:11,84`: `isActive != 1` returns 0).

I searched every pad file and for "kit", "bank", "performance", "padToInstrument" and similar. Found none besides the above. `mtPadBoard` only knows pad to note. The Performance screen is FX only. The drum-synth percussion notes (`fIinstrEdit.cpp:2268-2300`) are Tracker+/Mini only (`FEATURE_SYNTH` is defined only for IMX, `core/makefile:57`).

There is no per-sample "Pad mode / Keys mode" flag anywhere: `strInstrument` is fixed at 372 bytes (`mtStructs.h:728`, static_assert) and has no such field. The only per-instrument pad-layout switch is play mode Beat Slice. So "chosen per sample" needs new storage (our own table or a changed file format).

### 1.3 MIDI input

- Two classes, `UartMidi` (jack, `Serial5`, `platforms/mk66/source/UartMidiImpl.cpp`) and `UsbMidi` (`core/include/Platform.h:120-146`). Handlers are registered in `midiInit` (`core/source/mtMidi.cpp:35-61`).
- Polling happens inside the audio interrupt: `TrackerImpl::updateAudioBuffers` calls `midiUpdate()` (`Tracker.cpp:182`, `mtMidi.cpp:63-83`). So MIDI note handling runs in interrupt context, once per 128-sample block, not in the main loop.
- Gate: Config "Notes in" must be USB, jack or both. **The default is Off** (`mtConfig.cpp:517`, enum `mtMidi.h:25-30`). "Notes in channel" is only a filter, 0 = ALL (`mtMidi.cpp:85-88`); there is no channel-to-instrument mapping for sample sounds.
- Pitch: raw note plus `12 * (middleC - 2)` (default middleC = 2, so no shift) (`mtMidi.cpp:106-111`). The pad scale, root and layout are not used. MIDI note 60 plays unity pitch.
- Which instrument: by `midiInVoiceMode`. Default is `midiInVoiceMode_sequencer` (`mtSequencer.h:109-116`, set by Sample Playback, Sample Editor and Sample Recorder only: `fSamplePlay.cpp:318,333`, `fSampleEdit.cpp:248,422`, `fSampleRec.cpp:302,437`). In the default mode `handleNoteOn(GRID_OUTSIDE_PATTERN, pitch, velocity, pitch + 100)` runs (`mtMidi.cpp:112-118`), which plays `lastUsedInstrument` on the first free sample track (`mtSequencer.cpp:2782-2825`). Nothing in `fFourTrack.cpp` sets a mode, so on our screen MIDI would already work once "Notes in" is on.
- Velocity is ignored unless Config "Recording options" includes velocity: `recVelocity()` (`mtSequencer.cpp:2545-2549`) and `handleNoteOn` (`:2557-2560`) force 127; default `recOptions = 0` (`mtConfig.cpp:552`). The survey proposes hiding that item.
- It reaches into the Pattern editor: `sendSelection()` and `patternEditor.refreshPatternFromOutsideModule()` (`mtMidi.cpp:114-118`; `fPatternEdit.cpp:474-483, 3231-3254`). The Pattern editor code and state must stay compiled and initialised for MIDI to work, or `mtMidi.cpp` must be changed.
- "Same sounds as the pads": true only for Keys mode if you accept different pitch mapping (pad grid versus raw MIDI note). For one-pad-one-instrument mode you must add a note-to-instrument map for MIDI yourself; stock has none.

### 1.4 How many voices, and how they are shared

- 8 voices: `audioEngine::kVoices = 8` (`mtAudioEngine.h:40`), `playerEngine instrumentPlayer[8]` (`mtAudioEngine.cpp:24`).
- Sequencer: track t plays on voice t (`mtSequencer.cpp:566-570` and others, `instrumentPlayer[row]`). Pads and MIDI: same, voice = the sample track chosen in `handleNoteOn`. So pads, MIDI and the sequencer share the same 8 voices through the track index, with no allocator.
- `mtPadBoard.voices[8]` (`mtPadBoard.h:45`, `.cpp:44-52`) is a second, separate bookkeeping used only for raw-buffer previews (`startNoteWithoutInstrument`) in the Sample Recorder and Sample Editor. It is unaware of `noteOpen`, so both can pick the same voice. `padBoardMaxVoices` (`mtStructs.h:851`) is never read.
- SD streaming voices: `VoiceStreamManager::kVoiceCount = 8` (`VoiceStreamManager.h:12`), indexed by the same voice number (`AudioPlayMemory::init(timer, channel, nChannel)`, `mtAudioEngine.cpp:1687, 2452`, `play_memory.cpp:176-181`). They are not extra voices: a voice playing a streamed instrument uses that voice's ring buffer. Only one SD read job is in flight at a time (`VoiceStreamManager.h:158`), so SD speed is shared by all.
- Our four Parts do not use voices at all. They play through `AudioPlayLoops` (`play_loops.h`, `mtAudioEngine.h:236`) into `tracksAndSendsMixer` input 4 (`MixerAndFxs.h:23-30`, `mtAudioEngine.cpp:152-158`). So all 8 voices are free for pads and MIDI. The metronome tick and the SD preview player are separate audio objects, not voices.

### 1.5 Memory cost of a sample on the Original

| Kind | Pool cost | Evidence |
|---|---|---|
| Loaded (RAM) sample, mono | length x 2 bytes (e.g. 1 s = 88,200 B at 44.1 kHz) | `used_memory += samples * 2` (`fileManagerInstrument.cpp:352`, `SampleImport.cpp:552-557`); total is `getSampleMemorySize` (`fileManagerSamples.cpp:412-445`) |
| Streamed sample (file at least 256 KB mono, i.e. at least 131,072 frames, about 2.97 s; play mode One-Shot or any Loop mode) | at most 32,768 frames x 2 = 65,536 B (0.743 s) | `kStreamThresholdBytes` (`mtStructs.h:775`), `kStreamPreCacheFrames` (`:776`), `isPlayModeStreamable` (`:779-793`), decision `fileManagerInstrument.cpp:141-153`, `used_memory += ramBytes` (`:296`); Original streams One-Shot and the three loops only (`CHANGELOG.md:49`) |
| Streamed sample, outside the pool | none per sample except small metadata (path 256 B, CLMT 256 B, waveform peaks 600 x 4 B per slot); plus fixed ring buffers for the 8 voices: 8 x 16,384 frames x 2 B = 262,144 B, loop wrap 8 x 2,048 x 2 = 32,768 B | `VoiceStreamManager.cpp:12-38`, `VoiceStreamManager.h:19,22`. These sit in external RAM outside the 11.25 MiB pool and are already in the build's SDRAM figure |
| Stereo sample | no extra cost: the Original loads everything mono | `stereoSampleMode = false` (`Tracker.cpp:138-142`), `isStereo && stereoSampleMode` (`fileManagerInstrument.cpp:141`), `SAMPLE_CHANNELS = 1` (`mtStructs.h:80-82`), `VoiceStreamManager::kMaxChannels = 1` (`VoiceStreamManager.h:14-18`). A stereo WAV imports the first channel only (`fileTransfer.cpp:1118-1180`, `AudioDataUtilities.cpp:73-110`) |
| Slice, Beat Slice, Granular, Wavetable | full length, never streamed on the Original | `mtStructs.h:779-790`, `CHANGELOG.md:49` |

Important detail the documents missed: a long sample is first loaded into RAM in full and only then cut down to its 64 KB head (`SampleImport.cpp:155-166`, `demoteRamInstrumentToStream`). The Sample Loader's meter and its "memory full" gate use the full size (`fSampleImport.cpp:834-860`, `:1396`, `:1465-1530`; `fileManagerBrowse.cpp:355-364`; there is no stream awareness in `sampleImporter/`). So importing a long sample needs its full length free at that moment. A project load (`fileManagerInstrument.cpp:262-296`) only needs the 64 KB.

### 1.6 Can a streamed instrument be played by a pad while the recorder records?

Yes, on our screen. Streaming is serviced from the main loop (`engine.updateSdStream()` at `Tracker.cpp:159,161,167` calls `voiceStreamManager.update()`, `mtAudioEngine.cpp:323-327`) and by the voice players inside the audio interrupt. Neither checks the recorder. The only recorder gate in the engine is the `playerEngine::update()` skip (`mtAudioEngine.cpp:288-318`). The stock Sample Recorder screen blocks instrument note-ons while it is open (`fSampleRec.cpp:317`), but our screen does not call that. Real limits: the single SD read job, and any other SD work we do at the same time (WAV saves, undo renames). Not measured.

### 1.7 What happens if the four recording regions overlap loaded instruments

How the pool works:
- One block `externalRamSampleMemory[SAMPLE_MEMORY_SIZE/2]` (`Tracker.cpp:22`), 11,796,480 bytes (`mtStructs.h:65`). Active instruments are packed back to back from the start of the pool in slot order (`setInstrumentSampleAddress`, `fileManagerInstrument.cpp:400-455`; first sample at `externalRamSampleMemory`). `used_memory` is the exact end of that packed run (`fileManagerSamples.cpp:412-445`).
- `max_memory` is `SAMPLE_MEMORY_SIZE` (`mtStructs.h:996`) and is reset to that value whenever a default project is made (`fileManagerProject.cpp:187`). It is the only fence. It is checked in the project loader (`fileManagerInstrument.cpp:275`), promote-to-RAM (`:475`), Sample Loader (`fSampleImport.cpp:1501-1524, 1876`), Sample Recorder save (`fSampleRec.cpp:1289, 1842`), Sample Editor (`fSampleEdit.cpp:1938, 2056`) and Render Selection (`fPatternEdit.cpp:1926`). The `.it` import checks `SAMPLE_MEMORY_SIZE` directly (`fileManagerImportIt.cpp:554, 611`); the `.mod` import wipes the whole pool (`fileManagerImportMod.cpp:208`).
- Our regions (`fFourTrack.cpp:23-43`) are 4 x 2,294,136 B = 9,176,544 B at the very top: track 4 starts at byte 2,619,936 (2.50 MiB), track 3 at 4,914,072, track 2 at 7,208,208, track 1 at 9,502,344. Nothing sets `max_memory` to reflect that.

Consequences, by code reading:
- As soon as loaded samples extend past byte 2,619,936, the end of the packed run lies inside track 4's region. Recording a Part writes over those samples (their RAM data, including the 64 KB heads of streamed ones, becomes noise; there is no crash, only wrong audio). Loading or importing a sample writes over Part audio. Nothing detects either.
- The stock Sample Recorder does the same to loaded samples and repairs it afterwards by reloading from the workspace (`fSampleRec.cpp:1525, 1530-1531, 2296-2311`). Our screen has no such step; `fFourTrack.cpp:38-39` says "do not use them with this test build".
- Other stock code that writes into the upper pool: the Sample Editor and effects use the top 7.5 MB (`EDIT_BUFF_OFFSET = 3,932,160`, `mtStructs.h:85`, `Tracker.cpp:28`, `mtEffect.h:33-37`), `mtEffect.cpp:119` uses pool base + 7.5 MB as overlap scratch, and the auto-slice code reads into the same buffer (`mtSliceManager.cpp:116`).

Safe place: there is no spare memory outside the pool (about 1.13 MiB of SDRAM is free according to `docs/build-notes.md`, not re-measured), so the regions have to stay inside the pool. The least invasive safe arrangement is: keep the regions at the top, set `mtProject.max_memory = SAMPLE_MEMORY_SIZE - 4 * kRegionBytes` (2,619,936 B) right after `getDefaultProject` and after every project load (it resets at `fileManagerProject.cpp:187`), and make the writers listed above unreachable (Sample Recorder, Sample Editor, `.it`/`.mod` import, auto-slice). All the checks in the list above then respect the fence. With that fence, at most 39 streamed samples fit (39 x 65,536 = 2,555,904; 40 would need 2,621,440, which is 1,504 B too many), and an import of a long sample needs its full length free.

### 1.8 Is "a pad-bus source on reserved voices can be recorded dry" still right?

Mostly yes in outcome, with several parts of the reasoning wrong for v1.0.0:
- Record queues exist on all 8 voices (post-pan, dry), on reverb, delay and the final mix (`mtAudioEngine.cpp:729-743, 764-795, 797-812`; `ExportQueues.h:22`). The recorder has only two sources and both are external (`Recorder.h:13-17`, `Recorder.cpp:54-67`). So a new source or a new node is needed. VERIFIED.
- On the Original the voice tap is `MonoToStereo` L/R (pan applied by its gains, `mtAudioEngine.cpp:1750-1770`), not a `panningAmplifier`. There is no `stereoMirror` anywhere in the source.
- No voice reservation is needed: Parts do not use voices (1.4). A queue pair on `tracksAndSynthMixer` (`MixerAndFxs.cpp:14-31`) would carry only pad and MIDI voices and no Parts, because the loop player enters after it. The master mix queue (`stem_master`) also holds the line-in monitor and the metronome (`AudioOutput.h:19-26`), and the loop.
- Taps differ in what they inherit: voice taps include instrument volume and `trackVolume[nChannel]` (`mtAudioEngine.cpp:1781`) and pan; a `tracksAndSynthMixer` tap additionally includes the project's saturation type (`setSaturationType`, `mtAudioEngine.cpp:1618-1625`, `mixer.cpp:52-56`). Per-instrument bit depth is applied before any tap (`AudioVoice.cpp:4-11`). Reverb and delay sends branch off before the amplifier (`mtAudioEngine.cpp:357-366`), so they are never in a voice tap.
- Idle voices still produce silent blocks (an idle `AudioPlayMemory` transmits a zero block every update: `play_memory.cpp:126-139`, `effect_envelope.cpp:439-457`), so a voice or mixer tap records silence, not gaps, as long as the 70-block pool does not run out (`mtAudioEngine.cpp:195`). `AUDIO_NULL_IS_SILENCE`, which the research cites for this, does not exist in v1.0.0.
- Mono sum: the Recorder averages L and R when going stereo to mono (`Recorder.cpp:148-149`). Pan here is "centre = both channels at full gain, hard side attenuates the other channel" (`mtAudioEngine.cpp:1750-1770`), so the average of a hard-panned voice is half level, and a centred voice is unchanged.
- The "update freeze" is much narrower than claimed (3 below, A39 and A40).

## 2. Claim-by-claim

### 2.1 `docs/research/pad-recording-path.md`

| ID | Claim (one line) | Status | Evidence / correct statement |
|---|---|---|---|
| A1 | Recorder knows only two sources, USB and I2S, both external | VERIFIED | `core/source/Recorder.cpp:54-67`, enum `Recorder.h:13-17` (doc cites `:49-62`, `:14-18`). On the Original only I2S is ever set; the USB assignment is IMX-only (`fSampleRec.cpp:1559-1561`) |
| A2 | UI source list is line, radio, mic, USB only (`sampleRecorder.h:9-13`) | DIFFERS | Original list is Line in, Radio, Mic LG, Mic HG; USB exists only on IMX (`sampleRecorder.h:8-23`) |
| A3 | Export record queues sit on every voice, reverb, delay and the final mix | VERIFIED | `mtAudioEngine.cpp:729-743` (voices), `:764-795` (reverb, delay), `:797-812` (master). Doc cites `:679-746`. `ExportQueues.h:7-23` right |
| A4 | They are the same `AudioRecordQueue` class the recorder reads | VERIFIED | `ExportQueues.h:22`, `AudioInput.h:30-32` |
| A5 | Change is small: a third `Source` reading a queue | ESTIMATE | Design judgement. Code is compatible: `Recorder::record()` selects queues in one branch (`Recorder.cpp:54-72`) |
| A6 | A block is 260 bytes from a pool of 70 | VERIFIED | `audio_block_t` 1+1+2+256 bytes (`libs/Audio/AudioStream.h:60-66`); `AudioMemory(70)` for non-IMX (`mtAudioEngine.cpp:195`) |
| A7 | CPU: under 1% for capture, 1 to 2% for a sum node, no cycle counts | ESTIMATE | Correctly labelled a guess. The figure 522,000 cycles per block is right: 180e6 x 128 / 44,117.65 = 522,240 |
| A8 | Capture runs on the same clock as playback, in the same interrupt | VERIFIED | `Tracker.cpp:171-199` (tick, `engine.update`, `update_all` in one function); `Recorder::update` first (`mtAudioEngine.cpp:288`) |
| A9 | Internal capture has no input round trip, so the compensation constant would not apply | ESTIMATE | Inference, not checkable from code alone |
| A10 | Stock start of recording is called from the main loop and block-rounded | VERIFIED | `Recorder::record()` just sets `isActive` (`Recorder.cpp:84-86`), called from UI code (`fSampleRec.cpp:1520-1575`, `fFourTrack.cpp:182,331`). Our test build already avoids needing an in-interrupt arm: it starts early and trims with `firstBlockCallback` (`Recorder.cpp:160-163`, `fFourTrack.cpp:323-336`) |
| A11 | Performance FX pad handler selects FX slots and starts no samples (`fPerformance.cpp:872-912`) | VERIFIED | `fPerformance.cpp:835-870` (no `handleNoteOn`); line range wrong |
| A12 | `padToBeatNoteConverter` maps pad 0-47 to `FIRST_BEAT_SLICE_NOTE + pad`, used from `mtPadBoard.cpp` | VERIFIED | `padToBeatNoteConverter.cpp:6-12`; `mtPadBoard.cpp:202-205`; constant 24 (`mtStructs.h:360`) |
| A13 | Pad press arrives as a UI event on the main loop (`Tracker.cpp:165-179`) | VERIFIED | `Tracker.cpp:152-168`, `mtInterface.update()` at `:166`; pad interrupt only sets a flag (`PadsImpl.cpp:21-24`). The scan code itself not traced |
| A14 | Pattern editor `functPads` calls `handleNoteOn(GRID_INSIDE_PATTERN, note, -1, pad)` at `fPatternEdit.cpp:4716` | DIFFERS | The call is at `:4640` (play mode). `:4719-4741` is the Instrument column in record mode. Other screens use `GRID_OUTSIDE_PATTERN` (right in substance) |
| A15 | Sample Playback and Instrument editor pads end in `handleNoteOn` (`fSamplePlay.cpp:641`, `fIinstrEdit.cpp:2967`) | VERIFIED | `fSamplePlay.cpp:593-595, 687`; `fIinstrEdit.cpp:2914`. Both line numbers wrong |
| A16 | The "import screens" also play samples through `handleNoteOn` | DIFFERS | Sample Loader pad playback is disabled (`fSampleImport.cpp:1998-2011`); only the Preview button plays the selected slot (`:1577-1606, 1643-1660`) |
| A17 | `Sequencer::handleNoteOn` at `mtSequencer.cpp:2754` | DIFFERS | At `:2551`. Behaviour as described: `noteOff()` then `noteOn(...)` on `instrumentPlayer[track]` (`:2611-2620`, `:2766-2775`, `:2817-2825`; doc cites `:2965-2978`, `:3021-3033`) |
| A18 | A live pad hit uses one of the eight track voices, the same as the sequencer | VERIFIED | `mtSequencer.cpp:2782-2825` and 1.1 above. Added detail: first free track from the selection start, skips muted tracks, no stealing |
| A19 | `startNoteWithoutInstrument` takes a free voice for raw-buffer preview (`mtPadBoard.cpp:28-42`; callers `fSampleRec.cpp:686`, `fSampleEdit.cpp:784,3321`) | VERIFIED | Function right. Callers are `fSampleRec.cpp:652`, `fSampleEdit.cpp:655, 1998`. It uses a separate allocator (`mtPadBoard.h:45`) from `handleNoteOn` |
| A20 | `playerEngine::noteOn` (`mtAudioNoteOnHandle.cpp:84-`) sets up envelopes, filter, gain, pan, sends under `AudioNoInterrupts()` and runs on the main loop | VERIFIED | `mtAudioNoteOnHandle.cpp:76-221` (`AudioNoInterrupts` `:89`). Main loop for pads: yes. For MIDI: no, it runs in the audio interrupt (`Tracker.cpp:182`) |
| A21 | Voice is `memoryPlayer -> bitDepth -> filter -> amplifier -> stereoMirror -> panningAmplifier`, one chain per channel (`AudioVoice.cpp:32-44`) | DIFFERS | Original: one mono chain `memoryPlayer -> bitDepth -> filter -> amplifier -> monoToStereo`, plus RMS (`AudioVoice.cpp:4-11`). The two-channel `panningAmplifier` chain is IMX only (`:13-23`). `stereoMirror` does not exist in the source |
| A22 | Voice outputs to `tracksAndSynthMixer` from `panningAmplifier` (`mtAudioEngine.cpp:334-346`) | DIFFERS | From `MonoToStereo` outputs 0 and 1 (`mtAudioEngine.cpp:349-356`); pan is done by its gains (`:1750-1770`) |
| A23 | Reverb and delay sends come off the filter, before pan and before the dry mix | VERIFIED | `mtAudioEngine.cpp:357-366` (filter output to `preFxMixer`; before the amplifier too) |
| A24 | `stereoMirror` feeds the limiter sidechain mux (`:428-440`) | DIFFERS | `amplifier` feeds `limiterSidechainMux` input idx (`mtAudioEngine.cpp:453-457`) |
| A25 | Per-voice export taps are on `panningAmplifier` (`:679-694`) | DIFFERS | On `MonoToStereo` L/R (`mtAudioEngine.cpp:729-743`). 16 queues for 8 voices: right |
| A26 | `tracksAndSynthMixer -> tracksAndSendsMixer` input 0, delay 1, reverb 2, line-in monitor input 3 | VERIFIED | `MixerAndFxs.cpp:14-26`; `mtAudioEngine.cpp:437-449` (`kTracksAndSendsMixerInputIdx = 3`). New in our tree: input 4 is the loop player (`MixerAndFxs.h:23-30`) |
| A27 | `tracksAndSendsMixer -> limiter -> finalMixer` input 0; `finalMixer` also takes SD preview, test tone, metronome | VERIFIED | `AudioOutput.cpp:24-27`, `AudioOutput.h:19-26`, `mtAudioEngine.cpp:551-562, 706-725` |
| A28 | `finalMixer -> outputStream` and USB out | DIFFERS | `outputStream` yes (`AudioOutput.cpp:29-32`); the USB output exists only on IMX (`:34-62`) |
| A29 | Audio runs in the output-DMA interrupt once per 128-sample block (2.90 ms) | VERIFIED | `Tracker.cpp:171-199`; 128 / 44,117.65 = 2.901 ms |
| A30 | Tap table: `tracksAndSynthMixer` has only an RMS meter and no queue; master queue holds metronome and monitor | VERIFIED | `MixerAndFxs.cpp:30` (`dryMixRMS`); master queue `mtAudioEngine.cpp:797-806`; `AudioOutput.h:19-26`. Added: the master also carries the loop player and the line-in monitor |
| A31 | A queue keeps blocks only after `begin()` and drops them after `end()` | DIFFERS | `begin()` clears and enables (`record_queue.h:44-48`); `end()` only clears the enabled flag (`:53`); blocks already queued stay until read or cleared. While disabled, arriving blocks are released at once (`record_queue.cpp:92-96`) |
| A32 | Ring is 10 slots, 9 usable; full ring drops silently | VERIFIED | `QUEUE_SIZE 10` (`record_queue.h:30`); `record_queue.cpp:100-106` |
| A33 | `AUDIO_NULL_IS_SILENCE` is on (`AudioStream.h:54-56`), so an empty input yields a zero block (`record_queue.cpp:92-103`) | NOT FOUND | The symbol appears nowhere in the source. `AudioRecordQueue::update` simply returns when no block arrives (`record_queue.cpp:84-91`). The outcome still holds for voice and mixer taps for a different reason: idle voices emit zero blocks (see 1.8) |
| A34 | The queue stores the block pointer; the copy is in `Recorder::update`, one block per queue per interrupt, at the top of the interrupt | VERIFIED | `record_queue.cpp:84-106`; `Recorder.cpp:89-199`; `mtAudioEngine.cpp:288` |
| A35 | Recorder already averages L and R for mono (`Recorder.cpp:130-141`); needs both channels available (`:96-99`) | VERIFIED | `Recorder.cpp:132-155` (average at `:148-149`); `:101-104` |
| A36 | Option C: master queue is probably one block late because of an update-list rule in `AudioStream::connect` (`AudioStream.cpp:239-254`, `mtAudioEngine.h:246-247`) | DIFFERS | v1.0.0 has no such rule. Update order is construction order (`AudioStream.h:136-149`). `exportQueues` is declared before `output` (`mtAudioEngine.h:251-252`), so the master queue updates before `finalMixer` and probably sees the previous block. The conclusion holds by static reasoning, not run. Voice taps: voices are global objects built before `engine` (`mtAudioEngine.cpp:24-25`), so no delay. ESTIMATE on the outcome |
| A37 | Exporter renders faster than real time using `renderMode` and virtual timers | VERIFIED | `mtAudioExport.cpp:65-95`; `AudioStream.cpp:46, 329-352` (doc cites `:72-101, 724-758`, `:342-371`) |
| A38 | The WAV exporter is the only offline render; it writes SD files, not sample memory | DIFFERS | Pattern editor "Render Selection" renders to `Export/<project>/Selection` and then imports the file into a free instrument slot (`fPatternEdit.cpp:1906-1957`, `gPatternEdit.cpp:1284`, `fileManagerExport.cpp:106`). It is offline and SD-based, pattern events only |
| A39 | `instrumentPlayer[i].update()` runs only when not recording (`mtAudioEngine.cpp:277-302`) | VERIFIED | `mtAudioEngine.cpp:286-319` (`recorder.update()` `:288`, guard `:290`, loop `:312-315`) |
| A40 | That update handles end-of-note detection, release, envelopes, LFOs, parameter refresh | DIFFERS | It handles the secondary envelopes (cutoff, pan, wavetable, granular, finetune), LFO tempo sync, parameter refresh and UI flags (`mtAudioUpdateHandle.cpp:7-70, 160-193, 196-222`). The amplitude envelope (attack, decay, sustain, release) and end of sound are done inside `AudioPlayMemory::update` and `envelopeUpdate` (`play_memory.cpp:126-160`, `effect_envelope.cpp:439+`), which the recorder does not gate |
| A41 | So a pad hit during recording would not release, end or follow its envelope correctly | DIFFERS | It starts, follows its amp envelope, releases and ends. Frozen: filter, pan, finetune and similar envelopes, LFOs, and live parameter changes. A plain drum sample with default instrument settings would not sound different (ESTIMATE; by reading) |
| A42 | Removing the freeze is "Required" (sections 5, 9.3) | DIFFERS | Not required for plain pads; needed only if pad instruments use the frozen features |
| A43 | The freeze "was presumably made to save CPU while recording" | ESTIMATE | No comment in the code says why |
| A44 | RAM: exporter's 768 KB scratch is only used by offline render | DIFFERS | `externalRamScratchBuff` (768 KB, `mtStructs.h:75`) is shared by the exporter (`mtAudioExport.cpp:32-37`), the SD preview stream (320 KB, `mtAudioEngine.cpp:14-22, 281`), file lists (`fProjEdit.cpp:643`) and the effect loader |
| A45 | 4 x 30 s mono Parts are 10.1 of 11.25 MiB | VERIFIED | 4 x 30 x 44,118 x 2 = 10,588,320 B = 10.10 MiB. Note the firmware uses 24 s loops and 26 s regions (`fFourTrack.cpp:27-29`) = 9,176,544 B |
| A46 | Pad hit is placed to the nearest block; the delay is the same for every hit but jitter cannot be removed | ESTIMATE | Not measured; for pads the dispatch is in the main loop. For MIDI the note starts at the next block boundary (`Tracker.cpp:182`) |
| A47 | Live pads are not quantised to the grid; the sequencer ticks in the interrupt (`Tracker.cpp:204`) | VERIFIED | `Tracker.cpp:194` (`sequencer.tick`); pad path has no quantising (`mtSequencer.cpp:2782-2825`) |
| A48 | No bounce, freeze or resample-to-sample feature | DIFFERS | See A38. Also `tuneResample` exists as an import-time half-rate "lo-fi" import (`SampleImport.cpp:394`, `play_memory.cpp:877, 1138`) |
| A49 | The only real resampler is a cubic interpolator on import (`AudioDataUtilities.cpp:60-80,82-`; `fileTransfer.cpp:283,728`) | VERIFIED | `AudioDataUtilities.cpp:46-110`; callers `fileTransfer.cpp:249, 648, 1178, 1434` |
| A50 | Exporter header says 44,100 Hz although the hardware runs 44,117.6 Hz | VERIFIED | `mtAudioExport.cpp:365, 519`; `Tracker.cpp:184-193` (`kAudioSampleRate = 44117.6470588f`). Imported samples are also converted to 44,100 (`fileTransfer.cpp:1126-1131`) |
| A51 | Playback pitch uses interpolation, `setInterpolationEnable` (`mtAudioEngine.cpp:2040-2045`) | VERIFIED | `mtAudioEngine.cpp:1040-1046, 2110-2115`; project setting `connfigMenuActions.cpp:114-118` |
| A52 | Recommended: reserve voices (5-8) for pads, Parts on 1-4, so the capture never holds the loop | DIFFERS | Parts are not voices (loop player, 1.4). All 8 voices are pad voices, and a `tracksAndSynthMixer` tap is pad-only |
| A53 | Option B (dry mix of all voices) is wrong because it re-records the loop that is playing | DIFFERS | In this tree the loop enters at `tracksAndSendsMixer` input 4 (`mtAudioEngine.cpp:152-158`), after `tracksAndSynthMixer`. A tap there contains no Parts |
| A54 | Sends are before the dry sum, so captured pads carry no reverb or delay | VERIFIED | `mtAudioEngine.cpp:357-366` |
| A55 | 70-block pool might run dry with four Parts and an extra queue | ESTIMATE | Not checkable from code; counters exist (`AudioMemoryUsageMax`, `AudioStream.h:118`) |
| A56 | Stereo pad sample summed to a mono Part (plain average) | DIFFERS | On the Original samples and voices are mono; stereo only appears after pan into L and R. The average halves a hard-panned voice (`mtAudioEngine.cpp:1750-1770`, `Recorder.cpp:148-149`) |

### 2.2 `docs/research/screen-survey.md` (pad, instrument, memory, MIDI, "cannot be reused")

| ID | Claim (one line) | Status | Evidence / correct statement |
|---|---|---|---|
| B1 | v1.0.0 streams samples of 256 KB or more, keeps the first 32,768 frames (0.74 s, 64 KB mono) in One-Shot and Loop modes | VERIFIED | `CHANGELOG.md:43-49`; `mtStructs.h:775-793`; `fileManagerInstrument.cpp:141-153` (threshold is `>=`) |
| B2 | About 40 long samples fit in the 2.6 MB | DIFFERS | 2,619,936 / 65,536 = 39.98, so 39 (40 needs 1,504 B more). Applies only to files of 256 KB or more; shorter samples cost their full length |
| B3 | The real limits are 48 instrument slots, 8 streaming voices at once, and SD speed | DIFFERS | 48 slots: right (`mtStructs.h:105, 995`). The 8 streaming voices are the same 8 voices as total polyphony (1.4), not an extra limit. SD speed unmeasured (ESTIMATE). Missed limits: pool size (B2) and full-length import (1.5) |
| B4 | Pool is 11.25 MB (`SAMPLE_MEMORY_SIZE`, `mtStructs.h:65`) | VERIFIED | `45 * 256 * 1024` = 11,796,480 B |
| B5 | Sample Loader: pressing a pad previews the selected slot, pitched by pad (`playFromPads`, `fSampleImport.cpp` ~1597) | DIFFERS | Pad playback is disabled (`fSampleImport.cpp:1998-2011`). Only the Preview button plays it, at note 60 (`:1577-1606, 1643-1660`) |
| B6 | Sample Loader checks `used_memory + size > max_memory`; sampleRecorder does the same | VERIFIED | `fSampleImport.cpp:1501-1524, 1876`; `fSampleRec.cpp:1289, 1842`. The Loader uses full file size, ignoring streaming (1.5) |
| B7 | Our four 26 s regions take 9.18 MB and leave 2.6 MB, roughly 30 s mono (`fFourTrack.cpp:27-29`) | VERIFIED | 9,176,544 B; 11,796,480 - 9,176,544 = 2,619,936 B = 29.7 s at 44.1 kHz. But nothing in code reserves or enforces it (1.7) |
| B8 | Sample Editor edits only slots 0-47 (`fSampleEdit.cpp:125-139`) | VERIFIED | `fSampleEdit.cpp:125-151` |
| B9 | Its buffers sit at `EDIT_BUFF_OFFSET` (`mtStructs.h:85`, `Tracker.cpp:28`), 7.5 MB of the 11.25 MB pool, over where Parts live | VERIFIED | Offset = 11,796,480 - 2 x 1 x 3,932,160 = 3,932,160; the two buffers cover the top 7.5 MB (`mtEffect.h:33-37`). This also overlaps loaded samples above 3.75 MB |
| B10 | It refuses samples longer than `EDIT_BUFF_SIZE / 2` (`fSampleEdit.cpp:88-92`) | VERIFIED | `fSampleEdit.cpp:88-91` (length in frames > 1,966,080, about 44.6 s) |
| B11 | Effect code reads the global buffer (`effector/mtEffect.h:37`) | VERIFIED | `mtEffect.h:33-37`, `mtEffect.cpp:80-123` |
| B12 | Config Project Settings hold Notes scale, Root note, Notes layout (pad mapping) | VERIFIED | `configMenu.cpp:66-68, 77-79`. They are project values (`mtStructs.h:848-850`), so they live in the stock project file |
| B13 | "Pads preview" is a General item | VERIFIED | `configMenu.cpp:138-142`; it only affects the Pattern editor (`fPatternEdit.cpp:4372-4375`) |
| B14 | On the Original, stereo line-in is summed to mono for recording unless Left or Right is chosen (`configMenuDefs.h:333-342`) | VERIFIED | `configMenuDefs.h:330-343` |
| B15 | Sample Recorder sources are Line in, Radio, Mic LG, Mic HG; USB on newer hardware | VERIFIED | `sampleRecorder.h:15-23` |
| B16 | Performance does not play samples; it is mute and FX toggles | VERIFIED | `fPerformance.cpp:835-870` (FX slots), mute via soft keys `:700-708` |
| B17 | 48 sample instruments live in the 11.25 MB pool beside our Parts | VERIFIED | `mtStructs.h:105`; "beside" is not fenced (1.7) |
| B18 | A pad handler per screen calls `getNoteFromPad` then `handleNoteOn(GRID_OUTSIDE_PATTERN, ..., pad)` on `lastUsedInstrument` (`fSampleImport.cpp:1597-1607`, `fSamplePlay.cpp:675-700`) | DIFFERS | Right for Sample Playback, Instrument editor, Master, Song, popups. Not the Sample Loader (disabled, B5). Pattern editor Instrument column uses the pad as the instrument (B21). Sample Recorder and Sample Editor play raw buffers (table 1.1). Beat Slice maps pad to slice |
| B19 | One instrument at a time, pitched chromatically across the 48 pads by scale, root and layout | VERIFIED | `mtPadBoard.cpp:133-178, 198-210`; exception Beat Slice (`:202-205`) |
| B20 | There is no "pad 5 plays sample 5" mapping; none found | DIFFERS | The Pattern editor does it (1.2): `fPatternEdit.cpp:4631-4636, 4719-4741, 4743-4757`; `mtSequencer.cpp:2575-2579, 2713`. No table exists, but the pad-number-is-instrument rule does |
| B21 | Our screen has no pad handler; `fFourTrack.cpp` only clears pad lights (line 138); 21 uses of `setPadsGlobal` | VERIFIED | `fFourTrack.cpp:138`; 21 call sites; `interface.cpp:415, 437` |
| B22 | Pad voices share the stock voice allocator (8 pad voices, `mtPadBoard.h`) | DIFFERS | `mtPadBoard.voices[8]` only serves raw-buffer previews (`mtPadBoard.cpp:28-52`). Instrument pads use the sequencer's track-to-voice rule (1.4), and Parts use no voice |
| B23 | Finger drums need a kit mapping that stock lacks | DIFFERS | The mapping primitive exists (B20); only a dedicated screen and a way to store the choice are missing |
| B24 | Recording pads onto a track: pad bus on reserved voices recorded dry (medium-high confidence) | DIFFERS | Dry capture is feasible; reservation is not needed; update freeze not required (see A40-A42, A52, A53) |
| B25 | Whether Parts pass through the stock mixer, so Master and Track Mixer levels apply, was not traced (L) | VERIFIED | Traced now: Parts enter `tracksAndSendsMixer` input 4, then limiter and `finalMixer` (`mtAudioEngine.cpp:152-158`, `MixerAndFxs.cpp:14-26`). Master limiter applies; per-voice track volume (`mtAudioEngine.cpp:1781`) and the reverb and delay sends do not apply to Parts |
| B26 | "MIDI/USB: hide in v1" (Config) is harmless | DIFFERS | Notes-in defaults to Off (`mtConfig.cpp:517`). If the MIDI menu is hidden, MIDI input cannot be turned on. Ticket 01 says MIDI settings stay visible |
| B27 | Sample Recorder has undo of 5 levels (`undoCount = 5`) | VERIFIED | `sampleRecorder.h:49` |
| B28 | 14 module slots, 11 mapped buttons (`interface.cpp` 56-85) | VERIFIED | `interface.cpp:55-86` (stock has 13 slots; ours added the 14th) |

### 2.3 `docs/readiness-matrix.md`

| ID | Claim (one line) | Status | Evidence / correct statement |
|---|---|---|---|
| C1 | Recorder has no internal source today | VERIFIED | `Recorder.h:13-17` |
| C2 | Audio graph already has record queues on every voice and the master, built for the exporter | VERIFIED | `mtAudioEngine.cpp:729-743, 797-806` |
| C3 | Best option is a pad-bus source on reserved voices, recorded dry into the Part buffer | DIFFERS | Dry capture is right. Reservation unnecessary (A52, A53) |
| C4 | It needs two changes: stop the recorder freezing voice updates, and arm the start inside the audio interrupt | DIFFERS | Freeze only affects secondary envelopes, LFOs and live parameter changes (A40-A42). Our test build uses an early start plus a first-block callback instead of arming in the interrupt (`fFourTrack.cpp:323-336`, `Recorder.cpp:160-163`) |
| C5 | Evidence 1: stock recording writes to the start of the whole pool, must be pointed at each Part's memory, and must stop freezing other voices | VERIFIED | Pool start: `fSampleRec.cpp:1530-1531`. Part regions: `fFourTrack.cpp:236`. The freeze part: see C4 |
| C6 | Evidence 3: four 30 s mono Parts take about 10.1 of 11.25 MiB | VERIFIED | Arithmetic as A45. The firmware regions are 26 s, loop limit 24 s (`fFourTrack.cpp:27-29`) |
| C7 | Evidence 6: SDRAM 92.91% used, about 1.13 MiB free | ESTIMATE | Not re-measured here. 16,777,216 - 15,587,616 = 1,189,600 B = 1.13 MiB follows from the stated numbers (`docs/build-notes.md`). The build folders in the tree are stale and were not used |
| C8 | Feature 16 (pads captured on a track): fits hardware "Partly", firmware path "Partly", small change | ESTIMATE | Consistent with the code, not measured |

### 2.4 `wayfinder/tickets/01-core-workflow-and-device.md`

| ID | Claim (one line) | Status | Evidence / correct statement |
|---|---|---|---|
| T1 | Four mono tracks, about 30 s each, all in RAM | DIFFERS | Firmware: loop at most 24 s, 26 s region per track (`fFourTrack.cpp:27-29`). 30 s each would leave 1,208,160 B, about 18 streamed samples |
| T2 | Stock sample and pad playback is kept | VERIFIED | Present, except that Sample Loader pads are disabled (B5) |
| T3 | Pad mode: stock instrument slots 0 to 47 map one to one onto the 48 pads | DIFFERS | Not a stock mode. Pad number equals instrument index only in the Pattern editor Instrument column (B20). Elsewhere pads play `lastUsedInstrument` |
| T4 | Keys mode: one sample across the pads at different pitches (stock behaviour) | VERIFIED | `mtPadBoard.cpp:133-178, 198-210` |
| T5 | Mode chosen per sample | NOT FOUND | No per-instrument pad-mode field (`mtStructs.h:728` fixed 372-byte struct); would need our own storage |
| T6 | Samples of 256 KB or more stream; about 0.74 s (64 KB mono on the Original) stays in memory, One-Shot and Loop modes | VERIFIED | B1 |
| T7 | Slice, Beat Slice, Granular, Wavetable stay fully in memory | VERIFIED | `mtStructs.h:779-790`; `CHANGELOG.md:49` |
| T8 | Memory is therefore not the limit for a pad bank | DIFFERS | With regions at the top and no fence: nothing protects loaded samples (1.7). With a fence at 2.62 MB: 39 streamed samples maximum, and import needs the full sample size free (1.5) |
| T9 | Limits are 48 slots, 8 simultaneous streaming voices, SD speed | DIFFERS | See B3 |
| T10 | MIDI input must trigger the same sounds as the pads | DIFFERS | Works from our screen already in the default voice mode, but: Notes-in default Off; pitch ignores scale, root, layout; velocity ignored by default; channel is only a filter; runs in the audio interrupt; needs Pattern editor state (1.3) |
| T11 | Pads record as audio: sample hits in record mode | ESTIMATE | Design decision. Path exists (1.8) |

## 3. Things this got wrong or missed

1. **Pad N to instrument N already exists** in the Pattern editor (1.2). The survey and ticket describe it as missing or "stock".
2. **Sample Loader pads are switched off** in v1.0.0 (`fSampleImport.cpp:1998-2011`). The survey's "pressing a pad previews the selected slot" is false; only the Preview button plays.
3. **The voice chain in the research is the Tracker+/Mini one.** On the Original there is no `panningAmplifier` or `stereoMirror`; the taps are on `MonoToStereo` and `amplifier`. No USB output.
4. **`AUDIO_NULL_IS_SILENCE` and the update-list rule do not exist in v1.0.0.** They came from the unreleased branch (the stale build files contain them). Voice taps still capture silence, because idle voices emit zero blocks.
5. **The update freeze is overstated.** Amp envelope, release and end-of-note run in the audio object. The required fix is not required for plain pads.
6. **Parts are not voices.** The loop player is separate. No voice reservation is needed, and a `tracksAndSynthMixer` tap is pad-only. The doc's rejection of Option B rests on a wrong premise.
7. **Nothing fences the four regions.** `max_memory` stays at 11.25 MiB; samples pack from the bottom and will run into track 4's region once `used_memory` passes 2,619,936 B. The reset at `fileManagerProject.cpp:187` would also undo any fence set once.
8. **Streamed samples still need full-length room at import time**, and the Sample Loader's meter ignores streaming (`SampleImport.cpp:155-166`; `fSampleImport.cpp:1396, 1465-1530`).
9. **MIDI facts missed:** processed in the audio interrupt; default Off; ignores pad scale, root and layout; velocity ignored by default; channel is only a filter; depends on Pattern editor state.
10. **Pads on the Original have no velocity** (`mtActions.cpp:56`).
11. **Voice allocation quirks:** 8 pads held is the maximum, no stealing; released pads' tails are cut by the next hit on the same voice; muted tracks (`trackMute`) silence pads; `trackVolume` scales pad voices.
12. **Pad mapping settings live in the stock project** (`mtStructs.h:848-850`). `padNotes` is only filled by `setPadNotes`, called at project load (`fProjEdit.cpp:382`) and on a Config change. If our project load replaces the stock one, pads would all play note 0 unless we call it. `mtPadBoard::voicesCount` is also only set at `fProjEdit.cpp:385`.
13. **A bounce exists:** Render Selection (A38). It is offline, but it is a render-to-instrument path.
14. **Stereo samples cost nothing extra on the Original**; the research never says so. Stereo files import as their left channel only.
15. **Other writers in the upper pool:** Sample Recorder (whole pool), Sample Editor and effects (top 7.5 MB), auto-slice (`mtSliceManager.cpp:116`), `.it`/`.mod` import (`fileManagerImportIt.cpp:554, 611`, `fileManagerImportMod.cpp:208`).
16. **Line numbers in `pad-recording-path.md` are almost all from another tree** (examples: A14, A17, A25, A39, A50). The conclusions need re-citing before anyone relies on them.
17. **Region size mismatch:** documents say about 30 s per track; code is 24 s loop, 26 s region.

## 4. Limits we declared that the code does not actually impose

- "About 2.6 MB left for samples / roughly 30 s of pad samples": a result of where we put our regions, not a limit in the code. The code does not enforce it at all (1.7). Either enforce it or it is a corruption risk, not a limit.
- "Pad voices share the stock allocator, 8 pad voices": there is no pad-specific voice limit and `padBoardMaxVoices` is unused. The real limit is 8 voices total, shared by track index.
- "8 simultaneous streaming voices" as a separate limit: it is the same 8 voices.
- "Reserve voices for pads, Parts on 1 to 4": nothing requires it.
- "Update freeze must be removed": not for plain pads.
- "Must arm the recording start inside the audio interrupt": our test build does not do that.
- "No bounce feature": one exists (Render Selection).
- "Pads and MIDI cannot play a chosen instrument per pad": possible by writing `lastUsedInstrument` before `handleNoteOn` (no engine change).
- "Stereo sample costs": not applicable on the Original.
- "Four 30 s tracks": the code already limits to 24 s loops.

## 5. Not checkable here (needs the device)

1. Whether `trackMute` or leftover pattern-editor state affects pads on our screen in practice.
2. Whether quick repeated hits on one voice sound acceptable (tail cut).
3. Streaming start-up and underrun behaviour with several streamed pads plus a Part recording, and with SD writes at the same time.
4. Block pool headroom with four Parts, a pad capture queue and delay and reverb running.
5. Whether pad voice taps record continuous silence between hits (expected from the code, not heard).
6. MIDI timing and jitter from the audio interrupt, and whether `sendSelection` and the pattern refresh inside the interrupt are harmless from our screen.
7. All CPU figures in the research (no cycle counts exist).
