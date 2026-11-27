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
`firmware/PolyendTracker_FT31_full.ptf` (FT16 plus the fixes below; FT31 is the current one). Built from v1.0.0 with the shared core (`core/ft`), the same code the web emulator runs. Test the emulator first; this build should behave the same, apart from audio hardware timing. It has not been run on a device yet.

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

FT28: the lane source tag is back on every track (it had been hidden on empty tracks in FT24 to copy the design). New rule: the web emulator and the firmware are always built together by `tools/build-all.sh <number>` from the same sources, and both carry the same build id (shown in the emulator's top line and at the top right of the Settings page on the device). A test (`tests/check_build_id.py`, also run on every push) fails if the shared sources change without a rebuild of both.

FT29 (built, being tested in the emulator first): 
- **Count-in length:** Off, 1 beat, 2 beats, 4 beats (default), 8 beats. Set in the Starting tempo panel (B2 on an empty home screen, Left/Right to the Count-in box, jog) and in Settings (Count-in). With the tempo Free the count-in plays at 120 BPM.
- **Wait for sound:** the first take can wait for the input to get loud. Switch it on in the Starting tempo panel (the Wait for sound box, Insert) or in Settings; the level (default -36 dB) is set there or with Shift+jog on Input setup, where a marker shows it on the input meter. Rec arms the take (status Armed, "Waiting for sound" with the live level); recording begins at the first sound, keeping the pre-roll before it. Stop cancels quietly. A WAIT tag shows in the top bar. It does not apply to the Pads source.

FT30 (emulator first): the Play button now stops the loop when it is already playing, on the main screen and on every page (before, on a page it could only start). Button 3 under the screen still only starts.

Emulator only: Params, Config, Sample Load, Sample Play and Sample Rec now show a picture of the stock screen over the emulator's screen (click or press any button to close). Pictures go in `prototype/emulator/stock/` (see the README there); on the Tracker, hold Shift and press Power on a screen to save a picture to `Snapshots/` on the SD card. Until a picture is added a placeholder card is shown.

FT31 (carve step 1, NOT yet sent or tested on a device): frees about 79 KB of flash (883 KB used of 1 MB, was 963 KB). No change to the Four-Track screens.
- The Roboto Mono font set is removed; the Liberation set is used whatever the Config font setting says (that setting now changes nothing). The emulator's font choice is gone too.
- The Four-Track screens and pages are compiled for size (-Os); the audio maths stays on speed (-O2). Please watch for any slowdown in screen updates.
- The hidden factory test combinations (button and pad combinations in Config) and their code are removed.
- Random names (the Random button on the name keyboard) now come from two short lists instead of two lists of about a thousand words each.
To check on the device: the Four-Track screens look the same as FT30, text is the same font, and Config, Sample Load, Sample Rec and Sample Play still open and work.

Emulator: the five stock-screen buttons now show the real pictures (Params, Config, Sample Load, Sample Play, Sample Rec) supplied by the owner. The same pictures are in docs/design/stock/ for the design tool.

## FT32 (not sent)
Round 5 so far: 16 s tracks, track mixer on Params (level, pan, tone, mute, solo, reset), pad sample settings on Sample Play (tune, volume, pan, play mode, start, end, audition, Keys or Pad). Emulator only for now; firmware built but not sent.

## FT33 (not sent)
Round 5 C2: Sample Load opens Load samples. Browse SD folders and WAV files, preview, choose the slot, Load or Load next, replace asks first, empty folder and no card states. Emulator: a pretend card with a Samples folder (Drums, Keys) and a way to add your own WAV files in the settings drawer; sample memory now 6 MB. Firmware: uses the stock importer and previewer for the loading and preview (waits for the importer to finish); not tried on a device yet, and a risk to check first.

## FT34 (not sent)
Round 5 C4: Sample Rec opens Resample. It renders the finished mix (tracks, glitch settings as saved, lo-fi master) for 1 to 4 loops into a WAV at `/Samples/Resamples/Resample NN.wav` on the card and loads it into a pad slot with the same loader as Load samples. Differences from the design: it is rendered, not captured live, so it is the same as an Export (held glitch effects and live moves are not in it) and it runs faster than real time; B1 while it runs cancels and keeps nothing; memory is mono 16-bit, so about 0.7 MB for an 8 s loop, not 1.5 MB. Firmware: untested on a device.

## FT35 (not sent)
Round 5 C5: the Config button opens Device settings instead of the stock Config screen. Groups: MIDI (MIDI in, In channel, MIDI out), USB audio, Line input, Volume, Display (brightness), Four-Track (alignment window, pre-roll, tempo rounding, wait level), Firmware (installed version, update file, B1 Update), About (version and build id). Left and Right move between the group list and its rows; jog changes a row. They read and write the same values the stock Config used. B1 Update saves the project first, shows the update screen, then hands over to the stock bootloader update (the bootloader area is not touched). In the emulator the update is a pretend one that counts up and returns home. Not on the screens, because the stock config has no setting for them: MIDI "Out channel", and a separate headphone and master volume (there is one volume). The stock Config code is still in the firmware, just not reachable; removing it is a later step. The Four-Track settings page (File button, settings) still shows the same four device rows too. Untested on a device: the MIDI, USB audio, line input, volume, brightness and update calls (they call the stock functions).

## FT36 (not sent)
Fix: a page button (Params, Sample Load, Sample Play, Sample Rec, Config, Sample Edit, Performance, Master, File) now opens its page straight from another page. Before, it worked only from the home screen, so you had to press Pattern first.

## FT37 (not sent)
Home screen redesign (round 5 design, 4.1), with waveform columns (plan A):
- Each track has its own colour on its number, header edge, level meter and waveform; the header and lane get a faint tint (10%, 22% on the selected header). Uses a see-through fill (`Gfx::alpha`, the chip's colour alpha): never tried on this firmware before, so check the tints and the dark "ahead of the playhead" shading first.
- The part of each lane the playhead has not reached is darker (one see-through rectangle per lane, in the live layer); a muted lane is darker still.
- Two layouts: overview (four 84 px lanes, 60 columns) and focus (the armed, recording or chosen track at 250 px with 120 columns, the others as 28 px strips with 30 columns). Focus opens on arming or recording, and when you press the selected track button again; Stop or the button again closes it. Pressing the selected track no longer changes its source (use Shift+B4 Input setup).
- Grid lines over the lanes: bars, beats and optionally sixteenths, set in Device settings, Display, Grid. Default Bars and beats.
- The armed lane shows the live input meters and the Wait level mark; the take being recorded grows as red columns.
- Home with four Parts is about 7.0 KB by the cost model, under the 8,172 byte limit (a test keeps it under 7.5 KB).

## FT38 (not sent)
Waveform bitmaps (plan D) behind a switch: Config, Display, Waveforms: Bitmaps or Columns. On the Tracker it starts on Columns (safe); the emulator starts on Bitmaps.
- Each Part's waveform is drawn once into three one-bit pictures per track (84, 250 and 28 px high, 600 px wide, 27,152 bytes per track, 108,608 for four) and written to the display chip's graphics memory from address 200,000 (the stock images end near 128,800; the picture viewer starts at 350,000). The home screen draws one bitmap per lane in the track colour. Home with four Parts is about 5.2 KB (overview) and 4.7 KB (focus) with bitmaps, against about 7.0 KB with columns.
- To try on the device: switch Waveforms to Bitmaps, record a Part, look at the home screen. What to watch for: no waveform or garbage (bitmap setup wrong: the 600 px width needs the extended size command), a pause when a take ends (about 27 KB goes over the slow link per Part, estimated 15 ms), flicker, or the picture viewer or other screens drawing wrong afterwards (graphics memory overlap). If anything is wrong, switch back to Columns; nothing else depends on it.

## FT39 (not sent)
- Waveform bitmaps are now the default on the Tracker (they worked on the first device test); Config, Display, Waveforms still switches back to Columns.
- Copy button: copies the selected track's Part to another track (B1 to B4, Left/Right or jog choose, B8 Copy, B7 Cancel). A target that has a Part shows a REPLACES warning. The copy keeps trims, fades and Offset; level, pan, tone and effect slots stay with the target.
- Delete button: clears the selected track's Part (B8 Clear, red). Shift + Delete clears all tracks. The Loop and tempo stay, so the next take still lands on the grid.
- Shift + Copy undoes the last copy or clear (one step; the cleared audio is kept on the card in `FourTrack/undo/`). A note in the hint line says what happened until the next button.

## FT40 (not sent)
The Part Editor and the Pad sample page draw their big waveform as a bitmap too (Config, Display, Waveforms: Bitmaps, the default). One shared picture, 760 x 200 at one pixel per column (it was 150 columns), 19,000 bytes in the display chip's graphics memory at 308,608 to 327,608 (the picture viewer starts at 350,000). Only one of those pages is open at a time, so they share it. The editor redraws it when the zoom, trims or offset change; the pad page only when the sample changes. Each page's waveform goes from about 1.2 KB of display commands to about 50 bytes. Load samples keeps its 58 column preview (a finer one would need many slow reads from the card).

## FT41 (not sent)
Fix: Device settings, Firmware found no update files. The stock list call skips names that fill its length limit, and `PolyendTracker_FT38_full.ptf` is 28 characters, exactly the limit I passed. Now 48. The list also follows the stock updater's rule: the name starts with `PolyendTrac`, ends `.ptf` and is at least 15 characters long. The same limit hid sample files with long names in Load samples; names up to 47 characters now list.

## FT42 (not sent)
The take being recorded is a bitmap too (it was 120 red columns). It grows at one pixel per column in the lane's own 250 px picture; only the newest columns are written to the display chip each update (about 250 short writes of 1 to 2 bytes, an estimated 1.5 ms every 100 ms). The picture is cleared once when a take starts and replaced by the finished waveform when it ends. With Waveforms on Columns the old red columns are used.

## FT43 (not sent): the stock screens are gone
- **Boot:** the Four-Track screen opens straight away on load. (Before, the stock Pattern screen opened and you had to press Song.)
- **Removed from the firmware:** the stock Pattern editor, Song editor, Performance mode, Instrument editor, Sample editor, Sample recorder, Sample playback, Sample loader, Master screen, Project editor, Config editor (its hardware actions for USB audio, line input and brightness stay) and Image viewer. Their MIDI, sequencer and file-manager hooks were stubbed.
- **Flash:** 910,136 bytes before, 529,292 after (50.5% of 1 MB, about 519 KB free). Step by step with a fresh library each time: Song editor, Config editor, Image viewer and Master screen together -70.6 KB; Sample loader -36.4 KB; everything else together -273.8 KB (they depend on each other, so they went in one step). Internal RAM 172,656 to 157,152 bytes; the external RAM the stock screens used went from 15.59 MB to 15.01 MB.
- **MIDI:** incoming notes (on the channel set in Device settings) now play the pads: C2 (note 36) is pad 1, up to 48 pads. MIDI CC input does nothing for now. This is new and untested.
- **Not touched:** the bootloader area and the flash start address (the linker script and platform files are unchanged).
- **Test first:** it boots to Four-Track; every button still does its job; project save and load; Load samples and Resample; Device settings and the firmware update (the update still goes through the stock bootloader, which is untouched, so a bad build can be fixed by installing another file).

## FT44 (not sent): quieter defaults
- Every track starts at -6 dB (it was 0 dB), so four Parts have headroom; the mixer's Reset (B3) goes back to -6 dB. Saved projects keep their own levels.
- The default output volume is 60 (the stock default was 81). A config still on 81 is set to 60 once when the Four-Track screen starts; Device settings, Volume changes it and the change is kept.
- Shift + Left or Right on the home screen changes the output volume by 2; the hint line shows the value.

## FT45 (not sent): Clear is instant, Copy shows progress, Reset all
- Clear no longer writes anything to the card. The audio stays in the track's memory until something is recorded or copied there, so Shift + Copy still brings it back (that is lost once you record on that track, copy onto it, or open a project). It used to write the whole Part to the card first, which is why the last clear took so long with nothing on screen.
- Copying onto a track that has a Part still keeps the old Part on the card, but now a Copying panel with a progress bar shows while it writes, and buttons are ignored until it finishes.
- Shift + Delete, Clear all tracks?, has a new B6 Reset all: it clears every Part and also forgets the Loop, tempo, glitch settings, mixer levels and the lo-fi master, like a new project. B8 Clear keeps those, and the panel now says so.

## FT46 (not sent): the synth and the chord pads
New, and the first firmware with sound that comes from the Tracker rather than a sample.
- **Note button:** opens Chords and turns the pads into chord pads. The top two rows are chords (triads, then 7ths or 9ths), the bottom two rows are scale notes in the key. Fields (Left and Right choose, jog changes): Key, Scale (9), Style (Lo-fi, Pop, Jazz-ish, Synthwave, Soul), Rows (how many rows are chords, 0 to 4), Note pads (Scale, Chord: the notes of the last chord, Follow: the scale with the last chord's notes lit), Octave, Sound (8 synth presets, or the pad Sample pitched). B1 switches chord pads on or off (the top bar shows CHORD), B2 Shuffle re-rolls the five colour chords (columns 8 to 12) and B3 goes back to the style's own. The seven degree columns never move.
- **Synth:** six voices built on Plaits' oscillators (shape, saw, super-square), a second oscillator with detune, a sub, noise, a 24 dB low-pass, two envelopes, an LFO, drive and a stereo ensemble chorus. Presets: Warm Pad, Brass, Strings, Bass, Lead, Pluck, Organ, Dream Keys. It plays into the pad sound bus, so Source: Pads records it like any pad sound. It has no delay or reverb send yet.
- **Pad lights:** brightness steps mark the tonic, the colour chords and (in Follow) the chord's notes; a pad flashes when played and fades; the tonic pulses on every beat while something plays or records. The Tracker's pad lights have brightness only, no colour.
- **Flash:** 546,780 bytes (52.2%), up 17 KB. Internal RAM unchanged at 62.9%.
- **Not measured: the audio load.** Six voices plus four Parts, delay and reverb on a 180 MHz chip is the open question. If the audio breaks up (clicks, dropouts), tell me which preset and how many notes. I can cut it to four voices or turn the chorus off.
- **Not done yet:** no reverb or delay on the synth; no pad velocity.
- **Click moved:** Shift + Play switches the click (metronome) on or off; the CLICK tag still shows. The Song button does nothing now.
- Tests: the chord engine checks the notes of every chord against the theory tables (C major, A minor, 7ths, 9ths, sus), every key, scale and style stays in range; the synth checks pitch (220 Hz for A3), that every preset is quiet at the end of its release, and bounded output.
- **Instr button, the Sound page:** pick one of 12 patches (Warm Pad, Juno Pad, Strings, Brass, Dream Keys, Tine Keys, Organ, Pluck, Bass, Sub Bass, Lead, Square Lead), each with a kind and a one-line description. Eight tweak knobs sit on top of the patch (Bright, Reson, Attack, Release, Thick, Chorus, Drive, Motion, each -10 to +10; each moves several raw settings). Left and Right move between the list and the knobs, jog changes. Hold B1 to hear the patch (a chord, or a note for the mono patches); the pads play it too. B2 puts the knobs to 0, B3 puts the whole patch back. B7 Advanced lists all 28 raw settings (oscillator, filter, both envelopes, LFO, drive, chorus, glide, level); a changed setting is orange, B2 puts one back, B3 all. The knobs keep working on top of an edited patch.
- **Kept:** the chord pad settings, the patch with its edits and the knobs are saved to `FourTrack/sound.cfg` on the card a moment after the last change, and loaded at start. They are not part of a project.

## FT46 addendum: pad regions, chord types, performance pads
- **Layout:** the pads are now regions. The right two columns (8 pads) are performance pads. The other ten columns hold the chords (top rows) and the scale notes (the rest; 10 notes per row, so the scale runs on one row further up). Columns 1 to 7 of the chord rows are the scale's degrees, columns 8 to 10 are three colour chords (it was five).
- **Chord types per row:** each chord row has a type: Triad, 6th, 7th, Maj7, 9th, Add9, Add4, Sus2, Sus4, 6/9, 7sus4, Power. 7th and 9th follow the scale (ii gives m7, V gives a dominant 7; a flat 9th is left out). The others are literal over the degree's own triad (so Maj7 on ii is mMaj7) and light dimmer when they have notes outside the key. A style sets the first four rows (Triad, then 7th or 9th, then the other, then Sus4) and you can change any row.
- **Chords, page 2 (B7):** the type of each row, and each colour chord's root and kind (7, maj7, m7, dim). Shuffle re-rolls only the colour chords; changing the Style resets rows and colours. All of it is saved in `FourTrack/sound.cfg` (the file format changed, so an earlier one is ignored).
- **Performance pads (right hand, 2 columns):** Oct + and Oct - (held), Invert (root up an octave), Open (second note up an octave), Sus (replaces the third), 7th (adds the scale's seventh to a triad or add chord) are held while you play a chord; they combine, and Oct also moves the scale notes. Hold latches: the chord keeps sounding after you let go until you play the next chord or press Hold again. The last pad is spare. The pad lights show them dimly and bright while held or latched.

## FT46 addendum 2: play styles, humanise, velocity
- **The last performance pad (bottom right) is the Style pad.** Each press steps the play style: Block, Strum D (low to high), Strum U (high to low), Arp Up, Arp Dn, Arp U/D, Arp Rnd. The pad shows the style's name and lights brighter when it is not Block. The style only changes chord pads; scale-note pads always play one note.
- **Strum:** the notes start one after another, 10 to 80 ms apart (default 28); the first note leans in a little. Releasing the pad stops all of it, and a tap shorter than the strum plays only the notes that had started.
- **Arpeggio:** steps through the chord's notes (1 to 3 octaves) at 1/4, 1/8, 1/8T, 1/16 (default), 1/16T or 1/32 of a beat, each note lasting the Gate percent of the step (default 80). Only one arpeggio runs at a time: a new chord pad replaces it. The tempo is the loop's once there is one, otherwise the starting tempo, otherwise 120 BPM. While a loop plays, the first step waits for the next step of the loop's grid, so recorded arpeggios sit in time. Hold keeps an arpeggio going after you let go.
- **Humanise (0 to 100, default 0 = exact):** lowers each note's velocity by up to a third (35% at 100), loosens a block chord by a few milliseconds per note, and varies the arpeggio's note length. Velocity goes to the synth, and to the pad sample.
- **Chords page 3 (B7 twice):** Play style, Humanise, Strum speed, Arp rate, Arp range, Arp gate, Velocity. Velocity Fixed plays everything at 100 (before humanise); From the pads uses the velocity the pad sends. **Untested:** I do not know what values the Tracker's pads send, so this is Fixed by default.
- All of it is saved in `FourTrack/sound.cfg` (the format changed again, so an older file is ignored).
- **Timing:** the strum and arpeggio are timed by the application's main loop (a step is on time to within one loop pass). If a strum or arpeggio sounds lumpy on the device, tell me; the fix is to schedule it in the audio block.
- Flash 560,236 bytes (53.4%).

## FT46 addendum 3: levels, and the lo-fi master is opt in
- **Found:** a recorded chord loop played about 10 dB louder than the live synth, so melody notes were buried. The cause was the Lo-fi master, on by default: its saturation raises quiet material, and the live pads do not go through it.
- **The lo-fi master now starts bypassed** (Master page, B1 switches it on). Saved projects keep their own setting; new projects start off.
- **Home screen levels:** Left and Right change the synth's level (1 dB a click, -30 to +12 dB, saved with the sound settings); Shift + Jog changes the selected track's level (1 dB a click). The hint line shows the value. The synth level is also on the Chords page 3 ("Synth level").
- Because the loop no longer gets the boost, recorded loops are about 10 dB quieter than before relative to the live synth. Raise a track with Shift + Jog if you want it louder.

## FT47 (sent on request): input latency calibration
This is FT46 (chord pads, synth, play styles, Sound page, lo-fi master off by default, levels on Left and Right and Shift + Jog) plus the latency test. **Everything from FT45 on is new on the device: the synth's CPU load and the chord pads have not run on a Tracker yet.** Keep a stock `.ptf` on the card.
- **Calibrate:** Config, Four-Track, Input latency (row 5), B1 Calibrate. Connect a cable from the Tracker's output to its line input, turn the volume to about 40, press B8 Start. It plays a loop of clicks through the normal loop player and records the line input (4 seconds), then shows the delay in ms. B8 Keep stores it (the file `FourTrack/latency.cfg` on the card); B7 Retry. It needs an empty project (it uses Track 4's memory): clear all Parts first (Shift + Delete) if it says so.
- **What it changes:** line and mic Parts are moved earlier by the measured latency when they are recorded (first takes and later Parts); pad and synth Parts are not. You can also set the number by hand with the jog on the Input latency row (Shift: finer).
- **What to report:** the number it gives (it should be around 6 to 12 ms if the research guess is right; ticket 13 wants the value), whether the clicks were heard ("Heard nothing" means the cable or the volume), and whether a guitar or line take then lands where you played it.
- Tests: the measurement is checked against known delays from 0 to 2,500 frames at every loop start position, a quiet noisy line, no cable and a steady tone; in the emulator the same test measures the simulated cable delay exactly.


## FT48 to FT51 (not sent): chords to the design, Play view, loop mechanics
Built and tested in the emulator and the host tests; not run on a Tracker.
- **Chords (Note button):** left hand = chords I to vii (7ths row 1, triads row 2) and two octaves of scale notes; right hand = modifiers sus2 sus4 6 7 / maj7 add9 9 flip, play styles Block, Strum dn, Strum up, Arp up, Arp dn, Oct -, Oct +, Hold. Hold a modifier while you press a chord, or double-tap it to latch. Chord colours: Home, Away, Tension. Settings across the top: Key, Scale, Style (a feel preset), Sound, Voicing, Octave. B2 Sets: 8 chord sets saved on the card; each project remembers its set. Config, Chords group: Humanise, Strum speed, Arp rate, range, gate, Velocity, Synth level.
- **Play view:** press Pattern on the home screen while the pads are chords (or B7 on the Chords page): the selected Part's waveform with the playhead, the other tracks in one line, the chord that is sounding, and the pad map. Rec, Play, Stop and the track buttons work there.
- **Loop mechanics:** new projects start at 80 BPM, 4 bars, one bar of count-in. Rec: the click counts one bar in, the take records exactly 4 bars and stops by itself, then plays (no question). Tempo panel (B2): Free, Set, tempo, count-in, wait for sound, **Bars** (only lengths that fit 16 s). Bars "Ask later" brings back the old question. **Tail wrap** (Config, Four-Track): the recorder keeps running 2 s after the bar line and what rang on is folded onto the start of the loop. **Later Parts:** a countdown (4 3 2 1) shows in the lane before the Part starts. **Layer (default) or Replace** (Config, Four-Track, Later Parts): a new take on a track that has a Part is added on top of it; **Fade** (40 to 100%, Shift + Jog while armed or recording) lowers the older audio under each new pass. Undo (Shift + Copy, or the actions menu) brings back the previous layer. Needs the SD card.
- **Check on the device:** that Rec at 80 BPM stops on the bar (the loop should be exactly 12 s), that the loop does not click at the join, that a layered take lines up with the old one, and whether the 2 s of extra recording after the bar line is in the way.

## FT49 to FT57 (not sent): the rest of round 6
Everything below is new on the device and untested there. Built and tested in the emulator and the host tests. Keep a stock `.ptf` on the card.
- **Sound page:** twelve patch tiles with their one-cycle shapes, a picture of one note (press, hold, release; orange when tweaked), eight dials. Dial rings, shapes and the note are 1-bit pictures in the display chip's memory (6.5 KB after the page picture).
- **Home controls (as the design):** the jog scrubs the position, stopped or playing (a sixteenth a click, Shift a bar); Left and Right step one beat; Up and Down choose the track; Shift + Up or Down is the selected track's level, Shift + Left or Right the synth level; Shift + Stop is Undo take; Settings is a button on the Project page; the output volume is only in Config.
- **FX1 and FX2:** FX1 opens reverb and delay (per-track sends). FX2 opens the live filter (hold or latch, sweep with the jog; a FILTER tag shows in the top bar). Both are in the shared core, so no stock audio file changed; they add about 300 KB of external RAM (now 91.8% used). **On the Tracker the filter acts on the loops and the synth, not on the line monitor or the sample pads.** Check CPU: a long loop with reverb and delay on, the synth playing and a filter latched.
- **Boot logo:** the design's tape-reel logo replaces the Polyend logo at start-up on the Original Tracker, centred.
- **Track roles:** Rec on an empty track asks what to record: B1 Chords, B2 Bass, B3 Melody, B4 Drums, B5 Audio; then pick the sound with the jog and press B8 to arm. Shift + Rec arms with the last role. The track's tag shows CHORD, BASS, LEAD or DRUM. Press Pattern to see the pad map. **Drums** use Plaits' kick, snare and hi-hat models and 24 kits (rows 1 to 3 on the left); right-hand pads are the voices; hold a repeat rate (1/8 to 1/16T) while hitting a drum to roll it; Latch keeps the rate. **Bass and Melody:** three octaves of scale notes, the right-hand helpers (Root, 5th, 3rd, Octave, Walk, Lead-in, Ghost, Short, Accent; Glide, Mono, Bend, Vib, Scale, Pent, Blues, Chord, Repeat, Hold). They follow the chord at the playhead from the chord changes written during a Chords take. Role pads are recorded as audio like any take.
- **Check on the device:** the drum sounds (the Plaits models were tuned for 48 kHz and here run at 44.1 kHz), the CPU with a drum roll and a held synth chord together, that nothing clicks when you change kit, and the pad lights in each role.

## FT58: synth CPU fix
FT57 crashed on the first synth note (stuck chord, scratchy sound, then a crash). Cause found by counting instructions: four voices cost about 90 million instructions a second, too much for the Tracker's processor alongside everything else, so the audio interrupt starved the main loop. FT58 replaces the Plaits oscillators in the synth with lean band-limited ones (the Plaits ensemble, overdrive and drums stay), works the filter cutoff out every 8 frames and the envelopes every 4, and renders a drum voice only while it rings. **New: Config, About, "Synth load"** shows the heaviest block of the synth since you opened the page as a percent of its time. Play a four-note chord, open About and tell me the number; above 80% means dropouts. The sound is a little different from FT57 (the oscillators are not the Plaits ones).

## FT59: waiting for the first note
- Wait for sound (Tempo panel, B2: Left/Right to the box, Insert switches it) now replaces the count-in: Rec arms at once.
- While it waits, the lane shows the input level and the level it starts at, the hint says "Waiting for your first note", and the **jog sets the start level live** (1 dB a click, Shift 3 dB). Set it above any buzz and below your note.
- The home hint on an empty project says B2 holds tempo, bars and Wait for sound.

## FT60: the start line on the arming screen
While Wait for sound is armed the lane says "Recording starts at the orange line", draws a thick orange line on the input meter labelled "START -36 dB", and shows the input level now at the right. The jog moves the line; no trip to Config is needed.

## FT61: layering in real time
FT60 and earlier recorded a layer over the Part's own memory, so the old audio was overwritten while the pass played (a gap), and the mix was done afterwards from the card. Now the pass is recorded in the memory of an **empty track** and added into the playing Part as it comes in, each frame lowered by the Fade level first. The old layers keep playing during the pass, the new one joins the loop as you play it (heard from the next time round), and Stop part-way only changes the part you played. **It needs one empty track** (otherwise a take replaces, with the question as before) and an SD card (the old audio is still saved there for Undo, which happens when you press Rec, so there is a pause of a second or so before it arms).

## FT62 and FT63: the arm level is a Config setting
FT62 put a level test in front of every take; that was wrong, so FT63 removes it. Arming a take shows only the start line (the level is not changed there). **Config, Four-Track, Wait level, B1 "Set level"** opens a page with the live input, the orange start line, a peak mark and "would start / stays below"; the jog moves the line; it is saved (`loop.cfg`) so you set it once for a new device or guitar. Wait for sound itself is still switched on or off in the Tempo panel (B2).

## FT64: every update file listed
Config, Firmware now has one row for each `.ptf` in the card's `Firmware` folder (up to 8), newest build first, instead of one row you had to jog through. Move to a row and press B1 to install that file; B1 on the "Installed" row installs the newest.
