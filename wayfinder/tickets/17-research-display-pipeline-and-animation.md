---
type: research
status: closed
blocked-by: []
assignee:
---
# How the screen is drawn on the Original and what motion it can sustain

## Question
Find how a frame reaches the screen: the display list built by the CPU, the swap at the panel's next frame, how controls update in steps, and the speed of the link to the screen chip on the Original (`core/libs/NewDisplay`, `platforms/mk66/source/DisplayDriverImpl`). Estimate from the code how many small updates per second are realistic (meters, a playhead, a few text readouts) alongside recording and playback, and which stock controls already do such updates. State what the design's motion table needs (at most four things moving, none wider than 720 by 18 px, up to 20 updates a second) and whether it looks feasible. Say plainly what only a test on the device can answer.

## Resolution
Findings are in `docs/research/display-pipeline-and-motion.md`. The research agent's summary, with its own confidence levels:
- The design's motion table looks feasible on the Original. Twenty updates a second is about 37% of the screen's ceiling of roughly 54 a second, and about 3 to 10% of main-loop time. These are estimates.
- The 720 by 18 px size limit does not matter. The cost is the number of changed words in the display list, and things that move together share one frame.
- Watch-outs: the link to the screen chip sends data byte by byte, with no DMA, at 20 or 30 MHz (the code disagrees on which). A control whose list length changes forces a re-send of everything after it. That happens with a playhead at the loop wrap, text that changes length, and a blink that disappears completely.
- Caveat: the `docs/developer_guide.md` figure of 20 to 35 ms per step does not describe this display code, which builds its list on the CPU.
- Confidence: medium overall. The pipeline and link are high; the timings stack guesses and are low to medium. Main-loop pass length, memory speed and interference with audio can only be answered on the device.
