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
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
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
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
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
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
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
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
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
    {   // random jumps and pitch steps: stay in time, differ from pass to pass, change the sound
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
        const int L = 44100 * 2;
        std::vector<int16_t> d(L);
        for (int i = 0; i < L; i++) d[i] = (int16_t)(std::sin(i * 0.03f) * 8000 + (i / 4000 % 2) * 6000);
        b.data[0] = d.data();
        b.loop = L;
        b.slice = 11025;
        b.cfg[0].slot[0] = { FxType::RandomJumps, true, 100, 0 };
        std::vector<float> l1(L), r1(L), l2(L), r2(L);
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l1.data() + p, r1.data() + p);
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l2.data() + p, r2.data() + p);
        int diffPass = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - l2[i]) > 0.001f) diffPass++;
        CHECK(diffPass > L / 10, "random jumps are different on the next pass");
        TrackBank plain; for (auto& c : plain.cfg) c.levelDb10 = 0;
        plain.data[0] = d.data(); plain.loop = L; plain.slice = 11025;
        std::vector<float> lp(L), rp(L);
        for (int p = 0; p < L; p += 1000) plain.render(p, 1000 < L - p ? 1000 : L - p, lp.data() + p, rp.data() + p);
        int diffPlain = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - lp[i]) > 0.001f) diffPlain++;
        CHECK(diffPlain > L / 10, "random jumps change the sound");
        b.cfg[0].slot[0] = { FxType::PitchSteps, true, 100, 1 };
        b.pass = 0;
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l1.data() + p, r1.data() + p);
        int diffPitch = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - lp[i]) > 0.001f) diffPitch++;
        CHECK(diffPitch > L / 10, "pitch steps change the sound");
        // chance 0 while latched does nothing; holding forces it
        b.cfg[0].slot[0] = { FxType::RandomJumps, true, 50, 0, 0 };
        b.pass = 0;
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l1.data() + p, r1.data() + p);
        int diffZero = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - lp[i]) > 0.001f) diffZero++;
        CHECK(diffZero == 0, "a chance of 0 percent never triggers");
        b.cfg[0].held[0] = true;
        b.pass = 0;
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l1.data() + p, r1.data() + p);
        int diffHeld = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - lp[i]) > 0.001f) diffHeld++;
        CHECK(diffHeld > L / 10, "holding triggers it whatever the chance");
        b.cfg[0].held[0] = false;
        b.cfg[0].slot[0] = { FxType::Reverse, true, 50, 0, 50 };
        b.pass = 0;
        for (int p = 0; p < L; p += 1000) b.render(p, 1000 < L - p ? 1000 : L - p, l1.data() + p, r1.data() + p);
        int some = 0;
        for (int i = 0; i < L; i++) if (std::fabs(l1[i] - lp[i]) > 0.001f) some++;
        CHECK(some > L / 20 && some < L * 9 / 10, "a chance of 50 percent triggers on some slices only");
    }
    {   // the mixer: level, pan, tone and solo
        TrackBank b; for (auto& c : b.cfg) c.levelDb10 = 0; // these tests measure the effects at unity level
        const int L = 4410;
        std::vector<int16_t> d(L);
        for (int i = 0; i < L; i++) d[i] = (int16_t)(std::sin(i * 0.2f) * 10000);
        b.data[0] = d.data(); b.data[1] = d.data(); b.loop = L;
        auto rms = [&](bool left) { std::vector<float> l(L), r(L); b.pass = 0; b.lastPos = 0; b.render(0, L, l.data(), r.data()); double s = 0; for (int i = 0; i < L; i++) s += (left ? l[i] : r[i]) * (left ? l[i] : r[i]); return std::sqrt(s / L); };
        b.cfg[1].mute = true; // only track 0 plays, so the maths is about one track
        const double base = rms(true);
        b.cfg[0].levelDb10 = -60;
        CHECK(std::fabs(rms(true) / base - 0.5012) < 0.02, "-6 dB is about half the level");
        b.cfg[0].levelDb10 = 0; b.cfg[0].pan = 100;
        CHECK(rms(true) < 0.01 * base && rms(false) > 0.9 * base, "pan right silences the left");
        b.cfg[0].pan = 0; b.cfg[0].tone = -100;
        CHECK(rms(true) < 0.5 * base, "a dark tone takes the top off");
        b.cfg[0].tone = 100;
        CHECK(rms(true) < base, "a thin tone takes the bottom off");
        b.cfg[0].tone = 0; b.cfg[1].mute = false; b.cfg[1].solo = true;
        std::vector<float> l1(L), r1(L);
        b.render(0, L, l1.data(), r1.data());
        double e0 = 0; for (int i = 0; i < L; i++) e0 += l1[i] * l1[i];
        b.cfg[1].solo = false; b.cfg[0].solo = true; b.cfg[1].mute = true;
        std::vector<float> l2(L), r2(L);
        b.render(0, L, l2.data(), r2.data());
        double e1 = 0; for (int i = 0; i < L; i++) e1 += l2[i] * l2[i];
        CHECK(e0 > 0 && e1 > 0, "a soloed track plays");
        CHECK(toneCutoffHz(0) == 0 && toneCutoffHz(-100) < 400 && toneCutoffHz(100) > 1000, "the tone control spans about 200 Hz to 1.3 kHz");
    }
    {   // zero crossings and the end of the sound
        std::vector<int16_t> x(44100);
        for (int i = 0; i < 44100; i++) x[i] = (int16_t)(std::sin(i * 0.05f) * 10000);
        const uint32_t z = snapToZero(x.data(), x.size(), 1000, 100);
        CHECK(z > 0 && ((x[z - 1] <= 0 && x[z] > 0) || (x[z - 1] >= 0 && x[z] < 0)), "snapToZero lands on a crossing");
        for (int i = 30000; i < 44100; i++) x[i] = 0;
        const int e = findLastSound(x.data(), 0, x.size(), 44100);
        CHECK(e > 29000 && e <= 30500, "the end of the sound is found");
        std::vector<int16_t> silent(44100, 0);
        CHECK(findLastSound(silent.data(), 0, silent.size(), 44100) < 0, "silence has no end of sound");
    }
    {   // wav header
        uint8_t h[44];
        CHECK(wavHeader(h, 1000, 44100) == 44, "a WAV header is 44 bytes");
        CHECK(h[0] == 'R' && h[8] == 'W' && h[40] == 0xE8 && h[41] == 0x03, "the header names RIFF, WAVE and the data size");
        CHECK(h[24] == 0x44 && h[25] == 0xAC, "the sample rate is 44100");
        WavInfo w;
        CHECK(parseWav(h, 44, w) && w.channels == 1 && w.bits == 16 && w.rate == 44100 && w.dataOffset == 44 && w.dataBytes == 1000 && w.frames() == 500, "a WAV header written here reads back");
        h[20] = 3; // float data
        CHECK(!parseWav(h, 44, w), "float WAV files are refused");
        const uint8_t junk[16] = { 1, 2, 3 };
        CHECK(!parseWav(junk, 16, w), "something that is not a WAV is refused");
        // a WAV with an extra chunk before the data
        uint8_t g[60] = {};
        wavHeader(g, 400, 48000);
        for (int i = 0; i < 4; i++) g[36 + i] = "LIST"[i];
        g[40] = 8; g[41] = g[42] = g[43] = 0; // 8 bytes of list data
        for (int i = 0; i < 4; i++) g[52 + i] = "data"[i];
        g[56] = 0x90; g[57] = 0x01;
        CHECK(parseWav(g, 60, w) && w.dataOffset == 60 - 0 && w.dataBytes == 400 && w.rate == 48000, "a chunk before the data is skipped");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
