---
type: research
status: closed
blocked-by: []
assignee:
---
# Which lo-fi effects exist in stock on MK66

## Question
Which stock effects on MK66 fit the lo-fi chain: limiter, dynamics, bit depth, chorus, delay, reverb? Is there a usable compressor for master glue? Is there anything for tape saturation or wow/flutter (a modulated short delay)? Report CPU/RAM cost where documented and what would need writing from scratch, for MK66 specifically.

## Resolution
Findings are in `docs/research/master-fx-availability.md` in this repo. The research agent's summary, with its own confidence levels:
- No stock real-time glue compressor, tape-saturation model or wow/flutter exists on MK66.
- Closest stock parts:
  - `AudioEffectDynamics`: a mono block-RMS compressor with a 30 ms minimum attack/release, used today only as the line-in gate.
  - The master mixer's 5 selectable saturation curves, for crude saturation at no CPU cost.
  - `AudioBitDepth` for bit-crush. It has no sample-rate reduction.
- To write from scratch: a modulated short delay for wow/flutter (the offline flanger is the template; the stock short delay has a fixed time), a stereo-linked compressor, sample-rate reduction, and the logic that sheds effects when the CPU runs short.
- All offline Sample Editor effects are unusable live. Everything checked is compiled for MK66. The "master enhancer" is codec hardware.
- Confidence: high on what exists and its MK66 status; low on every CPU figure, because the repo documents no costs and nothing was measured or built.
