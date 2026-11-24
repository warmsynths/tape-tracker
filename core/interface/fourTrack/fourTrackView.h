#ifndef INTERFACE_FOURTRACK_FOURTRACKVIEW_H_
#define INTERFACE_FOURTRACK_FOURTRACKVIEW_H_

#include "displayControls.h"
#include <stdint.h>

// Everything the Four-Track main screen shows. The module fills it in; the view only draws it.
// Layout follows docs/design/screen-requirements.md section 4.1 (800 x 480).
struct strFtLane
{
    char name[12];
    char tag[10];  // source: LINE, MIC LOW, ...
    char info[20]; // second line, for example "Offset +522 ms"
    char msg1[32]; // shown in the waveform area when there is no Part (Empty, Recording ...)
    char msg2[24]; // bigger second line in that area
    bool hasPart;
    bool muted;
    bool selected;
    bool glitch;
    bool active; // being recorded or armed: red mark
    uint8_t waveCount;
    uint8_t wave[120]; // 0..255 height of each waveform column
    uint8_t meter;     // 0..100, filled width of the level bar
};

struct strFtView
{
    char project[20];
    bool unsaved;
    char tempo[10];
    char bars[6];
    char pos[10];
    char input[12];
    bool monitor;
    char status[20];
    uint8_t statusKind; // 0 stopped, 1 playing, 2 recording, 3 armed
    char statusNote[24];
    uint32_t loopFrames; // 0: no loop yet
    uint32_t posFrames;
    uint8_t barCount;
    char hint[72];
    char loopMode[8];
    char padsMode[6];
    char btn[8][10];
    uint8_t btnKind[8]; // 0 text, 1 Rec dot, 2 Stop square, 3 Play glyph
    int8_t btnSelected; // -1 none
    strFtLane lane[4];
};

// Draws the main screen. Three copies are created, told apart by `value`:
//   0: top bar, status, loop ruler and the four lanes (rebuilt on events). Five stages: the
//      display chip's command buffer is only 4 KB and a stage must stay well below that.
//   1: playhead, position, counters and level bars (rebuilt about 20 times a second).
//   2: hint line and the button row (two stages).
// Text is drawn the way the stock tracker grid does it, one character at a time, so the drawing
// budget stays small.
class cFourTrackView : public cDisplayControl
{
  public:
    cFourTrackView(strControlProperties* properties);
    virtual ~cFourTrackView() {}

    virtual void update() override;
    virtual void update2() override;
    virtual void update3() override;
    virtual void update4() override;
    virtual void update5() override;
    virtual void setStyle(uint32_t style) override { this->style = style; }
    virtual void setText(const char* text) override { this->text = text; }
    virtual void setValue(int value) override { this->value = value; }
    virtual void setColors(const uint32_t* colors) override { this->colors = colors; }
    virtual void setData(void* data) override { view = (const strFtView*)data; }

  private:
    // Drawing helpers
    void rect(int x, int y, int w, int h, uint32_t color);
    void frame(int x, int y, int w, int h, uint32_t color);
    void dot(int x, int y, int radius, uint32_t color);
    // Draws text, returns the x just after it. `fontIndex` is 0 to 4 (the five font slots).
    int drawText(int x, int y, const char* s, int fontIndex, uint32_t color);
    int textWidth(const char* s, int fontIndex) const;

    void drawTop();
    void drawLane(int index, int y0);
    void drawHint();
    void drawButtons();
    void drawDynamic();

    const strFtView* view = nullptr;
};

#endif /* INTERFACE_FOURTRACK_FOURTRACKVIEW_H_ */
