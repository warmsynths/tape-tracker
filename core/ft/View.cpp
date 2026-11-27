#include "View.h"

#include "MiniFmt.h"

namespace ft
{
namespace
{
// Screen geometry (design, section 4.1)
constexpr int kTopBarH = 32;
constexpr int kRulerY = 32;
constexpr int kLanesY = 56;
constexpr int kLaneH = 86;
constexpr int kHeaderW = 164;  // left column of a lane
constexpr int kWaveX = 170;    // waveform area
constexpr int kWaveW = 600;
constexpr int kHintY = 400;
constexpr int kButtonsY = 440;

// Font slots, as in the design: 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular
constexpr int kF14 = 0;
constexpr int kF17 = 1;
constexpr int kF14Bold = 2;
constexpr int kF17Bold = 3;
constexpr int kF12 = 4;
} // namespace

namespace
{
class Painter
{
  public:
    Painter(const View& view, const Palette& palette, Gfx& gfx)
      : v(view)
      , pal(palette)
      , g(gfx)
    {
    }

    void rect(int x, int y, int w, int h, uint32_t color)
    {
        if (w <= 0 || h <= 0) return;
        g.color(color);
        g.rect(x, y, w, h);
    }
    void frame(int x, int y, int w, int h, uint32_t color)
    {
        g.color(color);
        g.line(x, y, x + w - 1, y);
        g.line(x, y + h - 1, x + w - 1, y + h - 1);
        g.line(x, y, x, y + h - 1);
        g.line(x + w - 1, y, x + w - 1, y + h - 1);
    }
    void dot(int x, int y, int radius, uint32_t color)
    {
        g.color(color);
        g.dot(x, y, radius);
    }
    int textWidth(const char* s, int font) { return g.textWidth(s, font); }
    int T(int x, int y, const char* s, int font, uint32_t color);
    int drawText(int x, int y, const char* s, int font, uint32_t color)
    {
        if (s == nullptr || s[0] == 0) return x;
        g.color(color);
        g.text(x, y, s, font);
        return x + g.textWidth(s, font);
    }

    void tint(int x, int y, int w, int h, uint32_t color, int a)
    {
        g.color(color);
        g.alpha(a);
        g.rect(x, y, w, h);
        g.alpha(255);
    }
    // rounded (radius 6) when at least 18 x 18, as the design asks for cards, fields and the M box
    void card(int x, int y, int w, int h, uint32_t color, int a = 255)
    {
        g.color(color);
        if (a != 255) g.alpha(a);
        if (w >= 18 && h >= 18) g.roundRect(x, y, w, h, 6);
        else g.rect(x, y, w, h);
        if (a != 255) g.alpha(255);
    }
    void laneGeom(int i, int& y, int& h) const;
    void columns(const Lane& L, int y, int h, int cols, uint32_t color);
    void drawTop();
    void drawLane(int index);
    void drawHint();
    void drawButtons();
    void drawDynamic();
    void drawPrompt();
    void drawPromptChoices();

    const View& v;
    const Palette& pal;
    Gfx& g;
};

// Text placed by the design's line-box top (see textDy in Gfx.h)
int Painter::T(int x, int y, const char* s, int font, uint32_t color)
{
    if (s == nullptr || s[0] == 0) return x;
    g.color(color);
    g.text(x, y + textDy(font), s, font);
    return x + g.textWidth(s, font);
}

void Painter::drawTop()
{
    const uint32_t dim = pal.dim;

    // ---- Top bar ------------------------------------------------------------------------
    rect(0, 0, 800, kTopBarH, pal.top);
    rect(0, 31, 800, 1, pal.frame);
    int x = T(12, 7, v.project, kF17Bold, pal.topText);
    if (v.unsaved) rect(x + 7, 13, 6, 6, pal.warn);
    static const int seps[4] = { 140, 240, 322, 446 };
    for (int i = 0; i < 4; i++) rect(seps[i], 8, 1, 16, pal.frame);
    T(150, 10, "BPM", kF12, dim);
    T(180, 7, v.tempo, kF17Bold, pal.topText);
    T(250, 10, "BARS", kF12, dim);
    T(286, 7, v.bars, kF17Bold, pal.topText);
    T(332, 10, "POS", kF12, dim); // the value is drawn by the dynamic layer
    T(456, 10, "IN", kF12, dim);
    T(474, 8, v.input, kF14, pal.topText);
    const int mw = textWidth("MON", kF12) + 5;
    if (v.monitor)
    {
        rect(540, 7, mw, 18, pal.topText);
        T(542, 9, "MON", kF12, pal.top);
    }
    else
    {
        const int ow = textWidth("MON OFF", kF12) + 5;
        frame(540, 7, ow, 18, dim);
        T(542, 9, "MON OFF", kF12, dim);
    }
    int cx = 610;
    if (v.filter)
    {
        card(cx, 7, textWidth("FILTER", kF12) + 11, 18, pal.marker);
        T(cx + 5, 9, "FILTER", kF12, pal.bg);
        cx += textWidth("FILTER", kF12) + 17;
    }
    if (v.click)
    {
        rect(cx, 7, textWidth("CLICK", kF12) + 11, 18, pal.meter);
        T(cx + 5, 9, "CLICK", kF12, pal.bg);
    }
    if (v.wait)
    {
        const int wx = v.click ? cx + textWidth("CLICK", kF12) + 17 : cx;
        rect(wx, 7, textWidth("WAIT", kF12) + 11, 18, pal.marker);
        T(wx + 5, 9, "WAIT", kF12, pal.bg);
    }
    if (v.fb[0] != 0)
    {
        const int w = textWidth(v.fb, kF12) + 12;
        rect(716, 7, w, 18, pal.warn);
        T(722, 9, v.fb, kF12, pal.bg);
    }

    // ---- Status row and loop ruler ------------------------------------------------------
    switch (v.statusKind)
    {
        case 1: // playing: a small triangle
            for (int d = -5; d <= 5; d++) rect(14, 43 + d, 12 - (d < 0 ? -d : d) * 2, 1, pal.text);
            break;
        case 2: dot(19, 43, 5, pal.rec); break;
        case 3: dot(19, 43, 5, pal.sel); break; // armed: a dark disc
        default: rect(14, 38, 10, 10, pal.text); break;
    }
    T(32, 36, v.status, kF14, v.statusKind == 2 ? pal.rec : pal.text);
    if (v.statusNote[0] != 0) T(174, 36, v.statusNote, kF12, dim);
    if (v.loopFrames > 0)
    {
        rect(kWaveX, 50, kWaveW, 3, pal.marker);
        const int step = (v.barCount <= 8) ? 1 : 4;
        for (int b = 0; v.barCount > 0 && b < v.barCount; b += step)
        {
            const int bx = kWaveX + (b * kWaveW) / v.barCount;
            rect(bx, 34, 1, 14, dim);
            char n[4];
            n[0] = (char)('0' + ((b + 1) / 10));
            n[1] = (char)('0' + ((b + 1) % 10));
            n[2] = 0;
            T(bx + 4, 34, (b + 1) >= 10 ? n : n + 1, kF12, dim);
        }
    }
    rect(784, 56, 6, 342, pal.track); // lane scroll bar
}

void Painter::drawHint()
{
    rect(0, kHintY, 800, 1, pal.frame);
    T(14, 411, v.hint, kF14, pal.text);
    T(566, 413, "Loop", kF12, pal.dim);
    T(598, 411, v.loopMode, kF14, pal.text);
    T(676, 413, "Pads", kF12, pal.dim);
    frame(708, 410, textWidth(v.padsMode, kF12) + 8, 18, pal.dim);
    T(712, 412, v.padsMode, kF12, pal.text);
}

// Where a lane sits: the overview has four 84 px lanes; in focus one lane opens to 250 px and the
// others become 28 px strips, always in track order
void Painter::laneGeom(int i, int& y, int& h) const
{
    if (v.focusLane < 0) { y = kLanesY + i * kLaneH; h = 84; return; }
    int yy = kLanesY;
    for (int k = 0; k < i; k++) yy += (k == v.focusLane) ? 252 : 30;
    y = yy;
    h = (i == v.focusLane) ? 250 : 28;
}

// The Part's waveform as columns: 60 in the overview, 120 in focus, 30 in a strip (plan A)
void Painter::columns(const Lane& L, int y, int h, int cols, uint32_t color)
{
    uint8_t heights[120];
    const int group = 120 / cols;
    for (int c = 0; c < cols; c++)
    {
        int peak = 0;
        for (int k = 0; k < group; k++) { const int idx = c * group + k; if (idx < L.waveCount && L.wave[idx] > peak) peak = L.wave[idx]; }
        heights[c] = (uint8_t)(2 + (peak * (h - 4)) / 255);
    }
    const int step = 600 / cols;
    g.color(color);
    g.bars(kWaveX, y + h / 2, step, step * 4 / 5, heights, cols);
}

void Painter::drawLane(int i)
{
    const Lane& L = v.lane[i];
    const uint32_t dim = pal.dim;
    const uint32_t tc = pal.t[i];
    int y, h;
    laneGeom(i, y, h);
    const bool focus = v.focusLane == i;
    const bool strip = v.focusLane >= 0 && !focus;

    // header and audio area, each with a faint tint of the track colour; the selected header is stronger
    card(0, y, 164, h, pal.panel);
    card(0, y, 164, h, tc, L.selected ? 56 : 26);
    rect(0, y + 6, L.selected ? 6 : 3, h - 12, tc);
    card(168, y, 604, h, pal.waveBg);
    card(168, y, 604, h, tc, 26);
    if (L.active && v.statusKind == 3) rect(168, y, 3, h, pal.rec); // armed: where recording will begin

    // number, name and mute box
    char num[2] = { (char)('1' + i), 0 };
    const int by = strip ? y + 4 : y + 9;
    if (strip)
    {
        T(14, y + 5, num, kF17Bold, tc);
        T(32, y + 7, L.name, kF14, pal.text);
    }
    else
    {
        T(14, y + 10, num, kF17Bold, tc);
        T(32, y + 10, L.name, kF17Bold, pal.text);
    }
    if (L.muted)
    {
        card(134, by, 20, 20, pal.marker);
        T(138, by + 2, "M", kF14, pal.bg);
    }
    else
    {
        frame(134, by, 20, 20, dim);
        T(138, by + 2, "M", kF14, dim);
    }

    if (!strip)
    {
        // tags: source, glitch
        int tx = 14;
        if (L.tag[0] != 0)
        {
            const int tagW = textWidth(L.tag, kF12) + 13;
            frame(tx, y + 36, tagW, 18, dim);
            T(tx + 7, y + 38, L.tag, kF12, pal.text);
            tx += tagW + 4;
        }
        if (L.glitch)
        {
            rect(tx, y + 36, textWidth("GLT", kF12) + 11, 18, pal.meter);
            T(tx + 5, y + 38, "GLT", kF12, pal.bg);
        }
        if (L.info[0] != 0) T(14, y + 59, L.info, kF12, dim);
        rect(14, y + h - 8, 140, 3, pal.track); // the filled part is in the dynamic layer
    }

    // waveform, or what the lane says
    if (L.hasPart && !L.active && L.waveCount > 0)
    {
        if (L.bitmap)
        {
            g.color(tc);
            g.mask(i * 3 + (strip ? 2 : (focus ? 1 : 0)), kWaveX, y, 600, h);
        }
        else columns(L, y, h, strip ? 30 : (focus ? 120 : 60), tc);
        if (L.muted) tint(168, y, 604, h, 0x000000, 153); // a muted lane is darker still
    }
    else if (focus && L.active)
    {
        if (v.statusKind == 3)
        {
            // armed or waiting: the input meters stand ready
            T(190, y + 41, L.msg1, kF17Bold, pal.text);
            char b[24];
            fmt(b, sizeof b, "Input - %s", v.inputName);
            T(190, y + 119, b, kF12, dim);
            rect(190, y + 136, 560, 10, pal.track);
            rect(190, y + 150, 560, 10, pal.track);
            rect(680, y + 130, 1, 34, dim);
            T(686, y + 169, "-6", kF12, dim);
        }
        else if (v.loopFrames == 0)
        {
            T(184, y + 11, "First take", kF12, dim);
            if (L.side1[0] != 0) T(290, y + 30, L.side1, kF12, L.warn ? pal.warn : dim);
            rect(170, y + 236, 600, 4, pal.track);
        }
    }
    else if (L.active || L.msg1[0] != 0)
    {
        if (L.msg1[0] != 0 && L.msg2[0] != 0)
        {
            T(184, y + 22, L.msg1, kF14, pal.text);
            T(184, y + 46, L.msg2, kF14, dim);
        }
        else if (L.msg1[0] != 0) T(184, y + 33, L.msg1, kF14, pal.text);
    }
    else if (L.msg2[0] != 0)
    {
        T(184, y + (strip ? 8 : 33), L.msg2, strip ? kF12 : kF14, dim);
    }
}

void Painter::drawButtons()
{
    rect(0, kButtonsY, 800, 40, pal.frame);
    for (int b = 0; b < 8; b++)
    {
        const int x0 = b * 100 + 1;
        const bool on = v.btnSelected == b || ((v.btnMask >> b) & 1);
        rect(x0, 441, 97, 39, on ? pal.sel : pal.cell);
        if (on) rect(x0, 441, 97, 2, pal.text);
        const char* label = v.btn[b];
        switch (v.btnKind[b])
        {
            case 1: // Rec: dot and text
                dot(x0 + 27, 460, 5, pal.rec);
                T(x0 + 40, 451, label, kF14, pal.text);
                break;
            case 2: // Stop: square and text
                rect(x0 + 22, 455, 10, 10, pal.text);
                T(x0 + 40, 451, label, kF14, pal.text);
                break;
            case 3: // Play: triangle and text
                for (int d = -8; d <= 8; d++) rect(x0 + 19, 460 + d, 16 - 2 * (d < 0 ? -d : d), 1, pal.text);
                T(x0 + 40, 451, label, kF14, pal.text);
                break;
            default: T(x0 + (97 - textWidth(label, kF14)) / 2, 451, label, kF14, pal.text); break;
        }
    }
}

namespace
{
int levelX(int db, int width)
{
    const int v = (db + 48) * width / 48;
    return v < 0 ? 0 : (v > width ? width : v);
}
} // namespace

void Painter::drawDynamic()
{
    // position readout in the top bar
    T(362, 7, v.pos, kF17Bold, pal.topText);

    for (int i = 0; i < 4; i++)
    {
        const Lane& L = v.lane[i];
        int y, h;
        laneGeom(i, y, h);
        const bool focus = v.focusLane == i;
        if (focus && L.active)
        {
            if (v.statusKind == 3)
            {
                // armed: the headline's second line and the live input meters
                if (L.msg2[0] != 0) T(190, y + 67, L.msg2, kF14, pal.text);
                rect(190, y + 136, levelX(v.inL, 560), 10, pal.meter);
                rect(190, y + 150, levelX(v.inR, 560), 10, pal.meter);
                if (v.waitDb != 0)
                {
                    // the line the input has to reach to start the take, with its level written over it
                    const int bx = 190 + 560 * (v.waitDb + 48) / 48;
                    rect(bx - 1, y + 110, 3, 58, pal.marker);
                    char lv[24];
                    fmt(lv, sizeof lv, "START %d dB", (int)v.waitDb);
                    const int tw = textWidth(lv, kF14);
                    T(bx + tw + 14 > 750 ? bx - tw - 8 : bx + 8, y + 108, lv, kF14, pal.marker);
                    char now[24];
                    fmt(now, sizeof now, "input %d dB", (int)v.waitNow);
                    T(750 - textWidth(now, kF14), y + 108, now, kF14, pal.text);
                }
            }
            else if (v.loopFrames == 0)
            {
                T(184, y + 27, L.msg2, kF17Bold, pal.text);
                if (L.side2[0] != 0) T(766 - textWidth(L.side2, kF14), y + 27, L.side2, kF14, L.warn ? pal.warn : pal.dim);
                if (L.progress != 255) rect(170, y + 236, 600 * L.progress / 100, 4, pal.text);
            }
            else
            {
                char b[48];
                fmt(b, sizeof b, "Recording one Loop - %s", L.msg2);
                T(184, y + 11, b, kF14, pal.text);
            }
            if (v.statusKind == 2 && L.liveBitmap)
            {
                g.color(pal.rec);
                g.mask(i * 3 + 1, kWaveX, y, 600, h);
            }
            else if (v.statusKind == 2 && L.liveCount > 0)
            {
                uint8_t heights[120];
                for (int c = 0; c < L.liveCount; c++) heights[c] = (uint8_t)(2 + (L.live[c] * 118) / 255);
                g.color(pal.rec);
                g.bars(kWaveX, y + 134, 5, 4, heights, L.liveCount);
            }
            // the circle by the source tag: red while recording, blinking while armed
            const int tw = L.tag[0] != 0 ? textWidth(L.tag, kF12) + 13 + 4 : 0;
            dot(14 + tw + 5, y + 45, 5, (v.statusKind == 2 || v.blink) ? pal.rec : pal.track);
        }
        else if (L.active && L.msg2[0] != 0) T(184, y + 30, L.msg2, kF17Bold, pal.text);
    }

    // level bars in the track colour (red on the lane being recorded)
    for (int i = 0; i < 4; i++)
    {
        int y, h;
        laneGeom(i, y, h);
        if (v.focusLane >= 0 && v.focusLane != i) continue;
        const int w = (v.lane[i].meter * 140) / 100;
        if (w <= 0) continue;
        g.color(v.lane[i].active && v.statusKind == 2 ? pal.rec : pal.t[i]);
        g.rect(14, y + h - 8, w, 3);
    }

    // overall level on the right edge, growing from the bottom
    if (v.master > 0)
    {
        const int h = 342 * (v.master > 100 ? 100 : v.master) / 100;
        rect(784, 398 - h, 6, h, pal.meter);
    }

    if (v.loopFrames == 0) return;
    const uint32_t pos = v.posFrames % v.loopFrames;
    const int px = kWaveX + (int)(((uint64_t)pos * kWaveW) / v.loopFrames);

    // the part of each lane the playhead has not reached yet is darker: one see-through rectangle each
    g.color(0x000000);
    g.alpha(148);
    for (int i = 0; i < 4; i++)
    {
        if (!v.lane[i].hasPart || v.lane[i].active) continue;
        int y, h;
        laneGeom(i, y, h);
        if (px < 770) g.rect(px, y, 770 - px, h);
    }
    g.alpha(255);

    // grid over all four lanes: bar lines, fainter beat lines, fainter still sixteenths
    const int bars = v.barCount;
    if (v.gridMode > 0 && bars > 0)
    {
        const int beats = bars * 4;
        g.color(pal.text);
        g.alpha(56);
        for (int b = 1; b < bars; b++) g.rect(kWaveX + (1200 * b + bars) / (2 * bars), kLanesY, 1, 342);
        if (v.gridMode >= 2 && beats <= 48)
        {
            g.alpha(20);
            for (int b = 1; b < beats; b++) if (b % 4 != 0) g.rect(kWaveX + (1200 * b + beats) / (2 * beats), kLanesY, 1, 342);
            if (v.gridMode >= 3 && bars * 16 <= 32)
            {
                const int steps = bars * 16;
                g.alpha(12);
                for (int b = 1; b < steps; b++) if (b % 4 != 0) g.rect(kWaveX + (1200 * b + steps) / (2 * steps), kLanesY, 1, 342);
            }
        }
        g.alpha(255);
    }

    // the bar the playhead is in, in the bright colour on the ruler
    if (bars > 0)
    {
        const int step = (bars <= 8) ? 1 : 4;
        const int cur = (int)(((uint64_t)pos * bars) / v.loopFrames);
        const int shown = cur - (cur % step);
        const int bx = kWaveX + (shown * kWaveW) / bars;
        char n[4];
        n[0] = (char)('0' + ((shown + 1) / 10));
        n[1] = (char)('0' + ((shown + 1) % 10));
        n[2] = 0;
        T(bx + 4, 34, (shown + 1) >= 10 ? n : n + 1, kF12, pal.text);
    }

    // playhead across all lanes
    rect(px, 32, 1, 368, pal.text);
    rect(px - 3, 32, 7, 4, pal.text);
}

//--------------------------------------------------------------------------------------------
// "How many bars?" (design 4.3): a panel over the lower part of the screen. Up to five choices
// get big cells; more get compact cells in up to two rows and the panel moves up.
namespace
{
constexpr int kCellsPerRow = 6;
} // namespace

void Painter::drawPrompt()
{
    const Prompt& P = v.prompt;
    const bool big = P.count <= 5;
    const int top = big ? 160 : 120;

    rect(0, top, 800, big ? 280 : 280, pal.panel);
    rect(0, top, 800, 1, pal.frame);
    rect(0, top + 1, 800, 2, pal.text);

    int x = T(14, top + 14, "How many bars?", kF17Bold, pal.text);
    if (P.memoryFull)
    {
        rect(x + 20, top + 15, textWidth("MEMORY FULL", kF12) + 11, 18, pal.warn);
        x = T(x + 25, top + 17, "MEMORY FULL", kF12, pal.bg);
        T(x + 20, top + 15, P.note, kF14, pal.warn);
    }
    T(14, top + 42, P.played, kF14, pal.text);

    // result at the right
    const int ry = top + (big ? 76 : 76);
    const uint32_t bpm = P.bpm10[P.selected];
    char big_[16];
    fmt(big_, sizeof(big_), "%u.%u BPM", (unsigned)(bpm / 10), (unsigned)(bpm % 10));
    T(616, ry, "Result", kF12, pal.dim);
    T(616, ry + 18, big_, kF17Bold, pal.text);
    char line[32];
    fmt(line, sizeof(line), "%u bars of 4/4", (unsigned)P.bars[P.selected]);
    T(616, ry + 44, line, kF12, pal.dim);
    char lm[24];
    fmt(lm, sizeof(lm), "Loop Mode: %s", v.loopMode);
    T(616, ry + 62, lm, kF12, pal.dim);
    T(616, ry + 80, P.suggestion, kF12, pal.dim);

    T(14, top + 252, "Jog or Left/Right chooses.  B8 OK sets the Loop and tempo.  B7 Cancel discards the take.", kF12, pal.dim);

    // button row: only Cancel and OK do anything
    rect(0, kButtonsY, 800, 40, pal.frame);
    for (int b = 0; b < 8; b++) rect(b * 100 + 1, 441, 97, 39, pal.cell);
    T(601 + (97 - textWidth("Cancel", kF14)) / 2, 451, "Cancel", kF14, pal.text);
    T(701 + (97 - textWidth("OK", kF14)) / 2, 451, "OK", kF14, pal.text);
}

void Painter::drawPromptChoices()
{
    const Prompt& P = v.prompt;
    const bool big = P.count <= 5;
    const int top = big ? 160 : 120;
    const int cellH = big ? 100 : 70;

    for (int i = 0; i < P.count; i++)
    {
        const int x0 = 1 + (i % kCellsPerRow) * 100;
        const int y0 = top + 72 + (i / kCellsPerRow) * (big ? 100 : 76);
        const bool selected = (i == P.selected);
        rect(x0, y0, 97, cellH, selected ? pal.sel : pal.cell);
        if (selected) rect(x0, y0, 97, 2, pal.text);
        char n[8];
        fmt(n, sizeof(n), "%u", (unsigned)P.bars[i]);
        const int x = T(x0 + 12, y0 + 10, n, kF17Bold, pal.text);
        T(x + 8, y0 + 13, "bars", kF12, pal.dim);
        char t[12];
        fmt(t, sizeof(t), "%u.%u", (unsigned)(P.bpm10[i] / 10), (unsigned)(P.bpm10[i] % 10));
        T(x0 + 12, y0 + (big ? 50 : 38), t, kF17, pal.text);
        if (big) T(x0 + 12, y0 + 74, "BPM", kF12, pal.dim);
    }
}

} // namespace

int layerStages(int layer)
{
    switch (layer)
    {
        case 0: return 5;
        case 2:
        case 3: return 2;
        default: return 1;
    }
}

void drawLayer(int layer, int stage, const View& view, const Palette& palette, Gfx& gfx)
{
    Painter p(view, palette, gfx);
    switch (layer)
    {
        case 0:
            if (stage == 0) p.drawTop();
            else if (stage >= 1 && stage <= 4) p.drawLane(stage - 1);
            break;
        case 1:
            p.drawDynamic();
            break;
        case 2:
            if (stage == 0) p.drawHint();
            else p.drawButtons();
            break;
        case 3:
            if (stage == 0) p.drawPrompt();
            else p.drawPromptChoices();
            break;
        default: break;
    }
}
} // namespace ft
