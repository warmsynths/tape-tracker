#include "App.h"

#include "MiniFmt.h"
#include "loop_fill.h"

namespace ft
{
namespace
{
constexpr int kBlock = 128;
constexpr uint32_t kPostFrames = 6000; // audio kept after the end of the loop (about 136 ms)
const uint16_t kWindowMs[5] = { 0, 30, 60, 100, 150 };
const uint16_t kPreRollMs[5] = { 0, 60, 120, 250, 500 };

uint32_t hashBytes(uint32_t h, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
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
    for (int i = 0; i < 48; i++) padModeKeys[i] = true;
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        FxSlot defaults[4] = { { FxType::RandomJumps, false, 50, 0, 50 }, { FxType::PitchSteps, false, 50, 0, 40 }, { FxType::Stutter, false, 50, 7, 100 }, { FxType::Reverse, false, 50, 7, 100 } };
        for (int s = 0; s < 4; s++) hal.bank().cfg[t].slot[s] = defaults[s];
    }
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

uint32_t App::windowFrames() const { return hal.sampleRate() * kWindowMs[set.windowIdx] / 1000; }
uint32_t App::preRollFrames() const { return hal.sampleRate() * kPreRollMs[set.preRollIdx] / 1000; }

//--------------------------------------------------------------------------------------------
// Opening and closing the screen

void App::dirtyAll()
{
    for (int l = 0; l < 6; l++) redraw[l] = false;
    redraw[0] = redraw[1] = redraw[2] = true;
    redraw[4] = true;
    staticSignature = 0;
    pageSignature = 0;
}

void App::enter()
{
    visibleNow = true;
    current = Screen::Main;
    appliedSource = -1;
    applySource(trackSource[session.selected()]);
    hal.setMonitor(monitorFor(trackSource[session.selected()]));
    staticSignature = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++) waveDirty[t] = true;
    pushBank();
    refresh(true);
}

void App::leave()
{
    if (job != Job::None) job = Job::None;
    countingIn = false;
    moveMode = false;
    actionMenu = false;
    if (waitingSound) cancelWait();
    if (session.mode() != FourTrackSession::Mode::Idle) pressStop();
    if (session.playing()) session.pressStop();
    hal.playerStop();
    hal.padsReleaseAll();
    promptShown = false;
    visibleNow = false;
    for (int l = 0; l < 6; l++) redraw[l] = false;
}

//--------------------------------------------------------------------------------------------
// Input

void App::key(int k)
{
    if (popupKind != Popup::None)
    {
        popupKey(k);
        return;
    }
    if (k == kPattern) // the Pattern button is the home screen: it leaves a page or a menu
    {
        actionMenu = false;
        if (moveMode) moveKey(kB8);
        if (current != Screen::Main) back();
        else refresh(true);
        return;
    }
    if (current != Screen::Main)
    {
        // Play and Stop work on the pages that sit beside the recorder
        const bool transport = current == Screen::Glitch || current == Screen::Master || current == Screen::Editor ||
                               current == Screen::Input || current == Screen::Pads || current == Screen::Memory ||
                               current == Screen::Settings;
        if (transport && k == kPlay) { pressPlay(); refresh(true); return; }
        if (transport && k == kStop) { pressStop(); refresh(true); return; }
        keyScreen(k);
        return;
    }
    if (moveMode)
    {
        moveKey(k);
        return;
    }
    if (actionMenu)
    {
        actionKey(k);
        return;
    }
    if (session.mode() == FourTrackSession::Mode::ChoosingBars)
    {
        // while the question is open the buttons are Cancel (B7) and OK (B8)
        switch (k)
        {
            case kB7: promptCancel(); break;
            case kB8: promptConfirm(); break;
            case kLeft: case kUp: promptMove(-1); break;
            case kRight: case kDown: promptMove(1); break;
            default: break;
        }
        return;
    }
    if (shift)
    {
        switch (k)
        {
            case kB1: set.loopOnce = !set.loopOnce; unsaved = true; pushBank(); refresh(true); return;
            case kB2: open(Screen::Pads); return;
            case kB3: open(Screen::Memory); return;
            case kB4: open(Screen::Input); return;
            case kB5: muteTrack(0); return;
            case kB6: muteTrack(1); return;
            case kB7: muteTrack(2); return;
            case kB8: muteTrack(3); return;
            case kUp: undoTake(); return;
            case kDown: open(Screen::Settings); return;
            default: break;
        }
    }
    switch (k)
    {
        case kRec:
        case kB1: pressRecord(); break;
        case kStop: pressStop(); break;
        case kB2:
            if (session.loopFrames() == 0 && session.mode() == FourTrackSession::Mode::Idle && !countingIn) open(Screen::Tempo);
            else pressStop();
            break;
        case kPlay:
        case kB3: pressPlay(); break;
        case kB4:
        {
            const Source s = trackSource[session.selected()];
            if (s == Source::Line) set.monitorLine = !set.monitorLine;
            else if (s != Source::Pads) set.monitorMic = !set.monitorMic;
            hal.setMonitor(monitorFor(s));
            refresh(true);
            break;
        }
        case kSong: set.click = !set.click; unsaved = true; lastClickBeat = -1; refresh(true); break;
        case kB5: selectTrack(0); break;
        case kB6: selectTrack(1); break;
        case kB7: selectTrack(2); break;
        case kB8: selectTrack(3); break;
        case kUp: moveSelection(-1); break;
        case kDown: moveSelection(1); break;
        case kSampleEdit:
            if (session.mode() == FourTrackSession::Mode::Idle && session.hasPart(session.selected())) { editTrack = session.selected(); editEnter(); open(Screen::Editor); }
            break;
        case kPerformance: if (session.mode() == FourTrackSession::Mode::Idle) { gTrack = session.selected(); open(Screen::Glitch); } break;
        case kJogPress:
            if (session.mode() == FourTrackSession::Mode::Idle && session.loopFrames() > 0 && session.hasPart(session.selected()))
            {
                if (shift) enterMove();
                else { actionMenu = true; refresh(true); }
            }
            break;
        case kMaster: open(Screen::Master); break;
        case kFile: refreshProjectList(); open(Screen::Project); break;
        default: break;
    }
}

void App::jog(int delta)
{
    if (delta == 0) return;
    if (popupKind != Popup::None) return;
    if (current != Screen::Main)
    {
        jogScreen(delta);
        return;
    }
    if (session.mode() == FourTrackSession::Mode::ChoosingBars)
    {
        promptMove(delta > 0 ? 1 : -1);
        return;
    }
    if (moveMode)
    {
        moveStep(session.selected(), delta > 0 ? 1 : -1, shift);
        return;
    }
    moveSelection(delta > 0 ? 1 : -1);
}

void App::pad(int p, bool down)
{
    if (p < 0 || p >= 48) return;
    if (down) hal.padOn(p, padModeKeys[hal.currentSample() >= 0 ? hal.currentSample() : 0] ? padsKeysMode : false);
    else hal.padOff(p);
}

void App::togglePadMode()
{
    hal.padsReleaseAll();
    padsKeysMode = !padsKeysMode;
    refresh(true);
}

void App::muteTrack(int t)
{
    hal.bank().cfg[t].mute = !hal.bank().cfg[t].mute;
    unsaved = true;
    waveDirty[t] = true;
    refresh(true);
}

void App::applySource(Source source)
{
    if (appliedSource == (int)source) return;
    appliedSource = (int)source;
    hal.applySource(source);
    hal.setMonitor(monitorFor(source));
}

void App::promptMove(int delta)
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars || promptCount == 0) return;
    int next = promptSelected + delta;
    if (next < 0) next = 0;
    if (next >= promptCount) next = promptCount - 1;
    promptSelected = next;
    refresh(true);
}

void App::promptConfirm()
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars || promptCount == 0) return;
    const int bars = promptBars[promptSelected];
    const uint32_t rate = hal.sampleRate();
    const uint32_t played = session.pendingFrames();

    // The loop length: exactly what was played, or a grid at a set tempo (or a rounded one).
    uint32_t tempo = 0;
    if (set.tempoTenths > 0) tempo = set.tempoTenths;
    else if (set.roundIdx > 0)
    {
        tempo = loopTempoTenths(played, rate, bars);
        if (set.roundIdx == 2) tempo = (tempo + 5) / 10 * 10;
    }
    uint32_t frames = played;
    if (tempo > 0)
    {
        frames = (uint32_t)(((uint64_t)bars * 4u * 60u * 10u * rate + tempo / 2) / tempo);
        const uint32_t limit = hal.regionFrames() - kPostFrames - 2000;
        if (frames > limit) frames = limit;
        const int rec = session.recordingTrack();
        if (rec >= 0 && frames > played)
        {
            // Pad the end of the take with silence up to the grid.
            int16_t* base = const_cast<int16_t*>(partData[rec]);
            for (uint32_t i = played; i < frames; i++) base[i] = 0;
        }
        session.setPendingFrames(frames);
    }
    const int active = session.recordingTrack();
    session.confirmBars(bars);
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        waveDirty[t] = true;
        alignShift[t] = 0;
        userOffset[t] = 0;
        hasUndoTake[t] = false;
    }
    (void)active;
    unsaved = true;
    pushBank();
    syncPlayer();
    refresh(true);
}

void App::promptCancel()
{
    if (session.mode() != FourTrackSession::Mode::ChoosingBars) return;
    session.cancelBars();
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        partData[t] = session.hasPart(t) ? hal.region(t) : nullptr;
        waveDirty[t] = true;
    }
    pushBank();
    syncPlayer();
    refresh(true);
}

void App::moveSelection(int delta)
{
    const int track = session.selected() + delta;
    if (track < 0 || track >= FourTrackSession::kTracks) return;
    const bool idle = (session.mode() == FourTrackSession::Mode::Idle) && !countingIn;
    session.selectTrack(track);
    if (idle) applySource(trackSource[session.selected()]);
    refresh(true);
}

void App::selectTrack(int track)
{
    const bool idle = (session.mode() == FourTrackSession::Mode::Idle) && !countingIn;
    if (idle && track == session.selected())
        trackSource[track] = (Source)(((int)trackSource[track] + 1) % 4);
    else
        session.selectTrack(track);
    if (idle) applySource(trackSource[session.selected()]);
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Pressing Rec: the replace question, the count-in, then recording

void App::pressRecord()
{
    if (countingIn)
    {
        countingIn = false; // cancels the count-in
        refresh(true);
        return;
    }
    if (session.mode() != FourTrackSession::Mode::Idle)
    {
        if (session.mode() == FourTrackSession::Mode::Armed) { cancelRecording(); syncPlayer(); refresh(true); }
        return;
    }
    const int t = session.selected();
    if (session.loopFrames() > 0 && session.hasPart(t) && !replaceConfirmed)
    {
        showPopup(Popup::Replace, t);
        return;
    }
    replaceConfirmed = false;
    // A first take at a set tempo starts with a bar of clicks
    const bool firstTake = session.loopFrames() == 0 || (session.partCount() == 1 && session.hasPart(t));
    if (firstTake && set.countBeats() > 0 && set.tempoTenths < 2400)
    {
        countingIn = true;
        countInStart = hal.millis();
        countInBeats = 0;
        refresh(true);
        return;
    }
    startRecording();
}

void App::startRecording()
{
    lastClickBeat = -1;
    const int t = session.selected();
    // Keep the take that is about to be replaced, so it can be brought back
    if (session.hasPart(t) && session.loopFrames() > 0 && hal.sdPresent())
    {
        const uint32_t frames = session.loopFrames();
        const int32_t trailer[2] = { alignShift[t], userOffset[t] };
        char path[24];
        fmt(path, sizeof(path), "undo/t%d.raw", t);
        if (hal.fileWrite(path, partData[t], frames * 2, false)) { hal.fileWrite(path, trailer, 8, true); hasUndoTake[t] = true; }
    }
    session.pressRecord();
    const int track = session.recordingTrack();
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        hal.playerStop();
        applySource(trackSource[track]);
        hal.recordStart(track, trackSource[track]);
        takeOffset = 0;
        waitScanned = 0;
        waitLevelDb = -60;
        waitingSound = set.waitSound && trackSource[track] != Source::Pads;
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        syncPlayer();
    }
    refresh(true);
}

void App::pressStop()
{
    if (countingIn)
    {
        countingIn = false;
        refresh(true);
        return;
    }
    if (session.mode() == FourTrackSession::Mode::RecordingFirst && waitingSound)
    {
        cancelWait();
        refresh(true);
        return;
    }
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        hal.recordStop();
        session.firstRecordedFrames(hal.recordLength() - takeOffset, hal.recordFull());
        if (session.mode() == FourTrackSession::Mode::RecordingFirst) session.pressStop();
        onFirstTakeFinished();
    }
    else if (session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        if (hal.recordStartCaptured())
        {
            const uint32_t length = session.loopFrames();
            const uint32_t firstSample = (length - (hal.recordStartPosition() % length)) % length;
            hal.recordStop();
            if (hal.recordLength() > firstSample) finishPart(session.recordingTrack(), firstSample, true);
            else session.pressStop();
        }
        else
        {
            hal.recordStop();
            session.pressStop();
        }
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
    if (session.mode() != FourTrackSession::Mode::Idle) return;
    session.pressPlay();
    syncPlayer();
    refresh(true);
}

void App::undoTake()
{
    const int t = session.selected();
    if (session.mode() != FourTrackSession::Mode::Idle || !hasUndoTake[t]) { showPopup(Popup::Message, 2); return; }
    char path[24];
    fmt(path, sizeof(path), "undo/t%d.raw", t);
    const uint32_t frames = session.loopFrames();
    int32_t trailer[2] = { 0, 0 };
    if (hal.fileRead(path, 0, const_cast<int16_t*>(hal.region(t)), frames * 2) != frames * 2) { showPopup(Popup::SdMissing); return; }
    hal.fileRead(path, frames * 2, trailer, 8);
    partData[t] = hal.region(t);
    alignShift[t] = trailer[0];
    userOffset[t] = trailer[1];
    hasUndoTake[t] = false;
    noteAt[t] = -2;
    session.restore(session.loopFrames(), session.bars(), (uint8_t)((1u << t) | [&] { uint8_t m = 0; for (int i = 0; i < 4; i++) if (session.hasPart(i)) m |= (uint8_t)(1u << i); return m; }()));
    waveDirty[t] = true;
    unsaved = true;
    pushBank();
    syncPlayer();
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Recording and playing

void App::cancelWait()
{
    waitingSound = false;
    takeOffset = 0;
    hal.recordStop();
    session.pressStop();
    if (session.mode() == FourTrackSession::Mode::ChoosingBars) session.cancelBars();
}

// While waiting, the input is already being recorded; the take starts at the first sound
void App::serviceWait()
{
    const int track = session.recordingTrack() < 0 ? session.selected() : session.recordingTrack();
    const uint32_t rate = hal.sampleRate();
    const uint32_t got = hal.recordLength();
    int16_t* r = hal.region(track);
    float amp = 32767.0f;
    for (int i = 0; i > set.waitDb; i--) amp *= 0.89125f; // 10^(-1/20) per dB
    const int32_t thr = (int32_t)amp;
    uint32_t end = got < waitScanned + 4096 ? got : waitScanned + 4096;
    int32_t peak = 0;
    for (uint32_t i = waitScanned; i < end; i++)
    {
        const int32_t s = r[i] < 0 ? -(int32_t)r[i] : (int32_t)r[i];
        if (s > peak) peak = s;
        if (s >= thr)
        {
            uint32_t pre = preRollFrames();
            if (pre < rate / 50) pre = rate / 50;
            takeOffset = i > pre ? i - pre : 0;
            waitingSound = false;
            lastClickBeat = -1;
            refresh(true);
            return;
        }
    }
    waitScanned = end;
    waitLevelDb = peak > 0 ? ampToDb((float)peak / 32768.0f) : -60;
    // a long wait would fill the memory: start the recording again
    if (got > rate * 20)
    {
        hal.recordStop();
        hal.recordStart(track, trackSource[track]);
        waitScanned = 0;
    }
}

void App::cancelRecording()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst || session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        hal.recordStop();
        session.pressStop();
    }
    else if (session.mode() == FourTrackSession::Mode::Armed)
    {
        session.pressStop();
    }
}

void App::pushBank()
{
    TrackBank& b = hal.bank();
    b.loop = session.loopFrames();
    const uint32_t bars = session.bars() > 0 ? (uint32_t)session.bars() : 2;
    b.slice = b.loop / (bars * 8);
    b.playOnce = set.loopOnce;
    for (int t = 0; t < FourTrackSession::kTracks; t++) b.cfg[t].offset = alignShift[t] + userOffset[t];
}

void App::startPlayer()
{
    pushBank();
    for (int t = 0; t < FourTrackSession::kTracks; t++) hal.playerSetTrack(t, session.hasPart(t) ? partData[t] : nullptr);
    hal.playerStart(session.loopFrames());
    lastClickBeat = -1;
}

void App::syncPlayer()
{
    if (session.playing() && !hal.playerPlaying()) startPlayer();
    else if (!session.playing() && hal.playerPlaying()) hal.playerStop();
}

void App::onFirstTakeFinished()
{
    // The first Part defines the loop. Anything before its first note is cut.
    for (int t = 0; t < FourTrackSession::kTracks; t++) { partData[t] = nullptr; noteAt[t] = -2; }
    const int rec = session.recordingTrack();
    if (session.mode() == FourTrackSession::Mode::ChoosingBars && rec >= 0)
    {
        const uint32_t frames = session.pendingFrames();
        const int onset = findFirstOnset(hal.region(rec) + takeOffset, 0, frames, hal.sampleRate());
        if (onset < 0)
        {
            // Nothing was heard: throw it away
            session.cancelBars();
            hal.playerStop();
            showPopup(Popup::NoSound, rec + 1);
        }
        else
        {
            partData[rec] = hal.region(rec) + takeOffset + onset;
            session.setPendingFrames(frames - (uint32_t)onset);
        }
    }
    else if (session.hasPart(session.selected()))
    {
        partData[session.selected()] = hal.region(session.selected());
    }
    for (int t = 0; t < FourTrackSession::kTracks; t++) waveDirty[t] = true;
    takeOffset = 0;
    hal.playerStop();
}

void App::serviceFirstTake()
{
    const bool stoppedByItself = !hal.recordActive();
    session.firstRecordedFrames(hal.recordLength() - takeOffset, hal.recordFull() || stoppedByItself);
    if (session.mode() == FourTrackSession::Mode::RecordingFirst && stoppedByItself) session.pressStop();
    if (session.mode() != FourTrackSession::Mode::RecordingFirst)
    {
        onFirstTakeFinished();
        syncPlayer();
    }
}

void App::serviceArmed()
{
    if (!hal.playerPlaying()) return;
    // A Part starts recording shortly before the loop comes round (the pre-roll), and not if it
    // is already too close, so the start of the loop is never missed.
    const uint32_t rate = hal.sampleRate();
    uint32_t lead = preRollFrames();
    if (lead < rate / 10) lead = rate / 10;
    const uint32_t minLead = rate / 20;
    const uint32_t length = session.loopFrames();
    if (length < lead * 2) return;
    const uint32_t position = hal.playerPosition();
    if (position + lead >= length && position + minLead <= length)
    {
        applySource(trackSource[session.recordingTrack()]);
        hal.recordStart(session.recordingTrack(), trackSource[session.recordingTrack()]);
        session.armWindowReached();
    }
}

void App::applyAlignment(int track, uint32_t firstSample)
{
    const uint32_t length = session.loopFrames();
    const int16_t* region = hal.region(track);
    const int onset = findFirstOnset(region, 0, firstSample + length, hal.sampleRate());
    alignShift[track] = 0;
    userOffset[track] = 0;
    noteAt[track] = -2;
    if (onset < 0)
    {
        showPopup(Popup::NoSound, track + 1);
        session.dropPart(track);
        partData[track] = nullptr;
        return;
    }
    const int32_t rel = onset - (int32_t)firstSample; // first note, relative to the loop start
    const uint32_t window = windowFrames();
    if (window > 0 && (uint32_t)(rel < 0 ? -rel : rel) <= window) alignShift[track] = -rel;
}

void App::finishPart(int track, uint32_t firstSample, bool early)
{
    const uint32_t length = session.loopFrames();
    uint32_t needed = firstSample + length + kPostFrames;
    if (needed > hal.regionFrames()) needed = hal.regionFrames();
    const uint32_t recorded = hal.recordLength();
    if (early && recorded < needed)
    {
        // Everything after the stop point stays silent, not left over from an older take
        int16_t* r = hal.region(track);
        for (uint32_t i = recorded; i < needed; i++) r[i] = 0;
    }
    partData[track] = hal.region(track) + firstSample;
    session.partRecorded();
    applyAlignment(track, firstSample);
    waveDirty[track] = true;
    unsaved = true;
    pushBank();
    if (session.hasPart(track)) hal.playerSetTrack(track, partData[track]);
    else hal.playerSetTrack(track, nullptr);
    refresh(true);
}

void App::servicePartRecording()
{
    if (!hal.recordStartCaptured()) return;
    const uint32_t length = session.loopFrames();
    const uint32_t firstSample = (length - (hal.recordStartPosition() % length)) % length;
    const uint32_t needed = firstSample + length + kPostFrames;
    if (needed > hal.regionFrames() || !hal.recordActive())
    {
        hal.recordStop();
        session.pressStop(); // too long for the room, or the recorder stopped by itself
        return;
    }
    if (hal.recordLength() >= needed)
    {
        hal.recordStop();
        finishPart(session.recordingTrack(), firstSample, false);
    }
}

//--------------------------------------------------------------------------------------------
// Click and count-in

void App::serviceClick()
{
    if (set.click && session.mode() == FourTrackSession::Mode::RecordingFirst && session.loopFrames() == 0)
    {
        // the first take has no loop yet: the click follows the starting tempo, or 120 BPM when it is Free
        const uint32_t tenths = set.tempoTenths > 0 ? set.tempoTenths : 1200u;
        const uint32_t beatFrames = (uint32_t)((uint64_t)hal.sampleRate() * 600 / tenths);
        const int idx = beatFrames > 0 ? (int)(hal.recordLength() / beatFrames) : 0;
        if (idx != lastClickBeat)
        {
            lastClickBeat = idx;
            hal.click(idx % 4 == 0);
        }
        return;
    }
    if (!set.click || !hal.playerPlaying() || session.bars() <= 0 || session.loopFrames() == 0) return;
    const uint32_t beat = session.loopFrames() / ((uint32_t)session.bars() * 4);
    if (beat == 0) return;
    const int idx = (int)(hal.playerPosition() / beat);
    if (idx != lastClickBeat)
    {
        lastClickBeat = idx;
        hal.click(idx % 4 == 0);
    }
}

void App::serviceCountIn()
{
    if (!countingIn) return;
    const uint32_t beatMs = 600000u / (set.tempoTenths > 0 ? set.tempoTenths : 1200u); // Free tempo counts in at 120
    const uint32_t elapsed = hal.millis() - countInStart;
    const int beats = (int)(elapsed / beatMs);
    if (countInBeats == 0 && beats == 0)
    {
        hal.click(true);
        countInBeats = 1;
    }
    const int total = set.countBeats();
    while (countInBeats <= beats && countInBeats < total)
    {
        hal.click(false);
        countInBeats++;
    }
    if (beats >= total)
    {
        countingIn = false;
        startRecording();
    }
}

//--------------------------------------------------------------------------------------------

void App::tick()
{
    hal.tick();
    switch (session.mode())
    {
        case FourTrackSession::Mode::RecordingFirst: if (waitingSound) serviceWait(); else serviceFirstTake(); break;
        case FourTrackSession::Mode::Armed: serviceArmed(); break;
        case FourTrackSession::Mode::RecordingPart: servicePartRecording(); break;
        case FourTrackSession::Mode::Idle:
        case FourTrackSession::Mode::ChoosingBars: break;
    }
    serviceCountIn();
    serviceClick();
    serviceJob();
    refresh(false);
}

bool App::updateTrackMeters()
{
    const uint32_t now = hal.millis();
    if (now - meterTimer < 50) return false;
    meterTimer = now;

    const FourTrackSession::Mode mode = session.mode();
    const bool recordingNow = (mode == FourTrackSession::Mode::RecordingFirst || mode == FourTrackSession::Mode::RecordingPart);
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        uint16_t peak = 0;
        if (recordingNow && t == session.recordingTrack())
        {
            const uint32_t length = hal.recordLength();
            if (length >= (uint32_t)kBlock) peak = loopPeakBlock(hal.region(t) + (length - kBlock), kBlock, 0, kBlock);
        }
        else peak = hal.playerTakePeak(t);
        const int db = ampToDb((float)peak / 32768.0f);
        trackDb[t] = (db > trackDb[t]) ? db : (trackDb[t] - 3 > db ? trackDb[t] - 3 : db);
        int level = (trackDb[t] + 50) * 2;
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        model.lane[t].meter = (uint8_t)level;
    }
    return true;
}

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
    const int columns = 120;
    const int32_t off = alignShift[track] + userOffset[track];
    for (int c = 0; c < columns; c++)
    {
        // The waveform shows the loop as it plays, so a shifted Part is drawn shifted
        int64_t from = (int64_t)(((uint64_t)c * frames) / columns) - off;
        int64_t to = (int64_t)(((uint64_t)(c + 1) * frames) / columns) - off;
        const int64_t L = (int64_t)frames;
        from = ((from % L) + L) % L;
        to = from + (int64_t)(frames / columns);
        uint16_t peak = 0;
        const uint32_t span = (uint32_t)(to - from);
        if ((uint32_t)from + span <= frames) peak = loopPeakBlock(data + from, span, 0, (int)span);
        else
        {
            peak = loopPeakBlock(data + from, frames - (uint32_t)from, 0, (int)(frames - (uint32_t)from));
        }
        const float norm = squareRoot((float)peak / 32767.0f);
        const int h = (int)(norm * 255.0f);
        lane.wave[c] = (uint8_t)(h > 255 ? 255 : h);
    }
    lane.waveCount = columns;
}

void App::fillView()
{
    const uint32_t rate = hal.sampleRate();
    const FourTrackSession::Mode mode = session.mode();
    const int recTrack = session.recordingTrack();
    const uint32_t loop = session.loopFrames();
    View& v = model;
    TrackBank& bank = hal.bank();

    fmt(v.project, sizeof(v.project), "%s", projectName);
    v.unsaved = unsaved;
    if (session.bars() > 0 && loop > 0)
    {
        const uint32_t tempo = loopTempoTenths(loop, rate, session.bars());
        fmt(v.tempo, sizeof(v.tempo), "%u.%u", (unsigned)(tempo / 10), (unsigned)(tempo % 10));
        fmt(v.bars, sizeof(v.bars), "%d", session.bars());
    }
    else if (set.tempoTenths > 0)
    {
        fmt(v.tempo, sizeof(v.tempo), "%u.%u", (unsigned)(set.tempoTenths / 10), (unsigned)(set.tempoTenths % 10));
        fmt(v.bars, sizeof(v.bars), "-");
    }
    else
    {
        fmt(v.tempo, sizeof(v.tempo), "Free");
        fmt(v.bars, sizeof(v.bars), "-");
    }
    if (mode == FourTrackSession::Mode::ChoosingBars)
    {
        fmt(v.tempo, sizeof(v.tempo), "?");
        fmt(v.bars, sizeof(v.bars), "?");
    }
    fmt(v.input, sizeof(v.input), "%s", sourceName(trackSource[session.selected()]));
    v.monitor = monitorFor(trackSource[session.selected()]);
    v.click = set.click;
    v.wait = set.waitSound;
    {
        float l = 0.0f, r = 0.0f;
        hal.inputLevels(l, r);
        fmt(v.inputLevels, sizeof(v.inputLevels), "L %d  R %d dB", ampToDb(l), ampToDb(r));
        const int red = hal.monitorReductionDb();
        if (red < 0) fmt(v.fb, sizeof(v.fb), "FB %d dB", red);
        else v.fb[0] = 0;
    }
    v.barCount = (uint8_t)session.bars();
    v.loopFrames = loop;

    v.statusNote[0] = 0;
    if (countingIn)
    {
        v.statusKind = 3;
        fmt(v.status, sizeof(v.status), "Count-in");
    }
    else if (mode == FourTrackSession::Mode::RecordingFirst && waitingSound)
    {
        v.statusKind = 3;
        fmt(v.status, sizeof(v.status), "Armed");
        fmt(v.statusNote, sizeof(v.statusNote), "No Loop yet");
    }
    else if (mode == FourTrackSession::Mode::RecordingFirst)
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
        if (loop == 0) fmt(v.statusNote, sizeof(v.statusNote), "No Loop yet");
    }

    if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        fmt(v.pos, sizeof(v.pos), "-"); // the elapsed time is in the lane
        v.posFrames = 0;
    }
    else if (loop > 0)
    {
        v.posFrames = hal.playerPosition();
        if (session.bars() > 0)
        {
            const uint32_t sixteenths = (uint32_t)(((uint64_t)v.posFrames * session.bars() * 16) / loop);
            fmt(v.pos, sizeof(v.pos), "%u.%u.%02u", (unsigned)(sixteenths / 16 + 1), (unsigned)((sixteenths % 16) / 4 + 1), (unsigned)(sixteenths % 4 + 1));
        }
        else formatTime(v.pos, sizeof(v.pos), v.posFrames, rate);
    }
    else
    {
        v.posFrames = 0;
        fmt(v.pos, sizeof(v.pos), "-");
    }

    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        Lane& L = v.lane[t];
        fmt(L.name, sizeof(L.name), "Track %d", t + 1);
        L.info[0] = 0;
        L.side1[0] = 0;
        L.side2[0] = 0;
        L.progress = 255;
        L.warn = false;
        L.msg1[0] = 0;
        L.msg2[0] = 0;
        L.hasPart = session.hasPart(t);
        L.muted = bank.cfg[t].mute;
        L.glitch = false;
        for (int s = 0; s < 4; s++)
            if (bank.cfg[t].slot[s].on && bank.cfg[t].slot[s].type != FxType::None) L.glitch = true;
        L.selected = (t == session.selected());
        L.active = (t == recTrack && (mode == FourTrackSession::Mode::RecordingFirst || mode == FourTrackSession::Mode::RecordingPart || mode == FourTrackSession::Mode::Armed));
        const int32_t off = alignShift[t] + userOffset[t];
        if (L.hasPart && off != 0)
        {
            const int ms = (int)((int64_t)off * 1000 / (int64_t)rate);
            fmt(L.info, sizeof(L.info), "Offset %s%d ms", ms < 0 ? "-" : "+", ms < 0 ? -ms : ms);
        }
        if (waveDirty[t])
        {
            waveDirty[t] = false;
            computeWave(t);
            waveVersion[t]++;
        }
        if (!L.hasPart) L.waveCount = 0;

        fmt(L.tag, sizeof(L.tag), "%s", sourceName(trackSource[t])); // every track shows where it records from
        if (L.active && mode == FourTrackSession::Mode::RecordingFirst && waitingSound)
        {
            fmt(L.msg1, sizeof(L.msg1), "Waiting for sound");
            fmt(L.msg2, sizeof(L.msg2), "%d dB", waitLevelDb);
            fmt(L.side1, sizeof(L.side1), "Starts at %d dB", (int)set.waitDb);
        }
        else if (L.active && mode == FourTrackSession::Mode::RecordingFirst)
        {
            fmt(L.msg1, sizeof(L.msg1), "First take - elapsed");
            const uint32_t took = hal.recordLength() > takeOffset ? hal.recordLength() - takeOffset : 0;
            formatTime(L.msg2, sizeof(L.msg2), took, rate);
            const uint32_t maxFrames = rate * 24;
            const uint32_t got = took < maxFrames ? took : maxFrames;
            const uint32_t leftTenths = (maxFrames - got) * 10 / rate;
            L.progress = (uint8_t)(got * 100 / maxFrames);
            L.warn = leftTenths <= 30;
            fmt(L.side1, sizeof(L.side1), "%s", L.warn ? "Memory nearly full" : "Up to 24 s per Part");
            fmt(L.side2, sizeof(L.side2), "%u.%u s left", (unsigned)(leftTenths / 10), (unsigned)(leftTenths % 10));
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
            const uint32_t pos = hal.playerPosition();
            const int bars = session.bars() > 0 ? session.bars() : 1;
            const uint32_t sixteenths = loop > 0 ? (uint32_t)(((uint64_t)pos * bars * 16) / loop) : 0;
            fmt(L.msg2, sizeof(L.msg2), "Bar %u.%u of %d", (unsigned)(sixteenths / 16 + 1), (unsigned)((sixteenths % 16) / 4 + 1), bars);
        }
        else if (countingIn && t == session.selected())
        {
            fmt(L.msg1, sizeof(L.msg1), "Count-in");
            fmt(L.msg2, sizeof(L.msg2), "Beat %d of %d", countInBeats > set.countBeats() ? set.countBeats() : (countInBeats < 1 ? 1 : countInBeats), set.countBeats());
        }
        else if (!L.hasPart)
        {
            if (loop == 0 && t == session.selected() && mode == FourTrackSession::Mode::Idle && !countingIn && current == Screen::Main) fmt(L.msg1, sizeof(L.msg1), "Press Rec to record the first Part");
            else fmt(L.msg2, sizeof(L.msg2), "Empty");
        }
    }

    v.master = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++) if (v.lane[t].meter > v.master) v.master = v.lane[t].meter;

    {
        Prompt& P = v.prompt;
        P.active = (mode == FourTrackSession::Mode::ChoosingBars);
        if (P.active)
        {
            const uint32_t played = session.pendingFrames();
            if (!promptShown)
            {
                promptCount = loopBarChoices(played, rate, promptBars);
                promptSelected = loopBarSuggestion(played, rate, promptBars, promptCount);
                if (set.tempoTenths > 0)
                {
                    // nearest to the tempo that was set
                    uint32_t best = 0xFFFFFFFFu;
                    for (int i = 0; i < promptCount; i++)
                    {
                        const uint32_t tt = loopTempoTenths(played, rate, promptBars[i]);
                        const uint32_t d = tt > set.tempoTenths ? tt - set.tempoTenths : set.tempoTenths - tt;
                        if (d < best) { best = d; promptSelected = i; }
                    }
                }
            }
            const uint32_t hundredths = (uint32_t)(((uint64_t)played * 100) / rate);
            P.memoryFull = session.memoryFull();
            P.count = (uint8_t)promptCount;
            P.selected = (uint8_t)promptSelected;
            for (int i = 0; i < promptCount; i++)
            {
                P.bars[i] = (uint8_t)promptBars[i];
                P.bpm10[i] = (uint16_t)(set.tempoTenths > 0 ? set.tempoTenths : loopTempoTenths(played, rate, promptBars[i]));
            }
            fmt(P.played, sizeof(P.played), "You played %u.%02u s. No audio is stretched; only the tempo is worked out.", (unsigned)(hundredths / 100), (unsigned)(hundredths % 100));
            fmt(P.note, sizeof(P.note), "Stopped at %u.%u s. Everything recorded is kept.", (unsigned)(hundredths / 100), (unsigned)((hundredths % 100) / 10));
            fmt(P.suggestion, sizeof(P.suggestion), "%s", set.tempoTenths > 0 ? "Grid at the tempo you set" : "Suggested: nearest 120 BPM");
        }
    }

    if (countingIn) fmt(v.hint, sizeof(v.hint), "Count-in, then the first take starts. Stop cancels.");
    else if (mode == FourTrackSession::Mode::RecordingFirst)
    {
        if (waitingSound) fmt(v.hint, sizeof(v.hint), "Recording starts when the input reaches %d dB. Stop cancels.", (int)set.waitDb);
        else if (v.lane[session.recordingTrack() < 0 ? 0 : session.recordingTrack()].warn) fmt(v.hint, sizeof(v.hint), "At 24 s the take stops by itself and keeps everything.");
        else fmt(v.hint, sizeof(v.hint), "B2 or Stop ends the take. Then: How many bars?");
    }
    else if (mode == FourTrackSession::Mode::Armed) fmt(v.hint, sizeof(v.hint), "Records one Loop from the loop start, then stops. B1 again disarms.");
    else if (mode == FourTrackSession::Mode::RecordingPart) fmt(v.hint, sizeof(v.hint), "Stop keeps what was recorded and fills the rest of the Loop with silence.");
    else if (loop == 0) fmt(v.hint, sizeof(v.hint), "B1 records the first Part. B2 sets a starting tempo first.");
    else if (session.playing()) fmt(v.hint, sizeof(v.hint), "B2 stops. Jog or Up/Down select a track. Insert opens actions.");
    else fmt(v.hint, sizeof(v.hint), "B1 arms Track %d. It records %s from the next loop start.", session.selected() + 1, sourceName(trackSource[session.selected()]));
    if (mode == FourTrackSession::Mode::Idle && loop > 0 && session.hasPart(session.selected()))
        fmt(v.hint, sizeof(v.hint), "Track %d: Insert for actions. Shift+Insert move. Shift+B%d mute.", session.selected() + 1, session.selected() + 5);
    if (actionMenu) fmt(v.hint, sizeof(v.hint), "Track %d: choose an action. Insert or B8 closes.", session.selected() + 1);
    if (moveMode) fmt(v.hint, sizeof(v.hint), "Move Track %d: jog or arrows step by the grid, Shift+arrows by 1 ms.", session.selected() + 1);
    fmt(v.loopMode, sizeof(v.loopMode), "%s", set.loopOnce ? "Play once" : "Repeat");
    fmt(v.padsMode, sizeof(v.padsMode), "%s", padsKeysMode ? "KEYS" : "PAD");

    static const char* const names[8] = { "Rec", "Stop", "Play", "", "Track 1", "Track 2", "Track 3", "Track 4" };
    for (int b = 0; b < 8; b++)
    {
        fmt(v.btn[b], sizeof(v.btn[b]), "%s", names[b]);
        v.btnKind[b] = (b == 0) ? 1 : (b == 1) ? 2 : (b == 2) ? 3 : 0;
    }
    if (loop == 0 && mode == FourTrackSession::Mode::Idle && !countingIn)
    {
        fmt(v.btn[1], sizeof(v.btn[1]), "Tempo");
        v.btnKind[1] = 0;
    }
    fmt(v.btn[3], sizeof(v.btn[3]), "Monitor");
    v.btnSelected = (int8_t)(4 + session.selected());
    v.btnMask = (uint8_t)((v.monitor ? 8 : 0) | ((session.playing() || mode == FourTrackSession::Mode::RecordingPart || mode == FourTrackSession::Mode::Armed) ? 4 : 0) | ((mode != FourTrackSession::Mode::Idle && mode != FourTrackSession::Mode::ChoosingBars) ? 1 : 0));
    if (current == Screen::Tempo)
    {
        static const char* const tp[8] = { "Free", "Set", "", "", "", "", "OK", "Cancel" };
        for (int b = 0; b < 8; b++) { fmt(v.btn[b], sizeof(v.btn[b]), "%s", tp[b]); v.btnKind[b] = 0; }
        v.btnSelected = -1;
        v.btnMask = (uint8_t)(set.tempoTenths > 0 ? 2 : 1);
        v.hint[0] = 0;
    }
    if (actionMenu)
    {
        static const char* const am[8] = { "Move", "Edit", "Glitch", "Mute", "Source", "Undo take", "", "Close" };
        for (int b = 0; b < 8; b++) { fmt(v.btn[b], sizeof(v.btn[b]), "%s", am[b]); v.btnKind[b] = 0; }
        fmt(v.btn[3], sizeof(v.btn[3]), "%s", hal.bank().cfg[session.selected()].mute ? "Unmute" : "Mute");
        v.btnSelected = -1;
        v.btnMask = 0;
    }
    if (moveMode)
    {
        static const char* const mv[8] = { "To start", "To beat", "", "", "", "", "Cancel", "OK" };
        for (int b = 0; b < 8; b++) { fmt(v.btn[b], sizeof(v.btn[b]), "%s", mv[b]); v.btnKind[b] = 0; }
        fmt(v.btn[2], sizeof(v.btn[2]), "%s", gridName(set.gridIdx));
        fmt(v.btn[3], sizeof(v.btn[3]), "All %s", moveAll ? "on" : "off");
        v.btnSelected = -1;
        v.btnMask = 0;
    }
}

void App::refresh(bool force)
{
    if (!visibleNow) return;
    const bool metersUpdated = updateTrackMeters();
    fillView();

    uint32_t sig = 2166136261u;
    const View& v = model;
    sig = hashBytes(sig, v.project, sizeof(v.project));
    sig = hashBytes(sig, &v.unsaved, sizeof(v.unsaved));
    sig = hashBytes(sig, v.tempo, sizeof(v.tempo));
    sig = hashBytes(sig, v.bars, sizeof(v.bars));
    sig = hashBytes(sig, v.input, sizeof(v.input));
    sig = hashBytes(sig, v.fb, sizeof(v.fb));
    sig = hashBytes(sig, v.status, sizeof(v.status));
    sig = hashBytes(sig, v.statusNote, sizeof(v.statusNote));
    sig = hashBytes(sig, v.hint, sizeof(v.hint));
    sig = hashBytes(sig, v.loopMode, sizeof(v.loopMode));
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
        sig = hashBytes(sig, L.info, sizeof(L.info));
        sig = hashBytes(sig, L.msg1, sizeof(L.msg1));
        if (!L.active) sig = hashBytes(sig, L.msg2, sizeof(L.msg2));
        sig = hashBytes(sig, &L.hasPart, sizeof(L.hasPart));
        sig = hashBytes(sig, &L.selected, sizeof(L.selected));
        sig = hashBytes(sig, &L.active, sizeof(L.active));
        sig = hashBytes(sig, &L.muted, sizeof(L.muted));
        sig = hashBytes(sig, &L.glitch, sizeof(L.glitch));
        sig = hashBytes(sig, &waveVersion[t], sizeof(waveVersion[t]));
    }

    const bool wantPrompt = (session.mode() == FourTrackSession::Mode::ChoosingBars);
    if (wantPrompt && !promptShown) { promptShown = true; redraw[3] = true; }
    else if (!wantPrompt && promptShown) { promptShown = false; redraw[3] = false; }
    else if (wantPrompt && force) redraw[3] = true;

    if (current == Screen::Main || current == Screen::Tempo)
    {
        if (force || sig != staticSignature)
        {
            staticSignature = sig;
            redraw[0] = true;
            redraw[2] = true;
        }
        if (force || metersUpdated) redraw[1] = true;
    }
    if (current != Screen::Main)
    {
        // Pages are rebuilt when something they show changes (live readouts about 10 times a second)
        const uint32_t now = hal.millis();
        if (force || now - pageTimer > 100)
        {
            pageTimer = now;
            buildPage();
            redraw[4] = true;
        }
    }
    if (popupKind != Popup::None && (force || redraw[5])) redraw[5] = true;
}

//--------------------------------------------------------------------------------------------
// Drawing

bool App::layerVisible(int layer) const
{
    if (!visibleNow) return false;
    if (layer == 3) return promptShown && current == Screen::Main;
    if (layer <= 2) return current == Screen::Main || current == Screen::Tempo;
    if (layer == 4) return current != Screen::Main;
    return popupKind != Popup::None;
}

int App::stagesOf(int layer) const
{
    if (layer < 4) return layerStages(layer);
    return layer == 4 ? page.stageCount() : popupPage.stageCount();
}

bool App::takeRedraw(int layer)
{
    if (layer < 0 || layer >= 6) return false;
    const bool r = redraw[layer];
    redraw[layer] = false;
    return r;
}

void App::draw(int layer, int stage, Gfx& gfx) const
{
    if (layer < 4) drawLayer(layer, stage, model, pal, gfx);
    else if (layer == 4) page.replay(stage, gfx);
    else popupPage.replay(stage, gfx);
}
} // namespace ft

namespace ft
{
//--------------------------------------------------------------------------------------------
// Beat grid and moving a Part

const char* App::gridName(int idx)
{
    static const char* const names[5] = { "Grid off", "Bar", "1/4", "1/8", "1/16" };
    return names[idx < 0 || idx > 4 ? 0 : idx];
}

uint32_t App::gridFrames() const
{
    const uint32_t loop = session.loopFrames();
    const uint32_t bars = session.bars() > 0 ? (uint32_t)session.bars() : 1;
    uint32_t g;
    switch (set.gridIdx)
    {
        case 1: g = loop / bars; break;
        case 2: g = loop / (bars * 4); break;
        case 3: g = loop / (bars * 8); break;
        case 4: g = loop / (bars * 16); break;
        default: g = hal.sampleRate() / 1000; break;
    }
    return g > 0 ? g : 1;
}

int32_t App::noteFrame(int t)
{
    if (noteAt[t] == -2)
    {
        const int16_t* d = partData[t];
        noteAt[t] = (d == nullptr) ? -1 : findFirstOnset(d, 0, session.loopFrames(), hal.sampleRate());
    }
    return noteAt[t];
}

void App::shiftTrack(int track, int32_t delta, bool all)
{
    const int32_t L = (int32_t)session.loopFrames();
    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        if (!(t == track || (all && session.hasPart(t)))) continue;
        int32_t u = userOffset[t] + delta;
        if (L > 0)
        {
            u %= L;
            if (u > L / 2) u -= L;
            if (u < -L / 2) u += L;
        }
        userOffset[t] = u;
    }
    unsaved = true;
    pushBank();
    for (int t = 0; t < FourTrackSession::kTracks; t++) waveDirty[t] = waveDirty[t] || t == track || all;
    editWaveDirty = true;
    refresh(true);
}

// Steps the Part so its first note moves to the next (or previous) grid line. With the grid off,
// or `fine`, it moves by 1 ms.
void App::moveStep(int track, int dir, bool fine)
{
    const int32_t L = (int32_t)session.loopFrames();
    if (L <= 0) return;
    const int32_t note = noteFrame(track) < 0 ? 0 : noteFrame(track);
    int32_t pos = (note + alignShift[track] + userOffset[track]) % L;
    if (pos < 0) pos += L;
    const int32_t g = (int32_t)gridFrames();
    int32_t target;
    if (fine || set.gridIdx == 0) target = pos + dir * (int32_t)(hal.sampleRate() / 1000);
    else if (pos % g == 0) target = pos + dir * g;
    else target = dir > 0 ? (pos / g + 1) * g : (pos / g) * g;
    shiftTrack(track, target - pos, moveAll);
}

void App::moveTo(int track, bool toStart)
{
    const int32_t L = (int32_t)session.loopFrames();
    if (L <= 0) return;
    const int32_t note = noteFrame(track) < 0 ? 0 : noteFrame(track);
    int32_t pos = (note + alignShift[track] + userOffset[track]) % L;
    if (pos < 0) pos += L;
    int32_t target = 0;
    if (!toStart)
    {
        const int32_t g = (int32_t)gridFrames();
        target = ((pos + g / 2) / g) * g;
    }
    int32_t delta = target - pos;
    if (delta > L / 2) delta -= L;
    if (delta < -L / 2) delta += L;
    shiftTrack(track, delta, moveAll);
}

void App::moveKey(int k)
{
    const int t = session.selected();
    switch (k)
    {
        case kB1: moveTo(t, true); break;
        case kB2: moveTo(t, false); break;
        case kB3: set.gridIdx = (uint8_t)((set.gridIdx + 1) % 5); unsaved = true; break;
        case kB4: moveAll = !moveAll; break;
        case kB7:
            for (int i = 0; i < FourTrackSession::kTracks; i++) userOffset[i] = moveEntry[i];
            moveMode = false;
            pushBank();
            for (int i = 0; i < FourTrackSession::kTracks; i++) waveDirty[i] = true;
            break;
        case kB8: moveMode = false; break;
        case kLeft: moveStep(t, -1, shift); break;
        case kRight: moveStep(t, 1, shift); break;
        case kPlay: pressPlay(); break;
        case kStop: pressStop(); break;
        default: break;
    }
    refresh(true);
}
} // namespace ft

namespace ft
{
void App::enterMove()
{
    moveMode = true;
    actionMenu = false;
    for (int t = 0; t < FourTrackSession::kTracks; t++) { moveEntry[t] = userOffset[t]; noteAt[t] = -2; }
    refresh(true);
}

// The action menu for the selected track: the same things the shortcuts do, in plain words
void App::actionKey(int k)
{
    const int t = session.selected();
    switch (k)
    {
        case kB1: enterMove(); return;
        case kB2: actionMenu = false; editTrack = t; editEnter(); open(Screen::Editor); return;
        case kB3: actionMenu = false; gTrack = t; open(Screen::Glitch); return;
        case kB4: muteTrack(t); break;
        case kB5: selectTrack(t); break; // pressing the selected track again changes its source
        case kB6: actionMenu = false; undoTake(); return;
        case kB7: case kB8: case kJogPress: actionMenu = false; break;
        case kUp: moveSelection(-1); break;
        case kDown: moveSelection(1); break;
        case kPlay: pressPlay(); break;
        case kStop: pressStop(); break;
        default: break;
    }
    if (!session.hasPart(session.selected())) actionMenu = false;
    refresh(true);
}
} // namespace ft
