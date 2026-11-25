#include "fourTrack.h"

#include "Platform.h"
#include "core/interface.h"
#include "core/songTimer.h"
#include "mtAudioEngine.h"
#include "mtConfig.h"
#include "mtStructs.h"

cFourTrack fourTrack;
static cFourTrack* FT = &fourTrack;

static void functKey(uint8_t index)
{
    FT->application().key(index);
}
static void functB1() { functKey(ft::kB1); }
static void functB2() { functKey(ft::kB2); }
static void functB3() { functKey(ft::kB3); }
static void functB4() { functKey(ft::kB4); }
static void functB5() { functKey(ft::kB5); }
static void functB6() { functKey(ft::kB6); }
static void functB7() { functKey(ft::kB7); }
static void functB8() { functKey(ft::kB8); }
static void functUp() { functKey(ft::kUp); }
static void functDown() { functKey(ft::kDown); }
static void functLeft() { functKey(ft::kLeft); }
static void functRight() { functKey(ft::kRight); }
static void functRec() { functKey(ft::kRec); }
static void functPlay() { functKey(ft::kPlay); }
static void functEncoder(int16_t value) { FT->application().jog(value); }
static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    (void)velo;
    FT->application().pad(pad, state == buttonPress);
}

static void functSwitchModule(uint8_t button)
{
    FT->eventFunct(eventSwitchModule, FT, &button, 0);
}

cFourTrack::cFourTrack()
  : hal(platform)
{
}

// Colours come from the Tracker's theme, so a custom theme recolours this screen too.
ft::Palette cFourTrack::paletteFromTheme() const
{
    Theme& th = display.getTheme();
    ft::Palette p;
    p.background = th.backgroundColor;
    p.text = th.activeLabelsColors[0];
    p.bright = th.white;
    p.dim = th.inactiveLabelsColors[3];
    p.panel = th.grey_22;
    p.hairline = th.frameColors[1];
    p.selected = th.frameColors[1];
    p.meterBack = th.inactiveBarColors[0];
    p.wave = th.waveformColors[1];
    p.record = th.red;
    p.marker = th.orange;
    p.good = th.green;
    return p;
}

ft::App& cFourTrack::application()
{
    if (app == nullptr)
    {
        app = new ft::App(hal, paletteFromTheme());
    }
    return *app;
}

void cFourTrack::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;
    application().setPalette(paletteFromTheme());
    hal.connectRecorder();

    songTimer.hide();

    ip->setButtonObj(interfaceButtonPerformance, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonParams, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonFile, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonConfig, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonMaster, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSamplePlay, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleEdit, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleRec, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleLoad, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSong, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonPattern, buttonPress, functSwitchModule);

#ifndef TRACKER_MINI
    platform->getRadio()->enableLineInput(false);
#endif

    showScreen();
    setDefaultScreenFunct();
    promptOnScreen = false;
    application().enter();
}

void cFourTrack::stop()
{
    application().leave();
    moduleRefresh = 0;
    hal.restoreAudio();
    display.setControlHide(viewStatic);
    display.setControlHide(viewDynamic);
    display.setControlHide(viewFooter);
    display.setControlHide(viewPrompt);
    promptOnScreen = false;
    songTimer.show();
}

void cFourTrack::setDefaultScreenFunct()
{
    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    ip->clearAllPots();

    ip->setButtonObj(interfaceButton0, buttonPress, functB1);
    ip->setButtonObj(interfaceButton1, buttonPress, functB2);
    ip->setButtonObj(interfaceButton2, buttonPress, functB3);
    ip->setButtonObj(interfaceButton3, buttonPress, functB4);
    ip->setButtonObj(interfaceButton4, buttonPress, functB5);
    ip->setButtonObj(interfaceButton5, buttonPress, functB6);
    ip->setButtonObj(interfaceButton6, buttonPress, functB7);
    ip->setButtonObj(interfaceButton7, buttonPress, functB8);
    ip->setPadsGlobal(functPads);
    ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functRight);
    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);
    ip->setPotObj(interfacePot0, functEncoder);
    ip->setButtonObj(interfaceButtonRec, buttonPress, functRec);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlay);
}

void cFourTrack::update()
{
    ft::App& a = application();
    a.tick();

    bool changed = false;
    if (a.takeRedraw(0)) { display.refreshControl(viewStatic); changed = true; }
    if (a.takeRedraw(2)) { display.refreshControl(viewFooter); changed = true; }
    if (a.takeRedraw(1)) { display.refreshControl(viewDynamic); changed = true; }

    const bool wantPrompt = a.layerVisible(3);
    if (wantPrompt && !promptOnScreen)
    {
        promptOnScreen = true;
        display.setControlShow(viewPrompt);
        changed = true;
    }
    else if (!wantPrompt && promptOnScreen)
    {
        promptOnScreen = false;
        display.setControlHide(viewPrompt);
        changed = true;
    }
    if (a.takeRedraw(3))
    {
        display.refreshControl(viewPrompt);
        changed = true;
    }
    if (changed)
    {
        display.synchronizeRefresh();
    }
}
