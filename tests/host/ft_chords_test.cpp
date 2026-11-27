// The chord engine as the round 6 design lays it out
#include "Chords.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

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
static const int kRow1 = 0, kRow2 = 12, kRow3 = 24, kRow4 = 36;

int main()
{
    static Chords c;
    c.init();
    uint8_t n[8];
    char l[24];

    // the regions
    CHECK(Chords::info(0).role == Role::Chord && Chords::info(kRow2 + 7).role == Role::Chord, "left hand rows 1 and 2 are chords, columns 1 to 8");
    CHECK(Chords::info(kRow3).role == Role::Note && Chords::info(kRow4 + 7).role == Role::Note, "rows 3 and 4 are notes");
    CHECK(Chords::info(8).role == Role::Mod && Chords::info(8).index == 0 && Chords::info(11).index == 3, "row 1, right hand: sus2 sus4 6 7");
    CHECK(Chords::info(kRow2 + 8).index == 4 && Chords::info(kRow2 + 11).index == 7, "row 2, right hand: maj7 add9 9 flip");
    CHECK(Chords::info(kRow3 + 8).role == Role::Style && Chords::info(kRow3 + 8).index == 0 && Chords::info(kRow3 + 11).index == 3, "row 3: Block Strum dn Strum up Arp up");
    CHECK(Chords::info(kRow4 + 8).role == Role::Style && Chords::info(kRow4 + 8).index == 4, "row 4: Arp dn");
    CHECK(Chords::info(kRow4 + 9).role == Role::OctDown && Chords::info(kRow4 + 10).role == Role::OctUp && Chords::info(kRow4 + 11).role == Role::Hold, "row 4: Oct - Oct + Hold");

    // C major, Pop style (the style sets 7ths or 9ths on the top row; Pop is 7ths)
    c.cfg.style = 1; c.applyStyle(); c.rebuild();
    { const char* w[8] = { "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5", "Cmaj7" }; for (int i = 0; i < 8; i++) { label(c, kRow1 + i, l); CHECK(std::strcmp(l, w[i]) == 0, w[i]); } }
    { const char* w[8] = { "C", "Dm", "Em", "F", "G", "Am", "Bdim", "C" }; for (int i = 0; i < 8; i++) { label(c, kRow2 + i, l); CHECK(std::strcmp(l, w[i]) == 0, w[i]); } }
    CHECK(same(n, c.notes(kRow1, n), { 48, 52, 55, 59 }), "I as a 7th: Cmaj7 at C3");
    CHECK(same(n, c.notes(kRow1 + 4, n), { 55, 59, 62, 65 }), "V as a 7th: G7 stays in the C3 to B3 window");
    CHECK(same(n, c.notes(kRow2 + 6, n), { 59, 62, 65 }), "vii as a triad: B dim");
    // the families
    CHECK(c.family(kRow1) == Family::Home && c.family(kRow1 + 1) == Family::Away && c.family(kRow1 + 4) == Family::Tension && c.family(kRow1 + 6) == Family::Tension, "families of the degrees");
    CHECK(c.family(kRow2 + 2) == Family::Home && c.family(kRow2 + 3) == Family::Away && c.family(kRow2 + 5) == Family::Home, "iii and vi are Home, IV is Away");

    // the scale notes: two octaves, the lower row is the first
    label(c, kRow4, l);     CHECK(std::strcmp(l, "C3") == 0, "bottom left is C3");
    label(c, kRow4 + 1, l); CHECK(std::strcmp(l, "D") == 0, "then D");
    label(c, kRow4 + 7, l); CHECK(std::strcmp(l, "C4") == 0, "the eighth is C4");
    label(c, kRow3, l);     CHECK(std::strcmp(l, "C4") == 0, "the next row starts at C4");
    label(c, kRow3 + 7, l); CHECK(std::strcmp(l, "C5") == 0, "and ends at C5");
    CHECK(same(n, c.notes(kRow4, n), { 48 }) && same(n, c.notes(kRow3 + 7, n), { 72 }), "note numbers");
    CHECK(c.notes(8, n) == 0 && c.notes(kRow4 + 9, n) == 0, "the right hand makes no notes");

    // the modifiers
    c.mods = ModSus4;
    CHECK(same(n, c.notes(kRow2, n), { 48, 53, 55 }), "sus4 on a triad");
    c.mods = ModSus2;
    CHECK(same(n, c.notes(kRow2, n), { 48, 50, 55 }), "sus2 on a triad");
    c.mods = ModMaj7;
    label(c, kRow2 + 1, l); CHECK(std::strcmp(l, "DmM7") == 0, "maj7 held: ii becomes DmM7");
    CHECK(same(n, c.notes(kRow2 + 1, n), { 50, 53, 57, 61 }), "its notes");
    char out[8];
    c.outsideName(kRow2 + 1, out, sizeof out); CHECK(std::strcmp(out, "C#") == 0, "a note outside the key is named: C#");
    label(c, kRow1, l); CHECK(std::strcmp(l, "Cmaj7") == 0, "both rows change: the top row shows maj7 too");
    c.mods = ModSeven;
    label(c, kRow2 + 4, l); CHECK(std::strcmp(l, "G7") == 0, "7 held on a triad: G7");
    label(c, kRow2, l); CHECK(std::strcmp(l, "Cmaj7") == 0, "7 takes the seventh from the key");
    c.mods = ModSix;
    CHECK(same(n, c.notes(kRow2, n), { 48, 52, 55, 57 }), "6: C6");
    label(c, kRow2 + 1, l); CHECK(std::strcmp(l, "Dm6") == 0, "Dm6");
    c.mods = ModAdd9;
    CHECK(same(n, c.notes(kRow2, n), { 48, 52, 55, 62 }), "add9 on a triad");
    c.mods = ModNine;
    CHECK(same(n, c.notes(kRow2, n), { 48, 52, 55, 59, 62 }), "9: the seventh and the ninth");
    label(c, kRow2, l); CHECK(std::strcmp(l, "Cmaj9") == 0, "named Cmaj9");
    label(c, kRow2 + 2, l); CHECK(std::strcmp(l, "Em7") == 0, "no flat 9th on iii: it stays a 7th");
    c.mods = ModFlip;
    CHECK(same(n, c.notes(kRow2, n), { 52, 55, 60 }), "flip: first inversion");
    c.mods = ModSus4 | ModSeven | ModFlip;
    CHECK(c.notes(kRow2, n) == 4, "modifiers from different groups combine");
    c.mods = 0;
    CHECK(Chords::groupOf(ModSus2) == (ModSus2 | ModSus4) && Chords::groupOf(ModSix) == (ModSix | ModSeven | ModMaj7) && Chords::groupOf(ModNine) == (ModAdd9 | ModNine) && Chords::groupOf(ModFlip) == ModFlip, "the groups");

    // voicing
    c.cfg.voicing = Voicing::Open;
    CHECK(same(n, c.notes(kRow2, n), { 48, 55, 64 }), "Open: the second note up an octave");
    c.cfg.voicing = Voicing::Drop2;
    CHECK(same(n, c.notes(kRow1, n), { 43, 48, 52, 59 }), "Drop 2: the second from the top drops an octave");
    c.cfg.voicing = Voicing::Close;
    c.cfg.octave = 1;
    CHECK(same(n, c.notes(kRow2, n), { 60, 64, 67 }) && same(n, c.notes(kRow4, n), { 60 }), "octave +1 lifts chords and notes");
    c.cfg.octave = 0;

    // the style is a feel preset
    c.cfg.style = 0; c.applyStyle();
    CHECK(c.cfg.voicing == Voicing::Open && c.cfg.nineths && c.cfg.humanise == 30, "Lo-fi: open voicing, 9ths, some humanise");
    c.rebuild();
    label(c, kRow1, l); CHECK(std::strcmp(l, "Cmaj9") == 0, "Lo-fi: the top row is 9ths");
    label(c, kRow1 + 2, l); CHECK(std::strcmp(l, "Em7") == 0, "with no flat 9th");
    c.cfg.style = 1; c.applyStyle(); c.cfg.voicing = Voicing::Close; c.rebuild();

    // A minor and the minor-key families
    c.cfg.key = 9; c.cfg.scale = 1; c.rebuild();
    { const char* w[8] = { "Am", "Bdim", "C", "Dm", "Em", "F", "G", "Am" }; for (int i = 0; i < 8; i++) { label(c, kRow2 + i, l); CHECK(std::strcmp(l, w[i]) == 0, w[i]); } }
    CHECK(same(n, c.notes(kRow2, n), { 57, 60, 64 }), "A minor's i sits at A3, above C3 as the roots rise");
    label(c, kRow4, l); CHECK(std::strcmp(l, "A3") == 0, "the tonic note pad shows its octave");
    c.cfg.key = 0; c.cfg.scale = 0; c.rebuild();

    // the note pads mark the notes of the chord that sounded
    c.played(kRow1 + 4); // G7: G B D F
    CHECK(c.isChordTone(kRow4 + 4) && c.isChordTone(kRow4 + 6) && c.isChordTone(kRow4 + 1) && c.isChordTone(kRow4 + 3), "G B D F are chord tones");
    CHECK(!c.isChordTone(kRow4) && !c.isChordTone(kRow4 + 2), "C and E are not");

    // every pad in every key, scale, style and modifier stays in range
    bool ok = true;
    for (int s = 0; s < kScaleCount && ok; s++)
        for (int k = 0; k < 12 && ok; k++)
            for (int st = 0; st < kStyleCount && ok; st++)
                for (int mo = 0; mo < 256 && ok; mo += 7)
                    for (int v = 0; v < kVoicings && ok; v++)
                    {
                        c.cfg.key = (uint8_t)k; c.cfg.scale = (uint8_t)s; c.cfg.style = (uint8_t)st; c.applyStyle();
                        c.cfg.voicing = (Voicing)v; c.mods = (uint8_t)mo; c.rebuild();
                        for (int p = 0; p < 24 && p % 12 < 8; p++)
                        {
                            const int cnt = c.notes(p, n);
                            if (cnt < 3 || cnt > kMaxChordNotes) ok = false;
                            for (int i = 0; i < cnt; i++) if (n[i] < 24 || n[i] > 100) ok = false;
                            char t[24]; label(c, p, t);
                            if (t[0] == 0) ok = false;
                        }
                    }
    CHECK(ok, "chord pads stay in range with 3 to 6 notes and a name for every key, scale, style, modifier and voicing");
    c.init();

    // arpeggio order
    {
        const uint8_t ch[3] = { 48, 52, 55 };
        uint8_t o[24];
        int k = Chords::arpSequence(ch, 3, StyleArpUp, 1, o, 24);
        CHECK(k == 3 && o[0] == 48 && o[2] == 55, "arp up");
        k = Chords::arpSequence(ch, 3, StyleArpDown, 1, o, 24);
        CHECK(k == 3 && o[0] == 55 && o[2] == 48, "arp down");
        k = Chords::arpSequence(ch, 3, StyleArpUpDown, 1, o, 24);
        CHECK(k == 4 && o[0] == 48 && o[2] == 55 && o[3] == 52, "arp up and down does not play the ends twice");
        k = Chords::arpSequence(ch, 3, StyleArpUp, 2, o, 24);
        CHECK(k == 6 && o[3] == 60 && o[5] == 67, "two octaves");
        CHECK(Chords::arpRateBeats(3) == 0.25f && Chords::isArp(StyleArpRandom) && !Chords::isArp(StyleStrumUp), "rates and style kinds");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
