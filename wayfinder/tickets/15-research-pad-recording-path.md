---
type: research
status: open
blocked-by: []
assignee:
---
# How pads played live can be captured as audio

## Question
The plan is that pads are played live and captured as audio on a track, like any other source. The stock recorder's sources are line, mic, radio and USB, and no internal mix source was seen. Find out how pad sample playback reaches the output (mixer graph in `core/AudioEngine`) and whether the recorder can take the internal mix, or only the pad voices, as a source. Report what would have to change, the cost in CPU and memory, and whether timing stays sample-accurate against the loop.
