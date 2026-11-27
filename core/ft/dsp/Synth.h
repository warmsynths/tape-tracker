#ifndef FT_SYNTH_H_
#define FT_SYNTH_H_

#include <stdint.h>

#include "Drums.h"
#include "SynthPatch.h"
#include "plaits/dsp/fx/ensemble.h"
#include "plaits/dsp/fx/overdrive.h"
#include "plaits/dsp/oscillator/super_square_oscillator.h"
#include "plaits/dsp/oscillator/variable_saw_oscillator.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"

namespace ft
{
// A vintage-style polysynth built on Plaits' oscillators and ensemble (MIT, see NOTICE.md):
// two oscillators and a sub oscillator, a 24 dB per octave resonant low-pass, two envelopes, one
// low-frequency oscillator, drive and a stereo ensemble chorus. Portable: the web emulator and the
// Tracker run the same code.

class Synth
{
  public:
    static constexpr int kVoices = 6;
    static constexpr int kMaxBlock = 128;

    void init(float sampleRate);
    void setPatch(const SynthPatch& patch);
    void setPreset(int index) { preset = index; setPatch(synthPreset(index)); }
    const SynthPatch& patch() const { return p; }
    int presetIndex() const { return preset; }

    // A user macro on top of the patch, 0 to 1 (0.5: as the patch says)
    void setBrightness(float b) { brightness = b; }

    DrumKit drums;   // the Drums role plays through the same output

    void noteOn(int note, int velocity); // velocity 1 to 127
    void noteOff(int note);
    void allNotesOff();
    bool active() const;

    // Renders `n` frames (n <= kMaxBlock), overwriting left and right.
    void render(float* left, float* right, int n);

  private:
    struct Env
    {
        float value = 0.0f;
        uint8_t stage = 0; // 0 idle, 1 attack, 2 decay, 3 release
        void trigger() { stage = 1; }
        void release() { if (stage != 0) stage = 3; }
    };
    struct Voice
    {
        plaits::VariableShapeOscillator shapeA, shapeB;
        plaits::VariableSawOscillator sawA, sawB;
        plaits::SuperSquareOscillator squareA, squareB;
        Env amp, flt;
        float y[4] = {};       // the filter's four stages
        float subPhase = 0.0f;
        float note = 60.0f;    // as played (glide moves `pitch` towards it)
        float pitch = 60.0f;
        float vel = 1.0f;
        int midi = -1;
        bool gate = false;
        uint32_t age = 0;
        bool busy() const { return amp.stage != 0; }
    };

    void startVoice(Voice& v, int note, int velocity);
    void advance(Env& e, float attackRate, float decayK, float sustain, float releaseK, bool gate);

    SynthPatch p{};
    int preset = 0;
    float rate = 44100.0f;
    float brightness = 0.5f;
    Voice voice[kVoices];
    uint32_t clock = 0;
    float lfoPhase = 0.0f;
    uint32_t noiseState = 22222;
    plaits::Overdrive overdrive;
    plaits::Ensemble ensemble;
    float ensembleBuffer[1024] = {};
    float mixA[kMaxBlock], mixB[kMaxBlock], sum[kMaxBlock], outR[kMaxBlock];
};
} // namespace ft

#endif /* FT_SYNTH_H_ */
