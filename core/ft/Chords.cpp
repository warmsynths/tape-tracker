#include "Chords.h"

#include "MiniFmt.h"

namespace ft
{
namespace
{
// Chord shapes (chroma-chords QUALITY_INTERVALS, plus m7b5 for the 7th chord on the 7th degree)
enum Q : uint8_t
{
    qMaj, qMin, qDim, qAug, qDom7, qMin7, qMaj7, qDim7, qSus4, qSus2, qDom9, qMaj9, qMin9, qMaj6, qMin6, qMMaj7, qSus7, qSus9, qMin7b5, qCount
};
struct Shape { const char* name; uint8_t n; uint8_t iv[5]; };
const Shape kShapes[qCount] = {
    { "", 3, { 0, 4, 7 } },           { "m", 3, { 0, 3, 7 } },          { "dim", 3, { 0, 3, 6 } },
    { "aug", 3, { 0, 4, 8 } },        { "7", 4, { 0, 4, 7, 10 } },      { "m7", 4, { 0, 3, 7, 10 } },
    { "maj7", 4, { 0, 4, 7, 11 } },   { "dim7", 4, { 0, 3, 6, 9 } },    { "sus4", 3, { 0, 5, 7 } },
    { "sus2", 3, { 0, 2, 7 } },       { "9", 5, { 0, 4, 7, 10, 14 } },  { "maj9", 5, { 0, 4, 7, 11, 14 } },
    { "m9", 5, { 0, 3, 7, 10, 14 } }, { "6", 4, { 0, 4, 7, 9 } },       { "m6", 4, { 0, 3, 7, 9 } },
    { "mM7", 4, { 0, 3, 7, 11 } },  { "7sus4", 4, { 0, 5, 7, 10 } }, { "9sus4", 5, { 0, 5, 7, 10, 14 } },
    { "m7b5", 4, { 0, 3, 6, 10 } }
};

// Scales: the 7 steps in semitones from the tonic (chroma-chords SCALE_DEGREE_SEMITONES)
struct ScaleDef { const char* name; uint8_t semi[7]; };
const ScaleDef kScales[kScaleCount] = {
    { "Major", { 0, 2, 4, 5, 7, 9, 11 } },          { "Minor", { 0, 2, 3, 5, 7, 8, 10 } },
    { "Harmonic minor", { 0, 2, 3, 5, 7, 8, 11 } }, { "Melodic minor", { 0, 2, 3, 5, 7, 9, 11 } },
    { "Dorian", { 0, 2, 3, 5, 7, 9, 10 } },         { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
    { "Lydian", { 0, 2, 4, 6, 7, 9, 11 } },         { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
    { "Locrian", { 0, 1, 3, 5, 6, 8, 10 } }
};

const char* const kKeys[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

const char* const kSharpKeys[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Styles: feel presets
struct StyleDef { const char* name; Voicing voicing; uint8_t humanise, strumMs, arpRate, arpRange, gate; bool nineths; };
const StyleDef kStyles[kStyleCount] = {
    { "Lo-fi", Voicing::Open, 30, 40, 1, 1, 90, true },
    { "Pop", Voicing::Close, 0, 28, 3, 1, 80, false },
    { "Jazz-ish", Voicing::Drop2, 25, 35, 1, 2, 85, true },
    { "Synthwave", Voicing::Open, 0, 22, 3, 2, 70, false },
    { "Soul", Voicing::Close, 20, 45, 1, 1, 85, false }
};

const char* const kModNames[kMods] = { "sus2", "sus4", "6", "7", "maj7", "add9", "9", "flip" };
const char* const kStyleNames[kPlayStyles] = { "Block", "Strum dn", "Strum up", "Arp up", "Arp dn", "Arp u/d", "Arp rnd" };
const char* const kVoicingNames[kVoicings] = { "Close", "Open", "Drop 2" };

int findShape(const uint8_t* iv, int n)
{
    for (int q = 0; q < qCount; q++)
    {
        if (kShapes[q].n != n) continue;
        bool same = true;
        for (int i = 0; i < n; i++) if (kShapes[q].iv[i] != iv[i]) { same = false; break; }
        if (same) return q;
    }
    return 255;
}


void sortAscending(uint8_t* a, int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && a[j] < a[j - 1]; j--) { const uint8_t t = a[j]; a[j] = a[j - 1]; a[j - 1] = t; }
}

void sortAscending(int8_t* a, int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && a[j] < a[j - 1]; j--) { const int8_t t = a[j]; a[j] = a[j - 1]; a[j - 1] = t; }
}
} // namespace

const char* Chords::scaleName(int s) { return kScales[s < 0 || s >= kScaleCount ? 0 : s].name; }
const char* Chords::styleName(int s) { return kStyles[s < 0 || s >= kStyleCount ? 0 : s].name; }
const char* Chords::keyName(int k) { return kKeys[((k % 12) + 12) % 12]; }
const char* Chords::voicingName(int v) { return kVoicingNames[v < 0 || v >= kVoicings ? 0 : v]; }
const char* Chords::modName(int m) { return kModNames[m < 0 || m >= kMods ? 0 : m]; }
const char* Chords::playStyleName(int s) { return kStyleNames[s < 0 || s >= kPlayStyles ? 0 : s]; }

const char* Chords::arpRateName(int r)
{
    static const char* const n[kArpRates] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return n[r < 0 || r >= kArpRates ? 0 : r];
}

float Chords::arpRateBeats(int r)
{
    static const float b[kArpRates] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
    return b[r < 0 || r >= kArpRates ? 0 : r];
}

int Chords::arpSequence(const uint8_t* chord, int count, int style, int range, uint8_t* out, int max)
{
    uint8_t up[24];
    int n = 0;
    if (range < 1) range = 1;
    if (range > 3) range = 3;
    for (int o = 0; o < range; o++)
        for (int i = 0; i < count && n < 24; i++)
        {
            const int note = chord[i] + 12 * o;
            if (note <= 127) up[n++] = (uint8_t)note;
        }
    int m = 0;
    if (style == StyleArpDown)
        for (int i = n - 1; i >= 0 && m < max; i--) out[m++] = up[i];
    else if (style == StyleArpUpDown)
    {
        for (int i = 0; i < n && m < max; i++) out[m++] = up[i];
        for (int i = n - 2; i >= 1 && m < max; i--) out[m++] = up[i]; // the ends are not played twice
    }
    else
        for (int i = 0; i < n && m < max; i++) out[m++] = up[i];
    return m;
}

uint8_t Chords::groupOf(uint8_t bit)
{
    if (bit == ModSus2 || bit == ModSus4) return ModSus2 | ModSus4;
    if (bit == ModSix || bit == ModSeven || bit == ModMaj7) return ModSix | ModSeven | ModMaj7;
    if (bit == ModAdd9 || bit == ModNine) return ModAdd9 | ModNine;
    return ModFlip;
}

void Chords::init()
{
    cfg = ChordCfg();
    mods = 0;
    lastCount = 0;
    lastPcMask = 0;
    lastFamily = Family::Home;
    for (int i = 0; i < kMaxChordNotes; i++) lastNotes[i] = 0;
    applyStyle();
    rebuild();
}

void Chords::applyStyle()
{
    if (cfg.style >= kStyleCount) cfg.style = 0;
    const StyleDef& st = kStyles[cfg.style];
    cfg.voicing = st.voicing;
    cfg.humanise = st.humanise;
    cfg.strumMs = st.strumMs;
    cfg.arpRate = st.arpRate;
    cfg.arpRange = st.arpRange;
    cfg.gate = st.gate;
    cfg.nineths = st.nineths;
}

void Chords::rebuild()
{
    if (cfg.scale >= kScaleCount) cfg.scale = 0;
    if (cfg.style >= kStyleCount) cfg.style = 0;
    if (cfg.key >= 12) cfg.key = 0;
    if (cfg.octave < -2) cfg.octave = -2;
    if (cfg.octave > 2) cfg.octave = 2;
    if ((int)cfg.voicing >= kVoicings) cfg.voicing = Voicing::Close;
    if (cfg.playStyle >= kPlayStyles) cfg.playStyle = 0;
    if (cfg.humanise > 100) cfg.humanise = 100;
    if (cfg.strumMs < 10) cfg.strumMs = 10;
    if (cfg.strumMs > 80) cfg.strumMs = 80;
    if (cfg.arpRate >= kArpRates) cfg.arpRate = 3;
    if (cfg.arpRange < 1) cfg.arpRange = 1;
    if (cfg.arpRange > 3) cfg.arpRange = 3;
    if (cfg.gate < 10) cfg.gate = 10;
    if (cfg.gate > 100) cfg.gate = 100;
    if (cfg.padVelocity > 1) cfg.padVelocity = 0;
}

PadInfo Chords::info(int pad)
{
    PadInfo p;
    if (pad < 0 || pad >= 48) return p;
    const int row = pad / 12, col = pad % 12;
    p.row = (uint8_t)row;
    if (col < kLeftColumns)
    {
        p.role = row < 2 ? Role::Chord : Role::Note;
        p.index = (uint8_t)col;
        return p;
    }
    const int rc = col - kLeftColumns; // 0 to 3 across the right hand
    if (row < 2) { p.role = Role::Mod; p.index = (uint8_t)(row * 4 + rc); }
    else if (row == 2) { p.role = Role::Style; p.index = (uint8_t)rc; }
    else if (rc == 0) { p.role = Role::Style; p.index = 4; }
    else if (rc == 1) p.role = Role::OctDown;
    else if (rc == 2) p.role = Role::OctUp;
    else p.role = Role::Hold;
    return p;
}

uint16_t Chords::scaleMask() const
{
    uint16_t m = 0;
    for (int i = 0; i < 7; i++) m |= (uint16_t)(1u << ((cfg.key + kScales[cfg.scale].semi[i]) % 12));
    return m;
}

int Chords::tonicNote() const { return 48 + cfg.key + 12 * cfg.octave; }

int Chords::scaleNoteAt(int index) const
{
    const int note = tonicNote() + 12 * (index / 7) + kScales[cfg.scale].semi[index % 7];
    return note >= 0 && note <= 127 ? note : 0;
}

Chords::Chord Chords::build(int col, int row) const
{
    const int d = col % 7; // the eighth column is the first chord again
    const uint8_t* sc = kScales[cfg.scale].semi;
    const int rootSemi = sc[d];
    auto up = [&](int step) { return (int)((sc[(d + step) % 7] - rootSemi + 12) % 12); };
    const int third = up(2), fifth = up(4), seventh = up(6), ninth = up(1);
    Chord c;
    c.root = (uint8_t)((cfg.key + rootSemi) % 12);
    for (int i = 0; i < kMaxChordNotes; i++) c.offset[i] = 0;
    // the row's own colour: row 1 sevenths (or 9ths), row 2 triads; a held 6, 7, maj7, add9 or 9 changes both rows
    const bool topRow = row == 0;
    int8_t iv[kMaxChordNotes + 2];
    int n = 0;
    iv[n++] = 0;
    iv[n++] = (int8_t)((mods & ModSus2) && !(mods & ModSus4) ? 2 : (mods & ModSus4) ? 5 : third);
    iv[n++] = (int8_t)fifth;
    const bool groupB = (mods & (ModSix | ModSeven | ModMaj7)) != 0;
    const bool groupC = (mods & (ModAdd9 | ModNine)) != 0;
    bool wantSeventh = false;
    int seventhValue = seventh;
    if (mods & ModSix) { iv[n++] = 9; }
    else if (mods & ModMaj7) { wantSeventh = true; seventhValue = 11; }
    else if (mods & ModSeven) wantSeventh = true;
    else if (!groupB && topRow && !groupC) wantSeventh = true; // the top row is sevenths
    if ((mods & ModNine) && !(mods & ModSix) && !(mods & ModMaj7)) wantSeventh = true;
    if (wantSeventh) iv[n++] = (int8_t)seventhValue;
    const bool style9 = !groupB && !groupC && topRow && cfg.nineths;
    if ((mods & (ModAdd9 | ModNine)) || style9)
    {
        // the ninth is the scale's own second step, a major ninth, and left out when it would be flat
        if (ninth == 2) iv[n++] = 14;
        else if (mods & ModAdd9) iv[n++] = 14; // asked for explicitly
    }
    // order ascending
    sortAscending(iv, n);
    // flip: the lowest note goes up an octave
    if ((mods & ModFlip) && n > 1) { iv[0] = (int8_t)(iv[0] + 12); sortAscending(iv, n); }
    // voicing
    if (cfg.voicing == Voicing::Open && n > 2) { iv[1] = (int8_t)(iv[1] + 12); sortAscending(iv, n); }
    else if (cfg.voicing == Voicing::Drop2 && n > 3)
    {
        // the second note from the top drops an octave and becomes the lowest
        iv[n - 2] = (int8_t)(iv[n - 2] - 12);
        sortAscending(iv, n);
    }
    if (n > kMaxChordNotes) n = kMaxChordNotes;
    c.count = (uint8_t)n;
    for (int i = 0; i < n; i++) c.offset[i] = iv[i];
    // the name uses the notes as stacked from the root (before flip and voicing move them)
    uint8_t nm[kMaxChordNotes + 2];
    int nn = 0;
    nm[nn++] = 0;
    nm[nn++] = (uint8_t)((mods & ModSus2) && !(mods & ModSus4) ? 2 : (mods & ModSus4) ? 5 : third);
    nm[nn++] = (uint8_t)fifth;
    if (mods & ModSix) nm[nn++] = 9;
    else if (wantSeventh) nm[nn++] = (uint8_t)seventhValue;
    if (((mods & (ModAdd9 | ModNine)) || style9) && (ninth == 2 || (mods & ModAdd9))) nm[nn++] = 14;
    sortAscending(nm, nn);
    c.quality = (uint8_t)findShape(nm, nn);
    return c;
}

void Chords::place(const Chord& c, uint8_t* out, int& n) const
{
    const int base = 48 + c.root + 12 * cfg.octave;
    n = 0;
    for (int i = 0; i < c.count; i++)
    {
        const int note = base + c.offset[i];
        if (note >= 0 && note <= 127) out[n++] = (uint8_t)note;
    }
}

int Chords::notes(int pad, uint8_t* out) const
{
    const PadInfo pi = info(pad);
    if (pi.role == Role::Chord)
    {
        int n = 0;
        place(build(pi.index, pi.row), out, n);
        return n;
    }
    if (pi.role == Role::Note)
    {
        const int idx = (pi.row == 3 ? 0 : 7) + pi.index; // the lower row is the first octave
        const int note = scaleNoteAt(idx);
        if (note <= 0) return 0;
        out[0] = (uint8_t)note;
        return 1;
    }
    return 0;
}

void Chords::played(int pad)
{
    const PadInfo pi = info(pad);
    if (pi.role != Role::Chord) return;
    uint8_t tmp[kMaxChordNotes];
    const int n = notes(pad, tmp);
    if (n == 0) return;
    lastCount = n;
    lastPcMask = 0;
    for (int i = 0; i < n; i++) { lastNotes[i] = tmp[i]; lastPcMask |= (uint16_t)(1u << (tmp[i] % 12)); }
    lastFamily = family(pad);
}

Family Chords::family(int pad) const
{
    const PadInfo pi = info(pad);
    if (pi.role == Role::Note) return lastFamily;
    const int d = pi.index % 7;
    if (d == 0 || d == 2 || d == 5) return Family::Home;
    if (d == 1 || d == 3) return Family::Away;
    return Family::Tension;
}

bool Chords::isChordTone(int pad) const
{
    const PadInfo pi = info(pad);
    if (pi.role != Role::Note) return false;
    uint8_t n[1];
    if (notes(pad, n) == 0) return false;
    return (lastPcMask >> (n[0] % 12)) & 1;
}

bool Chords::inKey(int pad) const
{
    const PadInfo pi = info(pad);
    if (pi.role != Role::Chord) return true;
    uint8_t n[kMaxChordNotes];
    const int count = notes(pad, n);
    const uint16_t mask = scaleMask();
    for (int i = 0; i < count; i++) if (!((mask >> (n[i] % 12)) & 1)) return false;
    return true;
}

bool Chords::isTonic(int pad) const
{
    const PadInfo pi = info(pad);
    if (pi.role == Role::Chord) return pi.index % 7 == 0;
    if (pi.role == Role::Note) return (pi.index % 7) == 0;
    return false;
}

void Chords::outsideName(int pad, char* out, int size) const
{
    out[0] = 0;
    const PadInfo pi = info(pad);
    if (pi.role != Role::Chord) return;
    uint8_t n[kMaxChordNotes];
    const int count = notes(pad, n);
    const uint16_t mask = scaleMask();
    for (int i = 0; i < count; i++)
        if (!((mask >> (n[i] % 12)) & 1)) { fmt(out, size, "%s", kSharpKeys[n[i] % 12]); return; }
}

void Chords::label(int pad, char* out, int size) const
{
    out[0] = 0;
    const PadInfo pi = info(pad);
    switch (pi.role)
    {
        case Role::Chord:
        {
            const Chord c = build(pi.index, pi.row);
            if (c.quality != 255) { fmt(out, size, "%s%s", kKeys[c.root], kShapes[c.quality].name); return; }
            // not in the shape table: the root, then what was asked
            const uint8_t* sc = kScales[cfg.scale].semi;
            const int d = pi.index % 7;
            const int third = (sc[(d + 2) % 7] - sc[d] + 12) % 12, fifth = (sc[(d + 4) % 7] - sc[d] + 12) % 12;
            const char* mark = third == 3 ? (fifth == 6 ? "dim" : "m") : (fifth == 8 ? "aug" : "");
            const char* sus = (mods & ModSus4) ? "sus4" : (mods & ModSus2) ? "sus2" : "";
            const char* ext = (mods & ModSix) ? "6" : (mods & ModMaj7) ? "M7" : (mods & ModNine) ? "9" : (mods & ModAdd9) ? "add9" : (mods & ModSeven) ? "7" : "";
            fmt(out, size, "%s%s%s%s", kKeys[c.root], sus[0] ? "" : mark, sus, ext);
            return;
        }
        case Role::Note:
        {
            uint8_t n[1];
            if (notes(pad, n) == 0) return;
            const int idx = (pi.row == 3 ? 0 : 7) + pi.index;
            if (idx % 7 == 0) fmt(out, size, "%s%d", kSharpKeys[n[0] % 12], n[0] / 12 - 1); // the tonic shows its octave
            else fmt(out, size, "%s", kSharpKeys[n[0] % 12]);
            return;
        }
        case Role::Mod: fmt(out, size, "%s", modName(pi.index)); return;
        case Role::Style: fmt(out, size, "%s", playStyleName(pi.index)); return;
        case Role::OctDown: fmt(out, size, "Oct -"); return;
        case Role::OctUp: fmt(out, size, "Oct +"); return;
        case Role::Hold: fmt(out, size, "Hold"); return;
        default: return;
    }
}
} // namespace ft
