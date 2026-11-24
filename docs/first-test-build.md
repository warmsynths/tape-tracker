# First test build: record one Part and loop it

**What this is.** A first firmware for your Original Tracker with one new screen. You plug your guitar into the line input, press Record, play, press Stop, and press Play. The Tracker plays what you recorded, over and over, as a loop.

**What was checked, and what was not.**
- It compiles for the Original, and the loop logic passes its tests on a computer.
- It has not run on a real device yet. You will be the first to try it. Expect problems, and please report exactly what you see.

**What it replaces.** In this build the **Song** button opens the new screen instead of Song mode. Everything else on the Tracker is unchanged.

## Before you start
1. The recovery drill is done (see ticket 07). You need the official firmware file on your computer, and to have tried the paperclip reset once.
2. Know how to put the official file back: delete this test file from the card's `Firmware` folder, copy the official `.ptf` in, then press the paperclip into the reset hole.

## Put it on the Tracker
1. Copy the test `.ptf` file into the `Firmware` folder on the SD card. Keep the official file off the card while testing.
2. On the Tracker go to **Config**, then **Firmware**, then **Firmware update**, choose the test file, and confirm.
3. Wait for it to restart. If the screen sits at 100 and does not restart, restart the Tracker by hand. This happened during the recovery drill and a manual restart worked.

## Try it
1. **Set the input level first.** Press **Sample Rec**, choose the line input, and set the gain with the jog wheel while you play. Aim for a strong level without hitting the top. This build uses that same gain setting.
2. Press **Song**. The screen shows **Four-Track** and **EMPTY**.
3. Press the first button under the screen (**Rec**), or the Rec button. The text changes to **RECORDING** with a growing time.
4. Play something for 5 to 10 seconds, then press the second button (**Stop**).
5. The text shows **READY** and the length. Press the third button (**Play**), or the Play button.
6. The text shows **LOOPING** and you hear your recording repeat. Press **Stop** to stop it.
7. Press **Rec** again to record over it. The old recording is thrown away.

Limits: one Part, up to 30 seconds. If memory fills it stops by itself and keeps what it recorded.

## Please do not use
The Sample Editor while testing this build. The recording is stored in the same memory area the editor uses.

## What to tell me
- Did it restart and start normally after flashing? If not, what did the screen show?
- Does the Song button show the Four-Track screen?
- Does recording start and the time count up?
- Does the loop play, and is there a click, gap or pop where it repeats?
- Is the recording quiet, distorted or noisy compared with the Sample Recorder?
- Any glitch or freeze when you press buttons while it loops?
- Anything on the screen that looks wrong.

## If something goes badly
Put the official `.ptf` on the card as the only file in `Firmware`, then press the paperclip into the reset hole on the underside.

## Test build FT6: pads and the memory guard
New in this build:
- **Pads play your samples on the Four-Track screen.** Press the fourth button under the screen (it says **Pads: Keys** or **Pads: Pad**) to switch between the two modes.
  - **Keys:** the last sample you used is spread across the pads at different pitches, like the stock Tracker.
  - **Pad:** pad N plays sample slot N at its own pitch, like drum pads.
- **Loaded samples can no longer run into the track areas.** The sample memory limit is lowered, which leaves about 2.6 MB for samples while the tracks are in use.
- **The Sample Editor will not open while a track holds a recording.** It shows "Tracks hold recordings". Clear the tracks (or restart) to use it.

To try it:
1. Load a few short samples with **Sample Load** (the stock screen). Put a drum hit in slot 1, for example.
2. Press **Song** to open Four-Track.
3. Hit the pads. Switch between Keys and Pad with the fourth button and say what you hear.
4. Record a track, then play the pads over it.

Tell me: do the pads sound in both modes, does the mode switch work without a hang, and does anything glitch while a loop plays and you play pads?

## Test build FT7: a source for each track, and recording the pads
Includes everything in FT6 (pads, Keys and Pad modes, memory guard).

New:
- **Each track has its own source.** The track lines show it in brackets, for example `> Track 1  [LINE]  EMPTY`. Press the button of the track that is **already selected** to go to its next source: LINE, MIC LOW, MIC HIGH, PADS, then LINE again. Pressing a different track's button just selects it.
- **PADS records what you play on the pads** (the samples you loaded), not the audio input. Select a track, press it again until it says PADS, press Rec, play the pads, press Stop (first take) or let it run one loop (later tracks).
- Mic sources use the same gain as the Sample Recorder screen. Mic monitoring is off.
- Headphones go quiet for a fraction of a second when you switch to a mic. That is on purpose, to avoid a pop.

Please try:
1. Record a guitar on track 1 with LINE.
2. Select track 2, press it again until it says PADS, load a drum sample, record a drum pattern from the pads.
3. Check the pads recording lines up with the loop, and say whether it sounds clean or has clicks, gaps or delay.

Known limits: the level meter line only shows the audio input, so it does not move for PADS. Sources are not saved with the project yet.

## Test build FT8: a level meter on each track
Includes everything in FT7.

New: each track line now shows a ten-step level bar, for example `> Track 1 [LINE] ||||||....  PART`.
- A track that is playing shows the level of its own recording.
- The track being recorded shows the level of what is being recorded, whatever the source (line, mic or pads). So the bar now moves for PADS too.
- The bar jumps up at once and falls back slowly. It covers about -50 dB to 0 dB.
The separate input level line at the bottom is still there; it only shows the audio input.

Please check: does each bar match what you hear, and is the track line still fully readable on the screen (it is longer now, so tell me if it is cut off)?

## Test build FT9: the designed main screen (first version)
Includes everything in FT8. The Four-Track screen now follows the design: a top bar, four lanes with a name, source tag, level bar and a coarse waveform of each Part, a loop line with a moving playhead, a hint line and a row of eight labelled buttons.

Not done yet on this screen (shown as placeholders): tempo and bars (shows Free and -), the project name (shows Four-Track), the mute box and glitch mark (not wired), the Monitor button (button 4 is still the Keys/Pad switch), and the "How many bars?" step.

Please check: is everything on the screen readable and where you expect it, does the playhead move smoothly without slowing the buttons, do the waveforms look right after you record, and is any text cut off or overlapping? If part of the screen is missing, say which part.

## Test build FT10: fixes for FT9
From your photos of FT9:
- **The screen broke up once a track had a recording.** The main screen was drawn in one go, which is more than the display chip can take at once (about 4 KB). It is now drawn in small steps, and the waveform is 100 columns instead of 120 to stay inside the screen's total limit.
- **Text was spaced too widely and overlapped.** Letters now advance by their real width.
- **The jog wheel and the Up and Down arrows now move between tracks.** The design gives the jog wheel scrubbing later; for now it selects tracks. Pressing the same track's button again still changes its source.

Please check: does the screen stay intact when a track has a recording, while arming a second track, and while recording it? Does track 1 still play together with track 2 after the second recording (in FT9 you heard track 2 only)? If track 1 goes silent again, tell me exactly what you pressed in what order.
