# Round 7 brief for the design tool: loop mechanics

Paste everything below the line. This follows round 6 (your export `SCREENS.md` and `STYLEGUIDE.md` is what we build from). The owner has settled how loops should work; these decisions change screens you already drew and need a few new ones. Same limits and the same style guide as round 6. The goal the owner gave: **perfect loops, insanely easy to record.** Pain they named: "awkward recording and getting the timing right".

---

## What changed (decisions, all settled)
1. **First take: set tempo and bars first, then one Rec press.** A count-in, then it records and **stops itself exactly at the bar line**, then plays. There is no "How many bars?" question after a take any more when a tempo is set (4.3 only appears in Free mode, where you stop by hand). **New projects start at Set 80 BPM, 4 bars, a 1-bar count-in.** Free stays one button away.
2. **Tail wrap (Config option, on by default):** a note's tail past the loop end is folded onto the start of the loop (about 2 s), so pads and reverb tails flow round the seam.
3. **Live playing is quantised (Config: grid Off, 1/8, 1/16, 1/32, strength 0 to 100%; default 1/16 at 100%).** Pad Parts are stored as recorded notes; after you stop, they are rendered to audio, so there is a short **Rendering** step after each pad take. Changing quantise, patch or key re-renders.
4. **Later Parts: Layer (overdub) with Fade.** Recording keeps looping; every pass adds to the Part, and the older layers fade by a **Fade** amount (100% = plain overdub, down to about 40%) only on the passes you record over. A way to switch to **Replace** (each pass replaces the last) for redoing a take. Undo covers the last pass.
5. **A countdown before later Parts start (on screen only, no extra clicks):** once armed, the last 4 beats before the loop comes round are shown as a counter.
6. **Changing tempo or bars after the first take** is allowed while every Part is a pad Part; once an audio Part exists the tempo is locked, with a message saying why.
7. **Calibrate input latency** (Config, Four-Track): a test with a cable from output to line input measures how late line and mic recordings arrive, then moves such Parts earlier by that amount. Pad and synth Parts are not affected. (The firmware for this exists; it needs your screens. Today's firmware screens are plain: see `docs/images` or run the emulator.)
8. **Fade quick control:** a Config default, plus Shift + Jog while a take is armed or recording, with the value shown on screen.
9. **Rec on an empty track arms straight away** with the last role and sound (a new project starts with Track 1 as Chords, Warm Pad). The role chooser (6.5a) opens only when you press the track button twice.
10. **Home controls follow your 4.1:** jog scrubs, Left and Right step a beat, Up and Down choose the track. Our levels: Shift + Up and Down is the selected track's level, Shift + Left and Right the synth level. Output volume moves to Config and Params. Undo take moves to Shift + Delete. Settings joins Project.

## Frames we need
For each, a frame per state, what the eight buttons, jog, arrows and Shift do, colour roles and font slots, and an element count (budget under about 7 KB).
- **4.2 Starting tempo, changed.** Tempo, **Bars** (1, 2, 4, 8, 16; only lengths that fit 16 s at that tempo, and the loop length in seconds shown), Count-in. Free and Set. Default 80 BPM, 4 bars.
- **4.1 first-take frames, changed:** count-in with a visible bar and beat counter; recording with the end of the loop marked (it will stop by itself); the moment it stops and plays.
- **Later-Part countdown** (armed on a later track, last 4 beats): where the counter goes, and how it differs from the existing blinking circle.
- **Layer and Fade while recording:** how you see the pass number, the Fade value (and Shift + Jog changing it), and the older layers fading in the lane (suggestion: older passes drawn dimmer). A **Layer / Replace** control: where does it live (a button on the armed state, or a Config setting only)?
- **Rendering**: the step after a pad take (a short progress state; it should feel like part of stopping, not a wait).
- **Config, Four-Track group, extended:** quantise grid and strength, tail wrap, Fade default, Layer or Replace default, Input latency (a value in ms with a Calibrate button).
- **Calibrate input latency** (4 states): ready (cable, volume, Start; dim when the project has Parts), measuring (progress), result (ms, frames, Keep or Retry), failed (no cable heard, clicks do not agree).
- **Tempo locked** message (once an audio Part exists): where it appears when you try to change tempo or bars.
- Anything else you think "perfect loops" needs on screen (for example showing the loop's bar and beat while scrubbing).

## What to keep from round 6
All of 4.x, 6.x and C1 to C9 as drawn, except where listed above. The Play view and the role views stay.

## Questions back to us
If a decision above makes a frame awkward, say so and offer an alternative with its cost.
