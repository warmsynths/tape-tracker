# Plan: a synth and a chord engine on the Original Tracker

Drafted 2026-11-26 after the stock screens were removed (FT43: flash 529 KB used, about 519 KB free; internal RAM about 105 KB free). Everything here is an estimate until measured on the device.

## Goal (owner)
- A synth that does a good Juno-style sound and vintage leads. It does not have to be Plaits.
- A chord engine, small: set up sets of pleasing chords on the pads so they can be played and recorded from the main screen. Not a port of the whole chroma-chords app.

## What the machine allows
- MK66, 180 MHz Cortex-M4F with a single-precision FPU. One audio block is 128 samples every 2.9 ms, so about 4,080 cycles per sample for everything (docs/research/cpu-ram-budget.md).
- Audio already uses an estimated 35 to 60% of that with four looping Parts, delay, reverb and the limiter. Not measured yet. The synth has to fit in what is left.
- There is no stock synth on the Original: `FEATURE_SYNTH` is only defined for the i.MX models. The synth is new code.
- Pad sounds come from 8 stock sample voices. Recording from the pads (Source: Pads) records the pad bus, so a synth that feeds the pad bus is recorded for free.
- Flash is no longer the constraint (519 KB free). CPU is.

## Synth
Portable C++ in `core/ft/dsp/Synth.*` like the rest of the shared core, so the emulator runs the same code and it can be heard before it goes on the device.

Voice (Juno-106 style):
- One DCO: saw and pulse (with PWM), plus a sub oscillator and noise. Band-limited (polyBLEP or a saw table).
- 24 dB per octave resonant low-pass, a simple high-pass, one ADSR for the amp and one for the filter, one LFO (vibrato, PWM, filter).
- The signature: a stereo chorus (modes I, II, I+II), two modulated delay lines. This does most of the Juno sound and costs little.
- Vintage leads: mono mode with legato and glide, optional unison. Later.
- About 16 presets in about 1 KB: Juno pad, strings, brass, bass, vintage lead, and so on.

Cost (my estimate): about 200 cycles per voice per sample, so 4 voices about 20% of the budget, 6 voices about 30%, chorus about 1.5%. That is too much on top of 35 to 60% unless it is made cheaper: envelopes and the LFO at control rate (every 16 samples), a cheaper filter, 4 voices to start. Flash about 15 to 25 KB. Plan for 4 voices and measure before promising 6.

## Chord engine
Port the data and the small logic from `warmsynths/chroma-chords` (`src/services/chord-engine.ts`, `taxonomy.json`), not the app.

Port:
- Theory: 12 roots, about 12 chord qualities (maj, min, 7, maj7, m7, m7b5, dim, aug, sus2, sus4, 6, add9, 9), scales and modes, the chords of each degree, and voicings (close, open, spread, inversions). From `buildVoicingNotes`, `SCALE_DEGREE_*`, `QUALITY_INTERVALS`.
- Chord sets: a small number of ready-made sets ("Lo-fi", "Pop", "Jazz-ish", "Synthwave", "Soul") made from the genre and mood rules, each giving the chords for the 12 pad columns.
- Shuffle: the genre and mood weights (from `GENRES`, `getMarkovTransitionWeight`) boiled down to a small table, used to roll the 5 colour chords and the extensions of a set. No next-chord suggestion: the whole set is on the pads, so it has little to add (owner, 2026-11-26).

Leave out: the vibe prompt and AI classifier, the melody engine, Band DNA, the song arranger, storage, sign-in and sync, exports. Estimated flash 8 to 15 KB, no CPU worth counting.

Check against the original: port its chord-engine tests as host tests (same chord in, same notes out).

Licence: chroma-chords and circuit-chords are GPL v3. This repo has its own licence file. If the code is all the owner's own, they can license it into this repo; do not copy third-party parts.

## The pads (MVP)
Idea from the owner (2026-11-26), like the minichord: chords and scale notes together. Two rows of chords (triads, then 7ths, 9ths and suspended) and two rows of scale notes (24 notes, about three and a half octaves of the chosen scale, every pad in key). A setting chooses the split (all chords, 3 and 1, 2 and 2, all notes). The note rows have three modes: Scale (fixed to the key, the default), Chord (the notes of the chord last played, across octaves), Follow (the scale with the current chord's tones marked, if the pad lights allow). In Keys mode any sample becomes a scale-locked instrument, which works before the synth exists. Up to 6 voices at once (a chord of 3 to 4 notes plus melody).

Original layout, before that idea:
The Note button opens Chords (design C10). The 48 pads (12 columns by 4 rows) become chord pads for a chosen key, scale and set:
- 12 columns: the 7 degrees of the scale plus 5 colour chords (borrowed and secondary), in the order of the chosen set.
- 4 rows: triads, 7ths, 9ths and suspended, open voicings. (Layout to confirm with the owner.)
- Fixed layout: a pad is always the same degree for a key and scale, so playing by muscle memory works. The 7 degree columns never move. Styles (Lo-fi, Pop, Jazz-ish, Synthwave, Soul) change the chord qualities and the 5 colour chords. A Shuffle re-rolls only the colour chords and extensions, with a lock.
- Pad lights: the tonic marked (needs a check of what the pad lights can show: the code has a front and a back layer with a brightness, not obviously colours).
- A chord plays through the chosen sound: a sample on a pad slot (3 to 4 of the 8 voices, pitched) or the synth. Chords are recorded from the home screen with Source: Pads, like any pad sound.

## Order of work
1. **Chords with samples (no synth yet).** Theory core and styles in `core/ft`, host tests, the Chords page, chord playback through the existing sample voices, recording from the home screen. Delivers the MVP.
2. **Styles, Shuffle and pad lights.** After the pad-light check. Later, if wanted: a progression starter that plays a short progression in time while you record.
3. **CPU measurement on the device.** Build a readout of audio load into Device settings (About) using the stock profiler (needs an empty `/.develop` file on the SD card). Four Parts playing with delay and reverb on, then a 4-voice test synth. This decides the voice count.
4. **Synth spike:** 4 voices, saw and pulse, filter, ADSR, in the emulator and on the device, feeding the pad bus.
5. **Juno voice:** sub, high-pass, LFO and PWM, chorus, presets, and the Sound screen (Instr button, design C11).
6. **Vintage leads:** mono, legato, glide, unison. Then chords through the synth, strum and arpeggio if wanted.

## Open questions
- Pad layout: functional rows (triads, 7ths, 9ths, open) as above, or one chord set per row?
- Which sets matter most to the owner.
- Whether the synth should be playable from the pads in note mode (Keys) as well as chords.
- The Song button: it switches the click on and off, which is an odd place for it. Proposal: the click moves to Shift + Play (the CLICK tag in the top bar stays), and Song stays free.
