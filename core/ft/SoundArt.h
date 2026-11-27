#ifndef FT_SOUND_ART_H_
#define FT_SOUND_ART_H_

#include <stdint.h>

#include "dsp/SynthPatch.h"

namespace ft
{
inline int ftRound(float x) { return (int)(x + (x >= 0.0f ? 0.5f : -0.5f)); }
// The one-bit pictures on the Sound screen: the dial ring, the one-cycle shape on each patch tile and the
// "one note" picture. Pure functions that fill byte buffers (rows of bits, most significant bit first), so
// they are tested on a PC and uploaded to the display chip by the application (see Mask.h).
constexpr int kDialPx = 64;                       // dial ring picture: 64 x 64
constexpr int kCycleW = 64, kCycleH = 16;         // one-cycle shape: 64 x 16
constexpr int kNoteW = 376, kNoteH = 48;          // one-note picture: 376 x 48
constexpr int kDialBytes = kDialPx / 8 * kDialPx; // 512
constexpr int kCycleBytes = kCycleW / 8 * kCycleH; // 128
constexpr int kNoteBytes = kNoteW / 8 * kNoteH;   // 2256

// A ring of 3 px dots every degree from -225 to +45 degrees, zero at the top.
void artDialRing(uint8_t* out);
// The angle of a knob value (-10 to +10) in degrees on the same scale: 0 is -90 (the top), +-10 is +-135 degrees from it.
int artDialAngle(int value);
// The shape of one cycle of the patch's oscillator.
void artPatchCycle(const SynthPatch& p, uint8_t* out);
// One note of the patch: the oscillator under its envelope (`wave`) and the envelope's silhouette (`silhouette`).
// Returns the x of the release point.
int artNote(const SynthPatch& p, uint8_t* wave, uint8_t* silhouette);
} // namespace ft

#endif /* FT_SOUND_ART_H_ */
