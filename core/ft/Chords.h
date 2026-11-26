#ifndef FT_CHORDS_H_
#define FT_CHORDS_H_

#include <stdint.h>

namespace ft
{
// Chords and scale notes for the 48 pads. The theory (scales, chord shapes, degree qualities) follows
// the chord engine of the owner's chroma-chords app, boiled down to tables: no next-chord
// suggestion, because the whole set is on the pads. Portable, no libc: the emulator and the Tracker
// run the same code.
//
// The pads are 12 columns by 4 rows; pad numbers run along the rows, so pad 0 is the top left. They
// are split into regions, so a layout is a short list of rectangles rather than fixed rows:
//   Performance: the two right-hand columns (8 pads), played with the right hand.
//   Chords: the top `chordRows` rows of the other ten columns. Each row is one chord type (Triad,
//           7th, 9th, ...). The columns are the 7 degrees of the scale, then 3 colour chords.
//   Notes:  the rest of those ten columns, scale notes rising left to right and from the bottom up.
// A pad is always the same degree for a key and scale, so playing by memory works.

constexpr int kScaleCount = 9;
constexpr int kStyleCount = 5;
constexpr int kMaxChordNotes = 6;
constexpr int kColourChords = 3;
constexpr int kPlayColumns = 10;   // columns 0 to 9 are chords and notes; 10 and 11 are performance pads
constexpr int kChordTypes = 12;
constexpr int kColourKinds = 4;

// What a chord row plays. The seventh and ninth follow the scale ("diatonic"): the 7th on ii is m7,
// on V it is a dominant 7. The others are literal over the degree's own triad (Maj7, 6, add9, add4)
// or replace its third (sus2, sus4, 7sus4), so they can leave the key; those pads light dimmer.
enum class ChordType : uint8_t
{
    Triad, Sixth, Seventh, MajSeventh, Ninth, Add9, Add4, Sus2, Sus4, SixNine, SevenSus4, Power
};

enum class NoteMode : uint8_t
{
    Scale,   // the rows are the scale's notes (fixed to the key)
    Chord,   // the rows are the notes of the chord played last, across octaves
    Follow   // as Scale, and the pad lights mark the tones of the chord played last
};

struct ChordCfg
{
    uint8_t key = 0;         // 0 C .. 11 B
    uint8_t scale = 0;       // see scaleName
    uint8_t style = 0;       // see styleName
    uint8_t chordRows = 2;   // 0 to 4; the rest of the rows are notes
    NoteMode noteMode = NoteMode::Scale;
    int8_t octave = 0;       // -2 to +2 octaves
    uint16_t shuffle = 0;    // 0: the style's own colour chords
    uint8_t rowType[4] = { 0, 2, 4, 8 };        // the chord type of each row (Triad, 7th, 9th, Sus4)
    uint8_t colSemi[kColourChords] = { 1, 3, 5 }; // colour chords: semitones above the tonic (1 to 11)
    uint8_t colKind[kColourChords] = { 1, 1, 2 }; // and their kind, see kindName
    // how the chords are played
    uint8_t playStyle = 0;   // see playStyleName
    uint8_t humanise = 0;    // 0 to 100: velocity, timing and gate looseness
    uint8_t strumMs = 28;    // time between the notes of a strum, 10 to 80
    uint8_t arpRate = 3;     // see arpRateName
    uint8_t arpRange = 1;    // octaves the arpeggio covers, 1 to 3
    uint8_t gate = 80;       // how long an arpeggio note lasts, percent of the step, 10 to 100
    uint8_t padVelocity = 0; // 1: the pad's own velocity is used, 0: a fixed one
};

constexpr int kPlayStyles = 7;
constexpr int kArpRates = 6;
enum PlayStyle : uint8_t { StyleBlock, StyleStrumDown, StyleStrumUp, StyleArpUp, StyleArpDown, StyleArpUpDown, StyleArpRandom };

// The right-hand performance pads
enum class Perf : uint8_t { None, OctUp, OctDown, Inversion, Open, Sus, Seventh, Hold, Style };
// Modifiers that act while their pad is held and a chord is played
enum Mod : uint8_t { ModOctUp = 1, ModOctDown = 2, ModInversion = 4, ModOpen = 8, ModSus = 16, ModSeventh = 32 };

// What a pad lights as
enum class Tier : uint8_t
{
    Off = 0,
    Other,      // a note or chord that is in the set but not special
    Outside,    // a chord that has notes outside the key
    ChordTone,  // a scale note that is in the chord played last (Follow mode)
    Colour,     // a colour chord
    Tonic,      // the key's tonic chord or note
    Control     // a performance pad
};

class Chords
{
  public:
    void init();
    ChordCfg cfg;
    uint8_t mods = 0;        // the performance modifiers held right now

    // Call after the settings change
    void rebuild();
    // Fills the colour chords (and with `rows` the row types) from the style and the shuffle
    void applyStyle(bool rows = true);

    // The notes a pad plays (MIDI numbers), 0 if the pad has no chord or note. Does not change state.
    int notes(int pad, uint8_t* out) const;
    // Tell the engine a pad was played (for the Chord and Follow modes)
    void played(int pad);
    Tier tier(int pad) const;
    static bool isPerf(int pad) { return pad >= 0 && pad < 48 && pad % 12 >= kPlayColumns; }
    static Perf perfOf(int pad);
    static uint8_t modFor(Perf p);
    bool isChordPad(int pad) const { return pad >= 0 && pad < 48 && !isPerf(pad) && pad / 12 < cfg.chordRows; }
    // "Dm7", "Bb", "F#", or the name of a performance pad
    void label(int pad, char* out, int size) const;

    static const char* scaleName(int s);
    static const char* styleName(int s);
    static const char* keyName(int k);
    static const char* typeName(int t);
    static const char* kindName(int k);
    static const char* perfName(Perf p);
    static const char* playStyleName(int s);
    static const char* arpRateName(int r);
    static float arpRateBeats(int r);   // the length of a step in beats
    static bool isArp(int style) { return style >= StyleArpUp; }
    // The order an arpeggio plays a chord's notes in, across `range` octaves; returns how many
    // (random order is picked step by step by the player: this returns them ascending).
    static int arpSequence(const uint8_t* chord, int count, int style, int range, uint8_t* out, int max);

    // The notes of a chord for tests: a degree 0 to 6 or a colour 7 to 9, and a type
    int degreeChord(int col, int type, uint8_t* out) const;

  private:
    struct Tones { int t, f, s, n; };  // third, fifth, seventh and ninth above the root, in semitones
    struct Chord
    {
        uint8_t root;    // pitch class
        uint8_t quality; // index into the shape table, 255 if unnamed
        uint8_t type;
        uint8_t third, fifth;
        uint8_t count;
        uint8_t offset[kMaxChordNotes]; // semitones above the root, ascending
        Tones tones;
    };
    Chord columnChord(int col, int type) const;
    Chord makeChord(int rootPc, const Tones& tones, int type) const;
    void placeChord(const Chord& c, bool withMods, uint8_t* out, int& n) const;
    bool inKey(const Chord& c) const;
    int scaleNote(int index) const; // MIDI note of the scale's index-th note counting from the bottom
    int noteRowIndex(int pad) const;
    int octaveShift() const;

    uint8_t lastNotes[kMaxChordNotes];
    int lastCount;
    uint8_t lastPc[12];
    bool lastHas;
};
} // namespace ft

#endif /* FT_CHORDS_H_ */
