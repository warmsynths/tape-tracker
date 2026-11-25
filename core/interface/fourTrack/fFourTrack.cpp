#include "fourTrack.h"

#include "Platform.h"
#include "core/interface.h"
#include "core/songTimer.h"
#include "mtAudioEngine.h"
#include "mtConfig.h"
#include "mtStructs.h"
#include "fourTrackView.h"
#include <new>

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
static void functB1Up() { FT->application().keyUp(ft::kB1); }
static void functB2Up() { FT->application().keyUp(ft::kB2); }
static void functB3Up() { FT->application().keyUp(ft::kB3); }
static void functB4Up() { FT->application().keyUp(ft::kB4); }
static void functShiftDown() { FT->application().setShift(true); }
static void functShiftUp() { FT->application().setShift(false); }
static void functJogPress() { functKey(ft::kJogPress); }
static void functSampleEdit() { functKey(ft::kSampleEdit); }
static void functPerformance() { functKey(ft::kPerformance); }
static void functMaster() { functKey(ft::kMaster); }
static void functFile() { functKey(ft::kFile); }
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

extern int16_t externalRamSampleMemory[];

// The colours are the design's own (docs/design/); the emulator uses the same ones.
ft::Palette cFourTrack::paletteFromTheme() const { return ft::designPalette(); }

ft::App& cFourTrack::application()
{
    if (app == nullptr)
    {
        static_assert(sizeof(ft::App) <= kFourTrackAppBytes, "the application must fit its reserved memory");
        void* where = (uint8_t*)externalRamSampleMemory + (SAMPLE_MEMORY_SIZE - kFourTrackReservedBytes);
        app = new (where) ft::App(hal, paletteFromTheme());
        app->setMeasure(fourTrackTextMeter());
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

    ip->setButtonObj(interfaceButtonParams, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonConfig, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSamplePlay, buttonPress, functSwitchModule);
    // These four open pages of this screen: Part Editor, Glitch, Lo-fi master, Project
    ip->setButtonObj(interfaceButtonSampleEdit, buttonPress, functSampleEdit);
    ip->setButtonObj(interfaceButtonPerformance, buttonPress, functPerformance);
    ip->setButtonObj(interfaceButtonMaster, buttonPress, functMaster);
    ip->setButtonObj(interfaceButtonFile, buttonPress, functFile);
    ip->setButtonObj(interfaceButtonSampleRec, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleLoad, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSong, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonPattern, buttonPress, functSwitchModule);

#ifndef TRACKER_MINI
    platform->getRadio()->enableLineInput(false);
#endif

    showScreen();
    setDefaultScreenFunct();
    application().enter();
}

void cFourTrack::stop()
{
    application().leave();
    moduleRefresh = 0;
    hal.restoreAudio();
    for (int l = 0; l < 6; l++)
    {
        display.setControlHide(view[l]);
        shown[l] = false;
    }
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
    ip->setButtonObj(interfaceButton0, buttonRelease, functB1Up);
    ip->setButtonObj(interfaceButton1, buttonRelease, functB2Up);
    ip->setButtonObj(interfaceButton2, buttonRelease, functB3Up);
    ip->setButtonObj(interfaceButton3, buttonRelease, functB4Up);
    ip->setButtonObj(interfaceButtonShift, buttonPress, functShiftDown);
    ip->setButtonObj(interfaceButtonShift, buttonRelease, functShiftUp);
    ip->setButtonObj(interfaceButtonInsert, buttonPress, functJogPress); // the Original has no push on the wheel
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
    // A layer is shown while the application says so, and redrawn when it asks to be
    for (int layer = 0; layer < 6; layer++)
    {
        const bool want = a.layerVisible(layer);
        const bool redraw = a.takeRedraw(layer);
        if (want && !shown[layer])
        {
            shown[layer] = true;
            display.setControlShow(view[layer]);
            display.refreshControl(view[layer]);
            changed = true;
        }
        else if (!want && shown[layer])
        {
            shown[layer] = false;
            display.setControlHide(view[layer]);
            changed = true;
        }
        else if (want && redraw)
        {
            display.refreshControl(view[layer]);
            changed = true;
        }
    }
    if (changed)
    {
        display.synchronizeRefresh();
    }
}
