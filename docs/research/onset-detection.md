# First-note (onset) detection for the four-track recorder

Ticket: which lightweight first-note detection works for guitar, bass and finger-drum hits on the
original Tracker (MK66, 180 MHz Cortex-M4F), run after recording on a mono 16-bit 44.1 kHz buffer.

Confidence: **medium-high** on the method and on the code-reuse findings (read directly from the
repo); **medium-low** on the cycle counts, the bass timing figure and the default window value
(estimates, marked GUESS below). Nothing was compiled, run or measured on hardware, and no audio
was tested. The literature references are from memory and were not re-fetched in this session.

## Recommendation (one method)

**Rectified-peak envelope with an adaptive noise floor, a confirmation check and a back-search**
(a "level threshold with local refinement"), run once over the search region after recording.

1. Reduce the buffer to a peak-abs envelope in 32-sample cells (0.73 ms at 44.1 kHz).
2. Estimate the noise floor from the quietest cells of the search region (or the pre-roll before
   the loop start), as a low percentile (about 10th) of the cell peaks.
3. Threshold = max(floor x K, absolute minimum), with K about 4 (about +12 dB) and an absolute
   minimum of about -50 dBFS (GUESS; both need tuning on real recordings).
4. First cell over the threshold is a candidate. Confirm it: the mean of the next ~5 ms must also be
   above threshold/2. This rejects single-cell clicks and pops.
5. Refine: walk back from the candidate to the first cell still above ~25% of threshold, then snap
   to the nearest zero crossing in that cell. That is the reported onset sample.

Why this one: it is a few integer operations per sample, needs no FFT, no float in the inner loop,
and gives sub-millisecond resolution. Guitar picks, bass plucks and drum-pad hits all have a fast
amplitude rise from silence, which is the one case where a level threshold is close to ideal. The
only thing it needs beyond a bare threshold is the noise-floor term, which is what handles pickup
hum and string noise.

## Comparison

| Method | Cost on M4F | Timing | Strengths | Weaknesses for this use |
|---|---|---|---|---|
| Fixed threshold on abs/RMS | ~3-5 cycles/sample | Excellent if peak-based; poor if block RMS (2.9 ms blocks) | Trivial | One fixed level fails for quiet bass vs loud finger-drum; trips on hum and count-in bleed |
| Envelope derivative (rise rate in dB) | ~5-10 cycles/sample plus a log per cell | Good | Level independent: catches a soft pluck and a loud hit with one setting | Log of near-silence is noisy; needs a floor anyway; slow bass swells and slow guitar volume-swell attacks are missed |
| Spectral flux (FFT, positive bin differences) | 256-pt real FFT per hop; about 0.1 ms each (GUESS), so ~40% of real time at hop 128, ~10% at hop 512 | Frame-limited (hop 512 = 11.6 ms); needs refinement anyway | Best on pitched, soft-onset material, vibrato, and polyphonic mixes | Overkill for a single monophonic first note; needs FFT code and RAM; poor time resolution; the lowest bass fundamentals (E1 41 Hz) barely fill a 256-bin frame |
| Adaptive noise floor (the recommended addition to level threshold) | Same as threshold plus one pass to estimate the floor | Same as threshold | Handles hum, hiss, different input gain, different players | Floor estimate can be wrong if the loop has no silence at all (see failure modes) |

Spectral flux and the complex-domain / phase methods are the literature's general-purpose choice
(Bello et al., "A Tutorial on Onset Detection in Music Signals", IEEE Trans. Speech and Audio
Processing 13(5), 2005; Dixon, "Onset Detection Revisited", DAFx 2006). The same literature
describes the energy envelope as the simplest and, for percussive material, a competitive method;
the weakness is soft pitched onsets, which are not the main case here because the detector looks
for the first note after a silent or near-silent start, not for every note in a performance.

## Timing precision needed

- **Target: 1-2 ms is enough for guitar and finger-drum; about 1 ms is a fine goal but not needed.**
  Reasoning (GUESS, perceptual figures from general psychoacoustics, not looked up here): shifts
  under about 5 ms are not heard as timing errors in a loop; flams and "machine-gun" artefacts show
  from about 10-20 ms.
- At 44.1 kHz, 1 ms is 44 samples. The 32-sample envelope cell is 0.73 ms, which meets that.
- **Bass is looser: expect 3-5 ms ambiguity (GUESS).** A 41 Hz fundamental has a 24 ms period, and
  many basses have a slow-rising fundamental under a quick fret/pluck transient. The detector will
  usually lock to the transient, but the "feel" onset is later than the first energy.
- **The bigger error is systematic latency**, not detector resolution. Input converter plus 128-sample
  audio block buffering (2.9 ms per block, `core/libs/Audio/AudioStream.h:51`) means the recorded audio
  lags what the player heard. A fixed latency offset must be applied when aligning; it is likely
  larger than the detector's own error. The size of that offset was not measured here.

## False triggers

| Source | Effect | Mitigation |
|---|---|---|
| String noise (finger squeak, fret buzz, string touch before the note) | Low-level event before the real first note, can be above a fixed threshold | Floor-relative threshold K (not just absolute); confirmation window; the pre-note noise is usually 15-25 dB below the note |
| Count-in clicks (bleed from headphones/monitor into the input, or the click routed to the input) | Sharp loud transient that looks like a hit | Only search the intended region (loop start minus window, to loop start plus window and the first note), never the count-in; and/or require the post-onset 5 ms mean (a click decays within a few ms, a note does not) |
| Breath / handling noise | Low-frequency thump, moderate level | Run the envelope on a first-difference or simple high-pass of the signal (y[n] = x[n] - x[n-1], one subtraction per sample) so DC and sub-100 Hz thumps are reduced; the bass case is the exception, see below |
| Pickup hum (50/60 Hz and harmonics) | Constant elevated floor | Adaptive floor absorbs it if threshold is floor x K; with a very hot hum the floor-relative threshold simply sits above the hum |
| Cable/jack pop on plug-in | Single huge cell | Confirmation window rejects it |

The first-difference pre-filter helps guitar and finger-drum but removes much of the bass fundamental
energy. For a bass part, skip the pre-filter and rely on floor K and the confirmation window.
That is a per-part setting (GUESS on whether it is worth exposing; a simple fix is to apply it
only to the guitar and finger-drum parts).

Known failure mode of any floor-based method: if the recording has **no quiet stretch at all**
(continuous hum plus playing from the very first sample), the floor is the hum level and the first
note may be missed or the hum itself triggers. Fall back to "no detection" (leave the Part in
place) rather than guessing, consistent with the stated rule that parts not within the window are
left alone.

## Alignment Window

**Default: 60 ms either side of the loop start, clamped to at most half a 16th note at the current
tempo (so at 180 BPM, 16th = 83 ms, window = 41 ms; at 120 BPM it is 62 ms, so 60 ms applies).**
GUESS on the exact value; reasoning:

- Typical player timing error against a click is tens of ms, so smaller than about 30 ms would
  miss legitimate "on the one" attempts.
- A window larger than half the shortest likely note grid (a 16th) would pull a deliberately late
  or pickup note onto the downbeat. Half a 16th keeps the snap unambiguous: any onset within the
  window is closer to the downbeat than to any grid neighbour.
- The window should be user-editable (suggested range 10-250 ms); the default only matters for the
  zero-config case.
- Early onsets (before loop start) need audio from before the loop start. Whether the recorder
  keeps pre-roll was not checked; if it does not, early onsets can only be handled by trimming from
  the front of the buffer (which shortens the Part) or must be treated as late-only.
- The search for the first note should start at the loop start minus the window and stop at the
  window end; if the first onset found lies beyond it, do nothing.

## Cost estimates (all GUESS, nothing measured)

- Peak-abs envelope pass: about 4-6 cycles/sample from RAM, so a 10 s loop (441,000 samples) is
  about 2.6M cycles, roughly 15 ms at 180 MHz. If the buffer is in external RAM, wait states could
  make it 2-4x slower, so well under 100 ms. This can run in one go or in the slice-detector's
  cooperative style.
- Back-search and zero-crossing snap: negligible (hundreds of samples).
- Only the search window needs scanning for the first onset, not the whole loop: 2 x 60 ms plus the
  first note is about 5,300 samples, which is well under 1 ms. The whole-buffer pass is needed
  only for the floor estimate, and a short stretch (e.g. the 100 ms before the window) is enough.
- For comparison, spectral flux at 256-point FFT, hop 128: about 3,445 frames/s of audio x ~20k
  cycles (GUESS for a CMSIS-class q15 real FFT) is about 69M cycles per second of audio, about
  38% of one second. A 10 s loop would be ~4 s. Not worth it. Whether CMSIS-DSP is available in this
  repo was not checked.

## Reusable stock code

Nothing in the repo is a ready-made first-note detector, but there are close relatives.

| Location | What it is | Reuse verdict |
|---|---|---|
| `core/source/mtSliceDetector.cpp:9-111`, `core/source/mtSliceDetector.h:6-20` | Existing offline energy-onset detector for auto-slice: squares samples in 2048-sample buffers, compares with a 10240-sample local average using a variance-dependent sensitivity (`:51`), then refines with 128-sample cells (`preciseDetect`, `:144-296`) | The closest match, and it proves a statistical-energy approach is already accepted in the codebase. **Not reusable directly**: its resolution is 128 samples (2.9 ms) at best, it also steps back by `(i - 5)` cells (`:218`) so the answer is about 15 ms early, it finds many slices rather than the first note, it needs 10240 samples of history before it detects anything (`:44`, `:66`), it writes into an instrument's slice table, and it uses a double-precision constant (`:51`) that is slow on a single-precision FPU (it only runs per buffer, so harmless there). Copy the idea, not the code |
| `core/source/mtSliceDetector.cpp:39`, `:113-121` | Per-sample square and average | Same operation as the envelope pass, but squares into a `uint32_t` buffer in external RAM (`:5`) |
| `core/source/mtSliceManager.cpp:108-127` | Entry point that starts the detector on instrument memory, with a streamed-sample path via `externalRamEffectsBuff` | Pattern for feeding a recorded buffer to a detector |
| `core/source/Tracker.cpp:174` | `sliceDetector.update()` called from the main loop | Shows the cooperative "do a chunk per loop iteration" pattern if the scan has to be time-sliced |
| `core/libs/Audio/analyze_rms.cpp:32-70` | Per-block sum of squares with the dual-16-bit MAC intrinsic `multiply_accumulate_16tx16t_add_16bx16b` (`utility/dspinst.h`) | Reusable only for the intrinsic idiom. It is a streaming `AudioStream` node working on 128-sample blocks (2.9 ms), so the time resolution is too coarse and it cannot be pointed at a finished buffer |
| `core/libs/Audio/effect_dynamics.cpp:35-57` | Static `analyse_rms(int16_t*)` on one block, returns 0..1 | Can be called directly on a block-sized slice, but again 128-sample resolution |
| `core/libs/Audio/effect_dynamics.cpp:98-107`, `core/libs/Audio/effect_dynamics.h:46-60` | Input gate: open at `gateThresholdOpen`, close at open minus hysteresis (max 6 dB), attack/release smoothing in dB, default -50 dBFS | The **threshold and hysteresis defaults (-50 dBFS, 6 dB) are a good starting point for the absolute minimum**. The gate itself is block-based, smoothed and works on a live stream, so it cannot give a sample position. It also needs the `unitToDb` log (`effect_dynamics.cpp:31` includes `fast_log.h`) per block |
| `core/libs/Audio/play_memory.cpp:826-849` (declared `play_memory.h:292`, threshold `play_memory.h:225`) | `isSampleInZeroCrossing`: true if a sample is 0 or within +/-10 and has a sign change with a neighbour | Directly relevant to the zero-crossing snap step; it takes a reference into the sample buffer and clamps to given bounds. It is a private static of `AudioPlayMemory`, so either make it accessible or re-implement (it is 10 lines) |
| `core/effector/mtEffectNormalizer.cpp:21` | Peak-based normalise (`norm_peak / max_peak`) | Has a max-peak scan, but not an onset finder |
| `core/libs/Audio/effect_limiter.cpp:85` (`isSteadyOnSilence`) | Silence-state check for the limiter's own relaxation logic | Not an input silence detector; ignore |

There is **no transient shaper** in `core/effector` or `core/source` (the effector list is cut,
fade, normalise, EQ, chorus, flanger, bitcrusher, delay, compressor, limiter, overdrive, reverse,
timestretch, wavetable smoother and similar; searches for "transient", "onset" and "trim silence"
found only the slice detector above and unrelated silence-flag code). There is also no FFT-based
analysis in the paths I searched (`core/libs/Audio` has an `analyze_*` set from the Teensy audio
library; only `analyze_rms` was inspected).

## What I did not verify

- No audio was tested; the K=4, -50 dBFS, 32-sample cell, 5 ms confirmation and 25% back-search
  values are starting points from general practice, not tuned.
- Cycle counts are estimates from instruction counts, not measurements; external-RAM latency was
  not looked up.
- Where the recorder's buffer lives and whether it has pre-roll before loop start was not checked
  (`core/source/Recorder.cpp` was only skimmed for block size and head index).
- The input path's fixed latency (converter plus buffering) was not measured; it must be calibrated
  separately, as it is likely larger than the detector's error.
- The literature citations (Bello et al. 2005, Dixon 2006) are from memory and were not re-opened.
