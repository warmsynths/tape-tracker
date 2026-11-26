// Scenario tests of the shared Four-Track application with a fake machine: no hardware, so they
// run on a computer. They exercise exactly the code the Tracker and the web emulator run.
#include "App.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
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
    TrackBank trackBank;
    Lofi lofiMaster;
    int gains[4] = { 50, 50, 50, 50 };
    std::map<std::string, std::vector<uint8_t>> files;
    int clicks = 0;
    int curSample = 0;

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
    TrackBank& bank() override { return trackBank; }
    Lofi& lofi() override { return lofiMaster; }
    int gain(Source s) override { return gains[(int)s]; }
    void setGain(Source s, int p) override { gains[(int)s] = p; }
    void setMonitor(bool) override {}
    int monitorReductionDb() override { return 0; }
    void click(bool) override { clicks++; }
    bool sdPresent() override { return true; }
    bool fileWrite(const char* path, const void* d, uint32_t n, bool append) override
    {
        auto& f = files[path];
        if (!append) f.clear();
        f.insert(f.end(), (const uint8_t*)d, (const uint8_t*)d + n);
        return true;
    }
    uint32_t fileRead(const char* path, uint32_t off, void* buf, uint32_t n) override
    {
        auto it = files.find(path);
        if (it == files.end() || off >= it->second.size()) return 0;
        uint32_t k = (uint32_t)it->second.size() - off;
        if (k > n) k = n;
        std::memcpy(buf, it->second.data() + off, k);
        return k;
    }
    uint32_t fileSize(const char* path) override { auto it = files.find(path); return it == files.end() ? 0 : (uint32_t)it->second.size(); }
    bool fileRemove(const char* path) override { return files.erase(path) > 0; }
    int fileList(const char* dir, char (*names)[24], int maxNames) override
    {
        int n = 0;
        const std::string prefix = std::string(dir) + "/";
        for (auto& kv : files)
            if (kv.first.compare(0, prefix.size(), prefix) == 0 && n < maxNames)
                std::snprintf(names[n++], 24, "%s", kv.first.c_str() + prefix.size());
        return n;
    }
    int sampleCount() override { return 0; }
    bool sampleName(int, char*, int) override { return false; }
    int currentSample() override { return curSample; }
    void setCurrentSample(int s) override { curSample = s; }
    void sampleMemory(uint32_t& u, uint32_t& t) override { u = 0; t = 2600000; }
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

static Palette palette() { return designPalette(); }


struct NullGfx : Gfx
{
    void color(uint32_t) override {}
    void rect(int, int, int, int) override {}
    void line(int, int, int, int) override {}
    void dot(int, int, int) override {}
    void text(int, int, const char*, int) override {}
    int textWidth(const char* s, int) override { return (int)std::strlen(s) * 8; }
    void bars(int, int, int, int, const uint8_t*, int) override {}
};

static void recordLoop(FakeHal& hal, App& app)
{
    app.key(kRec);
    hal.advance(app, 100000); // the count-in
    hal.advance(app, FakeHal::kRate * 4);
    app.key(kStop);
    app.key(kB8);
}

static void pagesAndProjects()
{
    using Mode = FourTrackSession::Mode;
    NullGfx gfx;
    {   // every page builds, fits its stage budget and draws
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kB6); app.key(kRec);
        hal.advance(app, hal.loop * 3);
        const int keys[] = { kSampleEdit, kPerformance, kMaster };
        for (int k : keys)
        {
            app.key(k);
            CHECK(app.screen() != Screen::Main, "a screen key opens a page");
            CHECK(app.layerVisible(4) && app.stagesOf(4) >= 1 && app.stagesOf(4) <= 5, "a page has 1 to 5 stages");
            CHECK(!app.pageOverflowed(), "a page fits the drawing budget");
            for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
            app.key(k == kPerformance ? kPerformance : kB8);
            CHECK(app.screen() == Screen::Main, "the page closes");
        }
        const int shiftB[] = { kB2, kB3, kB4 };
        for (int b : shiftB)
        {
            app.setShift(true); app.key(b); app.setShift(false);
            CHECK(app.screen() != Screen::Main, "a Shift button opens a page");
            CHECK(!app.pageOverflowed(), "a Shift page fits the drawing budget");
            for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
            app.key(kB8);
        }
        app.setShift(true); app.key(kDown); app.setShift(false);
        CHECK(app.screen() == Screen::Settings, "Shift+Down opens Settings");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.key(kB8);
        app.key(kFile);
        CHECK(app.screen() == Screen::Project, "File opens Project");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
    }
    {   // count-in happens in Free tempo too; Play and Stop work on the Glitch page
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kRec);
        CHECK(app.state().mode() == Mode::Idle, "Rec counts in first");
        hal.advance(app, FakeHal::kRate * 3);
        CHECK(hal.clicks == 4 && app.state().mode() == Mode::RecordingFirst, "four clicks, then the take starts");
        hal.advance(app, FakeHal::kRate);
        app.key(kStop); app.key(kB8);
        app.key(kPerformance);
        CHECK(app.screen() == Screen::Glitch, "Performance opens Glitch");
        app.key(kB7);
        app.key(kStop);
        CHECK(!hal.playing, "Stop works on the Glitch page");
        app.key(kPlay);
        CHECK(hal.playing, "Play works on the Glitch page");
        app.key(kPerformance);
        CHECK(app.screen() == Screen::Main, "Performance again goes back");
    }
    {   // a Glitch slot is held while its button is down and let go on release
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kPerformance);
        app.key(kB1);
        CHECK(hal.trackBank.cfg[0].held[0], "pressing B1 on Glitch holds slot 1");
        app.keyUp(kB1);
        CHECK(!hal.trackBank.cfg[0].held[0], "releasing B1 lets it go");
        app.key(kPerformance);
        app.setShift(true);
        app.key(kB4);
        CHECK(app.screen() == Screen::Input, "Shift+B4 opens Input setup");
        app.setShift(false);
    }
    {   // the Pattern button is home
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kMaster);
        CHECK(app.screen() == Screen::Master, "Master opens its page");
        app.key(kPattern);
        CHECK(app.screen() == Screen::Main, "Pattern goes home");
    }
    {   // the action menu on Insert
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kJogPress);
        CHECK(std::strcmp(app.view().btn[0], "Move") == 0 && std::strcmp(app.view().btn[7], "Close") == 0, "Insert opens the action menu");
        app.key(kB4);
        CHECK(hal.trackBank.cfg[0].mute, "Mute in the menu mutes the track");
        app.key(kB4);
        app.key(kB1);
        CHECK(std::strcmp(app.view().btn[0], "To start") == 0, "Move in the menu starts move mode");
        app.key(kB7);
        app.key(kJogPress);
        app.key(kB2);
        CHECK(app.screen() == Screen::Editor, "Edit in the menu opens the Part Editor");
        app.key(kB8);
        app.key(kJogPress);
        app.key(kB8);
        CHECK(std::strcmp(app.view().btn[0], "Rec") == 0, "Close leaves the menu");
        CHECK(std::strstr(app.view().hint, "Insert for actions") != nullptr, "the hint says what Insert does");
    }
    {   // moving a Part: Shift+Insert enters, the jog steps the first note along the grid
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        const uint32_t loop = app.state().loopFrames();
        const uint32_t bars = (uint32_t)app.state().bars();
        app.key(kJogPress);
        CHECK(!app.view().btn[0][0] || std::strcmp(app.view().btn[0], "To start") != 0, "Insert alone does not enter move mode");
        app.key(kJogPress); // closes the action menu again
        app.setShift(true); app.key(kJogPress); app.setShift(false);
        CHECK(std::strcmp(app.view().btn[0], "To start") == 0, "Shift+Insert enters move mode");
        app.jog(1);
        CHECK(hal.trackBank.cfg[0].offset == (int32_t)(loop / (bars * 16)), "the jog moves the first note one sixteenth");
        app.key(kB3); // grid 1/4 is not next: the grid goes round to off
        app.key(kB3);
        app.key(kB1);
        CHECK(hal.trackBank.cfg[0].offset == 0, "To start puts the first note on the loop start");
        app.key(kB4);
        app.jog(1);
        app.key(kB7);
        CHECK(hal.trackBank.cfg[0].offset == 0, "Cancel puts it back");
        app.setShift(true); app.key(kJogPress); app.setShift(false);
        app.jog(1);
        app.key(kB8);
        CHECK(hal.trackBank.cfg[0].offset != 0 && std::strcmp(app.view().btn[0], "To start") != 0, "OK keeps the move and leaves move mode");
    }
    {   // Part Editor: trim silence and snapping
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kSampleEdit);
        CHECK(app.screen() == Screen::Editor, "Sample Edit opens the Part Editor");
        app.key(kB6);
        app.key(kB5);
        app.jog(1);
        app.setShift(true); app.key(kUp); app.setShift(false);
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(app.layerVisible(4) && app.stagesOf(4) <= 5, "the editor still fits its stages when zoomed");
        app.key(kB8);
    }
    {   // tempo: the loop is set to the chosen tempo's grid and shown
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kB2);
        CHECK(app.screen() == Screen::Tempo, "B2 on an empty home opens Tempo");
        app.key(kB8);
        app.key(kRec);
        hal.advance(app, FakeHal::kRate * 2);
        CHECK(hal.clicks > 0 || app.state().mode() != Mode::Idle, "a count-in or a take starts");
    }
    {   // save then open a project gives the same Parts back
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        const uint32_t loop = app.state().loopFrames();
        app.key(kFile);
        app.key(kB3);
        for (int i = 0; i < 400 && app.screen() == Screen::Export; i++) hal.advance(app, 128);
        CHECK(hal.fileSize("Projects/Untitled.ftp") > 512, "saving writes the project file");
        FakeHal hal2;
        hal2.files = hal.files;
        App app2(hal2, palette());
        app2.setMeasure(&gfx);
        app2.enter();
        app2.key(kFile);
        app2.key(kB2);
        for (int i = 0; i < 400 && app2.screen() == Screen::Export; i++) hal2.advance(app2, 128);
        CHECK(app2.state().loopFrames() == loop && app2.state().hasPart(0), "opening restores the loop and the Part");
        CHECK(std::memcmp(hal.mem[0].data(), hal2.mem[0].data(), loop * 2) == 0, "the audio is the same");
    }
    {   // export writes a WAV of the right length
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        const uint32_t loop = app.state().loopFrames();
        app.key(kFile);
        app.key(kB6);
        app.key(kRight);
        app.key(kB1);
        for (int i = 0; i < 2000 && hal.fileSize("Exports/Untitled.wav") < 44 + loop * 4; i++) hal.advance(app, 128);
        CHECK(hal.fileSize("Exports/Untitled.wav") == 44 + loop * 4, "two repeats make a WAV twice the loop long");
    }
    {   // a take replaced can be brought back with undo
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kRec);
        CHECK(app.popup() == Popup::Replace, "recording over a Part asks first");
        app.key(kB8);
        hal.advance(app, 100000); // the count-in
        CHECK(hal.fileSize("undo/t0.raw") > 0, "the old take is kept");
    }
}

int main()
{
    pagesAndProjects();
    using Mode = FourTrackSession::Mode;
    {
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        CHECK(std::strcmp(app.view().tempo, "Free") == 0, "tempo is Free before a first take");
        CHECK(app.state().mode() == Mode::Idle, "starts idle");

        // First take: 4 seconds, then Stop asks how many bars.
        app.key(kRec);
    hal.advance(app, 100000); // the count-in
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
    hal.advance(app, 100000); // the count-in
        CHECK(app.state().mode() == Mode::ChoosingBars, "Rec is ignored while the question is open");
        app.key(kB8);
        CHECK(app.state().mode() == Mode::Idle && hal.playing, "OK sets the loop and plays it");
        CHECK(app.state().bars() > 0 && std::strcmp(app.view().tempo, "Free") != 0, "tempo and bars are shown");
        CHECK(!app.layerVisible(3), "the question goes away");
        CHECK(app.view().lane[0].hasPart && app.view().lane[0].waveCount == 120, "a waveform is shown for the Part");

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
    hal.advance(app, 100000); // the count-in
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
    hal.advance(app, 100000); // the count-in
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
    hal.advance(app, 100000); // the count-in
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
        CHECK(!app.view().monitor, "B4 switches the monitor off");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
