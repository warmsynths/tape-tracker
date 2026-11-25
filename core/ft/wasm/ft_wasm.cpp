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
constexpr uint32_t kRegionSeconds = 26;
constexpr int kBlock = 128;

int16_t g_mem[4][kMaxRate * kRegionSeconds];

float g_inL[kBlock], g_inR[kBlock], g_outL[kBlock], g_outR[kBlock];
uint32_t g_palette[12];
uint8_t g_adv[5][128];

//------------------------------------------------------------------------------------------
// Commands for the canvas: 7 ints each.
enum Op { kColor = 1, kRect = 2, kLine = 3, kDot = 4, kText = 5, kBars = 6 };
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
// The machine
class EmuHal : public Hal
{
  public:
    uint32_t rate = 44100;
    uint32_t nowMs = 0;
    bool monitor = true;
    Source source = Source::Line;

    // recording
    bool recActive = false, full = false, captured = false;
    int recTrack = 0;
    uint32_t recLen = 0, startPos = 0;
    // player
    bool playing = false;
    uint32_t loop = 0, pos = 0;
    const int16_t* data[4] = {};
    volatile uint16_t peaks[4] = {};
    // input level
    float levelL = 0, levelR = 0;
    // pad voices
    struct Voice { bool on; int pad; float phase; float inc; float amp; bool held; };
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
    void playerSetTrack(int t, const int16_t* d) override { data[t] = d; }
    void playerStart(uint32_t f) override { loop = f; pos = 0; playing = f > 0; }
    void playerStop() override { playing = false; }
    bool playerPlaying() override { return playing; }
    uint32_t playerPosition() override { return pos; }
    uint16_t playerTakePeak(int t) override { const uint16_t p = peaks[t]; peaks[t] = 0; return p; }

    void padOn(int pad, bool keysMode) override
    {
        // Until real samples are loaded, a pad plays a plucked tone: in Keys mode the pitch
        // follows the pad like a keyboard, in Pad mode each pad has its own fixed tone.
        int slot = 0;
        for (int i = 0; i < 8; i++) if (!voices[i].on) { slot = i; break; }
        const int semis = keysMode ? (pad % 12) + 12 * ((pad / 12) % 4) : (pad * 5) % 24;
        float f = 110.0f;
        for (int i = 0; i < semis; i++) f *= 1.05946309f;
        voices[slot] = Voice{ true, pad, 0.0f, f / (float)rate, 0.35f, true };
    }
    void padOff(int pad) override
    {
        for (auto& v : voices) if (v.on && v.pad == pad) v.held = false;
    }
    void padsReleaseAll() override { for (auto& v : voices) v.held = false; }

    // One block of audio: the same job the firmware's audio interrupt does.
    void audioBlock(int n)
    {
        // input level (smoothed amplitude)
        float sl = 0, sr = 0;
        for (int i = 0; i < n; i++)
        {
            const float a = g_inL[i] < 0 ? -g_inL[i] : g_inL[i];
            const float b = g_inR[i] < 0 ? -g_inR[i] : g_inR[i];
            if (a > sl) sl = a;
            if (b > sr) sr = b;
        }
        levelL = levelL * 0.9f + sl * 0.1f;
        levelR = levelR * 0.9f + sr * 0.1f;

        // pads
        float pad[kBlock];
        for (int i = 0; i < n; i++) pad[i] = 0;
        for (auto& v : voices)
        {
            if (!v.on) continue;
            for (int i = 0; i < n; i++)
            {
                v.phase += v.inc;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                const float saw = v.phase * 2.0f - 1.0f;
                pad[i] += saw * v.amp;
                v.amp *= v.held ? 0.99985f : 0.9985f;
            }
            if (v.amp < 0.002f) v.on = false;
        }

        // recording: what is recorded depends on the source
        if (recActive)
        {
            for (int i = 0; i < n; i += kBlock)
            {
                if (recLen + kBlock > regionFrames())
                {
                    recActive = false;
                    full = true;
                    break;
                }
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
                break;
            }
        }

        // playing: the four tracks mixed at one shared position
        int16_t mix[kBlock];
        for (int i = 0; i < n; i++) mix[i] = 0;
        if (playing && loop > 0)
        {
            const int16_t* active[4];
            for (int t = 0; t < 4; t++)
            {
                active[t] = data[t];
                const uint16_t p = loopPeakBlock(data[t], loop, pos, n);
                if (p > peaks[t]) peaks[t] = p;
            }
            loopMixBlock(active, 4, loop, pos, mix, n);
            pos = (pos + n) % loop;
        }

        for (int i = 0; i < n; i++)
        {
            float o = (float)mix[i] / 32768.0f + pad[i];
            if (monitor && source != Source::Pads) o += (g_inL[i] + g_inR[i]) * 0.5f;
            g_outL[i] = o;
            g_outR[i] = o;
        }
    }
};

EmuHal g_hal;
Palette g_pal;
alignas(8) char g_appStorage[sizeof(App)];
App* g_app = nullptr;

void loadPalette()
{
    g_pal.background = g_palette[0]; g_pal.text = g_palette[1]; g_pal.bright = g_palette[2];
    g_pal.dim = g_palette[3]; g_pal.panel = g_palette[4]; g_pal.hairline = g_palette[5];
    g_pal.selected = g_palette[6]; g_pal.meterBack = g_palette[7]; g_pal.wave = g_palette[8];
    g_pal.record = g_palette[9]; g_pal.marker = g_palette[10]; g_pal.good = g_palette[11];
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
    g_hal.rate = rate > kMaxRate ? kMaxRate : rate;
    loadPalette();
    g_app = new (g_appStorage) App(g_hal, g_pal);
    g_app->enter();
}

void ft_set_palette() { loadPalette(); g_app->setPalette(g_pal); }
void ft_set_monitor(int on) { g_hal.monitor = on != 0; }
void ft_audio(int n) { g_hal.audioBlock(n); }
void ft_tick(uint32_t ms) { g_hal.nowMs = ms; g_app->tick(); }
void ft_key(int k) { g_app->key(k); }
void ft_jog(int d) { g_app->jog(d); }
void ft_pad(int p, int down) { g_app->pad(p, down != 0); }

// Collects everything on the screen, layer by layer in drawing order, and returns the number of
// commands. Layer 0 (top bar and lanes), 2 (hint and buttons), 1 (playhead and levels), then 3
// (the question) on top.
int ft_frame()
{
    g_cmdCount = 0;
    g_poolUsed = 0;
    BufferGfx gfx;
    static const int order[4] = { 0, 2, 1, 3 };
    for (int i = 0; i < 4; i++)
    {
        const int layer = order[i];
        if (!g_app->layerVisible(layer)) continue;
        for (int s = 0; s < layerStages(layer); s++) g_app->draw(layer, s, gfx);
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

