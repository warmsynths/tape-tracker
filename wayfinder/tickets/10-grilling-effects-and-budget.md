---
type: grilling
status: open
blocked-by: [02-research-cpu-ram-budget, 05-research-master-fx-availability, 06-task-build-stock-and-measure]
assignee:
---
# Effects and resource budget

## Question
Decide which effects ship in v1 (master glue compressor, saturation, wow/flutter, bit-crush; per-track only if affordable), their controls, what is cut first if the CPU runs short, and the final track length given RAM and flash.

Added after the recording-path research: an audio pre-roll buffer takes SDRAM out of the roughly 1 MB free, so include it in the budget.
