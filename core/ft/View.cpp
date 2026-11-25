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

void Painter::drawTop()
{
    const uint32_t text = pal.text;
    const uint32_t bright = pal.text;
    const uint32_t dim = pal.dim;
    const uint32_t panel = pal.panel;
    const uint32_t hair = pal.frame;

    // ---- Top bar ------------------------------------------------------------------------
    rect(0, 0, 800, kTopBarH, pal.top);
    int x = drawText(12, 8, v.project, kF17Bold, pal.topText);
    if (v.unsaved)
    {
        rect(x + 6, 12, 6, 6, pal.marker);
    }
    x = 150;
    x = drawText(x, 12, "BPM", kF12, dim);
    x = drawText(x + 6, 8, v.tempo, kF17Bold, pal.topText);
    x = drawText(x + 18, 12, "BARS", kF12, dim);
    x = drawText(x + 6, 8, v.bars, kF17Bold, pal.topText);
    drawText(330, 12, "POS", kF12, dim); // the value is drawn by the dynamic layer
    x = drawText(456, 12, "IN", kF12, dim);
    x = drawText(x + 6, 9, v.input, kF14, text);
    if (v.monitor)
    {
        rect(x + 10, 7, 36, 18, pal.topText);
        drawText(x + 14, 11, "MON", kF12, pal.bg);
    }
    else
    {
        frame(x + 10, 7, 66, 18, dim);
        drawText(x + 14, 11, "MON OFF", kF12, dim);
    }

    if (v.fb[0] != 0)
    {
        const int w = textWidth(v.fb, kF12) + 12;
        rect(716, 7, w, 18, pal.warn);
        drawText(722, 9, v.fb, kF12, pal.bg);
    }

    // ---- Status row and loop ruler ------------------------------------------------------
    switch (v.statusKind)
    {
        case 1: drawText(14, 36, "\x1a\x1b", kF17Bold, bright); break;                 // play glyph
        case 2: dot(20, 44, 5, pal.rec); break;                                        // recording
        case 3: frame(15, 39, 10, 10, dim); break;                                    // armed
        default: rect(15, 39, 10, 10, bright); break;                                 // stopped
    }
    drawText(32, 37, v.status, kF14, v.statusKind == 2 ? pal.rec : bright);
    if (v.statusNote[0] != 0)
    {
        drawText(kWaveX + 14, 40, v.statusNote, kF12, dim);
    }
    if (v.loopFrames > 0)
    {
        rect(kWaveX, 48, kWaveW, 3, pal.marker);
        const int step = (v.barCount <= 8) ? 1 : 4;
        for (int b = 0; v.barCount > 0 && b < v.barCount; b += step)
        {
            const int bx = kWaveX + (b * kWaveW) / v.barCount;
            rect(bx, 38, 1, 12, dim);
            char n[4];
            n[0] = (char)('0' + ((b + 1) / 10));
            n[1] = (char)('0' + ((b + 1) % 10));
            n[2] = 0;
            drawText(bx + 4, 36, (b + 1) >= 10 ? n : n + 1, kF12, dim);
        }
    }

}

void Painter::drawHint()
{
    const uint32_t text = pal.text;
    const uint32_t dim = pal.dim;
    const uint32_t hair = pal.frame;

    rect(0, kHintY, 800, 1, hair);
    drawText(14, kHintY + 14, v.hint, kF14, text);
    int rx = 566;
    rx = drawText(rx, kHintY + 15, "Loop", kF12, dim);
    rx = drawText(rx + 6, kHintY + 14, v.loopMode, kF14, text);
    rx = drawText(rx + 18, kHintY + 15, "Pads", kF12, dim);
    frame(rx + 6, kHintY + 11, textWidth(v.padsMode, kF12) + 12, 18, text);
    drawText(rx + 12, kHintY + 15, v.padsMode, kF12, text);
}

void Painter::drawLane(int i, int y0)
{
    const Lane& L = v.lane[i];
    const uint32_t text = pal.text;
    const uint32_t bright = pal.text;
    const uint32_t dim = pal.dim;
    const uint32_t panel = pal.panel;
    const uint32_t hair = pal.frame;

    rect(kWaveX - 4, y0, kWaveW + 8, kLaneH - 1, pal.waveBg);
    if (L.selected)
    {
        rect(0, y0, kHeaderW, kLaneH - 1, pal.sel);
        rect(0, y0, 3, kLaneH - 1, bright);
    }
    rect(0, y0 + kLaneH - 1, 800, 1, hair);

    // name and mute box
    char head[20];
    head[0] = (char)('1' + i);
    head[1] = ' ';
    { int k = 0; for (; k < (int)sizeof(head) - 3 && L.name[k] != 0; k++) head[2 + k] = L.name[k]; head[2 + k] = 0; }
    head[sizeof(head) - 1] = 0;
    drawText(14, y0 + 8, head, kF17Bold, L.hasPart || L.active || L.selected ? bright : dim);
    if (L.muted)
    {
        rect(134, y0 + 8, 20, 18, pal.marker);
        drawText(140, y0 + 11, "M", kF12, pal.bg);
    }
    else
    {
        frame(134, y0 + 8, 20, 18, dim);
        drawText(140, y0 + 11, "M", kF12, dim);
    }

    // tags: source, glitch
    int tx = 14;
    const int tagW = textWidth(L.tag, kF12) + 12;
    frame(tx, y0 + 36, tagW, 18, text);
    drawText(tx + 6, y0 + 40, L.tag, kF12, text);
    tx += tagW + 4;
    if (L.glitch)
    {
        rect(tx, y0 + 36, textWidth("GLT", kF12) + 12, 18, pal.meter);
        drawText(tx + 6, y0 + 40, "GLT", kF12, pal.bg);
    }
    if (L.active)
    {
        dot(kHeaderW - 18, y0 + 45, 5, pal.rec);
    }
    if (L.info[0] != 0)
    {
        drawText(14, y0 + 60, L.info, kF12, dim);
    }
    // meter background (the filled part is the dynamic layer)
    rect(14, y0 + 76, 140, 3, pal.track);

    // waveform or message
    if (L.hasPart && L.waveCount > 0)
    {
        const int cy = y0 + 42;
        uint8_t heights[120];
        for (int c = 0; c < L.waveCount; c++)
        {
            heights[c] = (uint8_t)(2 + (L.wave[c] * 68) / 255);
        }
        g.color(L.muted ? pal.waveOff : pal.wave);
        g.bars(kWaveX, cy, 6, 4, heights, L.waveCount); // 100 columns of 6 px
    }
    else
    {
        if (L.msg1[0] != 0)
        {
            drawText(kWaveX + 14, y0 + 22, L.msg1, kF12, dim);
        }
        if (L.msg2[0] != 0 && !L.active)
        {
            drawText(kWaveX + 14, y0 + 42, L.msg2, kF17Bold, dim); // active lanes: dynamic layer
        }
    }
}

void Painter::drawButtons()
{
    const uint32_t text = pal.text;
    const uint32_t bright = pal.text;
    const uint32_t panel = pal.panel;
    const uint32_t hair = pal.frame;

    rect(0, kButtonsY, 800, 1, hair);
    for (int b = 0; b < 8; b++)
    {
        const int x0 = b * 100;
        rect(x0 + 1, kButtonsY + 1, 97, 38, pal.cell);
        if (v.btnSelected == b)
        {
            rect(x0 + 1, kButtonsY + 1, 97, 2, bright);
        }
        const char* label = v.btn[b];
        const int lw = textWidth(label, kF14);
        switch (v.btnKind[b])
        {
            case 1: // Rec dot and text
                dot(x0 + 24, kButtonsY + 20, 5, pal.rec);
                drawText(x0 + 38, kButtonsY + 12, label, kF14, text);
                break;
            case 2: // Stop square and text
                rect(x0 + 19, kButtonsY + 15, 10, 10, bright);
                drawText(x0 + 38, kButtonsY + 12, label, kF14, text);
                break;
            case 3: // Play glyph and text
                drawText(x0 + 14, kButtonsY + 9, "\x1a\x1b", kF17Bold, bright);
                drawText(x0 + 42, kButtonsY + 12, label, kF14, text);
                break;
            default: drawText(x0 + 1 + (97 - lw) / 2, kButtonsY + 12, label, kF14, text); break;
        }
    }
}

void Painter::drawDynamic()
{
    const uint32_t bright = pal.text;
    const uint32_t dim = pal.dim;

    // position readout in the top bar
    drawText(358, 8, v.pos, kF17Bold, bright);
    drawText(612, 11, v.inputLevels, kF12, dim);

    // counters of the lanes being recorded or armed
    for (int i = 0; i < 4; i++)
    {
        if (v.lane[i].active && v.lane[i].msg2[0] != 0)
        {
            drawText(kWaveX + 14, kLanesY + i * kLaneH + 42, v.lane[i].msg2, kF17Bold, bright);
        }
    }

    // level bars
    g.color(pal.meter);
    for (int i = 0; i < 4; i++)
    {
        const int w = (v.lane[i].meter * 140) / 100;
        if (w > 0)
        {
            g.rect(14, kLanesY + i * kLaneH + 76, w, 3);
        }
    }

    // playhead across all lanes
    if (v.loopFrames > 0)
    {
        const uint32_t pos = v.posFrames % v.loopFrames;
        const int px = kWaveX + (int)(((uint64_t)pos * kWaveW) / v.loopFrames);
        rect(px, kRulerY + 4, 1, kHintY - kRulerY - 4, bright);
        rect(px - 1, kRulerY + 2, 3, 4, bright);
    }
}

//--------------------------------------------------------------------------------------------
// "How many bars?" (design 4.3): a panel over the lower part of the screen
namespace
{
constexpr int kPromptY = 160;
constexpr int kCellsY = 232;
constexpr int kCellH = 76;
constexpr int kCellsPerRow = 6;
} // namespace

void Painter::drawPrompt()
{
    const Prompt& P = v.prompt;
    const uint32_t text = pal.text;
    const uint32_t bright = pal.text;
    const uint32_t dim = pal.dim;
    const uint32_t panel = pal.panel;
    const uint32_t hair = pal.frame;

    rect(0, kPromptY, 800, 480 - kPromptY, pal.bg);
    rect(0, kPromptY, 800, 2, bright);

    int x = drawText(14, kPromptY + 16, "How many bars?", kF17Bold, bright);
    if (P.memoryFull)
    {
        rect(x + 16, kPromptY + 17, textWidth("MEMORY FULL", kF12) + 12, 18, pal.marker);
        x = drawText(x + 22, kPromptY + 21, "MEMORY FULL", kF12, pal.bg);
        drawText(x + 22, kPromptY + 20, P.note, kF14, pal.marker);
    }
    drawText(14, kPromptY + 44, P.played, kF14, text);

    // result box on the right
    const uint32_t bpm = P.bpm10[P.selected];
    char big[16];
    fmt(big, sizeof(big), "%u.%u BPM", (unsigned)(bpm / 10), (unsigned)(bpm % 10));
    drawText(616, kCellsY + 4, "Result", kF12, dim);
    drawText(616, kCellsY + 22, big, kF17Bold, bright);
    char line[24];
    fmt(line, sizeof(line), "%u bars of 4/4", (unsigned)P.bars[P.selected]);
    drawText(616, kCellsY + 48, line, kF12, dim);
    drawText(616, kCellsY + 64, v.loopMode, kF12, dim);
    drawText(616, kCellsY + 80, P.suggestion, kF12, dim);

    drawText(14, kHintY + 14, "Jog or Left/Right chooses. B8 OK sets the Loop and tempo. B7 Cancel discards the take.", kF12, dim);

    // button row: only Cancel and OK do anything
    rect(0, kButtonsY, 800, 1, hair);
    for (int b = 0; b < 8; b++)
    {
        rect(b * 100 + 1, kButtonsY + 1, 97, 38, panel);
    }
    drawText(600 + 1 + (97 - textWidth("Cancel", kF14)) / 2, kButtonsY + 12, "Cancel", kF14, text);
    drawText(700 + 1 + (97 - textWidth("OK", kF14)) / 2, kButtonsY + 12, "OK", kF14, bright);
}

void Painter::drawPromptChoices()
{
    const Prompt& P = v.prompt;
    const uint32_t text = pal.text;
    const uint32_t bright = pal.text;
    const uint32_t dim = pal.dim;
    const uint32_t panel = pal.panel;

    for (int i = 0; i < P.count; i++)
    {
        const int x0 = (i % kCellsPerRow) * 100;
        const int y0 = kCellsY + (i / kCellsPerRow) * (kCellH + 4);
        const bool selected = (i == P.selected);
        rect(x0 + 1, y0, 97, kCellH, selected ? pal.sel : panel);
        if (selected)
        {
            rect(x0 + 1, y0, 97, 2, bright);
        }
        char n[8];
        fmt(n, sizeof(n), "%u", (unsigned)P.bars[i]);
        int x = drawText(x0 + 12, y0 + 12, n, kF17Bold, bright);
        drawText(x + 8, y0 + 16, "bars", kF12, dim);
        char t[12];
        fmt(t, sizeof(t), "%u.%u", (unsigned)(P.bpm10[i] / 10), (unsigned)(P.bpm10[i] % 10));
        drawText(x0 + 12, y0 + 38, t, kF17, selected ? bright : text);
        drawText(x0 + 12, y0 + 58, "BPM", kF12, dim);
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
