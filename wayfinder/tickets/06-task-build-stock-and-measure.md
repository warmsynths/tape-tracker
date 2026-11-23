---
type: task
status: open
blocked-by: []
assignee:
---
# Build the stock Original firmware and measure it

## Question
Nothing to decide yet; the flash and RAM numbers are needed first. Install the ARM toolchain named in the README, run `make original` without flashing anything, and record the linker memory usage (flash, RAM, SDRAM). Then try a stripped build (granular, wavetable and slice modes off) and note the flash saved. If the toolchain cannot be downloaded in this environment, record that and hand the user a checklist.

## Blocker (2026-11-23)
Checked on the user's Windows work laptop:
- Neither `make` nor `arm-none-eabi-gcc` is installed on Windows or in WSL (`Ubuntu`).
- Downloading the GNU Arm Embedded Toolchain (`10.3-2021.10`) from WSL hits `407 Proxy Authentication Required` (corporate proxy), and `sudo` is not passwordless.
- Needs to be run either on a personal machine with `make` + Arm toolchain `10.3-2021.10`, or by checking the existing GitHub Actions workflow (`.github/workflows/build.yml`).
