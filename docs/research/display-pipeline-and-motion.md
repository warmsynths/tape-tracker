# Display pipeline and motion on the Original Tracker (MK66)

Ticket: `wayfinder/tickets/17-research-display-pipeline-and-animation.md`.

Method: read-only reading of `core/libs/NewDisplay/display.cpp`, `core/libs/FT812/DisplayList.*`, the MK66 display driver and FT812 layer, and the stock screens that animate. Nothing was built or run. Every time and rate below is my own arithmetic from the code and is labelled "est." Line numbers are from this checkout. The motion table is quoted from `docs/design/four-track-recorder-design.zip` (`Four-Track Recorder.dc.html`, section "Motion layer").

## Headline

- **The design's motion table looks feasible on the Original, confidence medium.** At 20 screen updates a second, the display work is about 3 to 10 percent of the main loop's time (est.), and 20 a second is about 37 percent of the screen's hard ceiling of about 54 updates a second.
- Two reasons it is easier than it sounds. The screen chip redraws the whole picture every panel refresh by itself, so the CPU only sends the words that changed. And everything that moves in the same moment goes out as one frame, so four moving things at 20 per second cost 20 frames a second, not 80.
- The "720 x 18 px" size limit does not matter to the CPU or the link. Cost depends on how many display-list words change, not how many pixels move.
- Stock already does most of this during playback and recording (playhead, level bar, a 100 ms timer, a 500 ms blink). Nobody has measured it on the Original.
- What this cannot tell you: real frame time, how the main loop's other work (SD streaming, sequencer) delays frames, and whether audio glitches. Only a device test answers those (last section).

## Important caveat: this tree's display code is not the old one

`display.cpp` here builds whole display lists on the CPU and writes them to the chip's list memory over SPI (`display.cpp:153-190`, `:722-760`). `docs/developer_guide.md:43` still talks about "~20-35 ms per step on FT812 SPI pattern redraws". That figure describes pattern redraws and Song Mode reads, and the repo holds no measurement for the current drawing code. Do not rely on it for this design. I could not tell from the shallow git history whether the rewrite is the fork's own or upstream's (one commit visible).

## The pipeline, step by step

1. **Something changes.** Screen code calls `setControlValue`, `setControlText` and so on, then `display.refreshControl(handle)`. That puts the control's handle in a queue (`display.cpp:1155-1195`, queue size 66, `displayStructs.h:21,225`). A handle already in the queue is not added twice, so many refreshes between frames collapse into one entry per control.
2. **The main loop calls `display.update()` once per pass** (`core/source/Tracker.cpp:162-179`, call at `:170`). The same pass also runs the SD stream three times, then pad lights, exporter, slice detector, file manager, interface, sequencer. Audio is not in this loop: it runs in the I2S interrupt at priority 5 and pre-empts the main loop (`docs/research/cpu-ram-budget.md`, fixed facts).
3. **Gate: is the chip free?** `readyForFrame()` (`display.cpp:686-716`). A swap issued earlier "waits for the panel's next frame, up to 20 ms away". The code asks the chip (`REG_DLSWAP`, one short SPI read, `DisplayDriverImpl.cpp:140-143`) at most once per millisecond. No new frame starts until the previous swap has happened. **This is the pipeline's hard limit: one frame per panel refresh.**
4. **Build** (`renderFrame`, `display.cpp:722-760`; `buildControl`, `:763-799`). Each queued control runs `update()`, then `update2()`, `update3()` and so on if it has more than one stage, writing display-list words into a CPU memory buffer. No SPI is used here. New builds stop being started after 1 ms in one pass (`kBuildBudgetUs`, `:162`). The words are stored in the "arena" (below).
   - In this tree the stages run back to back in one call (`display.cpp:769-784`); they are not spread over later passes. Only `songPlayer` (3 stages, `songPlayer.cpp:29`) and the pattern tracker (5 stages, `tracker.cpp:199`) declare more than one. `cSpectrum` and `cProgressCursor` declare one (`spectrum.cpp:8`, `progressCursor.cpp:8`), so the `update2()` and `update3()` written on the spectrum control (`spectrum.cpp:214,248`) are never called in this tree.
5. **Compose** (`composeFrame`, `display.cpp:801-831`). Copies the stored words of every shown control into one frame buffer, newest control first (`:810`), with a 3-word header and a trailer. Cap: 8172 bytes of control words, which is 2043 words (`:157`). A control that does not fit is silently dropped and counted in `overflows` (`:818-822`).
6. **Skip if nothing changed** (`showFrame`, `display.cpp:833-849`). The new frame is compared with what the chip is showing. Controls that rebuild to an identical picture cost no upload. This matters: the sample recorder refreshes its time label on every main-loop pass while recording (`fSampleRec.cpp:184`, `gSampleRec.cpp:602-610`), but only a changed string costs SPI.
7. **Find what differs** (`findChangedRuns`, `display.cpp:855-895`). The chip has two list buffers. The code remembers what each holds (`chipLists[2][2048]`, `:187`) and compares the new frame with the one that is not on screen. Stretches closer than 6 equal words are sent as one, because starting a new transfer costs about as much as 4 words (`:851-856`). Up to 24 runs per frame (`display.h`, `kMaxUploadRuns`).
8. **Upload** (`continueUpload`, `display.cpp:899-951`). Writes the changed runs into the hidden list buffer, at most 320 words per main-loop pass (`kUploadWordsPerPass`, `:161`; the comment says "under half a millisecond"). Sending starts on the pass after the build (`renderFrame(kBuildBudgetUs, 0)`, `:632`). MK66 has no background transfer, so the CPU does the sending itself (see the link section).
9. **Swap.** After the last run, one SPI write to `REG_DLSWAP` (`DisplayDriverImpl.cpp:135-138`; the call is at `display.cpp:943-946`) tells the chip to switch to the new list at the panel's next frame. The buffer roles flip (`backList ^= 1`). Then the gate in step 3 waits for the swap to happen. If the panel is off there is no next frame, so waits time out after 100 ms (`:428`).
10. **The chip draws.** The FT812 re-rasterises the current display list for every panel refresh on its own. Panel timing, `MCU_Layer.cpp:291-304`: 1056 x 525 clocks per frame, pixel clock divider 2. If the chip's clock is 60 MHz that is 30 MHz / (1056 x 525) = **54 Hz, 18.5 ms per panel refresh** (est.; the chip's clock setup is not in the code I read). The handover's "about 20 ms" agrees.

### The memory behind it (all in external SDRAM, `display.cpp:179-187`)

| Buffer | Size |
| --- | --- |
| Arena (every control's stored list) | 14336 words = 57,344 B (`:173-177`) |
| Build buffer (one control) | 2048 words = 8 KB |
| Frame buffer | 2048 words = 8 KB |
| Chip-list shadows (two) | 2 x 2048 words = 16 KB |
| Total | about 88 KB (derived) |

The arena packs lists back to back. A list that grows moves to the end, and the arena is repacked when full (`display.cpp:192-290`).

## The link to the screen chip

- **Hardware SPI, one byte at a time, no buffer, no DMA.** `SPI_transfer` (`platforms/mk66/3rdparty/SPI/SPI.h:6-13`) clears a flag, pushes one byte, and busy-waits until it is sent. A 32-bit word is four such calls (`EVE_Write32`, `EVE_Layer.cpp:98-109`). The CPU is occupied for the whole transfer. The audio interrupt can pre-empt it; nothing else I found touches this SPI port (the only other "SPI2" hit is a text label, `Profiler.cpp:158`).
- **No background write on MK66.** `writesDisplayListInBackground()` defaults to false (`core/include/DisplayDriver.h:42`); only the IMX platform turns it on (`platforms/imx/source/DisplayDriverImpl.h:42`). So `continueUpload` always takes the blocking path (`display.cpp:915-934`).
- **The clock speed is unclear in the code.** `MCU_Layer.cpp:92` says `baudrateFast = 30000000`, but `SPI_start` ignores its speed argument (`SPI.cpp:27-39`) and sets fixed dividers: SCK = source clock x 2 / (3 x 2) = source / 3. With the source clock the file assumes (90 MHz, `MCU_Layer.cpp:93`) that is 30 MHz. With the 60 MHz bus clock the bundled Teensy startup file sets for 180 MHz (`teensy_core/mk20dx128.c:955-958`) it is 20 MHz. I did not confirm which startup file is linked. **I use 20 MHz as the cautious case.** The FT812's SPI limit is 30 MHz, so either is in range.
- **Cost per word (est.).** 8 bits at 20 MHz is 0.4 us; with the push, wait and read overhead per byte, say 0.5 to 0.6 us per byte, so **about 2 to 2.4 us per 32-bit word**. At 30 MHz, about 1.4 us. 320 words per pass is therefore about 0.65 to 0.75 ms at 20 MHz, a bit above the code comment's "under half a millisecond", which fits 30 MHz.
- Each run of changed words also costs 3 address bytes and chip-select toggling, about 4 words' worth (`display.cpp:851-856`). Chip select has a 10-nop delay on each edge (`MCU_Layer.cpp:471-492`), about 0.06 us each at 180 MHz.
- A swap poll is a short read of a few bytes, at most once per millisecond.

## Estimate: how many small updates a second

Define one "update" as one frame holding all small changes made since the last frame.

**Cost of one typical frame (est., 20 MHz SPI):**

| Step | Arithmetic | Time |
| --- | --- | --- |
| Build 3-6 small controls | tens of microseconds each, my guess | 0.1 to 0.3 ms |
| Compose: copy up to 8 KB, SDRAM to SDRAM | guess | 0.3 to 0.8 ms |
| Compare with chip image (memcmp) | 8 KB read, guess | 0.2 to 0.4 ms |
| Find changed runs: up to 2043 word compares | about 10 cycles each = 20k cycles at 180 MHz | about 0.15 to 0.3 ms |
| Upload: a playhead line, 5 meter bars, a text readout; say 10 runs of about 6 changed words plus 4 words of start-up each | 100 words x 2.2 us | about 0.25 ms |
| Swap write, poll reads | few bytes | under 0.05 ms |
| **Total, display work per frame** | | **about 1 to 2 ms** |

The compose and compare costs are the same however little changed. They depend on SDRAM speed, which I could not read from the code, so the 1 to 2 ms range is a guess and the biggest uncertainty after the audio load.

**Worst-case frame:** everything shifts or the screen changes completely: 2043 words x 2.2 us is about 4.5 ms of SPI, plus 1 to 2 ms for the rest, so **about 6 to 7 ms**. Stock screen changes land here.

**Audio takes CPU from the main loop.** `docs/research/cpu-ram-budget.md` estimates audio at 35 to 60 percent of the CPU for the four-track workload (itself an estimate). The main loop therefore gets 40 to 65 percent, so wall-clock time stretches by 1.5 to 2.5 times:

- typical frame: about 1.5 to 5 ms of wall time
- worst frame: about 10 to 17 ms of wall time

**Capacity:**

- Ceiling from the panel: **54 frames a second**, one per 18.5 ms. A frame's work must finish before the next panel refresh to make that one. A typical frame does; a worst frame on a loaded CPU is close to the limit.
- Ceiling from CPU time, if the display may use at most a quarter of the main loop (my choice): 250 ms per second / 1.5 to 5 ms = 50 to 170 frames a second. So the panel is the binding limit, not the CPU.
- The main loop also does other work in each pass. A frame needs at least two passes (build, then upload), one more per 320 words (`display.cpp:161,632`), plus passes spent waiting for the swap. If a pass averages 5 ms, one frame's cycle is 10 to 15 ms, which still allows 20 Hz with room. If a pass averages 12 ms or more, 20 Hz is lost. **Pass length is unknown** (SD streaming runs three times per pass, `Tracker.cpp:169-177`). This is the largest open number.

**Result:** the design's **20 updates a second is about 37 percent of the 54 Hz ceiling, and costs 20 x (1.5 to 5 ms) = 30 to 100 ms per second, i.e. 3 to 10 percent of main-loop time** (est.). A realistic sustained range for small updates is **20 to 40 a second, with 54 as the hard ceiling.**

**Quantisation.** A frame can only go out on a panel refresh (18.5 ms). A 20 Hz target (50 ms) lands on every 2nd or 3rd refresh: 37 or 55 ms. So the rate averages correctly but gaps jitter by about one panel period. That is visible on a playhead; it is not a speed problem.

**Why four moving things cost nearly one.** The queue holds each control once (`display.cpp:1155-1195`) and a frame builds everything queued (`:728-756`). While the previous swap is pending, nothing starts, so refreshes accumulate and then go out as one frame. The cost per frame depends on the number of changed words, not the number of things.

### Things that make a frame expensive (design implications)

1. **A list that changes length shifts every word after it.** The frame is compared word by word (`findChangedRuns`, `display.cpp:855-895`) and controls are laid out one after another (`:810-826`). Anything laid out after a control that grew or shrank looks "changed" and is re-sent: up to about 4.5 ms of SPI.
   - `cProgressCursor::update()` draws nothing when the value is 0 or at the width (`progressCursor.cpp:62-70`), so **the playhead's list length changes at every loop start**. That is one expensive frame per loop wrap. Not a problem alone; avoidable by drawing the line off-screen instead.
   - Text costs about 4 setup words plus 1 word per character, and 1 more word per out-of-range character (`DisplayList.cpp:63-85`). A readout that grows from 9 to 10 characters shifts what follows. Use fixed-width formats (`%02d`) for position and countdown.
   - A blink that draws nothing when off (Rec glyph, naming cursor) changes the length each toggle. At 2 Hz this is harmless; drawing in the background colour keeps it small.
2. **Waveforms are the heavy control.** `cSpectrum` type 0 emits two vertices per point (`spectrum.cpp:166-184`), so 600 points is about 1200 words, 59 percent of the 2043-word frame cap. Two channels at 600 points would not fit; stock playback uses 200 or 300 points (`gSamplePlay.cpp:369-389`). The recorder redraws its spectrum only every 100 ms, growing by 50 ms each time (`fSampleRec.cpp:165,185,1604`), so stock treats it as expensive. For the design: never animate a waveform per frame; only the playhead and meters should move that fast.
3. **Gradients and shadows use extra words.** `cBar` can add a blend, scissor and gradient (`bar.cpp:105-118,134`). The design's flat bars do not need them.
4. **The cap is 2043 words per frame.** Overflowing controls are dropped silently (`display.cpp:818-822`). The main screen with four lanes, text, meters and waveforms needs to be counted. `displayList::overflows` records it, but only a debug tool shows it.

## What stock already updates while audio runs

| Control, screen | What it does | Rate, source |
| --- | --- | --- |
| Playhead line, sample playback and sample recorder preview (`cProgressCursor`) | one 1 px line; position read from the voice, then `refreshControl` | position re-read at most every 5 ms (`PLAY_REFRESH_US = 5000`, `fSamplePlay.cpp:86,2631`; `fSampleRec.cpp:22,2862`); screen update then happens as fast as frames allow, up to 54/s. Update sites: `fSamplePlay.cpp:172-176`, `fSampleRec.cpp:168-172` |
| Level bar, recorder input screen | `changeLevelBar()` called at the end of every `update()`; redraws only when the value changes | up to the frame rate (`fSampleRec.cpp:295,2479-2484`). Colour has a 350 ms red hold (`gSampleRec.cpp:716-722`) |
| Recording time text, recorder | text refreshed every pass while recording (`fSampleRec.cpp:184`, `gSampleRec.cpp:602-610`) | only a changed string reaches the chip |
| Recorder waveform while recording | rebuilt every 100 ms, slowing by 50 ms per rebuild | `fSampleRec.cpp:165,185,1604` |
| Song timer, top bar (`cSongTimer`) | text "mm:ss, pattern, step" | throttled to 100 ms and skipped when the text is unchanged (`songTimer.cpp:59`, `:73-100`), so at most 10/s: this is the design's "text at most 10/s" |
| Pattern playhead blink | on/off flip | every 500 ms (`fPatternEdit.cpp:170,282`) |

These are the same kinds of motion the design wants, which is good evidence, not proof: nobody has recorded what they do to frame rate or audio on the Original (HANDOVER: nothing measured on the device).

On `update2`/`update3` of spectrum and progress cursor: as noted in step 4 they are not separate steps spread over time in this tree, and neither control animates through them. The progress cursor is one small stage of a few words.

## Verdict per item in the design's motion table

| Item (design) | Verdict | Why |
| --- | --- | --- |
| At most four things moving at once | **Feasible** | Things moving together share one frame. The limit that matters is words per frame, not number of things. Confidence medium-high. |
| None wider than 720 x 18 px | **Not a limit that matters** | The chip draws the whole list every refresh; only vertex words change when a bar grows. Keep bars flat (no gradient or shadow, `bar.cpp:105-118`). |
| Input level meter, 720 x 18 px bar + 2 px peak marker, up to 20/s | **Likely feasible** | Stock level bar already redraws on change at frame rate (`fSampleRec.cpp:2479-2484`). One bar and one 2 px marker is a handful of changed words. Confidence medium. |
| Track and master meters, four 114 x 3 px bars + 6 px master, up to 20/s | **Likely feasible** | Five small bars, about 15 changed words each (est.), about 75 words, about 0.2 ms of SPI. The CPU cost is mostly compose and compare. Confidence medium. Where the meter values come from is outside this ticket; the voices have RMS meters (`cpu-ram-budget.md`). |
| Playhead, 1 px line + 7 x 5 px cap across four lanes, up to 20/s | **Feasible, best-supported item** | Stock moves a playhead during playback at the same or a higher rate. Use a fixed-length list so a loop wrap does not shift everything. At 20/s a 4 s loop moves the line 9 px per step (720 px / 80 steps), so it will look steppy but not slow. Confidence medium-high. |
| Position and countdown text, at most 10/s or once per beat | **Feasible** | Same as the stock song timer (100 ms throttle, change-only). Use fixed-width formats. Confidence high. |
| Count-in text, once per beat | **Feasible** | A few words per beat. |
| Feedback-protection bar 200 x 6 px, up to 10/s; state text twice per event | **Feasible** | One small bar and a text swap. |
| CLIP tag, per event | **Feasible** | One small text box on and off. |
| Save and export progress, 390 x 6 px bar + % text, at most 4/s | **Feasible, with a caveat** | The display side is trivial. During a save the main loop may be busy with the SD card and call `display.update()` less often; stock calls `processDisplayUpdateImmediate()` before long jobs (`display.cpp:1228-1248`). I did not read the save code. Needs a device check. |
| Armed Rec glyph blink, 500 ms on / 500 ms off | **Feasible** | Stock blinks at 500 ms (`fPatternEdit.cpp:282`). Two small glyphs. |
| Naming cursor blink, 530 ms | **Feasible** | Same. |
| Value-changed tick, 150 ms | **Feasible, with jitter** | One 2 px line changing colour twice. A 150 ms tick is about 8 panel refreshes; no problem. |
| Snap confirmation (marker 1 s, text) | **Feasible** | One-off text and marker. |

**Combined worst moment** (playing with an armed track: four track meters, master meter, playhead, POS text, countdown, two Rec glyph blinks): about 10 to 12 controls changing, estimated 150 to 250 changed words per frame (est.), about 0.4 to 0.6 ms of SPI per frame, plus the fixed compose and compare costs. It looks within budget, but this is the case to test first.

## Confidence

- **Pipeline description (steps 1-9): high.** Read directly from `display.cpp`, `display.h` and the driver.
- **Link mechanism (polled byte-at-a-time SPI, no DMA, no background write): high.**
- **SPI clock (20 or 30 MHz): low-medium.** Two values in the code disagree, and I did not verify which startup file is linked.
- **Panel at 54 Hz: medium.** Needs the chip's own clock, not in the code read.
- **Per-frame times and the 20 to 40 per second range: low-medium.** They stack estimates for SDRAM speed, build cost, audio load and main-loop pass length. The shape of the conclusion (display work is small next to the 18.5 ms panel period) is robust; the exact numbers are not.
- **Overall feasibility of the motion table: medium.**

## Where I am guessing

- SDRAM speed for memcpy and memcmp (the 0.3 to 0.8 ms and 0.2 to 0.4 ms rows).
- Build cost per control (tens of microseconds).
- Audio load of 35 to 60 percent: taken from `cpu-ram-budget.md`, which is also an estimate.
- The share of the main loop the display may use (I picked a quarter) and the main-loop pass length.
- Changed-word counts per meter bar (about 15).
- The FT812 clock behind the 54 Hz figure.
- Whether the rewritten display code is the fork's or upstream's.

## What only a test on the device can answer

1. **Real frame time and rate.** A counter or log of the time from `readyForFrame` to swap issued, and swaps per second, on a screen with the full set of moving items at 20/s, with four Parts playing and one recording.
2. **Main-loop pass length** during that workload. It decides whether 20 Hz holds (it fails if a pass averages about 12 ms or more).
3. **The real SPI clock.** Time sending a full 2043-word frame (expect about 4.5 ms at 20 MHz, about 3 ms at 30 MHz).
4. **Audio glitches.** Whether the display work, the SDRAM traffic from compose and compare, or longer main-loop passes cause dropouts; read the profiler (`/.develop` file, percent of block period, `cpu-ram-budget.md`).
5. **Frame overflow.** Whether `displayList::overflows` or `droppedCommands` is nonzero on the busiest screen.
6. **How it looks.** Stepping of the playhead at 20/s, jitter from panel quantisation, flicker on blinks.
7. **Save and export** at 4 updates/s: does the progress bar move while the SD card is written.
8. **Not covered here:** fades (the theme format rejects alpha, `docs/custom_themes_and_fonts.md`) and a growing waveform, which ticket 18 lists for the device test. I read nothing that says they are feasible.
