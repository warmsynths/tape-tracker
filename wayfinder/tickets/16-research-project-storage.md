---
type: research
status: closed
blocked-by: []
assignee:
---
# How a four-track project could be stored on the SD card

## Question
Project save, open, rename and the project list are "not yet specified". Find how the stock firmware stores projects (`core/FileManager`, `.mt`, `.pti`, the workspace copy-on-write model, backward-compatibility rules in `CONTRIBUTING.md`) and what a four-track project needs to save: four mono Parts with their offsets and alignment data, loop length, bars, tempo, per-project settings, a name. Compare reusing the stock project format with a new simple format. Also cover how the existing file browser could be reused for the Open and Save As lists, and what one level of undo would cost if the previous take is stored on the SD card.

## Resolution
Findings are in `docs/research/project-storage.md`. The research agent's summary, with its own confidence levels:
- Recommendation: a new folder-per-project format, not the stock `.mt`/`.pti` one. Each project folder holds `project.ft` (about 200 bytes: version, size and CRC), four mono `part1..4.wav` files and an `undo/` folder.
- Open and Save As reuse the existing file browser in folder mode, using `project.ft` as the marker. Naming uses its built-in keyboard (32-character limit). Device settings go in a new section appended to `mtConfig`.
- Undo keeps the previous take on the SD card: it renames `partN.wav` to `undo/partN.wav` when the Part is unchanged since its last save, and writes it out first when it is not.
- Why not stock: its structs are size-pinned (`strMtValues` 1792 B, `strInstrument` 372 B), so there is no room for our fields.
- Timings are guesses, since no SD throughput is measured in the repo: saving all four Parts (10.6 MB) about 0.7 to 3.5 s, one Part about 0.2 to 0.9 s, and an undo is one Part read of about the same.
- Confidence: medium-high on the format and file-browser reuse; low on timings; medium on the undo design. The file lists what only the device can confirm: card speed, audio dropouts during a save, and an old saved config after the `mtConfig` change.
