# Readiness matrix

Which features are ready to be built into the firmware, and what is still missing for each. Drafted from the decisions in `wayfinder/`, the research in `docs/research/`, and a reading of the simulator source in `prototype/sim/`.

## How to read it
A feature is ready to build only when every check below says Yes. Looking right in the simulator is one check out of six, because a browser hides what makes firmware hard: it has unlimited CPU, perfect timing and as much memory as it wants.

| Check | Question |
|---|---|
| Decided | Is the rule written down in the map with no open question? |
| Simulator | Does the simulator do it, and is there a test? |
| Fits hardware | Does it fit the Original's memory and CPU? |
| Real control | Is it mapped to a real button, wheel or pad? (Ticket 11 is still open, so this is "Open" for almost everything.) |
| Firmware path | Do we know which stock code to change or add? |
| Device test | Is there a short test you can run on the device? (None defined yet.) |

Cell values: **Yes**, **Partly**, **No**, **Unknown**, **Open** (waiting on a named ticket), **n/a**.

What I have and have not verified:
- Simulator column: read from the source. The four logic tests (first take, alignment, starting tempo, nudge and undo) **pass** under the simulator's own test runner, and its type check and production build pass. I also loaded the built page in a browser and recorded a first take with no errors. I have not listened to it or judged how it feels.
- Hardware columns: taken from the research files, which contain no measurements from a real device. Every CPU figure is a guess.

## The matrix

| # | Feature | Decided | Simulator | Fits hardware | Real control | Firmware path | Device test | Verdict |
|---|---|---|---|---|---|---|---|---|
| 1 | First take records freely, silence before the first note is cut | Yes | Yes, tested | Partly | Open | Partly | No | Needs recorder change |
| 2 | Choose bars, tempo derived from the length played | Yes | Yes, tested | Yes | Open | Yes | No | Bar choices follow the design (see finding 3) |
| 3 | Optional starting tempo with a one-bar count-in | Yes | Yes, tested | Partly | Open | Unknown | No | Needs a click sound source |
| 4 | Later Parts: arm, wait for loop start, keep pre-roll, record one loop, stop | Yes | Yes, not tested | Partly | Open | Partly | No | Highest risk, see finding 1 |
| 5 | First note snaps to the loop start inside the window | Partly | Yes, tested | Partly | n/a | Yes | No | Window size is a guess |
| 6 | Offset in ms, bars or sixteenths, wrapping around the loop | Partly | Yes, tested | Yes | Open | Unknown | No | Play-once case undecided |
| 7 | Undo the last take per track | Yes | Yes, tested | No | Open | Unknown | No | Memory problem, see finding 2 |
| 8 | Loop Mode: repeat or play once | Yes | Yes, not tested | Yes | Open | Yes | No | Stock loop modes exist |
| 9 | Mute per track | Partly | Yes | Yes | Open | Yes | No | Only implied in the design |
| 10 | Four mono tracks playing in sync from RAM | Yes | Yes | Partly | n/a | Partly | No | CPU unmeasured |
| 11 | Live input monitor with feedback protection | Yes | Partly | Partly | Open | Unknown | No | Protection is text only in the simulator; the design also shows how far the monitor is turned down, which needs a gain-reduction readout from the limiter |
| 12 | Input setup: line, mic low, mic high, gain, level meter | Yes | Yes | Yes | Open | Yes | No | Gain functions found in code |
| 13 | Settings, project-wide versus device-wide | Yes | Yes | Unknown | Open | Unknown | No | Needs the project file decision |
| 14 | Project: new, open, save, save as, rename | Partly | Mock only | Unknown | Open | Unknown | No | File format not decided |
| 15 | Export WAV with 1 to 16 repeats | Yes | Fake progress only | Partly | Open | Partly | No | Stock WAV export exists |
| 16 | Pads played live and captured as audio on a track | Partly | Partly | Partly | Open | Partly | No | Small change, and it needs the same two changes as feature 4 (see finding 4) |
| 17 | Lo-fi master chain: glue compressor, saturation, wow and flutter, bit-crush | Partly | No | Unknown | Open | No | No | Mostly to be written, after the CPU is measured |
| 18 | Screens: main, recording states, offset, bars, settings, input, project | Yes | Yes | Partly | Partly | Yes | No | The design assigns the eight screen buttons, jog and arrows on every screen; the printed-label mapping is still open. Redraw cost unmeasured |
| 19 | Popups: replace, no sound, memory full, export progress, SD missing | Yes | Partly | Yes | n/a | Yes | No | SD-missing is not in the simulator |
| 20 | First take stops at the memory limit and is kept | Yes | Yes | Yes | n/a | Yes | No | |
| 21 | Click during playback and before a Part | Yes | Yes | Partly | Open | Unknown | No | Same click source as feature 3 |
| 22 | 4/4 only | Yes | Yes | Yes | n/a | Yes | No | |

## Evidence notes
1. Stock recording is rounded to a 128-sample block and triggered from the screen loop, and the recorder writes to the start of the whole sample pool. It must be pointed at each Part's own memory and must stop freezing other voices. `docs/research/record-path-and-latency.md`, `docs/research/cpu-ram-budget.md`.
2. Alignment detector: about 15 ms per 10 s loop, an estimate. `docs/research/onset-detection.md`.
3. Four 30 s mono Parts take about 10.1 of the 11.25 MiB sample pool. `docs/research/cpu-ram-budget.md`.
4. Stock effects: only a gate-style compressor, crude saturation curves and bit-crush exist. Wow and flutter, a proper glue compressor and sample-rate reduction would be new code. `docs/research/master-fx-availability.md`.
5. Gain ranges: line about 0 to +22 dB, mic low 0 to +25 dB, mic high 0 to +44 dB, from the audio chip code.
6. Measured on the stock Original build (not on the device): flash 89.21% used, about 113 KB free; RAM 69.08%; SDRAM 93.50%. `docs/build-notes.md`. Flash headroom is tighter than the repo's own figure of about 144 KB, so new code has less room than the research assumed until the official build is compared.

## Findings this table surfaced
1. **Arming a take inside the audio interrupt is the riskiest change.** Sample-accurate recording needs new code in the part of the firmware where a mistake means glitches or a hang. It should be the first slice built and tested on the device.
2. **Undo probably does not fit in memory.** The simulator keeps the previous take for each track. Four 30 s Parts already use about 10.1 of 11.25 MiB, so a single undo copy at that length (about 2.5 MiB) does not fit. Options: undo only for shorter loops, store the previous take on the SD card, or drop undo from v1. This is a decision for the owner.
3. **Bar choices now follow the design.** The design offers 1, 2, 3, 4, 6 and 8 bars, and 12 to 32 bars after a long first take that ran out of memory. The original decision (1, 2, 4, 8, 16) is superseded. One small mismatch remains: the simulator's long list is 8, 12, 16, 20, 24, 32, while the design's frame 8i shows 12, 16, 20, 24, 28, 32.
4. **Recording pad hits as audio has a route, and it is small.** The recorder has no internal source today, but the audio graph already has record queues on every voice and the master, built for the exporter. The best option is a pad-bus source on reserved voices recorded dry into the Part buffer. It needs the same two changes as later-Part recording: stop the recorder freezing voice updates, and arm the start inside the audio interrupt. CPU and RAM costs are estimates. `docs/research/pad-recording-path.md`.
5. **Several simulator features are mock-ups.** The project list, the export progress, the level meters and the feedback protection are drawn but do nothing real. They count as screens, not as proven features.
6. **No feature has a device test yet.** Until the recovery drill (ticket 07) and the stock build (ticket 06) are done, nothing should be flashed.

## Proposed build slices
Each slice is flashed by the owner and checked on the device before the next starts.
1. Record one track and loop it back, with sample-accurate starts. This proves the audio path and the memory plan (features 1, 8, 10).
2. The loop rules: bars prompt, tempo, later Parts, alignment, offset (features 2, 4, 5, 6).
3. Input setup, monitoring and feedback protection (features 11, 12).
4. Settings, projects and export (features 13, 14, 15).
5. Screens and popups on the real display (features 18, 19, 21).
6. Pads and the lo-fi chain, if the research and CPU budget allow (features 16, 17).

## What turns a cell to Yes
- Tickets 06 and 13 and a first device test fill the "Fits hardware" and "Device test" columns.
- Ticket 09 settles features 5 and 6.
- Ticket 10 settles feature 17 and the memory plan.
- Ticket 11 fills the "Real control" column.
- Ticket 16 (project storage) and ticket 17 (display pipeline and motion) are in progress.

## Added from the design (`docs/design/four-track-recorder-design.zip`)
7. **The design lists things it wanted but left out.** The ones that need an owner decision: an "Unsaved changes" question before New or Open; a quick monitoring toggle on the main screen; a protection indicator on the main screen; deleting a project. None is in the matrix yet.
8. **Play once mode and Offset wrap is still open.** The design flags that a wrapped piece would sound before the Part itself, and that frame 7d would need to draw differently if the alignment-rules ticket chooses cut or delay instead of wrapping.
