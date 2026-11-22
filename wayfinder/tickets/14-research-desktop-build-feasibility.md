---
type: research
status: closed
blocked-by: []
assignee:
---
# Feasibility of a desktop build of the firmware

## Question
No firmware emulator exists, so testing means flashing the real device. Could the firmware's core be built and run on a PC (a desktop "platform" behind the existing `Platform` interface in `core/include/Platform.h`) so the UI and the recording logic can be tried without hardware? Find what would have to be replaced or shimmed:
- the FT812 display-list output (is a reusable FT81x renderer available, and how does the firmware build its screens?)
- the Teensy-derived audio library and its interrupt-driven timing
- ARM-specific code, memory placement macros (`__NOINIT(EXTERNAL_RAM)`) and linker sections
- the SD card, buttons, pads, jog wheel and codec interfaces

Give a rough size of the work, what it would and would not tell us (timing and CPU load cannot be learned on a PC), and a recommendation: build it, defer it, or never. This feeds the "how we test" part of the v1 spec and does not block it.

## Resolution
Findings are in `docs/research/desktop-build-feasibility.md`. The research agent's summary, with its own confidence levels:
- Recommendation: defer the full windowed desktop build. Later, build a headless test harness first (estimated 2-4 weeks), and add a screen only if iterating without the device proves worth it (estimated 4-8 weeks in total). Not "never".
- 213 of 229 core `.cpp` files passed a host `g++` syntax check unchanged, apart from one 64-bit struct-size assert. Nothing was linked or run.
- The screen is the biggest obstacle. No source-available FT81x emulator was found; Bridgetek's is a proprietary binary for Windows and Ubuntu 24 only, its licence is limited to EVE development, and the agent could not try it.
- A PC build cannot show CPU load, SD throughput, interrupt timing or the roughly 1 MB of free SDRAM. The ARM CI link report covers memory without hardware.
- Twelve obstacles are listed with file and line.
- Confidence: high that core can build and run on a PC; low to medium on the effort estimates and on the display route; medium on the "defer" call, which depends on how often the user has a device to flash.
