# Test firmware files

`PolyendTracker_FT_test1.ptf` is the first test build for the Original Tracker (see `docs/first-test-build.md`).

Built from commit "Add the Four-Track test screen" with GCC 10.3.1 from Ubuntu's archive, not Arm's exact release. Not tested on a device yet.

Download it from GitHub with the "Download raw file" button, copy it into the `Firmware` folder on the SD card, then install it from Config, Firmware, Firmware update.

## Current work (important)
The earlier test files were built from Polyend's unreleased `main` code. That caused Config to crash on the Original. The working builds are based on the official release **v1.0.0** (commit `9b9a13b`).

- `patches/ft3-on-v1.0.0.patch`: all firmware changes so far, as a patch against tag v1.0.0. Apply with `git apply` on a checkout of `9b9a13b`. It adds the Four-Track screen, loop player, and monitor and level meter.
- `PolyendTracker_FT3_monitor.ptf`: the build made from that patch. Config works with the FT2 build that this one extends; FT3 itself is not yet tested.
- `PolyendTracker_FT_test1.ptf`: older, built from unreleased code. Do not use.
