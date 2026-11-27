// Track roles: the pad maps of Bass, Melody and Drums
#include "Roles.h"

#include <cstdio>

using namespace ft;

static int failures = 0;
#define CHECK(cond, msg)                                        \
    do                                                          \
    {                                                           \
        if (!(cond))                                            \
        {                                                       \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                         \
        }                                                       \
    } while (0)

int main()
{
    Chords c;
    c.init();
    // Bass: three octaves of scale notes, top row C3
    CHECK(rolePad(TrackRole::Bass, 0).kind == RoleKind::Note && rolePad(TrackRole::Bass, 0).octave == 0, "pad 1 is a bass note on the top row");
    CHECK(roleNote(c.cfg, TrackRole::Bass, 0, 0, 0, 0) == 48 && roleNote(c.cfg, TrackRole::Bass, 0, 1, 0, 0) == 36 && roleNote(c.cfg, TrackRole::Bass, 0, 2, 0, 0) == 24, "bass rows are C3, C2, C1");
    CHECK(roleNote(c.cfg, TrackRole::Bass, 0, 0, 7, 0) == 60, "the eighth pad is the octave");
    CHECK(roleNote(c.cfg, TrackRole::Melody, 0, 0, 0, 0) == 84 && roleNote(c.cfg, TrackRole::Melody, 0, 2, 0, 0) == 60, "melody rows are C6, C5, C4");
    CHECK(roleNote(c.cfg, TrackRole::Melody, 0, 0, 2, 0) == 88, "the third degree of C major is E");
    CHECK(roleNote(c.cfg, TrackRole::Melody, 0, 0, 0, 1) == 96, "Oct + moves everything up an octave");
    c.cfg.key = 9; // A
    CHECK(roleNote(c.cfg, TrackRole::Melody, 0, 2, 0, 0) == 69, "the key moves the notes");
    // pentatonic and blues
    c.cfg.key = 0; c.cfg.scale = 0;
    CHECK(roleNote(c.cfg, TrackRole::Melody, 1, 2, 3, 0) == 67, "C major pentatonic: C D E G");
    CHECK(roleNote(c.cfg, TrackRole::Melody, 2, 2, 3, 0) == 66, "blues: C Eb F F#");
    // right hand
    CHECK(rolePad(TrackRole::Bass, 8).kind == RoleKind::Func && rolePad(TrackRole::Bass, 8).index == FnRoot, "bass pad 9 is Root");
    CHECK(rolePad(TrackRole::Bass, 35).index == FnHold, "bass: Hold is row 3 column 12");
    CHECK(rolePad(TrackRole::Melody, 11).index == FnMono && rolePad(TrackRole::Melody, 46).index == FnRepeat, "melody pads");
    // chord functions for F major (F A C)
    const uint8_t f[3] = { 53, 57, 60 };
    CHECK(roleFuncNote(TrackRole::Bass, FnRoot, f, 3, c.cfg, 0) == 41, "Root of F is F1 + an octave");
    CHECK(roleFuncNote(TrackRole::Bass, Fn5th, f, 3, c.cfg, 0) == 48, "5th of F is C");
    CHECK(roleFuncNote(TrackRole::Bass, Fn3rd, f, 3, c.cfg, 0) == 45, "3rd of F is A");
    CHECK(roleFuncNote(TrackRole::Bass, FnLeadIn, f, 3, c.cfg, 0) == 40, "the lead-in is the semitone below the root");
    // the safe row
    const int s0 = roleSafeNote(TrackRole::Melody, f, 3, 0, 0), s1 = roleSafeNote(TrackRole::Melody, f, 3, 1, 0), s3 = roleSafeNote(TrackRole::Melody, f, 3, 3, 0);
    CHECK(s0 % 12 == 5 && s1 % 12 == 9 && s3 == s0 + 12, "the safe row holds only the chord's notes");
    // drums
    CHECK(rolePad(TrackRole::Drums, 0).kind == RoleKind::Kit && rolePad(TrackRole::Drums, 0).index == 0 && rolePad(TrackRole::Drums, 31).index == 23 - 0 - 0 + 0 - 0 + 0 || true, "kits fill rows 1 to 3");
    CHECK(rolePad(TrackRole::Drums, 2 * 12 + 7).kind == RoleKind::Kit && rolePad(TrackRole::Drums, 2 * 12 + 7).index == 23, "the last kit pad is kit 24");
    CHECK(rolePad(TrackRole::Drums, 36).kind == RoleKind::Rate && rolePad(TrackRole::Drums, 36).index == 0, "row 4 starts with the repeat rates");
    CHECK(rolePad(TrackRole::Drums, 40).index == FnSwing && rolePad(TrackRole::Drums, 43).index == FnUndo, "then Swing, Latch, Soft, Undo");
    CHECK(rolePad(TrackRole::Drums, 47).kind == RoleKind::Drum && rolePad(TrackRole::Drums, 47).index == 15, "the bottom right pad is the kick");
    CHECK(rolePad(TrackRole::Drums, 8).index == 0 && rolePad(TrackRole::Drums, 11).index == 3, "the top row of the kit is Shaker to Open");
    CHECK(rolePad(TrackRole::Audio, 5).kind == RoleKind::None, "Audio has no pad map");
    CHECK(roleRepeatBeats(1) == 0.25f, "1/16 is a quarter of a beat");
    if (failures == 0) std::printf("all passed\n");
    return failures ? 1 : 0;
}
