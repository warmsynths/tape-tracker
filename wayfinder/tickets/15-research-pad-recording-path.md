---
type: research
status: closed
blocked-by: []
assignee:
---
# How pads played live can be captured as audio

## Question
The plan is that pads are played live and captured as audio on a track, like any other source. The stock recorder's sources are line, mic, radio and USB, and no internal mix source was seen. Find out how pad sample playback reaches the output (mixer graph in `core/AudioEngine`) and whether the recorder can take the internal mix, or only the pad voices, as a source. Report what would have to change, the cost in CPU and memory, and whether timing stays sample-accurate against the loop.

## Resolution
Findings are in `docs/research/pad-recording-path.md`. The research agent's summary, with its own confidence levels:
- The recorder cannot take an internal source today, but the audio graph already has record queues on every voice and on the master (built for the exporter), so pad capture is a small change.
- Best option: a pad-bus source on reserved voices, recorded dry into the Part buffer.
- Two changes are required, and both are shared with later-Part recording: stop the recorder freezing voice updates (`core/AudioEngine/mtAudioEngine.cpp:277`), and arm the start inside the audio interrupt (ticket 03).
- Timing: the capture stays on the same clock as loop playback and avoids line-in input delay. The pad hit itself lands on a 128-sample block plus an unmeasured main-loop delay.
- No bounce or freeze feature exists. The exporter renders to an SD WAV only, and the only resampler is a cubic one used on import.
- Confidence: medium-high on routing and the size of the change; medium-low on CPU and RAM numbers, which are estimates. Section 11 of the file lists what only the device can confirm.
