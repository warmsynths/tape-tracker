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
