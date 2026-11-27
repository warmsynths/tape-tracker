#ifndef FT_SEND_FX_H_
#define FT_SEND_FX_H_

#include <stdint.h>

// Reverb and delay sends, and the live filter. Portable like the rest of core/ft/dsp: no hardware and no C
// library, so the Tracker firmware and the emulator run the same code (tests/host/ft_sendfx_test.cpp).
// Each track sends part of its (post-fader, post-mute) sound to one reverb and one delay; their returns are
// added to the loop mix before the lo-fi master. The live filter sweeps the whole mix.

namespace ft
{
constexpr int kDelayMax = 66150;          // longest delay: 1.5 s at 44.1 kHz
constexpr int kRevLines = 4, kRevLen = 2048, kApLen = 512;
constexpr int kDelayDivs = 12;

// Delay times as note values of the beat, shortest first (the design: 1/32 to 1/2, with triplets and dotted)
const char* delayDivName(int index);
float delayDivBeats(int index); // length in beats (a quarter note is 1)

struct SendFxCfg
{
    bool reverbOn = false;
    uint8_t reverbSize = 50;    // 0..100: how long it rings
    uint8_t reverbDamp = 50;    // 0..100: how fast the highs die
    uint8_t reverbReturn = 50;  // 0..100: level of the reverb in the mix
    bool delayOn = false;
    uint8_t delayDiv = 5;       // index into the note values (5 is 1/8)
    uint8_t delayFeedback = 40; // 0..95
    uint8_t delayTone = 50;     // 0..100: how bright the repeats stay
    uint8_t delayReturn = 50;   // 0..100
    uint32_t delayFrames = 22050; // the time in frames, worked out from the tempo by the application
};

// Where the effects keep their memory (about 300 KB): in external RAM on the Tracker.
struct SendFxMemory
{
    int16_t delay[2][kDelayMax];
    float rev[kRevLines][kRevLen];
    float ap[2][kApLen];
};

class SendFx
{
  public:
    SendFxCfg cfg;
    // Clears the memory and the state. Call it from the main loop, not the audio interrupt.
    void init(SendFxMemory* memory);
    // Adds the returns of the reverb and delay to `left` and `right`. `sendReverb` and `sendDelay` are the
    // tracks' summed sends (mono), `n` frames.
    void process(const float* sendReverb, const float* sendDelay, float* left, float* right, int n);

  private:
    SendFxMemory* mem = nullptr;
    int revPos[kRevLines] = {};
    int apPos[2] = {};
    float revLp[kRevLines] = {};
    int dPos = 0;
    float dLpL = 0.0f, dLpR = 0.0f;
};

//------------------------------------------------------------------------------------------
// Live filter: a sweep on the whole mix, held or latched

struct LiveFilterCfg
{
    uint8_t mode = 0;     // 0 LP-HP sweep (left of the middle closes a low-pass, right of it raises a high-pass), 1 LP, 2 HP, 3 Band
    uint8_t set = 50;     // 0..100: where the sweep sits (50 is the middle)
    bool held = false;
    bool latched = false;
    bool active() const { return held || latched; }
};

struct LiveFilterState
{
    float ic1[2] = {}, ic2[2] = {};
    float wet = 0.0f;     // 0 open, 1 fully filtered; moves smoothly
    float g = 0.0f;       // smoothed cutoff coefficient
};

// The cutoff in Hz a setting gives in a mode, and whether it is open (nothing is filtered) there
float liveFilterCutoff(const LiveFilterCfg& cfg, bool* low, bool* open);

void liveFilterProcess(const LiveFilterCfg& cfg, LiveFilterState& st, float* left, float* right, int n);
// The filter every audio node reads. The application sets it; the audio side only reads it.
LiveFilterCfg& liveFilter();
} // namespace ft

#endif /* FT_SEND_FX_H_ */
