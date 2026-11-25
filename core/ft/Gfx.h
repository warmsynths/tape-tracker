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

// Colour roles used by the screens: the design's 17 roles (docs/design). Filled from the
// Tracker's theme on the device, so a custom theme recolours everything.
struct Palette
{
    uint32_t bg;       // screen ground
    uint32_t top;      // top bar
    uint32_t topText;  // top-bar text
    uint32_t frame;    // hairlines, button strip, panel edge
    uint32_t text;     // primary text, playhead, focus line
    uint32_t dim;      // captions, outlined tags, off states
    uint32_t cell;     // button cells
    uint32_t sel;      // selected track, field, row, slot
    uint32_t panel;    // track headers, fields, panels
    uint32_t rec;      // record only
    uint32_t meter;    // level meters, GLT and ON tags, activity
    uint32_t track;    // meter and bar backgrounds
    uint32_t wave;     // waveform
    uint32_t waveBg;   // lane audio area
    uint32_t waveOff;  // muted or trimmed audio
    uint32_t marker;   // loop region and markers, mute on
    uint32_t warn;     // memory and feedback warnings, unsaved mark, bypass
};

// The design places text by the top of its line box; the Tracker's font bitmaps have their own
// space above the capitals. Add this to the design's y to draw a font so the letters land on the
// same rows as in the design (fonts: 14 Regular, 17 Regular, 14 Bold, 17 Bold, 12 Regular).
inline int textDy(int font)
{
    static const int dy[5] = { -3, -4, -3, -4, -1 };
    return dy[font < 0 || font > 4 ? 0 : font];
}

// The design's "Original" palette (docs/design/four-track-recorder-design-r4.zip).
inline Palette designPalette()
{
    return Palette{ 0x15130f, 0x221f19, 0xd3c8ae, 0x3a342a, 0xe6dcc3, 0x91876f, 0x211e18, 0x332e25, 0x1c1a15,
                    0xc4452e, 0x93a66a, 0x2c2922, 0x8d826c, 0x191712, 0x575043, 0xd8873a, 0xd2a94f };
}
} // namespace ft

#endif /* FT_GFX_H_ */
