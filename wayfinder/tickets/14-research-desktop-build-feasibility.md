---
type: research
status: open
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
