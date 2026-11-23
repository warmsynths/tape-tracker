# Round 4 brief for the design tool

Paste everything below the line.

---

Round 4. We checked your screens against the real firmware and made product decisions. Please do three things: (A) apply the corrected hardware limits, (B) update the main screen, (C) design the new screens. For every screen, tell me what each of the eight buttons, the jog wheel and the arrows do, as before. Colour, type and style stay your call.

## A. Corrected hardware limits (these replace the earlier ones where they differ)
- **Eight buttons** under the screen, confirmed: eight cells of 97 px on a 100 px pitch. A cell fits about 9 to 10 characters at size 14 or 17.
- **Fonts:** still five slots, but the default set has no bold at size 14. Slots are 14 light, 17 light, 14 regular, 17 bold and 12 regular. Use 17 bold for emphasis. The owner may supply a custom font file later; the five slots stay fixed.
- **Icons:** the only built-in glyphs are Pause, Play, Loop and Perf, each two cells wide. **There is no Stop or Rec glyph.** Draw Stop as a filled square and Rec as a filled circle, or use text. Everything else is text or simple shapes.
- **Waveforms are coarse.** The whole screen has a small drawing budget. Plan about **100 to 150 columns per lane**, not 600, and keep the number of separate elements low. If the budget overflows, the top bar and button labels are what disappears, so we protect them by keeping the rest simple.
- **Colour:** keep flat colours and named roles so it can be re-themed and still reads in the Monochrome theme.
- **Motion:** unchanged. Playhead, level meters, a pulsing record indicator, a moving highlight and changing numbers are fine. No fades, slides, scrolling or zoom. The screen redraws roughly every 20 to 35 ms at best, and none of this is measured on the device yet.

## B. Changes to the main screen
The main screen is now the home screen, opened by the Pattern button.
1. **Record source per track.** Each track shows where its recording came from: line in, mic, pads (playing a loaded sample), and later synth. It is set when the track is armed and stays after recording. Design a small label or mark for it; text of 3 to 4 letters is fine.
2. **Glitch indicator.** A track with glitch playback switched on shows a small mark, so you can see which tracks are being changed live.
3. **Pad mode indicator.** Pads can work two ways per sample: chromatic (one sample across the pads at different pitches) or one sample per pad (like drums). Show which mode the current pads are in.

## C. New screens (describe by what they do)
1. **Part Editor** (Sample Edit button). Edits the selected track's recording: trim start and end, fade in, fade out, normalize, and Offset (time or bars). The jog wheel changes the field in focus. A coarse waveform shows the trims. Show a way to undo the last change.
2. **Glitch and live effects** (Performance button). Four columns, one per track. Each column has a few effect slots you switch on while the loop plays:
   - Playhead jumps: jump back, stutter (repeat a small slice), reverse.
   - Pitch step: move to a musical interval (fifth, seventh, octave and others), with a choice of tape-style (speed changes with pitch) or granular-style (length stays).
   - Plus stock effects such as bit-crush, overdrive, pan and filter.
   Show which slots are active on each track, and the amount. Everything stays in time with the loop.
3. **Lo-fi master** (Master button). Controls for glue compression, saturation, wow and flutter, and bit-crush, with a way to see how much each is doing.
4. **Pad and sample settings.** Choose a loaded sample for the pads, and choose chromatic or one-sample-per-pad. Show memory used and how many samples are loaded, because memory is limited.
5. **Memory.** One simple view of how much recording time is left and how many samples are loaded.

## Not yet: chords and synth
A chord screen and a synth screen come later. Leave the Song button unused and do not design them now.

## What I want from you
- One 800 x 480 frame per screen, with a note on colour roles and font slots used.
- For the main screen, say what you changed and why.
- A list of anything you dropped because of the limits.
