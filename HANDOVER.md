# Handover

Read this first, then `START-HERE.md`, `CONTEXT.md` and `wayfinder/map.md`. This file says where the work stands and how to work with the person who owns it. It deliberately does not repeat decisions: they live in the map and its tickets, once each.

## The project in one paragraph
The owner is building a four-track recorder firmware for the Original Polyend Tracker (a hardware groove box, MK66 chip, 180 MHz). They record guitar, finger-drummed samples and bass one part at a time and want alignment to be nearly automatic. This repo is their fork of Polyend's open firmware (`polyend/tracker-ce`). No firmware has been changed yet. Everything so far is planning, research, a browser prototype and design briefs. The current goal, per the map, is a written v1 spec, not the firmware itself.

## The owner, and how to work with them
- They do not write C or C++. You write all code and explain in plain language. They test on hardware.
- They are the only one who can flash or test the device. Never assume you can reach it.
- Safety is a hard rule: never touch the bootloader area or the flash start address. They must keep a stock `.ptf` on the SD card and know the reset pinhole recovery (ticket 07).
- They like plain answers to their actual question. Give a recommendation, not a survey. If they say "go with your recommendations", record the decision and move on.
- Screen visuals (colors, fonts, style) are decided by a separate design AI (Claude Design) and the owner. Do not dictate those. Describe screens by what they are for and what the person does on them. Hardware limits are the exception and are in `docs/design/ui-constraints.md`.
- Say plainly what you verified and what you guessed. Nothing has been measured on the device. Every CPU number so far is an estimate.
- They asked for one branch. Everything is on `main`.

## Where the work stands
See `wayfinder/tickets/`. A ticket's `status:` and `blocked-by:` lines tell you what is open and what is takeable.
- Closed: 01 (core decisions), 02, 03, 04, 05 and 14 (research). Their resolutions are at the bottom of each ticket.
- Open and takeable by you: 06 (build the stock Original firmware and measure sizes). It may be blocked by network access to the ARM toolchain; if so, record that and write the owner a checklist.
- Open and needs the owner: 07 (recovery drill on their device), 13 (measure the audio round-trip delay with a cable), 08 (they play the prototype and report back).
- Blocked: 09, 10, 11 (alignment rules, effects and budget, screen and controls), then 12 (write the v1 spec).
- `docs/readiness-matrix.md` lists every feature against six readiness checks and the findings that came out of it. Keep it up to date as tickets close. The simulator in `prototype/sim` was added after this handover was first written; its tests had not been run when the matrix was drafted.
- Not created yet, offered and not yet answered: a research ticket on how the display pipeline works, and a hardware test ticket for what animation the Original can really sustain.

## Waiting on the owner
- Feedback from playing the prototype (`prototype/four-track-PROTOTYPE.html`, also at the private link in `START-HERE.md`). It is the main input for tickets 09 and 11.
- Screens back from the design AI. Two briefs were sent: the main screens (`docs/design/claude-design-first-prompt.md`) and a second round (settings, input setup, project, popups). A motion brief was also sent; see the tiers below. When screens arrive, check each against `docs/design/ui-constraints.md` and report anything unbuildable.

## Animation, in short
The firmware draws with a separate screen chip and swaps in a finished frame about every 20 ms. Likely fine: playhead, meters, a pulsing record indicator, a moving highlight, changing numbers. Needs a test on the device: fades, a growing waveform, a short slide. Unlikely on the Original: whole-screen transitions, smooth scrolling or zooming, blur. `ui-constraints.md` was updated to say this. `claude-design-first-prompt.md` still carries the older, stricter wording because it was already sent.

## How the plan is run
The map is a local-markdown task board: one file per question in `wayfinder/tickets/`, an index in `wayfinder/map.md`. Take an open ticket with nothing blocking it, resolve it, write the answer under `## Resolution`, set `status: closed`, and add one line to "Decisions so far" in the map. Do one ticket at a time. Research tickets can run in parallel. If an answer makes new questions specific, add tickets; if it is still vague, put it under "Not yet specified".

## Practical traps
- The clone is shallow. Merging branches fails with "unrelated histories". Copy files between branches instead.
- Pushing works with `git push -u origin <branch>`. Deleting a remote branch from the original session failed repeatedly through its proxy. Two stale branches, `wayfinder-map` and `prototype/four-track`, may still exist on GitHub with nothing in them that is not on `main`. The owner can delete them on GitHub.
- There is no firmware emulator. The browser prototype is the stand-in. `docs/research/desktop-build-feasibility.md` explains why a desktop build is deferred.
- The prototype shares no code with the firmware, but its logic module (inside the page) is written to be lifted into C++ later.
- License: MIT with a clause forbidding wholesale commercial clones of the complete Tracker app. A new product built from parts is fine. Read `LICENSE` before anything commercial.

## Suggested next steps, in order
1. Take ticket 06 (stock build and sizes), if the toolchain can be fetched.
2. Ask the owner for prototype feedback, then work tickets 09 and 11 with them.
3. Review incoming screens from the design AI against the hard limits.
4. Ask whether to create the two animation tickets.
5. With the owner at the device: 07, then 13.
6. Ticket 12, the v1 spec, when 09 to 11 are closed.
