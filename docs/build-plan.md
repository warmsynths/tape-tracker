# Build plan: the whole design, firmware and web emulator from one core

Owner request (2026-11-25): implement the whole design and all planned features as one coherent build, not a patchwork. Update the web emulator to match, so the owner tests there first and then installs the firmware.

## How the emulator can match the firmware
The firmware's screen logic and screen drawing are written as **portable C++ with no hardware calls** in `core/ft/`. Two thin adapters run the same code:
- **Tracker firmware:** draws through the display chip (FT812), records and plays through the audio engine.
- **Web emulator:** the same C++ compiled to WebAssembly (clang 18 with `wasm-ld`, installed here), drawn on a canvas with the Tracker's real font bitmaps and the Tracker's real theme colours, audio through the browser (microphone in, speakers out).

Because both run the same source, a screen cannot look or behave differently in the emulator than on the device, except for the hardware differences listed under "Known differences" below.

```
core/ft/            portable, no hardware
  Gfx.h             drawing interface: color, rect, line, dot, text
  Hal.h             what the app needs from the machine: record, play, time, input level, pads
  App.h/.cpp        the application: screens, state machine, every feature
  screens/*.cpp     one drawing routine per screen, text and boxes only
  dsp/*.h           audio maths: loop mix, alignment detector, offset, glitch, lo-fi, wav writer
firmware adapter    core/interface/fourTrack/ (FT812 Gfx, engine Hal)
emulator            prototype/emulator/ (wasm build script, page, font atlas, audio worklet)
tests               tests/host/ (the same core with a fake Hal, scenario tests)
```

## Known differences between emulator and device (cannot be removed)
- Audio timing: the browser has its own latency. Alignment accuracy and round-trip delay are only real on the device (ticket 13).
- The SD card, USB and MIDI hardware are simulated (browser storage stands in for the SD card).
- CPU and memory limits are modelled (24 s tracks, 11 MB pool, 8 voices, 4 KB display steps) but real speed is only known on the device.

## Milestones (each one ships firmware and emulator together)
1. **M0 Foundation.** Core library, wasm build, emulator shell with the hardware panel (8 buttons, jog, arrows, Shift, Rec/Play/Stop, pads), real font bitmaps. Main screen and the bars prompt running from the core.
2. **M1 Recording flow.** Starting tempo and count-in, click, source per track (line, mic, pads), pads with Keys and Pad modes, alignment (first-note detection and snap), offset, mute, Loop Mode, undo.
3. **M2 Screens.** Part Editor, Input setup, Settings, Project (new, open, save, save as, rename), Export WAV, Pads, Memory, all popups, memory-full rules.
4. **M3 Sound.** Glitch and live effects (jumps, stutter, reverse, tape and granular pitch steps), Master lo-fi (glue, saturation, wow and flutter, bit-crush).
5. **M4 Finish.** Design colours as a theme file, waveform styling, fine details checked against the design frames, track names, documentation.
6. **Later (not in this build):** chord screen, synths.

## Feature checklist (design section numbers)
Status (2026-11-25): every item below is built in the shared core, runs in the emulator and is in firmware build FT16. The device itself has not been tested yet; see `docs/first-test-build.md`.
- [x] 4.1 Main screen: states stopped, armed, recording first, recording part, playing, memory warning
- [x] 4.2 Starting tempo and count-in
- [x] 4.3 How many bars (40 to 240 BPM rule)
- [x] 4.4 Part Editor: trim, fades, normalize, offset, undo, redo, audition
- [x] 4.5 Glitch and live effects
- [x] 4.6 Lo-fi master with bypass
- [x] 4.7 Input setup: source per track, gain, level, monitor, feedback protection
- [x] 4.8 Project: new, open, save, save as, rename
- [x] 4.9 Four-Track settings (project and device)
- [x] 4.10 Export WAV, 1 to 16 repeats
- [x] 4.11 Pads and sample settings, Keys and Pad modes
- [x] 4.12 Memory
- [x] 4.13 Popups: replace, no sound, memory full (first take and later part), SD missing, unsaved changes, feedback protection indicator
- [x] Alignment: first-note detection, window, snap
- [x] Click and count-in
- [x] Undo with the previous take kept on the SD card
- [x] Per-track source, pads recorded onto a track
- [x] Stock features kept: Sample Load, Sample Rec, Sample Play, Instrument editor, Config (not redesigned)

## Rules for this build
- Build only from the official v1.0.0 release. Never touch the bootloader or the flash start address.
- Test files for the device keep names starting `PolyendTrac`, 15 to 31 characters, ending `.ptf`.
- Each screen is checked against `docs/design/screen-requirements.md` and the design frames, and against the drawing limits: steps under 2 KB, whole screen under 8 KB.
