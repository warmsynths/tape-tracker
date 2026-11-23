---
type: task
status: closed
blocked-by: []
assignee:
---
# Recovery drill on the user's device

## Question
Human-only. Before any custom build is flashed: put the current stable Community Edition .ptf for the Original on the SD card, flash it through Config > Firmware, then practise the recovery with the reset pinhole, with a known-good .ptf as the only file in /Firmware/. Resolved when the user confirms both worked and notes the exact filename and card layout.

## Resolution (2026-11-24, by the owner)
- Flashing the official Community Edition `PolyendTracker_CE_1.0.0.ptf` through Config, Firmware, Firmware update worked.
- The paperclip reset recovery then ran the flash. The screen stayed at 100 and did not restart by itself. Restarting the Tracker by hand booted it normally.
- Power was a wall USB adapter, and one official `.ptf` was in the `Firmware` folder.
- Learned: expect the update screen to sit at 100 and need a manual restart. Polyend's own support page is reported (in search summaries, not read directly) to say to reboot manually when an update reaches 100% without restarting.
- The filename rule from the repo code: starts with `PolyendTrac`, 15 to 31 characters. Both the official file and the test file fit.
