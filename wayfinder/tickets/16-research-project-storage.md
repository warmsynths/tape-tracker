---
type: research
status: open
blocked-by: []
assignee:
---
# How a four-track project could be stored on the SD card

## Question
Project save, open, rename and the project list are "not yet specified". Find how the stock firmware stores projects (`core/FileManager`, `.mt`, `.pti`, the workspace copy-on-write model, backward-compatibility rules in `CONTRIBUTING.md`) and what a four-track project needs to save: four mono Parts with their offsets and alignment data, loop length, bars, tempo, per-project settings, a name. Compare reusing the stock project format with a new simple format. Also cover how the existing file browser could be reused for the Open and Save As lists, and what one level of undo would cost if the previous take is stored on the SD card.
