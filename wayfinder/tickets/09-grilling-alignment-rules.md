---
type: grilling
status: open
blocked-by: [03-research-record-path-and-latency, 04-research-onset-detection, 08-prototype-computer-workflow]
assignee:
---
# Alignment rules

## Question
Settle the Alignment Window size, pre-roll length, what counts as the first note for each source, offset units and step size (milliseconds, beats, bars), what happens when a first note is outside the window, whether alignment is stored as an Offset or by trimming audio, and how the click and count-in behave.

## Progress note
Decided already: Offset wraps in Repeat mode and is cut off at the ends in Play once mode. Still open: window size, pre-roll length, what counts as the first note per source, offset step sizes, how the click and count-in behave.
