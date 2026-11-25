#ifndef FT_GFX_H_
#define FT_GFX_H_

#include <stdint.h>

namespace ft
{
// What the screens draw with. The firmware implements it on the display chip, the web emulator
// on a canvas. Coordinates are pixels on the 800 x 480 screen. Colours are 0xRRGGBB.
class Gfx
{
  public:
    virtual ~Gfx() {}
    virtual void color(uint32_t rgb) = 0;
    virtual void rect(int x, int y, int w, int h) = 0;           // filled, in the current colour
    virtual void line(int x0, int y0, int x1, int y1) = 0;       // one pixel wide
    virtual void dot(int x, int y, int radius) = 0;              // filled circle
    // Draws text with its top left at (x, y). `font` is 0 to 4, the five font slots.
    virtual void text(int x, int y, const char* s, int font) = 0;
    // Width in pixels the text will take (the sum of the glyph advances).
    virtual int textWidth(const char* s, int font) = 0;
    // Draws many equal-width vertical bars with the current colour (a waveform). `heights` are
    // pixel heights, centred on `cy`, bar `i` is at x0 + i * step, `barWidth` wide.
    virtual void bars(int x0, int cy, int step, int barWidth, const uint8_t* heights, int count) = 0;
};

// Colour roles used by the screens. Filled from the Tracker's theme so a custom theme recolours
// everything.
struct Palette
{
    uint32_t background;
    uint32_t text;     // normal text
    uint32_t bright;   // emphasised text, playhead
    uint32_t dim;      // captions, outlines, off states
    uint32_t panel;    // track headers, buttons, selected row
    uint32_t hairline; // thin separators
    uint32_t selected; // selected cell in a grid
    uint32_t meterBack;
    uint32_t wave;
    uint32_t record;
    uint32_t marker;   // loop line, mute, memory warnings
    uint32_t good;     // meters, ON tags
};
} // namespace ft

#endif /* FT_GFX_H_ */
