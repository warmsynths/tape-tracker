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
    { "mMaj7", 4, { 0, 3, 7, 11 } },  { "7sus4", 4, { 0, 5, 7, 10 } }, { "9sus4", 5, { 0, 5, 7, 10, 14 } },
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

// Colour chords: semitones above the tonic, the triad shape, the 7th shape
struct Col { uint8_t semi, triad, seventh; };
struct StyleDef { const char* name; uint8_t rich; Col c[5]; }; // rich: 0 the second row has 7ths, 1 it has 9ths
const StyleDef kStyles[kStyleCount] = {
    { "Lo-fi", 1, { { 1, qMaj, qMaj7 }, { 3, qMaj, qMaj7 }, { 5, qMin, qMin7 }, { 8, qMaj, qMaj7 }, { 10, qMaj, qDom7 } } },
    { "Pop", 0, { { 4, qMaj, qDom7 }, { 10, qMaj, qDom7 }, { 5, qMin, qMin7 }, { 8, qMaj, qMaj7 }, { 2, qMaj, qDom7 } } },
    { "Jazz-ish", 1, { { 2, qMaj, qDom7 }, { 9, qMaj, qDom7 }, { 1, qMaj, qDom7 }, { 5, qMin, qMin7 }, { 10, qMaj, qDom7 } } },
    { "Synthwave", 0, { { 8, qMaj, qMaj7 }, { 10, qMaj, qDom7 }, { 3, qMaj, qMaj7 }, { 5, qMin, qMin7 }, { 1, qMaj, qMaj7 } } },
    { "Soul", 0, { { 10, qMaj, qDom7 }, { 5, qMin, qMin7 }, { 3, qMaj, qMaj7 }, { 9, qMaj, qDom7 }, { 2, qMaj, qDom7 } } }
};

// What Shuffle picks the five colour chords from
const Col kPool[10] = {
    { 1, qMaj, qMaj7 }, { 2, qMaj, qDom7 }, { 3, qMaj, qMaj7 }, { 4, qMaj, qDom7 }, { 5, qMin, qMin7 },
    { 6, qDim, qDim7 }, { 8, qMaj, qMaj7 }, { 9, qMaj, qDom7 }, { 10, qMaj, qDom7 }, { 11, qDim, qDim7 }
};

int ninth(int q)
{
    switch (q)
    {
    case qDom7: return qDom9;
    case qMaj7: return qMaj9;
    case qMin7: return qMin9;
    default: return q;
    }
}

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
} // namespace

const char* Chords::scaleName(int s) { return kScales[s < 0 || s >= kScaleCount ? 0 : s].name; }
const char* Chords::styleName(int s) { return kStyles[s < 0 || s >= kStyleCount ? 0 : s].name; }
const char* Chords::keyName(int k) { return kKeys[((k % 12) + 12) % 12]; }
const char* Chords::qualityName(int q) { return q >= 0 && q < qCount ? kShapes[q].name : ""; }

void Chords::init()
{
    cfg = ChordCfg();
    lastCount = 0;
    lastHas = false;
    for (int i = 0; i < 12; i++) lastPc[i] = 0;
    for (int i = 0; i < kMaxChordNotes; i++) lastNotes[i] = 0;
    rebuild();
}

void Chords::rebuild()
{
    if (cfg.scale >= kScaleCount) cfg.scale = 0;
    if (cfg.style >= kStyleCount) cfg.style = 0;
    if (cfg.key >= 12) cfg.key = 0;
    if (cfg.chordRows > 4) cfg.chordRows = 4;
    if (cfg.octave < -2) cfg.octave = -2;
    if (cfg.octave > 2) cfg.octave = 2;
    buildColumns();
    // the chord the notes follow starts as the tonic chord
    uint8_t tmp[kMaxChordNotes];
    const int n = degreeChord(0, 0, tmp);
    lastCount = n;
    for (int i = 0; i < n; i++) lastNotes[i] = tmp[i];
    for (int i = 0; i < 12; i++) lastPc[i] = 0;
    for (int i = 0; i < n; i++) lastPc[tmp[i] % 12] = 1;
    lastHas = true;
}

void Chords::buildColumns()
{
    const StyleDef& st = kStyles[cfg.style];
    if (cfg.shuffle == 0)
    {
        for (int i = 0; i < 5; i++) colour[i] = Colour{ st.c[i].semi, st.c[i].triad, st.c[i].seventh };
        return;
    }
    // a seeded shuffle of the pool: pick five, keep them in order of pitch
    uint8_t order[10];
    for (int i = 0; i < 10; i++) order[i] = (uint8_t)i;
    uint32_t s = 2463534242u ^ ((uint32_t)cfg.shuffle * 2654435761u) ^ ((uint32_t)cfg.style << 20);
    for (int i = 9; i > 0; i--)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        const int j = (int)(s % (uint32_t)(i + 1));
        const uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    uint8_t pick[5];
    for (int i = 0; i < 5; i++) pick[i] = order[i];
    for (int i = 1; i < 5; i++)
        for (int j = i; j > 0 && kPool[pick[j]].semi < kPool[pick[j - 1]].semi; j--)
        { const uint8_t t = pick[j]; pick[j] = pick[j - 1]; pick[j - 1] = t; }
    for (int i = 0; i < 5; i++) colour[i] = Colour{ kPool[pick[i]].semi, kPool[pick[i]].triad, kPool[pick[i]].seventh };
}

Chords::Chord Chords::columnChord(int col, int variant) const
{
    // variant: 0 triad, 1 the style's richer row, 2 the other of 7th and 9th, 3 suspended
    const StyleDef& st = kStyles[cfg.style];
    const bool nine = variant == 1 ? st.rich == 1 : variant == 2 ? st.rich == 0 : false;
    const bool sev = variant == 1 || variant == 2;
    Chord c;
    c.count = 0;
    for (int i = 0; i < kMaxChordNotes; i++) c.offset[i] = 0;
    if (col < 7)
    {
        const uint8_t* sc = kScales[cfg.scale].semi;
        const int root = sc[col];
        auto up = [&](int step) { return (int)((sc[(col + step) % 7] - root + 12) % 12); };
        c.root = (uint8_t)((cfg.key + root) % 12);
        uint8_t iv[5];
        int n = 0;
        iv[n++] = 0;
        if (variant == 3)
        {
            iv[n++] = (uint8_t)(up(2) == 3 ? 2 : 5); // minor chords get sus2, the others sus4
            iv[n++] = 7;
        }
        else
        {
            iv[n++] = (uint8_t)up(2);
            iv[n++] = (uint8_t)up(4);
            if (sev) iv[n++] = (uint8_t)up(6);
            if (sev && nine && up(1) != 1) iv[n++] = (uint8_t)(up(1) + 12); // a flat 9th clashes: leave it out
        }
        for (int i = 0; i < n; i++) c.offset[i] = iv[i];
        c.count = (uint8_t)n;
        c.quality = (uint8_t)findShape(iv, n);
        return c;
    }
    const Colour& k = colour[col - 7];
    c.root = (uint8_t)((cfg.key + k.semi) % 12);
    int q = k.triad;
    if (variant == 3) q = (k.triad == qMin) ? qSus2 : qSus4;
    else if (sev) q = nine ? ninth(k.rich) : k.rich;
    const Shape& sh = kShapes[q];
    c.count = sh.n;
    for (int i = 0; i < sh.n; i++) c.offset[i] = sh.iv[i];
    c.quality = (uint8_t)q;
    return c;
}

int Chords::degreeChord(int degree, int variant, uint8_t* out) const
{
    int n = 0;
    placeChord(columnChord(degree, variant), out, n);
    return n;
}

void Chords::placeChord(const Chord& c, uint8_t* out, int& n) const
{
    const int pc = c.root;
    const int base = 48 + 12 * cfg.octave + (pc >= 6 ? pc - 12 : pc);
    n = 0;
    for (int i = 0; i < c.count; i++)
    {
        const int note = base + c.offset[i];
        if (note >= 0 && note <= 127) out[n++] = (uint8_t)note;
    }
}

int Chords::scaleNote(int index) const
{
    const int k = cfg.key >= 6 ? cfg.key - 12 : cfg.key;
    const int note = 48 + 12 * cfg.octave + k + 12 * (index / 7) + kScales[cfg.scale].semi[index % 7];
    return note >= 0 && note <= 127 ? note : 0;
}

int Chords::noteRowIndex(int pad) const { return (3 - pad / 12) * 12 + pad % 12; }

int Chords::notes(int pad, uint8_t* out) const
{
    if (pad < 0 || pad >= 48) return 0;
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows)
    {
        int n = 0;
        placeChord(columnChord(col, row), out, n);
        return n;
    }
    const int i = noteRowIndex(pad);
    int note;
    if (cfg.noteMode == NoteMode::Chord && lastCount > 0)
        note = lastNotes[i % lastCount] - 12 + 12 * (i / lastCount);
    else
        note = scaleNote(i);
    if (note <= 0 || note > 127) return 0;
    out[0] = (uint8_t)note;
    return 1;
}

void Chords::played(int pad)
{
    if (pad < 0 || pad >= 48 || pad / 12 >= cfg.chordRows) return;
    uint8_t tmp[kMaxChordNotes];
    const int n = notes(pad, tmp);
    if (n == 0) return;
    lastCount = n;
    for (int i = 0; i < n; i++) lastNotes[i] = tmp[i];
    for (int i = 0; i < 12; i++) lastPc[i] = 0;
    for (int i = 0; i < n; i++) lastPc[tmp[i] % 12] = 1;
    lastHas = true;
}

Tier Chords::tier(int pad) const
{
    if (pad < 0 || pad >= 48) return Tier::Off;
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows) return col == 0 ? Tier::Tonic : col < 7 ? Tier::Other : Tier::Colour;
    const int i = noteRowIndex(pad);
    if (cfg.noteMode == NoteMode::Chord)
        return lastCount > 0 && i % lastCount == 0 ? Tier::Tonic : Tier::ChordTone;
    if (i % 7 == 0) return Tier::Tonic;
    if (cfg.noteMode == NoteMode::Follow)
    {
        const int pc = (cfg.key + kScales[cfg.scale].semi[i % 7]) % 12;
        if (lastPc[pc]) return Tier::ChordTone;
    }
    return Tier::Other;
}

void Chords::label(int pad, char* out, int size) const
{
    out[0] = 0;
    if (pad < 0 || pad >= 48) return;
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows)
    {
        const Chord c = columnChord(col, row);
        if (c.quality == 255) fmt(out, size, "%s+", kKeys[c.root]);
        else fmt(out, size, "%s%s", kKeys[c.root], kShapes[c.quality].name);
        return;
    }
    uint8_t n[1];
    if (notes(pad, n) == 0) return;
    fmt(out, size, "%s", kKeys[n[0] % 12]);
}

void Chords::colourName(int col, char* out, int size) const { label(col, out, size); }
} // namespace ft
