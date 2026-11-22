# Could the firmware core run on a PC? Desktop build feasibility

Ticket question: could `core/` be built and run on a PC, as a desktop "platform" behind the existing `Platform` interface, so the UI and recording logic can be tried without hardware?

Target of the wider effort: a four-track recorder for the Original Tracker (platform MK66, Cortex-M4F at 180 MHz).

Repo state examined: commit `68f8cd3` (fork of polyend/tracker-ce). Nothing outside `docs/research/` was modified.

## Short answer

- **Yes, it is feasible, and the code is better prepared for it than I expected.** The core already talks to hardware only through the `Platform` interface (`core/include/Platform.h`) and `DisplayDriver` (`core/include/DisplayDriver.h`). The core's makefile does not even put the platform folders on its include path.
- I tested this instead of guessing. I compiled a scratch copy of `core/` (plus `util/` and `synth/` headers) with the PC compiler (g++ 13.3, x86-64). Of 229 `.cpp` files, 213 passed a syntax check with only one change (a struct-size assertion removed). Details are in the next section.
- **The real work is not the core. It is (a) the screen and (b) the many small platform classes, plus (c) a handful of ARM-only code spots.**
- **Recommendation: defer the full windowed build. Do a small headless harness first, and only once the recorder design is settled.** Reasoning and confidence are at the end.

## What I measured (method and limits)

I copied `core/`, `util/` and `synth/` to a scratch folder and ran the PC compiler over the same file set and include paths that `core/makefile` uses, with `-std=gnu++20 -fno-rtti -fno-exceptions`. This was read-only with respect to the repo. I did **not** link or run anything.

| Step | Result |
|---|---|
| First pass, no changes | 187 of 229 files failed. 184 of those failures were one thing: `static_assert(sizeof(strInstrument) == 372)` at `core/source/mtStructs.h:734` gives 392 on a 64-bit PC (see obstacle 9). |
| Same, with that one assertion removed | **213 OK, 16 fail** (all trivial, see list below). |
| Same, with the trivial fixes simulated by compiler flags (`-fpermissive`, force-include `assert.h` and `<optional>`), compiled to object files | 217 of 229 produce objects. **12 fail at the assembler**, because of ARM inline assembly (obstacle 3). Nothing else failed. |
| Symbols still undefined after all objects are built (excluding libc, libm and libstdc++) | `enableAudioInterrupt` and `disableAudioInterrupt` (declared `core/source/interruptsControl.h:3-4`), `sin_q15` (`platforms/mk66/source/platform_math.cpp`), `mallinfo` (`core/source/Profiler.cpp:47,57`), `muteAudioOutByKey`, `unmuteAudioOutByKey`, `isAudioOutMuted` (defined in `platforms/mk66/source/hardware.cpp:196-212`). This list is incomplete: the 12 files that did not assemble may hide more. |

The 16 files that failed the syntax check, and why:

- **No `assert` declared (11 files):** `mtStructs.cpp:5`, `FileBrowser.cpp:11`, `SdStream.cpp:14`, `newList.cpp:130`, `dspApplyGain.cpp:287`, `UsbOutput.cpp:10`, `play_memory_loop_forward.cpp:91`, `play_memory.cpp:556`, `Output.cpp:53`, `gSampleImport.cpp:369`, `gSampleRec.cpp:1038`, `interfacePopups.cpp:63` and others. On the device the platform provides it (`platforms/mk66/source/polyassert.h`).
- **Pointer stored in a 32-bit integer (3 files):** `core/source/mtConfig.cpp:108`, `core/FileManager/mtFileManagerOldVersionRecovery.cpp:106`, `core/AudioEngine/mtAudioEngine.cpp:1616`.
- **`strchr` returns `const char*` on the PC's C library (1 file):** `core/libs/NewDisplay/textBox.cpp:212,220`. The device's newlib does not do this.
- **Missing `#include <optional>` (1 file):** `core/interface/sampleImporter/sampleImporter.h:148`.

Compiling is a much weaker test than running. These results say "the source is portable in form", not "it behaves the same".

## Obstacle list

I count **12 concrete obstacles**, grouped by the four areas in the ticket. Each has a severity: S (small, hours), M (medium, days), L (large, a week or more).

### 1. The screen (FT812 display lists)

How the core draws: controls call `API_*` functions (for example `API_VERTEX2F`, `API_COLOR`). `core/libs/FT812/FT812.cpp:50-66` appends the resulting 32-bit display-list words to a buffer in MCU memory (`displayList::capture`, `core/libs/FT812/DisplayList.h`). `cDisplay::showScreen()` then sends that buffer through `DisplayDriver::writeDisplayList()` and `swapDisplayList()` (`core/libs/NewDisplay/display.cpp:400-416`). Text, numbers and gradients are expanded into display-list words in the core itself (`core/libs/FT812/DisplayList.cpp`).

So the core hands over a list of 32-bit words, which is a good seam for a desktop renderer. Two things are not covered by that seam:

- **Obstacle 1 (L): something has to turn display-list words into pixels.** Not every screen goes through the capture path. The core still sends coprocessor commands that need chip behavior: `CMD_DLSTART`, `CMD_APPEND`, `CMD_MEMCPY` from `RAM_DL` (`core/libs/NewDisplay/arrangement.cpp:226-276`), `CMD_SNAPSHOT2` (`core/libs/NewDisplay/snapshots.cpp:89,104`), `CMD_LOADIMAGE` (JPEG/PNG decode, `core/libs/NewDisplay/display.cpp:570`) and `CMD_GETPROPS` (`display.cpp:607`).
- **Obstacle 2 (M): fonts and bitmaps live in the chip's graphics RAM.** The renderer needs a simulated `RAM_G` that receives the font and image uploads (`writeDataRAMG`, `writeFontRAMG`), and the font metric/format handling used by `displayList::setFontMetrics`. The firmware uses its own fonts nearly everywhere; the built-in chip fonts (handles 27-29) appear only on the charging screen (`display.cpp:1626,1631`), the factory test (`mtTest.cpp`) and `simpleTextControl.cpp:85`.

The size of the drawing vocabulary is modest. Across `core/libs/NewDisplay`, `core/interface` and the other core folders, the display-list calls used are about 25 distinct ones. Most frequent: `VERTEX2F` (440 uses), `COLOR` (225), `BEGIN` (166), `END` (157), `LINE_WIDTH` (123), `SAVE/RESTORE_CONTEXT` (60 each), `BLEND_FUNC` (58), `BITMAP_*`, `SCISSOR_*`. A software renderer for that subset is bounded work.

**Is there a reusable FT81x renderer or emulator? Yes, with caveats, and I can confirm these from the sources named below:**

- **Bridgetek's official EVE emulator** ([github.com/Bridgetek/EVE_Emulator](https://github.com/Bridgetek/EVE_Emulator)). Its README says it targets the EVE family including FT80X and FT81x, ships as prebuilt binaries for **Windows and Ubuntu 24.04 (amd64)** only, and "is intended for emulator release artifacts and integration examples, not for building the emulator library itself from source". So: **no source, no macOS, no ARM Linux.** Its `LICENSE.md` says the binaries and headers remain Bridgetek's proprietary property, may be used "for the purpose of developing software for Bridgetek EVE devices", and the runtime DLLs may be shipped with your application. I did not find any statement on whether a community repo may redistribute them; check the license before committing binaries. A third-party project reports (pull request in `syamajala/polyend-tracker-re`, #78) that it downloads the emulator at a pinned version and never commits the binaries, citing that Bridgetek's licence does not allow redistributing it on its own; that is their reading, I did not verify it.
- **How it would plug in.** The Original Tracker's FT812 driver is a thin "wire protocol" layer (`platforms/mk66/3rdparty/FT812/API_Layer.cpp`, `EVE_Layer.cpp`, `MCU_Layer.cpp`) that ends in SPI byte transfers (`EVE_Layer.cpp:47-92`). Replacing the SPI transfer with the emulator's transfer call would let the existing driver code run unchanged. I have **not** tried this and I have not read the emulator's header, so treat this as a plausible design, not a confirmed one. Also note the licence block at the top of `MCU_Layer.cpp` says the FTDI driver "may be used only in conjunction with products based on FTDI parts"; whether an emulated chip counts is a question I cannot answer.
- **Evidence it works with this kind of stream.** The `polyend-tracker-re` project (a separate, clean-room firmware, **not derived from this repo**) replays the display-list word streams of its UI on this emulator under Wine, and compares the list sizes against the real driver limit (their PR #78 and #134). That shows the emulator accepts FT81x streams of this shape. It does not show it matches the Tracker's real output pixel for pixel.
- **Prior art for a home-made renderer.** The same project's desktop simulator README (`sim/README.md`) says it has its own software FT81x renderer (`src/sw_display.*`) interpreting "a subset of display-list commands", with custom fonts uploaded into a simulated `RAM_G`; it states custom fonts come out pixel-identical, built-in chip fonts are only approximated, and coprocessor timing and display-list size limits are not simulated. It is Apache-2.0 licensed per their repo page. It is written against their own code, not this firmware, and I did not read the renderer source, so I cannot judge its quality or reuse cost.
- **What I could not confirm:** any open-source (source-available) FT81x renderer that already handles the coprocessor commands listed above. My searches found drivers and example code (for instance Bridgetek's Eve-MCU-Dev) but no source-available emulator. If one exists I did not find it.

### 2. Audio library and audio timing

The Teensy-derived audio library in `core/libs/Audio` is portable in form: the graph code (`AudioStream`, mixers, players, effects) uses no hardware registers, DMA or `kinetis.h`. The only `kinetis.h` include in the core is in `core/libs/Audio/utility/pdb.h`, which nothing includes. The hardware side (I2S, DMA, codec) is in `platforms/mk66/3rdparty/Audio/` (`output_i2s.cpp`, `input_i2s.cpp`) and is not needed on a PC.

How audio timing works, and why it suits a desktop build:

- The device raises a DMA interrupt every 128 samples (`kAudioBlockSamples`, `platforms/mk66/config/config.h:14`; about 2.9 ms at 44,117.6 Hz). That interrupt calls `TrackerImpl::updateAudioBuffers()` (`core/source/Tracker.cpp:178-210`), which advances the **sequencer by the block duration** (`sequencer.tick(updateIntervalSeconds)`), runs the engine and renders the audio graph (`audioEngine::updateAudioBuffers` calls `AudioStream::update_all`, `core/AudioEngine/mtAudioEngine.cpp:310-322`).
- The main loop (`platforms/mk66/source/TrackerMain.cpp:60-67`) just calls `updateHardware()` and `Tracker::update()` (UI, file manager, SD streaming).
- Sequencer timing is therefore **driven by sample count, not by a wall clock**. A PC can call the same block function from a normal audio callback, or in a loop with no sound card at all.
- **The core already has an offline mode that does this.** `mtAudioExport` sets `AudioStream::renderMode` and calls `artificial_isr()` in a loop to render audio faster than real time (`core/Exporter/mtAudioExport.cpp:76,738`, `core/libs/Audio/AudioStream.cpp:360`). That is real evidence the audio graph can be pumped without hardware.

Obstacles:

- **Obstacle 3 (M): interrupt masking and concurrency.** On the device the audio interrupt preempts the main loop, and the code protects shared data by switching interrupts off. `DISABLE_GLOBAL_INTERRUPTS()` and `ENABLE_GLOBAL_INTERRUPTS()` are ARM assembly (`core/config/config.h:4-5`, `cpsid i` / `cpsie i`), used in about 20 files (including `AudioStream.cpp`, `mtSequencer.cpp`, `effect_envelope.cpp`, `mtAudioNoteOnHandle.cpp`). `ENABLE/DISABLE_AUDIO_INTERRUPTS()` (`core/source/interruptsControl.h`) are used in 7 more files (about 50 occurrences, mostly `Synth.cpp` and `SynthPatchEditor.cpp`). A text search for all interrupt-masking spellings finds about 120 occurrences in total. On a PC they must become either nothing (if audio and UI run on one thread, taking turns) or a lock (if audio runs on a separate thread). A single thread taking turns is simplest, but it needs care so UI work cannot starve the audio block. Cross-thread flags such as `volatile head/tail` in `core/libs/Audio/record_queue.h` assume the ARM's single-core behavior.
- **Obstacle 4 (S-M): `dspinst.h` ARM instructions.** `core/libs/Audio/utility/dspinst.h` wraps about 35 Cortex-M4 DSP instructions in inline assembly (`ssat`, `smulwb`, `smmul`, `pkhbt`, `qadd16`, `smlald`, `smuad`, ...; lines 39-387). Most have **no portable fallback** (only a few have a stub under `KINETISL`, and some of those just `return 0`). On a PC these must be rewritten in plain C. Each one has precise saturation and rounding behavior to match. `get_q_psr`/`clr_q_psr` (lines 361-375) read the ARM status register. About 134 uses across 22 audio files. Also `Input.cpp:38` and `analyze_rms.cpp:40` already have a plain fallback when `KINETISK` is not defined.
- **Obstacle 5 (S): other ARM-only spots in the core.** `core/effector/ptfx_loader.cpp:408-452` (`dsb`/`isb` cache barriers and the MPU), `core/libs/NewDisplay/arrangement.cpp:250,285` (`bkpt #0`), `core/libs/Audio/record_queue.h:48` (a harmless compiler barrier). `sin_q15` comes from CMSIS-DSP (`platforms/mk66/source/platform_math.cpp`, used only by `core/effector/mtEffectFlanger.cpp:133`); the core itself uses no other CMSIS-DSP function.
- **Obstacle 6 (S, optional): `.ptfx` effect plugins are ARM machine code** loaded into RAM and executed (`core/effector/ptfx_loader.cpp`; `docs/developer_guide.md` section 3). They cannot run on a PC. Built-in effects are fine. Not needed for a recorder; just stub the loader. (`.psyn` synth plugins are Tracker+/Mini only, `FEATURE_SYNTH` is defined only for the IMX build in `core/makefile`.)

### 3. Memory placement, linker sections and the Teensy core

- Memory placement macros are plain GCC section attributes (`core/include/cr_section_macros.h`, `core/config/config.h:9-12`). **They compiled on the PC compiler without change** (the ELF default linker script accepts unknown section names), and the 112 uses of `__NOINIT`/`__SECTION`/`DMAMEM` in 37 files are therefore not a blocker. On the PC the "external RAM" buffers just become ordinary static arrays.
- Memory sizes are not a problem on a PC. `docs/memory_map.md` lists an 11.25 MB sample pool, 1.5 MB delay buffer, 768 KB scratch buffer, 16 MB SDRAM total. All of that is small for a PC.
- **Fixed hardware addresses:** only one use in the core, a self-test routine (`core/source/mtTest.cpp:650`, `EXTERAL_MEMORY_ADDRESS`, defined at `core/source/mtStructs.h:63,68`). Stub or exclude `mtTest.cpp`.
- **No linker script is needed** and no `.S` file exists inside `core/`. `platforms/imx/source/fastmem.S` is Tracker+/Mini only and not part of the MK66 build. `platforms/mk66/3rdparty/Audio/memcpy_audio.S` is used only by the I2S driver.
- **Teensy core dependency:** the core itself does not include Arduino/Teensy headers. It has its own `elapsedMillis`/`elapsedMicros` (`core/libs/ElapsedMillis/elapsedMillis.h`) that read the `Timer` interface. The Teensy core, SPI, I2C, SDHC, USB and Snooze code is entirely under `platforms/mk66/3rdparty/` (about 140,000 lines of vendor code, none needed on a PC).
- **Obstacle 7 (S): `mallinfo`** in `core/source/Profiler.cpp:47,57` for the free-RAM readout. Stub it.
- **Obstacle 8 (S): the core assumes newlib behaviour in two places** (`strchr` const-ness, missing `<optional>` include, see the measurement section).
- **Obstacle 9 (M, uncertain): 64-bit PC vs 32-bit ARM.** Structs that are written to the SD card contain pointers: for example `int16_t* address` inside `strInstrument` (`core/source/mtStructs.h:608`) and `int16_t* address[kCount]` (line 816), plus the three pointer-to-`uint32_t` casts listed above. That is why `sizeof(strInstrument)` is 392 on a 64-bit PC against the required 372. File formats (`.pti` headers, project files) would be silently wrong. Two ways out: build 32-bit (`-m32`; I could not test this because this sandbox has no 32-bit C headers, and 32-bit builds are hard or impossible on modern macOS), or find and fix every serialized pointer (I found two struct members and three casts; I did not audit the whole code base for more).

### 4. SD card, buttons, pads, jog wheel, codec, USB

The platform classes are small. `platforms/mk66/source/` is 58 files, 3,948 lines in total, 2,858 of them `.cpp`. The biggest is `FileSystemImpl.cpp` (611 lines), then `Profiler.cpp` (265), `PlatformImpl.cpp` (260), `BatteryImpl.cpp` (224), `hardware.cpp` (214).

| Interface | What a PC version needs | Size |
|---|---|---|
| `FileSystem` (`core/include/Platform.h`) | **Obstacle 10 (M):** about 40 methods: open/read/write/seek, directory listing with filters, hidden-folder flag, rename/delete, mount-event callback. A host-folder implementation (case-insensitive like FAT) is straightforward but long; the device version sits on FatFs plus a 1,360-line Teensy SD wrapper (`platforms/mk66/3rdparty/SD/SD.cpp`, `SdFile.cpp`). Two methods are device-specific: `enableFastSeek` (FatFs cluster map) and `readSectorsAsync`/`pollAsyncRead`/`waitAsyncRead` (raw SD sector reads used by `core/AudioEngine/VoiceStreamManager.cpp:280,1236`). A host version can return `false` for both; **I did not verify that every streaming path then falls back cleanly**, so check before relying on it. | 3-5 days |
| Buttons, Pads (`SwitchGroup`), `Encoder` | **Obstacle 11 (S-M):** events reach the core through plain functions the platform calls: `onButtonPush/Release/Hold`, `onPadPush/Release/Hold`, `onPotChange(0, delta)` for the jog wheel, `onSDCardSlotChange` (see `platforms/mk66/source/hardware.cpp:21-30` and `EncoderImpl.cpp:7,31`). A PC front end only has to call them from mouse or keyboard events and answer `isButtonPressed`. The core's hold-time logic is in the platform driver (`setHoldTime`), so the PC version must reimplement hold and auto-repeat itself. | 2-4 days incl. an on-screen panel |
| `Sgtl5000` (codec), `Leds`, `Battery`, `Radio`, `UsbAudio`, `Usb`, `UsbMidi`, `UartMidi`, `CrcCalculator`, `ConfigManager`, `SystemInfo`, power functions | Mostly no-ops or small stubs. The codec is a set of settings calls with no audio data. For recording tests, audio input would come from a WAV file or the PC microphone via the audio callback. `CrcCalculator` needs a real CRC (the device uses FastCRC hardware-accelerated code; core only calls an interface). `ConfigManager` is an EEPROM-like store, one small file. | 2-4 days |
| `Timer` | `millis`, `micros`, `delay`, `clockCycles`, `clockCyclesPerUs`. `clockCycles` is used only for the synth overload check and the CPU profiler (`core/libs/Audio/synth_PolySynth.cpp:41`, `core/source/Profiler.cpp:111`). `scheduleMidiClockUs` already has a default in `Platform.h`. | hours |

**Obstacle 12 (M): the build system.** There is no host build today. `core/makefile` is ARM-only (`-mcpu=cortex-m4 -mfpu=fpv4-sp-d16`), and the only CI workflow (`.github/workflows/build.yml`) builds the ARM firmware. I found no host-side tests in the repo. A desktop build needs its own makefile or CMake file, a decision on a window/audio library (for example SDL2), and `#ifdef`s or shim headers for the items above.

## Rough size of the work (my estimate, not a measurement)

Unit: focused working days for one developer who knows C++ and embedded code. If the work is done by an AI agent with a human reviewing, the calendar time may be shorter, but the review time of the display and audio parts does not shrink much. **These ranges are guesses from reading the code. Confidence in them is low to medium.**

| Piece | Estimate |
|---|---|
| Host build files and compile fixes (assert, casts, `strchr`, `<optional>`) | 1-2 days |
| Plain-C replacements for `dspinst.h`, interrupt macros, ptfx and `bkpt` stubs | 1-3 days |
| Concurrency choice (single thread taking turns vs audio thread plus lock) and getting it stable | 1-3 days |
| 64-bit issue (serialized pointers) | 1-3 days (if `-m32` is not usable) |
| Platform classes except file system and display | 2-4 days |
| File system on a host folder | 3-5 days |
| Audio output and input through a PC audio library | 1-2 days |
| **Headless subtotal** (no screen: scripted buttons, offline audio, logs and WAV output) | **about 2-4 weeks** |
| Screen, using Bridgetek's emulator (Windows/Ubuntu only) | 4-7 days, plus licence check |
| Screen, using a home-made renderer for the ~25 commands, fonts, `RAM_G`, coprocessor commands | 8-15 days |
| Front panel (pads, buttons, jog wheel) and input mapping | 2-4 days |
| **Full windowed build** | **about 4-8 weeks** |

Ongoing cost: the host build must be kept working as the firmware changes. Every new use of an ARM instruction, a hardware address or a pointer in a saved struct can break it.

## What a desktop build would tell us

- **Whether the recorder logic and file handling are right.** State machines (arm, record, overdub, punch, stop), file naming and writing, project/pattern formats, undo and so on, run through the same code the device runs.
- **Whether the screens look and flow as intended**, if the display piece is built: layout, navigation, text fit, what each button does. (Not pixel-exact against the real chip unless the Bridgetek emulator is used, and even then I did not verify it.)
- **Repeatable tests of audio results.** Because the sequencer advances by audio block, a scripted session can be rendered offline to WAV and compared run to run. Caveat: the 35 ARM DSP instructions would be re-implemented, so results can differ from the device by small rounding differences. Do not use a desktop render as proof of the exact device sound.
- **Early bugs of the ordinary kind**: out-of-range indexes, uninitialized memory, crashes. PC tools such as AddressSanitizer find these far more easily than the device. This is probably the single most valuable thing a host build offers an AI-assisted workflow.

## What it would NOT tell us

- **CPU load and timing.** A PC is orders of magnitude faster than the 180 MHz Cortex-M4. The firmware's own profiler (`core/source/Profiler.cpp`) and its dropout behavior mean nothing on a PC. Whether four recording tracks plus 8 playing voices, effects and the UI fit into the roughly 2.9 ms audio block is **only answerable on the device**. `docs/developer_guide.md` section 1 already notes the M4 spends about 20-35 ms per step on FT812 redraws and Song Mode pattern reads, and that this limited streaming features on this platform.
- **SD card behavior.** Write speed, latency spikes, FatFs behavior, card removal, and the SDHC DMA path that the streaming code uses. A four-track recorder is mostly an SD write-throughput problem, and a PC file system hides all of it.
- **Memory headroom.** `docs/memory_map.md` puts the Original Tracker at about 93.5% of SDRAM used (about 1.04 MB free), about 68.5% of internal RAM (about 80 KB free, which must also hold the stack and heap) and about 86% of flash (about 144 KB free). Whether the recorder's buffers fit is answered by the ARM build's link report, not by a PC. **That link report is available now without hardware**: CI builds the firmware and posts the memory usage (`docs/memory_map.md` notes, `.github/workflows/build.yml`).
- **Interrupt interactions.** Priorities (`platforms/mk66/config/interruptsConfig.h`: MIDI 0, USB 3, audio in 4, audio out 5, SD 6) and what starves what under load do not exist on a PC.
- **Real codec and analog behavior**: input levels, noise, mic versus line, the real input-to-output latency.
- **FT812 timing and limits**: SPI bandwidth, display-list size limit, swap timing. The other project's README says the same for its simulator.
- **Hardware feel and faults**: pad and jog-wheel response, I2C bus errors and timeouts, USB audio and MIDI.

In short: a PC build checks *what the program does*; only the device (or a cycle-accurate model of it) checks *whether it does it in time*. For a recorder, the timing questions are the main risks.

## Alternatives I did not investigate

- Running the real ARM firmware in a hardware emulator (for example QEMU or Renode). I did not research whether any supports the MK66 peripherals this firmware needs; I would not assume so. Treat as unknown.
- Testing only on the device, with the existing CI size report as the early warning for memory.

## Recommendation: defer the full build; do a small headless slice first, later

1. **Do not start the windowed desktop build now.** The biggest items (the screen and the platform classes) cost weeks and do not touch the biggest risks of a four-track recorder (CPU headroom, SD throughput, 1 MB of free SDRAM). Those risks must be checked on the device or in the ARM link report whatever we do.
2. **Do not decide "never".** The compile test shows the core is unusually clean: 213 of 229 files compile on a PC unchanged apart from one assertion, the audio timing is sample-count driven, and an offline render mode already exists. A headless harness is realistic and would give fast, repeatable tests of recorder logic, plus access to AddressSanitizer.
3. **Order of work if/when it is done:** (a) first the ARM-only shims, the build file and a headless `Platform` with scripted button input, a host-folder file system and offline WAV output (about 2-4 weeks, my estimate); (b) then, only if screen iteration without the device is worth it, add the display (prefer a home-made subset renderer for portability, or the Bridgetek emulator if its licence and Windows/Ubuntu-only support are acceptable). Reading the `polyend-tracker-re` simulator for ideas is possible, but its code is a different code base and I did not review it.
4. **Trigger to build the headless slice:** when recorder logic is large enough that flashing a device for every test is the bottleneck, or when the user does not have a device available for most test cycles. Neither fact is known to me. If the device is at hand for every test, the headless slice is optional; if not, move it earlier.

**Confidence:**
- That `core/` can be made to build and run on a PC: **high** (I compiled it; the failures were few and trivial).
- That the audio and sequencer logic would behave the same on a PC apart from rounding: **medium** (not run; the ARM DSP shims and the interrupt model could hide surprises).
- The work estimates: **low to medium**.
- Which display route is better (Bridgetek emulator vs home-made renderer): **low**. I could not read the emulator's header or try it, and I did not read the other project's renderer.
- The recommendation itself (defer, headless first): **medium**. It rests on assumptions about how often the device is available that I cannot check from the code.

## Where I am guessing or did not check

- All effort numbers are estimates.
- I did not link or run any PC build, only compile and inspect object files; I did not try the Bridgetek emulator.
- I did not verify that `VoiceStreamManager` degrades cleanly when `enableFastSeek` and the sector read return `false`.
- I did not audit the whole code base for more pointers inside saved structs (obstacle 9).
- The "undefined symbols" list is incomplete because 12 objects did not assemble.
- I did not read the FT81x programmer guide to confirm which display-list behaviors (anti-aliasing, line caps, blend modes) a home-made renderer would have to copy exactly.
- Licence statements (Bridgetek emulator, FTDI driver header) are my reading of short texts, not legal advice.

## Sources

- Repo files cited inline, at commit `68f8cd3`: `core/include/Platform.h`, `core/include/DisplayDriver.h`, `core/libs/FT812/{FT812.cpp,DisplayList.h,DisplayList.cpp}`, `core/libs/NewDisplay/display.cpp`, `core/libs/Audio/utility/dspinst.h`, `core/config/config.h`, `core/source/{Tracker.cpp,mtStructs.h,Profiler.cpp,interruptsControl.h}`, `core/AudioEngine/{mtAudioEngine.cpp,VoiceStreamManager.cpp}`, `core/Exporter/mtAudioExport.cpp`, `platforms/mk66/source/*`, `platforms/mk66/3rdparty/{FT812,Audio,SD}`, `docs/memory_map.md`, `docs/developer_guide.md`.
- Bridgetek EVE emulator repository and licence: https://github.com/Bridgetek/EVE_Emulator and its `LICENSE.md` (read 2026-11-22 through a page summarizer, not the raw files).
- Third-party desktop simulator README (clean-room firmware, different code base): https://github.com/syamajala/polyend-tracker-re/blob/main/sim/README.md; use of the Bridgetek emulator in CI: https://github.com/syamajala/polyend-tracker-re/pull/78 (also #134 in `sandlbn/polyend-tracker-re`). Their statements are reported here as claims, not verified by me.
- Compile experiment: scripts and outputs were kept in the session scratchpad, not in the repo; the commands are described in the measurement section above so they can be repeated.
