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
- Flat, opaque `#RRGGBB` colors only. **No alpha, gradients, shadows or blur** (the theme format rejects alpha values, `docs/custom_themes_and_fonts.md`).
- Use color **roles**, not free colors, so every theme can recolor the screen: background; top bar (`text`, `background`, `frame`); labels (`active`, `inactive`, button states, `redButton`); lists; bars (`active`, `inactive`); waveform (`audio`, `background`, `progress`, `sliceMarker`, `points`); accents (`white`, `red`, `orange`, `yellow`, `green`, `cyan`, `grey`).
- Three built-in themes exist (Original, Monochrome, Anniversary); a design should still read in the Monochrome one.

## 4. What can be drawn
The firmware builds screens from a fixed set of controls (`core/libs/NewDisplay`): text labels, frames and filled boxes, lists, horizontal and vertical bars (meters), progress cursors and line indicators (playhead), point markers, and a waveform control.

- **Waveforms** are a min/max envelope of at most **600 points** across the width (`strTrackerSpectrum`: 600 upper and 600 lower values). Plan each lane's waveform as no more than 600 columns wide.
- No rounded corners, curves, icons-as-bitmaps or large images. Custom bitmaps cost scarce memory and are out unless a screen truly needs one.
- Keep the element count per screen low; the display list is built by the CPU.

## 5. Speed and memory
- The Original has the slowest CPU in the family (180 MHz). Stock redraws of a busy screen are noted at roughly **20 to 35 ms per step** (`docs/developer_guide.md`).
- Animate only the playhead line and level meters. No scrolling or zooming waveforms, no fades, no animated transitions. Waveforms are drawn once when they change.
- Free RAM is very tight (about 1 MB of SDRAM free, shared with recording buffers). Any screen state that needs extra memory must be justified.

## 6. This product
- Four tracks always visible together on the main screen; no vertical scrolling between tracks.
- Terms follow `CONTEXT.md`: Track, Part, Loop, Loop Mode, Offset.
- Dark display, readable at arm's length, no tiny text for anything the user must read while playing (use 14 or 17, keep 12 for secondary labels).
