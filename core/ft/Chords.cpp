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

// Colour chord kinds: the triad and the seventh they come with
struct KindDef { const char* name; uint8_t third, fifth, seventh; };
const KindDef kKinds[kColourKinds] = {
    { "7", 4, 7, 10 }, { "maj7", 4, 7, 11 }, { "m7", 3, 7, 10 }, { "dim", 3, 6, 9 }
};

// Styles: three colour chords each (semitones above the tonic, kind), and which of 7th and 9th the
// second chord row starts as (0: 7ths, 1: 9ths).
struct Col { uint8_t semi, kind; };
struct StyleDef { const char* name; uint8_t rich; Col c[kColourChords]; };
const StyleDef kStyles[kStyleCount] = {
    { "Lo-fi", 1, { { 1, 1 }, { 3, 1 }, { 5, 2 } } },
    { "Pop", 0, { { 4, 0 }, { 10, 0 }, { 5, 2 } } },
    { "Jazz-ish", 1, { { 2, 0 }, { 9, 0 }, { 1, 0 } } },
    { "Synthwave", 0, { { 8, 1 }, { 10, 0 }, { 3, 1 } } },
    { "Soul", 0, { { 10, 0 }, { 5, 2 }, { 3, 1 } } }
};

// What Shuffle picks the three colour chords from
const Col kPool[10] = {
    { 1, 1 }, { 2, 0 }, { 3, 1 }, { 4, 0 }, { 5, 2}, { 6, 3 }, { 8, 1 }, { 9, 0 }, { 10, 0 }, { 11, 3 }
};

struct TypeDef { const char* name; const char* suffix; };
const TypeDef kTypes[kChordTypes] = {
    { "Triad", "" }, { "6th", "6" }, { "7th", "7" }, { "Maj7", "maj7" }, { "9th", "9" }, { "Add9", "add9" },
    { "Add4", "add4" }, { "Sus2", "sus2" }, { "Sus4", "sus4" }, { "6/9", "6/9" }, { "7sus4", "7sus4" }, { "Power", "5" }
};

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
} // namespace

const char* Chords::scaleName(int s) { return kScales[s < 0 || s >= kScaleCount ? 0 : s].name; }
const char* Chords::styleName(int s) { return kStyles[s < 0 || s >= kStyleCount ? 0 : s].name; }
const char* Chords::keyName(int k) { return kKeys[((k % 12) + 12) % 12]; }
const char* Chords::typeName(int t) { return kTypes[t < 0 || t >= kChordTypes ? 0 : t].name; }
const char* Chords::kindName(int k) { return kKinds[k < 0 || k >= kColourKinds ? 0 : k].name; }

const char* Chords::perfName(Perf p)
{
    switch (p)
    {
        case Perf::OctUp: return "Oct +";
        case Perf::OctDown: return "Oct -";
        case Perf::Inversion: return "Invert";
        case Perf::Open: return "Open";
        case Perf::Sus: return "Sus";
        case Perf::Seventh: return "7th";
        case Perf::Hold: return "Hold";
        case Perf::Style: return "Style";
        default: return "";
    }
}

// Column 10: octave up, inversion, sus, hold. Column 11: octave down, open, 7th, spare.
Perf Chords::perfOf(int pad)
{
    if (!isPerf(pad)) return Perf::None;
    static const Perf map[4][2] = {
        { Perf::OctUp, Perf::OctDown }, { Perf::Inversion, Perf::Open }, { Perf::Sus, Perf::Seventh }, { Perf::Hold, Perf::Style }
    };
    return map[pad / 12][pad % 12 - kPlayColumns];
}

uint8_t Chords::modFor(Perf p)
{
    switch (p)
    {
        case Perf::OctUp: return ModOctUp;
        case Perf::OctDown: return ModOctDown;
        case Perf::Inversion: return ModInversion;
        case Perf::Open: return ModOpen;
        case Perf::Sus: return ModSus;
        case Perf::Seventh: return ModSeventh;
        default: return 0;
    }
}

const char* Chords::playStyleName(int s)
{
    static const char* const n[kPlayStyles] = { "Block", "Strum D", "Strum U", "Arp Up", "Arp Dn", "Arp U/D", "Arp Rnd" };
    return n[s < 0 || s >= kPlayStyles ? 0 : s];
}

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

void Chords::init()
{
    cfg = ChordCfg();
    mods = 0;
    lastCount = 0;
    lastHas = false;
    for (int i = 0; i < 12; i++) lastPc[i] = 0;
    for (int i = 0; i < kMaxChordNotes; i++) lastNotes[i] = 0;
    applyStyle();
    rebuild();
}

void Chords::applyStyle(bool rows)
{
    if (cfg.style >= kStyleCount) cfg.style = 0;
    const StyleDef& st = kStyles[cfg.style];
    if (rows)
    {
        cfg.rowType[0] = (uint8_t)ChordType::Triad;
        cfg.rowType[1] = (uint8_t)(st.rich ? ChordType::Ninth : ChordType::Seventh);
        cfg.rowType[2] = (uint8_t)(st.rich ? ChordType::Seventh : ChordType::Ninth);
        cfg.rowType[3] = (uint8_t)ChordType::Sus4;
    }
    if (cfg.shuffle == 0)
    {
        for (int i = 0; i < kColourChords; i++) { cfg.colSemi[i] = st.c[i].semi; cfg.colKind[i] = st.c[i].kind; }
        return;
    }
    // a seeded shuffle of the pool: pick three, keep them in order of pitch
    uint8_t order[10];
    for (int i = 0; i < 10; i++) order[i] = (uint8_t)i;
    uint32_t s = 2463534242u ^ ((uint32_t)cfg.shuffle * 2654435761u) ^ ((uint32_t)cfg.style << 20);
    for (int i = 9; i > 0; i--)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        const int j = (int)(s % (uint32_t)(i + 1));
        const uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    uint8_t pick[kColourChords];
    for (int i = 0; i < kColourChords; i++) pick[i] = order[i];
    for (int i = 1; i < kColourChords; i++)
        for (int j = i; j > 0 && kPool[pick[j]].semi < kPool[pick[j - 1]].semi; j--)
        { const uint8_t t = pick[j]; pick[j] = pick[j - 1]; pick[j - 1] = t; }
    for (int i = 0; i < kColourChords; i++) { cfg.colSemi[i] = kPool[pick[i]].semi; cfg.colKind[i] = kPool[pick[i]].kind; }
}

void Chords::rebuild()
{
    if (cfg.scale >= kScaleCount) cfg.scale = 0;
    if (cfg.style >= kStyleCount) cfg.style = 0;
    if (cfg.key >= 12) cfg.key = 0;
    if (cfg.chordRows > 4) cfg.chordRows = 4;
    if (cfg.octave < -2) cfg.octave = -2;
    if (cfg.octave > 2) cfg.octave = 2;
    for (int i = 0; i < 4; i++) if (cfg.rowType[i] >= kChordTypes) cfg.rowType[i] = 0;
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
    for (int i = 0; i < kColourChords; i++)
    {
        if (cfg.colSemi[i] < 1 || cfg.colSemi[i] > 11) cfg.colSemi[i] = 1;
        if (cfg.colKind[i] >= kColourKinds) cfg.colKind[i] = 0;
    }
    // the chord the notes follow starts as the tonic chord
    const uint8_t saved = mods;
    mods = 0;
    uint8_t tmp[kMaxChordNotes];
    const int n = degreeChord(0, 0, tmp);
    mods = saved;
    lastCount = n;
    for (int i = 0; i < n; i++) lastNotes[i] = tmp[i];
    for (int i = 0; i < 12; i++) lastPc[i] = 0;
    for (int i = 0; i < n; i++) lastPc[tmp[i] % 12] = 1;
    lastHas = true;
}

Chords::Chord Chords::makeChord(int rootPc, const Tones& tn, int type) const
{
    Chord c;
    c.root = (uint8_t)rootPc;
    c.type = (uint8_t)type;
    c.third = (uint8_t)tn.t;
    c.fifth = (uint8_t)tn.f;
    c.tones = tn;
    for (int i = 0; i < kMaxChordNotes; i++) c.offset[i] = 0;
    uint8_t iv[kMaxChordNotes];
    int n = 0;
    iv[n++] = 0;
    const int t = tn.t, f = tn.f;
    switch ((ChordType)type)
    {
        case ChordType::Triad: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; break;
        case ChordType::Sixth: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = 9; break;
        case ChordType::Seventh: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = (uint8_t)tn.s; break;
        case ChordType::MajSeventh: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = 11; break;
        case ChordType::Ninth:
            iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = (uint8_t)tn.s;
            if (tn.n == 2) iv[n++] = 14; // a flat 9th clashes: leave it out
            break;
        case ChordType::Add9: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = 14; break;
        case ChordType::Add4: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = 17; break;
        case ChordType::Sus2: iv[n++] = 2; iv[n++] = 7; break;
        case ChordType::Sus4: iv[n++] = 5; iv[n++] = 7; break;
        case ChordType::SixNine: iv[n++] = (uint8_t)t; iv[n++] = (uint8_t)f; iv[n++] = 9; iv[n++] = 14; break;
        case ChordType::SevenSus4: iv[n++] = 5; iv[n++] = 7; iv[n++] = 10; break;
        case ChordType::Power: iv[n++] = 7; iv[n++] = 12; break;
    }
    c.count = (uint8_t)n;
    for (int i = 0; i < n; i++) c.offset[i] = iv[i];
    c.quality = (uint8_t)findShape(iv, n);
    return c;
}

Chords::Chord Chords::columnChord(int col, int type) const
{
    Tones tn;
    int rootSemi;
    if (col < 7)
    {
        const uint8_t* sc = kScales[cfg.scale].semi;
        rootSemi = sc[col];
        auto up = [&](int step) { return (int)((sc[(col + step) % 7] - rootSemi + 12) % 12); };
        tn = Tones{ up(2), up(4), up(6), up(1) };
    }
    else
    {
        const int i = col - 7 < kColourChords ? col - 7 : kColourChords - 1;
        const KindDef& k = kKinds[cfg.colKind[i]];
        rootSemi = cfg.colSemi[i];
        tn = Tones{ k.third, k.fifth, k.seventh, 2 };
    }
    return makeChord((cfg.key + rootSemi) % 12, tn, type);
}

int Chords::degreeChord(int col, int type, uint8_t* out) const
{
    int n = 0;
    placeChord(columnChord(col, type), true, out, n);
    return n;
}

int Chords::octaveShift() const
{
    return 12 * cfg.octave + ((mods & ModOctUp) ? 12 : 0) - ((mods & ModOctDown) ? 12 : 0);
}

void Chords::placeChord(const Chord& c, bool withMods, uint8_t* out, int& n) const
{
    uint8_t off[kMaxChordNotes + 1];
    int count = c.count;
    for (int i = 0; i < count; i++) off[i] = c.offset[i];
    if (withMods)
    {
        const ChordType t = (ChordType)c.type;
        const bool hasThird = t != ChordType::Sus2 && t != ChordType::Sus4 && t != ChordType::SevenSus4 && t != ChordType::Power;
        if ((mods & ModSus) && hasThird) off[1] = (uint8_t)(c.third == 3 ? 2 : 5);
        if ((mods & ModSeventh) && (t == ChordType::Triad || t == ChordType::Add9 || t == ChordType::Add4 || t == ChordType::Sus2 || t == ChordType::Sus4) && count < kMaxChordNotes)
        {
            off[count++] = (uint8_t)c.tones.s;
            sortAscending(off, count);
        }
        if ((mods & ModInversion) && count > 1)
        {
            off[0] = (uint8_t)(off[0] + 12);
            sortAscending(off, count);
        }
        if ((mods & ModOpen) && count > 2)
        {
            off[1] = (uint8_t)(off[1] + 12);
            sortAscending(off, count);
        }
    }
    const int pc = c.root;
    const int base = 48 + (withMods ? octaveShift() : 12 * cfg.octave) + (pc >= 6 ? pc - 12 : pc);
    n = 0;
    for (int i = 0; i < count; i++)
    {
        const int note = base + off[i];
        if (note >= 0 && note <= 127) out[n++] = (uint8_t)note;
    }
}

bool Chords::inKey(const Chord& c) const
{
    uint16_t scaleMask = 0;
    for (int i = 0; i < 7; i++) scaleMask |= (uint16_t)(1u << ((cfg.key + kScales[cfg.scale].semi[i]) % 12));
    for (int i = 0; i < c.count; i++)
        if (!((scaleMask >> ((c.root + c.offset[i]) % 12)) & 1)) return false;
    return true;
}

int Chords::scaleNote(int index) const
{
    const int k = cfg.key >= 6 ? cfg.key - 12 : cfg.key;
    const int note = 48 + octaveShift() + k + 12 * (index / 7) + kScales[cfg.scale].semi[index % 7];
    return note >= 0 && note <= 127 ? note : 0;
}

int Chords::noteRowIndex(int pad) const { return (3 - pad / 12) * kPlayColumns + pad % 12; }

int Chords::notes(int pad, uint8_t* out) const
{
    if (pad < 0 || pad >= 48 || isPerf(pad)) return 0;
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows)
    {
        int n = 0;
        placeChord(columnChord(col, cfg.rowType[row]), true, out, n);
        return n;
    }
    const int i = noteRowIndex(pad);
    int note;
    if (cfg.noteMode == NoteMode::Chord && lastCount > 0)
        note = lastNotes[i % lastCount] - 12 + 12 * (i / lastCount) + (octaveShift() - 12 * cfg.octave);
    else
        note = scaleNote(i);
    if (note <= 0 || note > 127) return 0;
    out[0] = (uint8_t)note;
    return 1;
}

void Chords::played(int pad)
{
    if (pad < 0 || pad >= 48 || !isChordPad(pad)) return;
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
    if (isPerf(pad)) return Tier::Control;
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows)
    {
        if (col >= 7) return Tier::Colour;
        if (col == 0) return Tier::Tonic;
        return inKey(columnChord(col, cfg.rowType[row])) ? Tier::Other : Tier::Outside;
    }
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
    if (isPerf(pad)) { fmt(out, size, "%s", perfOf(pad) == Perf::Style ? playStyleName(cfg.playStyle) : perfName(perfOf(pad))); return; }
    const int row = pad / 12, col = pad % 12;
    if (row < cfg.chordRows)
    {
        const Chord c = columnChord(col, cfg.rowType[row]);
        if (c.quality != 255) { fmt(out, size, "%s%s", kKeys[c.root], kShapes[c.quality].name); return; }
        // not in the shape table: the root, the triad's mark, then the type
        const char* mark = c.third == 3 ? (c.fifth == 6 ? "dim" : "m") : (c.fifth == 8 ? "aug" : "");
        const ChordType t = (ChordType)c.type;
        const bool replacesThird = t == ChordType::Sus2 || t == ChordType::Sus4 || t == ChordType::SevenSus4 || t == ChordType::Power;
        fmt(out, size, "%s%s%s", kKeys[c.root], replacesThird ? "" : mark, kTypes[c.type].suffix);
        return;
    }
    uint8_t n[1];
    if (notes(pad, n) == 0) return;
    fmt(out, size, "%s", kKeys[n[0] % 12]);
}
} // namespace ft
