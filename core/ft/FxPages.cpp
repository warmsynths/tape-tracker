// FX1: reverb and delay sends (C6). FX2: the live filter (C7).
#include "App.h"
#include "Pen.h"
#include "MiniFmt.h"

namespace ft
{
namespace
{
int clampv(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

void App::fxPush()
{
    SendFxCfg& c = hal.bank().fx.cfg;
    const uint32_t rate = hal.sampleRate() ? hal.sampleRate() : 44100;
    uint32_t beat;
    if (session.loopFrames() > 0 && session.bars() > 0) beat = session.loopFrames() / ((uint32_t)session.bars() * 4);
    else beat = (uint32_t)((uint64_t)rate * 600 / (set.tempoTenths > 0 ? set.tempoTenths : 1200));
    const uint32_t f = (uint32_t)((float)beat * delayDivBeats(c.delayDiv));
    c.delayFrames = f < 1 ? 1 : (f >= (uint32_t)kDelayMax ? (uint32_t)kDelayMax - 1 : f);
}

//--------------------------------------------------------------------------------------------
// C6 Reverb and delay sends

void App::fxKey(int k)
{
    SendFxCfg& c = hal.bank().fx.cfg;
    const int settings = fxBlock == 0 ? 3 : 4;
    switch (k)
    {
        case kB1:
            c.reverbOn = !c.reverbOn;
            if (c.reverbOn) hal.bank().fx.init(hal.fxMemory());
            break;
        case kB2:
            c.delayOn = !c.delayOn;
            if (c.delayOn) hal.bank().fx.init(hal.fxMemory());
            break;
        case kLeft: fxBlock = 0; fxRow = clampv(fxRow, 0, 3 + 3); break;
        case kRight: fxBlock = 1; break;
        case kUp: fxRow = clampv(fxRow - 1, 0, settings + 3); break;
        case kDown: fxRow = clampv(fxRow + 1, 0, settings + 3); break;
        case kB8: case kFx1: back(); return;
        default: return;
    }
    unsaved = true;
    fxPush();
    refresh(true);
}

void App::fxJog(int delta)
{
    SendFxCfg& c = hal.bank().fx.cfg;
    const int d = delta > 0 ? 1 : -1;
    const int step = shift ? 1 : 5;
    const int settings = fxBlock == 0 ? 3 : 4;
    if (fxRow >= settings)
    {
        TrackCfg& t = hal.bank().cfg[fxRow - settings];
        uint8_t& v = fxBlock == 0 ? t.sendReverb : t.sendDelay;
        v = (uint8_t)clampv((int)v + d * step, 0, 100);
    }
    else if (fxBlock == 0)
    {
        uint8_t* p = fxRow == 0 ? &c.reverbSize : fxRow == 1 ? &c.reverbDamp : &c.reverbReturn;
        *p = (uint8_t)clampv((int)*p + d * step, 0, 100);
    }
    else if (fxRow == 0) c.delayDiv = (uint8_t)clampv((int)c.delayDiv + d, 0, kDelayDivs - 1);
    else
    {
        uint8_t* p = fxRow == 1 ? &c.delayFeedback : fxRow == 2 ? &c.delayTone : &c.delayReturn;
        *p = (uint8_t)clampv((int)*p + d * step, 0, fxRow == 1 ? 95 : 100);
    }
    unsaved = true;
    fxPush();
    refresh(true);
}

void App::drawFx(Pen& p)
{
    p.topBar(model);
    const SendFxCfg& c = hal.bank().fx.cfg;
    for (int b = 0; b < 2; b++)
    {
        const bool on = b == 0 ? c.reverbOn : c.delayOn;
        const int x = 14 + b * 392, w = 380;
        const uint32_t ink = on ? pal.text : pal.dim;
        p.text(x + 4, 42, b == 0 ? "Reverb" : "Delay", F17B, ink);
        if (on) { p.card(x + 100, 44, 34, 20, pal.meter); p.text(x + 109, 47, "ON", F12, pal.bg); }
        else { p.frame(x + 100, 44, 34, 20, pal.dim); p.text(x + 107, 47, "OFF", F12, pal.dim); }
        const int settings = b == 0 ? 3 : 4;
        for (int r = 0; r < settings; r++)
        {
            const int y = 72 + r * 42;
            const bool focus = fxBlock == b && fxRow == r;
            p.cell(x, y, w, 38, focus, pal.panel);
            static const char* const rn[2][4] = { { "Size", "Damp", "Return", "" }, { "Time", "Feedback", "Tone", "Return" } };
            p.text(x + 12, y + 6, rn[b][r], F14, ink);
            char v[32];
            int frac = 0;
            if (b == 0)
            {
                frac = r == 0 ? c.reverbSize : r == 1 ? c.reverbDamp : c.reverbReturn;
                fmt(v, sizeof v, "%d%%", frac);
            }
            else if (r == 0)
            {
                const int ms = (int)((uint64_t)c.delayFrames * 1000 / (hal.sampleRate() ? hal.sampleRate() : 44100));
                fmt(v, sizeof v, "%s  %d ms", delayDivName(c.delayDiv), ms);
                frac = c.delayDiv * 100 / (kDelayDivs - 1);
            }
            else
            {
                frac = r == 1 ? c.delayFeedback : r == 2 ? c.delayTone : c.delayReturn;
                fmt(v, sizeof v, "%d%%", frac);
            }
            p.textRight(x + w - 12, y + 4, v, F17B, ink);
            p.rect(x + 12, y + 31, w - 24, 3, pal.track);
            p.rect(x + 12, y + 31, (w - 24) * frac / 100, 3, on ? pal.text : pal.waveOff);
        }
        p.text(x + 4, 252, "Sends", F12, pal.dim);
        for (int t = 0; t < 4; t++)
        {
            const int y = 270 + t * 30;
            const bool focus = fxBlock == b && fxRow == settings + t;
            const int v = b == 0 ? hal.bank().cfg[t].sendReverb : hal.bank().cfg[t].sendDelay;
            p.cell(x, y, w, 27, focus, pal.panel);
            char n[16];
            fmt(n, sizeof n, "Track %d", t + 1);
            p.text(x + 12, y + 5, n, F14, ink);
            char vv[8];
            fmt(vv, sizeof vv, "%d%%", v);
            p.textRight(x + w - 12, y + 5, vv, F14B, ink);
            p.rect(x + 100, y + 21, 200, 3, pal.track);
            p.rect(x + 100, y + 21, 2 * v, 3, on ? pal.text : pal.waveOff);
        }
    }
    p.text(14, 412, "Left and Right: Reverb or Delay. Up and Down: the settings, then each track's send. Jog changes it.", F14, pal.text);
    const char* l[8] = { "Reverb", "Delay", "", "", "", "", "", "Back" };
    p.buttons(l, (uint8_t)((c.reverbOn ? 1 : 0) | (c.delayOn ? 2 : 0)));
}

//--------------------------------------------------------------------------------------------
// C7 Live filter

void App::filterKey(int k)
{
    LiveFilterCfg& f = liveFilter();
    switch (k)
    {
        case kB1: f.held = true; break;
        case kB2: f.latched = !f.latched; break;
        case kB3: f.latched = false; f.held = false; f.set = 50; break;
        case kB4: f.mode = 0; break;
        case kB5: f.mode = 1; break;
        case kB6: f.mode = 2; break;
        case kB7: f.mode = 3; break;
        case kB8: case kFx2: f.held = false; back(); return; // a latched filter stays in
        default: return;
    }
    refresh(true);
}

void App::filterJog(int delta)
{
    LiveFilterCfg& f = liveFilter();
    const int step = shift ? 1 : 4;
    f.set = (uint8_t)clampv((int)f.set + (delta > 0 ? step : -step), 0, 100);
    refresh(true);
}

static void hzText(char* out, int size, float hz)
{
    const int h = (int)(hz + 0.5f);
    if (h >= 1000) fmt(out, size, "%d.%d kHz", h / 1000, (h % 1000) / 100);
    else fmt(out, size, "%d Hz", h);
}

void App::drawFilter(Pen& p)
{
    p.topBar(model);
    LiveFilterCfg& f = liveFilter();
    p.text(14, 42, "Live filter", F17B, pal.text);
    p.text(14, 70, "Sweeps the whole mix. Hold it for a moment, or latch it and turn the jog.", F14, pal.dim);
    static const char* const modeName[4] = { "LP-HP", "LP", "HP", "Band" };
    static const char* const modeHelp[4] = { "Left of the middle closes a low-pass, right of it raises a high-pass.", "A low-pass: the highs go as you turn it down.", "A high-pass: the lows go as you turn it up.", "Only a band of the sound passes; it moves with the jog." };
    bool low = true, open = false;
    const float hz = liveFilterCutoff(f, &low, &open);
    char setText[32], nowText[32], hzs[16];
    hzText(hzs, sizeof hzs, hz);
    if (open) fmt(setText, sizeof setText, "Open");
    else if (f.mode == 3) fmt(setText, sizeof setText, "Band at %s", hzs);
    else fmt(setText, sizeof setText, "%s at %s", low ? "Low-pass" : "High-pass", hzs);
    if (f.active() && !open) fmt(nowText, sizeof nowText, "%s", setText);
    else fmt(nowText, sizeof nowText, "Open");
    p.text(40, 108, "NOW", F12, pal.dim);
    p.text(40, 124, nowText, F17B, f.active() ? pal.text : pal.dim);
    p.text(420, 108, "SET", F12, pal.dim);
    p.text(420, 124, setText, F17B, pal.text);
    // the bar: the whole sweep, with the middle marked in LP-HP, and a marker where Set is
    const int bx = 40, bw = 720, by = 184, bh = 56;
    p.card(bx, by, bw, bh, pal.panel);
    const uint32_t fillCol = f.active() ? pal.marker : pal.waveOff;
    if (f.mode == 0)
    {
        if (f.set < 50) p.rect(bx + bw * f.set / 100, by + 4, bw * (50 - f.set) / 100, bh - 8, fillCol);
        else p.rect(bx + bw / 2, by + 4, bw * (f.set - 50) / 100, bh - 8, fillCol);
        p.rect(bx + bw / 2, by, 1, bh, pal.frame);
    }
    else if (f.mode == 1) p.rect(bx + bw * f.set / 100, by + 4, bw * (100 - f.set) / 100 - 2, bh - 8, fillCol);
    else if (f.mode == 2) p.rect(bx + 4, by + 4, bw * f.set / 100, bh - 8, fillCol);
    else p.rect(bx + bw * f.set / 100 - 20, by + 4, 40, bh - 8, fillCol);
    p.rect(bx + bw * f.set / 100 - 1, by - 8, 3, bh + 16, pal.text);
    p.text(bx, by + bh + 14, f.mode == 0 ? "Low-pass" : "Closed", F12, pal.dim);
    p.textRight(bx + bw, by + bh + 14, f.mode == 0 ? "High-pass" : "Open", F12, pal.dim);
    p.text(40, 300, modeHelp[f.mode < 4 ? f.mode : 0], F14, pal.dim);
    p.text(14, 412, f.latched ? "Latched: it stays in until B2 again or B3. Jog moves the sweep." : "Hold B1 to filter. B2 latches it. Jog moves the sweep, held or not.", F14, pal.text);
    const char* l[8] = { "Hold", "Latch", "Reset", modeName[0], modeName[1], modeName[2], modeName[3], "Back" };
    p.buttons(l, (uint8_t)((f.held ? 1 : 0) | (f.latched ? 2 : 0) | (1u << (3 + (f.mode < 4 ? f.mode : 0)))));
}
} // namespace ft
