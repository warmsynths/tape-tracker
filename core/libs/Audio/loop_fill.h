#ifndef LOOP_FILL_H_
#define LOOP_FILL_H_

#include <stdint.h>

// Writes `count` samples to `out`, reading `data` cyclically from `position`.
// `position` is advanced modulo `length`. With `length == 0` the output is silence.
// Pure function with no hardware dependencies, so it can be tested on a PC.
inline void loopFillBlock(const int16_t* data, uint32_t length, uint32_t& position, int16_t* out, int count)
{
    if (length == 0 || data == nullptr)
    {
        for (int i = 0; i < count; i++)
        {
            out[i] = 0;
        }
        position = 0;
        return;
    }

    if (position >= length)
    {
        position %= length;
    }

    for (int i = 0; i < count; i++)
    {
        out[i] = data[position];
        if (++position >= length)
        {
            position = 0;
        }
    }
}


// Mixes several looping mono regions into `out`, all read at the same position so they stay in
// sync. `tracks[i]` may be nullptr (silent). Each region is `length` samples long. The sum is
// clipped to 16 bits. `position` is where the block starts; the caller advances it.
inline void loopMixBlock(const int16_t* const* tracks,
                         int trackCount,
                         uint32_t length,
                         uint32_t position,
                         int16_t* out,
                         int count)
{
    if (length == 0)
    {
        for (int i = 0; i < count; i++)
        {
            out[i] = 0;
        }
        return;
    }

    uint32_t pos = position % length;
    for (int i = 0; i < count; i++)
    {
        int32_t sum = 0;
        for (int t = 0; t < trackCount; t++)
        {
            if (tracks[t] != nullptr)
            {
                sum += tracks[t][pos];
            }
        }
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        out[i] = (int16_t)sum;
        if (++pos >= length)
        {
            pos = 0;
        }
    }
}

#endif /* LOOP_FILL_H_ */
