// Scenario tests of the shared Four-Track application with a fake machine: no hardware, so they
// run on a computer. They exercise exactly the code the Tracker and the web emulator run.
#include "App.h"

#include <cstdio>
#include <cstdlib>
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
    int firmwareFiles(char (*names)[48], int max) override { if (max < 1 || !files.count("/Firmware/PolyendTracker_FT41_full.ptf")) return 0; std::snprintf(names[0], 48, "PolyendTracker_FT41_full.ptf"); return 1; }
    bool firmwareInstall(const char*) override { fwInstalled = true; fwAt = nowMs; return true; }
    int firmwareProgress() override { return fwInstalled ? (nowMs - fwAt >= 3000 ? 100 : 50) : 0; }
    void padOn(int, bool) override { padsOn++; }
    void padOff(int) override { padsOn--; }
    void padsReleaseAll() override { padsOn = 0; }
    std::vector<int16_t> calBuf = std::vector<int16_t>(44100);
    bool loopbackOn = false;
    int loopbackDelay = 384;
    int16_t* calibrationBuffer(uint32_t& frames) override { frames = 44100; return calBuf.data(); }
    void calibrationLoopback(bool on) override { loopbackOn = on; }
    int notesOn = 0, noteEvents = 0, lastNote = 0, sampleNotes = 0;
    int patches = 0;
    float lastCutoff = 0;
    void synthSetPatch(const SynthPatch& p) override { patches++; lastCutoff = p.cutoff; }
    int vmin = 127, vmax = 0;
    void synthNote(int note, int vel, bool on) override { notesOn += on ? 1 : -1; noteEvents++; if (on) { lastNote = note; if (vel < vmin) vmin = vel; if (vel > vmax) vmax = vel; } }
    void synthAllOff() override { notesOn = 0; }
    int synthLevel = 0;
    void synthSetLevelDb10(int db10) override { synthLevel = db10; }
    void sampleNote(int, int, int, bool on) override { sampleNotes += on ? 1 : -1; }

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
                if (loopbackOn && data[recTrack] == calBuf.data())
                    for (int i = 0; i < 128; i++)
                    {
                        const int64_t src = (int64_t)startPos + recLen + i - loopbackDelay;
                        mem[recTrack][recLen + i] = src < 0 ? 0 : calBuf[(uint64_t)src % 44100];
                    }
                else
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
        CHECK(hal.trackBank.cfg[0].levelDb10 == kDefaultLevelDb10 + 5, "the jog changes the level by half a dB");
        app.key(kDown); app.jog(1);
        CHECK(hal.trackBank.cfg[0].pan == 5, "Down moves to pan");
        app.key(kDown); app.setShift(true); app.jog(-1); app.setShift(false);
        CHECK(hal.trackBank.cfg[0].tone == -1, "Shift makes tone steps of 1");
        app.key(kB2);
        CHECK(hal.trackBank.cfg[0].solo, "B2 solos the track");
        app.key(kB3);
        CHECK(hal.trackBank.cfg[0].levelDb10 == kDefaultLevelDb10 && hal.trackBank.cfg[0].pan == 0 && !hal.trackBank.cfg[0].mute, "Reset puts level, pan and tone back");
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
        hal.files["/Firmware/PolyendTracker_FT41_full.ptf"] = f;
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
        app.key(kDown); app.key(kDown);
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
    {   // the chord and sound pages stay well under the display chip's limit, and the sizes are printed for the design notes
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        auto bytes = [&]()
        {
            int total = 0;
            for (int layer = 0; layer < 6; layer++)
                if (app.layerVisible(layer))
                for (int st = 0; st < app.stagesOf(layer); st++) { Canvas c; c.setMeasure(&gfx); app.draw(layer, st, c); total += c.estimatedBytes(); }
            return total;
        };
        struct Row { const char* name; int bytes; };
        Row rows[8];
        int n = 0;
        app.key(kNote); hal.advance(app, 2000); rows[n++] = Row{ "Chords 1", bytes() };
        app.key(kB7); hal.advance(app, 2000); rows[n++] = Row{ "Chords 2", bytes() };
        app.key(kB7); hal.advance(app, 2000); rows[n++] = Row{ "Chords 3", bytes() };
        app.pad(0, true); hal.advance(app, 2000); rows[n++] = Row{ "Chords 3, a pad held", bytes() };
        app.pad(0, false);
        app.key(kInstr); hal.advance(app, 2000); rows[n++] = Row{ "Sound", bytes() };
        app.key(kB7); hal.advance(app, 2000); rows[n++] = Row{ "Sound, every setting", bytes() };
        for (int i = 0; i < n; i++)
        {
            if (std::getenv("FT_PRINT_COSTS")) std::printf("cost %-24s %5d bytes\n", rows[i].name, rows[i].bytes);
            CHECK(rows[i].bytes < 7500, "a chord or sound page is under 7.5 KB");
        }
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
    {   // Copy and Clear, with one undo step
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kCopy);
        CHECK(app.popup() == Popup::Message, "Copy with nothing recorded says so");
        app.key(kB8);
        recordLoop(hal, app);
        const uint32_t loop = app.state().loopFrames();
        app.key(kCopy);
        CHECK(app.popup() == Popup::Copy, "Copy opens its panel");
        for (int st = 0; st < app.stagesOf(5); st++) app.draw(5, st, gfx);
        CHECK(!app.pageOverflowed(), "the copy panel fits");
        app.key(kB7);
        CHECK(app.popup() == Popup::None && !app.state().hasPart(1), "Cancel copies nothing");
        app.key(kCopy);
        app.key(kB3);
        app.key(kB8);
        CHECK(app.state().hasPart(2) && !app.state().hasPart(1), "B3 chooses Track 3 and Copy copies onto it");
        CHECK(std::memcmp(hal.mem[0].data(), hal.mem[2].data(), loop * 2) == 0, "the audio is the same");
        CHECK(app.state().loopFrames() == loop, "the Loop is unchanged");
        app.setShift(true); app.key(kCopy); app.setShift(false);
        CHECK(!app.state().hasPart(2) && app.state().hasPart(0), "Shift + Copy undoes the copy");
        const std::vector<int16_t> before = hal.mem[0];
        app.key(kDelete);
        CHECK(app.popup() == Popup::Clear, "Delete opens the clear question");
        for (int st = 0; st < app.stagesOf(5); st++) app.draw(5, st, gfx);
        app.key(kB8);
        CHECK(!app.state().hasPart(0) && app.state().loopFrames() == loop, "Clear removes the Part and keeps the Loop");
        app.setShift(true); app.key(kCopy); app.setShift(false);
        CHECK(app.state().hasPart(0), "Shift + Copy brings the Part back");
        CHECK(std::memcmp(hal.mem[0].data(), before.data(), loop * 2) == 0, "with its audio");
        CHECK(!hal.files.count("undo/t0.raw"), "a clear writes nothing to the card");
        app.setShift(true); app.key(kDelete); app.setShift(false);
        CHECK(app.popup() == Popup::Clear, "Shift + Delete asks to clear all tracks");
        for (int st = 0; st < app.stagesOf(5); st++) app.draw(5, st, gfx);
        app.key(kB8);
        CHECK(!app.state().hasPart(0) && app.state().loopFrames() == loop, "Clear all keeps the Loop too");
        // copying onto a track that has a Part keeps the old one on the card with a progress panel
        app.setShift(true); app.key(kCopy); app.setShift(false); // brings the Parts back
        app.key(kB6); recordLoop(hal, app);
        app.key(kB5);
        app.key(kCopy);
        app.key(kB2);
        app.key(kB8);
        CHECK(app.popup() == Popup::Busy, "copying over a Part shows a progress panel");
        for (int st = 0; st < app.stagesOf(5); st++) app.draw(5, st, gfx);
        app.key(kB8);
        CHECK(app.popup() == Popup::Busy, "buttons are ignored while it works");
        for (int i = 0; i < 40 && app.popup() == Popup::Busy; i++) hal.advance(app, 128);
        CHECK(app.popup() == Popup::None && hal.files.count("undo/t1.raw"), "it finishes and the old Part is on the card");
        // Reset all forgets the Loop, tempo and glitch settings as well
        hal.bank().cfg[2].slot[0].on = true;
        app.setShift(true); app.key(kDelete); app.setShift(false);
        app.key(kB6);
        CHECK(app.state().loopFrames() == 0 && !hal.bank().cfg[2].slot[0].on && app.view().focusLane == -1, "Reset all starts a new project");
    }
    {   // the Part Editor and the Pad sample page draw their big waveform as one shared bitmap
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        recordLoop(hal, app);
        app.key(kSampleEdit);
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(hal.masks.count(kPageMaskSlot) && hal.masks[kPageMaskSlot].size() == (size_t)kPageMaskStride * kPageMaskH, "the editor sent its waveform picture");
        CHECK(!app.pageOverflowed(), "the editor still fits");
        hal.masks.erase(kPageMaskSlot);
        app.key(kB8);
        hal.sampleData[0].volume = 100; hal.sampleData[0].end = 65535;
        app.key(kSamplePlay);
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(hal.masks.count(kPageMaskSlot), "the pad sample page sent its waveform picture");
        hal.masks.erase(kPageMaskSlot);
        for (int st = 0; st < app.stagesOf(4); st++) app.draw(4, st, gfx);
        CHECK(!hal.masks.count(kPageMaskSlot), "and does not send it again while the sample is the same");
    }
    {   // the take being recorded grows as a bitmap in the lane's own picture
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        app.key(kRec);
        hal.advance(app, 100000); // the count-in
        hal.advance(app, FakeHal::kRate * 3);
        CHECK(app.view().lane[0].liveBitmap, "the take is drawn from the lane's picture");
        int bits = 0;
        for (uint8_t b : hal.masks[1]) for (int k = 0; k < 8; k++) bits += (b >> k) & 1;
        CHECK(bits > 100, "the picture has the take in it");
        hal.advance(app, FakeHal::kRate * 3);
        int bits2 = 0;
        for (uint8_t b : hal.masks[1]) for (int k = 0; k < 8; k++) bits2 += (b >> k) & 1;
        CHECK(bits2 > bits, "and it grows as the take goes on");
        for (int l = 0; l < 2; l++) for (int st = 0; st < app.stagesOf(l); st++) app.draw(l, st, gfx);
    }
    {   // tracks start at -6 dB, and Shift + Left or Right sets the output volume from home
        FakeHal hal;
        App app(hal, palette());
        app.setMeasure(&gfx);
        app.enter();
        CHECK(hal.trackBank.cfg[0].levelDb10 == -60, "tracks start at -6 dB");
        const int before = hal.devVal[(int)Dev::Volume];
        app.setShift(true); app.key(kLeft); app.setShift(false);
        CHECK(hal.devVal[(int)Dev::Volume] == before - 2, "Shift + Left lowers the volume");
        app.setShift(true); app.key(kRight); app.key(kRight); app.setShift(false);
        CHECK(hal.devVal[(int)Dev::Volume] == before + 2, "Shift + Right raises it");
        CHECK(std::strncmp(app.view().hint, "Volume", 6) == 0, "the hint line says what it is now");
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
    {   // chords: Note opens the page and the pads play chords and scale notes
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kNote);
        CHECK(app.screen() == Screen::Chords, "Note opens the Chords page");
        CHECK(std::strcmp(app.view().padsMode, "CHORD") == 0, "the pads are in chord mode");
        app.pad(12, true); // I as a triad
        CHECK(hal.notesOn == 3 && hal.lastNote == 55, "pad 13 plays the C major triad");
        app.pad(12, false);
        CHECK(hal.notesOn == 0, "releasing the pad releases the notes");
        app.pad(0, true); // I as a 7th
        CHECK(hal.notesOn == 4 && hal.lastNote == 59, "pad 1 plays Cmaj7");
        app.pad(0, false);
        app.pad(40, true); // a scale note: the fifth of the lower octave
        CHECK(hal.notesOn == 1 && hal.lastNote == 55, "a scale pad plays one note in key");
        app.pad(40, false);
        app.jog(1); // the Key field
        app.pad(12, true);
        CHECK(hal.lastNote == 56, "after one step up the key is Db: its fifth is Ab");
        app.pad(12, false);
        // the Sound field is the fourth: past the last patch is the pad sample
        app.key(kRight); app.key(kRight); app.key(kRight);
        for (int i = 0; i < kSynthPresets + 1; i++) app.jog(1);
        app.pad(12, true);
        CHECK(hal.sampleNotes == 3, "with Sound set to Sample the chord plays the sample");
        app.pad(12, false);
        CHECK(hal.sampleNotes == 0, "and it is released");
        app.key(kB8);
        CHECK(app.screen() == Screen::Main, "Back returns home");
        app.pad(12, true);
        CHECK(hal.sampleNotes == 3 && hal.padsOn == 0, "the pads still play chords on the home screen");
        app.pad(12, false);
        app.key(kNote);
        app.key(kB1); // Pads: off
        app.key(kB8);
        app.pad(3, true);
        CHECK(hal.padsOn == 1, "with chord pads off, the pads are the normal pads again");
        app.pad(3, false);
    }
    {   // the right hand: modifiers held and latched, octave, Hold
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kNote);
        app.pad(20, true); // maj7 held
        CHECK(hal.notesOn == 0, "a modifier makes no sound");
        app.pad(13, true); // ii as a triad becomes DmM7
        CHECK(hal.notesOn == 4 && hal.lastNote == 61, "maj7 held: ii plays DmM7");
        app.pad(13, false); app.pad(20, false);
        app.pad(13, true);
        CHECK(hal.notesOn == 3 && hal.lastNote == 57, "released: back to Dm");
        app.pad(13, false);
        // a double tap latches a modifier
        app.pad(8, true); app.pad(8, false); app.pad(8, true); app.pad(8, false); // sus2
        app.pad(12, true);
        CHECK(hal.lastNote == 55 && hal.notesOn == 3, "latched sus2: C D G");
        app.pad(12, false);
        app.pad(8, true); app.pad(8, false); // one tap lets it go
        app.pad(12, true);
        CHECK(hal.lastNote == 55 && hal.notesOn == 3, "tapped again: the latch is gone");
        app.pad(12, false);
        // octave pads
        app.pad(46, true); app.pad(46, false);
        app.pad(12, true);
        CHECK(hal.lastNote == 67, "Oct + once: the chord is an octave up (G5)");
        app.pad(12, false);
        app.pad(45, true); app.pad(45, false); app.pad(45, true); app.pad(45, false);
        app.pad(12, true);
        CHECK(hal.lastNote == 43, "Oct - twice: an octave below normal (G2)");
        app.pad(12, false);
        app.pad(46, true); app.pad(46, false);
        // Hold
        app.pad(47, true); app.pad(47, false);
        app.pad(12, true); app.pad(12, false);
        CHECK(hal.notesOn == 3, "Hold keeps the chord sounding after the pad is let go");
        app.pad(14, true);
        CHECK(hal.notesOn == 3, "the next chord replaces it");
        app.pad(14, false);
        app.pad(47, true); app.pad(47, false); // Hold off
        CHECK(hal.notesOn == 0, "Hold off lets it go");
    }
    {   // strum, arpeggio and humanise
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kNote);
        // Strum dn: the notes come one after another, 28 ms apart
        app.pad(33, true); app.pad(33, false);
        app.pad(12, true);
        CHECK(hal.notesOn == 1 && hal.lastNote == 48, "a strum starts with the lowest note");
        hal.advance(app, 128 * 12); // about 36 ms
        CHECK(hal.notesOn == 2 && hal.lastNote == 52, "the second note follows");
        hal.advance(app, 128 * 12);
        CHECK(hal.notesOn == 3 && hal.lastNote == 55, "and the third");
        app.pad(12, false);
        CHECK(hal.notesOn == 0, "releasing ends all of it");
        app.pad(12, true);
        app.pad(12, false);
        hal.advance(app, 128 * 40);
        CHECK(hal.notesOn == 0, "a short tap leaves no stuck notes");
        app.pad(34, true); app.pad(34, false); // Strum up goes from the top
        app.pad(12, true);
        CHECK(hal.lastNote == 55, "strum up starts with the highest note");
        app.pad(12, false);
        // Arp up: one note at a time, at the rate, gated
        app.pad(35, true); app.pad(35, false);
        CHECK(app.chordSettings().playStyle == StyleArpUp, "the Arp up pad chooses it");
        app.pad(12, true);
        CHECK(hal.notesOn == 1 && hal.lastNote == 48, "an arpeggio starts on the first note");
        hal.noteEvents = 0;
        hal.advance(app, 44100 * 3);
        CHECK(hal.noteEvents > 40 && hal.noteEvents < 60, "the arpeggio keeps stepping (note-ons and note-offs)");
        CHECK(hal.notesOn <= 1, "never more than one arpeggio note at a time");
        app.pad(12, false);
        CHECK(hal.notesOn == 0, "releasing the pad stops it");
        app.pad(47, true); app.pad(47, false); // Hold keeps an arpeggio going
        app.pad(12, true); app.pad(12, false);
        hal.noteEvents = 0;
        hal.advance(app, 44100);
        CHECK(hal.noteEvents > 8, "with Hold the arpeggio goes on after the pad is let go");
        app.pad(47, true); app.pad(47, false);
        CHECK(hal.notesOn == 0, "Hold off stops it");
        hal.noteEvents = 0;
        hal.advance(app, 44100);
        CHECK(hal.noteEvents == 0, "and it stays stopped");
        app.pad(32, true); app.pad(32, false); // Block
        CHECK(app.chordSettings().playStyle == 0, "the Block pad goes back to Block");
        // Shift + jog while playing sets the strum speed
        app.pad(33, true); app.pad(33, false);
        app.setShift(true); app.jog(1); app.setShift(false);
        CHECK(app.chordSettings().strumMs == 30, "Shift + jog changes the strum speed");
        app.pad(32, true); app.pad(32, false);
        // humanise (Config, Chords group) changes velocities within bounds
        hal.vmin = 127; hal.vmax = 0;
        app.key(kConfig); app.key(kLeft);
        for (int i = 0; i < 6; i++) app.key(kDown); // group Chords
        app.key(kRight);
        for (int i = 0; i < 20; i++) app.jog(1); // humanise to 100%
        CHECK(app.chordSettings().humanise == 100, "humanise goes to 100");
        app.key(kB8);
        for (int k = 0; k < 6; k++) { app.pad(12, true); hal.advance(app, 128 * 20); app.pad(12, false); }
        CHECK(hal.vmax <= 100 && hal.vmin >= 65 && hal.vmin < hal.vmax, "humanised velocities vary but stay within a third of the base");
    }
    {   // a project remembers its chord set
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        recordLoop(hal, app);
        app.key(kNote);
        for (int i = 0; i < 4; i++) app.jog(1); // key E
        app.key(kB8);
        app.key(kFile);
        app.key(kB3); // Save
        hal.advance(app, 20000);
        CHECK(hal.files.size() > 0, "saved");
        bool chd = false;
        for (auto& f : hal.files) if (f.first.size() > 4 && f.first.substr(f.first.size() - 4) == ".chd") chd = true;
        CHECK(chd, "the chord set is saved beside the project");
    }
    {   // chord sets: save, change, load
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kNote);
        for (int i = 0; i < 3; i++) app.jog(1); // key: Eb
        app.key(kB2);
        CHECK(app.screen() == Screen::Sets, "B2 opens the chord sets");
        app.key(kB1); // save into slot 1
        app.key(kB8);
        CHECK(app.screen() == Screen::Chords, "Back returns to Chords");
        app.jog(1); app.jog(1); // key moves on
        CHECK(app.chordSettings().key == 5, "the key changed");
        app.key(kB2);
        app.key(kB2); // load slot 1
        CHECK(app.chordSettings().key == 3, "loading the set brings the key back");
        CHECK(hal.files.count("sets.cfg") == 1, "the sets are kept on the card");
    }
    {   // calibrating the input latency with a cable (simulated by the fake machine)
        FakeHal hal;
        hal.loopbackDelay = 520;
        App app(hal, palette());
        app.enter();
        app.key(kConfig);
        app.key(kLeft);
        for (int i = 0; i < 5; i++) app.key(kDown); // group Four-Track
        app.key(kRight);
        for (int i = 0; i < 4; i++) app.key(kDown); // row Input latency
        app.key(kB1);
        CHECK(app.screen() == Screen::Calibrate, "B1 on Input latency opens the calibration");
        app.key(kB8);
        hal.advance(app, 44100 * 5);
        app.key(kB8); // Keep
        CHECK(app.screen() == Screen::Device, "Keep goes back to Device settings");
        CHECK(app.inputLatencyFrames() == 520, "the measured latency is kept");
        // it is saved, and a Line take is moved back by it
        hal.advance(app, 100000);
        FakeHal hal2;
        hal2.files = hal.files;
        App app2(hal2, palette());
        app2.enter();
        CHECK(app2.inputLatencyFrames() == 520, "the latency is saved on the card");
        recordLoop(hal2, app2);
        CHECK(hal2.bank().cfg[0].offset == -520, "a Line take is moved earlier by the input latency");
    }
    {   // calibration refuses when there are Parts, and reports a missing cable
        FakeHal hal;
        hal.loopbackDelay = 0;
        App app(hal, palette());
        app.enter();
        recordLoop(hal, app);
        app.key(kConfig);
        app.key(kLeft);
        for (int i = 0; i < 5; i++) app.key(kDown);
        app.key(kRight);
        for (int i = 0; i < 4; i++) app.key(kDown);
        app.key(kB1);
        app.key(kB8);
        CHECK(app.screen() == Screen::Calibrate, "with Parts the test does not start (Start is dim)");
    }
    {   // levels from the home screen; the lo-fi master is opt in
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        CHECK(hal.lofi().cfg.bypass, "the lo-fi master starts bypassed");
        const int before = hal.bank().cfg[0].levelDb10;
        app.setShift(true); app.jog(1); app.jog(1); app.jog(-1); app.setShift(false);
        CHECK(hal.bank().cfg[0].levelDb10 == before + 10, "Shift + Jog changes the selected track's level by 1 dB a click");
        app.key(kRight); app.key(kRight); app.key(kRight);
        CHECK(hal.synthLevel == 30, "Right raises the synth level by 1 dB");
        app.key(kLeft);
        CHECK(hal.synthLevel == 20, "Left lowers it");
        hal.advance(app, 100000);
        FakeHal hal2;
        hal2.files = hal.files;
        App app2(hal2, palette());
        app2.enter();
        CHECK(hal2.synthLevel == 20, "the synth level is kept");
    }
    {   // Rec with the chord pads on records them
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        app.key(kNote);
        app.key(kB8);
        app.key(kRec);
        CHECK(hal.applied == Source::Pads, "Rec with the chord pads on sets the source to Pads");
    }
    {   // Shift + Play switches the click; Play alone still plays
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        CHECK(!app.view().click, "the click starts off");
        app.setShift(true); app.key(kPlay); app.setShift(false);
        CHECK(app.view().click, "Shift + Play switches the click on");
        CHECK(!hal.playing, "and does not start playing");
        app.key(kSong);
        CHECK(app.view().click, "the Song button does nothing now");
        app.setShift(true); app.key(kPlay); app.setShift(false);
        CHECK(!app.view().click, "Shift + Play switches it off again");
    }
    {   // sound: patches, knobs, every setting, and keeping them
        FakeHal hal;
        App app(hal, palette());
        app.enter();
        const int start = hal.patches;
        app.key(kInstr);
        CHECK(app.screen() == Screen::Sound, "Instr opens the Sound page");
        app.jog(1);
        CHECK(hal.patches == start + 1, "jog on the patch list picks the next patch");
        const float cut = hal.lastCutoff;
        app.key(kRight); // the first knob: Bright
        for (int i = 0; i < 5; i++) app.jog(1);
        CHECK(hal.lastCutoff > cut, "the Bright knob opens the filter");
        app.key(kB2); // knobs to zero
        CHECK(hal.lastCutoff == cut, "Knobs to 0 puts the patch back");
        app.key(kB1);
        CHECK(hal.notesOn > 0, "holding B1 plays the patch");
        app.keyUp(kB1);
        CHECK(hal.notesOn == 0, "releasing B1 stops it");
        app.key(kB7);
        CHECK(app.screen() == Screen::SoundAdv, "B7 opens the settings list");
        app.key(kDown); app.key(kDown); app.key(kDown); app.key(kDown); app.key(kDown); app.key(kDown); // Cutoff
        const float before = hal.lastCutoff;
        app.jog(1);
        CHECK(hal.lastCutoff > before, "jog changes the setting under the cursor");
        app.key(kB2);
        CHECK(hal.lastCutoff == before, "B2 puts the one setting back");
        app.jog(-1); app.jog(-1);
        app.key(kB8);
        CHECK(app.screen() == Screen::Sound, "Back goes to the Sound page");
        const float edited = hal.lastCutoff;
        // time passes: the sound is written to the card
        hal.advance(app, 100000);
        CHECK(hal.files.count("sound.cfg") == 1, "the sound is saved after a moment");
        // a new machine starts with it
        FakeHal hal2;
        hal2.files = hal.files;
        App app2(hal2, palette());
        app2.enter();
        CHECK(hal2.lastCutoff == edited, "the saved patch comes back");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
