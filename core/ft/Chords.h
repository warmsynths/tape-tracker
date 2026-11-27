#ifndef FT_CHORDS_H_
#define FT_CHORDS_H_

#include <stdint.h>

namespace ft
{
// Chords and scale notes for the 48 pads, as the round 6 design lays them out. Portable, no libc: the
// emulator and the Tracker run the same code. Theory (scales, chord shapes) follows the owner's
// chroma-chords app, boiled down to tables.
//
// The pads are 12 columns by 4 rows; pad numbers run along the rows, so pad 0 is the top left.
//   Left hand, columns 1 to 8:
//     row 1  the seven chords as sevenths, then the first again   (I ii iii IV V vi vii I)
//     row 2  the same chords as triads
//     rows 3 and 4  the scale's notes over two octaves (row 4 is the lower one; the C pads are C3, C4, C5)
//   Right hand, columns 9 to 12:
//     row 1  sus2  sus4  6  7           modifiers: hold to apply, double-tap to latch
//     row 2  maj7  add9  9  flip
//     row 3  Block  Strum dn  Strum up  Arp up       play styles (tap to choose)
//     row 4  Arp dn  Oct -  Oct +  Hold
// Modifiers combine one per group: sus2 or sus4; 6, 7 or maj7; add9 or 9; flip. While one is held every
// chord on the left becomes what it will play.

constexpr int kScaleCount = 9;
constexpr int kStyleCount = 5;
constexpr int kMaxChordNotes = 6;
constexpr int kVoicings = 3;
constexpr int kLeftColumns = 8;

enum Mod : uint8_t { ModSus2 = 1, ModSus4 = 2, ModSix = 4, ModSeven = 8, ModMaj7 = 16, ModAdd9 = 32, ModNine = 64, ModFlip = 128 };
constexpr int kMods = 8;

enum PlayStyle : uint8_t { StyleBlock, StyleStrumDown, StyleStrumUp, StyleArpUp, StyleArpDown, StyleArpUpDown, StyleArpRandom };
constexpr int kPlayStyles = 7;  // what the engine can do
constexpr int kStylePads = 5;   // what the pads offer
constexpr int kArpRates = 6;

enum class Voicing : uint8_t { Close, Open, Drop2 };

// "Style" is a feel preset: a voicing, humanise, strum speed, arpeggio settings and whether the top row
// is 7ths or 9ths. Choosing one fills those in; each can then be changed by itself.
struct ChordCfg
{
    uint8_t key = 0;         // 0 C .. 11 B
    uint8_t scale = 0;       // see scaleName
    uint8_t style = 1;       // see styleName (Pop: clean, 7ths)
    Voicing voicing = Voicing::Close;
    int8_t octave = 0;       // -2 to +2 octaves
    uint8_t playStyle = 0;   // see playStyleName
    uint8_t humanise = 0;    // 0 to 100: velocity, timing and gate looseness
    uint8_t strumMs = 28;    // time between the notes of a strum, 10 to 80
    uint8_t arpRate = 3;     // see arpRateName
    uint8_t arpRange = 1;    // octaves the arpeggio covers, 1 to 3
    uint8_t gate = 80;       // how long an arpeggio note lasts, percent of the step, 10 to 100
    uint8_t padVelocity = 0; // 1: the pad's own velocity is used, 0: a fixed one
    bool nineths = false;    // the top row is 9ths instead of 7ths (set by the style)
};

enum class Role : uint8_t { None, Chord, Note, Mod, Style, OctDown, OctUp, Hold };
enum class Family : uint8_t { Home, Away, Tension };

struct PadInfo
{
    Role role = Role::None;
    uint8_t index = 0;   // Mod: 0 to 7 (the Mod bit number); Style: 0 to 4; Chord and Note: the column, 0 to 7
    uint8_t row = 0;     // Chord and Note: the pad row 0 to 3
};

class Chords
{
  public:
    void init();
    ChordCfg cfg;
    uint8_t mods = 0;        // the modifiers in effect right now (held or latched)

    // Fills the feel settings from the style preset (voicing, humanise, strum, arpeggio, 7ths or 9ths)
    void applyStyle();
    // Keeps the settings in range
    void rebuild();

    static PadInfo info(int pad);
    // The notes a pad plays (MIDI numbers), 0 if it is not a chord or note pad. Does not change state.
    int notes(int pad, uint8_t* out) const;
    // Tell the engine a chord pad was played (the note pads mark the notes of the chord that sounds)
    void played(int pad);
    // The chord's family: Home (I, iii, vi), Away (ii, IV), Tension (V, vii).
    Family family(int pad) const;
    // For note pads: is it a tone of the chord played last. For chord pads: is every note in the key.
    bool isChordTone(int pad) const;
    bool inKey(int pad) const;
    bool isTonic(int pad) const;
    // "Dm7", "Bb", "D" ... or the name of a modifier, play style or control
    void label(int pad, char* out, int size) const;
    // The first note of a chord that is outside the key ("C#"), or an empty string
    void outsideName(int pad, char* out, int size) const;
    // "C4" for the tonic note pads, else the note name
    static const char* modName(int mod);
    static const char* scaleName(int s);
    static const char* styleName(int s);
    static const char* keyName(int k);
    static const char* voicingName(int v);
    static const char* playStyleName(int s);
    static const char* arpRateName(int r);
    static float arpRateBeats(int r);
    static bool isArp(int style) { return style >= StyleArpUp; }
    // The order an arpeggio plays a chord's notes in, across `range` octaves; returns how many
    // (random order is picked step by step by the player: this returns them ascending).
    static int arpSequence(const uint8_t* chord, int count, int style, int range, uint8_t* out, int max);
    // True if the style pad (0 to 4) is the play style in use
    bool styleChosen(int styleIdx) const { return cfg.playStyle == styleIdx; }
    // Mod groups: returns the other bits in the same group as `bit`
    static uint8_t groupOf(uint8_t bit);

  private:
    struct Chord
    {
        uint8_t root;     // pitch class
        uint8_t quality;  // index into the shape table, 255 if unnamed
        uint8_t count;
        int8_t offset[kMaxChordNotes]; // semitones above the root, ascending (after voicing)
    };
    Chord build(int col, int row) const;
    void place(const Chord& c, uint8_t* out, int& n) const;
    int tonicNote() const;                         // MIDI note of the tonic in the C3 to B3 window, with octave
    int scaleNoteAt(int index) const;
    uint16_t scaleMask() const;

    uint8_t lastNotes[kMaxChordNotes];
    int lastCount;
    uint16_t lastPcMask;
    Family lastFamily;
};
} // namespace ft

#endif /* FT_CHORDS_H_ */
