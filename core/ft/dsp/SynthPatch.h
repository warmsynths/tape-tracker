#ifndef FT_SYNTH_PATCH_H_
#define FT_SYNTH_PATCH_H_

#include <stdint.h>

namespace ft
{
// What a sound is, without any of the signal processing: the settings of one synth patch, the ready
// made patches, the eight tweak knobs that sit on top of a patch, and a table of every raw setting for
// the Advanced page. No Plaits here, so the screens can use it.

enum class SynthOsc : uint8_t
{
    Shape,       // triangle to saw to square with pulse width (Plaits VariableShapeOscillator)
    Saw,         // saw with a moving notch (VariableSawOscillator)
    SuperSquare  // thick, detuned pulse (SuperSquareOscillator)
};

struct SynthPatch
{
    const char* name;
    const char* kind;  // Pad, Keys, Bass, Lead ...
    const char* blurb; // one line about what it is for
    SynthOsc osc;
    float shape;       // 0 triangle, 0.5 saw, 1 square (Shape); notch and slope (Saw); spread (SuperSquare)
    float pulse;       // pulse width / second shape control, 0 to 1
    float detune;      // oscillator B against A, in cents (0: off)
    float subLevel;    // square an octave down, 0 to 1
    float noise;       // 0 to 1
    float cutoff;      // 0 to 1 (about 60 Hz to 14 kHz, exponential)
    float resonance;   // 0 to 1
    float envAmount;   // filter envelope depth in octaves (can be negative)
    float keyTrack;    // 0 to 1: how much the cutoff follows the note
    float attack, decay, sustain, release;      // amp envelope, seconds and level
    float fAttack, fDecay, fSustain, fRelease;  // filter envelope
    float lfoRate;     // Hz
    float lfoPitch;    // semitones of vibrato
    float lfoCutoff;   // octaves
    float lfoPulse;    // 0 to 1
    float drive;       // 0 to 1
    float ensemble;    // chorus amount, 0 to 1
    float depth;       // chorus depth, 0 to 1
    float glide;       // seconds, mono mode only
    bool mono;
    float level;       // output level, 0 to 1
    float drift = 0.0f; // tape drift, 0 to 1: every voice wanders a little in pitch, slowly and differently (up to about 30 cents)
};

constexpr int kSynthPresets = 12;
const SynthPatch& synthPreset(int index);

// The tweak knobs: each moves several raw settings the way a player would think of them.
constexpr int kSynthMacros = 8;
constexpr int kMacroRange = 10; // each knob goes from -10 to +10; 0 leaves the patch as it is
struct SynthMacros
{
    int8_t v[kSynthMacros] = {};
    bool any() const { for (int i = 0; i < kSynthMacros; i++) if (v[i] != 0) return true; return false; }
};
const char* synthMacroHint(int index); // a few words on what the knob does
const char* synthMacroName(int index); // Bright, Reson, Attack, Release, Thick, Chorus, Drive, Motion
// The patch the knobs make: `base` with each knob applied.
SynthPatch synthApply(const SynthPatch& base, const SynthMacros& macros);

// Every raw setting, for the Advanced page. A parameter is a float, an oscillator choice or a switch.
constexpr int kSynthParams = 28;
const char* synthParamName(int index);
// Moves a setting one step up (dir +1) or down (-1); `fine` takes a smaller step
void synthParamStep(SynthPatch& p, int index, int dir, bool fine);
void synthParamText(const SynthPatch& p, int index, char* out, int size);
// A setting's value as a number (an oscillator choice is 0 to 2, a switch 0 or 1), and setting it
// (clamped to the range). These let a patch be saved and put back, and one setting be reset.
float synthParamGet(const SynthPatch& p, int index);
void synthParamSet(SynthPatch& p, int index, float value);
// The setting as a fraction of its range, for a bar (0 to 1)
float synthParamFraction(const SynthPatch& p, int index);
// The gain for a synth level in tenths of a dB (0 is 1.0)
float synthLevelAmp(int db10);
constexpr int kSynthLevelMinDb10 = -300, kSynthLevelMaxDb10 = 120;
} // namespace ft

#endif /* FT_SYNTH_PATCH_H_ */
