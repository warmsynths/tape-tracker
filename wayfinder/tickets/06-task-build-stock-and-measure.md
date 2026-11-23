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

## Result of the first half (2026-11-23)
The stock Original firmware built in the cloud environment with a GCC 10.3.1 from Ubuntu's archive. Recipe and numbers are in `docs/build-notes.md`. FLASH 935,436 B of 1 MB (89.21%), RAM 181,084 B of 256 KB (69.08%), EXTERNAL_RAM 15,686,472 B of 16 MB (93.50%). That is about 55 KB more flash than the repo's documented baseline, probably because of the compiler build and a small workaround, so free flash is about 110 to 145 KB until the official GitHub Actions build is compared.

Still to do before this ticket closes: the stripped build with granular, wavetable and slice modes off, to measure how much flash it saves.

## Update (2026-11-24)
The first build used Polyend's unreleased main, so those numbers do not apply to the release we now build on. Stock v1.0.0 built here: flash 873,936 B (83.35%), RAM 131,984 B (50.35%), SDRAM 15,587,616 B (92.91%). With the Four-Track screen, flash is 879,172 B (83.84%). Free flash is about 165 to 170 KB. The stripped build (granular, wavetable, slice modes off) is still to do and is probably unnecessary now.
