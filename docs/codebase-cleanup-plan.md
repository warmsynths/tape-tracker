# Codebase cleanup plan

Written after FT76 and the 20 s track change, using the codebase-design idea: deep modules (a lot of behaviour behind a small interface), real seams, and tests that go through the interface.

Nothing here is started. It is meant to be done after the next round of device testing, when the feature set has settled.

## Where things stand

- `core/ft` is about 10,900 lines. One class, `App`, holds most of it: about 240 functions across `App.cpp` (2,495 lines), `Pages.cpp` (2,561), `PageDraw.cpp` (1,901), `RolePages.cpp` and `FxPages.cpp`. They all read and write the same state, declared in `App.h` (675 lines).
- Recording, the tail ring-out, layering, bounce, copy, clear, undo, roles, export and every page are methods on that one class.
- Result: a change in one feature can break another through shared state, and tests must drive the whole `App` to check anything small.

## What is already good (keep it)

- `Hal` is a real seam. It has three adapters: the Tracker (`FourTrackHal`), the emulator (`EmuHal`) and the test fake (`FakeHal`). More than one adapter means the seam is real.
- The DSP modules (`Dsp`, `SendFx`, `Synth`, `Drums`, `Chords`, `Roles`) are deep and testable on their own, with their own host tests.

## Steps, in order of value

1. **`TakeRecorder`** (most value). Move first take, tail ring-out, layer and punch, auto-mute, and the live waveform out of `App`. Interface: start a take, tick it, stop it, ask what it produced. It takes the `Hal` and the settings it needs. Most of the bugs found in testing (gaps, folded tails, layer overwrites) lived here. It also gives loop-mechanics task #20 a clean home.
2. **`TrackOps`**. Bounce, Copy, Clear and Undo into one module. They share the undo step (`LastOp`, `snapshotForUndo`) and repeat the "restore the session" code.
3. **Pages as a table**. Each page becomes a small object with `draw`, `key` and `jog`. Today these are three big switch statements, so adding a page touches three places.
4. **Delete unreachable stock code**. The stock Tracker screens that can no longer be reached. Mostly flash; it also speeds up builds.

## Rules for doing it

- One step at a time, each ending with all host tests passing and a device build tested by the owner.
- No change to the bootloader area or the flash start address (CLAUDE.md).
- Replace tests that reach past an interface with tests that go through the new one. Do not keep both.
- Do not start while a feature is still being tested on the device.

## Deletion test (is a module earning its keep?)

If removing it would make the complexity reappear in several callers, it is deep and should stay. `Hal`, `Dsp` and `Synth` pass. Many one-line `App` helpers would fail: they are pass-throughs and can be folded into their callers.
