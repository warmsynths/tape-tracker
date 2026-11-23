# Test firmware files

`PolyendTracker_FT_test1.ptf` is the first test build for the Original Tracker (see `docs/first-test-build.md`).

Built from commit "Add the Four-Track test screen" with GCC 10.3.1 from Ubuntu's archive, not Arm's exact release. Not tested on a device yet.

Download it from GitHub with the "Download raw file" button, copy it into the `Firmware` folder on the SD card, then install it from Config, Firmware, Firmware update.

## Current work (important)
The earlier test files were built from Polyend's unreleased `main` code, which crashed Config on the Original. The working builds are based on the official release **v1.0.0** (commit `9b9a13b`).

- `patches/four-track-on-v1.0.0.patch`: all firmware changes so far, as a patch against tag v1.0.0. Apply with `git apply` on a checkout of `9b9a13b`. It adds the Four-Track screen, a four-track loop player, the live monitor, the input level meter, the session logic, and PC tests under `tests/host/`.
- `PolyendTracker_FT5_stop.ptf`: the build made from that patch (four tracks in sync; Stop during a take keeps what was played). Not yet tested on a device. `.ptf` files are normally ignored by git, so this one was added with `git add -f`.
