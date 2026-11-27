# Round 6 build decisions (owner, 2026-11-27)

The owner added a full new design (`docs/design/Four-Track Recorder Design.zip`: new drawing language, new chord and sound screens, Play view, track roles, FX sends, live filter, boot logo). Decisions so far, from the question round:

1. **Order of work: yes to the recommendation.** A restyle (rounded shapes in the firmware and the emulator, every screen in the round-6 language), then B chords, Sound and Play view, then D (FX sends C6, live filter C7, boot logo, Sets), then C track roles (Bass, Melody, Drums, Audio).
2. **Device test before building: skip.** No FT47 shape-test build for now; FT46 stays untested on the device. Risks accepted: shape rendering on the chip and the synth's CPU load are unverified, so keep both measurable later (CPU and audio-block readout still wanted).
3. **Chord layout: adopt the design's fixed layout.** Left hand columns 1 to 8 (7ths row, triads row, two scale-note rows), right hand columns 9 to 12 (hold modifiers, play styles, Oct -/+, Hold), one config page (Key, Scale, Style, Sound, Voicing, Octave). This **drops** per-row chord types, the three colour chords, Shuffle, the row count and the three config pages. Colour chords may return later as a Set option.
4. **The owner will add the re-exported `SCREENS.md` and `STYLEGUIDE.md`** (the zip's versions lag the HTML: no track roles, Play view numbered differently). Build from those once they arrive.

Open (next rounds): what "Style" does in the new top line; what a Set contains and where Sets are stored; whether track roles need a chord map stored per Part; the filter's scope (loops only or the whole mix); whether sends are before or after each track's level. Boot logo: researched, small and safe (see the feasibility notes in the session; a 200x120 L8 image fits the existing slot).

Ideas to take from LarkOS: `docs/research/lark-os-comparison.md`.

## Loop mechanics (owner, 2026-11-27)
Goal: perfect loops, insanely easy to record.
1. **First take: tempo and bars set up front, one Rec press, a count-in, then it records and stops itself exactly at the bar line (option A).** Free (hand-stopped, loop = what was played) stays for free playing.
2. **Tail wrap: yes, as a Config option.** A note's tail past the loop end is folded onto the start of the loop (record a little extra, mix it into the beginning, a few ms of fade). Default on, about 2 s of tail, switchable in Config.
3. **Live playing is quantised (option 2):** note starts snap to a grid with a strength. This means pad parts are stored as recorded notes (an event list), not only audio. Architecture still to settle.
Still open: the latency cable test (Calibrate latency button), and what has gone wrong in the owner's loops so far.

### Loop mechanics, second round (owner, 2026-11-27)
4. **Pad Parts: notes are the source, audio is a cache (option 3).** Recording a pad Part stores the notes; on stop they are rendered (quantised) into the Part's audio, which plays like every other track. Changing quantise, patch or key re-renders. A short "Rendering" step after each pad take.
5. **Quantise: global setting in Config, default 1/16 at 100%** (grid Off, 1/8, 1/16, 1/32; strength 0 to 100). Humanise is applied after quantising.
6. **New projects default to Set 80 BPM, 4 bars, a 1-bar count-in** (4 bars at 80 BPM is 12 s, inside the 16 s Part limit). The Tempo panel gets a Bars field (only lengths that fit 16 s at that tempo). Free stays one button away.
7. **Note editing afterwards: quantise only for now.** Change the sound and transpose come with the track roles. No piano roll.
8. Owner's pain with loops: "awkward recording and getting the timing right" (start and stop timing, aligning later Parts).
Latency cable test: owner asked what it needs (see the session notes); a Calibrate latency button in Config is planned.
