#include "../../core/source/loop_bars.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg)                                       \
    do                                                         \
    {                                                          \
        if (!(cond))                                           \
        {                                                      \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                        \
        }                                                      \
    } while (0)

int main()
{
    const uint32_t rate = 44118;

    // 4 bars of 4/4 in 8.35 s is 115.0 BPM (the design's example).
    {
        const uint32_t frames = (uint32_t)(8.35 * rate);
        const uint32_t t = loopTempoTenths(frames, rate, 4);
        CHECK(t >= 1148 && t <= 1152, "4 bars in 8.35 s is about 115.0 BPM");
        CHECK(loopTempoTenths(frames, rate, 2) * 2 + 2 >= loopTempoTenths(frames, rate, 4) - 2, "twice the bars is twice the tempo");
    }
    CHECK(loopTempoTenths(0, rate, 4) == 0 && loopTempoTenths(1000, rate, 0) == 0, "no frames or no bars gives 0");

    // Short take: only choices between 40 and 240 BPM are offered.
    {
        int bars[kBarChoiceMax];
        const uint32_t frames = (uint32_t)(8.35 * rate);
        const int n = loopBarChoices(frames, rate, bars);
        CHECK(n >= 3, "several choices for a short take");
        for (int i = 0; i < n; i++)
        {
            const uint32_t t = loopTempoTenths(frames, rate, bars[i]);
            CHECK(t >= 400 && t <= 2400, "every offered choice is between 40 and 240 BPM");
        }
        const int s = loopBarSuggestion(frames, rate, bars, n);
        CHECK(bars[s] == 4, "4 bars (115 BPM) is the suggestion for 8.35 s");
    }
    // Long take (24 s): long bar counts appear, absurd ones are hidden.
    {
        int bars[kBarChoiceMax];
        const uint32_t frames = 24 * rate;
        const int n = loopBarChoices(frames, rate, bars);
        bool hasOne = false, has12 = false, has32 = false;
        for (int i = 0; i < n; i++)
        {
            hasOne |= (bars[i] == 1);
            has12 |= (bars[i] == 12);
            has32 |= (bars[i] == 32);
        }
        CHECK(!hasOne, "1 bar in 24 s is 10 BPM and is hidden");
        CHECK(has12, "12 bars in 24 s (120 BPM) is offered");
        CHECK(!has32, "32 bars in 24 s is 320 BPM and is hidden");
        const int s = loopBarSuggestion(frames, rate, bars, n);
        CHECK(bars[s] == 12, "12 bars is the suggestion for 24 s");
    }
    // A take so short or long that nothing fits still gives one choice.
    {
        int bars[kBarChoiceMax];
        CHECK(loopBarChoices(rate / 100, rate, bars) == 1, "a 10 ms take still offers one choice");
        CHECK(loopBarChoices(rate * 600, rate, bars) == 1, "a very long take still offers one choice");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
