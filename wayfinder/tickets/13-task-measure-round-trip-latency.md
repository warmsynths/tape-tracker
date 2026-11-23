---
type: task
status: open
blocked-by: []
assignee:
---
# Measure the round-trip latency on the real device

## Question
Human-only, and needs a cable and the device. The recording-path research could not settle whether the audio delay is 2 or 3 blocks (about 5.8 or 8.7 ms) plus the codec delay, so someone has to measure it. Use the loopback-click method in `docs/research/record-path-and-latency.md`: send a click out, patch the output back into the line-in, and measure the gap in a recording. The alignment rules will use the measured number. Resolved when the number and the method are recorded here.
