#ifndef INTERFACE_FOURTRACK_H_
#define INTERFACE_FOURTRACK_H_

#include "App.h"
#include "core/modulesBase.h"
#include "fourTrackHal.h"

// The Four-Track screen as a Tracker module. All of its behaviour and drawing is the shared
// application in core/ft (which the web emulator runs too). This class only connects it to the
// Tracker: it forwards buttons, the jog wheel and the pads, runs the application's main loop,
// and keeps the display controls up to date.
class cFourTrack : public ModuleBase
{
  public:
    virtual void update() override;
    virtual void start(uint32_t options) override;
    virtual void stop() override;

    virtual void initDisplayControls() override;
    virtual void destroyDisplayControls() override {}

    cFourTrack();

    ft::App& application();

    // True while any track holds a recorded Part. The Sample Editor uses the same memory as the
    // track regions, so it must not be opened then.
    bool hasParts() { return application().state().partCount() > 0; }

  private:
    void showScreen();
    void setDefaultScreenFunct();
    ft::Palette paletteFromTheme() const;

    FourTrackHal hal;
    ft::App* app = nullptr;

    // One display control per layer (see core/ft/App.h): 0 top bar and lanes, 1 playhead and
    // levels, 2 hint and buttons, 3 the bars question, 4 a page, 5 a popup
    hControl view[6] = { -1, -1, -1, -1, -1, -1 };
    bool shown[6] = {};
};

extern cFourTrack fourTrack;

#endif /* INTERFACE_FOURTRACK_H_ */
