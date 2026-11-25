#include "../../core/source/FourTrackSession.h"

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

using Mode = FourTrackSession::Mode;

static FourTrackSession withLoop(uint32_t maxFrames, uint32_t loopFrames)
{
    FourTrackSession s(maxFrames);
    s.pressRecord();
    s.firstRecordedFrames(loopFrames, false);
    s.pressStop();
    s.confirmBars(4);
    return s;
}

int main()
{
    const uint32_t kMax = 1000;

    {
        FourTrackSession s(kMax);
        CHECK(s.mode() == Mode::Idle && s.loopFrames() == 0 && !s.playing(), "starts idle, empty, stopped");
        CHECK(s.selected() == 0, "track 1 selected");
        s.pressPlay();
        CHECK(!s.playing(), "play with no loop does nothing");
        s.pressStop();
        CHECK(s.mode() == Mode::Idle, "stop with nothing does nothing");
    }
    {   // first Part sets the loop and starts playing
        FourTrackSession s(kMax);
        s.pressRecord();
        CHECK(s.mode() == Mode::RecordingFirst && s.recordingTrack() == 0, "rec with no loop records the first Part");
        s.firstRecordedFrames(400, false);
        s.pressStop();
        CHECK(s.mode() == Mode::ChoosingBars && s.pendingFrames() == 400 && s.loopFrames() == 0, "stop asks how many bars before the loop is set");
        CHECK(!s.playing(), "nothing plays while the bars are asked");
        s.pressRecord(); s.pressPlay(); s.pressStop(); s.selectTrack(1);
        CHECK(s.mode() == Mode::ChoosingBars && s.selected() == 0, "other buttons are ignored while the bars are asked");
        s.confirmBars(4);
        CHECK(s.mode() == Mode::Idle && s.loopFrames() == 400 && s.hasPart(0) && s.bars() == 4, "choosing the bars keeps the first Part and sets the loop");
        CHECK(s.playing(), "loop plays right after the first Part");
        CHECK(!s.memoryFull(), "not full");
    }
    {   // nothing recorded keeps things empty
        FourTrackSession s(kMax);
        s.pressRecord();
        s.pressStop();
        CHECK(s.loopFrames() == 0 && !s.hasPart(0) && s.mode() == Mode::Idle, "stop with zero frames sets no loop");
    }
    {   // the first Part can be on any track
        FourTrackSession s(kMax);
        s.selectTrack(2);
        s.pressRecord();
        s.firstRecordedFrames(300, false);
        s.pressStop();
        s.confirmBars(2);
        CHECK(s.hasPart(2) && !s.hasPart(0) && s.loopFrames() == 300, "first Part on track 3");
    }
    {   // memory cap stops the first take by itself and keeps it
        FourTrackSession s(kMax);
        s.pressRecord();
        s.firstRecordedFrames(kMax + 50, false);
        CHECK(s.mode() == Mode::ChoosingBars && s.pendingFrames() == kMax && s.memoryFull(), "cap stops and clamps the first Part");
        FourTrackSession t(kMax);
        t.pressRecord();
        t.firstRecordedFrames(700, true);
        CHECK(t.mode() == Mode::ChoosingBars && t.pendingFrames() == 700 && t.memoryFull(), "memory full below the cap keeps the take");
        t.confirmBars(3);
        CHECK(t.mode() == Mode::Idle && t.loopFrames() == 700, "bars chosen after memory full");
    }
    {   // a later Part: arm, window, record, done
        FourTrackSession s = withLoop(kMax, 500);
        s.selectTrack(1);
        s.pressRecord();
        CHECK(s.mode() == Mode::Armed && s.recordingTrack() == 1, "rec on another track arms it");
        CHECK(s.playing(), "arming keeps the loop playing");
        s.armWindowReached();
        CHECK(s.mode() == Mode::RecordingPart, "window reached starts recording");
        s.partRecorded();
        CHECK(s.mode() == Mode::Idle && s.hasPart(1) && s.hasPart(0) && s.loopFrames() == 500, "Part added, loop unchanged");
        CHECK(s.playing(), "still playing");
    }
    {   // arming from stopped starts the loop
        FourTrackSession s = withLoop(kMax, 500);
        s.pressStop();
        CHECK(!s.playing(), "stopped");
        s.selectTrack(1);
        s.pressRecord();
        CHECK(s.mode() == Mode::Armed && s.playing(), "arm from stopped starts playing");
    }
    {   // stop cancels an armed or running part and keeps the old state
        FourTrackSession s = withLoop(kMax, 500);
        s.selectTrack(1);
        s.pressRecord();
        s.pressStop();
        CHECK(s.mode() == Mode::Idle && !s.hasPart(1) && s.playing(), "stop cancels the arm and keeps playing");
        s.pressRecord();
        s.armWindowReached();
        s.pressStop();
        CHECK(s.mode() == Mode::Idle && !s.hasPart(1) && s.playing(), "stop discards a take in progress");
        s.pressStop();
        CHECK(!s.playing(), "stop again stops the loop");
        s.pressPlay();
        CHECK(s.playing(), "play restarts it");
    }
    {   // re-recording a track that has a Part replaces it on the next take, keeps the old one if cancelled
        FourTrackSession s = withLoop(kMax, 500);
        s.selectTrack(1);
        s.pressRecord();
        s.armWindowReached();
        s.partRecorded();
        s.pressRecord(); // track 2 again
        s.pressStop();
        CHECK(s.hasPart(1), "a cancelled re-record keeps the old Part");
    }
    {   // the only Part can be re-recorded from scratch, which redefines the loop
        FourTrackSession s = withLoop(kMax, 500);
        s.pressRecord();
        CHECK(s.mode() == Mode::RecordingFirst, "re-recording the only Part is a new first take");
        s.pressStop(); // nothing recorded
        CHECK(s.loopFrames() == 500 && s.hasPart(0), "cancelled new first take keeps the old loop");
        s.pressRecord();
        s.firstRecordedFrames(250, false);
        s.pressStop();
        s.confirmBars(2);
        CHECK(s.loopFrames() == 250 && s.bars() == 2, "a new first take redefines the loop and the bars");
    }
    {   // with two Parts, re-recording the first is a normal later take
        FourTrackSession s = withLoop(kMax, 500);
        s.selectTrack(1);
        s.pressRecord();
        s.armWindowReached();
        s.partRecorded();
        s.selectTrack(0);
        s.pressRecord();
        CHECK(s.mode() == Mode::Armed, "with other Parts present, re-recording track 1 is a normal take");
    }
    {   // selecting and ignoring
        FourTrackSession s = withLoop(kMax, 500);
        s.selectTrack(3);
        CHECK(s.selected() == 3, "select track 4");
        s.selectTrack(9);
        CHECK(s.selected() == 3, "out of range select is ignored");
        s.pressRecord();
        s.selectTrack(1);
        CHECK(s.selected() == 3, "cannot change track while armed");
        s.armWindowReached();
        s.pressRecord();
        CHECK(s.mode() == Mode::RecordingPart, "rec while recording is ignored");
        s.pressPlay();
        CHECK(s.mode() == Mode::RecordingPart, "play while recording is ignored");
    }
    {   // cancelling the bars throws the take away
        FourTrackSession s(kMax);
        s.pressRecord();
        s.firstRecordedFrames(400, false);
        s.pressStop();
        s.cancelBars();
        CHECK(s.mode() == Mode::Idle && s.loopFrames() == 0 && !s.hasPart(0) && !s.playing() && s.bars() == 0, "cancel leaves no loop and no Part");
        FourTrackSession t = withLoop(kMax, 500);
        t.pressRecord(); // new first take over the only Part
        t.firstRecordedFrames(250, false);
        t.pressStop();
        t.cancelBars();
        CHECK(t.loopFrames() == 0 && !t.hasPart(0), "cancelling a re-recorded first take leaves nothing");
        s.confirmBars(4);
        CHECK(s.loopFrames() == 0, "confirm outside the question does nothing");
    }
    {   // events in the wrong mode are ignored
        FourTrackSession s(kMax);
        s.armWindowReached();
        s.partRecorded();
        s.firstRecordedFrames(100, false);
        CHECK(s.mode() == Mode::Idle && s.loopFrames() == 0, "stray events do nothing");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
