---
type: research
status: closed
blocked-by: []
assignee:
---
# Recording path and latency on the Original

## Question
How does recording work end to end on MK66 (codec, I2S DMA, record queue, SDRAM/SD)? What is the input-to-output latency and where does it come from? Can recording start sample-accurately on a loop boundary, and can a rolling pre-roll buffer be kept? How can the monitor-path delay be compensated so parts recorded while monitoring line up with the loop?

## Resolution
Findings are in `docs/research/record-path-and-latency.md` in this repo. The research agent's summary, with its own confidence levels:
- Input-to-output latency through the DSP path is 2 blocks of buffering (256 samples, 5.8 ms), or 3 blocks (384 samples, 8.7 ms) depending on interrupt order. Codec and FIFO delay on top is a guess of about 1-2 ms. Planning figure: roughly 300-480 samples (7-11 ms).
- Recording cannot start sample-accurately today: the start is rounded to a 128-sample block and is triggered from the UI loop. It can be made sample-accurate by arming the recording inside the audio interrupt with a frame counter and trimming the first block.
- The stock "pre-roll" is only a metronome count-in. An audio pre-roll ring buffer is cheap in CPU, but for four full stereo loops it needs SDRAM taken from the roughly 1 MB free.
- Compensation is one measured constant: recorded position = loop position + round-trip delay, so recorded data is shifted earlier by that amount.
- The live monitor is software-only, through the mixer, so it cannot feed back digitally; feedback could only come acoustically or through cabling. The document describes a loopback-click measurement.
- Confidence: high on the data path and interrupt order, medium on the 256-sample figure, low on the codec delay and the 2-versus-3-block case. Nothing was run on hardware.
