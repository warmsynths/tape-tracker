#ifndef LOOP_BARS_H_
#define LOOP_BARS_H_

#include <stdint.h>

// The "How many bars?" question after a first take. Pure functions, tested on a PC
// (tests/host/loop_bars_test.cpp). Time is 4/4. No audio is stretched: only the tempo is worked
// out from the length that was played.

// Tempo in tenths of a BPM if `frames` at `sampleRate` is `bars` bars of 4/4.
inline uint32_t loopTempoTenths(uint32_t frames, uint32_t sampleRate, int bars)
{
    if (frames == 0 || bars <= 0)
    {
        return 0;
    }
    // bars * 4 beats * 60 s * 10 tenths * sampleRate / frames, rounded
    const uint64_t num = (uint64_t)bars * 4u * 60u * 10u * sampleRate;
    return (uint32_t)((num + frames / 2) / frames);
}

constexpr int kBarChoiceMax = 12;

// Fills `bars` with the bar counts whose tempo lies between 40 and 240 BPM, and returns how many.
// If none does, the single choice nearest to that range is returned, so the list is never empty.
inline int loopBarChoices(uint32_t frames, uint32_t sampleRate, int* bars)
{
    static const int kAll[kBarChoiceMax] = { 1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32 };
    int count = 0;
    for (int i = 0; i < kBarChoiceMax; i++)
    {
        const uint32_t t = loopTempoTenths(frames, sampleRate, kAll[i]);
        if (t >= 400 && t <= 2400)
        {
            bars[count++] = kAll[i];
        }
    }
    if (count == 0)
    {
        int best = kAll[0];
        uint32_t bestDistance = 0xFFFFFFFFu;
        for (int i = 0; i < kBarChoiceMax; i++)
        {
            const uint32_t t = loopTempoTenths(frames, sampleRate, kAll[i]);
            const uint32_t d = (t < 400) ? (400 - t) : (t - 2400);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = kAll[i];
            }
        }
        bars[count++] = best;
    }
    return count;
}

// Index in `bars` of the choice whose tempo is closest to 120 BPM.
inline int loopBarSuggestion(uint32_t frames, uint32_t sampleRate, const int* bars, int count)
{
    int best = 0;
    uint32_t bestDistance = 0xFFFFFFFFu;
    for (int i = 0; i < count; i++)
    {
        const uint32_t t = loopTempoTenths(frames, sampleRate, bars[i]);
        const uint32_t d = (t > 1200) ? (t - 1200) : (1200 - t);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

#endif /* LOOP_BARS_H_ */
