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
    hal.setMonitor(set.monitorLine);
    staticSignature = 0;
    for (int t = 0; t < FourTrackSession::kTracks; t++) waveDirty[t] = true;
    pushBank();
    refresh(true);
}

void App::leave()
{
    if (job != Job::None) job = Job::None;
    countingIn = false;
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
        case kB4: togglePadMode(); break;
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
    if (firstTake && set.countIn && set.tempoTenths < 2400)
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
    if (session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        hal.recordStop();
        session.firstRecordedFrames(hal.recordLength(), hal.recordFull());
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
    session.restore(session.loopFrames(), session.bars(), (uint8_t)((1u << t) | [&] { uint8_t m = 0; for (int i = 0; i < 4; i++) if (session.hasPart(i)) m |= (uint8_t)(1u << i); return m; }()));
    waveDirty[t] = true;
    unsaved = true;
    pushBank();
    syncPlayer();
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Recording and playing

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
    for (int t = 0; t < FourTrackSession::kTracks; t++) partData[t] = nullptr;
    const int rec = session.recordingTrack();
    if (session.mode() == FourTrackSession::Mode::ChoosingBars && rec >= 0)
    {
        const uint32_t frames = session.pendingFrames();
        const int onset = findFirstOnset(hal.region(rec), 0, frames, hal.sampleRate());
        if (onset < 0)
        {
            // Nothing was heard: throw it away
            session.cancelBars();
            hal.playerStop();
            showPopup(Popup::NoSound, rec + 1);
        }
        else
        {
            partData[rec] = hal.region(rec) + onset;
            session.setPendingFrames(frames - (uint32_t)onset);
        }
    }
    else if (session.hasPart(session.selected()))
    {
        partData[session.selected()] = hal.region(session.selected());
    }
    for (int t = 0; t < FourTrackSession::kTracks; t++) waveDirty[t] = true;
    hal.playerStop();
}

void App::serviceFirstTake()
{
    const bool stoppedByItself = !hal.recordActive();
    session.firstRecordedFrames(hal.recordLength(), hal.recordFull() || stoppedByItself);
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
    while (countInBeats <= beats && countInBeats < 4)
    {
        hal.click(false);
        countInBeats++;
    }
    if (beats >= 4)
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
        case FourTrackSession::Mode::RecordingFirst: serviceFirstTake(); break;
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
    const int columns = 100;
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
    fmt(v.input, sizeof(v.input), "%s", sourceName(trackSource[session.selected()]));
    v.monitor = (trackSource[session.selected()] == Source::Line) && set.monitorLine;
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
        formatTime(v.pos, sizeof(v.pos), hal.recordLength(), rate);
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
        fmt(L.tag, sizeof(L.tag), "%s", sourceName(trackSource[t]));
        L.info[0] = 0;
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
        else if (countingIn && t == session.selected())
        {
            fmt(L.msg1, sizeof(L.msg1), "Count-in");
            fmt(L.msg2, sizeof(L.msg2), "Beat %d of 4", countInBeats > 4 ? 4 : (countInBeats < 1 ? 1 : countInBeats));
        }
        else if (!L.hasPart) fmt(L.msg2, sizeof(L.msg2), "Empty");
    }

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
            fmt(P.suggestion, sizeof(P.suggestion), "%s", set.tempoTenths > 0 ? "Grid at the tempo you set" : "");
        }
    }

    if (countingIn) fmt(v.hint, sizeof(v.hint), "Count-in, then the first take starts. Stop cancels.");
    else if (mode == FourTrackSession::Mode::RecordingFirst) fmt(v.hint, sizeof(v.hint), "Stop ends the take and keeps it.");
    else if (mode == FourTrackSession::Mode::Armed) fmt(v.hint, sizeof(v.hint), "Records one Loop from the loop start. Rec disarms.");
    else if (mode == FourTrackSession::Mode::RecordingPart) fmt(v.hint, sizeof(v.hint), "Stop keeps what was recorded, the rest is silence.");
    else if (loop == 0) fmt(v.hint, sizeof(v.hint), "Rec records the first Part, which sets the Loop.");
    else fmt(v.hint, sizeof(v.hint), "Rec arms Track %d. Press it again to change its source.", session.selected() + 1);
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
    fmt(v.btn[3], sizeof(v.btn[3]), "%s", padsKeysMode ? "Pads:Keys" : "Pads:Pad");
    v.btnSelected = (int8_t)(4 + session.selected());
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

    if (current == Screen::Main)
    {
        if (force || sig != staticSignature)
        {
            staticSignature = sig;
            redraw[0] = true;
            redraw[2] = true;
        }
        if (force || metersUpdated) redraw[1] = true;
    }
    else
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
    if (layer <= 2) return current == Screen::Main;
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
