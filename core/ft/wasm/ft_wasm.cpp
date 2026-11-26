// The Four-Track application compiled to WebAssembly for the web emulator. It runs the same
// core/ft code as the Tracker firmware. This file is the emulator's "machine": it provides the
// Hal (recording, playing, pads, time) from the browser's audio, and collects the screen drawing
// into a list of commands that the page paints on a canvas.
//
// No C library is available, so the few functions the compiler needs are defined at the bottom.

#include "App.h"
#include "loop_fill.h"

#include <stddef.h>
#include <stdint.h>

using namespace ft;

inline void* operator new(size_t, void* where) noexcept { return where; }
void operator delete(void*) noexcept {} // nothing is ever freed here
void operator delete(void*, size_t) noexcept {}

namespace
{
constexpr uint32_t kMaxRate = 48000;
constexpr uint32_t kRegionSeconds = 18;
constexpr int kBlock = 128;

int16_t g_mem[4][kMaxRate * kRegionSeconds];

float frac(float x) { return x - (float)(int)x; }
float saw(float ph) { return frac(ph) * 2.0f - 1.0f; }
float triangle(float ph) { const float f = frac(ph); return f < 0.5f ? f * 4.0f - 1.0f : 3.0f - f * 4.0f; }

float g_inL[kBlock], g_inR[kBlock], g_outL[kBlock], g_outR[kBlock];
uint32_t g_palette[21];
uint8_t g_adv[5][128];

//------------------------------------------------------------------------------------------
// Commands for the canvas: 7 ints each.
enum Op { kColor = 1, kRect = 2, kLine = 3, kDot = 4, kText = 5, kBars = 6, kAlpha = 7, kMask = 8 };
constexpr int kMaxCommands = 4096;
constexpr int kPoolSize = 32768;
int32_t g_cmd[kMaxCommands * 7];
uint8_t g_pool[kPoolSize];
int g_cmdCount = 0;
int g_poolUsed = 0;

class BufferGfx : public Gfx
{
  public:
    void color(uint32_t rgb) override { push(kColor, (int)rgb, 0, 0, 0, 0, 0); }
    void alpha(int a) override { push(kAlpha, a, 0, 0, 0, 0, 0); }
    void mask(int slot, int x, int y, int w, int h) override { push(kMask, slot, x, y, w, h, 0); }
    void rect(int x, int y, int w, int h) override { push(kRect, x, y, w, h, 0, 0); }
    void line(int x0, int y0, int x1, int y1) override { push(kLine, x0, y0, x1, y1, 0, 0); }
    void dot(int x, int y, int radius) override { push(kDot, x, y, radius, 0, 0, 0); }
    void text(int x, int y, const char* s, int font) override
    {
        int len = 0;
        while (s[len] != 0) len++;
        const int off = store((const uint8_t*)s, len);
        push(kText, x, y, font, off, len, 0);
    }
    int textWidth(const char* s, int font) override
    {
        int w = 0;
        for (; *s != 0; s++) w += g_adv[font][(uint8_t)*s & 127];
        return w;
    }
    void bars(int x0, int cy, int step, int barWidth, const uint8_t* heights, int count) override
    {
        const int off = store(heights, count);
        push(kBars, x0, cy, step, barWidth, off, count);
    }

  private:
    void push(int op, int a, int b, int c, int d, int e, int f)
    {
        if (g_cmdCount >= kMaxCommands) return;
        int32_t* p = &g_cmd[g_cmdCount++ * 7];
        p[0] = op; p[1] = a; p[2] = b; p[3] = c; p[4] = d; p[5] = e; p[6] = f;
    }
    int store(const uint8_t* data, int len)
    {
        if (g_poolUsed + len > kPoolSize) return 0;
        const int off = g_poolUsed;
        for (int i = 0; i < len; i++) g_pool[off + i] = data[i];
        g_poolUsed += len;
        return off;
    }
};

//------------------------------------------------------------------------------------------
// Files: kept by the page (browser memory, saved to browser storage), reached through imports
extern "C"
{
__attribute__((import_module("host"), import_name("file_write"))) int host_file_write(const char* path, const void* data, uint32_t len, int append);
__attribute__((import_module("host"), import_name("file_read"))) uint32_t host_file_read(const char* path, uint32_t offset, void* buf, uint32_t len);
__attribute__((import_module("host"), import_name("file_size"))) uint32_t host_file_size(const char* path);
__attribute__((import_module("host"), import_name("file_remove"))) int host_file_remove(const char* path);
__attribute__((import_module("host"), import_name("mask_write"))) void host_mask_write(int slot, uint32_t offset, const uint8_t* data, uint32_t len);
__attribute__((import_module("host"), import_name("file_browse"))) int host_file_browse(const char* dir, uint8_t* out, int maxEntries);
__attribute__((import_module("host"), import_name("file_list"))) int host_file_list(const char* dir, char* out, int maxNames);
}

constexpr int kSampleSlots = 48;
constexpr uint32_t kSamplePool = 3000000; // frames: about 6 MB, what the Tracker has left for samples with 16 s tracks
int16_t g_samples[kSamplePool];
struct SampleSlot { uint32_t start, length; char name[24]; int8_t tuneSt = 0, tuneCt = 0; uint8_t volume = 100; int8_t pan = 0; bool loop = false; uint16_t pStart = 0, pEnd = 65535; };
constexpr int kScratchSlot = kSampleSlots; // a sample being previewed from the card; it lives in the free tail
SampleSlot g_slot[kSampleSlots + 1];
uint32_t g_samplesUsed = 0;

// Room for a sample on a pad. The old sample's room is reclaimed only if it was the last one loaded.
int16_t* prepareSlot(int slot, uint32_t frames)
{
    if (slot < 0 || slot >= kSampleSlots) return nullptr;
    if (g_slot[slot].length > 0 && g_slot[slot].start + g_slot[slot].length == g_samplesUsed) g_samplesUsed = g_slot[slot].start;
    if (g_samplesUsed + frames > kSamplePool) return nullptr;
    SampleSlot& s = g_slot[slot];
    s.start = g_samplesUsed;
    s.length = frames;
    s.name[0] = 0;
    s.tuneSt = 0; s.tuneCt = 0; s.volume = 100; s.pan = 0; s.loop = false; s.pStart = 0; s.pEnd = 65535; // global constructors are not run
    g_samplesUsed += frames;
    return g_samples + s.start;
}

//------------------------------------------------------------------------------------------
// The machine
class EmuHal : public Hal
{
  public:
    uint32_t rate = 44100;
    uint32_t nowMs = 0;
    bool monitor = true;
    Source source = Source::Line;
    int gains[4] = { 50, 50, 50, 50 };
    int currentSampleSlot = 0;

    TrackBank trackBank;
    Lofi lofiMaster;

    // recording
    bool recActive = false, full = false, captured = false;
    int recTrack = 0;
    uint32_t recLen = 0, startPos = 0;
    // player
    bool playing = false;
    uint32_t loop = 0, pos = 0;
    // input level
    float levelL = 0, levelR = 0;
    // click
    float clickPhase = 0, clickEnv = 0, clickInc = 0;
    // pad voices: a sample, or a plucked tone when the slot is empty
    struct Voice { bool on; int pad; float phase; float inc; float amp; bool held; int slot; float rate; float pan; float endIdx; float startIdx; bool loop; };
    Voice voices[8] = {};

    uint32_t sampleRate() const override { return rate; }
    uint32_t millis() override { return nowMs; }
    int16_t* region(int t) override { return g_mem[t]; }
    uint32_t regionFrames() const override { return rate * kRegionSeconds; }
    void applySource(Source s) override { source = s; }
    void inputLevels(float& l, float& r) override { l = levelL; r = levelR; }
    void recordStart(int t, Source s) override
    {
        source = s;
        recActive = true; recTrack = t; recLen = 0; full = false; captured = false; startPos = 0;
    }
    void recordStop() override { recActive = false; }
    uint32_t recordLength() override { return recLen; }
    bool recordActive() override { return recActive; }
    bool recordFull() override { return full; }
    bool recordStartCaptured() override { return captured; }
    uint32_t recordStartPosition() override { return startPos; }

    void playerSetTrack(int t, const int16_t* d) override { trackBank.data[t] = d; }
    void playerStart(uint32_t f) override { loop = f; trackBank.loop = f; pos = 0; playing = f > 0; }
    void playerStop() override { playing = false; }
    bool playerPlaying() override { return playing; }
    uint32_t playerPosition() override { return pos; }
    uint16_t playerTakePeak(int t) override { const uint16_t p = trackBank.peak[t]; trackBank.peak[t] = 0; return p; }
    TrackBank& bank() override { return trackBank; }
    Lofi& lofi() override { return lofiMaster; }

    int gain(Source s) override { return gains[(int)s]; }
    void setGain(Source s, int p) override { gains[(int)s] = p; }
    void setMonitor(bool on) override { monitor = on; }
    int monitorReductionDb() override { return reduction; }
    int reduction = 0;

    void click(bool accent) override { clickInc = (accent ? 1800.0f : 1200.0f) / (float)rate; clickEnv = 0.5f; clickPhase = 0; }

    bool sdPresent() override { return true; }
    bool fileWrite(const char* path, const void* data, uint32_t len, bool append) override { return host_file_write(path, data, len, append ? 1 : 0) != 0; }
    uint32_t fileRead(const char* path, uint32_t offset, void* buf, uint32_t len) override { return host_file_read(path, offset, buf, len); }
    uint32_t fileSize(const char* path) override { return host_file_size(path); }
    bool fileRemove(const char* path) override { return host_file_remove(path) != 0; }
    int fileList(const char* dir, char (*names)[24], int maxNames) override { return host_file_list(dir, &names[0][0], maxNames); }

    int sampleCount() override { int n = 0; for (int i = 0; i < kSampleSlots; i++) if (g_slot[i].length > 0) n++; return n; }
    bool sampleName(int slot, char* out, int size) override
    {
        if (slot < 0 || slot >= kSampleSlots || g_slot[slot].length == 0) return false;
        int i = 0;
        for (; i < size - 1 && g_slot[slot].name[i] != 0; i++) out[i] = g_slot[slot].name[i];
        out[i] = 0;
        return true;
    }
    int currentSample() override { return currentSampleSlot; }
    void setCurrentSample(int slot) override { currentSampleSlot = slot; }
    void sampleMemory(uint32_t& used, uint32_t& total) override { used = g_samplesUsed * 2; total = kSamplePool * 2; }

    void startVoice(int pad, int smp, float ratio)
    {
        int slot = 0;
        for (int i = 0; i < 8; i++) if (!voices[i].on) { slot = i; break; }
        if (smp >= 0 && smp <= kSampleSlots && g_slot[smp].length > 0)
        {
            const SampleSlot& s = g_slot[smp];
            float r = ratio;
            const int total = s.tuneSt * 100 + s.tuneCt;
            for (int i = 0; i < (total < 0 ? -total : total) / 100; i++) r *= total < 0 ? 0.94387431f : 1.05946309f;
            const float frac = (float)((total < 0 ? -total : total) % 100) / 100.0f * 0.0594631f * (total < 0 ? -0.94f : 1.0f);
            r *= 1.0f + frac;
            const float startIdx = (float)((uint64_t)s.pStart * s.length / 65535);
            float endIdx = (float)((uint64_t)s.pEnd * s.length / 65535);
            if (endIdx <= startIdx + 2) endIdx = (float)s.length - 1;
            voices[slot] = Voice{ true, pad, startIdx, 0.0f, 0.8f * (float)s.volume / 100.0f, true, smp, r, (float)s.pan / 100.0f, endIdx, startIdx, s.loop };
        }
        else
        {
            float f = 110.0f * ratio * 2.0f;
            voices[slot] = Voice{ true, pad, 0.0f, f / (float)rate, 0.3f, true, -1, 1.0f, 0.0f, 0.0f, 0.0f, false };
        }
    }
    int browse(const char* dir, BrowseEntry* out, int max) override
    {
        static uint8_t rec[48 * 40];
        const int n = host_file_browse(dir, rec, max < 48 ? max : 48);
        for (int i = 0; i < n; i++)
        {
            const uint8_t* r = rec + i * 40;
            out[i].dir = r[0] != 0;
            for (int c = 0; c < 28; c++) out[i].name[c] = (char)r[4 + c];
            out[i].name[27] = 0;
            out[i].bytes = r[32] | (r[33] << 8) | (r[34] << 16) | ((uint32_t)r[35] << 24);
            out[i].count = r[36] | (r[37] << 8) | (r[38] << 16) | ((uint32_t)r[39] << 24);
        }
        return n;
    }
    uint32_t rawRead(const char* path, uint32_t offset, void* buf, uint32_t len) override { return host_file_read(path, offset, buf, len); }

    // A WAV file from the card, mixed to mono and brought to this machine's rate. `slot` is a pad
    // slot, or the scratch slot for a preview (which takes only the free tail and keeps nothing).
    bool loadWav(const char* path, int slot)
    {
        uint8_t head[1024];
        const uint32_t hn = host_file_read(path, 0, head, sizeof head);
        WavInfo w;
        if (hn == 0 || !parseWav(head, hn, w) || w.frames() == 0 || w.rate == 0) return false;
        uint32_t out = (uint32_t)((uint64_t)w.frames() * rate / w.rate);
        if (out < 2) return false;
        int16_t* dst;
        if (slot == kScratchSlot)
        {
            if (g_samplesUsed >= kSamplePool) return false;
            if (out > kSamplePool - g_samplesUsed) out = kSamplePool - g_samplesUsed;
            SampleSlot& s = g_slot[slot];
            s.start = g_samplesUsed; s.length = out; s.name[0] = 0;
            s.tuneSt = 0; s.tuneCt = 0; s.volume = 100; s.pan = 0; s.loop = false; s.pStart = 0; s.pEnd = 65535;
            dst = g_samples + s.start;
        }
        else
        {
            dst = prepareSlot(slot, out);
            if (dst == nullptr) return false;
        }
        const uint32_t fb = w.frameBytes(), frames = w.frames();
        static uint8_t chunk[256 * 8 * 3];
        uint32_t first = 0, have = 0;
        auto at = [&](uint32_t idx) -> int
        {
            if (idx >= frames) idx = frames - 1;
            if (idx < first || idx >= first + have)
            {
                first = idx;
                const uint32_t want = frames - idx < 256 ? frames - idx : 256;
                have = host_file_read(path, w.dataOffset + idx * fb, chunk, want * fb) / fb;
                if (have == 0) return 0;
            }
            int sum = 0;
            for (int c = 0; c < w.channels; c++) sum += wavSample16(chunk + (idx - first) * fb, c, w.bits);
            return sum / w.channels;
        };
        for (uint32_t o = 0; o < out; o++)
        {
            const uint64_t num = (uint64_t)o * w.rate;
            const uint32_t i0 = (uint32_t)(num / rate);
            const float fr = (float)(num % rate) / (float)rate;
            const int a = at(i0), b = at(i0 + 1);
            dst[o] = (int16_t)((float)a + (float)(b - a) * fr);
        }
        if (slot != kScratchSlot)
        {
            const char* base = path;
            for (const char* c = path; *c; c++) if (*c == '/') base = c + 1;
            int i = 0;
            for (; i < 23 && base[i] && !(base[i] == '.' && base[i + 1] == 'w'); i++) g_slot[slot].name[i] = base[i];
            g_slot[slot].name[i] = 0;
        }
        return true;
    }
    bool loadSampleFile(const char* path, int slot) override
    {
        previewFile("", false);
        return loadWav(path, slot);
    }
    void previewFile(const char* path, bool on) override
    {
        for (auto& v : voices) if (v.on && v.pad == 950) v.on = false;
        if (!on) return;
        if (!loadWav(path, kScratchSlot)) return;
        startVoice(950, kScratchSlot, 1.0f);
    }
    bool previewActive() override { for (auto& v : voices) if (v.on && v.pad == 950) return true; return false; }

    bool maskDefault() override { return true; }
    void maskWrite(int slot, uint32_t offset, const uint8_t* data, uint32_t len) override { host_mask_write(slot, offset, data, len); }

    // device settings and the firmware update (a pretend update: it counts up and then restarts)
    int devVal[7] = { 3, 0, 2, 3, 0, 80, 2 };
    uint32_t fwStartMs = 0;
    bool fwRunning = false;
    int deviceValue(Dev d) override { return devVal[(int)d]; }
    void setDeviceValue(Dev d, int v) override { devVal[(int)d] = v; }
    void firmwareVersion(char* out, int size) override
    {
        const char* v = "1.0.0 emulator";
        int i = 0;
        for (; i < size - 1 && v[i]; i++) out[i] = v[i];
        out[i] = 0;
    }
    int firmwareFiles(char (*names)[28], int max) override
    {
        const char* f = "tracker-ft6.ptf";
        if (max < 1 || host_file_size("/Firmware/tracker-ft6.ptf") == 0) return 0;
        int i = 0;
        for (; f[i]; i++) names[0][i] = f[i];
        names[0][i] = 0;
        return 1;
    }
    bool firmwareInstall(const char*) override { fwRunning = true; fwStartMs = nowMs; return true; }
    int firmwareProgress() override
    {
        if (!fwRunning) return 0;
        const uint32_t e = nowMs - fwStartMs;
        return e >= 6000 ? 100 : (int)(e / 60);
    }
    void padOn(int pad, bool keysMode) override
    {
        const int semis = keysMode ? (pad % 12) + 12 * ((pad / 12) % 4) - 12 : 0;
        float r = 1.0f;
        if (semis > 0) for (int i = 0; i < semis; i++) r *= 1.05946309f;
        if (semis < 0) for (int i = 0; i < -semis; i++) r /= 1.05946309f;
        startVoice(pad, keysMode ? currentSampleSlot : pad, r);
    }
    bool padSample(int slot, PadSample& out) override
    {
        if (slot < 0 || slot >= kSampleSlots || g_slot[slot].length == 0) return false;
        const SampleSlot& s = g_slot[slot];
        int i = 0;
        for (; i < 23 && s.name[i]; i++) out.name[i] = s.name[i];
        out.name[i] = 0;
        out.frames = s.length; out.bytes = s.length * 2;
        out.tuneSt = s.tuneSt; out.tuneCt = s.tuneCt; out.volume = s.volume; out.pan = s.pan; out.loop = s.loop; out.start = s.pStart; out.end = s.pEnd;
        return true;
    }
    void setPadSample(int slot, const PadSample& in) override
    {
        if (slot < 0 || slot >= kSampleSlots || g_slot[slot].length == 0) return;
        SampleSlot& s = g_slot[slot];
        s.tuneSt = in.tuneSt; s.tuneCt = in.tuneCt; s.volume = in.volume; s.pan = in.pan; s.loop = in.loop; s.pStart = in.start; s.pEnd = in.end;
    }
    int padSampleWave(int slot, uint8_t* out, int columns) override
    {
        if (slot < 0 || slot >= kSampleSlots || g_slot[slot].length == 0) return 0;
        const SampleSlot& s = g_slot[slot];
        for (int c = 0; c < columns; c++)
        {
            const uint32_t from = (uint32_t)((uint64_t)c * s.length / columns), to = (uint32_t)((uint64_t)(c + 1) * s.length / columns);
            int peak = 0;
            for (uint32_t i = from; i < to; i++) { int v = g_samples[s.start + i]; if (v < 0) v = -v; if (v > peak) peak = v; }
            const float n = __builtin_sqrtf((float)peak / 32768.0f);
            out[c] = (uint8_t)(n * 255.0f);
        }
        return columns;
    }
    void padAudition(int slot, bool on) override
    {
        if (on) { startVoice(900 + slot, slot, 1.0f); voices[0].held = true; for (auto& v : voices) if (v.pad == 900 + slot) v.held = true; }
        else for (auto& v : voices) if (v.on && v.pad == 900 + slot) v.held = false;
    }
    void padOff(int pad) override { for (auto& v : voices) if (v.on && v.pad == pad) v.held = false; }
    void padsReleaseAll() override { for (auto& v : voices) v.held = false; }

    static float inputFactor(int percent) { return percent <= 50 ? (float)percent / 50.0f : 1.0f + (float)(percent - 50) / 12.5f; }

    // One block of audio: the same job the firmware's audio interrupt does.
    void audioBlock(int n)
    {
        const float f = source == Source::Pads ? 1.0f : inputFactor(gains[(int)source]) * (source == Source::MicLow ? 2.0f : source == Source::MicHigh ? 4.0f : 1.0f);
        float sl = 0, sr = 0;
        for (int i = 0; i < n; i++)
        {
            g_inL[i] *= f;
            g_inR[i] *= f;
            const float a = g_inL[i] < 0 ? -g_inL[i] : g_inL[i];
            const float b = g_inR[i] < 0 ? -g_inR[i] : g_inR[i];
            if (a > sl) sl = a;
            if (b > sr) sr = b;
        }
        levelL = levelL * 0.9f + sl * 0.1f;
        levelR = levelR * 0.9f + sr * 0.1f;

        // pads
        float pad[kBlock], padL[kBlock], padR[kBlock];
        for (int i = 0; i < n; i++) pad[i] = padL[i] = padR[i] = 0;
        for (auto& v : voices)
        {
            if (!v.on) continue;
            for (int i = 0; i < n; i++)
            {
                if (v.slot >= 0)
                {
                    const SampleSlot& s = g_slot[v.slot];
                    const uint32_t idx = (uint32_t)v.phase;
                    if (idx + 1 >= s.length) { v.on = false; break; }
                    const float fr = v.phase - (float)idx;
                    const float a = g_samples[s.start + idx], b = g_samples[s.start + idx + 1];
                    const float o = (a + (b - a) * fr) / 32768.0f * v.amp;
                    pad[i] += o;
                    padL[i] += o * (v.pan > 0 ? 1.0f - v.pan : 1.0f);
                    padR[i] += o * (v.pan < 0 ? 1.0f + v.pan : 1.0f);
                    v.phase += v.rate;
                    if (v.loop && v.held && v.phase >= v.endIdx) v.phase = v.startIdx;
                    else if (v.phase >= v.endIdx) { v.on = false; break; }
                    if (!v.held && v.amp > 0.0f) v.amp *= 0.9995f;
                }
                else
                {
                    v.phase += v.inc;
                    if (v.phase >= 1.0f) v.phase -= 1.0f;
                    { const float o = (v.phase * 2.0f - 1.0f) * v.amp; pad[i] += o; padL[i] += o; padR[i] += o; }
                    v.amp *= v.held ? 0.99985f : 0.9985f;
                }
            }
            if (v.on && v.slot < 0 && v.amp < 0.002f) v.on = false;
        }

        // recording: what is recorded depends on the source
        if (recActive)
        {
            if (recLen + (uint32_t)n > regionFrames())
            {
                recActive = false;
                full = true;
            }
            else
            {
                if (!captured)
                {
                    captured = true;
                    startPos = pos;
                }
                int16_t* dst = g_mem[recTrack] + recLen;
                for (int k = 0; k < n; k++)
                {
                    float s = (source == Source::Pads) ? pad[k] : (g_inL[k] + g_inR[k]) * 0.5f;
                    if (s > 1.0f) s = 1.0f;
                    if (s < -1.0f) s = -1.0f;
                    dst[k] = (int16_t)(s * 32767.0f);
                }
                recLen += n;
            }
        }

        // playing: the four tracks, then the lo-fi master
        float pl[kBlock], pr[kBlock];
        for (int i = 0; i < n; i++) pl[i] = pr[i] = 0;
        if (playing && loop > 0)
        {
            trackBank.render(pos, n, pl, pr);
            pos = (pos + n) % loop;
        }
        lofiMaster.process(pl, pr, n, rate);

        // monitoring of the live input, with protection against feedback through speakers
        const bool mon = monitor && source != Source::Pads;
        reduction = 0;
        for (int i = 0; i < n; i++)
        {
            float cl = 0;
            if (clickEnv > 0.001f)
            {
                clickPhase += clickInc;
                if (clickPhase >= 1.0f) clickPhase -= 1.0f;
                cl = (clickPhase < 0.5f ? 1.0f : -1.0f) * clickEnv * 0.4f;
                clickEnv *= 0.9985f;
            }
            float ml = mon ? g_inL[i] : 0.0f, mr = mon ? g_inR[i] : 0.0f;
            g_outL[i] = pl[i] + padL[i] + cl + ml;
            g_outR[i] = pr[i] + padR[i] + cl + mr;
        }
    }
};

// Global constructors are not run in this build, so the machine is constructed in ft_init.
alignas(16) char g_halStorage[sizeof(EmuHal)];
EmuHal& g_hal = *reinterpret_cast<EmuHal*>(g_halStorage);
Palette g_pal;
alignas(8) char g_appStorage[sizeof(App)];
App* g_app = nullptr;

void loadPalette()
{
    uint32_t* d = (uint32_t*)&g_pal;
    for (int i = 0; i < 21; i++) d[i] = g_palette[i];
}

// A small built-in kit so the pads have real sounds to play without any files.
void makeKit(uint32_t rate)
{
    const char* names[8] = { "Kick", "Snare", "Hat", "Clap", "Bass", "Pluck", "Stab", "Zap" };
    uint32_t seed = 12345;
    auto noise = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(int32_t)(seed >> 8) / 8388608.0f - 1.0f; };
    for (int k = 0; k < 8; k++)
    {
        const uint32_t len = rate * (k == 2 ? 15 : k == 3 ? 30 : k == 4 ? 45 : k == 5 ? 40 : k == 6 ? 35 : 25) / 100;
        SampleSlot& s = g_slot[k];
        s.start = g_samplesUsed;
        s.length = len;
        s.tuneSt = 0; s.tuneCt = 0; s.volume = 100; s.pan = 0; s.loop = false; s.pStart = 0; s.pEnd = 65535; // global constructors are not run
        for (int i = 0; i < 23 && names[k][i]; i++) { s.name[i] = names[k][i]; s.name[i + 1] = 0; }
        float ph = 0, lp = 0;
        for (uint32_t i = 0; i < len; i++)
        {
            const float t = (float)i / (float)rate;
            const float e = 1.0f - (float)i / (float)len;
            float v = 0;
            switch (k)
            {
                case 0: { const float fr = 45.0f + 110.0f * e * e * e; ph += fr / (float)rate; v = triangle(ph) * e * e; break; }
                case 1: { ph += 190.0f / (float)rate; v = (triangle(ph) * 0.4f + noise() * 0.6f) * e * e; break; }
                case 2: { const float nz = noise(); lp = lp * 0.5f + nz * 0.5f; v = (nz - lp) * e * e * e; break; }
                case 3: { const float burst = ((i / (rate / 100)) % 3 == 2) ? 1.0f : 0.5f; v = noise() * e * e * burst * 0.7f; break; }
                case 4: { ph += 55.0f / (float)rate; v = (triangle(ph) + 0.3f * triangle(ph * 2)) * e * 0.9f; break; }
                case 5: { ph += 330.0f / (float)rate; v = (saw(ph) * 0.5f) * e * e * 0.8f; lp = lp * 0.9f + v * 0.1f; v = lp * 3.0f; break; }
                case 6: { ph += 220.0f / (float)rate; v = (saw(ph) + saw(ph * 1.5f) + saw(ph * 1.25f)) * 0.25f * e * e; break; }
                default: { ph += (1800.0f * e + 100.0f) / (float)rate; v = triangle(ph) * e * 0.6f; break; }
            }
            (void)t;
            if (v > 1) v = 1;
            if (v < -1) v = -1;
            g_samples[s.start + i] = (int16_t)(v * 30000.0f);
        }
        g_samplesUsed += len;
    }
}
} // namespace

extern "C"
{
uint32_t* ft_palette_ptr() { return g_palette; }
uint8_t* ft_adv_ptr(int font) { return g_adv[font]; }
float* ft_in_l_ptr() { return g_inL; }
float* ft_in_r_ptr() { return g_inR; }
float* ft_out_l_ptr() { return g_outL; }
float* ft_out_r_ptr() { return g_outR; }
int32_t* ft_cmd_ptr() { return g_cmd; }
uint8_t* ft_pool_ptr() { return g_pool; }

void ft_init(uint32_t rate)
{
    new (g_halStorage) EmuHal();
    g_hal.rate = rate > kMaxRate ? kMaxRate : rate;
    makeKit(g_hal.rate);
    loadPalette();
    static BufferGfx measure;
    g_app = new (g_appStorage) App(g_hal, g_pal);
    g_app->setMeasure(&measure);
    g_app->enter();
}

void ft_set_palette() { loadPalette(); g_app->setPalette(g_pal); }
void ft_set_monitor(int on) { g_hal.monitor = on != 0; }
void ft_shift(int held) { g_app->setShift(held != 0); }
void ft_key_up(int k) { g_app->keyUp(k); }

// Loading a sample onto a pad: the page asks for room, writes the audio there, then names it.
int16_t* ft_sample_prepare(int slot, uint32_t frames) { return prepareSlot(slot, frames); }
char* ft_sample_name_ptr(int slot) { return g_slot[slot].name; }
void ft_sample_clear(int slot) { if (slot >= 0 && slot < kSampleSlots) g_slot[slot].length = 0; }
int ft_state_screen() { return (int)g_app->screen(); }
int ft_state_waiting() { return g_app->waiting() ? 1 : 0; }
void ft_audio(int n) { g_hal.audioBlock(n); }
void ft_tick(uint32_t ms) { g_hal.nowMs = ms; g_app->tick(); }
void ft_key(int k) { g_app->key(k); }
void ft_jog(int d) { g_app->jog(d); }
void* ft_alloc_scratch(uint32_t n) { static uint8_t buf[65536]; return n <= sizeof buf ? buf : nullptr; }
void ft_pad(int p, int down) { g_app->pad(p, down != 0); }

// Collects everything on the screen, layer by layer in drawing order, and returns the number of
// commands: the main screen (layers 0, 2, 1, 3), or a page (4), then a popup (5) on top.
int ft_frame()
{
    g_cmdCount = 0;
    g_poolUsed = 0;
    BufferGfx gfx;
    static const int order[6] = { 0, 4, 2, 1, 3, 5 };
    for (int i = 0; i < 6; i++)
    {
        const int layer = order[i];
        if (!g_app->layerVisible(layer)) continue;
        for (int s = 0; s < g_app->stagesOf(layer); s++) g_app->draw(layer, s, gfx);
    }
    return g_cmdCount;
}

int ft_pool_used() { return g_poolUsed; }

// Debug: current session numbers for tests.
int ft_state_mode() { return (int)g_app->state().mode(); }
int ft_state_bars() { return g_app->state().bars(); }
uint32_t ft_state_loop() { return g_app->state().loopFrames(); }
}

// The compiler may call these; there is no C library to provide them.
extern "C"
{
void* memset(void* dst, int value, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)value;
    return dst;
}
void* memcpy(void* dst, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
void* memmove(void* dst, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) for (size_t i = 0; i < n; i++) d[i] = s[i];
    else for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    return dst;
}
int memcmp(const void* a, const void* b, size_t n)
{
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}
int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void* __dso_handle = nullptr;
size_t strlen(const char* s) { size_t n = 0; while (s[n] != 0) n++; return n; }
}

