#include "fourTrackView.h"

#include "FT812.h"
#include "display.h"
#include <string.h>

extern strFont fonts[];

namespace
{
// ft::Gfx on the display chip.
class DisplayGfx : public ft::Gfx
{
  public:
    explicit DisplayGfx(cFourTrackView& v)
      : view(v)
    {
    }

    void color(uint32_t rgb) override { API_COLOR(rgb); }
    void rect(int x, int y, int w, int h) override
    {
        API_BEGIN(RECTS);
        API_VERTEX2F(x, y);
        API_VERTEX2F(x + w - 1, y + h - 1);
        API_END();
    }
    void line(int x0, int y0, int x1, int y1) override
    {
        API_BEGIN(LINES);
        API_VERTEX2F(x0, y0);
        API_VERTEX2F(x1, y1);
        API_END();
    }
    void dot(int x, int y, int radius) override
    {
        API_POINT_SIZE(16 * radius);
        API_BEGIN(FTPOINTS);
        API_VERTEX2F(x, y);
        API_END();
    }
    void text(int x, int y, const char* s, int font) override { view.drawString(x, y, s, font); }
    int textWidth(const char* s, int font) override
    {
        int w = 0;
        for (; *s != 0; s++)
        {
            w += fonts[font].data[(uint8_t)*s];
        }
        return w;
    }
    void bars(int x0, int cy, int step, int barWidth, const uint8_t* heights, int count) override
    {
        API_BEGIN(RECTS);
        for (int i = 0; i < count; i++)
        {
            const int x = x0 + i * step;
            API_VERTEX2F(x, cy - heights[i] / 2);
            API_VERTEX2F(x + barWidth - 1, cy - heights[i] / 2 + heights[i] - 1);
        }
        API_END();
    }

  private:
    cFourTrackView& view;
};
} // namespace

cFourTrackView::cFourTrackView(strControlProperties* properties)
  : cDisplayControl(controlIdSimpleBox, properties->value >= 4 ? 5 : ft::layerStages(properties->value))
{
    posX = 0;
    posY = 0;
    width = 800;
    height = 480;
    value = properties->value; // layer
    style = properties->style;
    app = (const ft::App*)properties->data;
    colors = properties->colors;
}

void cFourTrackView::drawString(int x, int y, const char* s, int fontIndex)
{
    font = &fonts[fontIndex];
    string2Bitmaps(x, y, s, (int8_t)strlen(s), false);
}

void cFourTrackView::stage(int n)
{
    if (app == nullptr)
    {
        return;
    }
    DisplayGfx gfx(*this);
    if (value >= 4 && n >= app->stagesOf(value)) return; // a page uses as many of its five stages as it needs
    app->draw(value, n, gfx);
}

// Measures text for the application (page layout needs text widths) without drawing.
ft::Gfx* fourTrackTextMeter()
{
    class Meter : public ft::Gfx
    {
      public:
        void color(uint32_t) override {}
        void rect(int, int, int, int) override {}
        void line(int, int, int, int) override {}
        void dot(int, int, int) override {}
        void text(int, int, const char*, int) override {}
        int textWidth(const char* s, int font) override
        {
            int w = 0;
            for (; *s != 0; s++) w += fonts[font].data[(uint8_t)*s];
            return w;
        }
        void bars(int, int, int, int, const uint8_t*, int) override {}
    };
    static Meter meter;
    return &meter;
}
