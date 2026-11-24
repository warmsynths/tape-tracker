#include "fourTrack.h"

#include "Platform.h"
#include "Recorder.h"
#include "TrackerImpl.h"
#include "core/interface.h"
#include "core/songTimer.h"
#include "mtAudioEngine.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "loop_fill.h"
#include "mtConfig.h"
#include "mtSequencer.h"
#include "mtStructs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

cFourTrack fourTrack;
static cFourTrack* FT = &fourTrack;

namespace
{
// Sample clock of the Original (see docs/research/record-path-and-latency.md): 44117.647 Hz.
constexpr uint32_t kSampleRate = kFourTrackSampleRate;

// Longest loop, and the room each track gets. The extra room holds the audio recorded just
// before and after the loop, for aligning a Part later.
constexpr uint32_t kMaxLoopFrames = kSampleRate * 24;
constexpr uint32_t kRegionFrames = kFourTrackRegionFrames;
constexpr uint32_t kRegionBytes = kFourTrackRegionBytes;

// A Part starts recording when the loop is this close to its end (about 200 ms), and not if it
// is closer than about 50 ms, so the start of the loop is never missed.
constexpr uint32_t kLeadFrames = kSampleRate / 5;
constexpr uint32_t kMinLeadFrames = kSampleRate / 20;
// Extra audio kept after the end of the loop (about 136 ms).
constexpr uint32_t kPostFrames = 6000;

// Four regions at the very top of the sample memory, one per track. That area is shared with
// the Sample Editor and instrument samples, so do not use them with this test build.
int16_t* regionStart(int track)
{
    return externalRamSampleMemory +
           (SAMPLE_MEMORY_SIZE - (uint32_t)(track + 1) * kRegionBytes) / sizeof(int16_t);
}
} // namespace

static void functRec() { FT->pressRecord(); }
static void functStop() { FT->pressStop(); }
static void functPlay() { FT->pressPlay(); }
static void functPadMode() { FT->togglePadMode(); }
static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    (void)velo;
    FT->padEvent(pad, state);
}
static void functTrack1() { FT->selectTrack(0); }
static void functTrack2() { FT->selectTrack(1); }
static void functTrack3() { FT->selectTrack(2); }
static void functTrack4() { FT->selectTrack(3); }

static void functSwitchModule(uint8_t button)
{
    FT->eventFunct(eventSwitchModule, FT, &button, 0);
}

cFourTrack::cFourTrack()
  : session(kMaxLoopFrames)
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

#ifndef TRACKER_MINI
    platform->getRadio()->enableLineInput(false);
#endif

    recorder.setFullMemoryCallback([]() { FT->memoryFullFlag = true; });
    recorder.setFirstBlockCallback([]() {
        FT->recordStartPosition = engine.getLoopPlayer()->position();
        FT->recordStartKnown = true;
    });

    // Set up the input for the selected track's source (line, mic, or none for pads).
    appliedSource = -1;
    applySource(trackSource[session.selected()]);

    shownLevelText[0] = 0;
    shownStatusText[0] = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        shownTrackText[t][0] = 0;
    }

    showScreen();
    setDefaultScreenFunct();
    refreshTexts(true);
}

void cFourTrack::stop()
{
    // Leave nothing running behind another screen.
    if (session.mode() != FourTrackSession::Mode::Idle)
    {
        pressStop(); // keeps a first take, cancels anything else
    }
    if (session.playing())
    {
        session.pressStop();
    }
    engine.getLoopPlayer()->stop();

    moduleRefresh = 0;
    if (micUnmutePending)
    {
        micUnmutePending = false;
        platform->getCodecAudio()->setHeadphoneMuted(false);
    }
    engine.refreshLineInStreamVolume(); // back to the normal project setting
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        display.setControlHide(trackLabel[t]);
    }
    display.setControlHide(statusLabel);
    display.setControlHide(hintLabel);
    display.setControlHide(levelLabel);
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
    ip->setButtonObj(interfaceButton3, buttonPress, functPadMode);
    ip->setPadsGlobal(functPads);
    ip->setButtonObj(interfaceButton4, buttonPress, functTrack1);
    ip->setButtonObj(interfaceButton5, buttonPress, functTrack2);
    ip->setButtonObj(interfaceButton6, buttonPress, functTrack3);
    ip->setButtonObj(interfaceButton7, buttonPress, functTrack4);
    ip->setButtonObj(interfaceButtonRec, buttonPress, functRec);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlay);
}

//------------------------------------------------------------------------------------------------
// Actions from the buttons

void cFourTrack::togglePadMode()
{
    // Let go of anything still sounding from the old mode.
    mtPadBoard.releaseAllInstruments();
    padsKeysMode = !padsKeysMode;
    display.setControlText(label[3], padsKeysMode ? "Pads: Keys" : "Pads: Pad");
    display.refreshControl(label[3]);
    display.synchronizeRefresh();
}

void cFourTrack::padEvent(uint8_t pad, uint8_t state)
{
    if (pad >= 48)
    {
        return;
    }

    if (state == buttonPress)
    {
        padsBacklight.setFrontLayer(1, mtConfig.values.padsLightFront, pad);

        uint8_t note;
        if (padsKeysMode)
        {
            const bool isBeatSlice =
              mtProject.instrument[mtProject.values.lastUsedInstrument].playMode == playModeBeatSlice;
            note = mtPadBoard.getNoteFromPad(pad, isBeatSlice);
        }
        else
        {
            // Pad N plays sample slot N at its own pitch (the preview note).
            mtProject.values.lastUsedInstrument = pad;
            note = Sequencer::STEP_NOTE_DEFAULT;
        }
        padNote[pad] = note;
        sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, note, 127, pad);
    }
    else if (state == buttonRelease)
    {
        padsBacklight.setFrontLayer(0, 0, pad);
        sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, padNote[pad], 0, pad);
    }
}

const char* cFourTrack::sourceName(Source source)
{
    switch (source)
    {
        case Source::Line: return "LINE";
        case Source::MicLow: return "MIC LOW";
        case Source::MicHigh: return "MIC HIGH";
        case Source::Pads: return "PADS";
    }
    return "";
}

// Sets up the audio input for a source. Does nothing if it is already set up.
void cFourTrack::applySource(Source source)
{
    if (appliedSource == (int)source)
    {
        return;
    }
    appliedSource = (int)source;

    Sgtl5000* codec = platform->getCodecAudio();
    switch (source)
    {
        case Source::Line:
            codec->setInputType(Sgtl5000::InputType::kLineIn);
            // Same input gain as the Sample Recorder screen (set it there; 0 to 100 percent).
            codec->setLineInLevel(mtProject.values.gainLineIn * 15 / 100);
            // Hear the input while recording, as the stock Sample Recorder does.
            engine.forceLineInStreamVolume(DEFAULT_LINE_IN_VOLUME);
            break;
        case Source::MicLow:
        case Source::MicHigh:
            // Mute the headphones briefly while switching, so there is no pop (as the stock
            // Sample Recorder does). Mic monitoring starts off.
            codec->setHeadphoneMuted(true);
            codec->setInputType(Sgtl5000::InputType::kMic);
            codec->setMicGain(source == Source::MicLow ? mtProject.values.gainMicLow * 25.0f / 100.0f
                                                       : mtProject.values.gainMicHigh * 44.0f / 100.0f);
            micTimer = 0;
            micUnmutePending = true;
            engine.forceLineInStreamVolume(0);
            break;
        case Source::Pads:
            // Records the pads, not the input: do not hear the line input meanwhile.
            engine.forceLineInStreamVolume(0);
            break;
    }
}

void cFourTrack::selectTrack(int track)
{
    const bool idle = (session.mode() == FourTrackSession::Mode::Idle);
    if (idle && track >= 0 && track < FourTrackSession::kTracks && track == session.selected())
    {
        // Same track again: next source.
        trackSource[track] = (Source)(((int)trackSource[track] + 1) % 4);
    }
    else
    {
        session.selectTrack(track);
    }
    if (idle)
    {
        applySource(trackSource[session.selected()]);
    }
    refreshTexts(true);
}

void cFourTrack::pressRecord()
{
    const FourTrackSession::Mode before = session.mode();
    session.pressRecord();
    if (before != FourTrackSession::Mode::Idle)
    {
        return;
    }

    const int track = session.recordingTrack();
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        engine.getLoopPlayer()->stop();
        memoryFullFlag = false;
        configureRecorder(track);
        recorder.record();
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        syncPlayer();
    }
    refreshTexts(true);
}

void cFourTrack::pressStop()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        recorder.stop();
        session.firstRecordedFrames(recorder.getLength(), memoryFullFlag);
        if (session.mode() == FourTrackSession::Mode::RecordingFirst)
        {
            session.pressStop();
        }
        onFirstTakeFinished();
    }
    else if (session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        finishPartEarly(); // keep what was played up to now, the rest of the loop stays silent
    }
    else if (session.mode() != FourTrackSession::Mode::Idle)
    {
        cancelRecording();
    }
    else
    {
        session.pressStop();
    }
    syncPlayer();
    refreshTexts(true);
}

void cFourTrack::pressPlay()
{
    if (session.mode() != FourTrackSession::Mode::Idle)
    {
        return;
    }
    session.pressPlay();
    syncPlayer();
    refreshTexts(true);
}

//------------------------------------------------------------------------------------------------
// Recording and playing

void cFourTrack::configureRecorder(int track)
{
    Recorder::Config config;
    config.destinationBufferAddress = regionStart(track);
    config.destinationBufferSize = kRegionBytes;
    config.destinationChannels = 1;
    if (trackSource[track] == Source::Pads)
    {
        config.sourceChannels = 2;
        config.source = Recorder::Config::Source::kPadBus;
    }
    else
    {
        config.sourceChannels = 1;
        config.source = Recorder::Config::Source::kI2s;
    }
    applySource(trackSource[track]);
    recorder.setConfig(config);
}

void cFourTrack::cancelRecording()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        recorder.stop();
        session.pressStop(); // a first take with nothing kept
    }
    else if (session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        recorder.stop();
        session.pressStop();
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        session.pressStop();
    }
}

void cFourTrack::startPlayer()
{
    AudioPlayLoops* player = engine.getLoopPlayer();
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        player->setTrack(t, session.hasPart(t) ? partData[t] : nullptr);
    }
    player->start(session.loopFrames());
}

void cFourTrack::syncPlayer()
{
    AudioPlayLoops* player = engine.getLoopPlayer();
    if (session.playing() && !player->isPlaying())
    {
        startPlayer();
    }
    else if (!session.playing() && player->isPlaying())
    {
        player->stop();
    }
}

void cFourTrack::onFirstTakeFinished()
{
    // The first Part defines the loop. It sits at the start of its track's region.
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        partData[t] = nullptr;
    }
    if (session.hasPart(session.selected()))
    {
        partData[session.selected()] = regionStart(session.selected());
    }

    AudioPlayLoops* player = engine.getLoopPlayer();
    player->stop(); // so syncPlayer starts it again from the top with the new Parts
}

void cFourTrack::serviceFirstTake()
{
    const bool stoppedByItself = !recorder.isRecordingActive();
    session.firstRecordedFrames(recorder.getLength(), memoryFullFlag || stoppedByItself);
    if (session.mode() == FourTrackSession::Mode::RecordingFirst && stoppedByItself)
    {
        session.pressStop();
    }
    if (session.mode() != FourTrackSession::Mode::RecordingFirst)
    {
        onFirstTakeFinished();
        syncPlayer();
    }
}

void cFourTrack::serviceArmed()
{
    AudioPlayLoops* player = engine.getLoopPlayer();
    if (!player->isPlaying())
    {
        return;
    }

    const uint32_t length = session.loopFrames();
    const uint32_t position = player->position();
    if (position + kLeadFrames >= length && position + kMinLeadFrames <= length)
    {
        configureRecorder(session.recordingTrack());
        recordStartKnown = false;
        memoryFullFlag = false;
        recorder.record();
        session.armWindowReached();
    }
}

void cFourTrack::finishPartEarly()
{
    const int track = session.recordingTrack();
    if (!recordStartKnown)
    {
        recorder.stop();
        session.pressStop(); // nothing was captured yet
        return;
    }

    const uint32_t length = session.loopFrames();
    const uint32_t firstSample = (length - (recordStartPosition % length)) % length;
    recorder.stop();
    const uint32_t recorded = recorder.getLength();
    if (recorded <= firstSample)
    {
        session.pressStop(); // stopped before the loop had started
        return;
    }

    uint32_t needed = firstSample + length + kPostFrames;
    if (needed > kRegionFrames)
    {
        needed = kRegionFrames;
    }
    if (recorded < needed)
    {
        // Everything after the stop point stays silent, not left over from an older take.
        std::memset(regionStart(track) + recorded, 0, (needed - recorded) * sizeof(int16_t));
    }

    partData[track] = regionStart(track) + firstSample;
    engine.getLoopPlayer()->setTrack(track, partData[track]);
    session.partRecorded();
}

void cFourTrack::servicePartRecording()
{
    if (!recordStartKnown)
    {
        return;
    }

    const uint32_t length = session.loopFrames();
    const uint32_t start = recordStartPosition;
    // How far into the recording the loop's first sample falls.
    const uint32_t firstSample = (length - (start % length)) % length;
    const uint32_t needed = firstSample + length + kPostFrames;

    if (needed > kRegionFrames || !recorder.isRecordingActive())
    {
        // Too long for the room, or the recorder stopped by itself: drop this take.
        recorder.stop();
        session.pressStop();
        return;
    }

    if (recorder.getLength() >= needed)
    {
        recorder.stop();
        const int track = session.recordingTrack();
        partData[track] = regionStart(track) + firstSample;
        engine.getLoopPlayer()->setTrack(track, partData[track]);
        session.partRecorded();
    }
}

//------------------------------------------------------------------------------------------------

void cFourTrack::update()
{
    if (micUnmutePending && micTimer > 200)
    {
        micUnmutePending = false;
        platform->getCodecAudio()->setHeadphoneMuted(false);
    }
    switch (session.mode())
    {
        case FourTrackSession::Mode::RecordingFirst: serviceFirstTake(); break;
        case FourTrackSession::Mode::Armed: serviceArmed(); break;
        case FourTrackSession::Mode::RecordingPart: servicePartRecording(); break;
        case FourTrackSession::Mode::Idle: break;
    }

    refreshTexts(false);
    refreshLevel();
}

// Level of each track as a short text bar: a playing Part shows what it plays, the track being
// recorded shows what has just been recorded (from any source). Updated about 20 times a second.
void cFourTrack::updateTrackMeters()
{
    if (meterTimer < 50 && trackBar[0][0] != 0)
    {
        return;
    }
    meterTimer = 0;

    const FourTrackSession::Mode mode = session.mode();
    const bool recordingNow = (mode == FourTrackSession::Mode::RecordingFirst ||
                               mode == FourTrackSession::Mode::RecordingPart);
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        uint16_t peak = 0;
        if (recordingNow && t == session.recordingTrack())
        {
            const uint32_t length = recorder.getLength();
            if (length >= AUDIO_BLOCK_SAMPLES)
            {
                const int16_t* last = regionStart(t) + (length - AUDIO_BLOCK_SAMPLES);
                peak = loopPeakBlock(last, AUDIO_BLOCK_SAMPLES, 0, AUDIO_BLOCK_SAMPLES);
            }
        }
        else
        {
            peak = engine.getLoopPlayer()->takePeak(t);
        }

        int db = -60;
        if (peak > 32)
        {
            db = (int)(20.0f * std::log10((float)peak / 32768.0f));
            if (db < -60) db = -60;
        }
        // Fall back slowly, jump up at once.
        trackDb[t] = (db > trackDb[t]) ? db : (trackDb[t] - 3 > db ? trackDb[t] - 3 : db);

        int filled = (trackDb[t] + 50) / 5; // -50 dB to 0 dB in ten steps
        if (filled < 0) filled = 0;
        if (filled > 10) filled = 10;
        for (int i = 0; i < 10; i++)
        {
            trackBar[t][i] = (i < filled) ? '|' : '.';
        }
        trackBar[t][10] = 0;
    }
}

void cFourTrack::refreshTexts(bool force)
{
    updateTrackMeters();
    bool changed = force;
    const int recTrack = session.recordingTrack();
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        const char* state = session.hasPart(t) ? "PART" : "EMPTY";
        if (t == recTrack)
        {
            switch (session.mode())
            {
                case FourTrackSession::Mode::RecordingFirst:
                case FourTrackSession::Mode::RecordingPart: state = "RECORDING"; break;
                case FourTrackSession::Mode::Armed: state = "ARMED, waiting for the loop"; break;
                case FourTrackSession::Mode::Idle: break;
            }
        }
        std::snprintf(trackText[t],
                      sizeof(trackText[t]),
                      "%s Track %d [%s] %s  %s",
                      (t == session.selected()) ? ">" : " ",
                      t + 1,
                      sourceName(trackSource[t]),
                      trackBar[t],
                      state);
        if (force || std::strcmp(trackText[t], shownTrackText[t]) != 0)
        {
            std::strcpy(shownTrackText[t], trackText[t]);
            display.setControlText(trackLabel[t], shownTrackText[t]);
            display.setControlColors(trackLabel[t],
                                     (t == session.selected())
                                       ? display.getTheme().activeButtonLabelsColors
                                       : display.getTheme().activeLabelsColors);
            display.refreshControl(trackLabel[t]);
            changed = true;
        }
    }

    const uint32_t frames = (session.mode() == FourTrackSession::Mode::RecordingFirst)
                              ? recorder.getLength()
                              : session.loopFrames();
    const uint32_t tenths = frames * 10 / kSampleRate;
    if (frames == 0)
    {
        std::snprintf(statusText, sizeof(statusText), "No loop yet");
    }
    else
    {
        std::snprintf(statusText,
                      sizeof(statusText),
                      "%s %u.%u s  %s",
                      (session.mode() == FourTrackSession::Mode::RecordingFirst) ? "RECORDING" : "LOOP",
                      (unsigned)(tenths / 10),
                      (unsigned)(tenths % 10),
                      session.playing() ? "PLAYING" : "STOPPED");
    }
    if (force || std::strcmp(statusText, shownStatusText) != 0)
    {
        std::strcpy(shownStatusText, statusText);
        display.setControlText(statusLabel, shownStatusText);
        display.refreshControl(statusLabel);
        changed = true;
    }
    if (changed)
    {
        display.synchronizeRefresh();
    }
}

void cFourTrack::refreshLevel()
{
    // Input level as a text bar from -60 dB to 0 dB. Integer formatting only.
    AudioAnalyzeRMS* rms = engine.getAudioInput()->getRecordingRmsI2s(0);
    static float level = 0.0f;
    if (rms != nullptr && rms->available())
    {
        level = rms->read();
    }
    int db = -60;
    if (level > 0.001f)
    {
        db = (int)(20.0f * std::log10(level));
        if (db < -60) db = -60;
        if (db > 0) db = 0;
    }
    const int filled = (db + 60) / 3; // 0..20
    char bar[24];
    for (int i = 0; i < 20; i++)
    {
        bar[i] = (i < filled) ? '|' : '.';
    }
    bar[20] = 0;
    std::snprintf(levelText, sizeof(levelText), "IN  %s  %d dB", bar, db);
    if (std::strcmp(levelText, shownLevelText) != 0)
    {
        std::strcpy(shownLevelText, levelText);
        display.setControlText(levelLabel, shownLevelText);
        display.refreshControl(levelLabel);
        display.synchronizeRefresh();
    }
}
