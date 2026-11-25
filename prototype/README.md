# Prototypes

- **`emulator/`: the current one.** The web emulator of the Four-Track recorder. It runs the same screen and recording code as the Tracker firmware (`core/ft/`, compiled to WebAssembly) with the Tracker's real fonts.
  - Open `emulator/dist/four-track-emulator.html` (double-click it, then click once on the page). No build or server needed.
  - If the browser blocks sound or the microphone from a file: `cd emulator/dist && python3 -m http.server 8000`, then open http://localhost:8000/four-track-emulator.html.
  - Rebuild after changing `core/ft/`: `python3 emulator/build.py` (needs clang with the wasm target).
- **`sim/`: old, superseded.** The first throwaway sketch (a Vite page, `npm run dev` serves it at localhost:5173). It does not run the firmware code. Kept for reference only.
- **`four-track-PROTOTYPE.html`: old, superseded.** The very first single-file sketch.
