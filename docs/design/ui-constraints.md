# UI design constraints (Original Tracker, four-track recorder)

Hard limits for anyone designing screens for this firmware. Each one comes from the firmware source or its docs; anything uncertain is marked "unconfirmed". Designs that break these cannot be built, or would stall the device.

## 1. Screen and input
- **800 x 480 pixels**, one screen at a time. Source: `README`/`docs/custom_themes_and_fonts.md` (800x480 FT812 display).
- Controlled by buttons, one jog wheel and pads. Design nothing that needs hover, mouse or touch; every element must be reachable by moving a cursor.
- Stock screens put **eight 100 px columns along the bottom**, one per function button under the screen (`gSampleRec.cpp` uses `800/8` per column; `docs/info_viewer.md` calls them "8 Screen Buttons"). Unconfirmed: that the Original has exactly eight such buttons; check against the device.

## 2. Fonts (fixed set of five)
| Slot | Size and weight |
| --- | --- |
| 1 | 14 Regular |
| 2 | 17 Regular |
| 3 | 14 Bold |
| 4 | 17 Bold |
| 5 | 12 Regular |

- No other sizes or weights. Text is 4-bit grey antialiased.
- Each font must stay under 22,396 bytes decompressed (`kMaxFontBufferSize`), which also limits how many glyphs a custom font may carry.
- The only built-in icons are glyphs in the 17 Bold font: **Pause, Stop, Play, Rec, Folder, Up-dir, Loop, Arrow** (`docs/developer_guide.md`, section 4). Any other icon must be drawn from simple shapes or written as text.

## 3. Color
- Flat `#RRGGBB` colors. **No gradients, shadows or blur.** A theme cannot carry alpha values (`docs/custom_themes_and_fonts.md`), but a screen may draw a fill see-through (a transparency change costs 4 bytes): the home screen's track tints do. Use it for tints, not for layered effects.
- Use color **roles**, not free colors, so every theme can recolor the screen: background; top bar (`text`, `background`, `frame`); labels (`active`, `inactive`, button states, `redButton`); lists; bars (`active`, `inactive`); waveform (`audio`, `background`, `progress`, `sliceMarker`, `points`); accents (`white`, `red`, `orange`, `yellow`, `green`, `cyan`, `grey`).
- Three built-in themes exist (Original, Monochrome, Anniversary); a design should still read in the Monochrome one.

## 4. What can be drawn
The firmware builds screens from a fixed set of controls (`core/libs/NewDisplay`): text labels, frames and filled boxes, lists, horizontal and vertical bars (meters), progress cursors and line indicators (playhead), point markers, and a waveform control.

- **Waveforms** are a min/max envelope of at most **600 points** across the width (`strTrackerSpectrum`: 600 upper and 600 lower values). Plan each lane's waveform as no more than 600 columns wide.
- **Shapes are not limited to right angles** (corrected after round 5; the earlier text came from the stock firmware's control set, not from the display chip). Our screens draw straight on the display chip, which can do:
  - filled rectangles, lines at any angle and line strips, filled circles (the record dot is one);
  - **rounded rectangles** (the chip does it natively; the corner radius is one setting, so a screen that keeps one radius pays for it once: about 4 bytes);
  - rings (two circles, the inner one in the background colour), dial arcs (a row of small dots along a curve, about 20 for a smooth one) and needles (a line);
  - one-bit bitmaps (the waveforms) and see-through fills.
  What the drawing layer exposes today: rectangle, line, circle, text, bars, one-bit bitmap and transparency. Rounded rectangles, rings and arcs are cheap for the firmware side to add: ask, and say which you need.
- Still out: gradients, shadows, blur, anti-aliased vector icons, large images. Custom bitmaps cost scarce memory and are out unless a screen truly needs one.
- **Cost is the real limit, not geometry.** About 8,172 bytes of commands per screen: rectangle 20, line 20, circle 24, colour change 4, transparency change 4, text 16 + 4 to 8 per character, bar 12 + 8 per column. A screen full of outlined, rounded cells adds up, so keep a rough count of shapes.
- The web emulator draws shapes exactly; the chip's edges can differ a little. Anything curved should be checked on the Tracker before it becomes the whole language of the design (the firmware side can add a one-screen shape test).
- Keep the element count per screen low; the display list is built by the CPU.

## 5. Speed and memory
- The Original has the slowest CPU in the family (180 MHz) and sends data to the screen chip byte by byte. The developer guide's figure of 20 to 35 ms per busy redraw is a stock note, and the display research found it does not describe this code. Research estimate for the motion in the design: about 20 updates a second is roughly 3 to 10% of main-loop time. These are estimates, not measurements (`docs/research/display-pipeline-and-motion.md`).
- Animate only the playhead line, the level meters and a few small text readouts (position, countdown) that change in steps of about 100 ms or on the beat, never every frame. The stock firmware already updates a timer widget during playback, but this has not been measured on the Original. Waveforms are drawn once when they change.
- Motion tiers (nothing here has been measured on the Original):
  - Likely fine: playhead, level meters, a pulsing record or armed indicator, a highlight that moves between tracks or fields, changing numbers.
  - Needs a test on the device: fades, a waveform growing while recording, a short slide when a panel opens.
  - Keep the number of drawn elements stable. A blink should change an element's color, not make it disappear, and a readout should keep a fixed width, because a change in the length of the drawn list forces everything after it to be re-sent.
  - Unlikely on the Original: whole-screen transitions, smooth scrolling or zooming, blur, large image animation, anything moving across many parts of the screen at once.
- Free RAM is very tight (about 1 MB of SDRAM free, shared with recording buffers). Any screen state that needs extra memory must be justified.

## 6. This product
- Four tracks always visible together on the main screen; no vertical scrolling between tracks.
- Terms follow `CONTEXT.md`: Track, Part, Loop, Loop Mode, Offset.
- Dark display, readable at arm's length, no tiny text for anything the user must read while playing (use 14 or 17, keep 12 for secondary labels).
