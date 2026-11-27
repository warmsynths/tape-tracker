// The Sound screen's pictures: the dial ring, the patch shapes and the one-note picture
#include "SoundArt.h"

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

static int count(const uint8_t* b, int n)
{
    int c = 0;
    for (int i = 0; i < n; i++) for (int k = 0; k < 8; k++) if (b[i] & (1 << k)) c++;
    return c;
}
static bool px(const uint8_t* b, int stride, int x, int y) { return (b[y * stride + (x >> 3)] & (0x80 >> (x & 7))) != 0; }

int main()
{
    uint8_t ring[kDialBytes];
    artDialRing(ring);
    CHECK(px(ring, 8, 32, 6), "the ring has a dot at the top (zero)");
    CHECK(!px(ring, 8, 32, 58), "and none at the bottom, where the gap is");
    CHECK(px(ring, 8, 6, 32) && px(ring, 8, 58, 32), "dots at the left and the right");
    CHECK(!px(ring, 8, 32, 32), "the middle is empty");
    CHECK(artDialAngle(0) == -90 && artDialAngle(10) == 45 && artDialAngle(-10) == -225, "knob values map to the ends of the ring");

    for (int i = 0; i < kSynthPresets; i++)
    {
        uint8_t c[kCycleBytes];
        artPatchCycle(synthPreset(i), c);
        CHECK(count(c, kCycleBytes) > 40, "every patch has a visible one-cycle shape");
    }
    // a square is flat on top and bottom; a saw is not
    SynthPatch sq = synthPreset(0);
    sq.osc = SynthOsc::Shape; sq.shape = 1.0f; sq.pulse = 0.5f;
    uint8_t c[kCycleBytes];
    artPatchCycle(sq, c);
    CHECK(px(c, 8, 10, 1) && px(c, 8, 50, 15), "a square has its high part first and its low part after");

    uint8_t wave[kNoteBytes], sil[kNoteBytes];
    SynthPatch slow = synthPreset(0);
    slow.attack = 0.8f; slow.release = 1.5f;
    SynthPatch fast = slow;
    fast.attack = 0.005f; fast.release = 0.05f;
    const int r1 = artNote(slow, wave, sil);
    const int slowCount = count(sil, kNoteBytes);
    const int r2 = artNote(fast, wave, sil);
    const int fastCount = count(sil, kNoteBytes);
    CHECK(r1 > 100 && r1 < kNoteW && r2 == r1, "the release point sits at the same place");
    CHECK(slowCount != fastCount, "attack and release change the picture");
    CHECK(count(wave, kNoteBytes) > 100, "the wave is drawn under the envelope");

    if (failures == 0) std::printf("all passed\n");
    return failures ? 1 : 0;
}
