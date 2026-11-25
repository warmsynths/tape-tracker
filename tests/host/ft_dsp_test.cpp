// Tests of the Four-Track audio maths (core/ft/dsp), the code the Tracker and the web emulator share.
#include "dsp/Dsp.h"

#include <cmath>
#include <cstdio>
#include <vector>

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

static std::vector<int16_t> ramp(uint32_t n) // sample i has value i (so reads are easy to check)
{
    std::vector<int16_t> v(n);
    for (uint32_t i = 0; i < n; i++) v[i] = (int16_t)i;
    return v;
}

int main()
{
    const uint32_t rate = 44100;
    {   // first note: silence, then a burst at a known place
        std::vector<int16_t> x(rate, 0);
        uint32_t seed = 1;
        for (uint32_t i = 0; i < rate; i++) { seed = seed * 1664525u + 1013904223u; x[i] = (int16_t)(((int32_t)(seed >> 16) % 7) - 3); }
        const uint32_t at = 20000;
        for (uint32_t i = at; i < at + 6000; i++) x[i] = (int16_t)(12000 * std::sin(i * 0.05));
        const int found = findFirstOnset(x.data(), 0, rate, rate);
        CHECK(found > 0 && std::abs(found - (int)at) < 100, "finds the first note within 100 samples");
        std::vector<int16_t> quiet(rate, 0);
        CHECK(findFirstOnset(quiet.data(), 0, rate, rate) == -1, "no note in silence");
        CHECK(findFirstOnset(x.data(), 21000, 30000, rate) == -1 || true, "a window that starts inside the note does not crash");
        // a range that excludes the note finds nothing
        CHECK(findFirstOnset(x.data(), 0, 15000, rate) == -1, "range before the note finds nothing");
    }
    {   // offsets: repeat wraps, play once cuts
        auto d = ramp(1000);
        TrackBank b;
        b.data[0] = d.data(); b.loop = 1000;
        b.cfg[0].offset = 100;
        float L[8], R[8];
        b.render(0, 8, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 900.0f) < 1.0f, "a later offset wraps the end of the part to the start (repeat)");
        CHECK(std::fabs(L[5] * 32768.0f - 905.0f) < 1.0f, "and continues");
        b.render(100, 4, L, R);
        CHECK(std::fabs(L[0]) < 1e-6f + 0.0f || std::fabs(L[0] * 32768.0f - 0.0f) < 1.0f, "the part starts at the offset");
        b.playOnce = true;
        b.render(0, 4, L, R);
        CHECK(L[0] == 0.0f && L[3] == 0.0f, "play once: audio pushed past the end is cut, not wrapped");
        b.render(100, 4, L, R);
        CHECK(std::fabs(L[1] * 32768.0f - 1.0f) < 1.0f, "play once: the part is heard from its offset");
        b.cfg[0].offset = -100; b.playOnce = false;
        b.render(0, 2, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 100.0f) < 1.0f, "an earlier offset reads ahead");
    }
    {   // mute and trim
        auto d = ramp(1000);
        TrackBank b;
        b.data[0] = d.data(); b.loop = 1000;
        float L[4], R[4];
        b.cfg[0].mute = true; b.render(10, 4, L, R);
        CHECK(L[0] == 0.0f, "a muted track is silent");
        b.cfg[0].mute = false; b.cfg[0].trimStart = 500; b.render(10, 4, L, R);
        CHECK(L[0] == 0.0f, "before the trim start is silent");
        b.render(500, 4, L, R);
        CHECK(L[0] > 0.0f, "from the trim start is audible");
        b.cfg[0].trimStart = 0; b.cfg[0].trimEnd = 600; b.render(700, 4, L, R);
        CHECK(L[0] == 0.0f, "after the trim end is silent");
    }
    {   // glitch effects work in slices
        auto d = ramp(1600);
        TrackBank b;
        b.data[0] = d.data(); b.loop = 1600; b.slice = 400;
        float L[1], R[1];
        b.cfg[0].slot[0].type = FxType::JumpBack; b.cfg[0].slot[0].on = true;
        b.render(450, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 50.0f) < 1.0f, "jump back plays the previous slice");
        b.cfg[0].slot[0].type = FxType::Reverse;
        b.render(410, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 789.0f) < 1.0f, "reverse plays a slice backwards");
        b.cfg[0].slot[0].type = FxType::Stutter; b.cfg[0].slot[0].amount = 30; // 1/4 of a slice
        b.render(530, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 430.0f) < 1.0f, "stutter repeats the start of the slice");
        b.cfg[0].slot[0].on = false; b.cfg[0].held[0] = true;
        b.render(530, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 430.0f) < 1.0f, "a held slot acts like a latched one");
        b.cfg[0].held[0] = false;
        b.render(530, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 530.0f) < 1.0f, "released: plain playback again");
    }
    {   // pitch: an octave up reads twice as fast within a slice
        auto d = ramp(1600);
        TrackBank b;
        b.data[0] = d.data(); b.loop = 1600; b.slice = 400;
        b.cfg[0].slot[0].type = FxType::Pitch; b.cfg[0].slot[0].on = true; b.cfg[0].slot[0].semis = 12;
        float L[1], R[1];
        b.render(450, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - (400.0f + 100.0f)) < 3.0f, "tape pitch +12 reads at twice the rate");
        b.cfg[0].slot[0].semis = -12;
        b.render(500, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - (400.0f + 50.0f)) < 3.0f, "tape pitch -12 reads at half the rate");
        b.cfg[0].granular = true; b.cfg[0].slot[0].semis = 0;
        b.render(450, 1, L, R);
        CHECK(std::fabs(L[0] * 32768.0f - 450.0f) < 3.0f, "granular at no shift is the original");
    }
    {   // lo-fi: bypass is identity, saturation is bounded, glue reduces a loud signal
        Lofi lofi;
        lofi.cfg.bypass = true;
        float l[64], r[64];
        for (int i = 0; i < 64; i++) l[i] = r[i] = 0.5f;
        lofi.process(l, r, 64, rate);
        CHECK(l[10] == 0.5f, "bypass leaves the signal alone");
        lofi.cfg.bypass = false; lofi.cfg.wowOn = false; lofi.cfg.glueOn = true; lofi.cfg.satOn = true; lofi.cfg.glue = 100; lofi.cfg.sat = 100;
        std::vector<float> a(4410, 0.9f), c(4410, 0.9f);
        lofi.process(a.data(), c.data(), 4410, rate);
        CHECK(std::fabs(a[4000]) <= 1.0f && a[4000] > 0.2f, "saturation and glue keep the level in range");
        CHECK(lofi.gainReductionDb() < 0, "glue reports gain reduction on a loud signal");
        lofi.reset(); lofi.cfg.wowOn = true; lofi.cfg.glueOn = false; lofi.cfg.satOn = false; lofi.cfg.wowDepth = 50;
        std::vector<float> s(8000), s2(8000);
        for (int i = 0; i < 8000; i++) s[i] = s2[i] = std::sin(i * 0.1f) * 0.5f;
        lofi.process(s.data(), s2.data(), 8000, rate);
        float peak = 0; for (int i = 4000; i < 8000; i++) peak = std::fmax(peak, std::fabs(s[i]));
        CHECK(peak > 0.3f && peak < 0.7f, "wow and flutter keeps the signal at about the same level");
    }
    {   // wav header
        uint8_t h[44];
        CHECK(wavHeader(h, 1000, 44100) == 44, "a WAV header is 44 bytes");
        CHECK(h[0] == 'R' && h[8] == 'W' && h[40] == 0xE8 && h[41] == 0x03, "the header names RIFF, WAVE and the data size");
        CHECK(h[24] == 0x44 && h[25] == 0xAC, "the sample rate is 44100");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
