# Keep or drop: stock screens and features (owner decisions)

Decided with the owner. Only the Original Tracker matters. Sizes are program-space estimates from the v1.0.0 build, not device measurements.

## Screens
| Screen | Decision |
|---|---|
| Project | Keep, rework for four-track projects |
| Sample Loader | Keep |
| Sample Recorder | Keep (records a sound into a pad slot) |
| Sample Editor | Keep, grows into the Part Editor (edits the selected track) |
| Sample Playback | Keep |
| Instrument Editor | Keep, simpler layout (play modes, Slice, BeatSlice, Granular) |
| Master | Keep (lo-fi chain and glue compression live here) |
| Performance | Keep and rework: glitch and live effects per track |
| Config | Keep (firmware update lives here) |
| Pattern | Removed from the controls; code kept for now |
| Song | Button left free; later the chord screen |
| Image Viewer | Hidden |
| Wavetable (play mode) | Dropped |

## Button map
Pattern: Four-Track main screen. Sample Edit: edits the selected track. Performance: glitch and live effects. Master: lo-fi and glue. Sample Play, Sample Rec, Sample Load, Params, File, Config: as stock. Song: free, pressing it does nothing safe until the chord screen exists.

## Later features
- Chords: a small set of pleasing chord sets played with a chosen sample, plus suggest-next-chord. Recorded onto a track as audio. 3 to 4 voices while playing.
- Glitch playback per track: playhead jumps (back, stutter, reverse), plus musical pitch steps (fifth, octave) per half-beat slice, in time. Tape-style and granular-style pitch both offered.
- Record-source label per track (line, mic, pads, later synth).
- Synth: first, looped single-cycle waves through the stock voice (filter, envelope). Then a small custom voice. Then one or two Plaits models (virtual analogue, chords), MIT licensed, after measuring CPU on the device. Check licence of sub-libraries first.

## Decisions, 2026-11-26 (owner)
- Granular play mode: drop (never used on the stock firmware). No live granular effect is wanted either; granular was only an example.
- Instrument editor: not wanted in full. Candidates to cut: envelope and LFO editing, MIDI and instrument-synth modes, the stock synth editor, macros and patch editor. Keep a slim pad-sample page (tune, volume, pan, play mode, start and end) if the owner confirms.
- Goal: carve out as much flash as possible for a Plaits-style synth and the chord engine. Free flash is about 85 KB of 1 MB (FT30). Candidate savings are listed in the conversation of that day: pattern editor UI (~58 KB), stock Performance (~19 KB), Song editor (~11 KB), themes (~24 KB), random names (~15 KB), MOD/IT (~12 KB), test code (~9 KB), fonts (~35 KB from keeping one set). Nothing is removed until the owner confirms each item and the real savings are measured.
- 2026-11-26: done in FT43. All stock screens that the Four-Track buttons no longer reach were removed (see docs/first-test-build.md). Free flash is now about 519 KB.
