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
9. **Later Parts: a count-in?** On-screen countdown for the last 4 beats before a later Part starts, no extra clicks (option 2).
10. **Changing the tempo or bars after the first take:** allowed while every Part is a pad Part; locked once an audio Part exists, with a message saying why (option 2).
Q10 (later-Part recording model) is being reshaped: the owner asked for a looper-pedal overdub where older layers fade over time (feedback). Feasibility being checked (memory for a scratch pass buffer).

### Loop mechanics, third round (owner, 2026-11-27)
11. **Later Parts: Layer (overdub) with a Fade.** Each pass adds to the Part; older layers fade by the Fade amount (100% = plain overdub, down to about 40%) and only on passes you record over. Pad Parts: each note remembers its pass and is rendered at Fade^age. Audio Parts: the new pass is mixed in with the old audio times Fade (needs a spare Part-sized buffer; memory being checked). A way to switch to Replace for redoing a take; Undo covers the last pass. Always-fading ("decay") may come later as an opt-in.
12. **Fade control:** a Config default, plus a quick control while recording (Shift + Jog while a take is armed or recording, value shown on screen).

### Design exports received (2026-11-27)
`docs/design/round-6-export/SCREENS.md` and `STYLEGUIDE.md` (exported from the HTML, now complete: roles 6.5, Play view 6.4, Chord sets 6.1d). Build from these. Known conflicts with the decisions above that the design must absorb (round 7 brief): the first-take flow (4.2, 4.3) assumes the old Free/How many bars flow; new frames needed for Bars in the Tempo panel, the later-Part countdown, Layer and Fade, Rendering, and the Config options (quantise grid and strength, tail wrap, calibrate latency).

### Overdub with Fade on audio Parts: memory check (verified in code, 2026-11-27)
- The four track regions are fixed slices of the sample memory array (each 1,588,248 B: 16 s plus 2 s spare, 16-bit mono). External RAM is 16 MB with 1,768,016 B free (from the FT46 build figure).
- A scratch pass buffer of one region (1,588,248 B) fits and leaves about 180 KB; a 16 s buffer (1,411,776 B) leaves about 356 KB. Declare it in external RAM; the linker fails loudly if it does not fit. The alternative that frees RAM is lowering the 5.3 MB reserved for loaded samples.
- Least risky method: record the new pass into the scratch buffer, then mix `old * fade + new` into the track's region in the main loop after the pass (about 700k samples, tens of ms, estimated), keeping the alignment and offset. Today a new pass overwrites the old one in place as it records, so this needs the change.
- One-step undo already exists: before a new take the old Part is saved to `undo/tN.raw` on the SD card; an overdub through the same path is undone by the existing undo (last pass only).
- Pad (note) Parts need no scratch buffer: notes carry their pass number and are re-rendered.
- Cost to remember: this uses nearly all remaining external RAM.

### Round 6, questions 14 to 19 (owner, 2026-11-27)
13. **Fade control:** Config default plus a quick control while recording (Shift + Jog when armed or recording).
14. **Style = a feel preset** (sets Voicing, Humanise, strum speed, arp rate and range, and whether the top row defaults to 7ths or 9ths). A Set saves the result.
15. **Chord Sets: both.** 8 device-wide slots on the SD card, and each project remembers its current chord set.
16. **Drums: the owner asks whether we can use Plaits or another Mutable Instruments engine** instead of writing our own drum synth. Plaits (MIT, already vendored in part) has analog and synthetic bass drum, snare and hi-hat models; Peaks (MIT) has 808-style drums. Toms from the kick model; clap needs a small noise burst of our own. To be done with the roles, after the restyle and chord work.
17. **Rec on an empty track arms straight away with the last role and sound** (a new project starts with Track 1 as Chords and Warm Pad); change role by pressing the track button twice.
18. **Adopt the design's home controls** (jog scrubs, Left and Right step a beat, Up and Down choose the track) with my level mapping: Shift + Up and Down is the selected track's level, Shift + Left and Right the synth level; output volume moves to Config and Params; Undo take moves to Shift + Delete; Settings joins Project.
19. **Live filter: whole mix** (two stock filters on the final mix).
Also: the owner asked for a firmware with the latency calibration test now (FT47).
