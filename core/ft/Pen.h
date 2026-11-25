#ifndef FT_PEN_H_
#define FT_PEN_H_

#include "Gfx.h"
#include "MiniFmt.h"

namespace ft
{
// Small drawing helpers for the pages (everything except the main screen). Fonts are the five
// slots: 0 14 Regular, 1 17 Regular, 2 14 Bold, 3 17 Bold, 4 12 Regular.
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
        g.text(x, y, s, font);
        return x + g.textWidth(s, font);
    }
    void textRight(int xRight, int y, const char* s, int font, uint32_t c) { text(xRight - width(s, font), y, s, font, c); }
    void textMid(int x, int w, int y, const char* s, int font, uint32_t c) { text(x + (w - width(s, font)) / 2, y, s, font, c); }
    // A horizontal bar: `percent` 0..100 filled
    void bar(int x, int y, int w, int h, int percent, uint32_t fill, uint32_t back)
    {
        rect(x, y, w, h, back);
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        rect(x, y, w * percent / 100, h, fill);
    }
    // An outlined or filled tag, returns its width
    int tag(int x, int y, const char* s, bool filled, uint32_t c, uint32_t textOn)
    {
        const int w = width(s, F12) + 12;
        if (filled)
        {
            rect(x, y, w, 18, c);
            text(x + 6, y + 2, s, F12, textOn);
        }
        else
        {
            frame(x, y, w, 18, c);
            text(x + 6, y + 2, s, F12, c);
        }
        return w;
    }
    // The top bar shared by all pages: project name, then right-aligned readouts
    void topBar(const char* project, bool unsaved, const char* tempo, const char* bars, const char* pos, const char* input, const char* fb)
    {
        rect(0, 0, 800, 32, p.top);
        int x = text(12, 8, project, F17B, p.topText);
        if (unsaved) rect(x + 6, 12, 6, 6, p.warn);
        x = 150;
        x = text(x, 12, "BPM", F12, p.dim);
        x = text(x + 6, 8, tempo, F17B, p.topText);
        x = text(x + 18, 12, "BARS", F12, p.dim);
        x = text(x + 6, 8, bars, F17B, p.topText);
        text(330, 12, "POS", F12, p.dim);
        text(358, 8, pos, F17B, p.topText);
        x = text(456, 12, "IN", F12, p.dim);
        text(x + 6, 9, input, F14, p.text);
        if (fb[0] != 0)
        {
            const int w = width(fb, F12) + 12;
            rect(700, 7, w, 18, p.warn);
            text(706, 9, fb, F12, p.bg);
        }
    }
    // The row of eight buttons under the screen. Empty labels leave a blank cell.
    void buttons(const char* const* labels, int selected = -1, const bool* accent = nullptr)
    {
        rect(0, 440, 800, 1, p.frame);
        for (int b = 0; b < 8; b++)
        {
            rect(b * 100 + 1, 441, 97, 38, p.cell);
            if (selected == b) rect(b * 100 + 1, 441, 97, 2, p.text);
            if (labels[b] != nullptr && labels[b][0] != 0)
                textMid(b * 100 + 1, 97, 452, labels[b], F14, (accent != nullptr && accent[b]) ? p.warn : p.text);
        }
    }

    Gfx& g;
    const Palette& p;
};
} // namespace ft

#endif /* FT_PEN_H_ */
