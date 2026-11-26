# Round 5 brief for the design tool

Paste everything below the line. Same style and limits as rounds 3 and 4 (docs/design/round-4-brief.md, screen-requirements.md). The firmware now matches your round 4 frames closely, so keep the same look: the same top bar, the same 97 px button cells, the same card and row shapes.

---

Round 5. The Four-Track screens are built. Now we want to rebuild the Tracker's remaining stock screens in the same language, so the whole device feels like one product and we can drop a lot of stock code to make room for a synth and chords. Please design the screens below. For every screen give, as before, a frame per state, what each of the eight buttons, the jog wheel, the arrows and Shift do, and a note of the colour roles and font slots used.

## A. Limits (unchanged from round 4)
- 800 x 480. Eight buttons under the screen, 97 px cells on a 100 px pitch, about 9 to 10 characters per label.
- Five font slots: 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular. No icons except shapes you draw (square, circle, triangle, lines).
- Drawing budget: about 8 KB of display commands for the whole screen, 65 controls. Keep each screen to roughly 100 elements. Waveforms 100 to 150 columns.
- No fades, slides, scrolling or zoom. Playheads, meters, a moving highlight and changing numbers are fine.
- Use the named colour roles from round 4 so a theme can recolour it.

## A2. The stock screens today
Pictures of the five stock screens are in `docs/design/stock/` (params.png, config.png, sample-load.png, sample-play.png, sample-rec.png). What they show, so the rebuilt screens can keep what matters:
- **Params ("Instrument Parameters 1/2"):** eight vertical faders with labels and values underneath: Volume, Panning, Tune, Finetune, Filter Type (Disabled, Low-pass, High-pass, Band-pass), Cutoff, Resonance, and More (page 2).
- **Config:** a list of groups on the left (Project Settings, General, MIDI, Metronome, Firmware, Info) and the rows of the chosen group on the right (Performance presets, Pattern divider, Notes scale, Root note, Notes layout, Anti-aliasing, Limiter mode, Sends mode, Extra headroom, Snap to zero). The bottom row has up and down arrows for the group and the row, and Enter.
- **Sample Playback:** a big waveform, Preview, Start, End and Zoom at the bottom, and a list of play modes (1-Shot, Forward Loop, Backward Loop, Pingpong Loop, Slice, Beat Slice, Wavetable, Granular).
- **Sample Recorder:** a source list (Line in, Radio, Mic LG, Mic HG), two level meters, a Monitor on/off list, and buttons Source, Metronome Config, Level, Gain, Monitor and a red Record.
- **Sample Loader:** the SD card folders on the left, the 48 instrument slots in the middle, a preview level meter, and buttons Enter, Add next, Preview, Rename, Import, Delete unused, Preview Vol and Memory.
Wavetable, Granular, Slice and Beat Slice are being dropped. The Radio source is not needed. The stock look is plain black and white; the rebuilt screens should use our palette and shapes.

## B. Buttons and what they open
The Tracker has these buttons. Four-Track screens already use the ones marked (done).

| Button | Opens | Status |
|---|---|---|
| Pattern | Home: the main screen | done |
| Sample Edit | Part Editor | done |
| Performance | Glitch | done |
| Master | Lo-fi master | done |
| File | Project | done |
| Song | switches the click on or off | done |
| Insert | action menu on the selected track | done |
| **Params** | **Track mixer (new, section C1)** | design |
| **Sample Load** | **Load samples (rebuilt, C2)** | design |
| **Sample Play** | **Pad sample settings (rebuilt, C3)** | design |
| **Sample Rec** | **Resample (C4)** | design |
| **Config** | **Device settings (rebuilt, C5)** | design |
| **FX1** | **Reverb and delay sends (C6)** | design |
| **FX2** | **Live filter (C7)** | design |
| **Copy** | **Copy a Part to another track (C8)** | design |
| **Delete** | **Clear a Part or all Parts (C9)** | design |
| **Note** | **Chords (C10, later, only a rough layout now)** | design |
| **Instr** | **Choose the sound (C11, later, only a rough layout now)** | design |

## C. Screens

### C1. Track mixer (Params button)
Purpose: set how loud each track is and where it sits, all in one view. Today there is no level control per track.
Shows: four columns, one per track, in the same order and with the same names as the main screen. Each column: level (a vertical fader or bar with a dB number), pan (left to right with a centre mark), tone (a low-pass to high-pass sweep with a centre mark), mute and solo marks, and the live meter. The selected track is highlighted.
Controls to define: arrows move between tracks and between the three controls; the jog changes the value in focus; Shift+jog moves in smaller steps; buttons for mute, solo and reset-to-default for the selected track.

### C2. Load samples (Sample Load button)
Purpose: pick WAV files from the SD card and put them on pad slots.
Shows: a list of folders and files with size and length, a preview of the highlighted sample (a coarse waveform and play button), the destination slot (1 to 48) with the name of what is there now, and memory left.
Controls: up and down choose; jog scrolls; one button loads into the chosen slot; one button previews; one button goes up a folder. Also a way to load straight onto a track as a Part (a later feature; leave a button for it).

### C3. Pad sample settings (Sample Play button)
Purpose: set how one pad sample plays. This replaces the stock instrument editor, which had far more than is needed.
Shows: the sample name and slot, a coarse waveform with start and end markers, and these fields only: tune (semitones and fine), volume, pan, play mode (one-shot or loop), start, end. Keys mode or Pad mode for the whole pad set.
Controls: arrows choose a field; jog changes it; a button to audition the sample.

### C4. Resample (Sample Rec button)
Purpose: record the finished mix (after the lo-fi master) into a pad slot, so a loop becomes a playable sample.
Shows: the destination slot, a record source label (always "Mix"), the length, a level meter, memory left, and state: ready, recording, saved.
Controls: one button starts and stops; one button picks the slot; a button to name the sample (reuse the name entry from Rename).

### C5. Device settings (Config button)
Purpose: the settings that belong to the device, not to a project: MIDI in and out routing and channel, USB audio, line input mode (stereo or mono left or right), headphone and master volume, display brightness, firmware update, about (build id).
Shows: grouped rows like the Four-Track settings page, with a short help line for the focused row. Leave a group for Four-Track device settings (alignment window, pre-roll, tempo rounding, wait level).
Controls: up and down choose; jog changes; Back.

### C6. Reverb and delay sends (FX1)
Purpose: a small effects page for the whole mix: reverb (amount, size) and delay (time as a note value, feedback, amount), plus a send level per track so each track can have its own amount.
Shows: two blocks and four small send bars per effect.

### C7. Live filter (FX2)
Purpose: a performance control: a low-pass to high-pass sweep on the master that you can hold and release while playing. One big control with a numeric readout, a reset button, and a choice of sweep (LP, HP, band).

### C8. Copy a Part (Copy button)
Purpose: copy the selected track's Part onto another track. A small panel over the main screen like the Replace popup: "Copy Track 2 to ...", the four tracks as choices (the source greyed), and a warning if the target already has a Part.

### C9. Clear (Delete button)
Purpose: clear a Part or every Part. A popup in the same style as Replace Part: "Clear Track 3?" with Cancel and Clear (red). With Shift: "Clear all tracks?". Say that the cleared take can be brought back with Undo.

### C10 and C11. Chords (Note) and Sound (Instr), later
Do not design these in detail yet. Give one rough frame each so we can reserve the buttons: Note is a chord picker with chord sets, a row of the next suggested chords, and the sample or synth that plays them. Instr is the list of sounds (a sample from the pads, or the synth later) and the few controls each one needs. We will detail these after we know how much space is left.

## D. What I want from you
- One 800 x 480 frame per screen and state, with a note on colour roles and font slots.
- A control table for each screen.
- A list of anything you dropped because of the limits, and anything that needs a button we do not have.
