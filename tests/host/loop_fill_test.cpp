#include "../../core/libs/Audio/loop_fill.h"

#include <cstdio>
#include <vector>

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
    // Wraps across the end: three blocks of 4 over a loop of 6 repeat the data with no gap.
    {
        const int16_t data[6] = { 1, 2, 3, 4, 5, 6 };
        uint32_t pos = 0;
        std::vector<int16_t> out;
        for (int b = 0; b < 3; b++)
        {
            int16_t blk[4];
            loopFillBlock(data, 6, pos, blk, 4);
            out.insert(out.end(), blk, blk + 4);
        }
        const int16_t expect[12] = { 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6 };
        bool same = out.size() == 12;
        for (int i = 0; same && i < 12; i++) same = out[i] == expect[i];
        CHECK(same, "loop repeats data with no gap across the boundary");
        CHECK(pos == 0, "position is back to 0 after 12 samples of a 6 loop");
    }
    // Position advances modulo length.
    {
        const int16_t data[5] = { 10, 20, 30, 40, 50 };
        uint32_t pos = 3;
        int16_t blk[4];
        loopFillBlock(data, 5, pos, blk, 4);
        CHECK(blk[0] == 40 && blk[1] == 50 && blk[2] == 10 && blk[3] == 20, "starts from position and wraps");
        CHECK(pos == 2, "position advanced modulo length");
    }
    // Length 0 gives silence and leaves position at 0.
    {
        uint32_t pos = 7;
        int16_t blk[4] = { 9, 9, 9, 9 };
        loopFillBlock(nullptr, 0, pos, blk, 4);
        CHECK(blk[0] == 0 && blk[1] == 0 && blk[2] == 0 && blk[3] == 0, "zero length gives zeros");
        CHECK(pos == 0, "zero length resets position");
    }
    // Loop shorter than the block repeats several times inside one block.
    {
        const int16_t data[2] = { 7, 8 };
        uint32_t pos = 0;
        int16_t blk[5];
        loopFillBlock(data, 2, pos, blk, 5);
        CHECK(blk[0] == 7 && blk[1] == 8 && blk[2] == 7 && blk[3] == 8 && blk[4] == 7, "short loop repeats inside one block");
        CHECK(pos == 1, "position after odd count");
    }
    // A position beyond the length (for example after a shorter new recording) is brought back in range.
    {
        const int16_t data[3] = { 1, 2, 3 };
        uint32_t pos = 100;
        int16_t blk[3];
        loopFillBlock(data, 3, pos, blk, 3);
        CHECK(pos < 3, "out of range position is wrapped");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
