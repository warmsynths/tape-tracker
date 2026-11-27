#ifndef FT_ROLES_H_
#define FT_ROLES_H_

#include <stdint.h>

#include "Chords.h"

namespace ft
{
// Track roles: what you record on a track decides what the pads are. Chords is the chord engine (Chords.h).
// Bass and Melody put three octaves of scale notes on the left hand and playing helpers on the right; Drums
// puts kits and note repeat on the left hand and the drum voices on the right. Portable, no libc.
enum class TrackRole : uint8_t { Chords, Bass, Melody, Drums, Audio };
constexpr int kRoleCount = 5;
const char* roleName(int role);   // Chords, Bass, Melody, Drums, Audio
const char* roleTag(int role);    // CHORD, BASS, LEAD, DRUM, LINE
const char* roleBlurb(int role);  // what the pads become, one line

enum class RoleKind : uint8_t { None, Note, Func, Kit, Rate, Drum };
enum RoleFunc : uint8_t
{
    FnRoot, Fn5th, Fn3rd, FnOctave, FnRootDown, Fn5thDown, FnWalk, FnRest, FnOctDown, FnOctUp, FnGlide, FnHold,
    FnLeadIn, FnGhost, FnShort, FnAccent,                                                   // bass
    FnBendUp, FnBendDown, FnVib, FnMono, FnScale, FnPent, FnBlues, FnChordLock, FnArpUp, FnArpDown, FnRepeat, // melody (Oct and Glide and Hold shared)
    FnSwing, FnLatch, FnSoft, FnUndo,                                                       // drums
    FnCount
};
const char* roleFuncName(int fn);

struct RolePad
{
    RoleKind kind = RoleKind::None;
    uint8_t index = 0;  // Note: the degree 0 to 7; Func: a RoleFunc; Kit: the kit 0 to 23; Rate: 0 to 3 (1/8, 1/16, 1/32, 1/16T); Drum: the voice 0 to 15
    uint8_t octave = 0; // Note: 0 for the top row, 1 the middle, 2 the bottom
};
RolePad rolePad(TrackRole role, int pad);

// The note of a scale degree (0 to 7, the eighth is the octave) on one of the three octave rows.
// mode: 0 the scale, 1 pentatonic, 2 blues. `octaveShift` is the Oct - / Oct + pads, in octaves.
int roleNote(const ChordCfg& cfg, TrackRole role, int mode, int octaveRow, int degree, int octaveShift);
// The notes of the safe row (the tones of the chord, over two octaves): column 0 to 7.
int roleSafeNote(TrackRole role, const uint8_t* chord, int count, int col, int octaveShift);
// Bass functions: the note a function pad plays for a chord (root, 5th, 3rd, octave, ...); -1 for none.
int roleFuncNote(TrackRole role, int fn, const uint8_t* chord, int count, const ChordCfg& cfg, int octaveShift);
// The rate of a repeat pad in beats (1/8 is 0.5)
float roleRepeatBeats(int rate);
} // namespace ft

#endif /* FT_ROLES_H_ */
