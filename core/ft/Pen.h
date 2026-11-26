#ifndef FT_PEN_H_
#define FT_PEN_H_

#include "Gfx.h"
#include "MiniFmt.h"
#include "View.h"

namespace ft
{
// Drawing helpers for the pages (everything except the main screen). Fonts are the five slots:
// 0 14 Regular, 1 17 Regular, 2 14 Bold, 3 17 Bold, 4 12 Regular. All coordinates are the
// design's (docs/design): text is placed by the top of its line box.
enum Font { F14 = 0, F17 = 1, F14B = 2, F17B = 3, F12 = 4 };

class Pen
{
  public:
    Pen(Gfx& gfx, const Palette& palette)
      : g(gfx)
      , p(palette)
    {
    }

    void rect(int x, int y, int w, int h, uint32_t c)
    {
        if (w <= 0 || h <= 0) return;
        g.color(c);
        g.rect(x, y, w, h);
    }
    void frame(int x, int y, int w, int h, uint32_t c)
    {
        g.color(c);
        g.line(x, y, x + w - 1, y);
        g.line(x, y + h - 1, x + w - 1, y + h - 1);
        g.line(x, y, x, y + h - 1);
        g.line(x + w - 1, y, x + w - 1, y + h - 1);
    }
    void dot(int x, int y, int r, uint32_t c)
    {
        g.color(c);
        g.dot(x, y, r);
    }
    int width(const char* s, int font) { return g.textWidth(s, font); }
    int text(int x, int y, const char* s, int font, uint32_t c)
    {
        if (s == nullptr || s[0] == 0) return x;
        g.color(c);
        g.text(x, y + textDy(font), s, font);
        return x + g.textWidth(s, font);
    }
    void textRight(int xRight, int y, const char* s, int font, uint32_t c) { text(xRight - width(s, font), y, s, font, c); }
    void textMid(int x, int w, int y, const char* s, int font, uint32_t c) { text(x + (w - width(s, font)) / 2, y, s, font, c); }

    // A track bar: the background, then the filled part
    void bar(int x, int y, int w, int h, int percent, uint32_t fill, uint32_t back)
    {
        rect(x, y, w, h, back);
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        rect(x, y, w * percent / 100, h, fill);
    }
    // A small tag: outlined (off) or filled (on). Returns its width.
    int tag(int x, int y, const char* s, bool filled, uint32_t fillColor, uint32_t textOn)
    {
        const int w = width(s, F12) + (filled ? 11 : 13);
        if (filled)
        {
            rect(x, y, w, 18, fillColor);
            text(x + 5, y + 2, s, F12, textOn);
        }
        else
        {
            frame(x, y, w, 18, p.dim);
            text(x + 7, y + 2, s, F12, p.text);
        }
        return w;
    }
    // A cell with the focus look: lighter fill and a bar along its top edge
    void cell(int x, int y, int w, int h, bool focus, uint32_t normal)
    {
        rect(x, y, w, h, focus ? p.sel : normal);
        if (focus) rect(x, y, w, 2, p.text);
    }

    // The top bar every page shares (design 4.1)
    void topBar(const View& v)
    {
        rect(0, 0, 800, 32, p.top);
        rect(0, 31, 800, 1, p.frame);
        int x = text(12, 7, v.project, F17B, p.topText);
        if (v.unsaved) rect(x + 7, 13, 6, 6, p.warn);
        static const int seps[4] = { 140, 240, 322, 446 };
        for (int i = 0; i < 4; i++) rect(seps[i], 8, 1, 16, p.frame);
        text(150, 10, "BPM", F12, p.dim);
        text(180, 7, v.tempo, F17B, p.topText);
        text(250, 10, "BARS", F12, p.dim);
        text(286, 7, v.bars, F17B, p.topText);
        text(332, 10, "POS", F12, p.dim);
        text(362, 7, v.pos, F17B, p.topText);
        text(456, 10, "IN", F12, p.dim);
        text(474, 8, v.input, F14, p.topText);
        if (v.monitor)
        {
            rect(540, 7, width("MON", F12) + 5, 18, p.topText);
            text(542, 9, "MON", F12, p.top);
        }
        else
        {
            frame(540, 7, width("MON OFF", F12) + 5, 18, p.dim);
            text(542, 9, "MON OFF", F12, p.dim);
        }
        if (v.click)
        {
            rect(610, 7, width("CLICK", F12) + 11, 18, p.meter);
            text(615, 9, "CLICK", F12, p.bg);
        }
        if (v.wait)
        {
            const int wx = v.click ? 610 + width("CLICK", F12) + 17 : 610;
            rect(wx, 7, width("WAIT", F12) + 11, 18, p.marker);
            text(wx + 5, 9, "WAIT", F12, p.bg);
        }
        if (v.fb[0] != 0)
        {
            rect(716, 7, width(v.fb, F12) + 12, 18, p.warn);
            text(722, 9, v.fb, F12, p.bg);
        }
    }
    // The row of eight buttons under the screen; `on` has a bit for each button shown as chosen
    void buttons(const char* const* labels, uint8_t on = 0, int accentButton = -1)
    {
        rect(0, 440, 800, 40, p.frame);
        for (int b = 0; b < 8; b++)
        {
            cell(b * 100 + 1, 441, 97, 39, (on >> b) & 1, p.cell);
            if (labels[b] != nullptr && labels[b][0] != 0)
                textMid(b * 100 + 1, 97, 451, labels[b], F14, b == accentButton ? p.rec : p.text);
        }
    }
    // The line above the buttons, with its text
    void hint(const char* s)
    {
        rect(0, 400, 800, 1, p.frame);
        text(14, 412, s, F14, p.text);
    }

    Gfx& g;
    const Palette& p;
};
} // namespace ft

#endif /* FT_PEN_H_ */
