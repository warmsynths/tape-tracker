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
    int curSample = 0; // the sample the pads play
    uint32_t quiet = 0; // frames of silence at the start of a take

    FakeHal()
    {
        for (auto& m : mem) m.assign(kRate * 18, 0);
    }
    uint32_t sampleRate() const override { return kRate; }
    uint32_t millis() override { return nowMs; }
    int16_t* region(int t) override { return mem[t].data(); }
    uint32_t regionFrames() const override { return kRate * 18; }
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
    std::map<int, std::string> loaded; // slot -> file loaded onto it
    bool previewing = false;
    bool sampleName(int slot, char* out, int size) override
    {
        auto it = loaded.find(slot);
        if (it == loaded.end()) return false;
        std::snprintf(out, size, "%s", it->second.c_str());
        return true;
    }
    int browse(const char* dir, BrowseEntry* out, int max) override
    {
        std::string d = dir;
        if (d.empty() || d.back() != '/') d += '/';
        std::map<std::string, int> folders;
        std::vector<std::pair<std::string, uint32_t>> found;
        for (auto& kv : files)
        {
            if (kv.first.compare(0, d.size(), d) != 0) continue;
            const std::string rest = kv.first.substr(d.size());
            const size_t slash = rest.find('/');
            if (slash != std::string::npos) folders[rest.substr(0, slash)]++;
            else if (rest.size() > 4 && rest.compare(rest.size() - 4, 4, ".wav") == 0) found.push_back({ rest, (uint32_t)kv.second.size() });
        }
        int n = 0;
        for (auto& f : folders) if (n < max) { std::snprintf(out[n].name, sizeof out[n].name, "%s", f.first.c_str()); out[n].dir = true; out[n].bytes = 0; out[n].count = f.second; n++; }
        for (auto& f : found) if (n < max) { std::snprintf(out[n].name, sizeof out[n].name, "%s", f.first.c_str()); out[n].dir = false; out[n].bytes = f.second; out[n].count = 0; n++; }
        return n;
    }
    uint32_t rawRead(const char* path, uint32_t off, void* buf, uint32_t n) override { return fileRead(path, off, buf, n); }
    bool loadSampleFile(const char* path, int slot) override
    {
        const char* base = std::strrchr(path, '/');
        loaded[slot] = base ? base + 1 : path;
        return true;
    }
    void previewFile(const char*, bool on) override { previewing = on; }
    bool previewActive() override { return previewing; }
    int currentSample() override { return curSample; }
    void setCurrentSample(int s) override { curSample = s; }
    void sampleMemory(uint32_t& u, uint32_t& t) override { u = 0; t = 2600000; }
    PadSample sampleData[48];
    bool padSample(int slot, PadSample& out) override { if (slot < 0 || slot > 11) return false; out = sampleData[slot]; std::snprintf(out.name, sizeof out.name, "Sample %d", slot + 1); out.frames = 44100; out.bytes = 88200; return true; }
    void setPadSample(int slot, const PadSample& in) override { if (slot >= 0 && slot < 48) sampleData[slot] = in; }
    int padSampleWave(int, uint8_t* out, int columns) override { for (int i = 0; i < columns; i++) out[i] = (uint8_t)(i % 200); return columns; }
    void padAudition(int, bool) override {}
    bool useMask = true;
    std::map<int, std::vector<uint8_t>> masks;
    bool maskDefault() override { return useMask; }
    void maskWrite(int slot, uint32_t off, const uint8_t* d, uint32_t n) override
    {
        auto& m = masks[slot];
        if (m.size() < off + n) m.resize(off + n);
        std::memcpy(m.data() + off, d, n);
    }
    int devVal[7] = { 3, 0, 2, 3, 0, 80, 2 };
    bool fwInstalled = false;
    uint32_t fwAt = 0;
    int deviceValue(Dev d) override { return devVal[(int)d]; }
    void setDeviceValue(Dev d, int v) override { devVal[(int)d] = v; }
    void firmwareVersion(char* out, int size) override { std::snprintf(out, size, "1.0.0 test"); }
    int firmwareFiles(char (*names)[28], int max) override { if (max < 1 || !files.count("/Firmware/tracker-ft6.ptf")) return 0; std::snprintf(names[0], 28, "tracker-ft6.ptf"); return 1; }
    bool firmwareInstall(const char*) override { fwInstalled = true; fwAt = nowMs; return true; }
    int firmwareProgress() override { return fwInstalled ? (nowMs - fwAt >= 3000 ? 100 : 50) : 0; }
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
                for (int i = 0; i < 128; i++) mem[recTrack][recLen + i] = (recLen + i) < quiet ? 0 : (int16_t)(((recLen + i) / 50 % 2) ? 9000 : -9000);
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
    {   // count-in length and waiting for sound
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kB2);                 // Starting tempo
        app.key(kRight);              // the count-in box
        app.jog(1);                   // 4 beats -> 8 beats
        CHECK(app.settings().countBeats() == 8, "the count-in can be 8 beats");
        app.key(kRight);              // the wait box
        app.key(kJogPress);
        CHECK(app.settings().waitSound, "Insert switches Wait for sound on");
        app.key(kB7);
        hal.quiet = FakeHal::kRate * 3 / 2;
        app.key(kRec);
        hal.advance(app, FakeHal::kRate * 3);
        CHECK(app.state().mode() == Mode::Idle, "still counting in after three seconds");
        hal.advance(app, FakeHal::kRate * 3 / 2);
        CHECK(hal.clicks == 8, "eight clicks before it arms");
        CHECK(app.state().mode() == Mode::RecordingFirst, "it is armed and recording is running");
        CHECK(app.view().statusKind == 3 && std::strstr(app.view().hint, "starts when") != nullptr, "it shows Armed, waiting for sound");
        hal.advance(app, FakeHal::kRate * 2);
        CHECK(app.view().statusKind == 2, "the sound starts the take");
        app.key(kStop);
        CHECK(app.state().mode() == Mode::ChoosingBars, "Stop asks how many bars");
        const uint32_t pending = app.state().pendingFrames();
        CHECK(pending > FakeHal::kRate && pending < hal.recLen - FakeHal::kRate / 2, "the time spent waiting is not in the take");
    }
    {   // Stop cancels a wait without a popup
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kB2); app.key(kRight); app.key(kRight); app.key(kJogPress); app.key(kB7);
        app.key(kRec);
        hal.quiet = FakeHal::kRate * 100;
        hal.advance(app, FakeHal::kRate * 4);
        app.key(kStop);
        CHECK(app.state().mode() == Mode::Idle && app.popup() == Popup::None, "Stop cancels waiting quietly");
    }
    {   // the Play button starts and stops, on the main screen and on pages
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        CHECK(hal.playing, "the loop plays after the bars are chosen");
        app.key(kPlay);
        CHECK(!hal.playing, "Play stops it");
        app.key(kPlay);
        CHECK(hal.playing, "Play again starts it");
        app.key(kPerformance);
        app.key(kPlay);
        CHECK(!hal.playing, "Play stops it on a page");
        app.key(kPlay);
        CHECK(hal.playing, "and starts it again");
    }
    {   // the track mixer opens with Params and changes the track
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kParams);
        CHECK(app.screen() == Screen::Mixer, "Params opens the mixer");
        app.jog(1);
        CHECK(hal.trackBank.cfg[0].levelDb10 == 5, "the jog changes the level by half a dB");
        app.key(kDown); app.jog(1);
        CHECK(hal.trackBank.cfg[0].pan == 5, "Down moves to pan");
        app.key(kDown); app.setShift(true); app.jog(-1); app.setShift(false);
        CHECK(hal.trackBank.cfg[0].tone == -1, "Shift makes tone steps of 1");
        app.key(kB2);
        CHECK(hal.trackBank.cfg[0].solo, "B2 solos the track");
        app.key(kB3);
        CHECK(hal.trackBank.cfg[0].levelDb10 == 0 && hal.trackBank.cfg[0].pan == 0 && !hal.trackBank.cfg[0].mute, "Reset puts level, pan and tone back");
        CHECK(!app.pageOverflowed() && app.stagesOf(4) <= 5, "the mixer fits the drawing budget");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.key(kB8);
        CHECK(app.screen() == Screen::Main, "Back goes home");
    }
    {   // pad sample settings
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        hal.sampleData[0].volume = 100; hal.sampleData[0].end = 65535;
        app.key(kSamplePlay);
        CHECK(app.screen() == Screen::PadSample, "Sample Play opens the pad sample page");
        app.jog(1);
        CHECK(hal.sampleData[0].tuneSt == 1, "the jog changes the tune");
        app.key(kRight); app.jog(-1);
        CHECK(hal.sampleData[0].volume == 98, "the next control is the volume");
        app.key(kRight); app.key(kRight); app.jog(1);
        CHECK(hal.sampleData[0].loop, "the play mode switches to loop");
        app.key(kRight); app.jog(1);
        CHECK(hal.sampleData[0].start > 0, "the jog moves Start");
        CHECK(!app.pageOverflowed() && app.stagesOf(4) <= 5, "the page fits the drawing budget");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.pad(3, true); app.pad(3, false);
        CHECK(hal.curSample == 3, "a pad chooses its sample on this page");
        app.key(kB8);
        CHECK(app.screen() == Screen::Main, "Back goes home");
    }
    {   // Load samples: browse the card, load into slots, replace asks first
        FakeHal hal;
        auto wav = [&](const char* path, uint32_t frames)
        {
            std::vector<uint8_t> f(44 + frames * 2, 0);
            wavHeader(f.data(), frames * 2, 44100);
            hal.files[path] = f;
        };
        wav("/Samples/Drums/Kick.wav", 4000);
        wav("/Samples/Drums/Snare.wav", 2000);
        wav("/Samples/Drums/808/Hat.wav", 1000);
        wav("/Samples/Keys/A.wav", 3000);
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kSampleLoad);
        CHECK(app.screen() == Screen::Load, "Sample Load opens the load page");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(!app.pageOverflowed() && app.stagesOf(4) <= 5, "the load page fits the drawing budget");
        app.key(kJogPress);             // the first entry is the folder Drums
        app.key(kDown);                 // 808 is a folder, so the first file is the one below it
        app.key(kB1);
        CHECK(hal.loaded.count(0) && hal.loaded[0] == "Kick.wav", "Load puts the file in the first empty slot");
        app.key(kRight);
        app.key(kB4);
        app.key(kB4);
        CHECK(hal.loaded.count(1) && hal.loaded[1] == "Kick.wav" && hal.loaded.count(2) && hal.loaded[2] == "Snare.wav", "Load next moves on to the next file and slot");
        app.key(kLeft); app.key(kLeft);
        app.key(kB1);
        CHECK(app.popup() == Popup::ReplaceSlot, "loading onto a used slot asks first");
        for (int st = 0; st < app.stagesOf(5); st++) app.draw(5, st, gfx);
        app.key(kB7);
        CHECK(app.popup() == Popup::None && hal.loaded[0] == "Kick.wav", "Cancel keeps the sample");
        app.key(kB2);
        CHECK(hal.previewing, "Preview plays the highlighted file");
        app.key(kB2);
        CHECK(!hal.previewing, "Preview again stops it");
        app.key(kB3);
        app.key(kB8);
        CHECK(app.screen() == Screen::Main, "Back goes home");
    }
    {   // Resample: render the mix into a pad slot
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kSampleRec);
        app.key(kB1);
        CHECK(app.popup() == Popup::Message, "resampling with nothing recorded says so");
        app.key(kB8);
        app.key(kPattern);
        recordLoop(hal, app);
        const uint32_t loop = app.state().loopFrames();
        app.key(kSampleRec);
        CHECK(app.screen() == Screen::Resample, "Sample Rec opens Resample");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(!app.pageOverflowed() && app.stagesOf(4) <= 5, "the resample page fits the drawing budget");
        app.jog(1);
        app.key(kB1);
        for (int i = 0; i < 20; i++) { hal.advance(app, 128); }
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        for (int i = 0; i < 4000 && hal.loaded.empty(); i++) hal.advance(app, 128);
        CHECK(hal.fileSize("/Samples/Resamples/Resample 01.wav") == 44 + loop * 2 * 2, "two loops are written as a WAV");
        CHECK(hal.loaded.count(0) && hal.loaded[0] == "Resample 01.wav", "the WAV is loaded into the first empty slot");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.key(kB1);
        for (int i = 0; i < 4000 && hal.loaded.size() < 2; i++) hal.advance(app, 128);
        CHECK(hal.fileSize("/Samples/Resamples/Resample 02.wav") > 44, "a second resample gets the next name");
        app.key(kB1);
        app.key(kB8);
        CHECK(app.screen() == Screen::Resample, "Back is off while recording");
        app.key(kB1);
        app.key(kB8);
        CHECK(app.screen() == Screen::Main, "Back goes home once the recording is cancelled");
    }
    {   // Device settings: groups, values, firmware update
        FakeHal hal;
        std::vector<uint8_t> f(100, 1);
        hal.files["/Firmware/tracker-ft6.ptf"] = f;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kConfig);
        CHECK(app.screen() == Screen::Device, "Config opens Device settings");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(!app.pageOverflowed() && app.stagesOf(4) <= 5, "the device page fits the drawing budget");
        app.key(kDown);
        app.jog(-1);
        CHECK(hal.devVal[(int)Dev::MidiInChannel] == 0 && hal.devVal[(int)Dev::MidiIn] == 3, "the jog changes the focused row only");
        app.jog(1);
        CHECK(hal.devVal[(int)Dev::MidiInChannel] == 1, "jog up moves the MIDI in channel to 1");
        app.key(kLeft);
        app.key(kDown);
        app.key(kDown);
        app.key(kDown);
        app.key(kRight);
        app.jog(-1);
        CHECK(hal.devVal[(int)Dev::Volume] == 79, "Volume group: the jog changes the volume");
        app.setShift(true); app.jog(-1); app.setShift(false);
        CHECK(hal.devVal[(int)Dev::Volume] == 74, "shift + jog moves in larger steps");
        app.key(kLeft);
        app.key(kDown); app.key(kDown);
        app.key(kRight);
        app.jog(1);
        CHECK(app.settings().windowIdx == 3, "Four-Track group: the alignment window changes");
        app.key(kLeft);
        app.key(kDown);
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.key(kB1);
        CHECK(!hal.fwInstalled, "the updater starts after the update screen has been drawn");
        hal.nowMs += 800;
        app.tick();
        CHECK(hal.fwInstalled, "B1 in the Firmware group starts the update");
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        app.key(kB8);
        CHECK(app.screen() == Screen::Device, "nothing answers during the update");
        hal.nowMs += 4000;
        app.tick();
        CHECK(app.screen() == Screen::Main, "the emulator returns home when its pretend update ends");
    }
    {   // a page button works straight from another page
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kParams);
        app.key(kSampleLoad);
        CHECK(app.screen() == Screen::Load, "Sample Load opens from the mixer");
        app.key(kConfig);
        CHECK(app.screen() == Screen::Device, "Config opens from Load samples");
        app.key(kSamplePlay);
        CHECK(app.screen() == Screen::PadSample, "Sample Play opens from Device settings");
        app.key(kParams);
        CHECK(app.screen() == Screen::Mixer, "Params opens from the pad sample page");
    }
    {   // the home screen with four Parts stays under the display chip's limit (8172 bytes), overview and focus
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        for (int t = 1; t < 4; t++) { app.key(kB5 + t); app.key(kRec); hal.advance(app, hal.loop * 3); }
        auto bytes = [&]()
        {
            int total = 0;
            for (int layer = 0; layer < 3; layer++)
                for (int st = 0; st < app.stagesOf(layer); st++) { Canvas c; c.setMeasure(&gfx); app.draw(layer, st, c); total += c.estimatedBytes(); }
            return total;
        };
        app.key(kB5); hal.advance(app, 2000);
        CHECK(bytes() < 7500, "the overview with four Parts is under 7.5 KB");
        app.key(kB5); hal.advance(app, 2000);
        CHECK(app.view().focusLane == 0 && bytes() < 7500, "the focus layout is under 7.5 KB");
    }
    {   // waveform bitmaps: one-bit pictures of the loop, drawn once per Part
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        hal.advance(app, 3000);
        CHECK(app.view().lane[0].bitmap, "the Part's waveform is a bitmap");
        CHECK(hal.masks.count(0) && hal.masks.count(1) && hal.masks.count(2) && hal.masks[0].size() == 12 * 75 * 7, "three sizes were uploaded for track 1");
        int setBits = 0;
        for (uint8_t b : hal.masks[0]) for (int k = 0; k < 8; k++) setBits += (b >> k) & 1;
        CHECK(setBits > 600 * 2 && setBits < 600 * 84, "the picture has the waveform in it");
        // the middle rows of the overview picture are set for every column that has sound
        int middle = 0;
        for (int c = 0; c < 600; c++) middle += (hal.masks[0][41 * 75 + (c >> 3)] >> (7 - (c & 7))) & 1;
        CHECK(middle == 600, "the centre line is drawn along the whole width");
        FakeHal cols;
        cols.useMask = false;
        App app2(cols, palette());
        app2.setMeasure(&gfx);
        app2.enter();
        recordLoop(cols, app2);
        cols.advance(app2, 3000);
        CHECK(!app2.view().lane[0].bitmap && cols.masks.empty(), "with columns nothing is uploaded");
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

        // Same track again opens it in focus, and again closes it; Stop closes it too.
        CHECK(app.view().focusLane == -1, "the overview shows four lanes");
        app.key(kB6);
        CHECK(app.view().focusLane == 1, "pressing the selected track again opens it in focus");
        app.key(kB6);
        CHECK(app.view().focusLane == -1, "and again returns to the overview");
        app.key(kB6);
        app.key(kStop);
        CHECK(app.view().focusLane == -1, "Stop returns to the overview");

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
        CHECK(app.state().pendingFrames() == FakeHal::kRate * 16, "the take is limited to 16 seconds");
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
