# Screen requirements: Four-Track Recorder (Original Polyend Tracker)

This is the complete list of screens and what each must do. It replaces the earlier briefs where they differ. It says WHAT each screen is for, what it shows, and what every control does. HOW it looks (colour, type, layout, style) is entirely your decision. The hardware limits in section 2 are the only visual rules, and they are real.

If a requirement here cannot be done within the limits, say so and propose the closest thing. Do not silently drop a requirement.

---

## 1. The product

A four-track recorder, like a cassette Portastudio but digital and immediate. One person records guitar, voice, bass, and finger-drummed samples, one part at a time. Alignment is nearly automatic. Lo-fi sound is wanted. The screen is small and fixed, with no touch.

Vocabulary, use these words on screen:
- **Track**: one of four lanes. **Part**: one recording on a track. **Loop**: the repeating region; the first Part recorded sets it. **Loop Mode**: Repeat or Play once. **Offset**: how far a Part is shifted from the loop start, in time or bars. **Alignment**: a new Part's first sound is placed on the loop start automatically.
- Time signature is 4/4. Position is shown as bar.beat.sixteenth.

Core flow the screens must serve:
1. Optionally set a starting tempo (with a one-bar count-in), or leave it Free.
2. Record the first Part. When stopped, ask "How many bars?" and derive the tempo from the length played.
3. Record later Parts: choose a track, arm, the Tracker waits for the loop start, records exactly one loop, stops. Its first sound is lined up with the loop start if it falls within a small window.
4. Edit a Part: trim, fades, normalize, offset.
5. Play live pads on top and record them onto a track.
6. Glitch and lo-fi the result. Save the project. Export WAV.

---

## 2. Hardware facts and limits (real, not negotiable)

Screen: 800 x 480 px, one screen at a time, no touch, no hover.

Controls:
- **Eight buttons** in a row under the screen, in eight cells of 97 px on a 100 px pitch. A cell fits about 9 to 10 characters. These are called B1 to B8 here, left to right. Their labels change per screen and are drawn by us on the screen.
- **Jog wheel**: turn and press.
- **Four arrow buttons**: up, down, left, right.
- **Shift** button.
- **Hardware Rec, Play and Stop buttons**.
- **48 pads**.
- Dedicated buttons that open screens (see section 3).

Fonts: five slots only: 14 light, 17 light, 14 regular, 17 bold, 12 regular. There is no bold at size 14. Use 17 bold for emphasis. Keep 12 for secondary text.

Icons: only Pause, Play, Loop and Perf exist as glyphs, each two cells wide. There is no Stop or Rec glyph: draw Stop as a filled square and Rec as a filled circle, or use text. Everything else is text or simple shapes.

Colour: flat opaque colours only, named by role so the screen can be re-themed, and it must still read in a one-colour Monochrome theme.

Drawing: text, filled and outlined rectangles, level meters, a thin playhead line, point markers, and a waveform shown as a min/max envelope. No images, no rounded corners.

Drawing budget: the whole screen has a small budget. **Waveforms: plan about 100 to 150 columns per track**, not more. Keep the number of separate elements low. If the budget runs out, the top bar and button labels are what disappear, so keep everything else simple.

Motion: allowed: playhead line, level meters, a pulsing record or armed indicator, a highlight that moves between tracks or fields, changing numbers (steps of about 100 ms or on the beat). Not allowed: fades, slides, scrolling, zooming, whole-screen transitions. A blink must change an element's colour, not remove it, and a readout must keep a fixed width.

Memory: about 24 seconds maximum per Part. Only so many seconds fit, so memory use is shown to the person.

---

## 3. Buttons that open screens

| Button | Screen |
|---|---|
| Pattern | Main screen (home) |
| Sample Edit | Part Editor |
| Performance | Glitch and live effects |
| Master | Lo-fi master |
| File | Project |
| Config | Settings (Four-Track settings page inside the stock Config) |
| Sample Play, Sample Rec, Sample Load, Params | stock screens (sample playing, recording a sound into a slot, loading samples, sound settings). These stay as they are and are NOT part of this brief. |
| Song | Free. Reserved for chords later. Do not design it. |

Return rule: Pattern always goes home. A screen reached from home returns to home when finished.

Top bar (on every screen of ours): project name; tempo (or "Free"); loop length in bars; position as bar.beat.sixteenth; input source and monitor state; feedback-protection indicator when it is acting; a small unsaved-changes mark.

---

## 4. Screens

### 4.1 Main screen (home)
Purpose: record, play and see all four tracks at once.

Shows:
- Top bar as above.
- Four tracks always visible, no scrolling. For each: number, name, mute state, level meter, a coarse waveform of its Part, record-source label, a mark if glitch is on, the Offset if not zero.
- The Loop region and a playhead across all four.
- The selected track clearly marked.
- A master level meter.
- Transport state: stopped, playing, armed (waiting for the loop start), recording.
- A line of hint text for what pressing a button will do next.
- Pad mode indicator: Keys (one sample across the pads at different pitches) or Pad (one pad, one sample).

Record-source label (per track): LINE, MIC, PAD (a loaded sample played from pads), later SYN. Set when the track is armed. Stays after recording. 3 to 4 letters is enough.

States to design (each is a frame):
1. Stopped, all four tracks with a Part (guitar, drums, bass), one empty.
2. Stopped, nothing recorded yet.
3. Recording the first Part: no loop exists yet, elapsed time counts up, level meter live.
4. Armed on a later track: waiting for the loop start; show the time until the start.
5. Recording a later Part: one loop, with a progress indication.
6. Playing.
7. Memory warning during the first take.

Controls:
- B1 Rec (arms or starts recording on the selected track). B2 Stop. B3 Play. B4 Monitor on/off (quick toggle). B5 to B8: Track 1 to 4 (select).
- Arrows: left/right move the playhead by a beat when stopped; up/down change the selected track.
- Jog turn: while stopped or playing, scrub the position. Shift plus jog: move by bar.
- Jog press: select what the highlight is on.
- Shift plus a Track button: mute or unmute that track.
- Shift plus B1 (Rec): Loop Mode toggle (Repeat or Play once).
- Hardware Rec and Play: same as B1 and B3.
- Stop during a later-Part recording keeps what was recorded up to that point and fills the rest of the loop with silence.

### 4.2 Starting tempo and count-in
Purpose: optionally set a tempo before the first take.
Reached: from the main screen when nothing is recorded, as a prompt, or by pressing a button on the empty main screen.
Shows: tempo with two choices, Free or a number; count-in on or off.
Controls: jog sets the value; B1 Free; B2 Set; B8 Cancel.
Rule: if Free, the first take sets the loop and the tempo is derived from its length. If a tempo is set, a one-bar count-in plays before the first take and the first take lands on the grid.

### 4.3 How many bars?
Purpose: after the first take, choose its length in bars; tempo follows.
Shows: the length played in seconds; the choices; the tempo each choice gives; the recommended one preselected.
Choices: 1, 2, 3, 4, 6, 8, and after a very long first take also 12, 16, 20, 24, 28, 32.
Controls: jog or left/right chooses; B7 Cancel (discard the take); B8 OK.
Rule: no audio is stretched. Only the tempo is calculated.

### 4.4 Part Editor
Purpose: fix one Part. Edits the selected track's recording.
Reached: Sample Edit button.
Shows: coarse waveform of the Part with the Loop region; the fields below, with the field in focus clearly marked.
Fields: Trim start; Trim end; Fade in; Fade out; Normalize (action); Offset in milliseconds and in bars (two readouts of one value).
Controls: left/right choose a field; jog changes it; Shift plus jog changes in bigger steps (bars for Offset). B1 Audition (play the Part). B2 Normalize. B3 Undo last change. B4 Redo. B7 Cancel. B8 Done.
Rules: Offset in Repeat mode wraps around the loop; in Play once mode anything pushed past either end is cut off. The caption and the waveform must follow Loop Mode.
Undo: one level; the previous take is kept on the SD card.

### 4.5 Glitch and live effects (Performance)
Purpose: change how a recorded track plays back, live, while the loop runs.
Reached: Performance button.
Shows: four columns, one per track. In each: a few effect slots, each showing its name, whether it is on, and its amount.
Effects per slot (choose from a list):
- Playhead jumps: jump back (one slice, two slices), stutter (repeat a small slice), reverse.
- Pitch step to an interval (fifth, seventh, octave, fourth, third, down an octave and others), per half-beat slice.
- Pitch style per track: tape-style (speed changes with pitch) or granular-style (length stays the same).
- Stock effects: bit-crush, overdrive, pan, filter (low-pass, high-pass), volume.
Controls: arrows move between tracks (left/right) and slots (up/down); jog changes the amount; jog press switches a slot on or off; B1 to B4 hold-to-trigger slot 1 to 4 on the selected track; B5 Clear track; B6 Clear all; B7 pitch style for the track; B8 Back to main.
Rules: everything stays in time with the loop; it never changes the length of the loop; a glitch mark shows on the main screen for any track with a slot on.

### 4.6 Lo-fi master
Purpose: the whole mix gets a lo-fi sound.
Reached: Master button.
Controls shown, each with a value and a clear on or off: Glue compression (amount); Saturation (amount); Wow and flutter (depth and speed); Bit-crush (amount); Master volume. Also a gain-reduction readout for the compressor.
Controls: arrows move between controls; jog changes; B1 Bypass all (compare with the dry mix); B8 Back.
Rule: bypass must be instant and obvious.

### 4.7 Input setup
Purpose: set where the sound comes from and how loud.
Reached: from the main screen (a button or Shift plus B4), and shown in the top bar.
Shows: source choice (Line, Mic low gain, Mic high gain); gain value; a live level meter with a peak marker; monitoring on or off; the feedback-protection state.
Gain ranges: line about 0 to +22 dB; mic low 0 to +25 dB; mic high 0 to +44 dB.
Controls: B1 to B3 choose the source; jog changes gain; B4 Monitor on or off; B8 Done.
Rules: line monitoring starts on, mic monitoring starts off. Feedback protection is always on while monitoring is on and has no off switch; it only affects the monitored signal, never what is recorded. When it is acting, the top bar shows it, and returns to normal when it stops.

### 4.8 Project
Purpose: open, save and manage projects.
Reached: File button.
Shows: a list of projects (name, length in bars, tempo, date); the one highlighted.
Actions: New; Open; Save; Save as; Rename. Deleting is not in this version.
Controls: up/down choose; jog scrolls; B1 New; B2 Open; B3 Save; B4 Save as; B5 Rename; B8 Back.
Rules: before New or Open with unsaved changes, ask "Save your changes?" (Cancel, Don't save, Save). Name entry uses a text entry that fits the controls (jog to choose a character).

### 4.9 Four-Track settings
Purpose: change the behaviour settings.
Reached: Config, then Four-Track.
Per project: Loop Mode (Repeat or Play once); Click (on or off, during playback and before a Part).
Whole device: Alignment window; Pre-roll; Tempo rounding; Input source; Gain; Monitoring default.
Controls: up/down choose; jog changes; B8 Back.
Rule: show clearly which settings belong to the project and which to the whole device.

### 4.10 Export
Purpose: write the mix to a WAV file.
Reached: from the Project screen (B6 Export).
Shows: choose how many times the Loop repeats (1 to 16, default 1); the length that gives; progress while writing.
Controls: jog chooses repeats; B1 Start; B8 Cancel.
Rule: progress is a bar with a percentage; it ends with a "Done" message and the file name.

### 4.11 Pad and sample settings
Purpose: choose the sample the pads play, and how.
Reached: from the main screen (a button), and the stock sample screens still exist.
Shows: the chosen sample name; pad mode (Keys or Pad); how many samples are loaded; memory used.
Controls: up/down choose; jog changes; B1 Keys; B2 Pad; B8 Back.
Rules: Pad mode: one pad triggers one sample (the 48 pads map to 48 sample slots). Keys mode: one sample spread across the pads at different pitches. Mode is set per sample. MIDI controllers play the same sounds as the pads.

### 4.12 Memory
Purpose: show how much room is left.
Shows: recording time left for the next Part; samples loaded; a plain bar for memory used.
Controls: B8 Back.

### 4.13 Popups and notices
Each is a frame that appears over the current screen with no fade. Each says what happened, and what each button does.
1. **Replace Part?** Shown when recording over an existing Part. Cancel or Replace.
2. **No sound.** Shown when a take has no sound or is silent. Retry or Cancel.
3. **Memory full (first take).** It stops by itself, keeps everything recorded so far, then goes to "How many bars?" with this notice.
4. **Memory full (later Part).** The Part is not armed because there is not enough room for one loop.
5. **Export progress.** See 4.10.
6. **SD card missing.** Recording and saving are unavailable. Retry or Back.
7. **Unsaved changes.** Cancel, Don't save, Save.
8. **Feedback protection acting.** A small indicator in the top bar, not a popup.

---

## 5. Not designed in this round
- Chord screen and synth screen (later). Song button stays free.
- Boot (start-up) graphic. Parked.
- The stock screens for sample loading, sample recording, instrument settings and Config, apart from the Four-Track settings page.

---

## 6. What to give back
- One 800 x 480 frame per screen and per state listed. Name each frame after its section number here.
- For every screen: which button does what, in a simple table (B1 to B8, jog turn, jog press, arrows, Shift combinations). Use the assignments above unless something does not fit, and say why.
- A note of the colour roles and font slots used.
- A list of anything you could not do within the limits, and what you did instead.
- Do not add features that are not listed here. Ask first.
