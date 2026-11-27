# STYLEGUIDE · Four-Track Recorder, round 6

Exported 2026-11-27 from `Four-Track Recorder.dc.html`, the source of truth. Frame 0 in SCREENS.md shows every component in its states. One drawing engine applies these rules to every screen (4.x, 6.x, C1–C9).

## Display and budget
- 800 × 480. Eight buttons under the screen: 97 px cells on a 100 px pitch, y 440.
- Bands: top bar y 0–32, content y 32–400, hint line y 400–440 (1 px frame line at 400, text at y 411), buttons y 440–480.
- 8,172 bytes of display commands per frame; aim under 7 KB. Costs: rectangle or rounded rectangle 20, line 20, circle 24, colour change 4, transparency change 4, text 16 + 4 to 8 per character, bitmap 40. Every frame in SCREENS.md lists its estimate.

## Shape
- One corner radius, 6 px, on everything at least 18 × 18 px: cards, fields, button cells, tags, M box, pad fills, lane cards, popups.
- Square: full-width bands (top bar, ruler, hint line, list rows, the strip behind the buttons) and anything under 18 px.
- Circles: Rec mark and lamps only. Lines: hairlines, bar lines, the playhead, markers, dial pointers.
- Outlines only for: outlined tags (READY, source tags), M off, Hold off, popups. Pads are never outlined.

## Fills, focus and choice
- A fill means pressed, chosen or focused. Content otherwise sits on the ground as type.
- Focus (where the jog acts): sel fill plus a 2 px text line along the top, inset 8 px from rounded corners. Same on cards, list rows, sentence words, dials and patch tiles.
- Chosen (current option in a set): sel fill and a bold label, no line.
- Held pad: solid fill, label in bg. Chords use their family colour, notes and drum pads use text.
- Held modifier or right-hand pad: sel fill, bold label. Latched: sel fill, orange bold label. Hold latched: orange fill, label in bg.
- Buttons: cell fill; on = sel fill and a short cream pill at the top.

## Colour roles
- Theme keys: bg, top, topText, frame, text, dim, cell, sel, panel, rec, meter, track, wave, waveBg, waveOff, marker, warn, t1–t4.
- Tracks: hue t1–t4 on two things only, the track number and a 3 px edge inside the header when selected. The selected track also has the sel header and a text-colour waveform; others use wave. The Play view and role views mute track hue (numbers dim, edge in text).
- Chord families, on chord pads and their strokes only: Home (I, iii, vi) t1, Away (ii, IV) t2, Tension (V, vii°) t3. The family belongs to the chord root; modifiers never change it. Bass and Melody strokes take the family of the chord at the playhead.
- rec red: recording only. marker orange: a changed value, Tweaked, latched, in use, Loop region and markers; never the tonic. meter green: levels and ON. warn yellow: memory, feedback, unsaved.
- Monochrome theme: t1, t2, t3 become white, light grey and mid grey so families still separate; every state also reads by fill against no fill and bold against regular.

## Drawing
- Levels: green bars, never dials or needles.
- Amounts (sends, depth, gain): 3 px bars, text colour when on, waveOff when off, over a track-colour rail.
- Waveforms: 1-bit bitmaps, one column per pixel, one command per Part, including the take being recorded and the Part Editor. Ahead of the playhead the lane is darkened by one see-through rectangle (0 6px 6px 0 radius); a muted lane by a second.
- Dials (Sound knobs): radius 26; track of 3 px dots every 1° from −225° to +45°, zero at the top; value arc of 4 px dots in marker from zero to the value (±10 = ±135°); 2 px text pointer. All tracks on a screen share one bitmap, all value arcs a second.
- Patch shapes and the one-note picture: bitmaps; a 16% see-through envelope silhouette behind the note.

## Type (five device slots)
- B17: titles, values, chord names up to 4 characters, track names, dial values.
- L17: note names, modifier names, unselected list items.
- B14: longer chord names, chosen or held labels, kit and slot names.
- R14: buttons, hint line, list text, labels, right-hand pads.
- R12: captions, tags, units, ruler numbers, row labels in the pad gutter.
- No arrows or symbols beyond − · ° … ; names are written out (Strum dn, Arp up, Bend +).

## Pad maps (6.1, 6.4, 6.5)
- 4 × 12, as on the device. Columns 1–8 left hand, 9–12 right hand, a 22 px gap and a 1 px frame line between them; a 10 px gap and a frame line between rows 2 and 3 on the left. Exact x, y and sizes per frame are in SCREENS.md.
- Labels centred; a 22 × 4 stroke under a label marks family or chord tone; a note outside the key replaces the stroke with its name (C#, F#). A 5 px dot top-right marks a drum pad already used in the Part.
- Chords: row 1 sevenths I–vii and I, row 2 triads, rows 3–4 scale notes (C pads show C3, C4, C5). Right: sus2 sus4 6 7 / maj7 add9 9 flip / Block Strum dn Strum up Arp up / Arp dn Oct − Oct + Hold. While a modifier is held every chord name shows what it will play.
- Bass: three octaves of scale notes (chord tones bold with a stroke, root strongest); row 4 is the chord track (four bars, then next root, 5th, lead-in, 3rd). Right: Root 5th 3rd Octave / Root − 5th − Walk Rest / Oct − Oct + Glide Hold / Lead-in Ghost Short Accent, each captioned with the note it plays now.
- Melody: three octaves of scale notes, chord tones bold; row 4 the safe row (only the current chord's notes, stroked). Right: Oct − Oct + Glide Mono / Bend + Bend − Vib Accent / Scale Pent Blues Chord / Arp up Arp dn Repeat Hold.
- Drums: left rows 1–3 ready-made kits by style (chosen kit has the sel fill), row 4 repeat and feel (1/8 1/16 1/32 1/16T Swing Latch Soft Undo). Right is the kit: far-right column Open, Hat, Snare, Kick; column 11 their alternates (captioned alt); columns 9–10 percussion. Shift + pad assigns a sample.
- LED brightness: held, latched or chosen 100%; chord tones of the current chord 70%; in-key chords 40%; other modifiers, styles and right-hand pads 25–40%; free pads off.

## Words
- Track, Part, Loop, Loop Mode, Offset, Chord set, Role (Chords, Bass, Melody, Drums, Audio). Hint line: one sentence. Recording limit: 16 s per Part.
