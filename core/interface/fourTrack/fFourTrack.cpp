#include "fourTrack.h"

#include "Platform.h"
#include "Recorder.h"
#include "TrackerImpl.h"
#include "core/interface.h"
#include "core/songTimer.h"
#include "mtAudioEngine.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "mtStructs.h"

#include <cstdio>
#include <cstring>

cFourTrack fourTrack;
static cFourTrack* FT = &fourTrack;

namespace
{
// Sample clock of the Original (see docs/research/record-path-and-latency.md): 44117.647 Hz.
constexpr uint32_t kSampleRate = 44118;
constexpr uint32_t kMaxSeconds = 30;
constexpr uint32_t kMaxFrames = kSampleRate * kMaxSeconds;
constexpr uint32_t kRegionBytes = kMaxFrames * sizeof(int16_t);

// The Part lives at the very top of the sample memory. That area is shared with the Sample
// Editor's buffers, so do not use the Sample Editor with this test build.
int16_t* regionStart()
{
    return externalRamSampleMemory + (SAMPLE_MEMORY_SIZE - kRegionBytes) / sizeof(int16_t);
}
} // namespace

static void functRec()
{
    FT->pressRecord();
}

static void functStop()
{
    FT->pressStop();
}

static void functPlay()
{
    FT->pressPlay();
}

static void functPlayButton()
{
    // Hardware Play button: starts the loop when ready, stops it (or the recording) otherwise.
    FT->pressPlay();
}

static void functSwitchModule(uint8_t button)
{
    FT->eventFunct(eventSwitchModule, FT, &button, 0);
}

cFourTrack::cFourTrack()
  : loop(kMaxFrames)
{
}

void cFourTrack::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;
    memoryFullFlag = false;

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

    // Line input, as the stock Sample Recorder sets it up.
    Sgtl5000* codec = platform->getCodecAudio();
    codec->setInputType(Sgtl5000::InputType::kLineIn);
    // Same input gain as the Sample Recorder screen (set it there; 0 to 100 percent).
    codec->setLineInLevel(mtProject.values.gainLineIn * 15 / 100);
#ifndef TRACKER_MINI
    platform->getRadio()->enableLineInput(false);
#endif

    recorder.setFullMemoryCallback([]() { FT->memoryFullFlag = true; });

    showScreen();
    setDefaultScreenFunct();
    refreshStatus(true);
}

void cFourTrack::stop()
{
    // Leave nothing running behind another screen.
    if (loop.state() == FourTrackLoop::State::Recording)
    {
        recorder.stop();
        loop.pressStop();
    }
    if (loop.state() == FourTrackLoop::State::Playing)
    {
        engine.getLoopPlayer()->stop();
        loop.pressStop();
    }

    moduleRefresh = 0;
    display.setControlHide(statusLabel);
    display.setControlHide(hintLabel);
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
    songTimer.show();
}

void cFourTrack::setDefaultScreenFunct()
{
    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    ip->clearAllPots();

    ip->setButtonObj(interfaceButton0, buttonPress, functRec);
    ip->setButtonObj(interfaceButton1, buttonPress, functStop);
    ip->setButtonObj(interfaceButton2, buttonPress, functPlay);
    ip->setButtonObj(interfaceButtonRec, buttonPress, functRec);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayButton);
}

void cFourTrack::pressRecord()
{
    if (loop.state() == FourTrackLoop::State::Recording)
    {
        return;
    }
    if (loop.state() == FourTrackLoop::State::Playing)
    {
        engine.getLoopPlayer()->stop();
    }

    memoryFullFlag = false;

    Recorder::Config config;
    config.destinationBufferAddress = regionStart();
    config.destinationBufferSize = kRegionBytes;
    config.destinationChannels = 1;
    config.sourceChannels = 1;
    config.useRightChannelForMono = false;
    config.source = Recorder::Config::Source::kI2s;

    recorder.setConfig(config);
    recorder.record();
    loop.pressRecord();
    refreshStatus(true);
}

void cFourTrack::pressStop()
{
    if (loop.state() == FourTrackLoop::State::Recording)
    {
        recorder.stop();
        loop.recordedFrames(recorder.getLength(), memoryFullFlag);
        if (loop.state() == FourTrackLoop::State::Recording)
        {
            loop.pressStop();
        }
    }
    else if (loop.state() == FourTrackLoop::State::Playing)
    {
        engine.getLoopPlayer()->stop();
        loop.pressStop();
    }
    refreshStatus(true);
}

void cFourTrack::pressPlay()
{
    if (loop.state() == FourTrackLoop::State::Ready)
    {
        engine.getLoopPlayer()->play(regionStart(), loop.lengthFrames());
        loop.pressPlay();
    }
    else if (loop.state() == FourTrackLoop::State::Playing ||
             loop.state() == FourTrackLoop::State::Recording)
    {
        pressStop();
        return;
    }
    refreshStatus(true);
}

void cFourTrack::finishRecordingIfNeeded()
{
    if (loop.state() != FourTrackLoop::State::Recording)
    {
        return;
    }

    // The recorder can stop by itself when the memory is full.
    const bool stoppedByItself = !recorder.isRecordingActive();
    loop.recordedFrames(recorder.getLength(), memoryFullFlag || stoppedByItself);
    if (loop.state() == FourTrackLoop::State::Recording && stoppedByItself)
    {
        loop.pressStop();
    }
}

void cFourTrack::update()
{
    finishRecordingIfNeeded();
    refreshStatus(false);
}

void cFourTrack::refreshStatus(bool force)
{
    const float seconds = (loop.state() == FourTrackLoop::State::Recording)
                            ? recorder.getLength() / (float)kSampleRate
                            : loop.lengthFrames() / (float)kSampleRate;

    switch (loop.state())
    {
        case FourTrackLoop::State::Empty: std::snprintf(statusText, sizeof(statusText), "EMPTY"); break;
        case FourTrackLoop::State::Recording:
            std::snprintf(statusText, sizeof(statusText), "RECORDING  %.1f s", (double)seconds);
            break;
        case FourTrackLoop::State::Ready:
            std::snprintf(statusText,
                          sizeof(statusText),
                          loop.memoryFull() ? "READY  %.1f s  (memory full)" : "READY  %.1f s",
                          (double)seconds);
            break;
        case FourTrackLoop::State::Playing:
            std::snprintf(statusText, sizeof(statusText), "LOOPING  %.1f s", (double)seconds);
            break;
    }

    if (force || std::strcmp(statusText, shownText) != 0)
    {
        std::strcpy(shownText, statusText);
        display.setControlText(statusLabel, shownText);
        display.refreshControl(statusLabel);
        display.synchronizeRefresh();
    }
}
