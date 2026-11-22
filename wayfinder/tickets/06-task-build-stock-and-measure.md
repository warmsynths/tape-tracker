---
type: task
status: open
blocked-by: []
assignee:
---
# Build the stock Original firmware and measure it

## Question
Nothing to decide yet; the flash and RAM numbers are needed first. Install the ARM toolchain named in the README, run `make original` without flashing anything, and record the linker memory usage (flash, RAM, SDRAM). Then try a stripped build (granular, wavetable and slice modes off) and note the flash saved. If the toolchain cannot be downloaded in this environment, record that and hand the user a checklist.
