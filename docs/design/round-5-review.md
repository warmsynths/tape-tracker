# Round 5 review (C1 to C5)

Reviewed 2026-11-26 against the real limits (the design file is in `docs/design/Four-Track Recorder Design r5.zip`; C6 to C11 are still to come).

## Verdict
Good and buildable. Same look as the built Four-Track screens, so the pages can reuse the same drawing code. Frames are C1a-b mixer, C2a-d load samples, C3a-b pad sample, C4a-d resample, C5a-d device settings.

## Fix in the design
1. **Memory totals say "of 64 MB" (C2, C4).** The real sample memory is about 2.4 MB: the pool is 11.8 MB and the four track regions (26 s each) take 9.2 MB, the application 128 KB. Say "of 2.4 MB" or show the bar only. This is also a product question: the pads have room for only about six samples of 370 KB. See "Questions".
2. **Resample length (C4).** The Length control is 1 to 4 loops; memory left "fits 2 min 06 s" is wrong for 2.4 MB (about 27 s of mono 16-bit at 44.1 kHz). Recompute from the real number.
3. **Firmware update (C5c, C5d).** Fine, but the update itself is stock code that we keep; the page only wraps it. "Basement 04 was saved before the update started" needs us to save first; that is buildable.

## Element counts (budget check)
Mixer 119 and 120 elements, load samples 89, pad sample 64 to 67, resample 61 to 65, settings 37 to 53. The built Glitch page is about 100 and fits. The mixer is the risk; we will know when it is built (the page canvas reports if it overflows its five stages).

## What each screen needs that does not exist yet
- **C1 Mixer:** per-track level (dB), pan, tone (low-pass to high-pass in one control), solo, reset. The engine has gain, pan and filters only as glitch slot effects; they need to become fixed per-track controls in the track renderer. Solo is new.
- **C2 Load samples:** browse the SD card and load a WAV into a pad slot, preview it, replace a slot. Uses the stock sample loader code in the firmware and browser files in the emulator. "To track" (load as a Part) is a later feature.
- **C3 Pad sample:** tune, volume, pan, play mode (one-shot or loop), start, end of one pad sample, and Keys or Pad mode. These are fields the stock instruments already have.
- **C4 Resample:** record the finished mix (after the lo-fi master) into a pad slot. Needs a tap on the master output; the existing pad-bus tap does not include the loop player.
- **C5 Device settings:** MIDI, USB audio, line input, volume, display, firmware, about, plus a Four-Track group. A rebuild of the stock Config; large, and the riskiest to remove stock code for.

## Questions
1. Memory: with the tracks at 26 s the pads get about 2.4 MB. Do you want shorter tracks (for example 12 s each, giving about 6 MB for samples) or keep 26 s?
2. Resample length: whole loops only (1 to 4) as designed, or free length?
3. Config: rebuild all of it, or keep the stock Config and only add a Four-Track group?
