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

## Open risk, needs a device test before the design is final
- **Control count and drawing budget.** The firmware allows 65 controls on the whole screen, about 43 free after the shared ones, and 8,172 bytes of drawing for the whole screen. The design reports 94 to 149 elements per screen. An element is not the same as a control, because we can write one custom control that draws a whole lane, but the byte budget still applies. Estimate for the main screen: about 2,900 bytes of waveform plus texts and boxes; Performance has 149 elements and no waveform. Both may be close to the cap. If the cap is hit, the top bar and button labels disappear first.
- Proposed check: a test build that draws the busiest screens (main with three lanes, Performance) with fixed content, and reports whether anything vanishes. Needs the owner to flash it.

## Firmware work the design implies (not design problems)
- Rename project has no stock support in v1.0.0 (needs new code).
- Per-slot effects on Parts, glitch, Wow and flutter, glue compression and bit-crush on the master are new code.
- Fades are drawn as tapering columns, as the design says.
