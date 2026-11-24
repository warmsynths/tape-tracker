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
static void functUp() { FT->moveSelection(-1); }
static void functDown() { FT->moveSelection(1); }
static void functEncoder(int16_t value)
{
    if (value != 0)
    {
        FT->moveSelection(value > 0 ? 1 : -1);
    }
}
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

    staticSignature = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
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
    display.setControlHide(viewStatic);
    display.setControlHide(viewDynamic);
    display.setControlHide(viewFooter);
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
    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);
    ip->setPotObj(interfacePot0, functEncoder);
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
    refreshTexts(true);
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

void cFourTrack::moveSelection(int delta)
{
    int track = session.selected() + delta;
    if (track < 0 || track >= FourTrackSession::kTracks)
    {
        return;
    }
    const bool idle = (session.mode() == FourTrackSession::Mode::Idle);
    session.selectTrack(track);
    if (idle)
    {
        applySource(trackSource[session.selected()]);
    }
    refreshTexts(true);
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
        // Both input channels, as the stock Sample Recorder does for a mono recording. The Config
        // setting "Line input" decides what ends up on the track: Stereo is summed to mono, Mono
        // Left and Mono Right use that channel.
        config.sourceChannels = 2;
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
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
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
    waveDirty[track] = true;
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
        waveDirty[track] = true;
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
}

// Level of each track: a playing Part shows what it plays, the track being recorded shows what
// has just been recorded (from any source). Returns true when the levels were updated (about 20
// times a second).
bool cFourTrack::updateTrackMeters()
{
    if (meterTimer < 50)
    {
        return false;
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
        int level = (trackDb[t] + 50) * 2; // -50 dB to 0 dB as 0 to 100
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        view.lane[t].meter = (uint8_t)level;
    }
    return true;
}

// Coarse waveform of a Part: 100 columns, each the peak of its share of the loop.
void cFourTrack::computeWave(int track)
{
    strFtLane& lane = view.lane[track];
    const int16_t* data = partData[track];
    const uint32_t frames = session.loopFrames();
    if (data == nullptr || frames == 0)
    {
        lane.waveCount = 0;
        return;
    }
    const int columns = 100;
    for (int c = 0; c < columns; c++)
    {
        const uint32_t from = (uint32_t)(((uint64_t)c * frames) / columns);
        const uint32_t to = (uint32_t)(((uint64_t)(c + 1) * frames) / columns);
        uint16_t peak = loopPeakBlock(data + from, to - from, 0, (int)(to - from));
        // Square-root scaling so quiet parts are still visible.
        const float norm = std::sqrt((float)peak / 32767.0f);
        int h = (int)(norm * 255.0f);
        lane.wave[c] = (uint8_t)(h > 255 ? 255 : h);
    }
    lane.waveCount = columns;
}

static uint32_t hashBytes(uint32_t h, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++)
    {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static void formatTime(char* out, size_t size, uint32_t frames, uint32_t rate)
{
    const uint32_t tenths = frames * 10 / rate;
    std::snprintf(out, size, "%u:%02u.%u", (unsigned)(tenths / 600), (unsigned)((tenths / 10) % 60), (unsigned)(tenths % 10));
}

// Builds everything the screen shows from the current state.
void cFourTrack::fillView()
{
    const FourTrackSession::Mode mode = session.mode();
    const int recTrack = session.recordingTrack();
    const uint32_t loop = session.loopFrames();

    std::snprintf(view.project, sizeof(view.project), "Four-Track");
    view.unsaved = false;
    std::snprintf(view.tempo, sizeof(view.tempo), "Free");
    std::snprintf(view.bars, sizeof(view.bars), "-");
    std::snprintf(view.input, sizeof(view.input), "%s", sourceName(trackSource[session.selected()]));
    view.monitor = (trackSource[session.selected()] == Source::Line);
    view.barCount = 0; // bars are chosen in a later step (the "How many bars?" screen)
    view.loopFrames = loop;

    // status line
    view.statusNote[0] = 0;
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        view.statusKind = 2;
        std::snprintf(view.status, sizeof(view.status), "Recording");
        std::snprintf(view.statusNote, sizeof(view.statusNote), "No Loop yet");
    }
    else if (mode == FourTrackSession::Mode::RecordingPart)
    {
        view.statusKind = 2;
        std::snprintf(view.status, sizeof(view.status), "Recording");
    }
    else if (mode == FourTrackSession::Mode::Armed)
    {
        view.statusKind = 3;
        std::snprintf(view.status, sizeof(view.status), "Armed");
    }
    else if (session.playing())
    {
        view.statusKind = 1;
        std::snprintf(view.status, sizeof(view.status), "Playing");
    }
    else
    {
        view.statusKind = 0;
        std::snprintf(view.status, sizeof(view.status), "Stopped");
        if (loop == 0)
        {
            std::snprintf(view.statusNote, sizeof(view.statusNote), "No Loop yet");
        }
    }

    // position readout
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        formatTime(view.pos, sizeof(view.pos), recorder.getLength(), kSampleRate);
        view.posFrames = 0;
    }
    else if (loop > 0)
    {
        view.posFrames = engine.getLoopPlayer()->position();
        formatTime(view.pos, sizeof(view.pos), view.posFrames, kSampleRate);
    }
    else
    {
        view.posFrames = 0;
        std::snprintf(view.pos, sizeof(view.pos), "-");
    }

    // lanes
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        strFtLane& L = view.lane[t];
        std::snprintf(L.name, sizeof(L.name), "Track %d", t + 1);
        std::snprintf(L.tag, sizeof(L.tag), "%s", sourceName(trackSource[t]));
        L.info[0] = 0;
        L.msg1[0] = 0;
        L.msg2[0] = 0;
        L.hasPart = session.hasPart(t);
        L.muted = false;
        L.glitch = false;
        L.selected = (t == session.selected());
        L.active = (t == recTrack && mode != FourTrackSession::Mode::Idle);

        if (waveDirty[t])
        {
            waveDirty[t] = false;
            computeWave(t);
            waveVersion[t]++;
        }
        if (!L.hasPart)
        {
            L.waveCount = 0;
        }

        if (L.active && mode == FourTrackSession::Mode::RecordingFirst)
        {
            std::snprintf(L.msg1, sizeof(L.msg1), "First take - elapsed");
            formatTime(L.msg2, sizeof(L.msg2), recorder.getLength(), kSampleRate);
        }
        else if (L.active && mode == FourTrackSession::Mode::Armed)
        {
            std::snprintf(L.msg1, sizeof(L.msg1), "Waits for the loop start");
            const uint32_t pos = engine.getLoopPlayer()->position();
            const uint32_t toEnd = (loop > pos) ? (loop - pos) : 0;
            const uint32_t tenths = toEnd * 10 / kSampleRate;
            std::snprintf(L.msg2, sizeof(L.msg2), "Starts in %u.%u s", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
        }
        else if (L.active && mode == FourTrackSession::Mode::RecordingPart)
        {
            std::snprintf(L.msg1, sizeof(L.msg1), "Recording one Loop");
            const uint32_t pos = engine.getLoopPlayer()->position();
            formatTime(L.msg2, sizeof(L.msg2), pos, kSampleRate);
        }
        else if (!L.hasPart)
        {
            std::snprintf(L.msg2, sizeof(L.msg2), "Empty");
        }
    }

    // hint line and mode texts
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        std::snprintf(view.hint, sizeof(view.hint), "Stop ends the take and keeps it.");
    }
    else if (mode == FourTrackSession::Mode::Armed)
    {
        std::snprintf(view.hint, sizeof(view.hint), "Records one Loop from the loop start. Rec disarms.");
    }
    else if (mode == FourTrackSession::Mode::RecordingPart)
    {
        std::snprintf(view.hint, sizeof(view.hint), "Stop keeps what was recorded, the rest is silence.");
    }
    else if (loop == 0)
    {
        std::snprintf(view.hint, sizeof(view.hint), "Rec records the first Part, which sets the Loop.");
    }
    else
    {
        std::snprintf(view.hint, sizeof(view.hint), "Rec arms Track %d. Press it again to change its source.", session.selected() + 1);
    }
    std::snprintf(view.loopMode, sizeof(view.loopMode), "Repeat");
    std::snprintf(view.padsMode, sizeof(view.padsMode), "%s", padsKeysMode ? "KEYS" : "PAD");

    // buttons
    static const char* const names[8] = { "Rec", "Stop", "Play", "", "Track 1", "Track 2", "Track 3", "Track 4" };
    for (int b = 0; b < 8; b++)
    {
        std::snprintf(view.btn[b], sizeof(view.btn[b]), "%s", names[b]);
        view.btnKind[b] = (b == 0) ? 1 : (b == 1) ? 2 : (b == 2) ? 3 : 0;
    }
    std::snprintf(view.btn[3], sizeof(view.btn[3]), "%s", padsKeysMode ? "Pads:Keys" : "Pads:Pad");
    view.btnSelected = (int8_t)(4 + session.selected());
}

void cFourTrack::refreshTexts(bool force)
{
    const bool metersUpdated = updateTrackMeters();
    fillView();

    // Rebuild the rarely changing layer only when something it shows has changed.
    uint32_t sig = 2166136261u;
    sig = hashBytes(sig, view.project, sizeof(view.project));
    sig = hashBytes(sig, view.tempo, sizeof(view.tempo));
    sig = hashBytes(sig, view.input, sizeof(view.input));
    sig = hashBytes(sig, view.status, sizeof(view.status));
    sig = hashBytes(sig, view.statusNote, sizeof(view.statusNote));
    sig = hashBytes(sig, view.hint, sizeof(view.hint));
    sig = hashBytes(sig, view.padsMode, sizeof(view.padsMode));
    sig = hashBytes(sig, view.btn, sizeof(view.btn));
    sig = hashBytes(sig, &view.btnSelected, sizeof(view.btnSelected));
    sig = hashBytes(sig, &view.statusKind, sizeof(view.statusKind));
    sig = hashBytes(sig, &view.monitor, sizeof(view.monitor));
    sig = hashBytes(sig, &view.loopFrames, sizeof(view.loopFrames));
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        const strFtLane& L = view.lane[t];
        sig = hashBytes(sig, L.name, sizeof(L.name));
        sig = hashBytes(sig, L.tag, sizeof(L.tag));
        sig = hashBytes(sig, L.msg1, sizeof(L.msg1));
        if (!L.active)
        {
            sig = hashBytes(sig, L.msg2, sizeof(L.msg2));
        }
        sig = hashBytes(sig, &L.hasPart, sizeof(L.hasPart));
        sig = hashBytes(sig, &L.selected, sizeof(L.selected));
        sig = hashBytes(sig, &L.active, sizeof(L.active));
        sig = hashBytes(sig, &L.muted, sizeof(L.muted));
        sig = hashBytes(sig, &L.glitch, sizeof(L.glitch));
        sig = hashBytes(sig, &waveVersion[t], sizeof(waveVersion[t]));
    }

    bool changed = false;
    if (force || sig != staticSignature)
    {
        staticSignature = sig;
        display.refreshControl(viewStatic);
        display.refreshControl(viewFooter);
        changed = true;
    }
    if (force || metersUpdated)
    {
        display.refreshControl(viewDynamic);
        changed = true;
    }
    if (changed)
    {
        display.synchronizeRefresh();
    }
}
