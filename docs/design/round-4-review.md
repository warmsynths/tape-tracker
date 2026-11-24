# Review of the round 4 design (docs/design/four-track-recorder-design-r4.zip)

Checked frame by frame (32 frames, sections 4.1 to 4.13) against `screen-requirements.md` and the v1.0.0 display limits in `docs/research/audits/b-display-ui.md`. Nothing was run on the device.

## Meets the brief
- All listed screens and states are present. Button tables, accepted proposals and a "could not do" list are included.
- Fonts match the default set (14 light, 17 light, 14 regular, 17 bold, 12 regular). Stop and Rec are drawn shapes. No sloped lines, no images.
- Waveforms are 120 columns per lane (360 on the main screen), inside the 100 to 150 guidance.
- Bar choices respect 40 to 240 BPM.

## Fixed by Claude in the zip (owner asked)
- Pads and Memory now say "7.3 of 11.3 MB" and "47 s free in total" (placeholder numbers, real total is about 11.3 MB of sample memory).
- Input setup: the "aim between -18 and -6" text moved above the meter so it no longer overlaps the -18 label.

## Was needing change in the design (now fixed)
1. **Pads and Memory screens say "of 64 MB".** The device has 16 MB of memory, of which about 11.25 MB holds samples and recordings. Show "of 11.3 MB" or a plain bar with no total.
2. **Input setup level meter:** the "-18" tick label overlaps the text "aim between -18 and -6". Move or shorten one.

## Drawing budget: checked, fits if built the stock way
- Limits (from source): 8,172 bytes of drawing per screen (display list size of the screen chip), 65 controls, 8 bytes per waveform column (`display.cpp:640`, `displayStructs.h:21`, `spectrum.cpp:199-204`).
- Stock text drawing (`cTracker`, `cArrangement`, `cHorizontalTracker`) puts one 4-byte word per character plus a colour or font word when they change. A generic label control (`CMD_TEXT`) costs more per string (estimate, not verified).
- All 32 frames priced both ways. Built stock-style (our own controls that draw a whole lane or row), the heaviest frame is about 5,050 bytes (main screen while recording). Built with one generic label per text it would be up to about 8,100, at the limit.
- For comparison, the stock Pattern grid is about 15 rows x 8 tracks x roughly 13 characters, about 6,200 bytes of characters alone (rough count of the code, not measured).
- Conclusion: the design is not heavier than the stock screens. The rule for the build is: draw text and boxes inside our own controls the way the stock tracker control does, never as one label per piece. No cuts to the design are needed. A debug readout of bytes per screen will confirm it on the device.

## Firmware work the design implies (not design problems)
- Rename project has no stock support in v1.0.0 (needs new code).
- Per-slot effects on Parts, glitch, Wow and flutter, glue compression and bit-crush on the master are new code.
- Fades are drawn as tapering columns, as the design says.
