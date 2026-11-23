# Stock screen survey for the four-track recorder (v1.0.0 source)

Source read: `core/interface/*` of v1.0.0 with our Four-Track screen (paths below are relative to `core/interface/`) plus `core/source/mtStructs.h`. Read-only; nothing built or run. Everything here comes from reading code, not from the device.

Confidence key: **H** = read directly in code, **M** = read, but I inferred how pieces connect, **L** = guess.

## 1. Summary table

| # | Stock module (button) | Screen title(s) | Verdict | One-line reason |
|---|---|---|---|---|
| 0 | `projectEditor` (File) | "File", "File - Open Project", "File - Save Project", "File - Import Mod", "File - Export" | **Replace** with our Project screen (reuse keyboard and list widgets) | Stock saves patterns/instruments/song; our format is a folder of WAV Parts (ticket 16) |
| 1 | `sampleImporter` (Sample Load) | "Sample Loader" | **Keep and trim** (this is how pad samples get in) | Only stock way to put SD WAVs into the 48 instrument slots, with pad preview and import trim |
| 2 | `patternEditor` (Pattern) | "Pattern", "Render Selection" | **Remove** | Dropped in v1; biggest sequencer UI (7,099 lines) |
| 3 | `samplePlayback` (Sample Play) | "Sample Playback" | **Remove from buttons; fold start/end/play mode into the Pad Sound page** | Loop, slice, granular, wavetable are not needed for finger drums |
| 4 | `songEditor` (Song button) | "Song" | **Remove** | Dropped in v1. The Song button is already given to our screen in the test build |
| 5 | `instrumentEditor` (Params) | "Instrument Parameters 1/2", "Instrument Automation 2/2", "Synth Parameters", "Synth Macros", "MIDI Instrument Parameters" | **Keep and trim** to one "Pad Sound" page | Volume, pan, tune, envelope, filter, sends per pad sound; synth, MIDI, automation go (8,875 lines, the largest module) |
| 6 | `sampleEditor` (Sample Edit) | "Sample Editor" (+ effect screens) | **Replace** with a new Part Editor | Bound to instrument slots and to a 7.5 MB scratch buffer that overlaps our Parts |
| 7 | `configEditor` (Config) | "Config", "Info" | **Keep and trim** | Device-wide items we still need (display, pad lights, line-in channel, firmware update, reset) |
| 8 | `sampleRecorder` (Sample Rec) | "Sample Recorder" | **Replace** with our Input setup screen (keep the recorder engine) | Writes to an instrument slot; we record into Parts |
| 9 | `performanceMode` (Performance) | "Performance FX" | **Remove** (v2 idea: live FX on Parts) | It is NOT pad sample playback; it is mute and FX toggles on pattern tracks |
| 10 | `imageViewer` (no button) | folder name or manual title | **Remove** (or leave unreachable) | Only shows manual images opened from Config > Info |
| 11 | (slot is `nullptr`) | none | nothing | Empty slot in the `modules` array |
| 12 | `masterParams` (Master) | "Master 1/3", "Master Reverb/Delay/Limiter/EQ", "Track Mixer 2/3", "Global Mixer 3/3" | **Keep and trim** the master-effect pages; **Replace** the track mixer | Reverb, delay, limiter, EQ, saturation, bass boost are what we have for tone; the mixer is for 8 pattern tracks |
| 13 | `fourTrack` (our test screen, on Song button) | none yet | **Replace** by the designed main screen | First prototype |

Modules array and button table: `core/interface.cpp` lines 56 to 85 (14 slots, 11 buttons mapped). Boot goes straight to `patternEditor` (line 267) after the project load, so that line must change.

## 2. Rationale per module

### projectEditor (File)
- **For:** new, open, save, save as, recent list, import a tracker module (`.mod`/`.it`), export stems (Song Stems, Patt Stems, "As .it"), delete a project. Strings in `projectEditor/gProjEdit.cpp` (H).
- **Controls:** project browser, text keyboard for names (Enter, Auto Name, Insert confirms, Delete backspaces), "This action will stop the pattern" popup (H).
- **Depends on:** the whole stock project: patterns, song, 48 instruments and their samples in the pool (M).
- **Verdict:** Replace. Reuse the keyboard and list widgets (M) for naming and for the project list. Drop Mod import and all stem export. WAV export is new work, not a stock feature (M).

### sampleImporter (Sample Loader)
- **For:** browse the SD card, preview a file, and import it into a numbered instrument slot (`sampleImporter/sampleImporter.h`, `fSampleImport.cpp`) (H).
- **Controls:** file tree, instrument list (48 slots), Add, Add next, Enter, Rename, Delete, Auto name, Preview volume, per-import Start/End trim and "Low quality" import (`gSampleImport.cpp`) (H). It also has copy/paste of instruments and "delete unused instruments" (H, names only).
- **Pads:** pressing a pad previews the selected slot, pitched by pad (`playFromPads`, `fSampleImport.cpp` ~1597) (H).
- **Memory:** shows a memory meter; checks `mtProject.used_memory + size > max_memory` (sampleRecorder does the same check) (H). The pool is 11.25 MB (`SAMPLE_MEMORY_SIZE`, `mtStructs.h:65`).
- **Budget:** our four 26 s regions (`fFourTrack.cpp:27-29`) take 9.18 MB. That leaves about 2.6 MB, roughly 30 s of mono pad samples at 44.1 kHz (arithmetic from code constants, H on the constants, M on the conclusion).
- **Verdict:** Keep and trim. Remove copy/paste/delete-unused if flash is tight; add a clear "remaining sample memory" figure that counts the Parts.

### patternEditor (Pattern)
- **For:** the tracker grid: 255 patterns, 8 sample tracks plus 16 MIDI tracks, note/instrument/volume/FX columns, live record, euclid/random/fill tools, "Render Selection" (H).
- **Verdict:** Remove. The sequencer engine underneath stays because pad notes run through it (see section 4).

### samplePlayback (Sample Play)
- **For:** playback shape of the current instrument. Modes: 1-Shot, Forward Loop, Backward Loop, Pingpong Loop, Slice, Beat Slice, Wavetable, Granular (`samplePlayback/samplePlayback.h:14`, `gSamplePlay.cpp`) (H).
- **Controls:** Start, End, Loop Start, Loop End, Zoom, slice count, auto-slice, window shape (Gauss, Triangle, Square), position (H).
- **Verdict:** Remove as a screen. Fold Start, End and "1-Shot vs Loop" into the Pad Sound page. Beat Slice (chopping a loop across pads) is a real finger-drum feature but not asked for; I would hold it for v2 (L on the owner's interest).

### songEditor (Song)
- **For:** 255-slot song playlist, tempo, external tempo (H, `SONG_MAX`, `mtStructs.h:146`). Remove.

### instrumentEditor (Params)
- **For:** per-instrument sound shaping. Strings include Volume, Panning, Tune, Finetune, Envelope (Attack, Decay, Sustain, Release), Filter Type (Low-pass, High-pass, Band-pass), Cutoff, Resonance, Reverb send, Delay send, Overdrive, and a "Bit depth" control (`gInstrEdit.cpp`) (H for strings). The "Bit depth" item looks like a per-instrument bit-crush, which would serve the lo-fi goal (M, not traced).
- **Other parts:** LFO, WT, Granular, Synth Parameters, Synth Macros, MIDI mapping, Instrument Automation (H). No synths wanted.
- **Verdict:** Keep and trim to one page, "Pad Sound": volume, pan, tune, envelope, filter, sends, bit depth. Drop the rest.

### sampleEditor (Sample Edit)
- **For:** destructive editing of one instrument's sample: Start, End, Zoom, Undo, Effect, Preview, Apply (`gSampleEdit.cpp`) (H).
- **Effects (18):** Normalizer, Crop, Reverse, Amplifier, Overdrive, Delay, Bitcrusher, Chorus, Flanger, Equalizer, Limiter, Compressor, WT Smoother, Timestretch Beat, Timestretch Note, Fade In, Fade Out, Modular (`sampleEditor.h:11-15`) (H).
- **Why it cannot be used as is on our Parts:**
  1. It edits `mtProject.instrument[lastUsedInstrument].sample`, only slots 0 to 47 (`fSampleEdit.cpp:125-139`) (H). Our Parts are not instruments.
  2. Its working and undo buffers sit at `EDIT_BUFF_OFFSET` (`mtStructs.h:85`, `Tracker.cpp:28`), 7.5 MB of the 11.25 MB pool, directly over where Parts live. Ticket 02 already says these buffers have to go (H).
  3. It refuses samples longer than `EDIT_BUFF_SIZE / 2` (`fSampleEdit.cpp:88-92`) (H).
  4. The effect code reads the global buffer (`effector/mtEffect.h:37`), so reusing the DSP on a Part needs a rewrite of the entry points (M; I did not read the DSP internals).
- **Verdict:** Replace with a **Part Editor**. We need: a zoomable waveform, trim start/end, the offset editing already designed, one undo (the stock recorder and editor have 5 undo levels, `sampleRecorder.h:` `undoCount = 5`, which we may not afford), plus fades and normalize (small, useful). Effects like Chorus, Flanger, Delay and Timestretch: Remove. Re-use its screen layout and waveform/zoom drawing (M).
- **Pad samples:** trimming at import time (Sample Loader Start/End) covers most needs, so pads do not require the Sample Editor (M).

### configEditor (Config)
- **For:** a menu tree (`configMenu.cpp`) (H). Groups and items:
  - Project Settings: Performance presets, Pattern divider, **Notes scale, Root note, Notes layout** (pad mapping), Anti-aliasing, Limiter mode, Sends mode, Extra headroom, Snap to zero.
  - General: Radio region, Display brightness, theme, font, **Pads brightness**, Pads preview, Recording options, Recording Threshold, Numbering mode, Pattern arrangement, Pattern top info, **Line in channel**.
  - Metronome: State, Pre roll, Time signature, Volume.
  - MIDI, USB (USB only on newer hardware), **Firmware** (update, version, build, Reset config), Fn buttons binding (Mini only; the popup code is under `#ifdef TRACKER_MINI`), **Info** (About, manuals).
- **Line in channel** matters: on the Original, stereo input is summed to mono for recording unless Left or Right is chosen (`configMenuDefs.h:333-342`) (H). Our Input screen must expose it or point to it.
- **Verdict:** Keep and trim. Keep Display, Pads brightness, Pad notes mapping (if pads stay playable), Line in channel (or move to Input setup), Firmware update, Reset config, About. Hide pattern items, Recording options, radio, Fn binding, manuals. MIDI/USB: hide in v1 (map says not yet specified). Put the metronome in our Settings screen to avoid two homes (L).

### sampleRecorder (Sample Rec): what our Input screen must match
- **Sources:** Line in, Radio, Mic LG, Mic HG (USB only on newer hardware) (`sampleRecorder.h:12-20`) (H). We need Line in, Mic LG, Mic HG; Radio goes.
- **Gain:** a 0 to 100 bar per source. Line-in maps to the codec's 0..15 steps; mic low maps to 0..25 and mic high to 0..44 (probably dB) (`fSampleRec.cpp:529-565`) (H on the code, L on the unit). Gain is stored per source in project values (`changeGainBar`) (H), so our project file needs those fields.
- **Level meter bar** and a live waveform (H). I did not find a clip indicator (L).
- **Monitor** On/Off; the implementation mutes or restores the line-in stream volume (`refreshMonitorState`, `fSampleRec.cpp:578`) (H).
- **Metronome** shortcut to Config and a pre-roll (H).
- **After a take:** Preview, Start Point, End Point, Zoom, Crop, Undo, Cancel, Save, Save & Load (needs a free instrument slot and memory) (`gSampleRec.cpp:392-466`) (H). Saves WAVs to `Recordings`, `Recordings 2`, ... as `recordingNNN` (`fSampleRec.cpp:1751-1785`) (H).
- **Blocks while a pattern plays** ("This action will stop the pattern") (H).
- **Verdict:** Replace. Keep the engine (`recorder`, `AudioInput`, codec gain calls): `fFourTrack.cpp` already uses it. Input setup must offer source, gain, level meter, monitor on/off, and the line-in channel. Gain and source should be applied by the same stock calls (`codec->setLineInLevel`, `setMicGain`) (M).

### performanceMode (Performance)
- **For:** "Performance FX": per-track Mute/Unmute and live FX slots triggered from the pad grid, with 12-column slots (`performanceMode.h`, `fPerformance.cpp:835 functPads`, `gPerformance.cpp`) (H).
- **It does not play samples.** The owner may have assumed it does. Pad sample playing is handled elsewhere (section 4) (H).
- **Verdict:** Remove for v1. A later "live FX on a Part" idea could reuse the pad-toggle pattern (L).

### masterParams (Master)
- **Pages:** Master 1/3 (Volume, Reverb, Delay, Limiter, EQ, **Saturation**, **Bass Boost**), sub-pages Master Reverb (Size, Damping, Predelay, Diffusion, Line in), Master Delay (Time, Feedback, Filter, PingPong, Sync, Line in), Master Limiter (Threshold, Attack, Release, Soft/Hard clip), Master EQ; "Track Mixer 2/3" (volume, mute, solo, for the 8 sample tracks plus synths); "Global Mixer 3/3" (Delay, Reverb, Dry Mix, **Line In** levels) (`gMasterParams.cpp`) (H).
- **Verdict:** Keep the master FX pages (trimmed) as the tone and lo-fi controls. Replace the Track Mixer with our four-track levels/mute/solo. Keep the Line In level in the Global Mixer as the monitor level, but decide whether it duplicates Input setup (L). Whether Parts play through the stock mixer, so that these levels and FX apply, is not something I traced (L, needs checking against `FourTrackSession`).

### imageViewer
- **For:** steps through images from a manual folder chosen in Config > Info (`imageViewer.h`, `fImageViewer.cpp:80`) (H). Not on a button.
- **Verdict:** Remove. 481 lines; the only value is an on-device manual.

### Shared pieces to keep
Boot/start screen, "no SD card" screen, title bar, popups (`interfacePopups`), `songTimer`, low-power shutdown, firmware-update flow (`core/interface.cpp`). `InterfaceMenuPopup` is Mini-only.

## 3. Sample editor: can any of it work on Parts, or do we build a Part editor?
Build a Part editor. Reusable at best: screen layout, zoom/waveform drawing, fade and normalize maths (M/L). Not reusable: the buffers, the instrument binding and the 18-effect framework. Needed in the Part editor beyond the design: trim start/end, one undo, fade in/out, normalize. Do not build: reverse, timestretch, chorus, flanger, delay, compressor (the master chain has what we need).

## 4. Stock instruments and pads versus the four-track recorder
- **Pad sounds are instruments.** Up to 48 sample instruments (`kSampleInstrumentsCount = 48`, `mtStructs.h:105`) live in the 11.25 MB pool beside our Parts (H).
- **How pads play today:** a pad handler per screen calls `mtPadBoard.getNoteFromPad` and then `sequencer.handleNoteOn(GRID_OUTSIDE_PATTERN, note, ..., pad)` on `mtProject.values.lastUsedInstrument` (`fSampleImport.cpp:1597-1607`, `fSamplePlay.cpp:675-700`) (H). So **one instrument at a time, pitched chromatically across the 48 pads** by scale, root and layout (Config). There is no "pad 5 plays sample 5" kit mapping (M; I searched for a pad-to-instrument table and found none).
- **Our screen has no pad handler.** `fFourTrack.cpp` only clears pad lights (line 138); the other modules register one with `ip->setPadsGlobal` (21 uses). So pads are silent on our screen today (H).
- **Recording pads onto a track:** per ticket 15 and `docs/research/pad-recording-path.md`, a pad-bus source on reserved voices recorded dry into the Part buffer (medium-high confidence there).
- **Keep pad playback reachable:** yes, on the main screen and Input setup, so the owner can play over parts and record pads.
- **Finger drums need a kit mapping that stock lacks:** one sample per pad (or per row). That is a new screen/function ("Pad Kit"), see section 6.
- **Limits:** pad voices share the stock voice allocator (8 pad voices, `mtPadBoard.h`) with whatever the Parts need; CPU is unmeasured (ticket 02).

## 5. Button map (Original Tracker)
Printed names from `core/interfaceDefs.h:157-171`. Stock mapping from `core/interface.cpp:74-86`. Proposals are mine (L on ergonomics).

| Button | Stock function | Proposed in new firmware |
|---|---|---|
| Params | Instrument Parameters | **Pad Sound** page (trimmed instrument params) |
| Performance | Performance FX | **Pads / Pad Kit** screen (which sample is on which pad) |
| File | Project browser | **Project** screen (new, save, load, export WAV) |
| Config | Config menu | **Config** (trimmed device settings) |
| Master | Master and mixer | **Master FX** (trimmed master pages) and four-track mix |
| Sample Load | Sample Loader | **Sample Loader** (trimmed; fills the pad kit) |
| Sample Play | Sample Playback | Unused in v1 (or opens Pad Sound start/end) |
| Sample Edit | Sample Editor | **Part Editor** (trim, offset, fades, undo) |
| Sample Rec | Sample Recorder | **Input setup** (source, gain, level, monitor) |
| Song | Song editor (test build: Four-Track) | **Four-track main screen** (home) |
| Note | (pattern column jump) | Select Track 1 |
| Instr | (pattern column jump) | Select Track 2 |
| Vol | (pattern column jump) | Select Track 3 |
| Fx | (pattern column jump) | Select Track 4 |
| Pattern | Pattern editor | Back to main screen (home alias) |
| Play | Play/stop pattern | Play/stop all four Parts |
| Rec | Live record | Record/arm the selected Track (first press arms, waits for the loop) |
| Shift | Modifier (screenshot on power button) | Modifier: Shift+Play = from loop start; Shift+Rec = cancel take (L) |
| Copy | Copy | Copy Part to another Track (decision pending in the map) |
| Insert | Text confirm | Confirm in dialogs and name entry; otherwise unused |
| Delete | Delete/backspace | Clear the selected Part (one undo), backspace in text entry |

Fn buttons binding in Config is Mini-only; the 8 buttons under the screen stay as soft keys (the test screen uses them for Rec, Stop, Play and Track 1 to 4).

## 6. Screens we need that have no stock equivalent
1. Main four-track screen with waveforms (stock has no equivalent).
2. Part Editor with offset in time or bars.
3. "How many bars?" prompt.
4. Settings (loop, snap window, metronome, lo-fi amount).
5. Input setup with live monitor (stock Sample Recorder is close but writes to instruments).
6. Project screen with WAV export.
7. **Pad Kit**: assign samples to pads, one per pad (not in the designed list).
8. Popups: memory low, loop too long, take cancelled, confirm clear.
9. Memory view: Parts versus pad samples, since they share one pool (not in the designed list).

## 7. Open questions for the owner
1. **Should each pad play a different sample (a drum kit), or one sample played across the 48 pads like stock?** Recommended: one sample per pad, loaded as a kit, with pitch only as an option later.
2. **How much of the 11.25 MB pool may drum samples take?** About 2.6 MB (about 30 s mono) is left after four 24 s Parts. Recommended: yes, accept about 2.5 MB; show remaining memory on Pad Kit and Sample Loader.
3. **Is a plain Part Editor enough, or do you want effects on a Part (reverse, chorus, delay)?** Recommended: trim, offset, fades, normalize only; use master effects for tone.
4. **Do you want to keep stock Config items for MIDI and USB?** Recommended: hide in v1 and keep the code so it can come back.
5. **Is losing the old projects and the pattern editor acceptable, with the stock `.ptf` kept on the SD card for recovery?** Recommended: yes (ticket 07 passed).

## 8. Where I am guessing
- Whether Parts pass through the stock mixer, so Master and Track Mixer levels apply (L).
- The units of mic gain and whether "Bit depth" is a true bit-crush (L, M).
- Reusability of the effect DSP on arbitrary buffers (M).
- Flash savings from removing modules: not measured; line counts only (7,099 pattern, 8,875 instrument, 5,159 master, 4,323 recorder, 3,114 sample edit, 4,246 playback, 2,142 song, 2,811 performance, 3,187 importer, 2,576 project, 481 image).
- Button ergonomics (Track select on Note/Instr/Vol/Fx).

Overall confidence: **medium-high** on what each stock module does and depends on; **medium-low** on resource effects (flash, CPU), which nobody has measured.
