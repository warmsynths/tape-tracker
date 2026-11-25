# Start here

This fork is the home of a four-track recorder firmware for the Original Polyend Tracker. There is no new firmware yet: everything here is planning, research and a prototype. The firmware source is still Polyend's, unchanged.

## What to run
- **The web emulator** (try this first): https://claude.ai/artifact/Jk1zHN3Y4xx8W5bXJrSijX (private to you), or double-click `prototype/emulator/dist/four-track-emulator.html` and click once. It runs the same screen and recording code as the Tracker firmware, compiled to WebAssembly, with the Tracker's real fonts.
- **The firmware** is in `firmware/` (files starting `PolyendTrac`). Build plan and progress: `docs/build-plan.md`. How to flash and what to check: `docs/first-test-build.md`.
- Rebuild the emulator with `python3 prototype/emulator/build.py` (needs clang with the wasm target).

## What is where
- `CONTEXT.md`: the words we use (Track, Part, Loop, Offset and so on).
- `wayfinder/map.md`: the plan, the decisions so far and what is still open. `wayfinder/tickets/` has one file per question.
- `docs/research/`: findings on CPU and memory, recording delay, first-note detection, lo-fi effects and a desktop build.
- `docs/design/`: the hard screen limits and the first prompt for Claude Design.
- `core/ft/`: the shared Four-Track application and screen drawing, run by both the firmware and the emulator.
- `prototype/emulator/`: the web emulator. `prototype/four-track-PROTOTYPE.html` is the old throwaway sketch.
