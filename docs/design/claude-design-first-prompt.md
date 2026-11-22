# First prompt for Claude Design

Paste everything below the line.

---

Design the screens for a four-track recorder that runs as firmware on an Original Polyend Tracker (a hardware groove box). Think of a classic cassette Portastudio, but digital and immediate. It is for one person recording guitar, finger-drummed samples and bass, one part at a time, with lo-fi character. The screen is a small fixed LCD with no touch, so every design has to be buildable with the limits below. These limits are hard; please do not design around them.

## Hard constraints
- Canvas: exactly 800 x 480 px, one screen per frame. No touch, mouse or hover. Everything is operated by buttons, one jog wheel and pads.
- Bottom row: eight equal columns, 100 px wide each, one label per physical button below the screen. Keep this row for contextual function labels.
- Fonts: only these five: 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular. Flat, 4-bit grey antialiasing. Keep 12 for secondary text only.
- Icons: only Pause, Stop, Play, Rec, Folder, Up-dir, Loop and Arrow exist as glyphs. Anything else must be plain text or simple rectangles and lines.
- Color: flat opaque colors, no alpha, gradients, shadows, blur or rounded corners. Name each color by role, not by hex, so it can be re-themed: background, top bar, label active, label inactive, button, red (record), list, bar active, bar inactive, waveform, waveform progress, marker, and accents (white, red, orange, yellow, green, cyan, grey). It must still read in a one-color Monochrome theme.
- Drawing vocabulary: text, filled and outlined rectangles, level meters, a thin playhead line, point markers, and a waveform shown as a min/max envelope no wider than 600 columns. No images or bitmaps.
- Motion: only the playhead line and level meters move. No scrolling or zooming, no fades, no animated transitions.
- Four tracks are always visible on the main screen. No vertical scrolling.

## Vocabulary (use these words on screen)
- Track: one of four lanes. Part: one recording on a track. Loop: the repeating region; the first Part recorded sets it. Loop Mode: repeat or play once. Offset: how far a Part is shifted from the loop start, in time or bars.

## Screens for this first round
1. Main screen, stopped, with a Part on every track (guitar, drums, bass, one empty). Show: a top bar with project name, tempo, loop length in bars and a position readout; four lanes, each with a track number, name, mute state, level meter and a waveform; the loop region; the playhead; a master meter; and a monitor indicator.
2. The same screen while recording the second Part: the target track is armed and waiting for the loop start, then recording. Show the waiting state and the recording state as two frames, with a clear red indicator.
3. A Part selected for editing its Offset: show the current Offset in milliseconds and in bars, and which fields the jog wheel will change.
4. The prompt right after the first Part is recorded: "How many bars?" with the choices and the tempo that results, as an overlay on the main screen.

## What I want from you
- One 800 x 480 frame per screen, plus a short note on every color role used and every font slot used.
- Two or three layout options for screen 1, each in a sentence on what it trades off, then a recommendation.
- A list of anything you were tempted to do but dropped because of the constraints.
- Style: calm, dark, a little worn like tape gear, achieved with color and type only, not effects.
