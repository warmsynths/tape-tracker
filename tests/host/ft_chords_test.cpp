// The chord engine: the same chords in, the same notes out, as the chroma-chords theory tables.
#include "Chords.h"

#include <cstdio>
#include <initializer_list>
#include <cstring>

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

static bool same(const uint8_t* a, int n, std::initializer_list<int> want)
{
    if ((int)want.size() != n) return false;
    int i = 0;
    for (int w : want) if (a[i++] != w) return false;
    return true;
}

static void label(Chords& c, int pad, char* out) { c.label(pad, out, 24); }
static void style(Chords& c, int st) { c.cfg.style = (uint8_t)st; c.cfg.shuffle = 0; c.applyStyle(true); c.rebuild(); }

int main()
{
    static Chords c;
    c.init();
    uint8_t n[8];
    char l[24];

    // C major: the seven chords of the key, triads on the top row
    const char* triads[7] = { "C", "Dm", "Em", "F", "G", "Am", "Bdim" };
    for (int d = 0; d < 7; d++)
    {
        label(c, d, l);
        CHECK(std::strcmp(l, triads[d]) == 0, triads[d]);
    }
    CHECK(same(n, c.notes(0, n), { 48, 52, 55 }), "pad 0 is C major at C3");
    CHECK(same(n, c.notes(1, n), { 50, 53, 57 }), "pad 1 is D minor");
    // 7ths on the second row (Pop; Lo-fi and Jazz-ish put 9ths there)
    style(c, 1);
    const char* sevenths[7] = { "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5" };
    for (int d = 0; d < 7; d++)
    {
        label(c, 12 + d, l);
        CHECK(std::strcmp(l, sevenths[d]) == 0, sevenths[d]);
    }
    CHECK(same(n, c.notes(16, n), { 43, 47, 50, 53 }), "pad 16 is G7 (G sits below middle so the voicing stays low)");

    // A minor
    style(c, 0); c.cfg.key = 9; c.cfg.scale = 1; c.rebuild();
    const char* am[7] = { "Am", "Bdim", "C", "Dm", "Em", "F", "G" };
    for (int d = 0; d < 7; d++)
    {
        label(c, d, l);
        CHECK(std::strcmp(l, am[d]) == 0, am[d]);
    }

    // the style's colour chords sit in columns 7 to 11
    c.cfg.key = 0; c.cfg.scale = 0; style(c, 1);
    label(c, 7, l);  CHECK(std::strcmp(l, "E") == 0, "Pop colour 1 is E (III)");
    label(c, 9, l);  CHECK(std::strcmp(l, "Fm") == 0, "Pop colour 3 is Fm (iv)");
    style(c, 0);
    label(c, 7, l);  CHECK(std::strcmp(l, "Db") == 0, "Lo-fi colour 1 is Db");
    label(c, 19, l); CHECK(std::strcmp(l, "Dbmaj9") == 0, "Lo-fi second row has 9ths");
    label(c, 14, l); CHECK(std::strcmp(l, "Em7") == 0, "no flat 9th on iii: it stays a 7th");
    label(c, 18, l); CHECK(std::strcmp(l, "Bm7b5") == 0, "and on vii");

    // suspended and 9ths with four chord rows: triad, 7th or 9th, the other, sus
    c.cfg.chordRows = 4; style(c, 1);
    label(c, 24 + 0, l); CHECK(std::strcmp(l, "Cmaj9") == 0, "third row is 9ths");
    label(c, 36 + 0, l); CHECK(std::strcmp(l, "Csus4") == 0, "fourth row is suspended");
    label(c, 36 + 1, l); CHECK(std::strcmp(l, "Dsus4") == 0, "the Sus4 type is sus4 on every degree");

    // the scale rows: the bottom row rises from the tonic, every pad in key
    c.cfg.chordRows = 2; c.cfg.noteMode = NoteMode::Scale; style(c, 0);
    CHECK(same(n, c.notes(36, n), { 48 }), "bottom left is the tonic");
    CHECK(same(n, c.notes(37, n), { 50 }), "then the second");
    CHECK(same(n, c.notes(43, n), { 60 }), "eighth note is the octave");
    CHECK(same(n, c.notes(24, n), { 48 + 12 + 5 }), "second note row continues the scale (index 10 of 10 per row)");
    CHECK(c.tier(36) == Tier::Tonic && c.tier(37) == Tier::Other, "tonic note is marked");
    CHECK(c.tier(0) == Tier::Tonic && c.tier(3) == Tier::Other && c.tier(8) == Tier::Colour, "chord tiers");

    // every pad in every key, scale and style stays in range and the chord pads always sound
    for (int s = 0; s < kScaleCount; s++)
        for (int k = 0; k < 12; k++)
            for (int st = 0; st < kStyleCount; st++)
            {
                c.cfg.key = (uint8_t)k; c.cfg.scale = (uint8_t)s; c.cfg.chordRows = 4; style(c, st);
                for (int p = 0; p < 48; p++)
                {
                    if (Chords::isPerf(p)) continue;
                    const int cnt = c.notes(p, n);
                    bool ok = cnt >= 3 && cnt <= 5;
                    for (int i = 0; i < cnt; i++) if (n[i] < 24 || n[i] > 100) ok = false;
                    if (!ok) { CHECK(false, "chord pad out of range"); s = 99; break; }
                }
            }

    // Follow marks the chord played last; Chord mode plays its tones
    c.cfg = ChordCfg(); c.cfg.noteMode = NoteMode::Follow; c.rebuild();
    c.played(4); // G major: G B D
    CHECK(c.tier(36 + 4) == Tier::ChordTone, "G is in the chord");
    CHECK(c.tier(36 + 6) == Tier::ChordTone && c.tier(36 + 1) == Tier::ChordTone, "B and D are in the chord");
    CHECK(c.tier(36 + 2) == Tier::Other, "E is not");
    c.cfg.noteMode = NoteMode::Chord; c.rebuild();
    c.played(4);
    CHECK(c.notes(36, n) == 1, "chord mode plays one note");
    CHECK(n[0] == 43 - 12, "bottom left is the chord's root an octave down");

    // shuffle: deterministic, changes the colours, keeps them apart, never touches the 7 degrees
    c.cfg = ChordCfg(); c.rebuild();
    char a[24], b[24];
    label(c, 7, a);
    c.cfg.shuffle = 5; c.applyStyle(false); c.rebuild();
    label(c, 7, b);
    char x[3][24];
    for (int i = 0; i < 3; i++) label(c, 7 + i, x[i]);
    c.cfg.shuffle = 5; c.applyStyle(false); c.rebuild();
    for (int i = 0; i < 3; i++) { char y[24]; label(c, 7 + i, y); CHECK(std::strcmp(x[i], y) == 0, "shuffle is repeatable"); }
    bool distinct = true;
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++) if (std::strcmp(x[i], x[j]) == 0) distinct = false;
    CHECK(distinct, "shuffle gives three different chords");
    int diff = 0;
    for (int sd = 1; sd < 40; sd++)
    {
        c.cfg.shuffle = (uint16_t)sd; c.applyStyle(false); c.rebuild();
        char y[24]; label(c, 7, y);
        if (std::strcmp(y, a) != 0) diff++;
    }
    CHECK(diff > 10, "shuffle varies");
    label(c, 0, a); CHECK(std::strcmp(a, "C") == 0, "shuffle leaves the degrees alone");
    c.cfg.shuffle = 0; c.applyStyle(false); c.rebuild();
    label(c, 7, a); CHECK(std::strcmp(a, "Db") == 0, "shuffle 0 is the style's own");

    // regions: the two right-hand columns are performance pads and play nothing themselves
    c.init();
    for (int r = 0; r < 4; r++)
        for (int col = 10; col < 12; col++)
        {
            CHECK(Chords::isPerf(r * 12 + col) && c.notes(r * 12 + col, n) == 0, "performance pads make no notes");
            CHECK(c.isChordPad(r * 12 + col) == false, "and are not chord pads");
        }
    CHECK(Chords::perfOf(10) == Perf::OctUp && Chords::perfOf(11) == Perf::OctDown && Chords::perfOf(46) == Perf::Hold, "perf pads are where the design says");
    CHECK(c.tier(10) == Tier::Control && c.tier(47) == Tier::Off, "perf pads light, the spare one does not");
    // chord types on the degrees (C major) and the rules for what fits the scale
    struct T { int type; const char* name[7]; };
    c.cfg.chordRows = 4; c.cfg.scale = 0; c.cfg.key = 0;
    auto rowAs = [&](int type) { c.cfg.rowType[0] = (uint8_t)type; c.rebuild(); };
    rowAs((int)ChordType::Seventh);
    { const char* w[7] = { "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5" }; for (int d = 0; d < 7; d++) { label(c, d, l); CHECK(std::strcmp(l, w[d]) == 0, w[d]); } }
    rowAs((int)ChordType::Sixth);
    { const char* w[7] = { "C6", "Dm6", "Em6", "F6", "G6", "Am6", "Bdim7" }; for (int d = 0; d < 7; d++) { label(c, d, l); CHECK(std::strcmp(l, w[d]) == 0, w[d]); } }
    rowAs((int)ChordType::MajSeventh);
    label(c, 1, l); CHECK(std::strcmp(l, "DmMaj7") == 0, "Maj7 is literal: it makes DmMaj7 on ii");
    CHECK(c.tier(1) == Tier::Outside && c.tier(0) == Tier::Tonic, "and a chord outside the key lights dim");
    rowAs((int)ChordType::Add9);
    label(c, 0, l); CHECK(std::strcmp(l, "Cadd9") == 0, "add9");
    CHECK(same(n, c.notes(0, n), { 48, 52, 55, 62 }), "Cadd9 notes");
    rowAs((int)ChordType::Add4);
    CHECK(same(n, c.notes(0, n), { 48, 52, 55, 65 }), "Cadd4 notes");
    rowAs((int)ChordType::SixNine);
    label(c, 0, l); CHECK(std::strcmp(l, "C6/9") == 0, "6/9");
    rowAs((int)ChordType::SevenSus4);
    label(c, 4, l); CHECK(std::strcmp(l, "G7sus4") == 0, "7sus4");
    rowAs((int)ChordType::Power);
    CHECK(same(n, c.notes(4, n), { 43, 50, 55 }), "power chord is root, fifth, octave (G)");
    rowAs((int)ChordType::Ninth);
    label(c, 2, l); CHECK(std::strcmp(l, "Em7") == 0, "9th drops a flat 9th on iii");
    label(c, 4, l); CHECK(std::strcmp(l, "G9") == 0, "and keeps the 9th on V");
    // a colour chord takes the same types
    c.cfg.colSemi[0] = 10; c.cfg.colKind[0] = 0; // bVII7
    rowAs((int)ChordType::Seventh);
    label(c, 7, l); CHECK(std::strcmp(l, "Bb7") == 0, "colour chord as a 7th");
    rowAs((int)ChordType::Sus4);
    label(c, 7, l); CHECK(std::strcmp(l, "Bbsus4") == 0, "colour chord as sus4");
    // the performance modifiers
    c.init();
    c.mods = ModOctUp;
    CHECK(same(n, c.notes(0, n), { 60, 64, 67 }), "Oct + lifts the chord an octave");
    c.mods = ModOctDown;
    CHECK(same(n, c.notes(0, n), { 36, 40, 43 }), "Oct - drops it");
    c.mods = ModInversion;
    CHECK(same(n, c.notes(0, n), { 52, 55, 60 }), "Invert moves the root up");
    c.mods = ModOpen;
    CHECK(same(n, c.notes(0, n), { 48, 55, 64 }), "Open spreads the second note up");
    c.mods = ModSus;
    CHECK(same(n, c.notes(0, n), { 48, 53, 55 }), "Sus replaces the third (C: sus4)");
    CHECK(same(n, c.notes(1, n), { 50, 52, 57 }), "and on a minor chord it is sus2");
    c.mods = ModSeventh;
    CHECK(same(n, c.notes(0, n), { 48, 52, 55, 59 }), "7th adds the scale's seventh to a triad");
    c.mods = ModSeventh | ModOctUp;
    CHECK(c.notes(0, n) == 4 && n[0] == 60, "modifiers combine");
    c.mods = ModOctUp;
    CHECK(same(n, c.notes(36, n), { 60 }), "Oct + also lifts the scale notes");
    c.mods = 0;
    // custom colour chords and row types survive a rebuild (they are the user's)
    c.cfg.colSemi[1] = 6; c.cfg.colKind[1] = 2; c.cfg.rowType[0] = (uint8_t)ChordType::Sus2;
    c.rebuild();
    label(c, 8, l); CHECK(std::strcmp(l, "F#sus2") == 0, "custom colour chord and row type are kept");
    // chordRows 0 has only notes and performance pads
    c.init(); c.cfg.chordRows = 0; c.rebuild();
    CHECK(c.notes(0, n) == 1, "with no chord rows the top rows are notes");
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
