#ifndef FT_CANVAS_H_
#define FT_CANVAS_H_

#include "Gfx.h"
#include <stdint.h>

namespace ft
{
// Records drawing commands so a page can be built once and replayed in small stages. The display
// chip takes about 4 KB of commands at a time, so a page is split into stages by estimated cost.
// Also the way the emulator gets the same pages: it replays the same stages.
class Canvas : public Gfx
{
  public:
    static constexpr int kMaxStages = 5;
    static constexpr int kStageBudget = 1700; // bytes of display chip commands per stage
    static constexpr int kMaxCommands = 900;
    static constexpr int kPoolSize = 9000;

    void clear();
    int stageCount() const;      // stages in use, 0 if the page is empty
    int estimatedBytes() const;  // whole page
    bool overflowed() const { return over; }
    void replay(int stage, Gfx& gfx) const;

    void color(uint32_t rgb) override { add(kColor, (int)rgb, 0, 0, 0, 0, 4); }
    void rect(int x, int y, int w, int h) override { add(kRect, x, y, w, h, 0, 20); }
    void roundRect(int x, int y, int w, int h, int r) override { add(kRoundRect, x, y, w, h, r, 28); } // the corner setting is set and put back around it
    void line(int x0, int y0, int x1, int y1) override { add(kLine, x0, y0, x1, y1, 0, 20); }
    void dot(int x, int y, int r) override { add(kDot, x, y, r, 0, 0, 24); }
    void text(int x, int y, const char* s, int font) override;
    int textWidth(const char* s, int font) override { return measure ? measure->textWidth(s, font) : 0; }
    void bars(int x0, int cy, int step, int bw, const uint8_t* heights, int count) override;
    void alpha(int a) override { add(kAlpha, a, 0, 0, 0, 0, 4); }
    void mask(int slot, int x, int y, int w, int h) override { add(kMask, slot, x, y, w, h, 40); } // handle, source, layout, size, size_h, begin, vertex, end and the context pair

    // Text widths come from the real display (or the emulator's font tables).
    void setMeasure(Gfx* g) { measure = g; }

  private:
    enum Op : uint8_t { kColor, kRect, kLine, kDot, kText, kBars, kAlpha, kMask, kRoundRect };
    struct Cmd
    {
        Op op;
        int16_t a, b, c, d;
        uint16_t off; // text or bars data in the pool
        uint16_t n;   // length
        int16_t cost;
    };
    void add(Op op, int a, int b, int c, int d, int off, int cost, int n = 0);
    Cmd cmds[kMaxCommands];
    uint8_t pool[kPoolSize];
    int count = 0, poolUsed = 0;
    bool over = false;
    Gfx* measure = nullptr;
};
} // namespace ft

#endif /* FT_CANVAS_H_ */
