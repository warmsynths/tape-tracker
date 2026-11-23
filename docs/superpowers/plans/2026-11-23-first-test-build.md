# First Test Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox syntax.

**Goal:** A firmware for the Original Tracker with one new screen where the person records one mono Part from the line input and hears it loop.

**Architecture:** Reuse the stock recorder (`Recorder`, configured mono into a reserved memory region) for capture. Add one new audio stream, `AudioPlayLoop`, wired into the master mixer as a fifth input, to loop the region seamlessly. Add one new screen module, `cFourTrack`, reached by the Song button in this test build. Keep the pure logic (state machine, loop fill) free of hardware so it can be tested on a PC.

**Tech Stack:** C++20 (existing firmware), arm-none-eabi-gcc 10.3, host g++ for unit tests.

**Spec:** `wayfinder/map.md`, `docs/readiness-matrix.md` (features 1, 8, 10), `docs/research/record-path-and-latency.md`, `docs/build-notes.md`.

## Global Constraints
- Never touch the bootloader area or the flash start address.
- One mono Part, at most 30 s (`MAX_TRACK_SEC`), stored in RAM at 16-bit, firmware sample clock.
- The stock build must still compile: `make original`, flash headroom reported in `docs/build-notes.md`.
- This is a test build, not for release: the Song button opens the new screen.

## Review Focus
- Loop wrap exactly at the end with no click or gap (test: fill across the boundary).
- Recording hits the 30 s cap (test: state machine clamps and reports full).
- Stop pressed when nothing recorded (test: stays empty, no loop).
- Rec pressed while looping (test: loop stops, recording starts).
- Memory region overlaps the Sample Editor buffer (documented: do not use the editor with this build).

---

### Task 1: Pure loop fill
**Files:** Create `core/libs/Audio/loop_fill.h`; Test `tests/host/loop_fill_test.cpp`
**Produces:** `inline void loopFillBlock(const int16_t* data, uint32_t length, uint32_t& position, int16_t* out, int count)` — writes `count` samples reading `data` cyclically from `position`, advancing `position` modulo `length`; zero-fills when `length == 0`.
- [ ] Write tests: wrap across the end gives data repeated; position advances modulo length; length 0 gives zeros; length smaller than count repeats several times.
- [ ] Run with host g++, expect fail, implement, expect pass, commit.

### Task 2: Pure state machine
**Files:** Create `core/source/FourTrackLoop.h`, `core/source/FourTrackLoop.cpp`; Test `tests/host/four_track_loop_test.cpp`
**Produces:** `class FourTrackLoop { enum class State { Empty, Recording, Ready, Playing }; State state() const; void pressRecord(); void recordedFrames(uint32_t frames, bool memoryFull); void pressStop(); void pressPlay(); uint32_t lengthFrames() const; }` with `static constexpr uint32_t kMaxFrames` set from a constructor argument (sample rate times 30).
- [ ] Tests for each Review Focus line above plus: Play from Ready starts Playing; Stop from Playing returns to Ready; Rec from Ready or Playing discards the old Part and starts Recording.
- [ ] Implement, run on host, commit.

### Task 3: Loop stream and mixer input
**Files:** Create `core/libs/Audio/play_loop.h`, `play_loop.cpp`; Modify `core/AudioEngine/MixerAndFxs.h` (add `kLoopChannel`, one more input), `core/AudioEngine/mtAudioEngine.cpp` and `.h` (connect, accessor `AudioPlayLoop* getLoopPlayer()`).
**Interfaces:** `class AudioPlayLoop : public AudioStream { void play(const int16_t* data, uint32_t lengthFrames); void stop(); bool isPlaying() const; }` using `loopFillBlock`.
- [ ] Implement, build `make original`, confirm it compiles and links, commit.

### Task 4: The screen
**Files:** Create `core/interface/fourTrack/fourTrack.h`, `gFourTrack.cpp`, `fFourTrack.cpp`; Modify `core/interface/core/interface.cpp` (add to `modules[]`, bump `modulesCount`, map Song button to it).
**Behavior:** Buttons 1 to 3 under the screen are Rec, Stop, Play. Status and length text. Rec uses `Recorder` mono, left channel, into the reserved region, full-memory callback calls `FourTrackLoop::recordedFrames(..., true)`. Play and Stop drive `AudioPlayLoop`.
- [ ] Implement, build, confirm memory report, commit.

### Task 5: Build, document, hand over
**Files:** Create `docs/first-test-build.md` (how to flash, what to press, what to report back).
- [ ] Build `make original`, record sizes, write the test steps, commit and push.
