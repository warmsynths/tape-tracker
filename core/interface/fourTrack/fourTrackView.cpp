#include "fourTrackView.h"
#include "FT812.h"
#include "display.h"
#include <string.h>

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

// Font slots
constexpr int kF14Light = 0;
constexpr int kF17Light = 1;
constexpr int kF14 = 2;
constexpr int kF17Bold = 3;
constexpr int kF12 = 4;
} // namespace

extern strFont fonts[];

cFourTrackView::cFourTrackView(strControlProperties* properties)
  : cDisplayControl(controlIdSimpleBox, properties->value == 0 ? 5 : (properties->value == 2 ? 2 : 1))
{
    posX = 0;
    posY = 0;
    width = 800;
    height = 480;
    value = properties->value; // layer
    style = properties->style;
    view = (const strFtView*)properties->data;
    colors = properties->colors;
}

void cFourTrackView::rect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    API_COLOR(color);
    API_BEGIN(RECTS);
    API_VERTEX2F(x, y);
    API_VERTEX2F(x + w - 1, y + h - 1);
    API_END();
}

void cFourTrackView::frame(int x, int y, int w, int h, uint32_t color)
{
    API_COLOR(color);
    API_BEGIN(LINES);
    API_VERTEX2F(x, y);
    API_VERTEX2F(x + w - 1, y);
    API_VERTEX2F(x, y + h - 1);
    API_VERTEX2F(x + w - 1, y + h - 1);
    API_VERTEX2F(x, y);
    API_VERTEX2F(x, y + h - 1);
    API_VERTEX2F(x + w - 1, y);
    API_VERTEX2F(x + w - 1, y + h - 1);
    API_END();
}

void cFourTrackView::dot(int x, int y, int radius, uint32_t color)
{
    API_COLOR(color);
    API_POINT_SIZE(16 * radius);
    API_BEGIN(FTPOINTS);
    API_VERTEX2F(x, y);
    API_END();
}

int cFourTrackView::textWidth(const char* s, int fontIndex) const
{
    int w = 0;
    for (; *s != 0; s++)
    {
        w += fonts[fontIndex].data[(uint8_t)*s];
    }
    return w;
}

int cFourTrackView::drawText(int x, int y, const char* s, int fontIndex, uint32_t color)
{
    if (s == nullptr || s[0] == 0)
    {
        return x;
    }
    font = &fonts[fontIndex];
    API_COLOR(color);
    string2Bitmaps(x, y, s, (int8_t)strlen(s), false);
    return x + textWidth(s, fontIndex);
}

//--------------------------------------------------------------------------------------------
void cFourTrackView::update()
{
    if (view == nullptr)
    {
        return;
    }
    if (value == 0)
    {
        drawTop();
    }
    else if (value == 1)
    {
        drawDynamic();
    }
    else
    {
        drawHint();
    }
}

void cFourTrackView::update2()
{
    if (view == nullptr) return;
    if (value == 0) drawLane(0, kLanesY);
    else if (value == 2) drawButtons();
}

void cFourTrackView::update3()
{
    if (view != nullptr && value == 0) drawLane(1, kLanesY + kLaneH);
}

void cFourTrackView::update4()
{
    if (view != nullptr && value == 0) drawLane(2, kLanesY + 2 * kLaneH);
}

void cFourTrackView::update5()
{
    if (view != nullptr && value == 0) drawLane(3, kLanesY + 3 * kLaneH);
}

void cFourTrackView::drawTop()
{
    Theme& th = display.getTheme();
    const uint32_t text = th.activeLabelsColors[0];
    const uint32_t bright = th.white;
    const uint32_t dim = th.inactiveLabelsColors[3];
    const uint32_t panel = th.grey_22;
    const uint32_t hair = th.frameColors[1];

    // ---- Top bar ------------------------------------------------------------------------
    rect(0, 0, 800, kTopBarH, panel);
    int x = drawText(12, 8, view->project, kF17Bold, bright);
    if (view->unsaved)
    {
        rect(x + 6, 12, 6, 6, th.orange);
    }
    x = 150;
    x = drawText(x, 12, "BPM", kF12, dim);
    x = drawText(x + 6, 8, view->tempo, kF17Bold, bright);
    x = drawText(x + 18, 12, "BARS", kF12, dim);
    x = drawText(x + 6, 8, view->bars, kF17Bold, bright);
    drawText(330, 12, "POS", kF12, dim); // the value is drawn by the dynamic layer
    x = drawText(456, 12, "IN", kF12, dim);
    x = drawText(x + 6, 9, view->input, kF14, text);
    if (view->monitor)
    {
        rect(x + 10, 7, 36, 18, bright);
        drawText(x + 14, 11, "MON", kF12, th.backgroundColor);
    }
    else
    {
        frame(x + 10, 7, 66, 18, dim);
        drawText(x + 14, 11, "MON OFF", kF12, dim);
    }

    // ---- Status row and loop ruler ------------------------------------------------------
    switch (view->statusKind)
    {
        case 1: drawText(14, 36, "\x1a\x1b", kF17Bold, bright); break;                 // play glyph
        case 2: dot(20, 44, 5, th.red); break;                                        // recording
        case 3: frame(15, 39, 10, 10, dim); break;                                    // armed
        default: rect(15, 39, 10, 10, bright); break;                                 // stopped
    }
    drawText(32, 37, view->status, kF14, view->statusKind == 2 ? th.red : bright);
    if (view->statusNote[0] != 0)
    {
        drawText(kWaveX + 14, 40, view->statusNote, kF12, dim);
    }
    if (view->loopFrames > 0)
    {
        rect(kWaveX, 48, kWaveW, 3, th.orange);
        const int step = (view->barCount <= 8) ? 1 : 4;
        for (int b = 0; view->barCount > 0 && b < view->barCount; b += step)
        {
            const int bx = kWaveX + (b * kWaveW) / view->barCount;
            rect(bx, 38, 1, 12, dim);
            char n[4];
            n[0] = (char)('0' + ((b + 1) / 10));
            n[1] = (char)('0' + ((b + 1) % 10));
            n[2] = 0;
            drawText(bx + 4, 36, (b + 1) >= 10 ? n : n + 1, kF12, dim);
        }
    }

}

void cFourTrackView::drawHint()
{
    Theme& th = display.getTheme();
    const uint32_t text = th.activeLabelsColors[0];
    const uint32_t dim = th.inactiveLabelsColors[3];
    const uint32_t hair = th.frameColors[1];

    rect(0, kHintY, 800, 1, hair);
    drawText(14, kHintY + 14, view->hint, kF14, text);
    int rx = 566;
    rx = drawText(rx, kHintY + 15, "Loop", kF12, dim);
    rx = drawText(rx + 6, kHintY + 14, view->loopMode, kF14, text);
    rx = drawText(rx + 18, kHintY + 15, "Pads", kF12, dim);
    frame(rx + 6, kHintY + 11, textWidth(view->padsMode, kF12) + 12, 18, text);
    drawText(rx + 12, kHintY + 15, view->padsMode, kF12, text);
}

void cFourTrackView::drawLane(int i, int y0)
{
    Theme& th = display.getTheme();
    const strFtLane& L = view->lane[i];
    const uint32_t text = th.activeLabelsColors[0];
    const uint32_t bright = th.white;
    const uint32_t dim = th.inactiveLabelsColors[3];
    const uint32_t panel = th.grey_22;
    const uint32_t hair = th.frameColors[1];

    if (L.selected)
    {
        rect(0, y0, kHeaderW, kLaneH - 1, panel);
        rect(0, y0, 3, kLaneH - 1, bright);
    }
    rect(0, y0 + kLaneH - 1, 800, 1, hair);

    // name and mute box
    char head[20];
    head[0] = (char)('1' + i);
    head[1] = ' ';
    strncpy(head + 2, L.name, sizeof(head) - 3);
    head[sizeof(head) - 1] = 0;
    drawText(14, y0 + 8, head, kF17Bold, L.hasPart || L.active || L.selected ? bright : dim);
    if (L.muted)
    {
        rect(134, y0 + 8, 20, 18, th.orange);
        drawText(140, y0 + 11, "M", kF12, th.backgroundColor);
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
        rect(tx, y0 + 36, textWidth("GLT", kF12) + 12, 18, th.green);
        drawText(tx + 6, y0 + 40, "GLT", kF12, th.backgroundColor);
    }
    if (L.active)
    {
        dot(kHeaderW - 18, y0 + 45, 5, th.red);
    }
    if (L.info[0] != 0)
    {
        drawText(14, y0 + 60, L.info, kF12, dim);
    }
    // meter background (the filled part is the dynamic layer)
    rect(14, y0 + 76, 140, 3, th.inactiveBarColors[0]);

    // waveform or message
    if (L.hasPart && L.waveCount > 0)
    {
        const int cy = y0 + 42;
        const uint32_t color = L.muted ? dim : th.waveformColors[1];
        API_COLOR(color);
        API_BEGIN(RECTS);
        for (int c = 0; c < L.waveCount; c++)
        {
            int h = 2 + (L.wave[c] * 68) / 255;
            const int x = kWaveX + c * 6; // 100 columns of 6 px
            API_VERTEX2F(x, cy - h / 2);
            API_VERTEX2F(x + 3, cy - h / 2 + h - 1);
        }
        API_END();
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

void cFourTrackView::drawButtons()
{
    Theme& th = display.getTheme();
    const uint32_t text = th.activeLabelsColors[0];
    const uint32_t bright = th.white;
    const uint32_t panel = th.grey_22;
    const uint32_t hair = th.frameColors[1];

    rect(0, kButtonsY, 800, 1, hair);
    for (int b = 0; b < 8; b++)
    {
        const int x0 = b * 100;
        rect(x0 + 1, kButtonsY + 1, 97, 38, panel);
        if (view->btnSelected == b)
        {
            rect(x0 + 1, kButtonsY + 1, 97, 2, bright);
        }
        const char* label = view->btn[b];
        const int lw = textWidth(label, kF14);
        switch (view->btnKind[b])
        {
            case 1: // Rec dot and text
                dot(x0 + 24, kButtonsY + 20, 5, th.red);
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

void cFourTrackView::drawDynamic()
{
    Theme& th = display.getTheme();
    const uint32_t bright = th.white;
    const uint32_t dim = th.inactiveLabelsColors[3];

    // position readout in the top bar
    drawText(358, 8, view->pos, kF17Bold, bright);
    drawText(640, 11, view->inputLevels, kF12, dim);

    // counters of the lanes being recorded or armed
    for (int i = 0; i < 4; i++)
    {
        if (view->lane[i].active && view->lane[i].msg2[0] != 0)
        {
            drawText(kWaveX + 14, kLanesY + i * kLaneH + 42, view->lane[i].msg2, kF17Bold, bright);
        }
    }

    // level bars
    API_COLOR(th.green);
    API_BEGIN(RECTS);
    for (int i = 0; i < 4; i++)
    {
        const int w = (view->lane[i].meter * 140) / 100;
        if (w > 0)
        {
            API_VERTEX2F(14, kLanesY + i * kLaneH + 76);
            API_VERTEX2F(14 + w - 1, kLanesY + i * kLaneH + 78);
        }
    }
    API_END();

    // playhead across all lanes
    if (view->loopFrames > 0)
    {
        const uint32_t pos = view->posFrames % view->loopFrames;
        const int px = kWaveX + (int)(((uint64_t)pos * kWaveW) / view->loopFrames);
        rect(px, kRulerY + 4, 1, kHintY - kRulerY - 4, bright);
        rect(px - 1, kRulerY + 2, 3, 4, bright);
    }
}
