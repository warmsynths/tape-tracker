#ifndef FT_DRUMS_H_
#define FT_DRUMS_H_

#include <stdint.h>
#include "Fast.h"

#include "plaits/dsp/drums/analog_bass_drum.h"
#include "plaits/dsp/drums/analog_snare_drum.h"
#include "plaits/dsp/drums/hi_hat.h"
#include "plaits/dsp/drums/synthetic_bass_drum.h"
#include "plaits/dsp/drums/synthetic_snare_drum.h"

namespace ft
{
// The drum voices of the Drums role: the kick, snare and hi-hat models of Plaits (MIT, see NOTICE.md) with
// small noise and sine voices of our own for the percussion. 16 voices laid out as the right hand of the pad
// map, and 24 ready-made kits (a kit is a set of tunings). Portable like the synth.
//
// Voices, in pad order (column 9 to 12, row 1 to 4): Shaker Tamb OpenAlt Open / Conga Rim HatAlt Hat /
// Clap Bell SnareAlt Snare / Tom1 Tom2 KickAlt Kick.
class DrumKit
{
  public:
    static constexpr int kVoices = 16;
    static constexpr int kKits = 24;
    static constexpr int kMaxBlock = 128;

    void init(float sampleRate);
    void setKit(int kit);
    int kit() const { return kitIndex; }
    void trigger(int voice, int velocity); // velocity 1 to 127
    bool active() const { return activeFor > 0; }
    // Adds `n` frames (n <= kMaxBlock) to left and right.
    FT_FASTRUN void render(float* left, float* right, int n);
    void reset();

    static const char* kitName(int kit);
    static const char* kitStyle(int kit); // HIP-HOP, DANCE or LIVE (eight kits each)
    static const char* voiceName(int voice);

  private:
    struct Simple
    {
        float env = 0.0f, phase = 0.0f, phase2 = 0.0f, lp = 0.0f, hp = 0.0f, age = 0.0f, gain = 0.0f;
    };

    void renderSimple(int voice, float* out, int n);

    float rate = 44100.0f;
    int kitIndex = 0;
    int activeFor = 0;  // blocks of sound left to render after the last hit
    float accent[kVoices] = {};
    bool pending[kVoices] = {};
    int ringFor[kVoices] = {};   // blocks a voice still has to ring after a hit; a silent voice is not rendered
    float chokeTarget = 1.0f, chokeGain = 1.0f; // the open hats shut when a closed one is played

    plaits::AnalogBassDrum analogKick, analogKickAlt, analogTom1, analogTom2;
    plaits::SyntheticBassDrum syntheticKick, syntheticKickAlt;
    plaits::AnalogSnareDrum analogSnare, analogSnareAlt;
    plaits::SyntheticSnareDrum syntheticSnare, syntheticSnareAlt;
    plaits::HiHat<plaits::SquareNoise, plaits::SwingVCA, true, true> hatA, openA;
    plaits::HiHat<plaits::RingModNoise, plaits::LinearVCA, false, false> hatB, openB;
    Simple simple[kVoices];
    uint32_t noise = 12345;
};
} // namespace ft

#endif /* FT_DRUMS_H_ */
