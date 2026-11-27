# SCREENS · Four-Track Recorder, round 6

Exported from `Four-Track Recorder.dc.html` on 2026-11-27. That file is the source of truth; this document is generated from it, so ids, titles, controls and pad grids match the frames exactly. 26 sections, 78 frames, each 800 × 480. Rules for shape, colour, type and drawing are in STYLEGUIDE.md.

Numbering: 4.x Four-Track screens, 6.x chord and synth screens (6.1 Chords, 6.2 Sound, 6.3 Home with chord pads, 6.4 Play view, 6.5 Track roles), C1 to C9 stock screens restyled. Section 0 is the component sheet. Gaps in 4.x (4.9, 4.11) are screens that were dropped.

Pad grids: 4 rows × 12 columns as on the device, row 1 at the top. Columns 1 to 8 are the left hand, 9 to 12 the right. Each cell is the label as drawn, then its state in brackets. x is the left edge of each column, y the top of each row, all in px; pads are w wide, h tall, fills inset 3 px.

## Index

| Section | Title | Frames |
|---|---|---|
| 0 | Components · the source of truth for every screen below | 0 |
| 4.1 | Main screen (home) · Pattern button | 4.1a, 4.1b, 4.1c, 4.1d, 4.1e, 4.1f, 4.1h, 4.1g |
| 4.2 | Starting tempo and count-in | 4.2 |
| 4.3 | How many bars? | 4.3a, 4.3b |
| 4.4 | Part Editor · Sample Edit button | 4.4a, 4.4b |
| 4.5 | Glitch and live effects · Performance button | 4.5 |
| 4.6 | Lo-fi master · Master button | 4.6a, 4.6b |
| 4.7 | Input setup | 4.7a, 4.7b |
| 4.8 | Project · File button | 4.8a, 4.8b |
| 4.10 | Export · from Project, B6 | 4.10a, 4.10b, 4.10c |
| 4.12 | Memory | 4.12 |
| 4.13 | Popups and notices | 4.13a, 4.13b, 4.13c, 4.13d, 4.13e, 4.13f, 4.13g |
| 6.1 | Chords · Note button | 6.1a, 6.1b, 6.1c, 6.1d |
| 6.2 | Sound · Instr button | 6.2a, 6.2b, 6.2c |
| 6.3 | Home with the chord pads on | 6.3a, 6.3b |
| 6.4 | Play view · Pattern again on home | 6.4a, 6.4b, 6.4c, 6.4d, 6.4e |
| 6.5 | Track roles · what you record decides the view | 6.5a, 6.5b, 6.5c, 6.5d, 6.5e |
| C1 | Track mixer · Params button | C1a, C1b |
| C2 | Load samples · Sample Load button | C2a, C2b, C2c, C2d |
| C3 | Pad sample settings · Sample Play button | C3a, C3b |
| C4 | Resample · Sample Rec button | C4a, C4b, C4c, C4d |
| C5 | Device settings · Config button | C5a, C5b, C5c, C5d |
| C6 | Reverb and delay sends · FX1 button | C6a, C6b |
| C7 | Live filter · FX2 button | C7a, C7b, C7c, C7d |
| C8 | Copy a Part · Copy button | C8a, C8b |
| C9 | Clear · Delete button | C9a, C9b, C9c |

## 0 · Components · the source of truth for every screen below

### 0 · Every component in its states

Budget: 56 shapes (11 rect, 41 rounded, 2 circle, 0 line, 2 bitmap) · 49 texts · ≈ 5.1 KB of 8.2

## 4.1 · Main screen (home) · Pattern button

### 4.1a · Stopped · three Parts, one empty track

Budget: 111 elements · 3 bitmap waveforms · 16 waveform columns

Track 4 is selected. Bass is muted and has an Offset. Guitar and Drums show GLT because a Performance slot is on.

### 4.1b · Stopped · nothing recorded

Budget: 76 elements

### 4.1c · Recording the first Part

Budget: 86 elements · 1 bitmap waveform

No Loop exists yet, so there is no ruler or playhead. The elapsed time counts up in steps of 100 ms.

### 4.1d · Armed on a later track

Budget: 119 elements · 3 bitmap waveforms · 64 waveform columns

Arming opens Track 4. The others shrink to strips in their usual order. The red edge is where recording will begin; the circle blinks.

### 4.1e · Recording a later Part

Budget: 110 elements · 4 bitmap waveforms · 64 waveform columns

### 4.1f · Playing

Budget: 116 elements · 3 bitmap waveforms · 16 waveform columns

### 4.1h · Focus on Track 2 while playing

Budget: 109 elements · 3 bitmap waveforms · 64 waveform columns

Same layout as recording, for listening closely or before trimming. Stop also returns to the overview.

### 4.1g · Memory warning during the first take

Budget: 86 elements · 1 bitmap waveform

#### Controls

| Control | Does |
|---|---|
| B1 Rec | Arms the selected track. With nothing recorded it starts the first take. Hardware Rec does the same. |
| B2 Stop | Stops playback or the take. On the empty screen (4.1b) it reads Tempo and opens 4.2. |
| B3 Play | Plays from the position. Hardware Play does the same. |
| B4 Monitor | Monitoring on or off for the current input. |
| B5–B8 | Track 1 to 4: select the track. Pressing the selected track again opens it in focus (4.1h); again returns to the overview. |
| Jog turn | Scrubs the position, stopped or playing. |
| Jog press | Selects what the highlight is on. |
| Arrows | Left/Right move the playhead one beat when stopped. Up/Down change the selected track. |
| Shift + jog | Moves by a bar. |
| Shift + B1 | Loop Mode: Repeat or Play once (shown at the right of the hint line). |
| Shift + B4 | Opens Input setup (4.7). |
| Shift + B5–B8 | Mute or unmute that track. |
| Shift + B3 | Opens Memory (4.12). |

#### Notes

- Two layouts. Overview (4.1a, b, f): four 84 px lanes. Focus (4.1c, d, e, g, h): the armed, recording or chosen track opens to 250 px; the other three become 28 px strips in their usual order. The switch is instant.
- Waveforms are drawn once per Part into a 1-bit bitmap, 600 px wide (one column per pixel), uploaded when the Part changes. Each frame shows it with one bitmap command: text colour for the selected track, wave for the others. Each lane size (84, 250, 28 px) has its own bitmap, or one is scaled if the chip allows. See STYLEGUIDE section 1.
- While recording (4.1c, e, g) the take is drawn as 120 red columns that grow behind the playhead; it becomes a bitmap when the take ends.
- Focus opens on arming or on the first take, and closes when the take ends or Stop is pressed.
- Each track has its colour (t1–t4) on its number, header edge, meter and waveform, with a 10% tint on the header and lane (22% on the selected header). Ahead of the playhead the lane is darkened by one see-through rectangle; muted lanes by a second one.
- Grid: bar lines and fainter beat lines over all four lanes, one rectangle each. In focus, step ticks are added to the ruler and the ruler highlight steps every 16th; in the overview it steps every beat.
- Header: number and name; M; source tag; GLT; Offset; level meter. Strips keep only number, name and M.
- Budget, from the coding agent: about 4.5 KB with four Parts and bitmaps, under the 8,172-byte limit. If bitmaps fail on the device, fall back to 60 columns in the overview, 120 in focus and 30 in strips (STYLEGUIDE section 1).

## 4.2 · Starting tempo and count-in

### 4.2 · Starting tempo · over the empty home screen

Budget: 83 elements

#### Controls

| Control | Does |
|---|---|
| B1 Free | No starting tempo. The first take sets it. |
| B2 Set | Uses the tempo shown and turns on the one-bar count-in. |
| B8 Cancel | Back to home with nothing changed. |
| Jog turn | Changes the highlighted value: tempo in 0.1 BPM steps, or count-in on/off. |
| Arrows | Up/Down move the highlight between Tempo and Count-in (added; the brief has no control for count-in). |
| Jog press · Shift | Not used. |

#### Notes

- Reached from B2 Tempo on the empty home screen (4.1b).
- Drawn as an opaque panel over the lower tracks, with no scrim.

## 4.3 · How many bars?

### 4.3a · After a first take of 8.35 s

Budget: 105 elements · 1 bitmap waveform

### 4.3b · After a very long first take · 12 to 32 added

Budget: 100 elements · 1 bitmap waveform

#### Controls

| Control | Does |
|---|---|
| Jog turn | Chooses the number of bars. |
| Arrows | Left/Right choose too. Up/Down not used. |
| B7 Cancel | Discards the take. |
| B8 OK | Sets the Loop and the tempo. |
| B1–B6 · Jog press · Shift | Not used. |

#### Notes

- Only choices that land between 40 and 240 BPM are shown; the rest are left out, with no "unavailable" state. 40 is the floor of the stock timing engine. If none land in range, the nearest one is shown. 4.3a: 1 bar (28.7 BPM) is hidden. 4.3b: 1–3, 24, 28 and 32 bars are hidden.
- Each choice shows the tempo it gives. The suggested one (closest to 120 BPM) is preselected, and the result is repeated on the right.
- No audio is stretched; only the tempo is calculated.

## 4.4 · Part Editor · Sample Edit button

### 4.4a · Loop Mode Repeat · Fade in in focus · auditioning

Budget: 75 elements · 2 bitmap waveforms

### 4.4b · Loop Mode Play once · Offset in focus

Budget: 71 elements · 2 bitmap waveforms

With the same Offset, the start of the Loop is silent and the tail past the Loop end is cut. The caption follows Loop Mode.

#### Controls

| Control | Does |
|---|---|
| B1 Audition | Plays the Part on its own, looped. |
| B2 Normalize | Brings the peak to 0 dB. It is an action, so it can be undone. |
| B3 Undo | Reverts the last change, named at the right of the caption line. One level. |
| B4 Redo | Re-applies it. |
| B7 Cancel | Back to home without the edits. |
| B8 Done | Back to home with the edits. |
| Arrows | Left/Right choose a field. Up/Down not used. |
| Jog turn | Changes the field: 1 ms for trims, fades and Offset. |
| Shift + jog | Bigger steps: 10 ms for trims and fades, 1/16 bar for Offset. |
| Jog press | Not used. |

#### Notes

- The Loop region is between the two orange lines. Audio trimmed off is grey; trim points are thin orange lines.
- Fades show as tapering columns with a small marker where each fade ends.
- Offset has two readouts of one value: ms in 17 bold, bars in 17 light.
- The previous take is kept on the SD card for Undo.

## 4.5 · Glitch and live effects · Performance button

### 4.5 · Four tracks, four slots each · Track 2 slot 2 in focus

Budget: 149 elements

The tick under each track name is that track's playhead: it repeats a slice for Stutter and runs backwards for Reverse.

#### Controls

| Control | Does |
|---|---|
| B1–B4 | Hold to trigger slot 1 to 4 on the selected track. |
| B5 Clear | Clears the selected track's slots (label shows the track: Clear T2). |
| B6 Clear all | Clears all four tracks. |
| B7 Pitch style | Tape or Grain for the selected track; the label shows the current one. |
| B8 Back | Back to home. |
| Arrows | Left/Right move between tracks. Up/Down move between slots. |
| Jog turn | Changes the amount. |
| Jog press | Switches the slot on or off. |
| Shift + jog | Proposed: chooses the effect in the slot from the list. The brief gives no control for this. |

#### Notes

- ON is a filled tag, OFF an outlined one. The bar and the line below it show how much.
- Effects: jump back (1 or 2 slices), stutter, reverse; pitch to an interval per half-beat slice; bit-crush, overdrive, pan, low-pass, high-pass, volume.
- Everything stays in time with the Loop and never changes its length. Any track with a slot on shows GLT on home.

## 4.6 · Lo-fi master · Master button

### 4.6a · Saturation in focus

Budget: 68 elements

### 4.6b · Bypass all

Budget: 69 elements

Bypass is a full-width filled band, and every ON tag goes outlined. It switches instantly.

#### Controls

| Control | Does |
|---|---|
| B1 Bypass all | Dry mix for comparison. Press again to return. |
| B8 Back | Back to home. |
| Arrows | Up/Down move between controls. Left/Right pick Depth or Speed in Wow & flutter (proposed). |
| Jog turn | Changes the focused value. |
| Jog press | Proposed: switches the focused control on or off. |
| Shift | Not used. |

#### Notes

- Glue shows a live gain-reduction readout and meter. Master volume shows the output meter with a −6 dB tick.

## 4.7 · Input setup

### 4.7a · Line · protection standing by

Budget: 71 elements

### 4.7b · Mic high · protection acting

Budget: 73 elements

While it acts, the top bar shows FB −6 dB. It disappears when it stops.

#### Controls

| Control | Does |
|---|---|
| B1–B3 | Source: Line, Mic low gain, Mic high gain. |
| B4 Monitor | Monitoring on or off. |
| B8 Done | Back to home. |
| Jog turn | Gain. |
| Jog press · Arrows · Shift | Not used. |

#### Notes

- Gain ranges: Line 0 to +22 dB, Mic low 0 to +25 dB, Mic high 0 to +44 dB.
- Reached with Shift + B4 on home. The source and monitor state are always in the top bar.

## 4.8 · Project · File button

### 4.8a · Project list

Budget: 74 elements

The orange square after the project name is the unsaved mark. New or Open with unsaved changes asks first (4.13f).

### 4.8b · Rename · name entry

Budget: 118 elements

#### Controls

| Control | Does |
|---|---|
| B1–B5 | New, Open, Save, Save as, Rename. |
| B6 Export | Opens Export (4.10). |
| B8 Back | Back to home. |
| Arrows | Up/Down choose a project. |
| Jog turn | Scrolls the list. |
| Name entry | Jog chooses the character; Left/Right move the cursor; Up/Down jump between letters, digits and symbols; B1 Delete, B2 Space, B7 Cancel, B8 Save. |

#### Notes

- Deleting projects is not in this version.

## 4.10 · Export · from Project, B6

### 4.10a · Choose repeats

Budget: 69 elements

### 4.10b · Writing

Budget: 33 elements

### 4.10c · Done

Budget: 32 elements

#### Controls

| Control | Does |
|---|---|
| Jog turn | Chooses 1 to 16 repeats. |
| B1 Start | Starts writing. |
| B8 Cancel | Back to Project; while writing it stops the export. |
| B8 Close | On Done, back to Project (label proposed). |
| Arrows · Shift | Not used. |

## 4.12 · Memory

### 4.12 · Memory

Budget: 37 elements

#### Controls

| Control | Does |
|---|---|
| B8 Back | Back to the previous screen. |
| Everything else | Not used. |

#### Notes

- Reached with Shift + B3 on home (proposed).
- Time left is capped at the 16 s limit for one Part; the total free time is on the line below.

## 4.13 · Popups and notices

### 4.13a · Replace Part?

Budget: 106 elements · 3 bitmap waveforms · 16 waveform columns

### 4.13b · No sound

Budget: 105 elements · 3 bitmap waveforms · 16 waveform columns

### 4.13c · Memory full · first take

Budget: 106 elements · 1 bitmap waveform

It stops by itself, keeps everything, and goes straight to How many bars? with this notice.

### 4.13d · Memory full · later Part

Budget: 106 elements · 3 bitmap waveforms · 16 waveform columns

### 4.13e · SD card missing

Budget: 105 elements · 3 bitmap waveforms · 16 waveform columns

### 4.13f · Unsaved changes

Budget: 90 elements

### 4.13g · Feedback protection acting · top-bar indicator

Budget: 117 elements · 3 bitmap waveforms · 16 waveform columns

Not a popup: only the FB tag in the top bar appears.

#### Controls

| Control | Does |
|---|---|
| 4.13a | B7 Cancel, B8 Replace (red, because it starts a recording). |
| 4.13b | B7 Cancel, B8 Retry. |
| 4.13c | As 4.3. |
| 4.13d | B8 OK. |
| 4.13e | B7 Back, B8 Retry. |
| 4.13f | B6 Cancel, B7 Don't save, B8 Save. |
| 4.13g | No controls. |
| Export progress | See 4.10b. |

#### Notes

- Each popup is an opaque panel with a hairline edge and a 2 px top line. It appears with no fade, over the screen it belongs to.

## 6.1 · Chords · Note button

### 6.1a · Nothing held · the colours explained

Budget: 59 shapes (11 rect, 48 rounded, 0 circle, 0 line, 0 bitmap) · 96 texts · ≈ 5.9 KB of 8.2

Pad grid (chord map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 136, 200, 274, 330; heights 64, 64, 56, 56.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | C [home] | Dm [away] | Em [home] | F [away] | G [tension] | Am [home] | Bdim [tension] | C [home] | maj7 | add9 | 9 | flip |
| 3 | C4 | D | E | F | G | A | B | C5 | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 | D | E | F | G | A | B | C4 | Arp dn | Oct − | Oct + | Hold |

### 6.1b · Right hand holds maj7 · names change on the left

Budget: 45 shapes (11 rect, 34 rounded, 0 circle, 0 line, 0 bitmap) · 100 texts · ≈ 5.9 KB of 8.2

Pad grid (chord map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 136, 200, 274, 330; heights 64, 64, 56, 56.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | DmM7 [away, outside: C#] | EmM7 [home, outside: D#] | Fmaj7 [away] | Gmaj7 [tension, outside: F#] | AmM7 [home, outside: G#] | BdimM7 [tension, outside: A#] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | Cmaj7 [home] | DmM7 [away, outside: C#] | EmM7 [home, outside: D#] | Fmaj7 [away] | Gmaj7 [tension, outside: F#] | AmM7 [home, outside: G#] | BdimM7 [tension, outside: A#] | Cmaj7 [home] | maj7 [held] | add9 | 9 | flip |
| 3 | C4 | D | E | F | G | A | B | C5 | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 | D | E | F | G | A | B | C4 | Arp dn | Oct − | Oct + | Hold |

### 6.1c · Holding 7 and pressing G

Budget: 64 shapes (11 rect, 53 rounded, 0 circle, 0 line, 0 bitmap) · 90 texts · ≈ 5.9 KB of 8.2

Pad grid (chord map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 136, 200, 274, 330; heights 64, 64, 56, 56.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 [held] |
| 2 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension, held] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | maj7 | add9 | 9 | flip |
| 3 | C4 [dim] | D [chord tone] | E [dim] | F [chord tone] | G [chord tone] | A [dim] | B [chord tone] | C5 [dim] | Block | Strum dn | Strum up | Arp up [chosen] |
| 4 | C3 [dim] | D [chord tone] | E [dim] | F [chord tone] | G [chord tone] | A [dim] | B [chord tone] | C4 [dim] | Arp dn | Oct − | Oct + | Hold |

### 6.1d · Chord sets · B2 · saving to slot 3

Budget: 21 shapes (9 rect, 12 rounded, 0 circle, 0 line, 0 bitmap) · 56 texts · ≈ 4.9 KB of 8.2

List of 8 slots: rows 372 x 38 on a 41 px pitch from x 14, y 66. Each: slot number (14 Bold), name (14 Bold) and summary (12 Regular, dim); empty slots read Empty. The loaded set is marked in use in orange. Focus: sel fill and 2 px top line. Right, from x 414: what Save will store. B1 Save, B2 Load (asks before replacing unsaved changes), B3 Rename (opens Rename, 4.8), B6 Clear, B8 Back to Chords. A set stores key, scale, style, sound with its tweaks, voicing, octave, latched modifiers and play style. It does not store recorded Parts.

#### Controls

| Control | Does |
|---|---|
| B1 Pads | Chord pads on or off. |
| B2 Sets | Opens Chord sets (6.1d): save the chord set (key, scale, style, sound, voicing, octave, latched modifiers, play style) to one of 8 slots, or load one. |
| B3 Reset | Chord settings back to their defaults. |
| B4 Sound | Opens Sound (6.2). |
| B7 Play | Opens the Play view (6.2). |
| B8 Back | Back to home. |
| Left/Right | Choose a setting in the top line: Key, Scale, Style, Sound, Voicing, Octave. |
| Jog turn | Changes it. |
| Up/Down · Jog press · Shift | Not used. |
| Left-hand pads (cols 1–8) | Row 1: the seven chords as sevenths, then I again. Row 2: the same chords as triads. Rows 3–4: scale notes, two octaves; the C pads show their octave (C3, C4, C5). |
| Right-hand pads (cols 9–12) | Rows 1–2: modifiers, hold to apply, double-tap to latch. Rows 3–4: Block, Strum dn, Strum up, Arp up, Arp dn (tap to switch), Oct −, Oct +, and Hold, which latches the chord. |

#### Notes

- One page instead of three. The top line is a sentence of settings; the focused one has the sel fill and a 2 px top line. The line below it explains the colours on first open, then says what is sounding ("G7 is a Tension chord: it wants to go home to C").
- Chord colours are the family of each step of the key: Home (I, iii, vi), Away (ii, IV), Tension (V, vii°). The colour belongs to the chord's root; modifiers never change it. Numerals above each column carry the same colour.
- Chords: names in 17 Bold (14 Bold for 5 or more characters) with a 22 × 4 stroke in the family colour. A note outside the key replaces the stroke with its name (C#, F#). Held: a rounded fill in the family colour, name in bg.
- While a modifier is held, every chord name on the left changes to what it will play. Modifiers combine one per group: sus2/sus4, 6/7/maj7, add9/9, flip. 7 takes the seventh from the key; maj7 always adds a major seventh.
- Modifiers: names only in 17 Regular. Held: sel fill, bold. Latched: sel fill, orange bold.
- Play styles: a three- or four-note drawing of how the notes fall, and the name. Chosen: sel fill, orange drawing, bold name. Hold: an outlined pill; latched, it fills orange.
- Note rows mark the notes of the sounding chord: bold name and a stroke in its family colour; the other notes go dim.
- Octave is on two pads and in the top line. Invert and Open are the Voicing setting (Close, Open, Drop 2). While playing, Shift + jog sets the arp rate or strum speed, shown in the Play view status.
- LED brightness: held pad, held or latched modifier, chosen style 100%; in-key chords 40%; other modifiers and styles 25%.
- Estimated 5.8 to 5.9 KB per frame.

## 6.2 · Sound · Instr button

### 6.2a · Patch · Warm Pad, Bright in focus

Budget: 38 shapes (12 rect, 12 rounded, 0 circle, 8 line, 6 bitmap) · 75 texts · ≈ 5.5 KB of 8.2

Each patch tile shows its kind and its one-cycle shape. The wide picture is one note of the patch as you would hear it: press, hold, release. Attack and Release reshape it, Bright tightens it. The eight knobs are dials: a dotted arc with zero at the top, an orange arc and value once moved, a cream pointer. All dial arcs are one bitmap, all moved arcs a second. Geometry: tiles 124 × 80 on a 128 × 84 pitch from x 14, y 60; kind 12 Regular dim at +10, name 17 Bold at +25, cycle trace 64 px at +61. Right side from x 414: name 17 Bold, kind caption 12 Regular dim after it, Tweaked mark (6 × 6 orange square + 12 Regular orange) after that; blurb at y 84; note picture 372 px wide centred on y 146, ±24 px, labels at y 176, hold line at 55% of the note. Dials: 4 × 2 on a 94 × 98 pitch from y 206, radius 26, arc −225° to +45°, value 17 Bold in the centre, name 14 Regular at +60, sub 12 Regular dim at +74; focus cell 90 × 92.

### 6.2b · Patch · choosing Dream Keys, hearing it

Budget: 36 shapes (11 rect, 12 rounded, 0 circle, 8 line, 5 bitmap) · 74 texts · ≈ 5.4 KB of 8.2

Same layout as 6.2a with no knob in focus: the chosen tile gets the focus line instead, and Hear (B1) is on.

### 6.2c · Advanced · Sub osc changed

Budget: 76 shapes (64 rect, 12 rounded, 0 circle, 0 line, 0 bitmap) · 84 texts · ≈ 6.6 KB of 8.2

Grouped by section so the names can be short (Attack in two groups). Bars show where each value sits in its range; settings with no range (Oscillator, Mono) have none.

#### Controls

| Control | Does |
|---|---|
| B1 Hear | Hold to hear the patch. |
| B2 Knobs to 0 | All eight knobs back to zero. Advanced: Reset one. |
| B3 Reset patch | Patch back to how it was saved. Advanced: Reset all. |
| B7 Advanced / Patch | Switches between the two pages (Patch proposed). |
| B8 Back | Back to where Sound was opened from. |
| Left/Right | Patch: focus moves Patch, then the eight knobs. Advanced: switch column (proposed). |
| Up/Down | Advanced: choose the row. Patch: not used. |
| Jog turn | Changes the patch, knob or setting in focus. |
| Shift + jog | Advanced: finer steps. |
| Pads | Play the patch. |

#### Notes

- Patch tiles: 3 × 4 at 124 × 80 on a 128 × 84 pitch from x 14, y 60. Each: kind (12 Regular, dim) at +12,+10, name (17 Bold) at +12,+25, and a one-cycle shape as a 64 px bitmap centred on y +61, amplitude 8. All dim shapes are one bitmap in wave, the chosen one a second in text. Chosen: sel fill; focus adds the 2 px top line.
- Knobs: dials in a 4 × 2 grid from x 414, y 206, pitch 94 × 98. Each: centre at +45,+30, radius 26. Track: 3 px dots every 1° from −225° to +45° (zero at the top, −90°), all eight in one bitmap in track. Value arc: 4 px dots from −90° to the value (±10 = ±135°), all in one bitmap in marker. Pointer: a 2 px line in text from radius 17 to 29. Value (17 Bold, orange when off zero) centred at +19; name (14 Regular) at +60; meaning (12 Regular, dim) at +74. Focus: sel fill 90 × 92 at −6 with the 2 px top line.
- Patch name (17 Bold) at x 414, y 60, then the kind in 12 Regular dim. When a knob is off zero: a 6 × 6 orange square and Tweaked in 12 Regular orange; after an Advanced change, Edited. Blurb (14 Regular) at y 84. One note: a 372 px bitmap at y 146, amplitude 24, the waveform of the patch under its envelope, with a 16% see-through silhouette of the envelope behind it, a 1 px frame line at the release point, captions ONE NOTE (y 112) and press, hold, release (y 176). Orange when tweaked.
- If the sound is a pad sample, the tiles are replaced by one panel reading Sample and its name; the knobs that don't apply are dim.

## 6.3 · Home with the chord pads on

### 6.3a · Recording the chord pads, first take

Budget: 46 shapes (16 rect, 26 rounded, 3 circle, 0 line, 1 bitmap) · 46 texts · ≈ 3.7 KB of 8.2

### 6.3b · Playing after the take

Budget: 70 shapes (36 rect, 30 rounded, 1 circle, 0 line, 1 bitmap, 16 columns) · 49 texts · ≈ 4.0 KB of 8.2

The only change from round 5: the Pads tag is filled when the chord pads are on, so it reads at a glance.

#### Controls

| Control | Does |
|---|---|
| All | As 4.1. |
| Pattern | On home, switches to the Play view (6.4) and back. |

#### Notes

- Source PADS appears in the top bar and on the track header, as in the firmware today.

## 6.4 · Play view · Pattern again on home

### 6.4a · Stopped

Budget: 77 shapes (22 rect, 51 rounded, 1 circle, 0 line, 1 bitmap, 16 columns) · 89 texts · ≈ 5.7 KB of 8.2

Pad grid (chord map). Columns x: 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724; width 63. Rows y: 172, 230, 298, 350; heights 58, 58, 52, 52.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | C [home] | Dm [away] | Em [home] | F [away] | G [tension] | Am [home] | Bdim [tension] | C [home] | maj7 | add9 | 9 | flip |
| 3 | C4 | D | E | F | G | A | B | C5 | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 | D | E | F | G | A | B | C4 | Arp dn | Oct − | Oct + | Hold |

### 6.4b · Playing · Am last played

Budget: 87 shapes (23 rect, 60 rounded, 1 circle, 0 line, 1 bitmap, 16 columns) · 90 texts · ≈ 5.8 KB of 8.2

Pad grid (chord map). Columns x: 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724; width 63. Rows y: 172, 230, 298, 350; heights 58, 58, 52, 52.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | C [home] | Dm [away] | Em [home] | F [away] | G [tension] | Am [home] | Bdim [tension] | C [home] | maj7 | add9 | 9 | flip |
| 3 | C4 [chord tone] | D [dim] | E [chord tone] | F [dim] | G [dim] | A [chord tone] | B [dim] | C5 [chord tone] | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 [chord tone] | D [dim] | E [chord tone] | F [dim] | G [dim] | A [chord tone] | B [dim] | C4 [chord tone] | Arp dn | Oct − | Oct + | Hold |

### 6.4c · Armed on Track 2

Budget: 86 shapes (29 rect, 52 rounded, 3 circle, 0 line, 0 bitmap, 16 columns) · 91 texts · ≈ 6.4 KB of 8.2

Pad grid (chord map). Columns x: 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724; width 63. Rows y: 172, 230, 298, 350; heights 58, 58, 52, 52.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | C [home] | Dm [away] | Em [home] | F [away] | G [tension] | Am [home] | Bdim [tension] | C [home] | maj7 | add9 | 9 | flip |
| 3 | C4 | D | E | F | G | A | B | C5 | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 | D | E | F | G | A | B | C4 | Arp dn | Oct − | Oct + | Hold |

### 6.4d · Recording · 7 held, G pressed, Strum dn with Hold

Budget: 93 shapes (25 rect, 62 rounded, 3 circle, 0 line, 1 bitmap, 16 columns) · 90 texts · ≈ 6.5 KB of 8.2

Pad grid (chord map). Columns x: 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724; width 63. Rows y: 172, 230, 298, 350; heights 58, 58, 52, 52.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 [held] |
| 2 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension, held] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | maj7 | add9 | 9 | flip |
| 3 | C4 [dim] | D [chord tone] | E [dim] | F [chord tone] | G [chord tone] | A [dim] | B [chord tone] | C5 [dim] | Block | Strum dn [chosen] | Strum up | Arp up |
| 4 | C3 [dim] | D [chord tone] | E [dim] | F [chord tone] | G [chord tone] | A [dim] | B [chord tone] | C4 [dim] | Arp dn | Oct − | Oct + | Hold [latched] |

### 6.4e · Recording · a note pad held

Budget: 92 shapes (25 rect, 61 rounded, 3 circle, 0 line, 1 bitmap, 16 columns) · 90 texts · ≈ 6.2 KB of 8.2

Pad grid (chord map). Columns x: 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724; width 63. Rows y: 172, 230, 298, 350; heights 58, 58, 52, 52.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cmaj7 [home] | Dm7 [away] | Em7 [home] | Fmaj7 [away] | G7 [tension] | Am7 [home] | Bm7b5 [tension] | Cmaj7 [home] | sus2 | sus4 | 6 | 7 |
| 2 | C [home] | Dm [away] | Em [home] | F [away] | G [tension] | Am [home] | Bdim [tension] | C [home] | maj7 | add9 | 9 | flip |
| 3 | C4 [chord tone] | D [dim] | E [dim] | F [chord tone] | G [dim] | A [chord tone] | B [dim] | C5 [chord tone] | Block [chosen] | Strum dn | Strum up | Arp up |
| 4 | C3 [chord tone] | D [dim] | E [held, dim] | F [chord tone] | G [dim] | A [chord tone] | B [dim] | C4 [chord tone] | Arp dn | Oct − | Oct + | Hold |

#### Controls

| Control | Does |
|---|---|
| B1–B4 | Rec, Stop, Play, Monitor, as home. |
| B5–B8 | Track 1 to 4: choose the lane on top and the track Rec arms. Pressing it again does nothing here. |
| Pattern | Back to the lanes (overview or focus, whichever was last). |
| Jog turn | Scrubs, as home. |
| Arrows · Shift | As home. |
| Pads | Play the chord engine. |

#### Notes

- Layout: ruler (y 32), the selected track's lane at 80 px (y 56), the other tracks as one line of names with M (y 144) and the sounding chord at the right, then the same pad map as the Chords page: chord rows 58 px, note rows 52 px.
- Track hue is muted in the Play view (numbers and the selected edge go cream), so the only hue on the screen is the chord families. The take being recorded is a 1-bit bitmap.
- Estimated 6.3 KB with a modifier held and a take recording.

## 6.5 · Track roles · what you record decides the view

### 6.5a · Rec on an empty track: what to record

Budget: 79 shapes (38 rect, 37 rounded, 1 circle, 0 line, 1 bitmap, 16 columns) · 76 texts · ≈ 6.6 KB of 8.2

The five roles sit over B1 to B5. Bass is focused. Each card says what the pads become and the default sound.

### 6.5b · Choose the sound · Bass

Budget: 21 shapes (9 rect, 12 rounded, 0 circle, 0 line, 0 bitmap) · 34 texts · ≈ 3.3 KB of 8.2

Sounds are filtered by role. Follow is on by default when another track has chords.

### 6.5c · Recording Bass · following the chords

Budget: 69 shapes (25 rect, 38 rounded, 3 circle, 0 line, 1 bitmap, 16 columns) · 109 texts · ≈ 6.6 KB of 8.2

Notes of the chord at the playhead are bold with a stroke, the root strongest. Rows 1 to 3 are three octaves. The bottom row is the chord track itself: four pads for the bars (the current one marked now), then the next chord's root, 5th, a lead-in a semitone below it, and its 3rd. Right hand: Root, 5th, 3rd and Octave play by function, each showing the note it will play right now; Walk steps to the next root; Lead-in plays the semitone below it.

Pad grid (role map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 172, 224, 276, 338; heights 52, 52, 52, 52. Row labels at x 14: HIGH, MID, LOW, TRACK 1.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | C3 [stroke, bold] | D [dim] | E [dim] | F [stroke, bold] | G [dim] | A [stroke, bold] | B [dim] | C4 [stroke, bold] | Root [caption: F] | 5th [caption: C] | 3rd [caption: A] | Octave [caption: F] |
| 2 | C2 [stroke, bold] | D [dim] | E [dim] | F [held, bold] | G [dim] | A [stroke, bold] | B [dim] | C3 [stroke, bold] | Root − [caption: F] | 5th − [caption: C] | Walk [caption: to G] | Rest |
| 3 | C1 [stroke, bold] | D [dim] | E [dim] | F [stroke, bold] | G [dim] | A [stroke, bold] | B [dim] | C2 [stroke, bold] | Oct − | Oct + | Glide | Hold |
| 4 | C [caption: bar 1, stroke, bold] | Am [caption: bar 2, stroke, bold] | F [caption: now, stroke, bold] | G [caption: bar 4, stroke, bold] | G [caption: next, stroke] | D [caption: 5th, stroke] | F# [caption: lead-in, stroke] | B [caption: 3rd, stroke] | Lead-in [caption: E] | Ghost | Short | Accent |

### 6.5d · Recording Drums · repeat 1/16 held, Kick down

Budget: 54 shapes (25 rect, 21 rounded, 5 circle, 0 line, 1 bitmap, 16 columns) · 98 texts · ≈ 6.5 KB of 8.2

Right hand, columns 9 to 12, is the kit: Kick, Snare, Hat and Open in the far-right column, their alternates beside them, and eight percussion pads in columns 9 and 10. A dot marks pads already used in this Part. Every pad is assignable: Shift + pad opens Pad sample (C3). Left hand: rows 1 to 3 are ready-made kits by style (hip-hop, dance, live); tapping one refills the right hand and stays chosen. Bottom row: note repeat (hold a rate with a pad to roll it), swing, latch, soft velocity and undo the last hit.

Pad grid (role map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 172, 224, 276, 338; heights 52, 52, 52, 52. Row labels at x 14: HIP-HOP, DANCE, LIVE, REPEAT.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 808 [dim] | Dusty [dim] | Lo-fi [chosen, bold] | Trap [dim] | Drill [dim] | Phonk [dim] | Soul [dim] | Funk [dim] | Shaker | Tamb | Open [caption: alt, bold] | Open [bold] |
| 2 | House [dim] | Techno [dim] | Garage [dim] | DnB [dim] | Electro [dim] | Breaks [dim] | Disco [dim] | Dub [dim] | Conga | Rim | Hat [caption: alt, bold] | Hat [used dot, bold] |
| 3 | Rock [dim] | Jazz [dim] | Brush [dim] | Latin [dim] | Afro [dim] | Reggae [dim] | Live [dim] | Tape [dim] | Clap | Bell | Snare [caption: alt, bold] | Snare [used dot, bold] |
| 4 | 1/8 [dim] | 1/16 [chosen, bold] | 1/32 [dim] | 1/16T [dim] | 55% [caption: Swing] | Latch [dim] | Soft [dim] | Undo [dim] | Tom 1 | Tom 2 | Kick [caption: alt, bold] | Kick [held, bold] |

### 6.5e · Recording Melody · the safe row

Budget: 71 shapes (25 rect, 40 rounded, 3 circle, 0 line, 1 bitmap, 16 columns) · 93 texts · ≈ 6.1 KB of 8.2

Rows 1 to 3: three octaves of scale notes, chord tones bold. The bottom row changes with the chords: it only ever holds the current chord's notes, so any pad there fits.

Pad grid (role map). Columns x: 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729; width 57. Rows y: 172, 224, 276, 338; heights 52, 52, 52, 52. Row labels at x 14: HIGH, MID, LOW, SAFE.

| Row | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | C6 [stroke, bold] | D [dim] | E [dim] | F [stroke, bold] | G [dim] | A [stroke, bold] | B [dim] | C7 [stroke, bold] | Oct − | Oct + | Glide | Mono [chosen, bold] |
| 2 | C5 [stroke, bold] | D [dim] | E [dim] | F [stroke, bold] | G [dim] | A [stroke, bold] | B [dim] | C6 [stroke, bold] | Bend + | Bend − | Vib | Accent |
| 3 | C4 [stroke, bold] | D [dim] | E [dim] | F [stroke, bold] | G [dim] | A [stroke, bold] | B [dim] | C5 [stroke, bold] | Scale [chosen, bold] | Pent | Blues | Chord |
| 4 | F [stroke, bold] | A [stroke, bold] | C [stroke, bold] | F [stroke, bold] | A [held, bold] | C [stroke, bold] | F [stroke, bold] | A [stroke, bold] | Arp up | Arp dn | Repeat | Hold |

#### Controls

| Control | Does |
|---|---|
| Rec on an empty track | Opens the role choice (6.5a). B1 to B5: Chords, Bass, Melody, Drums, Audio. B8 Cancel. |
| Role chosen | Opens the sound list for that role (6.5b). Jog chooses, pads play it, B1 holds to hear, B7 back to roles, B8 arms. |
| Shift + Rec | Skips both: arms with the last role and sound. |
| Rec on a track with a role | Arms straight away, as now. |
| Track button twice | Opens the role and sound for that track, to change them. |
| Audio role | Opens Input setup (4.7) and records with the focus view (4.1d, 4.1e). |
| Chords role | Records with the Play view (6.4). |

#### Notes

- The source tag in the track header becomes the role: CHORD, BASS, LEAD, DRUM, LINE or MIC. The Pads tag at the bottom right shows what the pads are now.
- Every role view has the same frame: ruler, the track's lane at 80 px, the other tracks as one line, a status at the right, then the pad map. Left hand, columns 1 to 8, plays; right hand, columns 9 to 12, shapes. Right-hand pads are plain words in every role, as on the Chords page: held is the sel fill and bold, latched or chosen is orange or the sel fill. No outlines.
- Follow: Bass and Melody read the chord changes stored with the chord track's Part (one chord per change, a few bytes). This is the one new piece of firmware; the rest reuses the synth and the pad-sample engine.
- Chord-family colour is used only for chords: the bass and melody strokes take the colour of the chord at the playhead (here F, Away), the drum view has no hue.
- LED brightness: held 100%; chord tones of the current chord 70%; other notes 25%; right-hand pads 40%.

## C1 · Track mixer · Params button

### C1a · Level in focus on Track 2

Budget: 118 elements

Bass is muted, as on home. Track 4 has no Part yet, so its column is dim but can still be set.

### C1b · Track 1 soloed · Pan in focus

Budget: 119 elements

Solo is a filled S. Tracks silenced by a solo keep their settings, show Off by solo, and lose their meter.

#### Controls

| Control | Does |
|---|---|
| B1 Mute | Mute or unmute the selected track. Same state as Shift + B5–B8 on home. |
| B2 Solo | Solo the selected track. More than one track can be soloed. |
| B3 Reset | Puts the focused control of the selected track back to its default: 0.0 dB, Centre, Flat. |
| Shift + B3 | Proposed: resets level, pan and tone on all four tracks. |
| B8 Back | Back to home. |
| Left/Right | Move between tracks. |
| Up/Down | Move between Level, Pan and Tone. |
| Jog turn | Level 0.5 dB, Pan 5, Tone one step. |
| Shift + jog | Level 0.1 dB, Pan 1, Tone a fine step. |
| Jog press | Not used. |

#### Notes

- Four columns in home's order and names. The selected track has the light header fill and the 4 px bar on its left edge.
- Level is a vertical fader with its live meter beside it. The tick across both is 0 dB. Range −inf to +6 dB.
- Pan and Tone are horizontal bars with a centre mark. Tone runs from low-pass on the left through Flat to high-pass on the right.
- Font slots: 17 bold for names and values, 14 regular for M, S and the hint line, 12 regular for captions.

## C2 · Load samples · Sample Load button

### C2a · Browsing · previewing Kick 04

Budget: 89 elements · 289 waveform columns

### C2b · Slot occupied · Replace?

Budget: 88 elements · 289 waveform columns

Shown after B1 Load when the slot is used. Replace isn't red because nothing is recorded.

### C2c · Empty folder

Budget: 53 elements

### C2d · SD card missing

Budget: 52 elements

Same popup as 4.13e, with its own first line.

#### Controls

| Control | Does |
|---|---|
| B1 Load | Loads the file into the slot shown. If the slot is used, asks first (C2b). |
| B2 Preview | Plays the highlighted file once. Press again to stop. |
| B3 Up folder | Goes to the parent folder. |
| B4 Load next | Loads, then moves to the next file and the next slot. |
| B6 To track | Reserved, shown dim: load as a Part on the selected track (later). |
| B8 Back | Back to home. |
| Up/Down | Choose a file or folder. |
| Jog turn | Scrolls the list a page at a time. |
| Jog press | Opens a folder. On a file, same as Preview. |
| Left/Right | Destination slot −1 / +1. Shift + Left/Right: −12 / +12. |

#### Notes

- Folders have a small outlined box and their item count in the Length column.
- The right panel shows the preview (58 columns), the destination slot with what's in it now, and memory left before and after loading.
- Font slots: 14 bold for the focused file and the previewed name, 14 regular for rows, 17 bold for the slot number.

## C3 · Pad sample settings · Sample Play button

### C3a · One-shot · Tune in focus

Budget: 65 elements · 749 waveform columns

### C3b · Loop · Start in focus · auditioning

Budget: 68 elements · 749 waveform columns

In Loop mode the orange line along the top marks the loop between Start and End.

#### Controls

| Control | Does |
|---|---|
| B1 Audition | Plays the sample at its root pitch. In Loop mode it keeps going until pressed again. |
| B2 Keys | Keys mode for the whole pad set: one sample across the 48 pads at different pitches. |
| B3 Pad | Pad mode for the whole pad set: each pad plays its own slot. |
| B8 Back | Back to home. |
| Arrows | Choose a field. Left/Right along a row, Up/Down between rows. |
| Jog turn | Tune 1 st, Volume 0.5 dB, Pan 5, Play mode toggles, Start and End 10 ms. |
| Shift + jog | Tune 1 cent, Volume 0.1 dB, Pan 1, Start and End 1 ms. |
| Jog press | Not used. |

#### Notes

- Audio before Start and after End is grey; Start and End are thin orange lines, as in the Part Editor.
- Tune shows semitones in 17 bold and cents in 17 regular: one field, two readouts.
- Keys or Pad now applies to the whole pad set, as the brief says, so C3 replaces the earlier per-sample setting.

## C4 · Resample · Sample Rec button

### C4a · Ready

Budget: 63 elements

### C4b · Recording

Budget: 64 elements

Slot, Name and Back are dim while recording. Meters and the progress bar turn red, the record colour.

### C4c · Saved

Budget: 61 elements

### C4d · Memory full

Budget: 60 elements

Record is dim. The memory line and bar use the warn colour.

#### Controls

| Control | Does |
|---|---|
| B1 Record / Stop | Starts at the next loop start and stops by itself at the loop end. While recording, B1 stops early. |
| B2 Slot | Moves the focus to the slot. Jog chooses; the next empty slot is picked first. |
| B3 Name | Opens the name entry from Rename (4.8b). |
| B8 Back | Back to home. |
| Jog turn | Ready: length 1 to 4 Loops (proposed). After B2: the slot. |
| Arrows · Jog press · Shift | Not used. |

#### Notes

- Source is always Mix, taken after the lo-fi master. It is a label, not a choice.
- State shows as a tag next to the title: READY (outlined; the outlined-tag exception in the style guide), RECORDING (red, with the circle), SAVED (green), MEMORY FULL (warn).
- Memory is worked out as 16-bit stereo at 44.1 kHz, about 10.6 MB per minute.

## C5 · Device settings · Config button

### C5a · MIDI · In channel in focus

Budget: 52 elements

### C5b · Four-Track group · Wait level in focus

Budget: 52 elements

Same rows as the This device group in the earlier Settings screen, which is dropped; project settings stay in Project (4.8).

### C5c · Firmware · update file found

Budget: 49 elements

### C5d · Firmware update in progress

Budget: 36 elements

Every button is blank and nothing responds until the restart.

#### Controls

| Control | Does |
|---|---|
| Up/Down | Choose a group, or a row in it. |
| Left/Right | Move between the group list and its rows. |
| Jog turn | Changes the focused row. |
| Shift + jog | Larger steps where a row has them (volume, brightness). |
| B1 Update | Firmware group only: installs the update file. |
| B8 Back | Back to home. |
| Jog press | Not used. |

#### Notes

- Groups: MIDI (in and out routing, channels), USB audio (on or off), Line input (Stereo, Mono left, Mono right), Volume (headphone, master), Display (brightness), Four-Track, Firmware, About (build id).
- The chosen group keeps its fill; the 4 px bar shows whether the focus is on the list or on the rows.
- The help line under the rows names the focused row in 17 bold and explains it in 14 regular.

## C6 · Reverb and delay sends · FX1 button

### C6a · Delay Time in focus

Budget: 94 elements

### C6b · Delay off · Reverb send for Track 2 in focus

Budget: 93 elements

When an effect is off, its values and sends stay as set and are drawn dim.

#### Controls

| Control | Does |
|---|---|
| B1 Reverb | Reverb on or off. |
| B2 Delay | Delay on or off. |
| B8 Back | Back to home. |
| Left/Right | Move between the Reverb and Delay blocks. |
| Up/Down | Move through the effect settings, then the four sends. |
| Jog turn | Changes the focused value: 5% steps; Time steps 1/32 to 1/2, with dotted and triplet values. |
| Shift + jog | 1% steps. |
| Jog press | Not used. |

#### Notes

- Each block has its settings on top and a send bar per track below it, in home's order.
- Sends follow each track's level and mute (post-fader), as stock Sends mode Post-fader did.
- Delay Time shows the note value in 17 bold and the ms it works out to at the current tempo in 17 regular.

## C7 · Live filter · FX2 button

### C7a · LP–HP sweep · holding

Budget: 61 elements

### C7b · Band sweep · latched

Budget: 60 elements

In Band mode the lit block is the band. It moves with the jog.

### C7c · Released · open

Budget: 59 elements

Now reads Open. The Set position stays as a marker over a dim bar.

### C7d · Home while the filter is latched

Budget: 94 elements · 1797 waveform columns

The FILTER tag stays in the top bar on every screen until the filter opens.

#### Controls

| Control | Does |
|---|---|
| B1 Hold | Filter acts while held. Let go and it opens again. |
| B2 Latch | Filter stays in until B2 again or B3. |
| B3 Reset | Opens the filter and puts Set back to the middle. |
| B4–B7 | Sweep: LP–HP, LP, HP, Band. |
| B8 Back | Back to home. A latched filter stays in. |
| Jog turn | Moves Set, whether held or not. |
| Shift + jog | Fine steps. |
| Arrows · Jog press | Not used. |

#### Notes

- LP–HP is the sweep in the brief's purpose: left of the centre closes a low-pass, right of it raises a high-pass. LP, HP and Band are the choices it lists.
- One big bar with a marker for Set. Now and Set are both read out, so you can see where it will land before you hold.
- While latched, a filled FILTER tag sits next to MON in the top bar on every screen (C7b, C7d).

## C8 · Copy a Part · Copy button

### C8a · Copy Track 2 to an empty track

Budget: 108 elements · 1797 waveform columns

### C8b · Target already has a Part

Budget: 109 elements · 1797 waveform columns

#### Controls

| Control | Does |
|---|---|
| B1–B4 | Choose the target track. The source is dim and can't be chosen. |
| Left/Right · Jog | Choose the target too. |
| B7 Cancel | Back to home. |
| B8 Copy | Copies at once and returns to home. |
| Shift + Copy | Proposed: undoes the last copy or clear. |

#### Notes

- The panel sits over home like Replace Part (4.13a). The next empty track is chosen first.
- The warning is a filled REPLACES tag in the warn colour.

## C9 · Clear · Delete button

### C9a · Clear Track 3?

Budget: 89 elements · 1797 waveform columns

### C9b · Shift + Delete · Clear all tracks?

Budget: 89 elements · 1797 waveform columns

### C9c · After clearing

Budget: 91 elements · 1198 waveform columns

The notice stays in the hint line until the next action.

#### Controls

| Control | Does |
|---|---|
| B7 Cancel | Back to home with nothing changed. |
| B8 Clear | Clears and returns to home. Red, as the brief asks. |
| Shift + Delete | Opens Clear all tracks? |
| Shift + Copy | Proposed: brings back the last clear. One level. |
| Everything else | Not used. |

#### Notes

- Same popup as Replace Part. The first line names the Part so you can see what goes.
- Clearing every Part keeps the Loop and tempo, so the next take still lands on the grid. Clearing those is New project (File).
