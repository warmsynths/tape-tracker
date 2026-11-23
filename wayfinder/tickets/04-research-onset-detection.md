---
type: research
status: closed
blocked-by: []
assignee:
---
# First-note detection

## Question
Which lightweight first-note detection works for guitar, bass and finger-drum hits on a 180 MHz M4F, run on the recorded buffer? Compare a simple threshold, envelope derivative and spectral flux, adaptive noise floor, and false triggers (string noise, count-in clicks, breath). State the precision needed (about 1 ms?), what stock analysis code (analyze_rms, the input gate) can be reused, and recommend one method plus a default Alignment Window.

## Resolution
Findings are in `docs/research/onset-detection.md` in this repo. The research agent's summary, with its own confidence levels:
- Recommended method: a peak-absolute envelope in 32-sample cells (about 0.7 ms) with an adaptive noise floor (threshold about 4x the floor, minimum about -50 dBFS), a 5 ms confirmation check, then a back-search and a zero-crossing snap. No FFT is needed. 1-2 ms precision is enough.
- Estimated cost is about 15 ms per 10 s loop. That is an estimate, not measured. Spectral flux costs roughly 10x more and isn't needed for a single first note.
- Default Alignment Window is 60 ms either side of the loop start, clamped to half a 16th note at the current tempo. This is the agent's estimate and is for the alignment-rules ticket to settle.
- Reuse: there is no transient shaper in the code. The nearest, `core/source/mtSliceDetector.cpp`, resolves only 128 samples (2.9 ms), so copy the idea, not the code. The zero-crossing helper in `play_memory.cpp` and the input-gate defaults (-50 dBFS, 6 dB hysteresis) are reusable.
- Confidence: medium-high on method and code findings; medium-low on cycle counts, bass timing and the window value. Nothing was run or tuned on real audio, and the literature references were cited from memory.
- Open point passed on: the agent says latency compensation matters more than detector resolution, which depends on the recording-path research.
