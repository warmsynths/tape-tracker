# Round 6 build decisions (owner, 2026-11-27)

The owner added a full new design (`docs/design/Four-Track Recorder Design.zip`: new drawing language, new chord and sound screens, Play view, track roles, FX sends, live filter, boot logo). Decisions so far, from the question round:

1. **Order of work: yes to the recommendation.** A restyle (rounded shapes in the firmware and the emulator, every screen in the round-6 language), then B chords, Sound and Play view, then D (FX sends C6, live filter C7, boot logo, Sets), then C track roles (Bass, Melody, Drums, Audio).
2. **Device test before building: skip.** No FT47 shape-test build for now; FT46 stays untested on the device. Risks accepted: shape rendering on the chip and the synth's CPU load are unverified, so keep both measurable later (CPU and audio-block readout still wanted).
3. **Chord layout: adopt the design's fixed layout.** Left hand columns 1 to 8 (7ths row, triads row, two scale-note rows), right hand columns 9 to 12 (hold modifiers, play styles, Oct -/+, Hold), one config page (Key, Scale, Style, Sound, Voicing, Octave). This **drops** per-row chord types, the three colour chords, Shuffle, the row count and the three config pages. Colour chords may return later as a Set option.
4. **The owner will add the re-exported `SCREENS.md` and `STYLEGUIDE.md`** (the zip's versions lag the HTML: no track roles, Play view numbered differently). Build from those once they arrive.

Open (next rounds): what "Style" does in the new top line; what a Set contains and where Sets are stored; whether track roles need a chord map stored per Part; the filter's scope (loops only or the whole mix); whether sends are before or after each track's level. Boot logo: researched, small and safe (see the feasibility notes in the session; a 200x120 L8 image fits the existing slot).

Ideas to take from LarkOS: `docs/research/lark-os-comparison.md`.
