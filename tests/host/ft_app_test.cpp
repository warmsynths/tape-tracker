// Scenario tests of the shared Four-Track application with a fake machine: no hardware, so they
// run on a computer. They exercise exactly the code the Tracker and the web emulator run.
#include "App.h"

#include <cstdio>
#include <cstring>
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

struct FakeHal : Hal
{
    static constexpr uint32_t kRate = 44100;
    uint32_t nowMs = 0;
    std::vector<int16_t> mem[4];
    bool recActive = false, full = false, captured = false;
    int recTrack = -1;
    uint32_t recLen = 0, startPos = 0;
    bool playing = false;
    uint32_t loop = 0, pos = 0;
    const int16_t* data[4] = {};
    Source applied = Source::Line;
    int padsOn = 0;

    FakeHal()
    {
        for (auto& m : mem) m.assign(kRate * 26, 0);
    }
    uint32_t sampleRate() const override { return kRate; }
    uint32_t millis() override { return nowMs; }
    int16_t* region(int t) override { return mem[t].data(); }
    uint32_t regionFrames() const override { return kRate * 26; }
    void applySource(Source s) override { applied = s; }
    void inputLevels(float& l, float& r) override { l = 0.1f; r = 0.1f; }
    void recordStart(int t, Source) override { recActive = true; recTrack = t; recLen = 0; full = false; captured = false; }
    void recordStop() override { recActive = false; }
    uint32_t recordLength() override { return recLen; }
    bool recordActive() override { return recActive; }
    bool recordFull() override { return full; }
    bool recordStartCaptured() override { return captured; }
    uint32_t recordStartPosition() override { return startPos; }
    void playerSetTrack(int t, const int16_t* d) override { data[t] = d; }
    void playerStart(uint32_t f) override { playing = f > 0; loop = f; pos = 0; }
    void playerStop() override { playing = false; }
    bool playerPlaying() override { return playing; }
    uint32_t playerPosition() override { return pos; }
    uint16_t playerTakePeak(int) override { return 8000; }
    void padOn(int, bool) override { padsOn++; }
    void padOff(int) override { padsOn--; }
    void padsReleaseAll() override { padsOn = 0; }

    // Let `frames` of audio go by, in blocks of 128, running the application's main loop as it
    // would run on the device.
    void advance(App& app, uint32_t frames)
    {
        for (uint32_t done = 0; done < frames; done += 128)
        {
            if (recActive && recLen + 128 <= regionFrames())
            {
                if (!captured)
                {
                    captured = true;
                    startPos = pos;
                }
                for (int i = 0; i < 128; i++) mem[recTrack][recLen + i] = (int16_t)(((recLen + i) / 50 % 2) ? 9000 : -9000);
                recLen += 128;
                if (recLen + 128 > regionFrames()) { recActive = false; full = true; }
            }
            if (playing && loop > 0) pos = (pos + 128) % loop;
            nowMs += 3;
            app.tick();
        }
    }
};

static Palette palette()
{
    Palette p{};
    p.background = 0; p.text = 0x777777; p.bright = 0xFFFFFF; p.dim = 0x575757; p.panel = 0x222222;
    p.hairline = 0x3F3F3F; p.selected = 0x3F3F3F; p.meterBack = 0x222222; p.wave = 0x777777;
    p.record = 0xcd1818; p.marker = 0xD67022; p.good = 0x30cb3f;
    return p;
}

int main()
{
    using Mode = FourTrackSession::Mode;
    {
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        CHECK(std::strcmp(app.view().tempo, "Free") == 0, "tempo is Free before a first take");
        CHECK(app.state().mode() == Mode::Idle, "starts idle");

        // First take: 4 seconds, then Stop asks how many bars.
        app.key(kRec);
        CHECK(app.state().mode() == Mode::RecordingFirst, "Rec starts the first take");
        hal.advance(app, FakeHal::kRate * 4);
        app.key(kStop);
        CHECK(app.state().mode() == Mode::ChoosingBars, "Stop asks how many bars");
        CHECK(app.view().prompt.active && app.view().prompt.count >= 3, "the question lists choices");
        CHECK(!hal.playing, "nothing plays while the question is open");
        CHECK(app.layerVisible(3), "the question layer is visible");

        // Other buttons are ignored; the jog wheel and arrows choose; B8 is OK.
        const int before = app.view().prompt.selected;
        app.jog(1);
        CHECK(app.view().prompt.selected == before + 1 || before + 1 >= app.view().prompt.count, "jog chooses the next bars");
        app.key(kLeft);
        CHECK(app.view().prompt.selected == before, "left goes back");
        app.key(kRec);
        CHECK(app.state().mode() == Mode::ChoosingBars, "Rec is ignored while the question is open");
        app.key(kB8);
        CHECK(app.state().mode() == Mode::Idle && hal.playing, "OK sets the loop and plays it");
        CHECK(app.state().bars() > 0 && std::strcmp(app.view().tempo, "Free") != 0, "tempo and bars are shown");
        CHECK(!app.layerVisible(3), "the question goes away");
        CHECK(app.view().lane[0].hasPart && app.view().lane[0].waveCount == 100, "a waveform is shown for the Part");

        // A later Part on track 2: arm, wait for the loop start, record one loop.
        app.key(kB6);
        CHECK(app.state().selected() == 1, "B6 selects track 2");
        app.key(kRec);
        CHECK(app.state().mode() == Mode::Armed, "Rec arms track 2");
        hal.advance(app, hal.loop * 3);
        CHECK(app.state().mode() == Mode::Idle && app.state().hasPart(1), "the second Part is recorded");
        CHECK(app.state().loopFrames() == hal.loop, "the loop length is unchanged");

        // Same track again cycles its source.
        app.key(kB6);
        CHECK(hal.applied == Source::MicLow, "pressing the selected track again changes its source");
        CHECK(std::strcmp(app.view().lane[1].tag, "MIC LOW") == 0, "the tag shows the source");

        // Drawing produces something for every layer and stage.
        int redrawn = 0;
        for (int l = 0; l < kLayerCount; l++) redrawn += app.takeRedraw(l) ? 1 : 0;
        CHECK(redrawn > 0, "something needs drawing");
    }
    {   // cancelling the question throws the take away
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kRec);
        hal.advance(app, FakeHal::kRate * 3);
        app.key(kStop);
        app.key(kB7);
        CHECK(app.state().mode() == Mode::Idle && app.state().loopFrames() == 0 && !app.state().hasPart(0), "Cancel discards the take");
        CHECK(!hal.playing, "nothing plays after Cancel");
    }
    {   // a take that fills memory stops by itself and still asks for the bars
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kRec);
        hal.advance(app, FakeHal::kRate * 30);
        CHECK(app.state().mode() == Mode::ChoosingBars, "memory full ends the first take and asks for the bars");
        CHECK(app.view().prompt.memoryFull, "the question says memory is full");
        CHECK(app.state().pendingFrames() == FakeHal::kRate * 24, "the take is limited to 24 seconds");
    }
    {   // stop during a later Part keeps what was played and silences the rest
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kRec);
        hal.advance(app, FakeHal::kRate * 4);
        app.key(kStop);
        app.key(kB8);
        app.key(kB6);
        app.key(kRec);
        hal.advance(app, hal.loop - hal.pos + 5000);
        CHECK(app.state().mode() == Mode::RecordingPart, "recording a Part");
        app.key(kStop);
        CHECK(app.state().hasPart(1), "stopping keeps the part played so far");
    }
    {   // pads
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.pad(3, true);
        CHECK(hal.padsOn == 1, "a pad press reaches the machine");
        app.pad(3, false);
        CHECK(hal.padsOn == 0, "a pad release reaches the machine");
        app.key(kB4);
        CHECK(std::strcmp(app.view().padsMode, "PAD") == 0, "B4 switches the pad mode");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
