# Claims audit against v1.0.0

Most research in `docs/research/` and the readiness matrix was written while the repo sat on Polyend's **unreleased main**. The firmware now builds on the **v1.0.0 release**. Five read-only audits re-checked every claim against v1.0.0 source. Full tables: `docs/research/audits/`. Nothing was measured on a device; figures are from reading code.

## Which documents were written against unreleased code
Treat these as suspect until corrected. Line numbers in them are mostly stale.
- `project-storage.md` (wrong: file browser, SdPaths and `/System` layout do not exist in v1.0.0)
- `display-pipeline-and-motion.md` and `screen-survey.md` (wrong pipeline, fonts, glyphs, memory claim)
- `pad-recording-path.md` (stale lines, wrong voice chain)
- `master-fx-availability.md`, `onset-detection.md`, `record-path-and-latency.md`, `cpu-ram-budget.md`, `docs/design/ui-constraints.md` (partly)
- `docs/readiness-matrix.md`, ticket 01 (memory and pad notes)

## Biggest corrections
1. **Memory.** Samples of 256 KB or more stream from SD (One-Shot and Loop modes only, mono, 8 voices) with a 64 KB pre-cache each. Short samples, Slice, BeatSlice, Granular, Wavetable stay fully in RAM. With four Parts in RAM about 18 streamed pads fit, not 40 and not 2.6 MB-worth. Parts could also stream from SD (open question, SD speed unmeasured).
2. **Nothing fences our four regions.** `max_memory` stays 11.25 MiB and loaded samples pack from pool start, so past about 2.6 MB they overlap track 4's region silently. Fix: lower `max_memory` after every project load, block other whole-pool writers.
3. **Display.** v1.0.0 uses the FT812 coprocessor path. Whole screen has about 8 KB of display list; overflow silently drops earliest controls. A waveform column costs 8 bytes, so use about 100 to 150 columns per lane. The 20 to 35 ms frame figure is right. Default font is Roboto Mono (no 14 Bold). Glyphs are Pause, Play, Loop, Perf only. Theme files forbid gradients/alpha, stock code draws them.
4. **Pads.** Pad N can play instrument N from any screen (`lastUsedInstrument = pad`, then `handleNoteOn`), no engine change. Sample Loader pad preview is disabled. Parts are not voices, so all 8 voices are free.
5. **Project storage** must be redone for v1.0.0's older FileBrowser (no rename, 256 entries per folder, 32-char keyboard cap, project file version 17).
6. **Reuse stock:** transport, tempo, MIDI clock, metronome tick (`makeMetronomeTick`), first-sound detector, codec compressor.
7. **MIDI:** runs in the audio interrupt, "Notes in" defaults Off, ignores pad scale/root/layout, velocity ignored by default.
8. Latency constant likely about 5.8 ms larger (codec DAP in the path). Ticket 13 still needed.
