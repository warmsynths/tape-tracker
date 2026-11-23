# Design files

- `ui-constraints.md`: the hard limits the screens must respect.
- `claude-design-first-prompt.md`: the first brief sent to the design tool.
- `four-track-recorder-design.zip`: the design export. It has two `.dc.html` pages, a `support.js` runtime, and the images and notes that were pasted in.

## What the design contains
- The main screens at 800 x 480 (frames 7a to 7e): the main screen stopped, armed and recording, editing a Part's Offset, and the "How many bars?" prompt.
- Round 2 (frames 8a to 8m): Settings, Input, Project, Save as, and the popups (replace, no sound, memory full, exporting, no SD card, not enough memory, export repeats).
- Under every frame: what each of the eight buttons does, plus the jog wheel, arrows and Shift.
- A motion table sorted into essential and nice-to-have, each with what remains if it is switched off.
- Color roles (Original and Monochrome), font slots, what was dropped because of the constraints, what was settled, and what was left out.

## How to view it
Unzip into an empty folder and serve that folder with a local web server, for example `python3 -m http.server`, then open `Four-Track Recorder.dc.html` in a browser. Opening the file directly may show empty frames, because the page loads parts of itself over the network. It also loads React and fonts from the internet, so it needs a connection.
