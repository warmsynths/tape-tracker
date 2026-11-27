// The drum voices: every voice sounds, stays bounded, dies away, and the kits differ
#include "Drums.h"

#include <cmath>
#include <cstdio>
#include <memory>
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

// Plays one voice and returns the output in blocks of 128
static std::vector<float> play(DrumKit& d, int voice, int velocity, int blocks)
{
    std::vector<float> out;
    for (int b = 0; b < blocks; b++)
    {
        if (b == 0) d.trigger(voice, velocity);
        float l[128] = {}, r[128] = {};
        d.render(l, r, 128);
        for (int i = 0; i < 128; i++) out.push_back(l[i]);
    }
    return out;
}
static float energy(const std::vector<float>& v, size_t a, size_t b)
{
    float e = 0;
    for (size_t i = a; i < b && i < v.size(); i++) e += v[i] * v[i];
    return e;
}

int main()
{
    std::unique_ptr<DrumKit> d(new DrumKit);
    d->init(44100.0f);
    for (int v = 0; v < DrumKit::kVoices; v++)
    {
        d->reset();
        const std::vector<float> o = play(*d, v, 110, 400); // about 1.2 s
        float peak = 0;
        bool bad = false;
        for (float x : o) { if (std::fabs(x) > peak) peak = std::fabs(x); if (x != x || std::fabs(x) > 4.0f) bad = true; }
        char msg[96];
        std::snprintf(msg, sizeof msg, "voice %d (%s) sounds", v, DrumKit::voiceName(v));
        CHECK(peak > 0.02f, msg);
        std::snprintf(msg, sizeof msg, "voice %d (%s) stays bounded", v, DrumKit::voiceName(v));
        CHECK(!bad && peak <= 1.0f, msg);
        std::snprintf(msg, sizeof msg, "voice %d (%s) dies away", v, DrumKit::voiceName(v));
        CHECK(energy(o, 40000, 51200) < 0.05f * energy(o, 0, 11000) + 0.001f, msg);
    }
    // velocity
    d->reset();
    const std::vector<float> soft = play(*d, 15, 30, 40);
    d->reset();
    const std::vector<float> loud = play(*d, 15, 127, 40);
    CHECK(energy(loud, 0, 5120) > energy(soft, 0, 5120), "a harder hit is louder");
    // kits differ
    d->setKit(0);
    d->reset();
    const std::vector<float> k0 = play(*d, 15, 100, 40);
    d->setKit(16);
    d->reset();
    const std::vector<float> k16 = play(*d, 15, 100, 40);
    CHECK(std::fabs(energy(k0, 0, 5120) - energy(k16, 0, 5120)) > 0.001f, "different kits sound different");
    CHECK(DrumKit::kitName(0)[0] == '8' && DrumKit::kitStyle(0)[0] == 'H' && DrumKit::kitStyle(8)[0] == 'D' && DrumKit::kitStyle(16)[0] == 'L', "the kits are named and grouped");
    // nothing active: nothing rendered
    d->reset();
    float l[128] = {}, r[128] = {};
    d->render(l, r, 128);
    CHECK(!d->active() && energy(std::vector<float>(l, l + 128), 0, 128) == 0.0f, "an idle kit adds nothing");
    // closed hat chokes the open hat
    d->setKit(0);
    d->reset();
    d->trigger(3, 110);
    std::vector<float> open;
    for (int b = 0; b < 30; b++) { float a[128] = {}, c[128] = {}; d->render(a, c, 128); for (int i = 0; i < 128; i++) open.push_back(a[i]); }
    d->trigger(7, 110);
    std::vector<float> after;
    for (int b = 0; b < 30; b++) { float a[128] = {}, c[128] = {}; d->render(a, c, 128); for (int i = 0; i < 128; i++) after.push_back(a[i]); }
    CHECK(energy(open, 0, 3840) > 0.0f, "the open hat rings");
    if (failures == 0) std::printf("all passed\n");
    return failures ? 1 : 0;
}
