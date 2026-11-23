---
label: wayfinder:map
---
# Four-track recorder for the Original Tracker

## Destination
A written v1 spec for a four-track recording firmware on the Original Polyend Tracker (MK66): workflow, alignment rules, controls, effects budget and limits, concrete enough to hand to a build plan. The map ends at the spec; building is not part of it.

## Notes
- Glossary is in `CONTEXT.md` at the repo root (Track, Part, Loop, Loop Mode, Offset, Alignment, Alignment Window).
- The user does not write C or C++; Claude writes the code and the user flashes and tests on hardware.
- Never touch the bootloader or the flash start address. Hardware facts live in `docs/memory_map.md` and `docs/developer_guide.md`.
- There is no firmware emulator, so UI and behaviour questions go through a computer prototype first.
- Consult the grilling and domain-modeling skills for HITL decisions, and the research skill for research tickets.

## Decisions so far
- Chord feature (owner, later): small only. Pleasing chord sets that play with a chosen sample, plus a "suggest next chord" helper. No vibe prompts, band modes, melody, song arranger or cloud features from chroma-chords. Estimated 15 to 40 KB of flash and 3 to 4 of the 8 voices (estimates). Pattern and Song screens are hidden, not deleted, until the keep/drop list is settled.
- [Core workflow, device and limits](tickets/01-core-workflow-and-device.md): Original Tracker, full UI replacement, 4 mono tracks, first Part sets the Loop, later Parts record one aligned loop, stock engine kept and trimmed, 4/4 only.
- [First-note detection](tickets/04-research-onset-detection.md): envelope detector with adaptive noise floor and zero-crossing snap, no FFT, about 15 ms per loop (estimated); default Alignment Window about 60 ms (a starting guess).
- [Which lo-fi effects exist in stock on MK66](tickets/05-research-master-fx-availability.md): only a gate-style compressor, crude saturation curves and bit-crush exist; wow/flutter, a proper glue compressor, sample-rate reduction and CPU load-shedding must be written (CPU costs unmeasured).
- [Recording path and latency](tickets/03-research-record-path-and-latency.md): about 7-11 ms input-to-output delay (planning figure, unmeasured); recording start is block-rounded today and needs arming in the audio interrupt for sample accuracy; compensation is one measured constant; the monitor cannot feed back digitally.
- [CPU and RAM budget for four tracks](tickets/02-research-cpu-ram-budget.md): RAM fits (about 10.1 of 11.25 MiB) only if the Sample Editor buffers go; CPU is an unmeasured guess of 35-60%; recorder and per-voice update behaviour must change.
- [Feasibility of a desktop build of the firmware](tickets/14-research-desktop-build-feasibility.md): defer; core can likely build on a PC, but the screen is the big obstacle (no usable FT81x emulator), and a PC build cannot show CPU load or timing. If wanted later, start with a headless harness (about 2-4 weeks, estimated).
- [How pads played live can be captured as audio](tickets/15-research-pad-recording-path.md): a pad-bus source on reserved voices, recorded dry into the Part buffer; a small change that shares two required changes with later-Part recording (CPU and RAM estimated, not measured).
- [How a four-track project could be stored](tickets/16-research-project-storage.md): a new folder per project with a small settings file, four mono WAV Parts and an `undo/` folder; the stock project format has no room for our fields; undo on the SD card is viable (timings estimated, not measured).
- [How the screen is drawn on the Original and what motion it can sustain](tickets/17-research-display-pipeline-and-animation.md): the design's motion table looks feasible (about 20 updates a second is 3 to 10% of main-loop time, estimated); watch the display-list length changing and the byte-by-byte link; the 20 to 35 ms stock note does not apply.
- [Recovery drill on the owner's device](tickets/07-task-recovery-drill.md): passed. Config firmware update works; paperclip recovery flashes the official file; at 100 the Tracker needs a manual restart.

## Open tickets
(Open tickets are the files in `tickets/` with `status: open`; the frontier is those with no open `blocked-by`.)

## Not yet specified
- Project save/load, naming and WAV export format.
- Editing tools beyond one undo: trim, split, duplicate a Part, copy between tracks.
- Per-track mix controls: level, pan, mute, solo.
- Drum kit loading from the SD card, and how pads behave in edit mode.
- MIDI/USB use.

## Out of scope
- Building the firmware: this map ends at the spec.
- Tracker+ and Tracker Mini support.
- Internal synths, the chord suite, more than four tracks, explicit pitch or varispeed, and bounce/mixdown: wanted later, each a fresh effort.
- Rewriting the audio engine, and upstreaming to polyend/tracker-ce.
- Deleting a project from the recorder (files can be deleted on a computer).
