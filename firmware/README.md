# Test firmware files

This fork is built on the official Polyend Tracker CE release **v1.0.0** (commit `9b9a13b`), plus our changes. The firmware source for the Four-Track screen is in the normal source tree (`core/interface/fourTrack`, `core/libs/Audio/play_loops.*`, `core/source/FourTrackSession.*`), and PC tests are in `tests/host/`.

An earlier version of this fork was built on Polyend's unreleased `main` branch. That crashed the Config screen on the Original, so it was rebuilt on v1.0.0. The old history is kept in the tag `archive/main-on-unreleased`.

`PolyendTracker_FT5_stop.ptf` is the latest test build, made from this source. Copy it into the `Firmware` folder on the SD card and install it from Config, Firmware, Firmware update. See `docs/first-test-build.md`. `.ptf` files are normally ignored by git, so this one was added with `git add -f`.

How it is built here: `docs/build-notes.md`.
