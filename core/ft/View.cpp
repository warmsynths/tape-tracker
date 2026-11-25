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

    void drawTop();
    void drawLane(int index, int y0);
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

void Painter::drawLane(int i, int y0)
{
    const Lane& L = v.lane[i];
    const uint32_t dim = pal.dim;

    rect(0, y0, 164, 84, L.selected ? pal.sel : pal.panel);
    if (L.selected) rect(0, y0, 4, 84, pal.text);
    rect(168, y0, 604, 84, pal.waveBg);
    if (L.active && v.statusKind == 3) rect(170, y0, 2, 84, pal.rec); // armed: waiting for the loop start

    // number, name and mute box
    const uint32_t nameColor = pal.text;
    char num[2] = { (char)('1' + i), 0 };
    T(14, y0 + 9, num, kF17Bold, nameColor);
    T(32, y0 + 9, L.name, kF17Bold, nameColor);
    if (L.muted)
    {
        rect(134, y0 + 9, 20, 20, pal.marker);
        T(138, y0 + 11, "M", kF14, pal.bg);
    }
    else
    {
        frame(134, y0 + 9, 20, 20, dim);
        T(138, y0 + 11, "M", kF14, dim);
    }

    // tags: source, glitch
    int tx = 14;
    if (L.tag[0] != 0)
    {
        const int tagW = textWidth(L.tag, kF12) + 13;
        frame(tx, y0 + 36, tagW, 18, dim);
        T(tx + 7, y0 + 38, L.tag, kF12, pal.text);
        tx += tagW + 4;
    }
    if (L.glitch)
    {
        rect(tx, y0 + 36, textWidth("GLT", kF12) + 11, 18, pal.meter);
        T(tx + 5, y0 + 38, "GLT", kF12, pal.bg);
    }
    if (L.active && v.statusKind == 2) dot(tx + 6, y0 + 45, 5, pal.rec); // recording
    if (L.info[0] != 0) T(14, y0 + 59, L.info, kF12, dim);
    rect(14, y0 + 76, 140, 3, pal.track); // the filled part is in the dynamic layer

    // waveform or message
    if (L.hasPart && L.waveCount > 0)
    {
        uint8_t heights[120];
        for (int c = 0; c < L.waveCount; c++) heights[c] = (uint8_t)(2 + (L.wave[c] * 82) / 255);
        g.color(L.muted ? pal.waveOff : pal.wave);
        g.bars(kWaveX, y0 + 42, 5, 4, heights, L.waveCount); // 120 columns of 5 px
    }
    else if (L.active || L.msg1[0] != 0)
    {
        if (L.active)
        {
            T(184, y0 + 12, L.msg1, kF12, dim);
            if (L.side1[0] != 0) T(368, y0 + 14, L.side1, kF12, L.warn ? pal.warn : dim);
            if (L.progress != 255 && !L.warn) rect(368, y0 + 40, 360, 4, pal.track);
            if (L.side2[0] != 0) T(368, y0 + 52, L.side2, L.warn ? kF17Bold : kF12, L.warn ? pal.warn : dim);
        }
        else T(184, y0 + 33, L.msg1, kF14, pal.text);
    }
    else if (L.msg2[0] != 0)
    {
        T(184, y0 + 33, L.msg2, kF14, dim);
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

void Painter::drawDynamic()
{
    // position readout in the top bar
    T(362, 7, v.pos, kF17Bold, pal.topText);

    // counters of the lanes being recorded or armed
    for (int i = 0; i < 4; i++)
    {
        const int y0 = kLanesY + i * kLaneH;
        if (v.lane[i].active && v.lane[i].msg2[0] != 0) T(184, y0 + 30, v.lane[i].msg2, kF17Bold, pal.text);
        if (v.lane[i].active && !v.lane[i].warn && v.lane[i].progress != 255 && v.lane[i].progress > 0)
            rect(368, y0 + 40, 360 * v.lane[i].progress / 100, 4, pal.text);
    }

    // level bars (red on the lane being recorded)
    for (int i = 0; i < 4; i++)
    {
        const int w = (v.lane[i].meter * 140) / 100;
        if (w <= 0) continue;
        g.color(v.lane[i].active && v.statusKind == 2 ? pal.rec : pal.meter);
        g.rect(14, kLanesY + i * kLaneH + 76, w, 3);
    }

    // overall level on the right edge, growing from the bottom
    if (v.master > 0)
    {
        const int h = 342 * (v.master > 100 ? 100 : v.master) / 100;
        rect(784, 398 - h, 6, h, pal.meter);
    }

    // playhead across all lanes
    if (v.loopFrames > 0)
    {
        const uint32_t pos = v.posFrames % v.loopFrames;
        const int px = kWaveX + (int)(((uint64_t)pos * kWaveW) / v.loopFrames);
        rect(px, 32, 1, 368, pal.text);
        rect(px - 3, 32, 7, 4, pal.text);
    }
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
            else if (stage >= 1 && stage <= 4) p.drawLane(stage - 1, kLanesY + (stage - 1) * kLaneH);
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
