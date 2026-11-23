# Audit B: display and UI claims against Polyend v1.0.0

Scope: `docs/research/display-pipeline-and-motion.md`, `docs/design/ui-constraints.md`, the hard limits in `docs/design/claude-design-first-prompt.md` and `docs/design/round-3-brief.md`, and the `docs/developer_guide.md` redraw-time statement.
All evidence is `file:line` in `/home/user/tape-tracker` (read-only). Nothing was built or run; the arithmetic is mine and labelled.

## 0. Bottom line

1. **The research document describes the wrong display code.** It describes Polyend's unreleased `main` (CPU-built display lists, an "arena", `readyForFrame`, `findChangedRuns`, `chipLists`, 320-word upload passes, a `REG_DLSWAP` gate). None of those exist in v1.0.0. Proof: `git show archive/main-on-unreleased:core/libs/NewDisplay/display.cpp` has 8 hits for `readyForFrame|chipLists|findChangedRuns`; `git show v1.0.0:core/libs/NewDisplay/display.cpp` has 0. `git diff v1.0.0 HEAD` on `core/libs/NewDisplay`, `platforms/mk66`, `core/source/Tracker.cpp` and `docs/developer_guide.md` shows only our Audio additions, so the display code in the working tree is v1.0.0's. Its open question "is the rewrite the fork's or upstream's" is answered: it is only on unreleased `main`.
2. **v1.0.0 draws with the FT812's coprocessor, one control per main-loop pass.** The CPU sends command words over SPI; the coprocessor builds each control's display-list fragment, which is copied into the chip's RAM_G and re-appended on every frame. See section 5.
3. **The `docs/developer_guide.md` 20 to 35 ms figure does describe v1.0.0.** The research dismissed it on a false premise. Section 4.
4. **The SPI link is 30 MHz, not "20 or 30".** Section 3, RD-21.
5. **Several "hard limits" in the UI constraints are wrong in both directions.** The glyph list is wrong, the font table is only right for the non-default font set, "no alpha/gradients/rounded/bitmaps" is a theme-file fact, not a drawing limit. And the one limit that really bites the design is missing: **8,172 bytes of display list per frame for the whole screen**, which four lanes of 600-column waveforms cannot fit.

Status keys: VERIFIED IN v1.0.0, DIFFERS IN v1.0.0 (correct statement given), NOT FOUND IN v1.0.0, ESTIMATE. Tally is at the end of section 3.

## 1. `docs/design/ui-constraints.md`

| ID | Claim | Status | Evidence / correct statement |
| --- | --- | --- | --- |
| UC-01 | Screen is 800 x 480 | VERIFIED IN v1.0.0 | `platforms/mk66/3rdparty/FT812/MCU_Layer.cpp:291-292` (`lcdWidth = 800; lcdHeight = 480`); `docs/custom_themes_and_fonts.md:58`. README does not state it (the cited "README" source is wrong; harmless). |
| UC-02 | Buttons, one jog wheel and pads; nothing needing touch or hover | VERIFIED IN v1.0.0 | Pads/jog: `docs/developer_guide.md:167` (48 pads, rotary jog-wheel); no touch handling anywhere in `core/interface`. |
| UC-03 | Stock screens put eight 100 px columns along the bottom, `gSampleRec.cpp` uses `800/8` | VERIFIED IN v1.0.0 | `core/interface/sampleRecorder/gSampleRec.cpp:17-25`: frames at x = 1 + 100 i, width 97, y = 424, height 55. Content area is y 31 to 421 (`:8-15`, height 391). Our own screen uses the same grid (`core/interface/fourTrack/gFourTrack.cpp:8-16`). |
| UC-04 | `docs/info_viewer.md` calls them "8 Screen Buttons" | NOT FOUND IN v1.0.0 | No such file. The wording is in `docs/image_viewer.md:86-89` ("Screen Button 1 ... 8", numbered 1 to 8, left to right). |
| UC-05 | Unconfirmed: the Original has exactly eight such buttons | VERIFIED IN v1.0.0 | Eight. Code numbers them `interfaceButton0` to `interfaceButton7` (`core/interface/core/interfaceDefs.h:66-73`), zero-based; docs say 1 to 8. Matrix mapping `platforms/mk66/source/ButtonsImpl.cpp:7-13` (last eight entries map to 7..0). UI convention puts button i under column i (`fSampleRec.cpp:445-463`, `gSampleRec.cpp:17-25`), so 0..7 is left to right. Other Original buttons: Params, Performance, File, Config, Master, Sample Load/Play/Edit/Rec, Song, Note, Instr, Vol, Fx, Pattern, Play, Copy, Insert, Up, Delete, Rec, Shift, Left, Down, Right (`interfaceDefs.h:145-176`). |
| UC-06 | Five font slots: 1 = 14 Regular, 2 = 17 Regular, 3 = 14 Bold, 4 = 17 Bold, 5 = 12 Regular | DIFFERS IN v1.0.0 | Five slots, yes (`display.cpp:85`). But the default font set "Original" (`display.cpp:113-135`, loaded at `:312`, `ThemeManager.cpp:47,1565`) is Roboto Mono: slot 1 **Light** 14 (cell 10x18), slot 2 **Light** 17 (10x22), slot 3 **Regular** 14 (10x19), slot 4 Bold 17 (12x22), slot 5 Regular 12 (8x16). There is no Bold 14 in it. The listed table is exactly the optional "New" set (Liberation Sans, `display.cpp:89-111`) and the custom-font converter (`docs/custom_themes_and_fonts.md:184-191`). The enum comments at `displayStructs.h:104-109` also use the nominal names. Roboto Mono advances are about 8-11 px (nearly monospaced); Liberation is proportional (3 to 18 px). (Cell sizes and widths decoded from `core/libs/fonts/*.cpp`.) |
| UC-07 | No other sizes or weights; text is 4-bit grey antialiased | VERIFIED IN v1.0.0 | Format byte is 2 (L4) in all ten built-in fonts; `API_BITMAP_LAYOUT(L4, ...)` at `display.cpp:284`. Style values above 5 are cleared (`display.h:80-83`, `display.cpp:724-727`). |
| UC-08 | Each font must stay under 22,396 bytes decompressed (`kMaxFontBufferSize`) | VERIFIED IN v1.0.0 | `display.cpp:81,83`; custom font rejected if `entries[i].size > kMaxFontBufferSize` (`:1290`), so "at most", not "under". Nuance: 22,396 is exactly the size of the largest built-in font (LiberationSans Bold 17: 103 glyphs x stride 9 x height 24 + 148). It is a software constant, not a hardware limit. |
| UC-09 | ...which also limits how many glyphs a custom font may carry | VERIFIED IN v1.0.0 | Size = glyphs x stride x height + 148 (`display.cpp:1318`); only codes 0 to 127 are counted (`:1304`). |
| UC-10 | The only built-in icons are eight glyphs in the 17 Bold font: Pause, Stop, Play, Rec, Folder, Up-dir, Loop, Arrow (`developer_guide.md` section 4) | DIFFERS IN v1.0.0 | Eight glyph cells at 0x18..0x1F exist in slot 4 (confirmed by decoding both Bold 17 fonts), but they are **four icons, each two cells wide**: Pause (0x18+0x19), Play (0x1A+0x1B), Loop (0x1C+0x1D), Perf (0x1E+0x1F). Source comment: `core/libs/fonts/RobotoMono-Bold_17_L4.h:4-5` ("PauseL, PauseR, PlayL, PlayR, LoopL, LoopR, PerfL, PerfR"); usage in pairs `core/interface/core/songTimer.cpp:89-107`. **No Stop, Rec, Folder, Up-dir or Arrow glyph.** Each icon is 24 px wide in Roboto Bold 17 (2 x 12) and 36 px in Liberation Bold 17. The developer-guide list (`docs/developer_guide.md:147`, which is in section 6, not section 4) is itself wrong. |
| UC-11 | Any other icon must be drawn from simple shapes or written as text | DIFFERS IN v1.0.0 | Also possible: RAM_G bitmaps (`displayStructs.h:31-87`, `display.cpp:141-214`: play 20x20, loop 20x20, double arrows, up/down arrows, battery, logo) and the JPEG `cImg` control. See section 8. |
| UC-12 | Colors are flat opaque `#RRGGBB`; the theme format rejects alpha | VERIFIED IN v1.0.0 | `core/source/ThemeManager.cpp:75-88` (exactly 7 characters, `#` then six hex digits); `docs/custom_themes_and_fonts.md:84-85`; `display.cpp:813` rejects colors above 0xFFFFFF. This is a statement about **theme colors only**. |
| UC-13 | "No alpha, gradients, shadows or blur" in drawing | DIFFERS IN v1.0.0 | Stock controls draw gradients (`bar.cpp:133-139,198-204` flag `controlStyleGradient`; `label.cpp:162-164`), bottom shadows (`bar.cpp:104-117`, `textPopup.cpp:161-181`), real alpha (`textPopup.cpp:176` `API_COLOR_A`, table at `:10`), additive blending (`lineIndicator.cpp:22`), 2 px rounded corners (`frame.cpp:146-152`, `edit.cpp:101-104`, `textPopup.cpp:146-149`). Gradients and shadows fade to black, not between theme roles. Blur: not found. See section 8. |
| UC-14 | Color roles list (background, top bar, labels, lists, bars, waveform, accents) | VERIFIED IN v1.0.0 | `docs/custom_themes_and_fonts.md:101-115`; struct `core/source/Themes.h:7-60`. Minor omissions: `topBar.text2`, waveform `activeSlice`, `slicePlayhead`, `granular`. |
| UC-15 | Three built-in themes: Original, Monochrome, Anniversary | VERIFIED IN v1.0.0 | `ThemeManager.cpp:40-42`; `display.cpp:1212-1221` (Monochrome is `themeWhiteout`). |
| UC-16 | Fixed set of controls: labels, frames, boxes, lists, bars, progress cursors, line indicators, point markers, a waveform control | DIFFERS IN v1.0.0 | Incomplete. 33 control classes exist; list in section 6a. |
| UC-17 | Waveform: min/max envelope of at most 600 points (`strTrackerSpectrum`: 600 upper, 600 lower) | VERIFIED IN v1.0.0 | `core/libs/NewDisplay/spectrumControl.h:9-11` (`int8_t upperData[600]`, `lowerData[600]`). Caveats: the control indexes the arrays by column up to the control width (`spectrum.cpp:198-204`), so a width over 600 overruns them. `init()` defaults to `k300` (`spectrumControl.h:23`); stock never uses `k600` (sample editor sets `k300` at `gSampleEdit.cpp:119-120`). The 600 columns are also 59% of the whole-screen display-list cap (section 7, item 1). |
| UC-18 | No rounded corners, curves, bitmaps or large images; custom bitmaps cost scarce memory | DIFFERS IN v1.0.0 | Rounded corners: 2 px radius exists (UC-13). Curves/circles: `POINTS` primitive used by `boxMenu.cpp:190-200`. Bitmaps: stock has 9 on MK66 (`display.cpp:141-172`) and a JPEG loader (`display.cpp:992-1066`). Bitmap data lives in the FT812's own RAM_G, not CPU RAM; on MK66 free RAM_G below the control area is 112,000..149,999 (about 38 KB; the guide's "21,220 B left" at `developer_guide.md:115` is the synth build). The genuinely scarce resource is flash (about 153 KB free, `developer_guide.md:23`). |
| UC-19 | "Keep the element count per screen low; the display list is built by the CPU" | DIFFERS IN v1.0.0 | Not built by the CPU: the CPU sends commands, the coprocessor builds the list (`display.cpp:500-535`). Real limits: **65 controls in total** (`displayStructs.h:21`; about 22 are created by the shared interface, so roughly 43 are left for a screen, my count of `createControl` call sites in `core/interface/core`), **8,192 bytes per control** (`:23`), **8,172 bytes per frame** (`display.cpp:640`). |
| UC-20 | Original has the slowest CPU in the family, 180 MHz | VERIFIED IN v1.0.0 | `platforms/mk66/makefile:44` (`-DF_CPU=180000000`); `developer_guide.md:165`. |
| UC-21 | Sends data to the screen chip byte by byte | VERIFIED IN v1.0.0 | `platforms/mk66/3rdparty/SPI/SPI.h:6-13`; `EVE_Layer.cpp:64-109`. |
| UC-22 | The developer guide's 20 to 35 ms "is a stock note, and the display research found it does not describe this code" | DIFFERS IN v1.0.0 | It does describe v1.0.0. See section 4. It is also not a "busy redraw" figure: the guide says "per step on FT812 SPI pattern redraws and Song Mode pattern reads" (`developer_guide.md:89`, not `:43`). |
| UC-23 | About 20 updates a second is roughly 3 to 10% of main-loop time | ESTIMATE | Derived from the unreleased-main pipeline. Do not cite. Replacement model in section 6 (SPI cost about 1 to 3% of CPU; the binding number is passes per frame, not CPU share). |
| UC-24 | Animate only playhead, meters, a few text readouts; stock already updates a timer widget during playback | VERIFIED IN v1.0.0 | `core/interface/core/songTimer.cpp:59` (100 ms throttle), `:78-87` (skip when unchanged). |
| UC-25 | "Waveforms are drawn once when they change" | DIFFERS IN v1.0.0 | Stock recorder rebuilds the waveform control while recording, every 100 ms and growing by 50 ms per rebuild (`fSampleRec.cpp:138-152,170`; start value `:1568`). So "a waveform growing while recording" (listed as needs-a-test) is something stock already does, at that cadence. |
| UC-26 | Motion tiers (likely fine / needs device test / unlikely) | ESTIMATE | Nothing measured. "Fades" and "slide" are not blocked by the theme rule (UC-12/13). |
| UC-27 | "A change in the length of the drawn list forces everything after it to be re-sent" (so keep element counts stable, fixed-width readouts, blink by color) | NOT FOUND IN v1.0.0 | There is no diff or re-send of unchanged words. Each queued control is rebuilt whole and every shown control is re-appended (`display.cpp:630-652`). List length does not change SPI cost. Fixed-width text is still tidy for layout but is not a performance rule here. |
| UC-28 | About 1 MB of SDRAM free, shared with recording buffers | VERIFIED IN v1.0.0 | `docs/developer_guide.md:25` (about 1,162 KB free; up to 3.5 MB more by lowering `SAMPLE_MEMORY_SIZE`). |

## 2. Design prompts

### `docs/design/claude-design-first-prompt.md`

| ID | Claim | Status | Evidence / correct statement |
| --- | --- | --- | --- |
| FP-01 | Canvas exactly 800 x 480; no touch, mouse or hover | VERIFIED IN v1.0.0 | UC-01, UC-02. |
| FP-02 | Bottom row: eight equal 100 px columns, one label per physical button | VERIFIED IN v1.0.0 | UC-03, UC-05. Note stock frames are 97 px wide on a 100 px pitch, 55 px tall at y 424. |
| FP-03 | Fonts: only 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular | DIFFERS IN v1.0.0 | UC-06. On the default font the first three slots are 14 Light, 17 Light, 14 Regular, so "14 Bold" renders as Regular and the Regular/Light distinctions the design draws are not available as stated. |
| FP-04 | Flat 4-bit grey antialiasing | VERIFIED IN v1.0.0 | UC-07. |
| FP-05 | Icons: only Pause, Stop, Play, Rec, Folder, Up-dir, Loop and Arrow exist as glyphs | DIFFERS IN v1.0.0 | UC-10. Only Pause, Play, Loop (and Perf), each two cells wide. The design export itself says it uses "Unicode stand-ins for Stop, Play, Rec, Loop and Arrow" (`four-track-recorder-design.zip`, `Four-Track Recorder.dc.html`). Stop and Rec must be drawn (rectangle; `POINTS` circle). |
| FP-06 | Color: flat opaque, no alpha, gradients, shadows, blur or rounded corners; name roles | DIFFERS IN v1.0.0 | Theme side is right (UC-12). As drawing limits they are not (UC-13). Safe as a design simplification, not as a firmware limit. |
| FP-07 | Drawing vocabulary: text, rectangles, meters, thin playhead, point markers, min/max envelope no wider than 600 columns; "No images or bitmaps" | DIFFERS IN v1.0.0 | 600-column array limit is real (UC-17). "No bitmaps" is not a firmware limit (UC-18). The vocabulary omits the per-frame budget that decides whether four 600-column lanes fit (section 7, item 1). |
| FP-08 | Motion: only playhead, meters, small text readouts (about ten a second at most); no scrolling, zooming, fades, transitions | ESTIMATE | A design policy resting on the research. Feasibility re-judged in section 6. "No fades" is not required by the code (alpha exists, UC-13). |
| FP-09 | Four tracks always visible; no vertical scrolling | VERIFIED IN v1.0.0 | Possible; see the budget caveat in section 7, item 1. |

### `docs/design/round-3-brief.md`

| ID | Claim | Status | Evidence / correct statement |
| --- | --- | --- | --- |
| R3-01 | Eight buttons, jog wheel, arrows | VERIFIED IN v1.0.0 | UC-05; arrows are `interfaceButtonUp/Down/Left/Right` (`interfaceDefs.h:148-154`). |
| R3-02 | "Buttons 6 to 8" for Cancel / Don't save / Save | VERIFIED IN v1.0.0 | Consistent with the docs' 1-to-8 numbering (UC-04/05). In code these are `interfaceButton5..7`. Keep one numbering in every brief. |
| R3-03 | "Use Shift plus the Input button" | NOT FOUND IN v1.0.0 | The Original has no physical Input button (list in UC-05). Shift exists (`interfaceButtonShift`, `interfaceDefs.h:152`). "Input" can only be a screen-button label or our own screen name; say which. |

## 3. `docs/research/display-pipeline-and-motion.md`

| ID | Claim | Status | Evidence / correct statement |
| --- | --- | --- | --- |
| RD-01 | Method: read `display.cpp`, `core/libs/FT812/DisplayList.*`, MK66 driver; line numbers "from this checkout" | DIFFERS IN v1.0.0 | `core/libs/FT812/` has only `FT812.cpp`, `FT812.h`, `FT81x.h` (no `DisplayList.*`). The cited lines (`display.cpp:153-190`, `:722-760`, `:1155-1195`...) are from unreleased main. `display.cpp` here is 1,441 lines and the cited functions do not exist. |
| RD-02 | Motion table quoted from the design zip | VERIFIED IN v1.0.0 | Checked against `Four-Track Recorder.dc.html` ("Motion layer"): sizes, rates and tiers match the doc's table. |
| RD-03 | "Screen chip redraws the whole picture every panel refresh by itself, so the CPU only sends the words that changed" | DIFFERS IN v1.0.0 | The chip re-rasterises the list each refresh (hardware). But the CPU does not diff words. Per refreshed control it sends the whole control (commands via the coprocessor), then one `CMD_APPEND` (12 bytes) per shown control for the whole screen on every swap (`display.cpp:630-655`). |
| RD-04 | Four things moving at once go out as one frame (20 frames a second, not 80) | DIFFERS IN v1.0.0 | Only if `synchronizeRefresh()` is called after queueing them (`display.cpp:924-935`; step 3 defers the swap while `stopAppend`, `:614-619`). Without it `step2()` always ends with `updateStep = 2` (`:608`), so **every control gets its own compose and swap**. Stock live updates (playhead, level bar) do not call it (`fSamplePlay.cpp:168`, `gSampleRec.cpp:245-247`). Batching costs 2 passes per control plus 1 (section 5). |
| RD-05 | The 720 x 18 px size limit does not matter to CPU or link | VERIFIED IN v1.0.0 | Cost is the commands sent, independent of pixel area (`bar.cpp`). Gradient/shadow flags add commands (`bar.cpp:104-117,133-139`). Fill rate on the chip is unmeasured but not a concern at this size. |
| RD-06 | Stock already does most of this (playhead, level bar, 100 ms timer, 500 ms blink) | VERIFIED IN v1.0.0 | See RD-37 to RD-42 for v1.0.0 line numbers. |
| RD-07 | "This tree's display code is not the old one": `display.cpp` builds whole lists on the CPU (`:153-190`, `:722-760`) | NOT FOUND IN v1.0.0 | v1.0.0 `display.cpp` is the coprocessor design (section 5). The CPU-built design is unreleased main only. |
| RD-08 | Step 1: refreshes go in a queue of 66; a handle already queued is not added twice | VERIFIED IN v1.0.0 | `display.cpp:882-922` (dedupe; except the control currently being built can be queued again, `:892-902`); queue size `controlsRefreshQueueSize = controlsCount + 1 = 66` (`displayStructs.h:21,244`; `display.h:199`). Cited lines differ. |
| RD-09 | Step 2: main loop calls `display.update()` once per pass; the SD stream runs three times per pass; audio runs in the I2S interrupt at priority 5 | VERIFIED IN v1.0.0 | `core/source/Tracker.cpp:159-168` (`updateSdStream` at `:159,161,167`; `display.update()` at `:160`; then pads, exporter, slice detector, file manager, interface, sequencer). Audio `kAudioOutputInterruptLevel = value2priority(5)`: `platforms/mk66/config/interruptsConfig.h:18`. Each `update()` call performs at most one display step (`display.cpp:466-472`). |
| RD-10 | Step 3: gate `readyForFrame()` polls `REG_DLSWAP` at most once a millisecond; one frame per panel refresh is the hard limit | NOT FOUND IN v1.0.0 | No `readyForFrame`, no `REG_DLSWAP` polling (`REG_DLSWAP` is only written at chip init, `MCU_Layer.cpp:413,549`). The gate is `isCoProEmpty()` every pass: `display.cpp:461-464`; `DisplayDriverImpl.cpp:91`; `EVE_Layer.cpp:333-355` (two SPI reads of `REG_CMD_READ`/`REG_CMD_WRITE`). Whether the coprocessor holds the FIFO until the swap completes is FT81x behaviour that is not in the repo (see RD-17). |
| RD-11 | Step 4: build on the CPU into a buffer; no SPI; stop starting builds after 1 ms (`kBuildBudgetUs`) | NOT FOUND IN v1.0.0 | Build is done by the coprocessor from commands the CPU writes to the command FIFO (`step1`, `display.cpp:475-538`: `API_CMD_DLSTART`, then `update()`). No time budget exists. |
| RD-12 | Multi-stage controls run their stages back to back in one call | DIFFERS IN v1.0.0 | Each stage is its own pass pair: `step2` sends the stage to RAM_G then sets `updateStep = 0` for the next stage (`display.cpp:563-568`). So a 5-stage control takes 10 passes. |
| RD-13 | Only `songPlayer` (3) and `tracker` (5) declare more than one stage | DIFFERS IN v1.0.0 | Also `cHorizontalTracker` (5): `horizontalTracker.cpp:111`. (`songPlayer.cpp:22`, `tracker.cpp:197` as stated.) |
| RD-14 | `cSpectrum` and `cProgressCursor` declare one stage, so `update2/3` of the spectrum are never called | VERIFIED IN v1.0.0 | `spectrum.cpp:8`, `progressCursor.cpp:7`. |
| RD-15 | Step 5: compose into a frame buffer; cap 8,172 bytes = 2,043 words; an overflowing control is dropped and counted in `overflows` | DIFFERS IN v1.0.0 | Same cap, different mechanism: `display.cpp:640` (`controlMemoryUsed + memoryUsed < 8172`, bytes of control display list). Controls are appended highest handle first (`:630`). On overflow the loop does `break` (`:647-650`), so **every lower-handle control is dropped too** (earlier-created: shared top bar, button labels, popups). No `overflows` counter; only `lastMemoryUsed` under `#if DEBUG` (`:659-664`). |
| RD-16 | Step 6: a rebuilt frame identical to what the chip shows costs no upload (so a label refreshed every pass is cheap) | NOT FOUND IN v1.0.0 | No comparison. Every `refreshControl` is a full rebuild plus (without batching) a compose and swap. The recorder refreshes its time label every pass while recording (`fSampleRec.cpp:169`; `gSampleRec.cpp:602-609`), so stock already runs a continuous rebuild-and-swap loop on that screen. |
| RD-17 | Steps 7 to 9: `findChangedRuns`, `chipLists[2][2048]`, 24 runs, 320 words per pass, swap by writing `REG_DLSWAP`, 100 ms timeout | NOT FOUND IN v1.0.0 | None exist. Swap is `API_CMD_SWAP` in step 3 (`display.cpp:654-655`; `core/libs/FT812/FT812.cpp:480-484`); no timeout. Hardware fact needed but not in repo: how long the coprocessor blocks after `CMD_SWAP` (assumed up to one panel period). |
| RD-18 | Step 10: panel timing 1056 x 525 clocks, divider 2, 54 Hz, 18.5 ms (if the chip clock is 60 MHz) | ESTIMATE | Registers verified: `MCU_Layer.cpp:294,299,304` (`lcdHcycle 1056`, `lcdVcycle 525`, `lcdPclk 2`). The 60 MHz chip clock is the FT812 default; the repo never sets `REG_FREQUENCY`/CLKSEL (`MCU_Layer.cpp:195-196` has CLKEXT commented out). 30 MHz / 554,400 = 54.1 Hz. |
| RD-19 | Memory behind it: arena 57,344 B, 8 KB build, 8 KB frame, 16 KB shadows, about 88 KB SDRAM | NOT FOUND IN v1.0.0 | v1.0.0 keeps control fragments in the chip's RAM_G: 65 slots x 8,192 B from address 150,000 (`displayStructs.h:21-23`). CPU-side display buffers: font decompress 22,396 B (`display.cpp:83`) and logo 27,520 B (`:82`). |
| RD-20 | Link: hardware SPI, one byte at a time, no buffer, no DMA, CPU busy for the whole transfer; audio ISR can pre-empt | VERIFIED IN v1.0.0 | `SPI.h:6-13`; `EVE_Layer.cpp:64-109`. Nuance: DMA code exists in `DisplayDriverImpl.cpp:133-290` but only for a full-frame video stream (`stream.cpp`), which nothing in the UI calls. Display lists never use it. |
| RD-21 | SPI clock is 20 or 30 MHz (code disagrees); use 20 as the cautious case | DIFFERS IN v1.0.0 | **30 MHz.** `SPI_start` (`SPI.cpp:29-39`) sets SCK = bus clock / 3. The 180 MHz default bus clock in `kinetis.h` is 90 MHz (`kinetis.h:790-793`; the 60 MHz line is commented out), and `mk20dx128.c:959-960` and `:1295-1296` handle 90 MHz. 90 / 3 = 30 MHz, matching `MCU_Layer.cpp:92-93` (`baudrateFast`, `spiSourceClock`; the speed argument is ignored, `MCU_Layer.cpp:577-580`). The research took the first `#if` branch (60 MHz) at `mk20dx128.c:955-958`. 30 MHz is also the FT812's maximum SPI rate (datasheet, not in repo), so there is no margin. The init-time rate is slower (about 4.5 MHz, `SPI.cpp:18-22`), irrelevant here. |
| RD-22 | `writesDisplayListInBackground()` defaults to false; only IMX turns it on | NOT FOUND IN v1.0.0 | The symbol does not exist (`core/include/DisplayDriver.h:9-37` has no such method). The effect (blocking CPU writes on MK66) is true. |
| RD-23 | Cost per word about 2 to 2.4 us at 20 MHz, about 1.4 us at 30 MHz; chip-select 10 nops | ESTIMATE | At 30 MHz a byte is 0.27 us on the wire plus loop overhead, my estimate 0.35 to 0.4 us per byte, so 1.4 to 1.6 us per 32-bit word. Chip select: `MCU_Layer.cpp:452-492` (10 nops each edge). **Missed:** every command burst also has two fixed 3 us delays (`API_Layer.cpp:69,80,96`; `MCU_Delay_3us`, `MCU_Layer.cpp:514-517`) and every pass starts with two register reads. |
| RD-24 | Typical frame costs about 1 to 2 ms of CPU (compose, memcmp, upload); worst frame 6 to 7 ms; wall time 1.5 to 2.5 times that | ESTIMATE | Built on the unreleased pipeline (compose/compare/upload). Not applicable. Replacement in section 6. |
| RD-25 | Panel ceiling 54 frames a second; 20/s is 37% of it; quantisation jitter of about one panel period | ESTIMATE | Arithmetic follows from RD-18; the ceiling only binds if `CMD_SWAP` holds the coprocessor to the next frame (unverified). |
| RD-26 | Main-loop pass length is unknown and the largest open number | VERIFIED IN v1.0.0 | Still true, and **more critical** here because a frame needs 2n + 1 passes (section 5). |
| RD-27 | Expensive: a list that changes length shifts everything after it (playhead at loop wrap, text length, blinks that vanish) | NOT FOUND IN v1.0.0 | No such mechanism (UC-27). `progressCursor.cpp:62-70` does draw nothing at 0 or at the width, but it only changes that one control's size. |
| RD-28 | Text costs about 4 setup words plus 1 per character (`DisplayList.cpp:63-85`) | NOT FOUND IN v1.0.0 | File absent. Text is `API_CMD_TEXT` through the coprocessor (`FT812.cpp:274-299`); its expansion into display-list words is FT81x behaviour, about one word per character plus setup is plausible but unverified. |
| RD-29 | Waveforms are the heavy control: type 0 emits two vertices per point; 600 points is about 1,200 words, 59% of the cap; two channels at 600 would not fit | VERIFIED IN v1.0.0 | `spectrum.cpp:199-204` (two `API_VERTEX2F` per point). 600 x 8 B = 4,800 B = 59% of 8,172; two = 9,600 B, over. |
| RD-30 | Stock playback uses 200 or 300 points | DIFFERS IN v1.0.0 | Default is 300 (`spectrumControl.h:23`); sample editor 300 (`gSampleEdit.cpp:119-120`). No call site uses 200 or 600. |
| RD-31 | Recorder redraws its spectrum every 100 ms, growing by 50 ms each time | VERIFIED IN v1.0.0 | `fSampleRec.cpp:138-152,170,1568`. |
| RD-32 | Gradients and shadows use extra words (`cBar`) | VERIFIED IN v1.0.0 | `bar.cpp:104-117,133-139`. |
| RD-33 | Playhead: position re-read at most every 5 ms (`PLAY_REFRESH_US = 5000`) | VERIFIED IN v1.0.0 | `fSamplePlay.cpp:86,2564`; `fSampleRec.cpp:22,2828`. Refresh sites `fSamplePlay.cpp:165-170`, `fSampleRec.cpp:153-157`. Each is its own queued control, so each is its own swap unless batched. |
| RD-34 | Level bar: `changeLevelBar()` redraws only when the value changes, 350 ms red hold | VERIFIED IN v1.0.0 | `fSampleRec.cpp:277,2439-2446`; `gSampleRec.cpp:245-247,716-722`. |
| RD-35 | Song timer: throttled to 100 ms, skipped when unchanged, so at most 10/s | VERIFIED IN v1.0.0 | `core/interface/core/songTimer.cpp:59,78-87`. |
| RD-36 | Pattern playhead blink every 500 ms | VERIFIED IN v1.0.0 | `core/interface/patternEditor/fPatternEdit.cpp:169,278-280`. |
| RD-37 | "Recording time text refreshed every pass; only a changed string reaches the chip" | DIFFERS IN v1.0.0 | First half true (RD-16); second half false: every refresh is a rebuild and swap. |
| RD-38 | Verdict: at most four things moving is feasible (medium-high) | ESTIMATE | Judged on the wrong pipeline. Re-judged in section 6: plausible only if the moving items are one or two controls and refreshes are batched. |
| RD-39 | Verdict: meters (one 720x18 bar + marker; five small bars) at up to 20/s likely feasible | ESTIMATE | As RD-38. Five bars as five controls is 11 passes per frame; as one custom control it is 3. |
| RD-40 | Verdict: playhead feasible and best supported; use a fixed-length list so a loop wrap does not re-send | ESTIMATE | Feasible; the fixed-length advice is moot (UC-27). |
| RD-41 | Verdict: save/export progress "trivial; stock calls `processDisplayUpdateImmediate()` before long jobs (`display.cpp:1228-1248`)" | VERIFIED IN v1.0.0 | Function exists at `display.cpp:965-983`; used at `fSampleRec.cpp:196`, `gProjEdit.cpp:750`, `fSongEdit.cpp:1492,1675`. Lines differ. Still needs a device check. |
| RD-42 | Combined worst moment: 150 to 250 changed words, 0.4 to 0.6 ms SPI per frame | ESTIMATE | "Changed words" is not a v1.0.0 concept. About 10 to 12 controls changing is 21 to 25 passes per frame (section 6). |
| RD-43 | Device-test item 5: check `displayList::overflows` or `droppedCommands` | NOT FOUND IN v1.0.0 | No such counters. Only `lastMemoryUsed` under `#if DEBUG` (`display.cpp:659-664`). |
| RD-44 | "Fades: the theme format rejects alpha, so I read nothing that says they are feasible" | DIFFERS IN v1.0.0 | Theme rejects alpha (UC-12) but the firmware draws alpha today (`textPopup.cpp:176`). A fade is a control rebuilt per step with `API_COLOR_A`: same cost class as any other control update. |
| RD-45 | Audio load 35 to 60% of CPU (from `cpu-ram-budget.md`) | ESTIMATE | Out of scope; stated as an estimate in its own document. |

### Tally (all three sections)

See the final line of this file for the machine-counted totals.

## 4. The developer guide's "20 to 35 ms per step": which is right?

**Exact text** (`docs/developer_guide.md:89`, inside the Hybrid SD Streaming platform-differences bullet): "the 180 MHz Cortex-M4 spends ~20-35 ms per step on FT812 SPI pattern redraws and Song Mode pattern reads". It is a justification for keeping Slice instruments resident in RAM on MK66, not a measurement.

**Verdict: it describes v1.0.0's display code; the research was wrong to say it does not.**
- The research's reason ("this tree builds display lists on the CPU, so the old figure no longer applies") rests on unreleased main. The guide sits in the v1.0.0 tree and is unchanged from the tag. The code it talks about is the coprocessor pipeline in section 5.
- Is the number plausible for that code? Yes, as an order of magnitude for a **whole pattern view redraw**. The pattern control has 5 stages (`tracker.cpp:197`), the song player 3 (`songPlayer.cpp:22`). Each stage is one pass to build and one to copy (`display.cpp:563-568`); then one pass to compose; then the swap wait (up to one panel period, about 18.5 ms, ESTIMATE). A pattern screen change with several other controls is roughly 11 to 25 passes. At a pass of 1 to 3 ms that is 11 to 75 ms, so 20 to 35 ms is consistent, and the swap wait alone is about one 18.5 ms period.
- What it is **not**: a cost for one small control. A small batched update is 5 to 11 passes (section 6). Nor is it evidence about SPI bandwidth.
- Nothing in the repo measures it (`docs/readiness-matrix.md:44`: "Redraw cost unmeasured"; no profiler hook for display). Status: ESTIMATE written by Polyend, never measured by us, and applicable to v1.0.0.
- Useful reading: if a pattern redraw takes 20 to 35 ms and needs about 11+ passes, a pass is plausibly 2 to 3 ms. That is the only hint of the main-loop pass length we have, and it is the number the motion table depends on.

## 5. How a frame reaches the screen in v1.0.0 (what the code does)

1. A screen sets control state and calls `display.refreshControl(handle)`; the handle joins a FIFO of 66 (`display.cpp:882-922`).
2. `TrackerImpl::update()` calls `display.update()` once per main-loop pass (`Tracker.cpp:160`), between SD-stream calls. `update()` returns at once unless the coprocessor FIFO is empty (`display.cpp:461-464`), then runs **one** of three steps (`:466-472`).
3. **step1** (`:475-538`): sends `CMD_DLSTART` plus the control's drawing commands (one stage) to the coprocessor FIFO. The coprocessor writes the list into the chip's RAM_DL.
4. **step2** (`:540-610`): reads `REG_CMD_DL` for the list size, sends `CMD_MEMCPY` to copy RAM_DL into this control's RAM_G slot (`150000 + 8192 * handle`, `:547-556`). Next stage or, when the control is done, `updateStep = 2`.
5. **step3** (`:612-668`): sends a fresh list: clear, then one `CMD_APPEND` (12 bytes) per shown control, highest handle first, stopping at 8,172 bytes (`:630-652`), then `DISPLAY` and `CMD_SWAP` (`:654-655`). If `synchronizeRefresh()` was called, step 3 is skipped until the queue has drained to the marked position (`:614-619`, `:591-594`).
6. The FT812 re-rasterises the active list every panel refresh: 54 Hz (RD-18).

Consequences:
- Passes per frame = **2n + 1** for n single-stage controls batched with `synchronizeRefresh()`; **3** per control when not batched (and then one swap each).
- CPU cost per pass is small (tens of microseconds of SPI at 30 MHz plus the 2 x 3 us delays). The latency is passes times the main-loop pass length `p`, which nobody has measured.
- SPI bytes for a typical small control (ESTIMATE): step 1 about 60 to 100 bytes, step 2 about 40 bytes, step 3 about 16 + 12 x (shown controls) bytes. With about 45 shown controls step 3 is about 560 bytes, about 0.2 ms.
- The 8,172-byte cap is on the whole screen's display list, bytes of already-built control lists (`ramSize`).

## 6. Motion table re-judged for v1.0.0

### 6a. The 33 control classes in `core/libs/NewDisplay` (answer to "list them")

Text and boxes: `cLabel` (commonControls.h:65), `cEdit` (:98), `cFrame` (:119), `cBgLabel` (:137), `cSimpleText`, `SimpleBox`, `cTextBox`, `cMultiLabel`, `cTextPopup`, `cNotePopout`, `cStartScreen`.
Bars and cursors: `cBar` (level/value bar with optional gradient and shadow), `cHorizontalBar` (progress bar with text), `cProgressCursor` (1 px line), `cLineIndicator` (line plus additive selection rectangle), `cWtProgress`, `cBattery`.
Waveforms and markers: `cSpectrum` (envelope or polyline), `cPoints`, `cSlicePoints`.
Lists and menus: `cList`, `NewList` (image list), `cParamValueList`, `cBoxMenu`, `cKeyboard`, `cNotesKeyboard`, `cGridEditorPopup`.
Large: `cTracker` (pattern grid, 5 stages), `cHorizontalTracker` (5), `cSongPlayer` (3), `cImg` (JPEG/bitmap), `ImgGrid`, `cArrangement` (compiled out unless `TRACKER_ARRANGEMENT`, `arrangement.cpp:1`).
Source: `core/libs/NewDisplay/display.h:10-38`, `displayStructs.h:141-182`. A new control is a subclass of `cDisplayControl` (`displayControls.h:28`); we write all C++.

### 6b. Feasibility of the motion table

Table items: at most four things moving; input meter 720 x 18 px plus marker and four track bars plus master (up to 20/s); playhead line and cap across four lanes (up to 20/s); text readouts at most 10/s.

**What limits it in v1.0.0:** not CPU share (SPI is about 1 to 3% at 20 frames/s, ESTIMATE) and not the panel ceiling alone, but the number of passes per frame times the main-loop pass length `p`:

| Moving controls per frame (n, batched) | Passes (2n + 1) | Frame time at p = 1 ms | p = 2.5 ms | p = 5 ms | p = 10 ms |
| --- | --- | --- | --- | --- | --- |
| 2 | 5 | 5 ms | 12.5 ms | 25 ms | 50 ms |
| 5 | 11 | 11 ms | 27.5 ms | 55 ms | 110 ms |
| 10 | 21 | 21 ms | 52 ms | 105 ms | 210 ms |

20 updates a second needs a frame time of 50 ms or less, plus the swap wait (up to about 18.5 ms, may overlap). Reading: with 2 to 5 controls it holds if `p` is under about 4 ms; with 10 controls only if `p` is under about 2.4 ms. **Unbatched, ten controls cannot reach 20/s** (about 3 passes and one swap each).

**Verdict (ESTIMATE, confidence low to medium):**
- *Feasible if built as designed for this pipeline:* merge everything that moves into one or two custom controls (for example one "meters" control drawing all five bars and peak markers, one "playhead" control), refresh them together and call `display.synchronizeRefresh()` once per cycle (our `fFourTrack.cpp:477-482,514-515` already batches). That is 5 passes per frame.
- *Not feasible as naive stock-control composition:* five bar controls, a cursor and two text labels is about 8 controls, 17 passes, and only if the pass is under about 3 ms.
- Text readouts at 10/s or once per beat: fine (single label, 3 passes; stock song timer does the same, `songTimer.cpp:59`).
- Waveform progress colour change at the playhead (the design colors the envelope behind the playhead differently, `waveformProgress`): this means rebuilding or double-drawing the lane waveform on every playhead step, thousands of bytes per update. Draw the playhead as its own line and drop the two-color split, or split by scissor over a once-built waveform.
- Evidence it can work: stock already runs rebuild-and-swap loops with audio playing: the recorder's time label every pass (`fSampleRec.cpp:169`), the level bar and playhead per pass (`fSampleRec.cpp:153-157,277`), with audio in an interrupt. Our own Four-Track screen already runs on the owner's device with batched text refreshes (`fFourTrack.cpp:450-515`). None of this is measured.
- Evidence against certainty: no measurement of `p`, of coprocessor stall at `CMD_SWAP`, or of SPI timing exists anywhere in the repo.

## 7. Things this got wrong or missed (anything that forces a redesign)

1. **Per-frame display-list cap of 8,172 bytes is missing from `ui-constraints.md`, and four 600-column lanes do not fit.** One envelope column is 2 vertices = 8 bytes (`spectrum.cpp:199-204`). 600 columns = 4,800 B per lane. Four lanes = 19,200 B against 8,172 B for the whole screen (`display.cpp:640`). At 300 columns a lane is 2,400 B (four = 9,600, over). At 200 columns it is 1,600 B (four = 6,400 B, leaving about 1,770 B for top bar, eight button labels, track text and meters: probably too tight). Practical envelope budget is about 100 to 150 columns per lane (about 3,200 to 4,800 B total), i.e. 4 to 6 px wide bars. Overflow does not fail loudly: the first control that does not fit stops the loop and all earlier-created controls (top bar, button labels, popups) vanish (`display.cpp:647-650`). The design (`Four-Track Recorder.dc.html`: four lanes x 600 columns, `Math.round(s.playBeat / 16 * 600)`) must be redrawn with coarser columns, a polyline waveform (`spectrumType == 1`, 1 vertex per point, `spectrum.cpp:79-170`), a pre-rendered bitmap per lane (section 8), or lanes that show waveforms only for the selected track.
2. **Font slots 1 to 3 are not what the design assumes on the default font** (UC-06). Design uses 14 Bold for status text; on "Original" font it is Regular. Light weights appear instead where the design says Regular. Widths: about 9 to 10 px per character at 14/17 Roboto Mono, so a 97 px button cell holds about 9 to 10 characters, and the glyph pairs are 24 px wide.
3. **No Stop or Rec glyph, and no Folder, Up-dir or Arrow glyph.** The design's `■ ▶ ● ↻ →` are browser stand-ins. Stop and Rec must be drawn shapes (rectangle, `POINTS` circle); Pause/Play/Loop exist as two-cell pairs; the rest as text or bitmaps (UC-10).
4. **The research's central motion argument is invalid for v1.0.0** (RD-03, RD-04, RD-10 to RD-17, RD-27). Its "four things cost one frame", "CPU sends only changed words" and "list length shifts re-send" arguments do not apply. The honest cost model is passes per frame (section 6b) and depends on batching.
5. **SPI is 30 MHz, not the 20 MHz "cautious case"** (RD-21). That is faster than the research assumed, so its per-word cost is too pessimistic; but SPI bandwidth was never the binding limit (section 6b), so this does not rescue the pipeline argument.
6. **`docs/info_viewer.md` does not exist** (UC-04), and the dev-guide glyph list that `ui-constraints.md` cites is itself wrong (UC-10).
7. **Total font size must fit the chip's RAM_G font region, not just 22,396 per font** (new). Fonts are packed from address 0 (`display.cpp:259-261,1321-1326`) with the logo bitmap at 80,000 and icons at 110,000 (`display.cpp:143-172`). The built-in "New" set already uses about 79,000 B (13,448 + 18,388 + 13,448 + 22,396 + 11,368, plus alignment). Five custom fonts at the cap (111,980 B) would overwrite the logo and icons; nothing checks the total (`display.cpp:1288-1331`). Matters only if a custom font is ever shipped.
8. **No "Input" physical button** for the round-3 shortcut (R3-03).
9. **Button numbering mismatch:** code 0 to 7, docs and briefs 1 to 8 (UC-05).
10. **The "frame" does not equal one swap per refresh-batch unless `synchronizeRefresh()` is used**; anything we write must call it (we already do). Easy to forget for a meter or playhead updated from its own timer.

## 8. Limits we declared that the code does not actually impose

| Declared limit | What v1.0.0 has | Evidence |
| --- | --- | --- |
| No alpha | `API_COLOR_A` / `API_CLEAR_COLOR_A` wrappers exist and stock uses alpha for popup shadows | `core/libs/FT812/FT812.cpp:160-170`; `textPopup.cpp:10,176` |
| No gradients | Vertical and horizontal gradients (fade to black), scissored | `bar.cpp:111-113,136-138`; `label.cpp:162-164`; `horizontalTracker.cpp:329,412,421`; style flags `controlStyleGradient`, `controlStyleBottomShadow` (`displayStructs.h:125,134`) |
| No shadows | Bottom shadow on bars, labels, popups | `bar.cpp:104-117`; `textPopup.cpp:161-181` |
| No rounded corners | 2 px radius via `controlStyleRoundedBorder` | `displayStructs.h:116`; `frame.cpp:146-152`; `edit.cpp:101-104` |
| No transparency or overlays | Additive translucent selection rectangle; "no transparency" is a style opt-in | `lineIndicator.cpp:22-30`; `displayStructs.h:117` |
| No curves | Round dots via `POINTS` | `boxMenu.cpp:190-200` |
| No bitmaps or images | RAM_G bitmaps with a 4-line table entry each; JPEG load from SD; full-frame bitmap stream | `display.cpp:141-214,992-1066`; `stream.cpp`; `imgControl.h`. Free RAM_G on MK66 about 38 KB (112,000..149,999). A pre-rendered lane waveform bitmap is possible but is new code and flash/RAM_G cost |
| Rotation, scale, bitmap transforms | Vertical text via `CMD_ROTATE/TRANSLATE/SETMATRIX`; bitmap transform in `cImg` | `label.cpp:205-228`; `img.cpp:119,142` |
| Waveform is one stock control | Any new subclass of `cDisplayControl` can emit any display commands | `displayControls.h:28`; `display.h:66-88` (`createControl<T>`) |
| No fades or short slides | Possible as rebuild-per-step of one control (same cost class as a meter); not a code limit | RD-44; section 6 |
| Only five fonts, only the listed sizes | True for slot count; custom `.ptfont` replace all five, with the per-font and total-size limits above | `display.cpp:1247-1361` |
| "Rec and Stop icons exist" | The opposite (this one is a limit we **assumed** that does not exist) | UC-10 |

Note: none of these are free. Each costs display-list bytes against the 8,172 B cap, and bitmaps cost flash (about 153 KB free) and RAM_G.

## 9. Corrected hard-limits block for the design tool (suggested replacement for `ui-constraints.md` sections 2 to 5)

- Fonts: five slots. Default font set is Roboto Mono: slot 1 Light 14, slot 2 Light 17, slot 3 Regular 14, slot 4 Bold 17, slot 5 Regular 12; about 9 to 10 px per character. The optional "New" set is Liberation Sans: Regular 14, Regular 17, Bold 14, Bold 17, Regular 12 (proportional). Design for the default.
- Glyphs: Pause, Play, Loop and Perf only, each two character cells (24 px) wide, in slot 4. Stop and Rec are drawn shapes.
- Colour: theme colours are flat `#RRGGBB`. The firmware can fade to black with gradients and has translucent shadows, but custom themes cannot define them; prefer flat.
- Whole-screen budget: 8,172 bytes of display list. Waveform envelope costs 8 bytes per column; four lanes together should use about 100 to 150 columns each.
- Controls: at most 65 in total, about 43 left for a screen.
- Eight screen buttons, numbered 1 to 8 left to right in docs (code 0 to 7); no Input button.
- Motion: batch all moving items into one or two controls and refresh them together; 20 per second is plausible, unmeasured.

## 10. What a device test must measure (v1.0.0-specific)

1. Main-loop pass length `p` on the Four-Track screen with four Parts playing and one recording (time between `display.update()` calls).
2. Frames per second actually reached with 2, 5 and 10 controls refreshed with `synchronizeRefresh()`.
3. Whether the swap wait blocks the coprocessor (time from `CMD_SWAP` to the next `isCoProEmpty() == true`).
4. Whether the screen loses its top bar or button labels when the total display list nears 8,172 bytes (log `lastMemoryUsed`, which needs a DEBUG build, or add a counter in `step3`).

Scratch files used for decoding fonts and unzipping the design export are in the scratchpad directory only; no repository file was modified.

**Machine-counted tally (85 claim rows): VERIFIED 37, DIFFERS 23, NOT FOUND 13, ESTIMATE 12.**
