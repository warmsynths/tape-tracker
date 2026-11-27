# LarkOS harmony engine, compared with ours (for later)

Source: https://lark-tracker.github.io/lark-os (read 2026-11-27; the page's published chord palette for C major and A minor was compared with our engine's output). LarkOS is community firmware, a fork of Polyend CE like this repo, for the Tracker Mini; closed beta, release planned mid-December 2026. Its code is in `core/source/mtHarmony.cpp` of `github.com/lark-tracker/lark-os` (not read: the host was not reachable). **Take ideas only. Ask the author before reusing any code or tables.**

## What it is
A pattern-based harmony system: the project key sits under every note; a chord is one step with chord-FX data indexing a firmware chord table of 134 entries (stock has 67: triads, 7ths, inversions; LarkOS adds 9ths, 11ths, 13ths, add9, 6/9, shell and open voicings). Different from ours: we play chords live on pads and record audio.

## Checked: the chords match
For C, D, E and F our triads, 7ths and 9ths give the same notes as theirs (Cmaj7 [48,52,55,59], Dm9 [50,53,57,60,64]). Differences: we drop G, A and B an octave (G is 43, theirs 55); we have no 11th or 13th.

## Ideas to take (in order)
1. **Per-degree fit rules and 11th and 13th colours.** A colour is fitted to the degree it lands on: iii skips the 9th and goes to Em7add11; IV gets Fmaj9#11; vii gets Bm11b5 (half-diminished with an 11th). Add two chord types (11th, 13th) and the fit rules in `core/ft/Chords.cpp` (extends the existing "drop a flat 9th" rule).
2. **One register for chord roots**: every root in C3 to B3 so chords rise smoothly (we drop pitch classes 6 to 11 an octave). Update `tests/host/ft_chords_test.cpp`.
3. **Shell voicing** (root, 3rd, 7th) beside the design's Close, Open and Drop 2.
4. **Bass rules for the Bass role**: least movement, root, third, fifth, or a pedal note. "Least movement" is a good basis for the Walk pad.
5. **A systematic pool for colour chords** instead of hand-picked: five secondary dominants, the tritone substitute, the sharp-four diminished, and chords borrowed from the parallel key (modal interchange). Also a good source for chord Sets.

## Maybe
6. Press a chord again for its next colour (their main interaction). The design's hold modifiers cover richer chords; revisit only if the modifiers feel fiddly on the device.
7. MIDI chord capture (hold three or more keys, write one chord): needs MIDI keyboard input.

## Leave
Pattern editor and chord-FX steps, the chooser that looks 32 steps either way (we have no suggestion feature, by decision), per-pattern key and project-wide key snapping, voice borrowing from idle tracks (we pitch the sample across the stock voices already).

## Also on their site (not harmony)
Beads granular send, a Cassette tape insert (like our lo-fi master), and eight instruments (808/909 drums, a string machine, a six-voice polysynth, a wavetable synth, a tonewheel organ). Worth a look when we plan more sounds.
