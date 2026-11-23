#include "../../core/source/FourTrackLoop.h"

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

using State = FourTrackLoop::State;

int main()
{
    const uint32_t kMax = 1000;

    {
        FourTrackLoop f(kMax);
        CHECK(f.state() == State::Empty && f.lengthFrames() == 0, "starts empty");
        f.pressPlay();
        CHECK(f.state() == State::Empty, "play with nothing recorded does nothing");
        f.pressStop();
        CHECK(f.state() == State::Empty, "stop with nothing recorded does nothing");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        CHECK(f.state() == State::Recording, "rec starts recording");
        f.recordedFrames(400, false);
        f.pressStop();
        CHECK(f.state() == State::Ready && f.lengthFrames() == 400, "stop keeps what was recorded");
        CHECK(!f.memoryFull(), "not flagged full");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.pressStop();
        CHECK(f.state() == State::Empty && f.lengthFrames() == 0, "stop with zero frames stays empty");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.recordedFrames(kMax + 500, false);
        CHECK(f.state() == State::Ready && f.lengthFrames() == kMax, "recording stops at the cap and clamps");
        CHECK(f.memoryFull(), "cap reached flags memory full");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.recordedFrames(700, true);
        CHECK(f.state() == State::Ready && f.lengthFrames() == 700, "memory full below the cap keeps the take");
        CHECK(f.memoryFull(), "memory full flagged");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.recordedFrames(300, false);
        f.pressStop();
        f.pressPlay();
        CHECK(f.state() == State::Playing, "play from ready plays");
        f.pressStop();
        CHECK(f.state() == State::Ready && f.lengthFrames() == 300, "stop returns to ready and keeps the take");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.recordedFrames(300, false);
        f.pressStop();
        f.pressPlay();
        f.pressRecord();
        CHECK(f.state() == State::Recording && f.lengthFrames() == 0, "rec while playing discards the old take");
        f.pressRecord();
        CHECK(f.state() == State::Recording, "rec while recording is ignored");
        f.pressPlay();
        CHECK(f.state() == State::Recording, "play while recording is ignored");
    }
    {
        FourTrackLoop f(kMax);
        f.recordedFrames(50, false);
        CHECK(f.state() == State::Empty && f.lengthFrames() == 0, "frame counts outside recording are ignored");
    }
    {
        FourTrackLoop f(kMax);
        f.pressRecord();
        f.recordedFrames(kMax + 1, false);
        f.pressRecord();
        CHECK(!f.memoryFull() && f.state() == State::Recording, "a new recording clears the memory full flag");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
