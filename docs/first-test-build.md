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

## Test build FT11 (replaced by FT12)
FT11 forced the left input into both ears and recorded the left input only. That was based on the guess that a mono instrument was in the left jack. It was wrong for a stereo instrument, so FT12 undoes it.

## Test build FT12: the input follows your Config setting
Your instrument (a Minichord through TRS cables) is stereo, so this build follows the Config setting **Line input** instead of forcing the left channel:
- **Stereo:** the live monitor is true stereo, and a recording is both channels added together into the mono track (this is how the stock Sample Recorder works).
- **Mono Left or Mono Right:** that channel is used for both the monitor and the recording.

Please check, with Config set to Stereo and then Mono Left: does the monitor sound as you expect, does the recording contain both sides of the Minichord (Stereo) or only the left (Mono Left), and is playback the same in both ears? If the left is still louder in Stereo, please also try the stock Sample Recorder screen with the Minichord. If it sounds the same there, the imbalance is in the signal or the cable, not in this firmware.

## Test build FT13: shows the left and right input level
Same as FT12, plus a small readout at the top right of the Four-Track screen: `L -20  R -26 dB`. It is the level arriving at the input, measured separately for the left and right jack, before anything else in the Tracker touches it.

Use it to settle the left-louder question: play the Minichord with Config set to Stereo and read the two numbers. If R is several dB lower than L, the imbalance is in the signal or the cable. If they match but you still hear the left louder, the cause is after the input (the headphone output or the headphones).

## Test build FT14: "How many bars?"
Includes FT13 (the left and right input readout).

New: after you stop the first take (or it stops by itself at 24 s), the Tracker asks **How many bars?** instead of starting the loop straight away.
- The choices are the bar counts that give a tempo between 40 and 240 BPM, each with the tempo it would give. The one nearest 120 BPM is highlighted to start.
- **Jog wheel** or **Left/Right arrows** choose. The button under the screen labelled **OK** (the 8th) sets the loop and the tempo and starts it playing. **Cancel** (the 7th) throws the take away.
- After OK the top bar shows the tempo and the number of bars, the loop line is split into bars with numbers, and the position reads as bar.beat.sixteenth.
- No audio is stretched. Only the tempo is calculated from the length you played.

Please check: does the question appear after the first take, can you choose with the jog wheel and the arrows, do OK and Cancel do what they say, and does the position readout count sensibly against what you play? Try a very short take (about 2 seconds) and a very long one (stop it by letting it fill) and tell me what choices you are offered.

## Test build FT16: the whole design, one build
`firmware/PolyendTracker_FT27_full.ptf` (FT16 plus the fixes below; FT27 is the current one). Built from v1.0.0 with the shared core (`core/ft`), the same code the web emulator runs. Test the emulator first; this build should behave the same, apart from audio hardware timing. It has not been run on a device yet.

What is in it (see `docs/build-plan.md` for the plan and `docs/design/screen-requirements.md` for each screen):
- **Main screen:** starting tempo and count-in, click, source per track (line, mic low, mic high, pads), alignment of each Part to the loop (first-note detection and snap, window in Settings), offset, mute, Loop Mode, undo of a replaced take, How many bars.
- **Pages:** Part Editor, Glitch, Master lo-fi, Input setup, Project (new, open, save, save as, rename), Settings, Export WAV (1 to 16 repeats), Pads, Memory, and the popups.
- **Buttons:** Pattern stays the main screen. Sample Edit opens the Part Editor, Performance opens Glitch, Master opens the lo-fi master, File opens Project. Shift+B1 Loop Mode, Shift+B2 Pads, Shift+B3 Memory, Shift+B4 Input setup, Shift+B5 to B8 mute, Shift+Up undo take, Shift+Down Settings. **Insert** does what pressing the jog wheel does in the emulator (the Original's wheel does not push).
- **Files:** everything is written to a `FourTrack` folder on the SD card (`Projects`, `Exports`, `undo`).
- **Memory:** the application lives in the sample memory next to the four tracks, so the memory for samples is about 128 KB smaller than FT15.

Not verified on a device (only in the emulator and by PC tests): CPU load with glitch and lo-fi running on four tracks, speed of saving and exporting to the SD card, the feedback-protection readout (always off on the Tracker for now), and pan (the loop player is now stereo).

Please check first: record a loop, record a second track, open Glitch and hold B1 to B4, open Master, save a project from File, export a WAV. If the audio crackles with Glitch or Master on, tell me which one.

FT17 changes after the first emulator test: the count-in also happens when the tempo is Free (four clicks at 120 BPM, the tempo stays Free); Play and Stop work on the Glitch, Master, Part Editor, Input, Settings, Pads and Memory pages; on Glitch, B5 to B8 pick the track (as on the main screen), Shift+B1 clears the track, Shift+B2 clears all, Shift+B3 switches the pitch style, and the Performance button goes back.

FT18: the Glitch page now does what the demo clips did. Two new effects for the four slots: **Random jumps** (each half-beat slice has a chance, set by the jog, to jump back one or two slices, stutter or play backwards) and **Pitch steps** (each slice has a chance to step to a musical interval; Shift+Left/Right picks the set: Mixed, Fifths, Octaves, Thirds, Sevenths; the track's pitch style tape or granular still applies). Both differ on every pass of the loop and never change the loop length. Slots 1 and 2 start as these two. The older fixed effects (Stutter, Reverse, Jump back and the stock ones) are still in the list.

FT19: every Glitch slot has a **chance** (jog, 0 to 100%). **Insert** (the jog press in the emulator) latches the slot on: it then happens on that share of the slices as the track plays, and 100% means always, until you latch it off. **Holding B1 to B4** triggers the slot at once, whatever the chance, until you let go. Shift+jog still picks the effect; Shift+Up/Down changes the effect's own setting (interval, set of intervals, how hard). Project files saved by FT16 to FT18 will not open.

FT20: editing.
- **Move a Part on the main screen:** select the track, hold Shift and press Insert (Enter in the emulator). The jog then moves the Part so its first note steps along the beat grid. Buttons: To start (first note to the loop start), To beat (to the nearest grid line), grid (cycles Off, Bar, 1/4, 1/8, 1/16), All (moves all tracks together), Cancel, OK. Shift+Left/Right moves by 1 ms.
- **Part Editor:** the jog steps start and end by the grid (Shift: 1 ms) and cut points land on a zero crossing when Snap 0 is on (B5; Shift+B5 cycles the grid). B6 Trim silence cuts the silence off both ends. Shift+Up/Down zooms (whole loop, 1/4, 1/16, 1/64) around the field being edited, the grid ticks show under the waveform, and the Level field now works from the jog (0.5 dB, Shift 0.1 dB).
- **Settings:** Grid and Snap cuts to zero are project settings. They are saved in the project.
- Beat lines are not drawn through the lanes on the main screen.

FT21: fixes buttons B1 to B4 doing nothing in FT16 to FT20. The Tracker's button handler keeps one function per button, so registering the release (for the Glitch hold) replaced the press. B1 to B4 and Shift now use one handler that gets both press and release. Shift did not work on the device before this either.

FT22: fixes the blank screen the first time the Four-Track screen opens after power-up. The screen's drawing parts were created before the application existed, so they had nothing to draw until the screen was opened a second time.

FT23: easier to find what you can do to a track. With a recorded track selected, the hint line says what Insert and Shift+Insert do, and the selected track's waveform has a frame around it. **Insert** (the jog press in the emulator) opens an action menu on the buttons: Move, Edit, Glitch, Mute, Source, Undo take, Close. The shortcuts (Shift+Insert move, Sample Edit, Performance, Shift+B5 to B8 mute, Shift+Up undo take) still work.

FT24: the screens now follow the design frame by frame. Every position, size and colour was read from the design file and compared with the screen: main screen (top bar with dividers, lane headers, 120-column waveforms, level bar on the right, playhead, Rec/Stop/Play drawn shapes, button row), Starting tempo and How many bars? as panels over the main screen, Part Editor, Glitch, Lo-fi master, Input setup, Project list and Rename, Settings, Export, Pads, Memory and the popups.
Changes you will notice:
- **B4 is Monitor** (as in the design). The pad mode (Keys or Pad) is chosen on the Pads page (Shift+B2), B1 and B2.
- The top bar no longer shows the L/R input level readout (it is on Input setup).
- The selected Part no longer has a frame around its waveform (the design marks the selected track by its header only).
- Pads page: shows the sample on the pads; Up/Down chooses the sample.
- Part Editor: the Level field is now the design's Normalize field (B2 applies it). The extras kept from earlier requests: B5 Snap 0, B6 Trim silence, and a small grid/snap/zoom note beside the track name.
- Glitch: B5 Clear track, B6 Clear all, B7 pitch style, B8 Back as in the design; Left/Right pick the track.
- Settings: Grid and Snap cuts to zero are added to the project block; the rows are a little closer together to fit.
- The Project list shows bars and tempo of each saved project; the date column is not shown because the Tracker has no clock set.
Where the design uses a different font, letters differ in width; positions are the design's.

FT25: Glitch keeps the design's look, with the track buttons back: B5 to B8 pick Track 1 to 4, Shift+B1 clears the track, Shift+B2 clears all, Shift+B3 switches the pitch style (tape or grain). Left/Right still pick the track too.

FT26:
- **Song button = metronome.** It switches the click on or off. A CLICK tag shows in the top bar while it is on. The click now also plays during the first take (at the starting tempo, or 120 BPM when the tempo is Free), not only once a loop exists. The same setting is in Settings (Click).
- **B4 Monitor works for every input.** Line and the mics each remember their own monitoring (Line starts on, mics start off). The top bar shows MON or MON OFF for the selected track's input.
- **Emulator matches the device's buttons:** Insert, Song, Shift, Sample Edit, Performance, Master, File, Rec, Play, arrows and the jog wheel (which turns but does not push). The extra Stop and jog-press buttons and the monitor checkbox are gone.

FT27: the Pattern button is home: from any page or menu it goes back to the main screen. The emulator's front panel is now laid out like the Original: the screen with the jog wheel beside it, the eight buttons under the screen, the 48 pads, and the 5 x 5 button block (Params, Performance, File, Config, Master / Sample Load, Sample Play, Sample Edit, Sample Rec, Song / Note, Instr, Vol, Fx, Pattern / Play, Copy, Insert, Up, Delete / Rec, Shift, Left, Down, Right). Buttons that belong to stock screens or that the Four-Track screen does not use are shown dimmed and do nothing in the emulator.

Emulator (no firmware change): the front panel is the chassis from the earlier simulator that looked right: screen at its real size with the eight buttons directly under it, 48 pads below, a big jog wheel with the 3 x 5 mode buttons (coloured tops on Note, Instr, FX1, FX2) and the 2 x 5 transport block on the right, and a status strip underneath. The whole device scales to fit the window. Sound, input, colours, files and pad samples are in a Settings drawer that opens and closes from the top right.
