# Four-Track Recorder

A firmware for the Original Polyend Tracker that replaces the tracker-style UI with a four-track recording workflow: record parts one at a time, with fast, near-automatic alignment between them.

## Language

**Track**:
One of the four lanes a recording lives on. Each track holds one part at a time.
_Avoid_: Channel, lane, voice

**Part**:
One recording on a track, such as the guitar part or the bass part.
_Avoid_: Take, clip, sample, region

**Loop**:
The region that repeats during recording and playback. The first part recorded sets its length.
_Avoid_: Song, pattern, section

**Loop Mode**:
A setting that decides whether the loop repeats or plays once through.
_Avoid_: Repeat, cycle

**Offset**:
How far a part is shifted relative to the loop start, set in time or in bars. Zero by default.
_Avoid_: Nudge, delay, shift

**Alignment**:
The default behaviour that places a new part's first note exactly on the loop start when it lands within the alignment window.
_Avoid_: Quantize, snap

**Alignment Window**:
The small distance from the loop start within which a first note counts as meant to start on it. A first note further out is treated as a deliberate late entry and left in place.
_Avoid_: Tolerance, threshold
