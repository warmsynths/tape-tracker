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
// Layout: 12 columns by 4 rows. Pad numbers run along the rows, so pad 0 is the top left. The top
// `chordRows` rows are chords; the rest are the scale's notes, rising left to right and from the
// bottom row up.
//   Chord rows, top down: triads, 7ths (or 9ths, per style), 9ths, suspended.
//   Columns: the 7 degrees of the scale, then 5 colour chords chosen by the style (or shuffled).
// A pad is always the same degree for a key and scale, so playing by memory works.

constexpr int kScaleCount = 9;
constexpr int kStyleCount = 5;
constexpr int kMaxChordNotes = 6;

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
};

// What a pad lights as
enum class Tier : uint8_t
{
    Off = 0,
    Other,      // a note or chord that is in the set but not special
    ChordTone,  // a scale note that is in the chord played last (Follow mode)
    Colour,     // a colour chord
    Tonic       // the key's tonic chord or note
};

class Chords
{
  public:
    void init();
    ChordCfg cfg;

    // Call after the settings change
    void rebuild();

    // The notes a pad plays (MIDI numbers), 0 if the pad has no chord or note. Does not change state.
    int notes(int pad, uint8_t* out) const;
    // Tell the engine a pad was played (for the Chord and Follow modes)
    void played(int pad);
    Tier tier(int pad) const;
    bool isChordPad(int pad) const { return pad >= 0 && pad < 48 && pad / 12 < cfg.chordRows; }
    // "Dm7", "Bb", "F#" ...
    void label(int pad, char* out, int size) const;
    // The 12 colour chords' names as "semitone quality" for the page: column 7 to 11
    void colourName(int col, char* out, int size) const;

    static const char* scaleName(int s);
    static const char* styleName(int s);
    static const char* keyName(int k);
    static const char* qualityName(int q);

    // The notes of a scale degree chord for tests: variant 0 triad, 1 7th, 2 9th, 3 suspended
    int degreeChord(int degree, int variant, uint8_t* out) const;

  private:
    struct Chord
    {
        uint8_t root;   // pitch class
        uint8_t quality; // index into the quality table, 255 if unnamed
        uint8_t count;
        uint8_t offset[kMaxChordNotes]; // semitones above the root, ascending
    };
    struct Colour { uint8_t semi; uint8_t triad; uint8_t rich; }; // semitone above the tonic, and qualities
    void buildColumns();
    Chord columnChord(int col, int variant) const;
    void placeChord(const Chord& c, uint8_t* out, int& n) const;
    int scaleNote(int index) const; // MIDI note of the scale's index-th note counting from the bottom
    int noteRowIndex(int pad) const;

    Colour colour[5];
    uint8_t lastNotes[kMaxChordNotes];
    int lastCount;
    uint8_t lastPc[12];
    bool lastHas;
};
} // namespace ft

#endif /* FT_CHORDS_H_ */
