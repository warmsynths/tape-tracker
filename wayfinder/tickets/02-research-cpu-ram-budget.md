---
type: research
status: closed
blocked-by: []
assignee:
---
# CPU and RAM budget for four tracks on the Original

## Question
On the Original Tracker (MK66, 180 MHz Cortex-M4F, 16 MB SDRAM with about 1 MB free), can the trimmed stock engine run 4 looping mono Parts from RAM, one recording, a live line-in monitor, pad sample hits and the master delay/reverb/limiter without dropouts? Find the audio block size, the cost per voice, how many voices stock runs, what buffers the recorder uses, remaining RAM headroom, and what costs the most. Return a budget table with a confidence level per row and a list of what must be measured on hardware.

## Resolution
Findings are in `docs/research/cpu-ram-budget.md` in this repo. The research agent's summary, with its own confidence levels:
- RAM: four mono Parts of 30 s are about 10.1 MiB of the 11.25 MiB sample pool, so it very likely fits, but only if the Sample Editor and effect-plugin buffers go. The stock editor buffer overwrites everything above the first 3.75 MiB of the pool.
- CPU: one 128-sample audio block is 2.9 ms (about 522k cycles at 180 MHz). The repo has no per-voice figures, so the agent's estimate of 35-60% of that budget is a guess, with the master reverb probably the biggest item. Stock itself can stutter at 8 stereo voices (`UPCOMING.md:57`); our target is about a quarter of that channel count. It needs a hardware run to confirm.
- Two stock behaviours must change: recording freezes per-voice updates (`mtAudioEngine.cpp:277`), and the recorder writes into the start of the whole sample pool, so it must be pointed at each Part's own region. Samples of 256 KB or more also default to SD streaming.
- Confidence: high on RAM figures, low to medium on CPU.
