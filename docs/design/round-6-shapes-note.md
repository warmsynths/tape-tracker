# Shapes note for the design tool (round 6 follow-up)

Paste everything below the line into the design tool's chat, together with (or after) the round 6 brief.

---

A correction to the limits I gave you before, and an invitation.

**You are not limited to square, right-angled shapes.** The earlier rule "no rounded corners, no curves" came from the stock firmware's few built-in controls. It was not a limit of the hardware. Our screens draw straight on the display chip, which can do:
- filled rectangles, **rounded rectangles** (one corner radius per screen is almost free), lines at **any angle**, line strips, and **filled circles**;
- **rings** (two circles) and **dials and arcs** (a row of small dots along a curve, about 20 for a smooth one, or a needle line);
- one-bit bitmaps (we use them for waveforms) and see-through fills (for tints and layers; a transparency change costs 4 bytes).

Still out: gradients, shadows, blur, large images, anti-aliased vector icons. Still fixed: the five fonts and the eight buttons under the screen.

**The real limit is the drawing budget, not geometry:** about 8,172 bytes of commands per screen. Costs: rectangle 20, line 20, circle 24, colour change 4, transparency change 4, text 16 + 4 to 8 per character, bar 12 + 8 per column. The new chord and sound pages already use 4.8 to 6.3 KB each, so there are about 2 KB to spend on shape. Rounded corners cost the same as square ones; what costs is the number of shapes (outlines, rings, many separate dots). Please keep a count of shapes per frame, and tell me if you would trade shape for budget somewhere.

**What I would like you to explore** (these are suggestions, not requirements):
- A visual language for the chord pads: tiers (tonic, in key, colour chord, outside the key, chord tone, performance pad, held) told apart by **shape and colour**, not only a fill and a thin bar. Rounded pads, a ring on the tonic, a diagonal mark for "outside the key" are all possible.
- The Sound screen as an instrument, not a form: dial-style knobs for the eight tweak knobs (arc plus needle), a patch card with its kind and blurb.
- Whether the whole product should soften (rounded cards, pill tags) or stay hard-edged like the stock Tracker. Show me one frame each way and say which you prefer.

**Two cautions:**
1. The web emulator draws shapes exactly; the display chip rounds edges slightly differently. If you choose a strongly curved language, I will put a small test screen on the Tracker before we commit.
2. The pad LEDs on the device have brightness only, no colour, so any meaning you give to a pad on the screen should also work as brightness steps.

Everything else in the round 6 brief stands.
