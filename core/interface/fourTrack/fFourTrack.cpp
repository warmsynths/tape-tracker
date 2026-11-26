#include "fourTrack.h"
#include "mtMidi.h"

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
// B1 to B4 and Shift report both press and release (one handler per button gets the state),
// because the Glitch page holds a slot while the button is down and Shift is a held key.
static void functB1(uint8_t state) { if (state == buttonPress) functKey(ft::kB1); else if (state == buttonRelease) FT->application().keyUp(ft::kB1); }
static void functB2(uint8_t state) { if (state == buttonPress) functKey(ft::kB2); else if (state == buttonRelease) FT->application().keyUp(ft::kB2); }
static void functB3(uint8_t state) { if (state == buttonPress) functKey(ft::kB3); else if (state == buttonRelease) FT->application().keyUp(ft::kB3); }
static void functB4(uint8_t state) { if (state == buttonPress) functKey(ft::kB4); else if (state == buttonRelease) FT->application().keyUp(ft::kB4); }
static void functShift(uint8_t state)
{
    if (state == buttonPress) FT->application().setShift(true);
    else if (state == buttonRelease) FT->application().setShift(false);
}
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
static void functJogPress() { functKey(ft::kJogPress); }
static void functSampleEdit() { functKey(ft::kSampleEdit); }
static void functPerformance() { functKey(ft::kPerformance); }
static void functMaster() { functKey(ft::kMaster); }
static void functFile() { functKey(ft::kFile); }
static void functPattern() { functKey(ft::kPattern); } // home
static void functParams() { functKey(ft::kParams); } // the track mixer
static void functSamplePlay() { functKey(ft::kSamplePlay); } // pad sample settings
static void functConfig() { functKey(ft::kConfig); } // device settings
static void functCopy() { functKey(ft::kCopy); }     // copy a Part (Shift: undo)
static void functDelete() { functKey(ft::kDelete); } // clear a Part (Shift: all)
// Incoming MIDI notes play the pads: C2 (36) is the first of the 48
static void ftMidiNote(uint8_t pitch, uint8_t velocity, bool on)
{
    (void)velocity;
    if (pitch < 36 || pitch >= 36 + 48) return;
    FT->application().pad(pitch - 36, on);
}
static void functSampleRec() { functKey(ft::kSampleRec); } // resample the mix
static void functSampleLoad() { functKey(ft::kSampleLoad); } // load samples from the card
static void functSong() { functKey(ft::kSong); } // the Song button switches the click on or off
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

    ip->setButtonObj(interfaceButtonParams, buttonPress, functParams);
    ip->setButtonObj(interfaceButtonConfig, buttonPress, functConfig);
    ip->setButtonObj(interfaceButtonSamplePlay, buttonPress, functSamplePlay);
    // These four open pages of this screen: Part Editor, Glitch, Lo-fi master, Project
    ip->setButtonObj(interfaceButtonSampleEdit, buttonPress, functSampleEdit);
    ip->setButtonObj(interfaceButtonPerformance, buttonPress, functPerformance);
    ip->setButtonObj(interfaceButtonMaster, buttonPress, functMaster);
    ip->setButtonObj(interfaceButtonFile, buttonPress, functFile);
    ip->setButtonObj(interfaceButtonSampleRec, buttonPress, functSampleRec);
    ip->setButtonObj(interfaceButtonSampleLoad, buttonPress, functSampleLoad);
    ip->setButtonObj(interfaceButtonCopy, buttonPress, functCopy);
    ip->setButtonObj(interfaceButtonDelete, buttonPress, functDelete);
    ip->setButtonObj(interfaceButtonSong, buttonPress, functSong);
    ip->setButtonObj(interfaceButtonPattern, buttonPress, functPattern);

#ifndef TRACKER_MINI
    platform->getRadio()->enableLineInput(false);
#endif

    showScreen();
    setDefaultScreenFunct();
    midiSetNoteHook(ftMidiNote);
    application().enter();
}

void cFourTrack::stop()
{
    midiSetNoteHook(nullptr);
    application().setShift(false);
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

    ip->setButtonObj(interfaceButton0, functB1);
    ip->setButtonObj(interfaceButton1, functB2);
    ip->setButtonObj(interfaceButton2, functB3);
    ip->setButtonObj(interfaceButton3, functB4);
    ip->setButtonObj(interfaceButtonShift, functShift);
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
