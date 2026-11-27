#include "Roles.h"

namespace ft
{
namespace
{
const char* const kRoleNames[kRoleCount] = { "Chords", "Bass", "Melody", "Drums", "Audio" };
const char* const kRoleTags[kRoleCount] = { "CHORD", "BASS", "LEAD", "DRUM", "LINE" };
const char* const kRoleBlurbs[kRoleCount] = {
    "Chords and scale notes",
    "Scale notes in low octaves and root, 5th and 3rd of the chord",
    "Three octaves of scale notes and a row that always fits",
    "Kits, note repeat and the drum voices",
    "A line, mic or other input" };
const char* const kFuncNames[FnCount] = {
    "Root", "5th", "3rd", "Octave", "Root -", "5th -", "Walk", "Rest", "Oct -", "Oct +", "Glide", "Hold",
    "Lead-in", "Ghost", "Short", "Accent",
    "Bend +", "Bend -", "Vib", "Mono", "Scale", "Pent", "Blues", "Chord", "Arp up", "Arp dn", "Repeat",
    "Swing", "Latch", "Soft", "Undo" };

// Right hand of Bass and Melody: [row][col 8..11]
const uint8_t kBassRight[4][4] = {
    { FnRoot, Fn5th, Fn3rd, FnOctave },
    { FnRootDown, Fn5thDown, FnWalk, FnRest },
    { FnOctDown, FnOctUp, FnGlide, FnHold },
    { FnLeadIn, FnGhost, FnShort, FnAccent } };
const uint8_t kMelodyRight[4][4] = {
    { FnOctDown, FnOctUp, FnGlide, FnMono },
    { FnBendUp, FnBendDown, FnVib, FnAccent },
    { FnScale, FnPent, FnBlues, FnChordLock },
    { FnArpUp, FnArpDown, FnRepeat, FnHold } };
const uint8_t kDrumRate[4] = { 0, 1, 2, 3 };
const uint8_t kDrumFuncs[4] = { FnSwing, FnLatch, FnSoft, FnUndo };
} // namespace

const char* roleName(int r) { return kRoleNames[r < 0 || r >= kRoleCount ? 0 : r]; }
const char* roleTag(int r) { return kRoleTags[r < 0 || r >= kRoleCount ? 0 : r]; }
const char* roleBlurb(int r) { return kRoleBlurbs[r < 0 || r >= kRoleCount ? 0 : r]; }
const char* roleFuncName(int fn) { return kFuncNames[fn < 0 || fn >= FnCount ? 0 : fn]; }
float roleRepeatBeats(int rate) { static const float b[4] = { 0.5f, 0.25f, 0.125f, 1.0f / 6.0f }; return b[rate < 0 || rate > 3 ? 1 : rate]; }

RolePad rolePad(TrackRole role, int pad)
{
    RolePad p;
    if (pad < 0 || pad >= 48) return p;
    const int row = pad / 12, col = pad % 12;
    switch (role)
    {
        case TrackRole::Bass:
        case TrackRole::Melody:
            if (col < 8)
            {
                if (row < 3) { p.kind = RoleKind::Note; p.index = (uint8_t)col; p.octave = (uint8_t)row; }
                else { p.kind = RoleKind::Note; p.index = (uint8_t)col; p.octave = 3; } // the bottom row: the safe row
            }
            else
            {
                p.kind = RoleKind::Func;
                p.index = role == TrackRole::Bass ? kBassRight[row][col - 8] : kMelodyRight[row][col - 8];
            }
            break;
        case TrackRole::Drums:
            if (col < 8)
            {
                if (row < 3) { p.kind = RoleKind::Kit; p.index = (uint8_t)(row * 8 + col); }
                else if (col < 4) { p.kind = RoleKind::Rate; p.index = kDrumRate[col]; }
                else { p.kind = RoleKind::Func; p.index = kDrumFuncs[col - 4]; }
            }
            else { p.kind = RoleKind::Drum; p.index = (uint8_t)(row * 4 + (col - 8)); }
            break;
        default: break;
    }
    return p;
}

static int pentatonic(const ChordCfg& cfg, int mode, int degree)
{
    const uint8_t* sc = Chords::scaleSemis(cfg.scale);
    const bool minorish = sc[2] == 3;
    static const uint8_t major[5] = { 0, 2, 4, 7, 9 };
    static const uint8_t minor[5] = { 0, 3, 5, 7, 10 };
    static const uint8_t blues[6] = { 0, 3, 5, 6, 7, 10 };
    if (mode == 2) return 12 * (degree / 6) + blues[degree % 6];
    return 12 * (degree / 5) + (minorish ? minor[degree % 5] : major[degree % 5]);
}

int roleNote(const ChordCfg& cfg, TrackRole role, int mode, int octaveRow, int degree, int octaveShift)
{
    // top row C3 / C6, middle C2 / C5, bottom C1 / C4
    static const int bass[3] = { 48, 36, 24 };
    static const int melody[3] = { 84, 72, 60 };
    const int base = (role == TrackRole::Bass ? bass : melody)[octaveRow < 0 || octaveRow > 2 ? 0 : octaveRow];
    int semis;
    if (mode == 0)
    {
        const uint8_t* sc = Chords::scaleSemis(cfg.scale);
        semis = sc[degree % 7] + 12 * (degree / 7);
    }
    else semis = pentatonic(cfg, mode, degree);
    int note = base + cfg.key + semis + 12 * octaveShift;
    while (note > 127) note -= 12;
    while (note < 0) note += 12;
    return note;
}

int roleSafeNote(TrackRole role, const uint8_t* chord, int count, int col, int octaveShift)
{
    if (count <= 0) return -1;
    // the chord's pitch classes, over two octaves around C5 (melody) or C3 (bass), cycling up the row
    const int base = role == TrackRole::Bass ? 36 : 72;
    int pcs[8], n = 0;
    for (int i = 0; i < count && n < 8; i++) pcs[n++] = chord[i] % 12;
    const int idx = col % 8;
    const int octave = idx / n;
    const int note = base + 12 * octaveShift + ((pcs[idx % n] - base % 12 + 12) % 12) + 12 * octave;
    return note > 127 ? note - 12 : note;
}

int roleFuncNote(TrackRole role, int fn, const uint8_t* chord, int count, const ChordCfg& cfg, int octaveShift)
{
    (void)cfg;
    if (count <= 0) return -1;
    const int root = chord[0] % 12;
    int i3 = 4, i5 = 7;
    for (int i = 1; i < count; i++)
    {
        const int iv = (chord[i] - chord[0] + 120) % 12;
        if (iv == 3 || iv == 4) i3 = iv;
        else if (iv == 6 || iv == 7 || iv == 8) i5 = iv;
    }
    const int base = (role == TrackRole::Bass ? 24 : 48) + 12 * octaveShift;
    const int low = base + root;      // the root in the lower of the two octaves
    const int high = low + 12;        // and one octave up
    switch (fn)
    {
        case FnRoot: return high;
        case Fn5th: return high + i5;
        case Fn3rd: return high + i3;
        case FnOctave: return high + 12;
        case FnRootDown: return low;
        case Fn5thDown: return low + i5;
        case FnWalk: return high + 2;              // a passing step above the root
        case FnLeadIn: return high - 1;            // a semitone below the root, leading into it
        default: return -1;
    }
}
} // namespace ft
