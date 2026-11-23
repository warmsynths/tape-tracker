---
type: grilling
status: closed
blocked-by: []
assignee: claude+user
---
# Core workflow, device and limits

## Question
What is this effort for, which device, and what is the shape of the workflow?

## Resolution
- Destination is a written v1 spec, not a build. Device is the Original Tracker (MK66, 180 MHz). UI is fully replaced (no Tracker screens kept); the stock audio engine is kept and trimmed, not rewritten.
- Four mono tracks, ~30 s each, all in RAM, saved to SD. No synths, no pitch changes, no bounce in v1.
- The first Part recorded sets the Loop (exact played length, trimmed to the first note). On stop the user chooses a bar count and a fractional BPM is derived. No audio is stretched.
- Loop Mode is a setting (repeat or play once). Later Parts wait for the loop start, capture a short pre-roll, record exactly one loop, then stop. Re-record replaces the Part; one level of undo.
- Alignment: a new Part's first note lands exactly on the loop start if it falls inside the Alignment Window; a later first note is left in place. Offset is set in time or bars.
- Pads record as audio (sample hits in record mode; other behaviour in edit mode is TBD). Line-in is monitored by default with feedback protection (limiter, fade-in, mic monitoring off).
- Jog wheel follows the stock model: arrows pick a field, the wheel edits it; scrub in playback, move points in edit; Shift for bars/coarse.
- Use the printed button labels where they fit. Keep stock sample/pad playback, project save, WAV export, delay/reverb/limiter; drop pattern editor, song mode, sample editor, radio.
- Lo-fi: master glue compressor plus saturation, wow/flutter, bit-crush; per-track effects only if the CPU allows.
- Safety: never touch the bootloader or flash start address; keep a stock .ptf on the card; the user flashes and tests.
- Prototype on a computer first (no firmware emulator exists).
- Time signature: 4/4 only for v1, with position shown as bar.beat.sixteenth. Beats per bar stays a parameter in the logic (the prototype already treats it as one) so other signatures can be a later setting. Decided later, after Claude Design asked.
- Loop behaviour confirmed with Claude Design: a Part stops at the Loop end, and Offset wraps around the Loop (the wrap is Claude's default, not yet tested against "play once" mode, which belongs to the alignment-rules ticket).
- Small text readouts (position, countdown) may update during playback at about ten times a second at most; waveforms never redraw while playing. Not yet measured on the Original.
- Feedback protection: on whenever monitoring is on, acts only on the monitored signal (never on what is recorded), has no off switch. Mic monitoring starts off; line monitoring starts on.
- Memory full: applies to the first take only. It stops by itself, keeps everything recorded so far, and goes to "How many bars?" with a notice. Later Parts reserve one Loop of room when armed; if there is not enough, the Part is not armed and a memory-full notice shows. (The reservation is intended design, not yet built.)
- Settings: per project are Loop Mode and click; whole device are alignment window, pre-roll, tempo rounding, input source, gain and monitoring.
- Input sources: Line, Mic low gain, Mic high gain. Gain ranges from the audio chip code: line about 0 to +22 dB in 16 steps (the +22 is Claude's arithmetic from measured voltages in the code), mic low 0 to +25 dB, mic high 0 to +44 dB.
- Export: the person chooses how many times the Loop repeats, 1 to 16, default 1.
- Undo keeps the previous take on the SD card (rename into the project's `undo/` folder), because memory cannot hold it.
- Ask "Save your changes?" before New or Open. Quick monitoring toggle and a feedback-protection indicator on the main screen. Deleting a project is out of v1.
- Offset in Repeat mode wraps around the loop; in Play once mode anything pushed past either end is cut off.
- Bar choices: 1, 2, 3, 4, 6 and 8, and 12, 16, 20, 24, 28 and 32 after a very long first take. This supersedes the earlier 1, 2, 4, 8, 16.
- Manual tempo (confirmed by the owner): before the first take the person can set a starting tempo, with a one-bar count-in. Left on "Free", the first take sets the loop and the tempo is derived from its length, as decided above. Built in the simulator (`prototype/sim`).
- Sounds that go onto a track: line input (live instruments) and any loaded sample played from the pads. The type of sample does not matter.
- Pad behaviour, owner's choice: both modes, chosen per sample. "Pad" mode: one pad triggers one sample. "Keys" mode: one sample spread across the pads at different pitches (the stock behaviour). Stock instrument slots 0 to 47 map one to one onto the 48 pads for Pad mode.
- Loaded samples can stream from the SD card, as in stock v1.0.0: samples of 256 KB or more keep only about 0.74 s in memory (64 KB, mono on the Original) in One-Shot and Loop modes. Slice, Beat Slice, Granular and Wavetable stay fully in memory. Memory is therefore not the limit for a pad bank; the limits are 48 instrument slots, 8 simultaneous streaming voices, and SD speed (not measured).
- Part Editor is the small set: trim start and end, offset, fades, normalize. Tone comes from the master effects.
- MIDI stays. The owner connects controllers, so the MIDI settings in Config stay visible and MIDI input must trigger the same sounds as the pads.
- No old Tracker projects need keeping: it is a new SD card with no projects.
