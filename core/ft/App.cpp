#include "App.h"

#include "MiniFmt.h"
#include "loop_fill.h"

namespace ft
{
namespace
{
constexpr int kBlock = 128; // frames in one audio block

uint32_t hashBytes(uint32_t h, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++)
    {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

void formatTime(char* out, size_t size, uint32_t frames, uint32_t rate)
{
    const uint32_t tenths = frames * 10 / rate;
    fmt(out, size, "%u:%02u.%u", (unsigned)(tenths / 600), (unsigned)((tenths / 10) % 60), (unsigned)(tenths % 10));
}

float squareRoot(float x) { return __builtin_sqrtf(x); }
} // namespace

App::App(Hal& h, const Palette& palette)
  : hal(h)
  , pal(palette)
  , session(h.sampleRate() * 24)
{
}

const char* App::sourceName(Source source)
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

//--------------------------------------------------------------------------------------------
// Opening and closing the screen

void App::enter()
{
    visibleNow = true;
    appliedSource = -1;
    applySource(trackSource[session.selected()]);
    staticSignature = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
    }
    refresh(true);
}

void App::leave()
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
    hal.playerStop();
    hal.padsReleaseAll();
    promptShown = false;
    visibleNow = false;
    for (int l = 0; l < kLayerCount; l++)
    {
        redraw[l] = false;
    }
}

//--------------------------------------------------------------------------------------------
// Input

void App::key(int k)
{
    switch (k)
    {
        case kRec:
        case kB1: pressRecord(); break;
        case kStop:
        case kB2: pressStop(); break;
        case kPlay:
        case kB3: pressPlay(); break;
        case kB4: togglePadMode(); break;
        case kB5: selectTrack(0); break;
        case kB6: selectTrack(1); break;
        case kB7: selectTrack(2); break;
        case kB8: selectTrack(3); break;
        case kUp: moveSelection(-1); break;
        case kDown: moveSelection(1); break;
        case kLeft: promptMove(-1); break;
        case kRight: promptMove(1); break;
        default: break;
    }
}

void App::jog(int delta)
{
    if (delta != 0)
    {
        moveSelection(delta > 0 ? 1 : -1);
    }
}

void App::pad(int p, bool down)
{
    if (p < 0 || p >= 48)
    {
        return;
    }
    if (down)
    {
        hal.padOn(p, padsKeysMode);
    }
    else
    {
        hal.padOff(p);
    }
}

void App::togglePadMode()
{
    hal.padsReleaseAll(); // let go of anything still sounding from the old mode
    padsKeysMode = !padsKeysMode;
    refresh(true);
}

void App::applySource(Source source)
{
    if (appliedSource == (int)source)
    {
        return;
    }
    appliedSource = (int)source;
    hal.applySource(source);
}

void App::promptMove(int delta)
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars || promptCount == 0)
    {
        return;
    }
    int next = promptSelected + delta;
    if (next < 0) next = 0;
    if (next >= promptCount) next = promptCount - 1;
    promptSelected = next;
    refresh(true);
}

void App::promptConfirm()
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars || promptCount == 0)
    {
        return;
    }
    session.confirmBars(promptBars[promptSelected]);
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
    }
    syncPlayer(); // the loop starts playing
    refresh(true);
}

void App::promptCancel()
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars)
    {
        return;
    }
    session.cancelBars();
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        partData[t] = session.hasPart(t) ? hal.region(t) : nullptr; // other Parts stay as they were
        waveDirty[t] = true;
    }
    syncPlayer();
    refresh(true);
}

void App::moveSelection(int delta)
{
    if (session.mode() == FourTrackSession::Mode::ChoosingBars)
    {
        promptMove(delta); // the jog wheel chooses the bars while the question is open
        return;
    }
    const int track = session.selected() + delta;
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
    refresh(true);
}

void App::selectTrack(int track)
{
    if (session.mode() == FourTrackSession::Mode::ChoosingBars)
    {
        // The buttons under the screen are Cancel (7th) and OK (8th) while the question is open.
        if (track == 2) promptCancel();
        else if (track == 3) promptConfirm();
        return;
    }
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
    refresh(true);
}

void App::pressRecord()
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
        hal.playerStop();
        applySource(trackSource[track]);
        hal.recordStart(track, trackSource[track]);
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        syncPlayer();
    }
    refresh(true);
}

void App::pressStop()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        hal.recordStop();
        session.firstRecordedFrames(hal.recordLength(), hal.recordFull());
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
    refresh(true);
}

void App::pressPlay()
{
    if (session.mode() != FourTrackSession::Mode::Idle)
    {
        return;
    }
    session.pressPlay();
    syncPlayer();
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Recording and playing

void App::cancelRecording()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst ||
        session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        hal.recordStop();
        session.pressStop(); // a first take with nothing kept, or a discarded Part
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        session.pressStop();
    }
}

void App::startPlayer()
{
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        hal.playerSetTrack(t, session.hasPart(t) ? partData[t] : nullptr);
    }
    hal.playerStart(session.loopFrames());
}

void App::syncPlayer()
{
    if (session.playing() && !hal.playerPlaying())
    {
        startPlayer();
    }
    else if (!session.playing() && hal.playerPlaying())
    {
        hal.playerStop();
    }
}

void App::onFirstTakeFinished()
{
    // The first Part defines the loop. It sits at the start of its track's region.
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        partData[t] = nullptr;
    }
    if (session.mode() == FourTrackSession::Mode::ChoosingBars && session.recordingTrack() >= 0)
    {
        // The take is done but its Part only counts once the bars are chosen.
        partData[session.recordingTrack()] = hal.region(session.recordingTrack());
    }
    else if (session.hasPart(session.selected()))
    {
        partData[session.selected()] = hal.region(session.selected());
    }
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
    }
    hal.playerStop(); // so syncPlayer starts it again from the top with the new Parts
}

void App::serviceFirstTake()
{
    const bool stoppedByItself = !hal.recordActive();
    session.firstRecordedFrames(hal.recordLength(), hal.recordFull() || stoppedByItself);
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

void App::serviceArmed()
{
    if (!hal.playerPlaying())
    {
        return;
    }
    // A Part starts recording when the loop is about 200 ms from its end, and not if it is
    // closer than about 50 ms, so the start of the loop is never missed.
    const uint32_t lead = hal.sampleRate() / 5;
    const uint32_t minLead = hal.sampleRate() / 20;
    const uint32_t length = session.loopFrames();
    const uint32_t position = hal.playerPosition();
    if (position + lead >= length && position + minLead <= length)
    {
        applySource(trackSource[session.recordingTrack()]);
        hal.recordStart(session.recordingTrack(), trackSource[session.recordingTrack()]);
        session.armWindowReached();
    }
}

void App::finishPartEarly()
{
    const int track = session.recordingTrack();
    if (!hal.recordStartCaptured())
    {
        hal.recordStop();
        session.pressStop(); // nothing was captured yet
        return;
    }

    const uint32_t length = session.loopFrames();
    const uint32_t firstSample = (length - (hal.recordStartPosition() % length)) % length;
    hal.recordStop();
    const uint32_t recorded = hal.recordLength();
    if (recorded <= firstSample)
    {
        session.pressStop(); // stopped before the loop had started
        return;
    }

    constexpr uint32_t kPostFrames = 6000; // audio kept after the end of the loop
    uint32_t needed = firstSample + length + kPostFrames;
    if (needed > hal.regionFrames())
    {
        needed = hal.regionFrames();
    }
    if (recorded < needed)
    {
        // Everything after the stop point stays silent, not left over from an older take.
        __builtin_memset(hal.region(track) + recorded, 0, (needed - recorded) * sizeof(int16_t));
    }

    partData[track] = hal.region(track) + firstSample;
    waveDirty[track] = true;
    hal.playerSetTrack(track, partData[track]);
    session.partRecorded();
}

void App::servicePartRecording()
{
    if (!hal.recordStartCaptured())
    {
        return;
    }

    constexpr uint32_t kPostFrames = 6000;
    const uint32_t length = session.loopFrames();
    // How far into the recording the loop's first sample falls.
    const uint32_t firstSample = (length - (hal.recordStartPosition() % length)) % length;
    const uint32_t needed = firstSample + length + kPostFrames;

    if (needed > hal.regionFrames() || !hal.recordActive())
    {
        // Too long for the room, or the recorder stopped by itself: drop this take.
        hal.recordStop();
        session.pressStop();
        return;
    }

    if (hal.recordLength() >= needed)
    {
        hal.recordStop();
        const int track = session.recordingTrack();
        partData[track] = hal.region(track) + firstSample;
        waveDirty[track] = true;
        hal.playerSetTrack(track, partData[track]);
        session.partRecorded();
    }
}

//--------------------------------------------------------------------------------------------

void App::tick()
{
    hal.tick();
    switch (session.mode())
    {
        case FourTrackSession::Mode::RecordingFirst: serviceFirstTake(); break;
        case FourTrackSession::Mode::Armed: serviceArmed(); break;
        case FourTrackSession::Mode::RecordingPart: servicePartRecording(); break;
        case FourTrackSession::Mode::Idle:
        case FourTrackSession::Mode::ChoosingBars: break;
    }
    refresh(false);
}

// Level of each track: a playing Part shows what it plays, the track being recorded shows what
// has just been recorded (from any source). Returns true when the levels were updated (about 20
// times a second).
bool App::updateTrackMeters()
{
    const uint32_t now = hal.millis();
    if (now - meterTimer < 50)
    {
        return false;
    }
    meterTimer = now;

    const FourTrackSession::Mode mode = session.mode();
    const bool recordingNow = (mode == FourTrackSession::Mode::RecordingFirst ||
                               mode == FourTrackSession::Mode::RecordingPart);
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        uint16_t peak = 0;
        if (recordingNow && t == session.recordingTrack())
        {
            const uint32_t length = hal.recordLength();
            if (length >= (uint32_t)kBlock)
            {
                peak = loopPeakBlock(hal.region(t) + (length - kBlock), kBlock, 0, kBlock);
            }
        }
        else
        {
            peak = hal.playerTakePeak(t);
        }

        const int db = ampToDb((float)peak / 32768.0f);
        // Fall back slowly, jump up at once.
        trackDb[t] = (db > trackDb[t]) ? db : (trackDb[t] - 3 > db ? trackDb[t] - 3 : db);
        int level = (trackDb[t] + 50) * 2; // -50 dB to 0 dB as 0 to 100
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        model.lane[t].meter = (uint8_t)level;
    }
    return true;
}

// Coarse waveform of a Part: 100 columns, each the peak of its share of the loop.
void App::computeWave(int track)
{
    Lane& lane = model.lane[track];
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
        const uint16_t peak = loopPeakBlock(data + from, to - from, 0, (int)(to - from));
        // Square-root scaling so quiet parts are still visible.
        const float norm = squareRoot((float)peak / 32767.0f);
        const int h = (int)(norm * 255.0f);
        lane.wave[c] = (uint8_t)(h > 255 ? 255 : h);
    }
    lane.waveCount = columns;
}

// Builds everything the screen shows from the current state.
void App::fillView()
{
    const uint32_t rate = hal.sampleRate();
    const FourTrackSession::Mode mode = session.mode();
    const int recTrack = session.recordingTrack();
    const uint32_t loop = session.loopFrames();
    View& v = model;

    fmt(v.project, sizeof(v.project), "Four-Track");
    v.unsaved = false;
    if (session.bars() > 0 && loop > 0)
    {
        const uint32_t tempo = loopTempoTenths(loop, rate, session.bars());
        fmt(v.tempo, sizeof(v.tempo), "%u.%u", (unsigned)(tempo / 10), (unsigned)(tempo % 10));
        fmt(v.bars, sizeof(v.bars), "%d", session.bars());
    }
    else
    {
        fmt(v.tempo, sizeof(v.tempo), "Free");
        fmt(v.bars, sizeof(v.bars), "-");
    }
    fmt(v.input, sizeof(v.input), "%s", sourceName(trackSource[session.selected()]));
    v.monitor = (trackSource[session.selected()] == Source::Line);

    // Left and right input level in dB, to see whether a stereo source arrives evenly.
    {
        static float levelL = 0.0f, levelR = 0.0f;
        float l = 0.0f, r = 0.0f;
        hal.inputLevels(l, r);
        levelL = l;
        levelR = r;
        fmt(v.inputLevels, sizeof(v.inputLevels), "L %d  R %d dB", ampToDb(levelL), ampToDb(levelR));
    }
    v.barCount = (uint8_t)session.bars();
    v.loopFrames = loop;

    // status line
    v.statusNote[0] = 0;
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        v.statusKind = 2;
        fmt(v.status, sizeof(v.status), "Recording");
        fmt(v.statusNote, sizeof(v.statusNote), "No Loop yet");
    }
    else if (mode == FourTrackSession::Mode::RecordingPart)
    {
        v.statusKind = 2;
        fmt(v.status, sizeof(v.status), "Recording");
    }
    else if (mode == FourTrackSession::Mode::Armed)
    {
        v.statusKind = 3;
        fmt(v.status, sizeof(v.status), "Armed");
    }
    else if (session.playing())
    {
        v.statusKind = 1;
        fmt(v.status, sizeof(v.status), "Playing");
    }
    else
    {
        v.statusKind = 0;
        fmt(v.status, sizeof(v.status), "Stopped");
        if (loop == 0)
        {
            fmt(v.statusNote, sizeof(v.statusNote), "No Loop yet");
        }
    }

    // position readout
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        formatTime(v.pos, sizeof(v.pos), hal.recordLength(), rate);
        v.posFrames = 0;
    }
    else if (loop > 0)
    {
        v.posFrames = hal.playerPosition();
        if (session.bars() > 0)
        {
            // bar.beat.sixteenth, 4/4
            const uint32_t sixteenths = (uint32_t)(((uint64_t)v.posFrames * session.bars() * 16) / loop);
            fmt(v.pos, sizeof(v.pos), "%u.%u.%02u", (unsigned)(sixteenths / 16 + 1), (unsigned)((sixteenths % 16) / 4 + 1), (unsigned)(sixteenths % 4 + 1));
        }
        else
        {
            formatTime(v.pos, sizeof(v.pos), v.posFrames, rate);
        }
    }
    else
    {
        v.posFrames = 0;
        fmt(v.pos, sizeof(v.pos), "-");
    }

    // lanes
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        Lane& L = v.lane[t];
        fmt(L.name, sizeof(L.name), "Track %d", t + 1);
        fmt(L.tag, sizeof(L.tag), "%s", sourceName(trackSource[t]));
        L.info[0] = 0;
        L.msg1[0] = 0;
        L.msg2[0] = 0;
        L.hasPart = session.hasPart(t);
        L.muted = false;
        L.glitch = false;
        L.selected = (t == session.selected());
        L.active = (t == recTrack && (mode == FourTrackSession::Mode::RecordingFirst ||
                                      mode == FourTrackSession::Mode::RecordingPart ||
                                      mode == FourTrackSession::Mode::Armed));

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
            fmt(L.msg1, sizeof(L.msg1), "First take - elapsed");
            formatTime(L.msg2, sizeof(L.msg2), hal.recordLength(), rate);
        }
        else if (L.active && mode == FourTrackSession::Mode::Armed)
        {
            fmt(L.msg1, sizeof(L.msg1), "Waits for the loop start");
            const uint32_t pos = hal.playerPosition();
            const uint32_t toEnd = (loop > pos) ? (loop - pos) : 0;
            const uint32_t tenths = toEnd * 10 / rate;
            fmt(L.msg2, sizeof(L.msg2), "Starts in %u.%u s", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
        }
        else if (L.active && mode == FourTrackSession::Mode::RecordingPart)
        {
            fmt(L.msg1, sizeof(L.msg1), "Recording one Loop");
            formatTime(L.msg2, sizeof(L.msg2), hal.playerPosition(), rate);
        }
        else if (!L.hasPart)
        {
            fmt(L.msg2, sizeof(L.msg2), "Empty");
        }
    }

    // the "How many bars?" question
    {
        Prompt& P = v.prompt;
        P.active = (mode == FourTrackSession::Mode::ChoosingBars);
        if (P.active)
        {
            if (!promptShown)
            {
                promptCount = loopBarChoices(session.pendingFrames(), rate, promptBars);
                promptSelected = loopBarSuggestion(session.pendingFrames(), rate, promptBars, promptCount);
            }
            const uint32_t played = session.pendingFrames();
            const uint32_t hundredths = (uint32_t)(((uint64_t)played * 100) / rate);
            P.memoryFull = session.memoryFull();
            P.count = (uint8_t)promptCount;
            P.selected = (uint8_t)promptSelected;
            for (int i = 0; i < promptCount; i++)
            {
                P.bars[i] = (uint8_t)promptBars[i];
                P.bpm10[i] = (uint16_t)loopTempoTenths(played, rate, promptBars[i]);
            }
            fmt(P.played, sizeof(P.played), "You played %u.%02u s. No audio is stretched; only the tempo is worked out.", (unsigned)(hundredths / 100), (unsigned)(hundredths % 100));
            fmt(P.note, sizeof(P.note), "Stopped at %u.%u s. Everything recorded is kept.", (unsigned)(hundredths / 100), (unsigned)((hundredths % 100) / 10));
            const int suggested = loopBarSuggestion(played, rate, promptBars, promptCount);
            fmt(P.suggestion, sizeof(P.suggestion), "%s", (promptSelected == suggested) ? "Suggested: nearest 120 BPM" : "");
        }
    }

    // hint line and mode texts
    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        fmt(v.hint, sizeof(v.hint), "Stop ends the take and keeps it.");
    }
    else if (mode == FourTrackSession::Mode::Armed)
    {
        fmt(v.hint, sizeof(v.hint), "Records one Loop from the loop start. Rec disarms.");
    }
    else if (mode == FourTrackSession::Mode::RecordingPart)
    {
        fmt(v.hint, sizeof(v.hint), "Stop keeps what was recorded, the rest is silence.");
    }
    else if (loop == 0)
    {
        fmt(v.hint, sizeof(v.hint), "Rec records the first Part, which sets the Loop.");
    }
    else
    {
        fmt(v.hint, sizeof(v.hint), "Rec arms Track %d. Press it again to change its source.", session.selected() + 1);
    }
    fmt(v.loopMode, sizeof(v.loopMode), "Repeat");
    fmt(v.padsMode, sizeof(v.padsMode), "%s", padsKeysMode ? "KEYS" : "PAD");

    // buttons
    static const char* const names[8] = { "Rec", "Stop", "Play", "", "Track 1", "Track 2", "Track 3", "Track 4" };
    for (int b = 0; b < 8; b++)
    {
        fmt(v.btn[b], sizeof(v.btn[b]), "%s", names[b]);
        v.btnKind[b] = (b == 0) ? 1 : (b == 1) ? 2 : (b == 2) ? 3 : 0;
    }
    fmt(v.btn[3], sizeof(v.btn[3]), "%s", padsKeysMode ? "Pads:Keys" : "Pads:Pad");
    v.btnSelected = (int8_t)(4 + session.selected());
}

void App::refresh(bool force)
{
    if (!visibleNow)
    {
        return;
    }
    const bool metersUpdated = updateTrackMeters();
    fillView();

    // Rebuild the rarely changing layers only when something they show has changed.
    uint32_t sig = 2166136261u;
    const View& v = model;
    sig = hashBytes(sig, v.project, sizeof(v.project));
    sig = hashBytes(sig, v.tempo, sizeof(v.tempo));
    sig = hashBytes(sig, v.bars, sizeof(v.bars));
    sig = hashBytes(sig, v.input, sizeof(v.input));
    sig = hashBytes(sig, v.status, sizeof(v.status));
    sig = hashBytes(sig, v.statusNote, sizeof(v.statusNote));
    sig = hashBytes(sig, v.hint, sizeof(v.hint));
    sig = hashBytes(sig, v.padsMode, sizeof(v.padsMode));
    sig = hashBytes(sig, v.btn, sizeof(v.btn));
    sig = hashBytes(sig, &v.btnSelected, sizeof(v.btnSelected));
    sig = hashBytes(sig, &v.statusKind, sizeof(v.statusKind));
    sig = hashBytes(sig, &v.monitor, sizeof(v.monitor));
    sig = hashBytes(sig, &v.loopFrames, sizeof(v.loopFrames));
    sig = hashBytes(sig, &v.barCount, sizeof(v.barCount));
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        const Lane& L = v.lane[t];
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

    const bool wantPrompt = (session.mode() == FourTrackSession::Mode::ChoosingBars);
    if (wantPrompt && !promptShown)
    {
        promptShown = true;
        redraw[3] = true;
    }
    else if (!wantPrompt && promptShown)
    {
        promptShown = false;
        redraw[3] = false;
    }
    else if (wantPrompt && force)
    {
        redraw[3] = true;
    }
    if (force || sig != staticSignature)
    {
        staticSignature = sig;
        redraw[0] = true;
        redraw[2] = true;
    }
    if (force || metersUpdated)
    {
        redraw[1] = true;
    }
}

//--------------------------------------------------------------------------------------------
// Drawing

bool App::layerVisible(int layer) const
{
    if (!visibleNow) return false;
    return layer != 3 || promptShown;
}

bool App::takeRedraw(int layer)
{
    if (layer < 0 || layer >= kLayerCount) return false;
    const bool r = redraw[layer];
    redraw[layer] = false;
    return r;
}

void App::draw(int layer, int stage, Gfx& gfx) const
{
    drawLayer(layer, stage, model, pal, gfx);
}
} // namespace ft
