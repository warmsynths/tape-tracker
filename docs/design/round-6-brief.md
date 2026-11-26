# Round 6 brief for the design tool: the chord and synth screens

Paste everything below the line, and attach the pictures in `docs/design/round-6/` (they are screenshots of the real firmware code running in the web emulator, so what you see is what the Tracker draws). Same rules and the same look as rounds 4 and 5: same top bar, same 97 px button cells, same card shapes, same colour roles.

---

## 1. What this is
The Four-Track recorder now has a chord engine and a synth. The screens for them work, but I (the firmware side) laid them out quickly with boxes and text. Please design them properly: better hierarchy, better use of the five fonts and the colour roles, and clearer reading of the pad layout. The firmware matches your frames closely when you give them, as it did in rounds 4 and 5.

How it is used: the 48 pads become chord pads and scale-note pads, with a synth (or a pad sample) as the sound. You play chords live and record them into a track with Rec (the track's source becomes Pads), then record a melody on the next track. So the screens are used mid-performance: glanceable, big, calm.

## 2. Limits (same as rounds 4 and 5, plus what we learned)
- 800 x 480. Eight buttons under the screen, 97 px cells on a 100 px pitch, about 9 to 10 characters per label.
- Five font slots: 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular. No icons except shapes you draw.
- **Drawing budget: 8,172 bytes of display commands per screen.** Costs (measured by the firmware's cost model): a rectangle 20, a colour change 4, a transparency change 4, text 16 + 4 to 8 per character, a bar 12 + 8 per column, a bitmap mask 40, a line about 12. Measured today: Chords page 1 = 5.9 KB, page 2 = 6.3 KB, page 3 = 6.0 KB, Sound = 4.8 KB, Sound every setting = 6.2 KB. **So there is about 2 KB left on each, and fewer fills and labels would buy more room.** Keep each screen under about 7 KB.
- Parts of a rectangle can be see-through (a transparency change costs 4 bytes), which is how the track tints are done on the home screen. Waveforms are one-bit pictures, drawn once per Part.
- No fades, slides, scrolling or zoom. A highlight that moves, a number that changes and a meter are fine. A pad press redraws the page, so keep what changes on a press small.
- Use the colour roles by name so a theme can recolour them: bg, top, topText, frame, text, dim, cell, sel, panel, rec, meter, track, wave, waveBg, waveOff, marker, warn, and the four track colours t[0..3]. It should still read in a monochrome theme.
- The pad LEDs on the Tracker have brightness only, no colour. The screen is where colour lives.

## 3. Screens to design
All are in `docs/design/round-6/`. For each, please give a frame per state, what each of the eight buttons, the jog, the arrows and Shift do, and the colour roles and font slots used, as before.

### 3.1 Chords (Note button), three pages
Pictures: `chords-1-harmony.png`, `chords-1-pad-held.png`, `chords-1-four-chord-rows.png`, `chords-1-note-pads-chord-mode.png`, `chords-2-types.png`, `chords-3-feel.png`, `chords-3-arp-pad-held.png`.

The pads are 12 columns by 4 rows, drawn on screen exactly as they sit on the device (pad 1 is the top left). They are in regions:
- **Left ten columns, top rows: chords.** Each row is one chord *type* (Triad, 7th, 9th, Sus4 ...), chosen on page 2. Columns 1 to 7 are the scale's seven chords; columns 8 to 10 are three "colour" chords borrowed from outside the key.
- **Left ten columns, other rows: single notes**, the scale rising left to right and upward. Three modes: the scale, the notes of the last chord, or the scale with the last chord's notes marked.
- **Right two columns, all rows: performance pads** (Oct +, Oct -, Invert, Open, Sus, 7th: held while playing a chord; Hold: latches; the last pad is the play-style pad and shows the current style, e.g. Block, Strum D, Arp Up).
What a pad can be (tier): tonic (the key's home chord or note), a chord in the key, a chord with notes outside the key (shown dimmer), a colour chord, a chord tone of the last chord, a performance pad, held right now. Today these are told apart by a fill and a 3 px bar. **Please make the tiers readable at arm's length and make "outside the key" and "held" obvious.** Long chord names (Dbmaj9, Bm7b5, G7sus4, C6/9) drop to the 12 font today; please decide how to handle them.

Page 1, harmony, seven fields in one row: Key, Scale (9 scales), Style (Lo-fi, Pop, Jazz-ish, Synthwave, Soul), Rows (how many are chord rows), Note pads (Scale, Chord, Follow), Octave, Sound (patch name, with a star if tweaked, or "Sample"). Buttons: B1 chord pads on or off, B2 Shuffle (re-rolls the colour chords), B3 Reset, B4 Sound (opens 3.2), B7 next page, B8 Back.
Page 2, chord types: the type of each row (four fields), and the root and kind of each colour chord (six fields). A row that is not a chord row is shown "off".
Page 3, feel: Play style, Humanise, Strum speed, Arp rate, Arp range, Arp gate, Velocity, Synth level.
Left and Right choose a field, the jog changes it. Three pages is a lot: would tabs, a page dot row or a different split read better?

### 3.2 Sound (Instr button) and every setting
Pictures: `sound.png`, `sound-advanced.png`.
A patch list on the left (12 patches, each with a kind and a one-line blurb), eight tweak knobs on the right (Bright, Reson, Attack, Release, Thick, Chorus, Drive, Motion, each -10 to +10 around zero), a tag saying "tweaked" or "edited". Hold B1 to hear the patch. B2 knobs to zero, B3 reset patch, B7 Advanced (a list of all 28 raw settings in two columns with a bar each, changed ones in orange), B8 back. Please design what a "patch" looks like (it should feel like choosing an instrument, not a list of parameters), the knob display, and the Advanced list.

### 3.3 Home screen, with the chord pads on
Pictures: `home-recording-chord-pads.png`, `home-after-take.png`. The top bar and lanes are unchanged. New: the bottom right shows "Pads CHORD" when the chord pads are on. See section 4 for the idea we want to explore.

## 4. New idea: a Play view on the home screen
Question for you, and a feasibility note from the firmware side.

The idea: a second home layout, "Play view", so you can see what you can trigger while you record. The selected track's lane stays at the top (recording or playing, with the playhead and the red recording waveform), and the rest of the screen shows the pad map: the chord names as laid out on the pads, the key, scale and style, the current patch, the Hold and style pads, which pad is held, and which pad was last played.

Feasibility (my read): yes. The pad grid and the lane drawing already exist. Estimated cost: top bar and one lane about 1.2 KB (the lane is a bitmap), the 48-pad map about 3 KB, the key and patch strip about 0.6 KB, the buttons and hint 0.5 KB: about 5.5 KB, under the 8,172 limit. A pad press redraws only the map. It would be one of the existing home layouts (the overview with four lanes, the focus layout with one lane large), toggled by pressing Pattern again on the home screen.

What I need from you:
- A frame for the Play view: stopped, playing, armed, recording and a pad held.
- What stays visible of the other three tracks (a thin strip each, only their colour and a mute mark?), since you may still want to see the whole project.
- Whether the pad map in the Play view should be the same drawing as on the Chords page, or simpler and bigger, since this is the performance screen.
- What the eight buttons are in this view (today: Rec, Stop or Tempo, Play, Monitor, Track 1 to 4).

## 5. Things that cannot change
- The pad layout: 12 x 4, regions as above. You can propose region changes, but say what each would cost.
- What each button does on the existing screens. Propose changes only if you list what moves where.
- The jog wheel changes the value in focus; Left and Right choose.
- Words: Track, Part, Loop, Loop Mode, Offset (see CONTEXT.md). Chord names as in the pictures.

## 6. What to send back
Frames at 800 x 480 for each state, a one-line note for each button, the colour role and font slot of every element, and a count of elements per frame (so the firmware side can check the byte budget). If something you want does not fit, say what you would cut first.

## 7. How to look at the real thing
The emulator is `prototype/emulator/dist/four-track-emulator.html` (open it in a browser, click once to start). Keys: N Chords, T Sound, 1 to 8 the eight buttons, [ and ] the jog, arrows, R Rec, S Stop, Space Play, H home, and click the pads. The Emulator settings overlay lists every device shortcut. The screenshots in `round-6/` were taken from it.
