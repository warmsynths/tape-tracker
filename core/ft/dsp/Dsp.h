#ifndef FT_DSP_H_
#define FT_DSP_H_

#include <stddef.h>
#include <stdint.h>

#include "SendFx.h"

// Audio maths of the Four-Track recorder. Portable and free of hardware calls and of the C
// library: the Tracker firmware and the web emulator run exactly this code. Everything here is
// unit-tested on a PC (tests/host/ft_dsp_test.cpp).

namespace ft
{
//------------------------------------------------------------------------------------------
// Finding the first note of a take

// Index of the first note in samples [from, to) of `x`, or -1 if none is heard. A 32-sample peak
// envelope with an adaptive noise floor, a 5 ms confirmation, a back-search to the true start and
// a zero-crossing snap (docs/research/onset-detection.md). `sampleRate` in Hz.
int findFirstOnset(const int16_t* x, uint32_t from, uint32_t to, uint32_t sampleRate);

// Index just after the last sound in [from, to), or -1 if there is none (the end of the audible part).
int findLastSound(const int16_t* x, uint32_t from, uint32_t to, uint32_t sampleRate);

// The zero crossing nearest to `pos` within `maxDistance` samples, or `pos` itself if there is none.
// Cutting there avoids a click.
uint32_t snapToZero(const int16_t* x, uint32_t frames, uint32_t pos, uint32_t maxDistance);

//------------------------------------------------------------------------------------------
// The four tracks, played with offsets, trims, glitch effects and per-track effects

enum class FxType : uint8_t
{
    None,
    JumpBack,   // each slice plays the previous slice
    JumpBack2,  // each slice plays the one two slices back
    Stutter,    // repeats the start of each slice (amount: how short)
    Reverse,    // each slice plays backwards
    Pitch,      // moves the pitch by `semis` (tape or granular style)
    Crush,      // bit-crush (amount: how hard)
    Drive,      // overdrive
    Pan,        // -100 to +100 (amount 0..200, 100 is centre)
    LowPass,
    HighPass,
    Volume,     // 0 to -48 dB
    RandomJumps, // each slice may jump back, stutter, reverse or jump back two (amount: chance per slice)
    PitchSteps,  // each slice may step to a musical interval (amount: chance; semis: which set of intervals)
    kCount
};

struct FxSlot
{
    FxType type = FxType::None;
    bool on = false;      // latched on
    uint8_t amount = 50;  // 0..100 (Pan: 0..200)
    int8_t semis = 7;     // Pitch: semitones, -12..12 (Pitch steps: which set of intervals)
    uint8_t chance = 100; // percent of slices it happens on while latched on; 100 is always. Held: always
};

constexpr int16_t kDefaultLevelDb10 = -60;

struct TrackCfg
{
    bool mute = false;
    int32_t offset = 0;             // frames the Part is shifted later (negative: earlier)
    uint32_t trimStart = 0;         // Part frames before this are silent
    uint32_t trimEnd = 0xFFFFFFFFu; // and after this
    uint32_t fadeIn = 0, fadeOut = 0;
    float gain = 1.0f;              // normalize
    FxSlot slot[4];
    // The track mixer (see C1 in the design)
    int16_t levelDb10 = -60;        // level in tenths of a dB, -600 (off) to +60; tracks start at -6 dB so four Parts have headroom
    int8_t pan = 0;                 // -100 left to +100 right
    int8_t tone = 0;                // -100 low-pass (darker) to +100 high-pass (thinner), 0 flat
    bool solo = false;
    uint8_t sendReverb = 0;         // how much of the track goes to the reverb, 0..100 (after level, mute and tone)
    uint8_t sendDelay = 0;          // and to the delay
    bool granular = false;          // pitch style: granular (same length) or tape (speed follows)
    bool held[4] = { false, false, false, false }; // slots held down right now (momentary)
};

// The cutoff of the tone control in Hz: below zero a low-pass, above zero a high-pass; 0 is flat
int toneCutoffHz(int tone);

class TrackBank
{
  public:
    TrackBank();

    TrackCfg cfg[4];
    const int16_t* data[4] = {};
    uint32_t loop = 0;          // loop length in frames, 0: nothing to play
    uint32_t slice = 0;         // length of a glitch slice (a half beat); 0: loop / 16
    bool playOnce = false;      // Repeat wraps shifted audio around the loop, play once cuts it
    uint16_t peak[4] = {};
    uint32_t pass = 0;          // how many times the loop has come round (the random effects differ each time)
    uint32_t lastPos = 0;      // largest sample of each track since the caller cleared it
    SendFx fx;                 // reverb and delay returns, added to the mix (see SendFx.h)

    // Renders `n` frames starting at loop position `pos` into stereo float buffers (-1..1).
    void render(uint32_t pos, int n, float* left, float* right);
    void resetState();

  private:
    float readAt(int t, float srcIndex) const;
    static constexpr int kMaxBlock = 1024;
    float sendRev[kMaxBlock] = {}, sendDel[kMaxBlock] = {};
    float lp[4] = {}, hp[4] = {}, hpPrev[4] = {};
    float toneLp[4] = {}, toneHp[4] = {};
    float crushHold[4] = {};
    uint8_t crushCount[4] = {};
};

//------------------------------------------------------------------------------------------
// The lo-fi master chain: wow and flutter, bit-crush, saturation, glue compression

struct LofiCfg
{
    bool bypass = true;   // the lo-fi master is opt in: it starts bypassed
    bool glueOn = true, satOn = true, wowOn = true, crushOn = false;
    uint8_t glue = 40;      // 0..100
    uint8_t sat = 60;       // 0..100
    uint8_t wowDepth = 25;  // 0..100
    uint8_t wowSpeed = 6;   // tenths of a Hz, 1..50
    uint8_t crushBits = 12; // 4..16
    uint8_t volume = 100;   // master volume 0..100 (100 is 0 dB, 0 is -48 dB)
};

class Lofi
{
  public:
    Lofi();
    LofiCfg cfg;
    // Processes `n` frames in place.
    void process(float* left, float* right, int n, uint32_t sampleRate);
    int gainReductionDb() const { return grDb; } // 0 or negative, whole dB
    void reset();

  private:
    static constexpr int kDelay = 4096;
    float dl[kDelay], dr[kDelay];
    int writePos = 0;
    float phase = 0.0f, phase2 = 0.0f;
    float env = 0.0f, gain = 1.0f;
    float holdL = 0.0f, holdR = 0.0f;
    int holdCount = 0;
    int grDb = 0;
};

//------------------------------------------------------------------------------------------
// WAV file header for 16-bit mono PCM; `dataBytes` is the size of the audio that follows.
size_t wavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate);

// What a WAV file holds, read from its first bytes (a kilobyte is enough). Only PCM of 8, 16
// or 24 bits is understood.
struct WavInfo
{
    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0, dataOffset = 0, dataBytes = 0;
    uint32_t frames() const { return (channels > 0 && bits > 0) ? dataBytes / (channels * (bits / 8u)) : 0; }
    uint32_t frameBytes() const { return channels * (bits / 8u); }
};
bool parseWav(const uint8_t* head, uint32_t len, WavInfo& out);
// One sample of one frame of PCM data as a 16-bit value (channels are mixed down by the caller)
int16_t wavSample16(const uint8_t* frame, int channel, int bits);
} // namespace ft

#endif /* FT_DSP_H_ */
