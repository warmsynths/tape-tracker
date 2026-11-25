#ifndef INTERFACE_FOURTRACK_FOURTRACKVIEW_H_
#define INTERFACE_FOURTRACK_FOURTRACKVIEW_H_

#include "App.h"
#include "displayControls.h"

// A display control that draws one layer of the Four-Track main screen (see core/ft/View.h).
// The drawing itself is the shared code in core/ft; this class only turns it into display chip
// commands. The layer is passed as `value`; each stage is one coprocessor step (the chip's
// command buffer is only 4 KB, so a stage must stay small).
class cFourTrackView : public cDisplayControl
{
  public:
    cFourTrackView(strControlProperties* properties);
    virtual ~cFourTrackView() {}

    virtual void update() override { stage(0); }
    virtual void update2() override { stage(1); }
    virtual void update3() override { stage(2); }
    virtual void update4() override { stage(3); }
    virtual void update5() override { stage(4); }
    virtual void setStyle(uint32_t style) override { this->style = style; }
    virtual void setText(const char* text) override { this->text = text; }
    virtual void setValue(int value) override { this->value = value; }
    virtual void setColors(const uint32_t* colors) override { this->colors = colors; }
    virtual void setData(void* data) override { app = (const ft::App*)data; }

    // Used by the drawing adapter.
    void drawString(int x, int y, const char* s, int fontIndex);

  private:
    void stage(int n);
    const ft::App* app = nullptr;
};

ft::Gfx* fourTrackTextMeter();

#endif /* INTERFACE_FOURTRACK_FOURTRACKVIEW_H_ */
