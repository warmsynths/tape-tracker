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

#endif /* LOOP_FILL_H_ */
