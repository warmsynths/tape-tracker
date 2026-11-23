#include "../../core/libs/Audio/loop_fill.h"

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
    // Two tracks are summed sample by sample.
    {
        const int16_t a[4] = { 1, 2, 3, 4 };
        const int16_t b[4] = { 10, 20, 30, 40 };
        const int16_t* tracks[2] = { a, b };
        int16_t out[4];
        loopMixBlock(tracks, 2, 4, 0, out, 4);
        CHECK(out[0] == 11 && out[1] == 22 && out[2] == 33 && out[3] == 44, "two tracks sum");
    }
    // The block wraps past the end of the loop with no gap, for every track.
    {
        const int16_t a[3] = { 1, 2, 3 };
        const int16_t b[3] = { 100, 200, 300 };
        const int16_t* tracks[2] = { a, b };
        int16_t out[5];
        loopMixBlock(tracks, 2, 3, 2, out, 5); // starts at the last sample
        CHECK(out[0] == 303 && out[1] == 101 && out[2] == 202 && out[3] == 303 && out[4] == 101, "wraps together");
    }
    // A null track is silent and the others still play.
    {
        const int16_t a[2] = { 5, 6 };
        const int16_t* tracks[3] = { nullptr, a, nullptr };
        int16_t out[2];
        loopMixBlock(tracks, 3, 2, 0, out, 2);
        CHECK(out[0] == 5 && out[1] == 6, "null tracks are skipped");
    }
    // The sum is clipped, not wrapped around.
    {
        const int16_t a[1] = { 30000 };
        const int16_t b[1] = { 30000 };
        const int16_t c[1] = { -30000 };
        const int16_t d[1] = { -30000 };
        const int16_t* hi[2] = { a, b };
        const int16_t* lo[2] = { c, d };
        int16_t out[1];
        loopMixBlock(hi, 2, 1, 0, out, 1);
        CHECK(out[0] == 32767, "clips high");
        loopMixBlock(lo, 2, 1, 0, out, 1);
        CHECK(out[0] == -32768, "clips low");
    }
    // Zero length or no tracks is silence.
    {
        int16_t out[3] = { 7, 7, 7 };
        const int16_t a[1] = { 9 };
        const int16_t* tracks[1] = { a };
        loopMixBlock(tracks, 1, 0, 0, out, 3);
        CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0, "zero length is silence");
    }
    // A position beyond the loop length is wrapped into range.
    {
        const int16_t a[3] = { 1, 2, 3 };
        const int16_t* tracks[1] = { a };
        int16_t out[2];
        loopMixBlock(tracks, 1, 3, 100, out, 2); // 100 % 3 == 1
        CHECK(out[0] == 2 && out[1] == 3, "out of range position wraps");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
