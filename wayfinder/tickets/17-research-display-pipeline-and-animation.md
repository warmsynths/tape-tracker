---
type: research
status: open
blocked-by: []
assignee:
---
# How the screen is drawn on the Original and what motion it can sustain

## Question
Find how a frame reaches the screen: the display list built by the CPU, the swap at the panel's next frame, how controls update in steps, and the speed of the link to the screen chip on the Original (`core/libs/NewDisplay`, `platforms/mk66/source/DisplayDriverImpl`). Estimate from the code how many small updates per second are realistic (meters, a playhead, a few text readouts) alongside recording and playback, and which stock controls already do such updates. State what the design's motion table needs (at most four things moving, none wider than 720 by 18 px, up to 20 updates a second) and whether it looks feasible. Say plainly what only a test on the device can answer.
