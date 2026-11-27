// Every screen except the main one: Tempo, Part Editor, Glitch, Master, Input, Project, Settings,
// Export, Pads, Memory, and the popups. Each is built into a Canvas (docs/design/screen-requirements.md).

#include "App.h"
#include "BuildId.h"
#include "MiniFmt.h"
#include "Pen.h"
#include "loop_fill.h"

namespace ft
{
namespace
{
const uint16_t kWindowMs[5] = { 0, 30, 60, 100, 150 };
const uint16_t kPreRollMs[5] = { 0, 60, 120, 250, 500 };
const char* const kRound[3] = { "Off", "0.1 BPM", "1 BPM" };

const char* const kFxNames[(int)FxType::kCount] = { "Empty slot", "Jump back", "Jump back 2", "Stutter", "Reverse", "Pitch", "Bit-crush", "Overdrive", "Pan", "Low-pass", "High-pass", "Volume", "Random jumps", "Pitch steps" };

const char* intervalName(int semis)
{
    switch (semis)
    {
        case -12: return "Down octave";
        case -7: return "Down fifth";
        case -5: return "Down fourth";
        case -3: return "Down minor third";
        case 0: return "No shift";
        case 3: return "Minor third";
        case 4: return "Major third";
        case 5: return "Fourth";
        case 7: return "Fifth";
        case 10: return "Minor seventh";
        case 11: return "Major seventh";
        case 12: return "Octave";
        default: return "Custom";
    }
}
const int kIntervals[12] = { -12, -7, -5, -3, 0, 3, 4, 5, 7, 10, 11, 12 };

int stepInterval(int semis, int delta)
{
    int idx = 4;
    for (int i = 0; i < 12; i++) if (kIntervals[i] == semis) idx = i;
    idx += delta;
    if (idx < 0) idx = 0;
    if (idx > 11) idx = 11;
    return kIntervals[idx];
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void slotValue(const FxSlot& s, char* out, size_t n)
{
    switch (s.type)
    {
        case FxType::Stutter: fmt(out, n, "1/%d slice - %d%%", s.amount < 25 ? 2 : s.amount < 50 ? 4 : s.amount < 75 ? 8 : 16, s.amount); break;
        case FxType::JumpBack: fmt(out, n, "1 slice"); break;
        case FxType::JumpBack2: fmt(out, n, "2 slices"); break;
        case FxType::Reverse: fmt(out, n, "1 slice - %d%%", s.amount); break;
        case FxType::Pitch: fmt(out, n, "%s", intervalName(s.semis)); break;
        case FxType::Crush: fmt(out, n, "%d bit", 16 - s.amount * 12 / 100); break;
        case FxType::Drive: fmt(out, n, "%d%%", s.amount); break;
        case FxType::Pan: { const int p = (int)s.amount - 100; fmt(out, n, "%s%d", p < 0 ? "L " : (p > 0 ? "R " : "C"), p < 0 ? -p : p); break; }
        case FxType::LowPass:
        case FxType::HighPass: { const int hz = 100 << (s.amount * 7 / 100); fmt(out, n, "%d.%d kHz", hz / 1000, (hz % 1000) / 100); break; }
        case FxType::Volume: fmt(out, n, "-%d dB", s.amount * 48 / 100); break;
        default: out[0] = 0; break;
    }
}
} // namespace

//--------------------------------------------------------------------------------------------
// Opening, leaving, popups

void App::open(Screen s)
{
    if (s == Screen::Tempo) { tempoEntry = set.tempoTenths; countEntry = set.countIdx; waitEntry = set.waitSound; waitDbEntry = set.waitDb; sRow = 0; }
    current = s;
    promptShown = false;
    redraw[4] = true;
    refresh(true);
}

void App::back()
{
    if (current == Screen::Load) loadStopPreview();
    if (job == Job::Resample) { job = Job::None; rsState = Rs::Ready; }
    if (auditioning)
    {
        auditioning = false;
        for (int t = 0; t < 4; t++) hal.bank().cfg[t].mute = glitchLatched[t];
    }
    current = Screen::Main;
    dirtyAll();
    refresh(true);
}

void App::showPopup(Popup kind, int arg)
{
    popupKind = kind;
    popupArg = arg;
    popupPage.clear();
    Pen pn(popupPage, pal);
    drawPopup(pn);
    redraw[5] = true;
    refresh(true);
}

void App::popupKey(int k)
{
    const Popup kind = popupKind;
    auto close = [&] { popupKind = Popup::None; redraw[5] = false; dirtyAll(); refresh(true); };
    switch (kind)
    {
        case Popup::Replace:
            if (k == kB7) close();
            else if (k == kB8) { close(); replaceConfirmed = true; pressRecord(); }
            break;
        case Popup::NoSound:
            if (k == kB7) close();
            else if (k == kB8) { close(); replaceConfirmed = true; pressRecord(); }
            break;
        case Popup::NotArmed:
        case Popup::Message:
            if (k == kB8) close();
            break;
        case Popup::SdMissing:
            if (current == Screen::Load)
            {
                if (k == kB7) { close(); back(); }
                else if (k == kB8) { close(); loadOpen(); }
            }
            else if (k == kB7) { close(); job = Job::None; }
            else if (k == kB8) { close(); }
            break;
        case Popup::Busy: break;
        case Popup::Copy:
        {
            const int src = session.selected();
            auto move = [&](int d) { int t = copyTarget; for (int i = 0; i < 4; i++) { t = (t + d + 4) % 4; if (t != src) { copyTarget = t; break; } } };
            if (k == kB7) close();
            else if (k == kB8) { close(); doCopy(); }
            else if (k >= kB1 && k <= kB4) { if (k - kB1 != src) copyTarget = k - kB1; showPopup(Popup::Copy); }
            else if (k == kLeft || k == kUp) { move(-1); showPopup(Popup::Copy); }
            else if (k == kRight || k == kDown) { move(1); showPopup(Popup::Copy); }
            break;
        }
        case Popup::Clear:
            if (k == kB7) close();
            else if (k == kB8) { close(); doClear(); }
            else if (k == kB6 && clearAll) { close(); resetAll(); }
            break;
        case Popup::ReplaceSlot:
            if (k == kB7) close();
            else if (k == kB8) { close(); ldConfirmed = true; loadDo(ldAdvance != 0); }
            break;
        case Popup::Unsaved:
            if (k == kB6) { close(); pendingAction = 0; }
            else if (k == kB7)
            {
                close();
                if (pendingAction == 1) { newProject(); }
                else if (pendingAction == 2) startLoad(pendingName);
                pendingAction = 0;
            }
            else if (k == kB8)
            {
                close();
                startSave(); // the pending action runs when the save is done
            }
            break;
        default: close(); break;
    }
}

void App::keyUp(int k)
{
    if ((current == Screen::Sound || current == Screen::SoundAdv) && k == kB1) soundHear(false);
    if (current == Screen::PadSample && k == kB1 && psAuditioning)
    {
        psAuditioning = false;
        hal.padAudition(hal.currentSample(), false);
    }
    if (current == Screen::Glitch && k >= kB1 && k <= kB4)
    {
        hal.bank().cfg[gTrack].held[k - kB1] = false;
        refresh(true);
    }
}

//--------------------------------------------------------------------------------------------
// Keys and the jog wheel on the pages

void App::keyScreen(int k)
{
    switch (current)
    {
        case Screen::Tempo:
            if (k == kB1) { set.tempoTenths = 0; unsaved = true; back(); return; }
            else if (k == kB2) { if (set.tempoTenths == 0) { set.tempoTenths = 800; if (set.bars == 0) set.bars = 4; } sRow = 0; }
            else if (k == kB7) { unsaved = true; back(); return; }
            else if (k == kB8) { set.tempoTenths = tempoEntry; set.countIdx = countEntry; set.waitSound = waitEntry; set.waitDb = waitDbEntry; back(); return; }
            else if (k == kLeft) sRow = clampi(sRow - 1, set.tempoTenths > 0 ? 0 : 1, 3);
            else if (k == kRight) sRow = clampi(sRow + 1, set.tempoTenths > 0 ? 0 : 1, 3);
            else if (k == kUp) jogScreen(1);
            else if (k == kDown) jogScreen(-1);
            else if (k == kJogPress) { if (sRow == 1) set.countIdx = (uint8_t)((set.countIdx + 1) % 5); else if (sRow == 2) set.waitSound = !set.waitSound; }
            break;
        case Screen::Editor:
        {
            if (shift && (k == kUp || k == kDown))
            {
                editZoom = clampi(editZoom + (k == kUp ? 1 : -1), 0, 3);
                editWaveDirty = true;
            }
            else if (k == kLeft || k == kUp) editField(-1);
            else if (k == kRight || k == kDown) editField(1);
            else if (k == kB5)
            {
                if (shift) set.gridIdx = (uint8_t)((set.gridIdx + 1) % 5);
                else set.snapZero = !set.snapZero;
                unsaved = true;
                editWaveDirty = true;
            }
            else if (k == kB6) editTrim();
            else if (k == kB1)
            {
                auditioning = !auditioning;
                for (int t = 0; t < 4; t++) hal.bank().cfg[t].mute = auditioning ? (t != editTrack) : glitchLatched[t];
                if (auditioning && !hal.playerPlaying()) { session.pressPlay(); syncPlayer(); }
            }
            else if (k == kB2)
            {
                editPush();
                const int16_t* d = partData[editTrack];
                const uint16_t pk = loopPeakBlock(d, session.loopFrames(), 0, (int)session.loopFrames());
                if (pk > 100) hal.bank().cfg[editTrack].gain = (0.891f * 32768.0f) / (float)pk;
                editPush();
            }
            else if (k == kB3) { if (editHist > 0) { editHist--; editApply(editStack[editHist]); } }
            else if (k == kB4) { if (editHist + 1 < editHistLen) { editHist++; editApply(editStack[editHist]); } }
            else if (k == kB7) { editApply(editEntry); back(); }
            else if (k == kB8) { unsaved = true; waveDirty[editTrack] = true; back(); }
            break;
        }
        case Screen::Mixer:
        {
            TrackCfg& c = hal.bank().cfg[mxTrack];
            if (k == kLeft) mxTrack = clampi(mxTrack - 1, 0, 3);
            else if (k == kRight) mxTrack = clampi(mxTrack + 1, 0, 3);
            else if (k == kUp) mxField = clampi(mxField - 1, 0, 2);
            else if (k == kDown) mxField = clampi(mxField + 1, 0, 2);
            else if (k == kB1) c.mute = !c.mute;
            else if (k == kB2) c.solo = !c.solo;
            else if (k == kB3) { c.levelDb10 = kDefaultLevelDb10; c.pan = 0; c.tone = 0; }
            else if (k == kB8 || k == kParams) { back(); return; }
            if (k >= kB1 && k <= kB3) unsaved = true;
            break;
        }
        case Screen::PadSample:
            if (k == kLeft) psField = clampi(psField - 1, 0, 5);
            else if (k == kRight) psField = clampi(psField + 1, 0, 5);
            else if (k == kUp) psField = clampi(psField - 3, 0, 5);
            else if (k == kDown) psField = clampi(psField + 3, 0, 5);
            else if (k == kB1) { psAuditioning = true; hal.padAudition(hal.currentSample(), true); }
            else if (k == kB2) padsKeysMode = true;
            else if (k == kB3) padsKeysMode = false;
            else if (k == kB8 || k == kSamplePlay) { if (psAuditioning) { psAuditioning = false; hal.padAudition(hal.currentSample(), false); } back(); return; }
            for (int i = 0; i < 48; i++) padModeKeys[i] = padsKeysMode;
            break;
        case Screen::Chords: chordsKey(k); break;
        case Screen::Calibrate: calibrateKey(k); break;
        case Screen::Sets: setsKey(k); break;
        case Screen::Sound: soundKey(k); break;
        case Screen::SoundAdv: soundAdvKey(k); break;
        case Screen::Load: loadKey(k); break;
        case Screen::Resample: resampleKey(k); break;
        case Screen::Device: deviceKey(k); break;
        case Screen::Glitch: glitchKey(k); break;
        case Screen::Master: masterKey(k); break;
        case Screen::Input:
        {
            const int t = session.selected();
            if (k == kB1) { trackSource[t] = Source::Line; applySource(Source::Line); }
            else if (k == kB2) { trackSource[t] = Source::MicLow; applySource(Source::MicLow); }
            else if (k == kB3) { trackSource[t] = Source::MicHigh; applySource(Source::MicHigh); }
            else if (k == kB5) { trackSource[t] = Source::Pads; applySource(Source::Pads); }
            else if (k == kB4) { const Source s = trackSource[t]; if (s == Source::Line) set.monitorLine = !set.monitorLine; else if (s != Source::Pads) set.monitorMic = !set.monitorMic; hal.setMonitor(monitorFor(s)); }
            else if (k == kB8) { back(); return; }
            else if (k == kUp) jogScreen(1);
            else if (k == kDown) jogScreen(-1);
            break;
        }
        case Screen::Project:
        case Screen::NameEntry: if (current == Screen::Project) projectKey(k); else nameKey(k); break;
        case Screen::Settings:
            if (k == kUp) sRow = clampi(sRow - 1, 0, 12);
            else if (k == kDown) sRow = clampi(sRow + 1, 0, 12);
            else if (k == kLeft) settingsJog(-1);
            else if (k == kRight) settingsJog(1);
            else if (k == kB8) back();
            break;
        case Screen::Export:
            if (job == Job::Export) { if (k == kB8) { job = Job::None; back(); } }
            else if (job == Job::Done) { if (k == kB8) { job = Job::None; back(); } }
            else if (k == kB1) startExport();
            else if (k == kB8) back();
            else if (k == kLeft) exportRepeats = clampi(exportRepeats - 1, 1, 16);
            else if (k == kRight) exportRepeats = clampi(exportRepeats + 1, 1, 16);
            break;
        case Screen::Pads:
            if (k == kUp || k == kDown)
            {
                int slot = hal.currentSample() + (k == kDown ? 1 : -1);
                hal.setCurrentSample(clampi(slot, 0, 47));
            }
            else if (k == kB1) padsKeysMode = true;
            else if (k == kB2) padsKeysMode = false;
            else if (k == kB8) back();
            for (int i = 0; i < 48; i++) padModeKeys[i] = padsKeysMode;
            break;
        case Screen::Memory:
            if (k == kB8) back();
            break;
        default: break;
    }
    if (current != Screen::Main) refresh(true);
}

void App::jogScreen(int delta)
{
    switch (current)
    {
        case Screen::Tempo:
            if (sRow == 0)
            {
                int t = (int)set.tempoTenths;
                if (t == 0) t = 1200;
                else t += delta * (shift ? 10 : 1);
                set.tempoTenths = (uint16_t)clampi(t, 400, 2400);
                while (set.bars > 0 && set.bars > barsFit(set.tempoTenths)) set.bars = (uint8_t)barsStep(set.bars, -1); // only lengths that fit one Part
            }
            else if (sRow == 3) set.bars = (uint8_t)barsStep(set.bars, delta > 0 ? 1 : -1);
            else if (sRow == 1) set.countIdx = (uint8_t)clampi(set.countIdx + (delta > 0 ? 1 : -1), 0, 4);
            else if (sRow == 2) { if (!set.waitSound) set.waitSound = true; else set.waitDb = (int8_t)clampi(set.waitDb + delta, -60, -12); }
            break;
        case Screen::Editor:
        {
            editPush();
            const uint32_t rate = hal.sampleRate();
            const uint32_t loop = session.loopFrames();
            TrackCfg& c = hal.bank().cfg[editTrack];
            const int32_t fine = (int32_t)(rate / 1000);
            const int32_t step = shift ? fine : (set.gridIdx == 0 ? fine * 10 : (int32_t)gridFrames());
            const int d = delta > 0 ? 1 : -1;
            // Cut points: to the grid unless Shift is held, then onto a zero crossing if that is on
            auto cut = [&](int32_t v) -> uint32_t {
                if (v < 0) v = 0;
                if (v > (int32_t)loop) v = (int32_t)loop;
                if (!shift && set.gridIdx > 0)
                {
                    const int32_t gs = (int32_t)gridFrames();
                    v = ((v + gs / 2) / gs) * gs;
                    if (v > (int32_t)loop) v = (int32_t)loop;
                }
                if (set.snapZero && partData[editTrack] != nullptr && loop > 0) v = (int32_t)snapToZero(partData[editTrack], loop, (uint32_t)v, rate / 100);
                return (uint32_t)v;
            };
            const uint32_t endNow = c.trimEnd > loop ? loop : c.trimEnd;
            switch (editFieldIdx)
            {
                case 0: { const uint32_t v = cut((int32_t)c.trimStart + d * step); if (v < endNow) c.trimStart = v; break; }
                case 1: { const uint32_t v = cut((int32_t)endNow + d * step); if (v > c.trimStart) c.trimEnd = v; break; }
                case 2: c.fadeIn = (uint32_t)clampi((int)c.fadeIn + d * (shift ? fine : fine * 10), 0, (int)loop); break;
                case 3: c.fadeOut = (uint32_t)clampi((int)c.fadeOut + d * (shift ? fine : fine * 10), 0, (int)loop); break;
                case 4: c.gain *= (d > 0 ? 1.0f : 0.0f) * (shift ? 1.0116f : 1.0593f) + (d > 0 ? 0.0f : 1.0f) * (shift ? 0.9885f : 0.9441f); break; // 0.1 dB or 0.5 dB
                case 5:
                {
                    const bool keep = moveAll;
                    moveAll = false;
                    moveStep(editTrack, d, shift);
                    moveAll = keep;
                    break;
                }
                default: break;
            }
            pushBank();
            editWaveDirty = true;
            break;
        }
        case Screen::Glitch:
        {
            TrackCfg& c = hal.bank().cfg[gTrack];
            FxSlot& s = c.slot[gSlot];
            if (shift)
            {
                int type = (int)s.type + (delta > 0 ? 1 : -1);
                if (type < 0) type = (int)FxType::kCount - 1;
                if (type >= (int)FxType::kCount) type = 0;
                s.type = (FxType)type;
            }
            else s.chance = (uint8_t)clampi((int)s.chance + delta * 5, 0, 100);
            break;
        }
        case Screen::Mixer:
        {
            TrackCfg& c = hal.bank().cfg[mxTrack];
            const int d = delta > 0 ? 1 : -1;
            if (mxField == 0) c.levelDb10 = (int16_t)clampi(c.levelDb10 + d * (shift ? 1 : 5), -600, 60);
            else if (mxField == 1) c.pan = (int8_t)clampi(c.pan + d * (shift ? 1 : 5), -100, 100);
            else c.tone = (int8_t)clampi(c.tone + d * (shift ? 1 : 5), -100, 100);
            unsaved = true;
            break;
        }
        case Screen::PadSample: psChange(delta); break;
        case Screen::Chords: chordsJog(delta); break;
        case Screen::Sets: setsJog(delta); break;
        case Screen::Sound: soundJog(delta); break;
        case Screen::SoundAdv: soundAdvJog(delta); break;
        case Screen::Load: loadJog(delta); break;
        case Screen::Resample: resampleJog(delta); break;
        case Screen::Device: deviceJog(delta); break;
        case Screen::Master: masterJog(delta); break;
        case Screen::Input:
        {
            const Source s = trackSource[session.selected()];
            if (shift) set.waitDb = (int8_t)clampi(set.waitDb + (delta > 0 ? 1 : -1), -60, -12);
            else if (s != Source::Pads) hal.setGain(s, clampi(hal.gain(s) + delta * (shift ? 5 : 1), 0, 100));
            break;
        }
        case Screen::Settings: settingsJog(delta > 0 ? 1 : -1); break;
        case Screen::Export: exportRepeats = clampi(exportRepeats + (delta > 0 ? 1 : -1), 1, 16); break;
        case Screen::Pads: hal.setCurrentSample(clampi(hal.currentSample() + (delta > 0 ? 1 : -1), 0, 47)); break;
        case Screen::Project: listSel = clampi(listSel + (delta > 0 ? 1 : -1), 0, listCount > 0 ? listCount - 1 : 0); break;
        case Screen::NameEntry: nameJog(delta); break;
        default: break;
    }
    unsaved = unsaved || current == Screen::Glitch || current == Screen::Master || current == Screen::Settings;
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Part Editor

App::EditParams App::editNow() const
{
    const TrackCfg& c = const_cast<App*>(this)->hal.bank().cfg[editTrack];
    return EditParams{ c.trimStart, c.trimEnd, c.fadeIn, c.fadeOut, c.gain, userOffset[editTrack] };
}

void App::editApply(const EditParams& e)
{
    TrackCfg& c = hal.bank().cfg[editTrack];
    c.trimStart = e.trimStart;
    c.trimEnd = e.trimEnd;
    c.fadeIn = e.fadeIn;
    c.fadeOut = e.fadeOut;
    c.gain = e.gain;
    userOffset[editTrack] = e.userOffset;
    pushBank();
    editWaveDirty = true;
}

void App::editEnter()
{
    editFieldIdx = 0;
    editZoom = 0;
    editEntry = editNow();
    editStack[0] = editEntry;
    editHist = 0;
    editHistLen = 1;
    editWaveDirty = true;
    auditioning = false;
    for (int t = 0; t < 4; t++) glitchLatched[t] = hal.bank().cfg[t].mute;
}

void App::editPush()
{
    if (editHist < 15)
    {
        editStack[editHist + 1] = editNow();
        editHist++;
        editHistLen = editHist + 1;
    }
}

void App::editField(int delta)
{
    editFieldIdx = (editFieldIdx + delta + 6) % 6;
}

uint32_t App::zoomSpan() const
{
    const uint32_t loop = session.loopFrames();
    if (editFieldIdx >= 4 || editZoom == 0) return loop;
    const uint32_t s = loop >> (2 * editZoom);
    return s < 100 ? (loop < 100 ? loop : 100) : s;
}

uint32_t App::zoomStart() const
{
    const uint32_t loop = session.loopFrames();
    const uint32_t span = zoomSpan();
    if (span >= loop) return 0;
    const TrackCfg& c = const_cast<App*>(this)->hal.bank().cfg[editTrack];
    const uint32_t endNow = c.trimEnd > loop ? loop : c.trimEnd;
    uint32_t center = c.trimStart;
    if (editFieldIdx == 1) center = endNow;
    else if (editFieldIdx == 2) center = c.trimStart + c.fadeIn;
    else if (editFieldIdx == 3) center = endNow > c.fadeOut ? endNow - c.fadeOut : 0;
    uint32_t start = center > span / 2 ? center - span / 2 : 0;
    if (start + span > loop) start = loop - span;
    return start;
}

void App::computeEditWave()
{
    const int16_t* d = partData[editTrack];
    const uint32_t frames = session.loopFrames();
    const uint32_t ws = zoomStart(), span = zoomSpan();
    for (int c = 0; c < 760; c++)
    {
        uint16_t peak = 0;
        if (d != nullptr && frames > 0)
        {
            const uint32_t from = ws + (uint32_t)(((uint64_t)c * span) / 760);
            const uint32_t to = ws + (uint32_t)(((uint64_t)(c + 1) * span) / 760);
            if (to > from) peak = loopPeakBlock(d + from, to - from, 0, (int)(to - from));
        }
        const float norm = __builtin_sqrtf((float)peak / 32767.0f);
        editWave[c] = (uint8_t)(norm * 255.0f);
    }
    editWaveDirty = false;
    if (set.waveBitmap) { uploadPageMask(editWave, 86); padWaveFor = -1; } // the pad page shares the picture
}

// The big waveform of a page as a one-bit picture in the display chip (slot 12), a band of rows at a time
void App::uploadPageMask(const uint8_t* peaks, int maxHalf)
{
    constexpr int kBand = 16;
    uint8_t buf[kBand * kPageMaskStride];
    const int mid = kPageMaskH / 2;
    for (int r0 = 0; r0 < kPageMaskH; r0 += kBand)
    {
        const int rows = (kPageMaskH - r0) < kBand ? (kPageMaskH - r0) : kBand;
        for (int i = 0; i < rows * kPageMaskStride; i++) buf[i] = 0;
        for (int c = 0; c < kPageMaskW; c++)
        {
            const int half = 1 + (peaks[c] * (maxHalf - 1)) / 255;
            for (int r = 0; r < rows; r++)
            {
                const int row = r0 + r;
                const int dist = row < mid ? mid - 1 - row : row - mid;
                if (dist < half) buf[r * kPageMaskStride + (c >> 3)] |= (uint8_t)(0x80 >> (c & 7));
            }
        }
        hal.maskWrite(kPageMaskSlot, (uint32_t)(r0 * kPageMaskStride), buf, (uint32_t)(rows * kPageMaskStride));
    }
}

// Cuts the silence off the start and the end of the Part (the Part itself is not changed)
void App::editTrim()
{
    const int16_t* d = partData[editTrack];
    const uint32_t loop = session.loopFrames();
    if (d == nullptr || loop == 0) return;
    const uint32_t rate = hal.sampleRate();
    const int on = findFirstOnset(d, 0, loop, rate);
    const int off = findLastSound(d, 0, loop, rate);
    if (on < 0 || off < 0 || off <= on) return;
    editPush();
    TrackCfg& c = hal.bank().cfg[editTrack];
    uint32_t start = (uint32_t)on > rate / 200 ? (uint32_t)on - rate / 200 : 0;
    if (set.snapZero) start = snapToZero(d, loop, start, rate / 200);
    c.trimStart = start;
    c.trimEnd = (uint32_t)off;
    editPush();
    pushBank();
    editWaveDirty = true;
}

//--------------------------------------------------------------------------------------------
// Glitch, Master, Settings

void App::glitchKey(int k)
{
    TrackCfg& c = hal.bank().cfg[gTrack];
    // Shift + B1 clears the track, Shift + B2 clears all, Shift + B3 switches the pitch style
    if (shift && k >= kB1 && k <= kB3)
    {
        if (k == kB1) for (int s = 0; s < 4; s++) { c.slot[s].on = false; c.held[s] = false; }
        else if (k == kB2) for (int t = 0; t < 4; t++) for (int s = 0; s < 4; s++) { hal.bank().cfg[t].slot[s].on = false; hal.bank().cfg[t].held[s] = false; }
        else { c.granular = !c.granular; unsaved = true; }
        return;
    }
    if (k >= kB1 && k <= kB4) { c.held[k - kB1] = true; return; }
    switch (k)
    {
        case kB5: case kB6: case kB7: case kB8: gTrack = k - kB5; break; // as on the main screen
        case kLeft: gTrack = clampi(gTrack - 1, 0, 3); break;
        case kRight: gTrack = clampi(gTrack + 1, 0, 3); break;
        case kUp: case kDown:
        {
            const int d = k == kDown ? 1 : -1;
            if (!shift) { gSlot = clampi(gSlot + d, 0, 3); break; }
            FxSlot& s = c.slot[gSlot];
            if (s.type == FxType::Pitch) s.semis = (int8_t)stepInterval(s.semis, -d);
            else if (s.type == FxType::PitchSteps) s.semis = (int8_t)clampi(s.semis - d, 0, 4);
            else s.amount = (uint8_t)clampi((int)s.amount - d * 5, 0, s.type == FxType::Pan ? 200 : 100);
            unsaved = true;
            break;
        }
        case kJogPress: { FxSlot& s = c.slot[gSlot]; s.on = !s.on; unsaved = true; break; }
        case kPerformance: back(); return;
        default: break;
    }
}

void App::masterKey(int k)
{
    LofiCfg& c = hal.lofi().cfg;
    switch (k)
    {
        case kUp: mRow = clampi(mRow - 1, 0, 4); mSub = 0; break;
        case kDown: mRow = clampi(mRow + 1, 0, 4); mSub = 0; break;
        case kLeft: mSub = 0; break;
        case kRight: if (mRow == 2) mSub = 1; break;
        case kJogPress:
            if (mRow == 0) c.glueOn = !c.glueOn;
            else if (mRow == 1) c.satOn = !c.satOn;
            else if (mRow == 2) c.wowOn = !c.wowOn;
            else if (mRow == 3) c.crushOn = !c.crushOn;
            unsaved = true;
            break;
        case kB1: c.bypass = !c.bypass; break;
        case kB8: back(); return;
        default: break;
    }
}

void App::masterJog(int delta)
{
    LofiCfg& c = hal.lofi().cfg;
    const int d = delta * 5;
    switch (mRow)
    {
        case 0: c.glue = (uint8_t)clampi(c.glue + d, 0, 100); break;
        case 1: c.sat = (uint8_t)clampi(c.sat + d, 0, 100); break;
        case 2:
            if (mSub == 0) c.wowDepth = (uint8_t)clampi(c.wowDepth + d, 0, 100);
            else c.wowSpeed = (uint8_t)clampi(c.wowSpeed + delta, 1, 50);
            break;
        case 3: c.crushBits = (uint8_t)clampi(c.crushBits - delta, 4, 16); break;
        case 4: c.volume = (uint8_t)clampi(c.volume + d, 0, 100); break;
        default: break;
    }
}

void App::settingsJog(int delta)
{
    switch (sRow)
    {
        case 0: set.loopOnce = !set.loopOnce; pushBank(); break;
        case 1: set.click = !set.click; break;
        case 2: set.countIdx = (uint8_t)clampi(set.countIdx + (delta > 0 ? 1 : -1), 0, 4); break;
        case 3: set.waitSound = !set.waitSound; break;
        case 4: set.gridIdx = (uint8_t)clampi(set.gridIdx + delta, 0, 4); break;
        case 5: set.snapZero = !set.snapZero; break;
        case 6: set.windowIdx = (uint8_t)clampi(set.windowIdx + delta, 0, 4); break;
        case 7: set.preRollIdx = (uint8_t)clampi(set.preRollIdx + delta, 0, 4); break;
        case 8: set.roundIdx = (uint8_t)clampi(set.roundIdx + delta, 0, 2); break;
        case 9: set.waitDb = (int8_t)clampi(set.waitDb + (delta > 0 ? 1 : -1), -60, -12); break;
        case 10: trackSource[session.selected()] = (Source)(((int)trackSource[session.selected()] + (delta > 0 ? 1 : 3)) % 4); applySource(trackSource[session.selected()]); break;
        case 11: { const Source s = trackSource[session.selected()]; if (s != Source::Pads) hal.setGain(s, clampi(hal.gain(s) + delta * 5, 0, 100)); break; }
        case 12: if (delta > 0) set.monitorLine = !set.monitorLine; else set.monitorMic = !set.monitorMic; hal.setMonitor(monitorFor(trackSource[session.selected()])); break;
        default: break;
    }
    unsaved = true;
}

//--------------------------------------------------------------------------------------------
// Project, name entry, save, load, export

static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_";

void App::refreshProjectList()
{
    char names[16][24];
    const int n = hal.fileList("Projects", names, 16);
    listCount = 0;
    for (int i = 0; i < n && listCount < 16; i++)
    {
        int len = 0;
        while (names[i][len] != 0) len++;
        if (len > 4 && names[i][len - 4] == '.' && names[i][len - 3] == 'f' && names[i][len - 2] == 't' && names[i][len - 1] == 'p')
        {
            for (int k = 0; k < len - 4 && k < 23; k++) list[listCount][k] = names[i][k];
            list[listCount][len - 4 < 23 ? len - 4 : 23] = 0;
            char path[48];
            fmt(path, sizeof path, "Projects/%s.ftp", list[listCount]);
            uint8_t hh[16] = {};
            listBars[listCount] = 0;
            listTempo[listCount] = 0;
            if (hal.fileRead(path, 0, hh, 16) == 16 && hh[0] == 'F' && hh[1] == 'T')
            {
                listBars[listCount] = hh[8];
                listTempo[listCount] = (uint16_t)(hh[12] | (hh[13] << 8));
            }
            listCount++;
        }
    }
    if (listSel >= listCount) listSel = listCount > 0 ? listCount - 1 : 0;
}

void App::newProject(bool everything)
{
    moveMode = false;
    lastOp = LastOp();
    focusUser = false;
    hal.playerStop();
    session.reset();
    for (int t = 0; t < 4; t++)
    {
        partData[t] = nullptr;
        alignShift[t] = userOffset[t] = 0;
        hasUndoTake[t] = false;
        TrackCfg fresh;
        FxSlot keep[4];
        for (int s = 0; s < 4; s++) keep[s] = hal.bank().cfg[t].slot[s];
        hal.bank().cfg[t] = fresh;
        for (int s = 0; s < 4; s++) { hal.bank().cfg[t].slot[s] = keep[s]; hal.bank().cfg[t].slot[s].on = false; }
        if (everything)
        {
            // the glitch effects go back to how they start, too
            FxSlot defaults[4] = { { FxType::RandomJumps, false, 50, 0, 50 }, { FxType::PitchSteps, false, 50, 0, 40 }, { FxType::Stutter, false, 50, 7, 100 }, { FxType::Reverse, false, 50, 7, 100 } };
            for (int s = 0; s < 4; s++) hal.bank().cfg[t].slot[s] = defaults[s];
        }
        hal.playerSetTrack(t, nullptr);
        waveDirty[t] = true;
        trackSource[t] = Source::Line;
    }
    if (everything) hal.lofi().cfg = LofiCfg(); // and the lo-fi master
    if (hal.startsAtSetTempo()) newProjectDefaults();
    else { set.tempoTenths = 0; set.bars = 0; }
    fmt(projectName, sizeof(projectName), "Untitled");
    unsaved = false;
    pushBank();
    back();
}

void App::projectKey(int k)
{
    auto askThen = [&](int action, const char* name) {
        pendingAction = action;
        if (name != nullptr) fmt(pendingName, sizeof(pendingName), "%s", name);
        if (unsaved) showPopup(Popup::Unsaved);
        else if (action == 1) newProject();
        else if (action == 2) startLoad(pendingName);
    };
    switch (k)
    {
        case kUp: listSel = clampi(listSel - 1, 0, listCount > 0 ? listCount - 1 : 0); break;
        case kDown: listSel = clampi(listSel + 1, 0, listCount > 0 ? listCount - 1 : 0); break;
        case kB1: askThen(1, nullptr); break;
        case kB2: if (listCount > 0) askThen(2, list[listSel]); break;
        case kB3: startSave(); break;
        case kB4: nameForSaveAs = true; fmt(nameEdit, sizeof(nameEdit), "%s", projectName); nameCursor = 0; nameChar = 0; open(Screen::NameEntry); break;
        case kB5: nameForSaveAs = false; fmt(nameEdit, sizeof(nameEdit), "%s", projectName); nameCursor = 0; nameChar = 0; open(Screen::NameEntry); break;
        case kB6: exportRepeats = 1; job = Job::None; open(Screen::Export); break;
        case kB8: back(); return;
        default: break;
    }
}

void App::nameKey(int k)
{
    int len = 0;
    while (nameEdit[len] != 0) len++;
    switch (k)
    {
        case kLeft: nameCursor = clampi(nameCursor - 1, 0, 15); break;
        case kRight: nameCursor = clampi(nameCursor + 1, 0, 15); break;
        case kUp: case kDown:
        {
            // jump between letters, digits and symbols
            const int c = nameEdit[nameCursor];
            int g = (c >= 'A' && c <= 'Z') ? 0 : (c >= '0' && c <= '9') ? 1 : 2;
            g = (g + (k == kUp ? 2 : 1)) % 3;
            nameEdit[nameCursor] = g == 0 ? 'A' : g == 1 ? '0' : ' ';
            if (nameCursor >= len) { for (int i = len; i < nameCursor; i++) nameEdit[i] = ' '; nameEdit[nameCursor + 1] = 0; }
            break;
        }
        case kB1:
            if (nameCursor > 0 || len > 0)
            {
                int at = nameCursor < len ? nameCursor : len;
                if (at > 0) { for (int i = at - 1; i < len; i++) nameEdit[i] = nameEdit[i + 1]; nameCursor = clampi(nameCursor - 1, 0, 15); }
            }
            break;
        case kB2:
            if (len < 16) { for (int i = len; i >= nameCursor; i--) nameEdit[i + 1] = nameEdit[i]; nameEdit[nameCursor] = ' '; nameCursor = clampi(nameCursor + 1, 0, 15); }
            break;
        case kB7: if (nameForResample) { nameForResample = false; open(Screen::Resample); } else open(Screen::Project); return;
        case kB8:
        {
            int e = 0;
            while (nameEdit[e] != 0) e++;
            while (e > 0 && nameEdit[e - 1] == ' ') nameEdit[--e] = 0;
            if (e == 0) break;
            if (nameForResample)
            {
                fmt(rsName, sizeof(rsName), "%s", nameEdit);
                rsNameAuto = false;
                nameForResample = false;
                open(Screen::Resample);
                return;
            }
            fmt(projectName, sizeof(projectName), "%s", nameEdit);
            unsaved = true;
            if (nameForSaveAs) { open(Screen::Project); startSave(); }
            else open(Screen::Project);
            return;
        }
        default: break;
    }
}

void App::nameJog(int delta)
{
    int len = 0;
    while (nameEdit[len] != 0) len++;
    if (nameCursor >= len)
    {
        for (int i = len; i < nameCursor; i++) nameEdit[i] = ' ';
        nameEdit[nameCursor] = 'A';
        nameEdit[nameCursor + 1] = 0;
        return;
    }
    const int n = (int)sizeof(kChars) - 1;
    int idx = 0;
    for (int i = 0; i < n; i++) if (kChars[i] == nameEdit[nameCursor]) idx = i;
    idx = (idx + (delta > 0 ? 1 : n - 1)) % n;
    nameEdit[nameCursor] = kChars[idx];
}

// ---- file format: a 512 byte header, then the audio of each Part (16-bit mono, one loop each)
namespace
{
struct W
{
    uint8_t* b;
    int n = 0;
    void u8(uint32_t v) { b[n++] = (uint8_t)v; }
    void u16(uint32_t v) { u8(v); u8(v >> 8); }
    void u32(uint32_t v) { u16(v); u16(v >> 16); }
    void f32(float f) { union { float f; uint32_t u; } x = { f }; u32(x.u); }
};
struct R
{
    const uint8_t* b;
    int n = 0;
    uint32_t u8() { return b[n++]; }
    uint32_t u16() { const uint32_t a = u8(); return a | (u8() << 8); }
    uint32_t u32() { const uint32_t a = u16(); return a | (u16() << 16); }
    float f32() { union { float f; uint32_t u; } x; x.u = u32(); return x.f; }
};
constexpr int kHeader = 512;
} // namespace

// A project remembers the chord set it was made with, in a small file beside it (Projects/NAME.chd), so it
// reopens sounding as you left it. A project without one keeps the settings that are on the pads.
void App::projectChordsWrite(const char* name)
{
    char path[64];
    fmt(path, sizeof path, "Projects/%s.chd", name);
    uint8_t b[4 + 138];
    b[0] = 'C'; b[1] = 'H'; b[2] = '0'; b[3] = '1';
    packSet(b + 4);
    hal.fileWrite(path, b, sizeof b, false);
}

void App::projectChordsRead(const char* name)
{
    char path[64];
    fmt(path, sizeof path, "Projects/%s.chd", name);
    uint8_t b[4 + 138];
    if (hal.fileSize(path) != sizeof b || hal.fileRead(path, 0, b, sizeof b) != sizeof b) return;
    if (b[0] != 'C' || b[1] != 'H' || b[2] != '0' || b[3] != '1') return;
    chordsReleaseAll();
    unpackSet(b + 4);
    soundApply();
    soundMark();
}

void App::startSave()
{
    if (!hal.sdPresent()) { showPopup(Popup::SdMissing); return; }
    fmt(jobPath, sizeof(jobPath), "Projects/%s.ftp", projectName);
    uint8_t h[kHeader];
    for (int i = 0; i < kHeader; i++) h[i] = 0;
    W w{ h };
    w.u8('F'); w.u8('T'); w.u8('2'); w.u8(0);
    w.u32(session.loopFrames());
    w.u8((uint32_t)session.bars());
    uint8_t mask = 0;
    for (int t = 0; t < 4; t++) if (session.hasPart(t)) mask |= (uint8_t)(1u << t);
    w.u8(mask);
    w.u8((set.loopOnce ? 1u : 0u) | (set.click ? 2u : 0u) | (set.countIdx > 0 ? 4u : 0u) | (set.snapZero ? 8u : 0u) | ((uint32_t)set.countIdx << 4) | (set.waitSound ? 128u : 0u));
    w.u8(set.gridIdx);
    w.u16(set.tempoTenths);
    for (int t = 0; t < 4; t++)
    {
        const TrackCfg& c = hal.bank().cfg[t];
        w.u8((uint32_t)trackSource[t]); w.u8(c.mute); w.u8(c.granular); w.u8(0);
        w.u32((uint32_t)alignShift[t]); w.u32((uint32_t)userOffset[t]);
        w.u32(c.trimStart); w.u32(c.trimEnd); w.u32(c.fadeIn); w.u32(c.fadeOut); w.f32(c.gain);
        for (int s = 0; s < 4; s++) { w.u8((uint32_t)c.slot[s].type); w.u8(c.slot[s].on); w.u8(c.slot[s].amount); w.u8((uint8_t)c.slot[s].semis); w.u8(c.slot[s].chance); }
    }
    const LofiCfg& l = hal.lofi().cfg;
    w.u8(l.bypass); w.u8(l.glueOn); w.u8(l.satOn); w.u8(l.wowOn); w.u8(l.crushOn);
    w.u8(l.glue); w.u8(l.sat); w.u8(l.wowDepth); w.u8(l.wowSpeed); w.u8(l.crushBits); w.u8(l.volume);
    for (int i = 0; i < 48; i++) w.u8(padModeKeys[i] ? 1 : 0);
    for (int t = 0; t < 4; t++) { const TrackCfg& c = hal.bank().cfg[t]; w.u16((uint16_t)c.levelDb10); w.u8((uint8_t)c.pan); w.u8((uint8_t)c.tone); w.u8(c.solo ? 1 : 0); }
    w.u8(set.bars);
    if (!hal.fileWrite(jobPath, h, kHeader, false)) { showPopup(Popup::SdMissing); return; }
    projectChordsWrite(projectName);
    job = Job::Save;
    jobTrack = 0;
    jobDone = 0;
    jobTotal = (uint32_t)__builtin_popcount(mask) * session.loopFrames() * 2;
    jobStage = 0;
    current = Screen::Export;
    exportPercent = 0;
    redraw[4] = true;
}

void App::startLoad(const char* name)
{
    lastOp = LastOp();
    if (!hal.sdPresent()) { showPopup(Popup::SdMissing); return; }
    fmt(jobPath, sizeof(jobPath), "Projects/%s.ftp", name);
    uint8_t h[kHeader];
    if (hal.fileRead(jobPath, 0, h, kHeader) != (uint32_t)kHeader || h[0] != 'F' || h[1] != 'T' || h[2] != '2') { showPopup(Popup::Message, 3); return; }
    projectChordsRead(name);
    R r{ h };
    r.u32();
    const uint32_t loop = r.u32();
    const int bars = (int)r.u8();
    const uint8_t mask = (uint8_t)r.u8();
    const uint32_t flags = r.u8();
    const uint32_t gridByte = r.u8();
    set.tempoTenths = (uint16_t)r.u16();
    set.loopOnce = flags & 1;
    set.click = flags & 2;
    { const uint32_t ci = (flags >> 4) & 7; set.countIdx = (uint8_t)(ci ? (ci > 4 ? 4 : ci) : ((flags & 4) ? 3 : 0)); }
    set.waitSound = (flags & 128) != 0;
    set.snapZero = flags & 8;
    set.gridIdx = (uint8_t)(gridByte % 5);
    hal.playerStop();
    moveMode = false;
    for (int t = 0; t < 4; t++) noteAt[t] = -2;
    for (int t = 0; t < 4; t++)
    {
        TrackCfg& c = hal.bank().cfg[t];
        trackSource[t] = (Source)(r.u8() % 4);
        c.mute = r.u8() != 0; c.granular = r.u8() != 0; r.u8();
        alignShift[t] = (int32_t)r.u32(); userOffset[t] = (int32_t)r.u32();
        c.trimStart = r.u32(); c.trimEnd = r.u32(); c.fadeIn = r.u32(); c.fadeOut = r.u32(); c.gain = r.f32();
        for (int s = 0; s < 4; s++) { c.slot[s].type = (FxType)(r.u8() % (int)FxType::kCount); c.slot[s].on = r.u8() != 0; c.slot[s].amount = (uint8_t)r.u8(); c.slot[s].semis = (int8_t)r.u8(); c.slot[s].chance = (uint8_t)r.u8(); c.held[s] = false; }
        partData[t] = nullptr;
    }
    LofiCfg& l = hal.lofi().cfg;
    l.bypass = r.u8() != 0; l.glueOn = r.u8() != 0; l.satOn = r.u8() != 0; l.wowOn = r.u8() != 0; l.crushOn = r.u8() != 0;
    l.glue = (uint8_t)r.u8(); l.sat = (uint8_t)r.u8(); l.wowDepth = (uint8_t)r.u8(); l.wowSpeed = (uint8_t)r.u8(); l.crushBits = (uint8_t)r.u8(); l.volume = (uint8_t)r.u8();
    for (int i = 0; i < 48; i++) padModeKeys[i] = r.u8() != 0;
    for (int t = 0; t < 4; t++) { TrackCfg& c = hal.bank().cfg[t]; c.levelDb10 = (int16_t)r.u16(); c.pan = (int8_t)r.u8(); c.tone = (int8_t)r.u8(); c.solo = r.u8() != 0; }
    set.bars = (uint8_t)r.u8();
    session.restore(loop, bars, mask);
    fmt(projectName, sizeof(projectName), "%s", name);
    for (int t = 0; t < 4; t++) hasUndoTake[t] = false;
    job = Job::Load;
    jobTrack = 0;
    jobDone = 0;
    jobTotal = (uint32_t)__builtin_popcount(mask) * loop * 2;
    jobStage = 0;
    current = Screen::Export;
    exportPercent = 0;
    redraw[4] = true;
}

void App::startExport()
{
    if (!hal.sdPresent()) { showPopup(Popup::SdMissing); return; }
    if (session.loopFrames() == 0) { showPopup(Popup::Message, 4); return; }
    fmt(exportFile, sizeof(exportFile), "Exports/%s.wav", projectName);
    exportBank = hal.bank();
    exportLofi.cfg = hal.lofi().cfg;
    exportLofi.reset();
    exportBank.resetState();
    for (int t = 0; t < 4; t++) { exportBank.data[t] = session.hasPart(t) ? partData[t] : nullptr; for (int s = 0; s < 4; s++) exportBank.cfg[t].held[s] = false; }
    exportBank.loop = session.loopFrames();
    jobTotal = session.loopFrames() * (uint32_t)exportRepeats;
    uint8_t wh[44];
    wavHeader(wh, jobTotal * 2, hal.sampleRate());
    if (!hal.fileWrite(exportFile, wh, 44, false)) { showPopup(Popup::SdMissing); return; }
    job = Job::Export;
    jobDone = 0;
    exportPercent = 0;
}

uint32_t App::partOffset(int track) const
{
    uint32_t n = 0;
    for (int t = 0; t < track; t++) if (session.hasPart(t)) n += session.loopFrames() * 2;
    return n;
}

void App::serviceJob()
{
    if (job == Job::None || job == Job::Done) return;
    if (job == Job::Backup)
    {
        // keep the Part that is about to be replaced on the card, 16,384 frames a pass
        constexpr uint32_t kChunk = 16384;
        const uint32_t loop = session.loopFrames();
        const uint32_t n = (loop - bkDone) < kChunk ? (loop - bkDone) : kChunk;
        char path[24];
        fmt(path, sizeof(path), "undo/t%d.raw", bkDst);
        if (!hal.fileWrite(path, partData[bkDst] + bkDone, n * 2, bkDone > 0)) { job = Job::None; popupKind = Popup::None; redraw[5] = false; dirtyAll(); lastOp = LastOp(); showPopup(Popup::SdMissing); return; }
        bkDone += n;
        if (bkDone >= loop)
        {
            const int32_t trailer[2] = { alignShift[bkDst], userOffset[bkDst] };
            hal.fileWrite(path, trailer, 8, true);
            job = Job::None;
            popupKind = Popup::None;
            redraw[5] = false;
            dirtyAll();
            finishCopy();
        }
        else showPopup(Popup::Busy, (int)((uint64_t)bkDone * 100 / loop));
        return;
    }
    if (job == Job::Export || job == Job::Resample)
    {
        const char* outFile = job == Job::Export ? exportFile : rsPath;
        constexpr int kChunk = 1024;
        float l[kChunk], r[kChunk];
        int16_t out[kChunk];
        for (int round = 0; round < 6 && jobDone < jobTotal; round++)
        {
            const uint32_t n = (jobTotal - jobDone) < (uint32_t)kChunk ? (jobTotal - jobDone) : (uint32_t)kChunk;
            exportBank.render(jobDone % exportBank.loop, (int)n, l, r);
            exportLofi.process(l, r, (int)n, hal.sampleRate());
            float pl = 0, pr = 0;
            for (uint32_t i = 0; i < n; i++)
            {
                const float al = l[i] < 0 ? -l[i] : l[i], ar = r[i] < 0 ? -r[i] : r[i];
                if (al > pl) pl = al;
                if (ar > pr) pr = ar;
                float m = (l[i] + r[i]) * 0.5f;
                if (m > 1.0f) m = 1.0f;
                if (m < -1.0f) m = -1.0f;
                out[i] = (int16_t)(m * 32767.0f);
            }
            hal.fileWrite(outFile, out, n * 2, true);
            jobDone += n;
            if (job == Job::Resample)
            {
                const int dl = pl > 0.0001f ? ampToDb(pl) : -60, dr = pr > 0.0001f ? ampToDb(pr) : -60;
                rsLevelL = dl > rsLevelL ? dl : rsLevelL - 4;
                rsLevelR = dr > rsLevelR ? dr : rsLevelR - 4;
            }
        }
        exportPercent = (int)((uint64_t)jobDone * 100 / (jobTotal ? jobTotal : 1));
        if (jobDone >= jobTotal)
        {
            if (job == Job::Resample) resampleFinish();
            else job = Job::Done;
        }
        redraw[4] = true;
        return;
    }
    // Saving or loading: one chunk of one Part per pass
    constexpr uint32_t kChunkFrames = 16384;
    const uint32_t loop = session.loopFrames();
    while (jobTrack < 4 && !session.hasPart(jobTrack)) jobTrack++;
    if (jobTrack >= 4)
    {
        if (job == Job::Load)
        {
            for (int t = 0; t < 4; t++) waveDirty[t] = true;
            unsaved = false;
            pushBank();
            if (session.loopFrames() > 0) { session.pressPlay(); }
        }
        else
        {
            unsaved = false;
        }
        job = Job::None;
        refreshProjectList();
        const int act = pendingAction;
        pendingAction = 0;
        current = Screen::Main;
        dirtyAll();
        if (job == Job::None && act == 1) newProject();
        else if (act == 2) startLoad(pendingName);
        else if (act == 3) { current = Screen::Device; fwSavedFirst = true; firmwareGo(); }
        syncPlayer();
        refresh(true);
        return;
    }
    const uint32_t frames = (loop - jobStage) < kChunkFrames ? (loop - jobStage) : kChunkFrames;
    if (job == Job::Save)
    {
        hal.fileWrite(jobPath, partData[jobTrack] + jobStage, frames * 2, true);
    }
    else
    {
        int16_t* dst = hal.region(jobTrack) + jobStage;
        hal.fileRead(jobPath, (uint32_t)kHeader + partOffset(jobTrack) + jobStage * 2, dst, frames * 2);
        partData[jobTrack] = hal.region(jobTrack);
    }
    jobStage += frames;
    jobDone += frames * 2;
    if (jobStage >= loop)
    {
        jobStage = 0;
        jobTrack++;
    }
    exportPercent = (int)((uint64_t)jobDone * 100 / (jobTotal ? jobTotal : 1));
    redraw[4] = true;
}
} // namespace ft

namespace ft
{
// The jog changes the control in focus on the pad sample page
void App::psChange(int delta)
{
    const int slot = hal.currentSample();
    PadSample s;
    if (!hal.padSample(slot, s)) return;
    const int d = delta > 0 ? 1 : -1;
    const uint32_t frames = s.frames > 0 ? s.frames : 1;
    int step = (int)((uint64_t)65535 * hal.sampleRate() / 200 / frames); // 5 ms
    if (step < 1) step = 1;
    if (shift) step = step / 10 < 1 ? 1 : step / 10;
    switch (psField)
    {
        case 0:
            if (shift) { int c = s.tuneCt + d; if (c > 100) { c = 0; s.tuneSt = (int8_t)clampi(s.tuneSt + 1, -24, 24); } if (c < -100) { c = 0; s.tuneSt = (int8_t)clampi(s.tuneSt - 1, -24, 24); } s.tuneCt = (int8_t)c; }
            else s.tuneSt = (int8_t)clampi(s.tuneSt + d, -24, 24);
            break;
        case 1: s.volume = (uint8_t)clampi((int)s.volume + d * (shift ? 1 : 2), 0, 100); break;
        case 2: s.pan = (int8_t)clampi(s.pan + d * (shift ? 1 : 5), -100, 100); break;
        case 3: s.loop = !s.loop; break;
        case 4: s.start = (uint16_t)clampi((int)s.start + d * step, 0, (int)s.end - 64 > 0 ? (int)s.end - 64 : 0); break;
        case 5: s.end = (uint16_t)clampi((int)s.end + d * step, (int)s.start + 64, 65535); break;
        default: break;
    }
    hal.setPadSample(slot, s);
    unsaved = true;
}

//--------------------------------------------------------------------------------------------
// C2 Load samples (Sample Load button)

static bool pathIs(const char* a, const char* b)
{
    while (*a != 0 && *a == *b) { a++; b++; }
    return *a == *b;
}
static size_t textLen(const char* s) { size_t n = 0; while (s[n] != 0) n++; return n; }
static char* lastSlash(char* s)
{
    char* r = nullptr;
    for (; *s != 0; s++) if (*s == '/') r = s;
    return r;
}

void App::loadOpen()
{
    ldCard = hal.sdPresent();
    ldCount = 0;
    ldSel = ldTop = 0;
    ldWaveOk = false;
    ldPreviewing = false;
    if (!ldCard) { showPopup(Popup::SdMissing); return; }
    // the first empty slot is the destination
    ldSlot = hal.currentSample() >= 0 ? hal.currentSample() : 0;
    for (int i = 0; i < 48; i++)
    {
        char n[4];
        if (!hal.sampleName(i, n, sizeof n)) { ldSlot = i; break; }
    }
    loadList();
}

void App::loadList()
{
    ldCount = hal.browse(ldDir, ldEntry, kLdMax);
    if (ldCount < 0) ldCount = 0;
    if (ldCount == 0 && pathIs(ldDir, "/Samples")) { fmt(ldDir, sizeof ldDir, "/"); ldCount = hal.browse(ldDir, ldEntry, kLdMax); if (ldCount < 0) ldCount = 0; }
    for (int i = 0; i < kLdMax; i++) { ldHasMeta[i] = false; ldMs[i] = 0; ldChannels[i] = 0; }
    ldSel = ldTop = 0;
    ldWaveOk = false;
    if (ldCount > 0) loadSelect(0);
}

void App::loadPath(int i, char* out, int size) const
{
    if (pathIs(ldDir, "/")) fmt(out, size, "/%s", ldEntry[i].name);
    else fmt(out, size, "%s/%s", ldDir, ldEntry[i].name);
}

void App::loadMeta(int i)
{
    if (i < 0 || i >= ldCount || ldEntry[i].dir || ldHasMeta[i]) return;
    ldHasMeta[i] = true;
    char path[140];
    loadPath(i, path, sizeof path);
    uint8_t head[1024];
    const uint32_t n = hal.rawRead(path, 0, head, sizeof head);
    WavInfo w;
    if (n == 0 || !parseWav(head, n, w) || w.rate == 0) return;
    ldChannels[i] = (uint8_t)w.channels;
    ldMs[i] = (uint32_t)((uint64_t)w.frames() * 1000 / w.rate);
}

void App::loadStopPreview()
{
    if (ldPreviewing) { hal.previewFile("", false); ldPreviewing = false; }
}

void App::loadSelect(int i)
{
    if (ldCount <= 0) return;
    i = clampi(i, 0, ldCount - 1);
    loadStopPreview();
    ldSel = i;
    if (ldSel < ldTop) ldTop = ldSel;
    if (ldSel >= ldTop + kLdRows) ldTop = ldSel - kLdRows + 1;
    for (int r = 0; r < kLdRows; r++) loadMeta(ldTop + r);
    // the picture of the highlighted file, from short reads spread along it
    ldWaveOk = false;
    if (ldEntry[ldSel].dir) return;
    char path[140];
    loadPath(ldSel, path, sizeof path);
    uint8_t head[1024];
    const uint32_t n = hal.rawRead(path, 0, head, sizeof head);
    WavInfo w;
    if (n == 0 || !parseWav(head, n, w) || w.frames() == 0) return;
    const uint32_t fb = w.frameBytes(), frames = w.frames();
    uint8_t buf[16 * 8 * 3];
    for (int c = 0; c < 58; c++)
    {
        const uint32_t from = (uint32_t)((uint64_t)c * frames / 58), to = (uint32_t)((uint64_t)(c + 1) * frames / 58);
        uint32_t take = to > from ? to - from : 1;
        if (take > 16) take = 16;
        const uint32_t got = hal.rawRead(path, w.dataOffset + from * fb, buf, take * fb) / fb;
        int peak = 0;
        for (uint32_t f = 0; f < got; f++)
            for (int ch = 0; ch < w.channels; ch++)
            {
                int v = wavSample16(buf + f * fb, ch, w.bits);
                if (v < 0) v = -v;
                if (v > peak) peak = v;
            }
        ldWave[c] = (uint8_t)(__builtin_sqrtf((float)peak / 32768.0f) * 255.0f);
    }
    ldWaveOk = true;
}

void App::loadDo(bool advance)
{
    if (ldCount <= 0 || ldEntry[ldSel].dir) return;
    loadMeta(ldSel);
    if (ldChannels[ldSel] == 0) { showPopup(Popup::Message, 6); return; }
    char old[4];
    const bool used = hal.sampleName(ldSlot, old, sizeof old);
    if (used && !ldConfirmed) { ldAdvance = advance ? 1 : 0; showPopup(Popup::ReplaceSlot); return; }
    ldConfirmed = false;
    uint32_t usedBytes = 0, totalBytes = 0;
    hal.sampleMemory(usedBytes, totalBytes);
    PadSample was;
    const uint32_t freed = hal.padSample(ldSlot, was) ? was.bytes : 0;
    const uint32_t need = ldMs[ldSel] * 88u;
    if ((int64_t)usedBytes - (int64_t)freed + (int64_t)need > (int64_t)totalBytes) { showPopup(Popup::Message, 5); return; }
    loadStopPreview();
    char path[140];
    loadPath(ldSel, path, sizeof path);
    if (!hal.loadSampleFile(path, ldSlot)) { showPopup(Popup::Message, 5); return; }
    hal.setCurrentSample(ldSlot);
    padWaveFor = -1;
    unsaved = true;
    if (advance)
    {
        int n = ldSel + 1;
        while (n < ldCount && ldEntry[n].dir) n++;
        if (n < ldCount) loadSelect(n);
        if (ldSlot < 47) ldSlot++;
    }
    refresh(true);
}

void App::loadKey(int k)
{
    switch (k)
    {
        case kUp: loadSelect(ldSel - 1); break;
        case kDown: loadSelect(ldSel + 1); break;
        case kLeft: ldSlot = clampi(ldSlot - (shift ? 12 : 1), 0, 47); break;
        case kRight: ldSlot = clampi(ldSlot + (shift ? 12 : 1), 0, 47); break;
        case kB1: ldConfirmed = false; loadDo(false); break;
        case kB4: ldConfirmed = false; loadDo(true); break;
        case kB2:
        case kJogPress:
            if (ldCount > 0 && ldEntry[ldSel].dir && k == kJogPress)
            {
                loadStopPreview();
                if (pathIs(ldDir, "/")) fmt(ldDir, sizeof ldDir, "/%s", ldEntry[ldSel].name);
                else { const size_t l = textLen(ldDir); fmt(ldDir + l, sizeof ldDir - l, "/%s", ldEntry[ldSel].name); }
                loadList();
            }
            else if (ldPreviewing) loadStopPreview();
            else if (ldCount > 0 && !ldEntry[ldSel].dir && ldWaveOk)
            {
                char path[140];
                loadPath(ldSel, path, sizeof path);
                hal.previewFile(path, true);
                ldPreviewing = true;
                ldPreviewAt = hal.millis();
            }
            break;
        case kB3:
        {
            loadStopPreview();
            char* cut = lastSlash(ldDir);
            if (cut != nullptr && cut != ldDir) { *cut = 0; loadList(); }
            else if (!pathIs(ldDir, "/")) { fmt(ldDir, sizeof ldDir, "/"); loadList(); }
            break;
        }
        case kB8:
        case kSampleLoad: back(); return;
        default: break;
    }
}

void App::loadJog(int delta)
{
    loadSelect(ldSel + (delta > 0 ? kLdRows : -kLdRows));
}

//--------------------------------------------------------------------------------------------
// C4 Resample (Sample Rec button): the mix, after the lo-fi master, rendered into a pad slot

uint32_t App::rsFreeBytes() const
{
    uint32_t used = 0, total = 0;
    hal.sampleMemory(used, total);
    PadSample was;
    const uint32_t freed = hal.padSample(rsSlot, was) ? was.bytes : 0;
    const int64_t f = (int64_t)total - (int64_t)used + (int64_t)freed;
    return f > 0 ? (uint32_t)f : 0;
}

void App::resamplePickName()
{
    if (!rsNameAuto) return;
    for (int n = 1; n < 100; n++)
    {
        char path[64];
        fmt(rsName, sizeof rsName, "Resample %02d", n);
        fmt(path, sizeof path, "/Samples/Resamples/%s.wav", rsName);
        if (hal.fileSize(path) == 0) return;
    }
}

void App::resampleOpen()
{
    if (job != Job::Resample) rsState = Rs::Ready;
    rsJogSlot = false;
    rsLevelL = rsLevelR = -60;
    rsSlot = 0;
    for (int i = 0; i < 48; i++)
    {
        char n[4];
        if (!hal.sampleName(i, n, sizeof n)) { rsSlot = i; break; }
    }
    resamplePickName();
}

void App::resampleStart()
{
    if (session.loopFrames() == 0) { showPopup(Popup::Message, 7); return; }
    if (!hal.sdPresent()) { showPopup(Popup::SdMissing); return; }
    const uint32_t frames = session.loopFrames() * (uint32_t)rsLoops;
    if (frames * 2 > rsFreeBytes()) { rsState = Rs::Full; return; }
    if (rsState == Rs::Saved) resamplePickName();
    fmt(rsPath, sizeof rsPath, "/Samples/Resamples/%s.wav", rsName);
    exportBank = hal.bank();
    exportLofi.cfg = hal.lofi().cfg;
    exportLofi.reset();
    exportBank.resetState();
    for (int t = 0; t < 4; t++) { exportBank.data[t] = session.hasPart(t) ? partData[t] : nullptr; for (int s = 0; s < 4; s++) exportBank.cfg[t].held[s] = false; }
    exportBank.loop = session.loopFrames();
    jobTotal = frames;
    uint8_t wh[44];
    wavHeader(wh, frames * 2, hal.sampleRate());
    if (!hal.fileWrite(rsPath, wh, 44, false)) { showPopup(Popup::SdMissing); return; }
    job = Job::Resample;
    jobDone = 0;
    exportPercent = 0;
    rsState = Rs::Recording;
    rsLevelL = rsLevelR = -60;
}

void App::resampleFinish()
{
    job = Job::None;
    rsLevelL = rsLevelR = -60;
    if (!hal.loadSampleFile(rsPath, rsSlot)) { rsState = Rs::Ready; showPopup(Popup::Message, 5); return; }
    hal.setCurrentSample(rsSlot);
    padWaveFor = -1;
    rsBytesSaved = jobTotal * 2;
    rsState = Rs::Saved;
    unsaved = true;
}

void App::resampleKey(int k)
{
    if (rsState == Rs::Recording)
    {
        if (k == kB1) { job = Job::None; rsState = Rs::Ready; }
        return;
    }
    switch (k)
    {
        case kB1: resampleStart(); break;
        case kB2: rsJogSlot = !rsJogSlot; break;
        case kB3: nameForResample = true; nameForSaveAs = false; fmt(nameEdit, sizeof(nameEdit), "%s", rsName); nameCursor = 0; nameChar = 0; open(Screen::NameEntry); return;
        case kLeft: resampleJog(-1); break;
        case kRight: resampleJog(1); break;
        case kB8:
        case kSampleRec: back(); return;
        default: break;
    }
}

void App::resampleJog(int delta)
{
    if (rsState == Rs::Recording) return;
    const int d = delta > 0 ? 1 : -1;
    if (rsJogSlot) rsSlot = clampi(rsSlot + d, 0, 47);
    else rsLoops = clampi(rsLoops + d, 1, 4);
    if (rsState == Rs::Saved) rsState = Rs::Ready;
}

//--------------------------------------------------------------------------------------------
// C5 Device settings (Config button)

int App::deviceRowCount(int g) const
{
    static const int n[9] = { 3, 1, 1, 1, 3, 6, 7, 2, 2 };
    return n[g < 0 || g > 8 ? 0 : g];
}

void App::deviceOpen()
{
    dvGroup = 0;
    dvRow = 0;
    dvRows = true;
    fwCount = hal.firmwareFiles(fwNames, 4);
    fwSel = 0;
}

void App::deviceRow(int g, int r, char* name, char* value, char* help, int size)
{
    static const char* const via[4] = { "Off", "USB", "TRS", "USB and TRS" };
    static const char* const usb[4] = { "Off", "Input", "Output", "Input and output" };
    static const char* const line[3] = { "Stereo", "Mono left", "Mono right" };
    static const char* const bright[3] = { "Low", "Medium", "High" };
    static const char* const wins[5] = { "Off", "30 ms", "60 ms", "100 ms", "150 ms" };
    static const char* const rolls[5] = { "0 ms", "60 ms", "120 ms", "250 ms", "500 ms" };
    static const char* const rounds[3] = { "Off", "0.1 BPM", "1 BPM" };
    const char* n = "";
    const char* h = "";
    value[0] = 0;
    switch (g)
    {
        case 0:
            if (r == 0) { n = "MIDI in"; fmt(value, size, "%s", via[hal.deviceValue(Dev::MidiIn) & 3]); h = "Where MIDI notes come from. Off ignores them."; }
            else if (r == 1)
            {
                n = "In channel";
                const int c = hal.deviceValue(Dev::MidiInChannel);
                if (c == 0) fmt(value, size, "All"); else fmt(value, size, "%d", c);
                h = "Notes on this channel play the pads. All listens on every channel.";
            }
            else { n = "MIDI out"; fmt(value, size, "%s", via[hal.deviceValue(Dev::MidiOut) & 3]); h = "Where the pads send their notes."; }
            break;
        case 1: n = "USB audio"; fmt(value, size, "%s", usb[hal.deviceValue(Dev::UsbAudio) & 3]); h = "Audio over USB: the Tracker's output, a computer's audio in, or both."; break;
        case 2: n = "Line input"; fmt(value, size, "%s", line[hal.deviceValue(Dev::LineInput) % 3]); h = "Stereo uses both line inputs. Mono left or right records one side to both."; break;
        case 3: n = "Volume"; fmt(value, size, "%d", hal.deviceValue(Dev::Volume)); h = "Headphone and output level. Shift + jog moves in larger steps."; break;
        case 4:
            if (r == 0) { n = "Brightness"; fmt(value, size, "%s", bright[hal.deviceValue(Dev::Brightness) % 3]); h = "How bright the screen is."; }
            else if (r == 2)
            {
                n = "Waveforms"; fmt(value, size, "%s", set.waveBitmap ? "Bitmaps" : "Columns");
                h = "Bitmaps draw each waveform once in the screen chip and are lighter and finer. Columns are the fallback.";
            }
            else
            {
                static const char* const grids[4] = { "Off", "Bars", "Bars and beats", "With sixteenths" };
                n = "Grid"; fmt(value, size, "%s", grids[set.homeGrid & 3]);
                h = "Lines over the lanes on the home screen. Sixteenths show only on short loops.";
            }
            break;
        case 5:
            if (r == 0) { n = "Alignment window"; fmt(value, size, "%s", wins[set.windowIdx % 5]); h = "A new Part whose first sound lands this close to the loop start is moved onto it."; }
            else if (r == 1) { n = "Pre-roll"; fmt(value, size, "%s", rolls[set.preRollIdx % 5]); h = "How long before the loop start a Part begins recording, so nothing is missed."; }
            else if (r == 2) { n = "Tempo rounding"; fmt(value, size, "%s", rounds[set.roundIdx % 3]); h = "The tempo worked out from the first take is rounded to this step."; }
            else if (r == 3) { n = "Wait level"; fmt(value, size, "%d dB", (int)set.waitDb); h = "How loud the input must get before a waiting first take starts."; }
            else if (r == 5)
            {
                n = "Tail wrap"; fmt(value, size, "%s", set.tailWrap ? "On" : "Off");
                h = "A note ringing past the end of a first take is folded onto the start of the loop, so the loop does not click.";
            }
            else
            {
                n = "Input latency";
                const uint32_t rate = hal.sampleRate() ? hal.sampleRate() : 44100;
                const int t10 = (int)((int64_t)latencyFrames * 10000 / (int64_t)rate);
                fmt(value, size, "%d.%d ms", t10 / 10, t10 % 10);
                h = "How late line and mic recordings arrive. B1 measures it with a cable from output to line input. Jog sets it by hand.";
            }
            break;
        case 6:
        {
            const ChordCfg& cc = chords.cfg;
            switch (r)
            {
                case 0: n = "Humanise"; fmt(value, size, "%d%%", cc.humanise); h = "How loose chords play: velocity, timing and arpeggio note length. 0 is exact."; break;
                case 1: n = "Strum speed"; fmt(value, size, "%d ms", cc.strumMs); h = "Time between the notes of a strum. Shift + jog on the Chords screen changes it too."; break;
                case 2: n = "Arp rate"; fmt(value, size, "%s", Chords::arpRateName(cc.arpRate)); h = "Length of one arpeggio step, as a note value."; break;
                case 3: n = "Arp range"; fmt(value, size, "%d oct", cc.arpRange); h = "How many octaves an arpeggio covers."; break;
                case 4: n = "Arp gate"; fmt(value, size, "%d%%", cc.gate); h = "How long each arpeggio note lasts, as a share of its step."; break;
                case 5: n = "Velocity"; fmt(value, size, "%s", cc.padVelocity ? "From the pads" : "Fixed"); h = "Fixed plays every chord at one velocity. From the pads uses how hard you hit."; break;
                default: n = "Synth level"; fmt(value, size, "%s%d dB", synthDb10 < 0 ? "-" : "+", (synthDb10 < 0 ? -synthDb10 : synthDb10) / 10); h = "How loud the synth is. Left and Right on the home screen change it too."; break;
            }
            break;
        }
        case 7:
            if (r == 0) { n = "Installed"; hal.firmwareVersion(value, size); h = "The firmware on this Tracker."; }
            else
            {
                n = "Update file";
                if (fwCount > 0) { fmt(value, size, "%s", fwNames[fwSel]); fmt(help, size, "B1 installs %s from the card. The project is saved first.", fwNames[fwSel]); { fmt(name, size, "%s", n); return; } }
                fmt(value, size, "None found");
                h = "Put a .ptf update file in the Firmware folder on the card.";
            }
            break;
        default:
            if (r == 0) { n = "Version"; hal.firmwareVersion(value, size); h = "The firmware version of this Tracker."; }
            else { n = "Build"; fmt(value, size, "%s", FT_BUILD_ID); h = "The build of the Four-Track code. The emulator shows the same id when it runs the same code."; }
            break;
    }
    fmt(help, size, "%s", h);
    fmt(name, size, "%s", n);
}

void App::deviceChange(int g, int r, int delta)
{
    const int d = delta > 0 ? 1 : -1;
    auto step = [&](Dev dev, int lo, int hi, int by) { hal.setDeviceValue(dev, clampi(hal.deviceValue(dev) + d * by, lo, hi)); };
    switch (g)
    {
        case 0: if (r == 0) step(Dev::MidiIn, 0, 3, 1); else if (r == 1) step(Dev::MidiInChannel, 0, 16, 1); else step(Dev::MidiOut, 0, 3, 1); break;
        case 1: step(Dev::UsbAudio, 0, 3, 1); break;
        case 2: step(Dev::LineInput, 0, 2, 1); break;
        case 3: step(Dev::Volume, 0, 100, shift ? 5 : 1); break;
        case 4:
            if (r == 0) step(Dev::Brightness, 0, 2, 1);
            else if (r == 2) { set.waveBitmap = !set.waveBitmap; for (int t = 0; t < 4; t++) waveDirty[t] = true; padWaveFor = -1; editWaveDirty = true; refresh(true); }
            else set.homeGrid = (uint8_t)clampi(set.homeGrid + d, 0, 3); break;
        case 5:
            if (r == 0) set.windowIdx = (uint8_t)clampi(set.windowIdx + d, 0, 4);
            else if (r == 1) set.preRollIdx = (uint8_t)clampi(set.preRollIdx + d, 0, 4);
            else if (r == 2) set.roundIdx = (uint8_t)clampi(set.roundIdx + d, 0, 2);
            else if (r == 3) set.waitDb = (int8_t)clampi(set.waitDb + d, -60, -12);
            else if (r == 5) set.tailWrap = !set.tailWrap;
            else { latencyFrames = clampi(latencyFrames + d * (shift ? 1 : 4), 0, 4000); latencyMark(); }
            break;
        case 6:
        {
            ChordCfg& cc = chords.cfg;
            if (r == 0) cc.humanise = (uint8_t)clampi(cc.humanise + d * 5, 0, 100);
            else if (r == 1) cc.strumMs = (uint8_t)clampi(cc.strumMs + d * 2, 10, 80);
            else if (r == 2) cc.arpRate = (uint8_t)clampi(cc.arpRate + d, 0, kArpRates - 1);
            else if (r == 3) cc.arpRange = (uint8_t)clampi(cc.arpRange + d, 1, 3);
            else if (r == 4) cc.gate = (uint8_t)clampi(cc.gate + d * 5, 10, 100);
            else if (r == 5) cc.padVelocity = (uint8_t)(1 - cc.padVelocity);
            else synthLevelStep(d);
            soundMark();
            break;
        }
        case 7: if (r == 1 && fwCount > 0) fwSel = clampi(fwSel + d, 0, fwCount - 1); break;
        default: break;
    }
}

void App::firmwareGo()
{
    fwUpdating = true;
    fwInstalled = false;
    fwStartMs = hal.millis();
    current = Screen::Device;
    redraw[4] = true;
    refresh(true);
}

void App::deviceKey(int k)
{
    if (fwUpdating) return; // nothing answers until the updater is done
    switch (k)
    {
        case kUp: case kDown:
        {
            const int d = k == kDown ? 1 : -1;
            if (dvRows) dvRow = clampi(dvRow + d, 0, deviceRowCount(dvGroup) - 1);
            else { dvGroup = clampi(dvGroup + d, 0, 8); dvRow = 0; }
            break;
        }
        case kLeft: dvRows = false; break;
        case kRight: dvRows = true; break;
        case kB1:
            if (dvGroup == 5 && dvRow == 4) { calibrateOpen(); return; }
            if (dvGroup == 7 && fwCount > 0)
            {
                if (unsaved) { pendingAction = 3; startSave(); }
                else firmwareGo();
            }
            break;
        case kB8:
        case kConfig: back(); return;
        default: break;
    }
}

void App::deviceJog(int delta)
{
    if (fwUpdating) return;
    if (!dvRows) { dvGroup = clampi(dvGroup + (delta > 0 ? 1 : -1), 0, 8); dvRow = 0; return; }
    deviceChange(dvGroup, dvRow, delta);
}

//--------------------------------------------------------------------------------------------
// C8 Copy a Part and C9 Clear, with one undo step (Shift + Copy)

// Keep a track as it is before a copy or a clear, so Shift + Copy can bring it back. A clear leaves the
// audio where it is (the memory behind the track is not touched until something is recorded or copied
// there), so it needs no write to the card.
void App::snapshotForUndo(int t)
{
    lastOp.cfg[t] = hal.bank().cfg[t];
    lastOp.align[t] = alignShift[t];
    lastOp.user[t] = userOffset[t];
    if (session.hasPart(t)) lastOp.prevHad |= (uint8_t)(1u << t);
}

void App::copyOpen()
{
    const int src = session.selected();
    if (session.mode() != FourTrackSession::Mode::Idle || session.loopFrames() == 0 || !session.hasPart(src)) { showPopup(Popup::Message, 8); return; }
    // the next empty track first, else the next one along
    copyTarget = -1;
    for (int k = 1; k < 4 && copyTarget < 0; k++) { const int t = (src + k) % 4; if (!session.hasPart(t)) copyTarget = t; }
    if (copyTarget < 0) copyTarget = (src + 1) % 4;
    showPopup(Popup::Copy);
}

void App::clearOpen(bool all)
{
    const int t = session.selected();
    bool any = false;
    for (int i = 0; i < 4; i++) if (session.hasPart(i)) any = true;
    if (session.mode() != FourTrackSession::Mode::Idle || (all ? !any : !session.hasPart(t))) { showPopup(Popup::Message, 9); return; }
    clearAll = all;
    showPopup(Popup::Clear);
}

void App::doCopy()
{
    const int src = session.selected(), dst = copyTarget;
    const uint32_t frames = session.loopFrames();
    if (dst == src || frames == 0 || !session.hasPart(src)) return;
    lastOp = LastOp();
    lastOp.kind = 1;
    lastOp.mask = (uint8_t)(1u << dst);
    snapshotForUndo(dst);
    if (session.hasPart(dst))
    {
        // the Part being replaced is kept on the card, a piece at a time with a progress panel
        if (!hal.sdPresent()) { showPopup(Popup::SdMissing); lastOp = LastOp(); return; }
        lastOp.fromFile = (uint8_t)(1u << dst);
        bkDst = dst;
        bkDone = 0;
        job = Job::Backup;
        showPopup(Popup::Busy, 0);
        return;
    }
    finishCopy();
}

void App::finishCopy()
{
    const int src = session.selected(), dst = bkDst = copyTarget;
    const uint32_t frames = session.loopFrames();
    int16_t* to = hal.region(dst);
    const int16_t* from = partData[src];
    for (uint32_t i = 0; i < frames; i++) to[i] = from[i];
    partData[dst] = to;
    alignShift[dst] = alignShift[src];
    userOffset[dst] = userOffset[src];
    // the copy keeps trims, fades and Offset; level, pan, tone and effect slots stay with the target
    TrackCfg& c = hal.bank().cfg[dst];
    const TrackCfg& o = hal.bank().cfg[src];
    c.trimStart = o.trimStart; c.trimEnd = o.trimEnd; c.fadeIn = o.fadeIn; c.fadeOut = o.fadeOut; c.gain = o.gain;
    uint8_t mask = 0;
    for (int i = 0; i < 4; i++) if (session.hasPart(i)) mask |= (uint8_t)(1u << i);
    const int keep = session.selected();
    session.restore(frames, session.bars(), (uint8_t)(mask | (1u << dst)));
    session.selectTrack(keep);
    noteAt[dst] = -2;
    waveDirty[dst] = true;
    unsaved = true;
    pushBank();
    syncPlayer();
    fmt(noteHint, sizeof noteHint, "Track %d copied to Track %d. Shift + Copy undoes it.", src + 1, dst + 1);
    refresh(true);
}

void App::doClear()
{
    const uint32_t frames = session.loopFrames();
    const int sel = session.selected();
    lastOp = LastOp();
    lastOp.kind = clearAll ? 3 : 2;
    uint8_t mask = 0;
    for (int i = 0; i < 4; i++) if (session.hasPart(i) && (clearAll || i == sel)) mask |= (uint8_t)(1u << i);
    if (mask == 0) return;
    lastOp.mask = mask;
    for (int i = 0; i < 4; i++)
        if ((mask >> i) & 1)
        {
            snapshotForUndo(i);
            session.clearPart(i);
            partData[i] = nullptr;
            alignShift[i] = 0;
            userOffset[i] = 0;
            TrackCfg& c = hal.bank().cfg[i];
            c.trimStart = 0; c.trimEnd = 0xFFFFFFFFu; c.fadeIn = 0; c.fadeOut = 0; c.gain = 1.0f;
            noteAt[i] = -2;
            waveDirty[i] = true;
        }
    (void)frames;
    unsaved = true;
    pushBank();
    syncPlayer();
    if (clearAll) fmt(noteHint, sizeof noteHint, "All tracks cleared. Shift + Copy brings them back.");
    else fmt(noteHint, sizeof noteHint, "Track %d cleared. Shift + Copy brings it back.", sel + 1);
    refresh(true);
}

void App::undoOp()
{
    if (lastOp.kind == 0 || session.mode() != FourTrackSession::Mode::Idle) { showPopup(Popup::Message, 8); return; }
    const uint32_t frames = session.loopFrames();
    uint8_t mask = 0;
    for (int i = 0; i < 4; i++) if (session.hasPart(i)) mask |= (uint8_t)(1u << i);
    for (int t = 0; t < 4; t++)
    {
        if (!((lastOp.mask >> t) & 1)) continue;
        if ((lastOp.prevHad >> t) & 1)
        {
            if ((lastOp.fromFile >> t) & 1)
            {
                char path[24];
                fmt(path, sizeof(path), "undo/t%d.raw", t);
                int32_t trailer[2] = { 0, 0 };
                if (hal.fileRead(path, 0, hal.region(t), frames * 2) != frames * 2) { showPopup(Popup::SdMissing); return; }
                hal.fileRead(path, frames * 2, trailer, 8);
            }
            partData[t] = hal.region(t);
            alignShift[t] = lastOp.align[t];
            userOffset[t] = lastOp.user[t];
            mask |= (uint8_t)(1u << t);
        }
        else
        {
            mask &= (uint8_t)~(1u << t);
            partData[t] = nullptr;
        }
        hal.bank().cfg[t] = lastOp.cfg[t];
        noteAt[t] = -2;
        waveDirty[t] = true;
    }
    const int keep = session.selected();
    session.restore(frames, session.bars(), mask);
    session.selectTrack(keep);
    lastOp = LastOp();
    unsaved = true;
    pushBank();
    syncPlayer();
    fmt(noteHint, sizeof noteHint, "Undone.");
    refresh(true);
}

// Clear all tracks, and also forget the Loop, tempo, glitch settings, mixer and lo-fi master: a new project
void App::resetAll()
{
    newProject(true);
    fmt(noteHint, sizeof noteHint, "Everything cleared. This is a new project.");
    refresh(true);
}
//--------------------------------------------------------------------------------------------
// C10 Chords (Note button): chords and scale notes on the pads

void App::chordsKey(int k)
{
    if (k == kLeft) chField = clampi(chField - 1, 0, 5);
    else if (k == kRight) chField = clampi(chField + 1, 0, 5);
    else if (k == kB1)
    {
        if (padsChords) chordsOff();
        else { hal.padsReleaseAll(); padsChords = true; }
    }
    else if (k == kB2) { setsOpen(); return; }
    else if (k == kB3)
    {
        chordsReleaseAll();
        latchedMods = 0;
        updateMods();
        const uint8_t keep = chords.cfg.key;
        chords.init();
        chords.cfg.key = keep;
        chords.rebuild();
        soundMark();
    }
    else if (k == kB4) { if (chSound >= kSynthPresets) chSound = sndPreset; open(Screen::Sound); return; }
    else if (k == kB7) { open(Screen::Play); return; }
    else if (k == kB8 || k == kNote) { back(); return; }
}

void App::chordsJog(int delta)
{
    ChordCfg& c = chords.cfg;
    const int d = delta > 0 ? 1 : -1;
    if (shift)
    {
        // while playing: Shift + jog sets the arpeggio rate, or the strum speed
        if (Chords::isArp(c.playStyle)) c.arpRate = (uint8_t)clampi(c.arpRate + d, 0, kArpRates - 1);
        else c.strumMs = (uint8_t)clampi(c.strumMs + d * 2, 10, 80);
        soundMark();
        return;
    }
    chordsReleaseAll();
    switch (chField)
    {
        case 0: c.key = (uint8_t)((c.key + 12 + d) % 12); break;
        case 1: c.scale = (uint8_t)((c.scale + kScaleCount + d) % kScaleCount); break;
        case 2: c.style = (uint8_t)((c.style + kStyleCount + d) % kStyleCount); chords.applyStyle(); break;
        case 3:
            chSound = clampi(chSound + d, 0, kSynthPresets); // the patches, then the pad sample
            if (chSound < kSynthPresets) soundPick(chSound);
            break;
        case 4: c.voicing = (Voicing)(((int)c.voicing + kVoicings + d) % kVoicings); break;
        default: c.octave = (int8_t)clampi(c.octave + d, -2, 2); break;
    }
    chords.rebuild();
    soundMark();
}

//--------------------------------------------------------------------------------------------
// C11 Sound (Instr button): pick a patch, tweak it with eight knobs, or open every raw setting

void App::soundApply()
{
    hal.synthSetPatch(synthApply(sndBase, sndMacros));
    hal.synthSetLevelDb10(synthDb10);
}

void App::synthLevelStep(int d)
{
    synthDb10 = clampi(synthDb10 + d * 10, kSynthLevelMinDb10, kSynthLevelMaxDb10);
    hal.synthSetLevelDb10(synthDb10);
    soundMark();
}

void App::soundMark()
{
    soundDirty = true;
    soundDirtyAt = hal.millis();
}

void App::soundPick(int preset)
{
    sndPreset = clampi(preset, 0, kSynthPresets - 1);
    sndBase = synthPreset(sndPreset);
    sndMacros = SynthMacros();
    chSound = sndPreset;
    soundApply();
    soundMark();
}

bool App::soundEdited() const
{
    const SynthPatch& o = synthPreset(sndPreset);
    for (int i = 0; i < kSynthParams; i++)
        if (synthParamGet(o, i) != synthParamGet(sndBase, i)) return true;
    return false;
}

void App::soundHear(bool on)
{
    if (on)
    {
        if (hearPad >= 0) return;
        hearPad = sndBase.mono ? 36 : 0; // a lead plays a note, the rest a chord
        chordPad(hearPad, true);
    }
    else if (hearPad >= 0)
    {
        chordPad(hearPad, false);
        hearPad = -1;
    }
}

void App::soundKey(int k)
{
    if (k == kLeft) sndField = clampi(sndField - 1, 0, 8);
    else if (k == kRight) sndField = clampi(sndField + 1, 0, 8);
    else if (k == kUp) { if (sndField == 0) soundPick(sndPreset - 1); else sndField = sndField <= 4 ? 0 : sndField - 4; }
    else if (k == kDown) { if (sndField == 0) { if (sndPreset >= kSynthPresets - 1) sndField = 1; else soundPick(sndPreset + 1); } else sndField = clampi(sndField + 4, 1, 8); }
    else if (k == kB1) soundHear(true);
    else if (k == kB2) { sndMacros = SynthMacros(); soundApply(); soundMark(); }
    else if (k == kB3) soundPick(sndPreset);
    else if (k == kB7) { soundHear(false); open(Screen::SoundAdv); return; }
    else if (k == kB8 || k == kInstr) { soundHear(false); back(); return; }
}

void App::soundJog(int delta)
{
    const int d = delta > 0 ? 1 : -1;
    if (sndField == 0) { soundPick(sndPreset + d); return; }
    int8_t& m = sndMacros.v[sndField - 1];
    m = (int8_t)clampi(m + d, -kMacroRange, kMacroRange);
    soundApply();
    soundMark();
}

void App::soundAdvKey(int k)
{
    if (k == kUp) advField = clampi(advField - 1, 0, kSynthParams - 1);
    else if (k == kDown) advField = clampi(advField + 1, 0, kSynthParams - 1);
    else if (k == kLeft) advField = clampi(advField - kSynthParams / 2, 0, kSynthParams - 1);
    else if (k == kRight) advField = clampi(advField + kSynthParams / 2, 0, kSynthParams - 1);
    else if (k == kB1) soundHear(true);
    else if (k == kB2) { synthParamSet(sndBase, advField, synthParamGet(synthPreset(sndPreset), advField)); soundApply(); soundMark(); }
    else if (k == kB3) { sndBase = synthPreset(sndPreset); soundApply(); soundMark(); }
    else if (k == kB8 || k == kInstr) { soundHear(false); open(Screen::Sound); return; }
}

void App::soundAdvJog(int delta)
{
    synthParamStep(sndBase, advField, delta > 0 ? 1 : -1, shift);
    soundApply();
    soundMark();
}

// The chord set: the chord pad settings, the patch with its edits and the knobs, the latched modifiers.
// One small blob, kept in sound.cfg (what the pads are now) and in each of the 8 chord set slots.
constexpr int kSetBlob = 138;

int App::packSet(uint8_t* b) const
{
    int n = 0;
    const ChordCfg& c = chords.cfg;
    b[n++] = c.key; b[n++] = c.scale; b[n++] = c.style; b[n++] = (uint8_t)c.voicing; b[n++] = (uint8_t)c.octave;
    b[n++] = c.playStyle; b[n++] = c.humanise; b[n++] = c.strumMs; b[n++] = c.arpRate; b[n++] = c.arpRange; b[n++] = c.gate;
    b[n++] = c.padVelocity; b[n++] = c.nineths ? 1 : 0; b[n++] = latchedMods;
    b[n++] = (uint8_t)chSound; b[n++] = (uint8_t)sndPreset;
    b[n++] = (uint8_t)(synthDb10 & 255); b[n++] = (uint8_t)((synthDb10 >> 8) & 255);
    for (int i = 0; i < kSynthMacros; i++) b[n++] = (uint8_t)sndMacros.v[i];
    for (int i = 0; i < kSynthParams; i++)
    {
        union { float f; uint32_t u; } v;
        v.f = synthParamGet(sndBase, i);
        for (int k = 0; k < 4; k++) b[n++] = (uint8_t)(v.u >> (8 * k));
    }
    return n;
}

void App::unpackSet(const uint8_t* b)
{
    int n = 0;
    ChordCfg& c = chords.cfg;
    c.key = b[n++]; c.scale = b[n++]; c.style = b[n++]; c.voicing = (Voicing)(b[n] >= kVoicings ? 0 : b[n]); n++; c.octave = (int8_t)b[n++];
    c.playStyle = b[n++]; c.humanise = b[n++]; c.strumMs = b[n++]; c.arpRate = b[n++]; c.arpRange = b[n++]; c.gate = b[n++];
    c.padVelocity = b[n++]; c.nineths = b[n++] != 0; latchedMods = b[n++];
    chSound = clampi(b[n++], 0, kSynthPresets);
    sndPreset = clampi(b[n++], 0, kSynthPresets - 1);
    synthDb10 = clampi((int)(int16_t)(b[n] | (b[n + 1] << 8)), kSynthLevelMinDb10, kSynthLevelMaxDb10); n += 2;
    sndBase = synthPreset(sndPreset);
    for (int i = 0; i < kSynthMacros; i++) sndMacros.v[i] = (int8_t)clampi((int8_t)b[n++], -kMacroRange, kMacroRange);
    for (int i = 0; i < kSynthParams; i++)
    {
        union { float f; uint32_t u; } v;
        v.u = (uint32_t)b[n] | ((uint32_t)b[n + 1] << 8) | ((uint32_t)b[n + 2] << 16) | ((uint32_t)b[n + 3] << 24);
        n += 4;
        synthParamSet(sndBase, i, v.f);
    }
    chords.rebuild();
    updateMods();
}

void App::soundSave()
{
    if (!hal.sdPresent()) return;
    uint8_t b[4 + kSetBlob];
    b[0] = 'F'; b[1] = 'S'; b[2] = '0'; b[3] = '5';
    packSet(b + 4);
    hal.fileWrite("sound.cfg", b, 4 + kSetBlob, false);
}

void App::soundLoad()
{
    uint8_t b[4 + kSetBlob];
    if (hal.fileSize("sound.cfg") != 4 + kSetBlob || hal.fileRead("sound.cfg", 0, b, 4 + kSetBlob) != 4 + kSetBlob) return;
    if (b[0] != 'F' || b[1] != 'S' || b[2] != '0' || b[3] != '5') return;
    unpackSet(b + 4);
}

//--------------------------------------------------------------------------------------------
// Input latency: Config, Four-Track, Input latency, B1 Calibrate

bool App::calibrateAllowed() const
{
    if (session.mode() != FourTrackSession::Mode::Idle || session.loopFrames() > 0) return false;
    for (int t = 0; t < 4; t++) if (session.hasPart(t)) return false;
    return true;
}

void App::calibrateOpen()
{
    calState = CalState::Ready;
    calWhy = 0;
    open(Screen::Calibrate);
}

void App::calibrateStart()
{
    uint32_t cap = 0;
    int16_t* buf = hal.calibrationBuffer(cap);
    if (buf == nullptr || cap < kLatencyLoopFrames) { calState = CalState::Failed; calWhy = 5; refresh(true); return; }
    const int t = 3; // an empty track: its memory takes the recording
    makeLatencyClicks(buf, kLatencyLoopFrames, kLatencyPulses);
    calSaveCfg = hal.bank().cfg[t];
    calSaveBypass = hal.lofi().cfg.bypass;
    TrackCfg c; // plain: 0 dB, centre, flat, no effects
    c.levelDb10 = 0;
    hal.bank().cfg[t] = c;
    hal.bank().playOnce = false;
    hal.lofi().cfg.bypass = true;
    appliedSource = -1;
    applySource(Source::Line);
    hal.setMonitor(false);
    hal.calibrationLoopback(true);
    hal.playerSetTrack(t, buf);
    hal.playerStart(kLatencyLoopFrames);
    hal.recordStart(t, Source::Line);
    calStart = hal.millis();
    calPercent = 0;
    calDrawAt = calStart;
    calState = CalState::Running;
    refresh(true);
}

void App::calibrateRestore()
{
    hal.calibrationLoopback(false);
    hal.bank().cfg[3] = calSaveCfg;
    hal.lofi().cfg.bypass = calSaveBypass;
    hal.playerSetTrack(3, nullptr);
    pushBank();
    appliedSource = -1;
    applySource(trackSource[session.selected()]);
}

void App::calibrateAbort()
{
    if (hal.recordActive()) hal.recordStop();
    hal.playerStop();
    calibrateRestore();
    calState = CalState::Ready;
}

void App::serviceCalibrate()
{
    if (calState != CalState::Running) return;
    const uint32_t need = (hal.sampleRate() ? hal.sampleRate() : 44100) * 7 / 2; // 3.5 s
    const uint32_t now = hal.millis();
    const bool timeout = now - calStart > 9000;
    if (hal.recordActive() && hal.recordLength() < need && !timeout)
    {
        if (now - calDrawAt > 250)
        {
            calDrawAt = now;
            calPercent = (int)((uint64_t)hal.recordLength() * 100 / need);
            redraw[4] = true;
            refresh(true);
        }
        return;
    }
    const bool captured = hal.recordStartCaptured();
    const uint32_t startPos = hal.recordStartPosition() % kLatencyLoopFrames;
    uint32_t got = hal.recordLength();
    if (got > need) got = need;
    if (hal.recordActive()) hal.recordStop();
    hal.playerStop();
    LatencyResult r;
    if (!captured) { r.why = 6; }
    else r = measureLatency(hal.region(3), got, startPos, kLatencyLoopFrames, kLatencyPulses);
    calibrateRestore();
    calResult = r;
    calWhy = r.why;
    calState = r.ok ? CalState::Done : CalState::Failed;
    redraw[4] = true;
    refresh(true);
}

void App::calibrateKey(int k)
{
    switch (calState)
    {
        case CalState::Ready:
            if (k == kB8 && calibrateAllowed()) calibrateStart();
            else if (k == kB7 || k == kConfig) open(Screen::Device);
            break;
        case CalState::Running:
            if (k == kB7) { calibrateAbort(); refresh(true); }
            break;
        case CalState::Done:
            if (k == kB8)
            {
                latencyFrames = calResult.frames;
                latencyMark();
                open(Screen::Device);
            }
            else if (k == kB7) { calState = CalState::Ready; refresh(true); }
            break;
        case CalState::Failed:
            if (k == kB8 && calibrateAllowed()) calibrateStart();
            else if (k == kB7) { calState = CalState::Ready; open(Screen::Device); }
            break;
    }
}

void App::latencySave()
{
    if (!hal.sdPresent()) return;
    uint8_t b[8] = { 'F', 'L', '0', '1', (uint8_t)(latencyFrames & 255), (uint8_t)((latencyFrames >> 8) & 255), (uint8_t)((latencyFrames >> 16) & 255), (uint8_t)((latencyFrames >> 24) & 255) };
    hal.fileWrite("latency.cfg", b, 8, false);
}

void App::latencyLoad()
{
    uint8_t b[8];
    if (hal.fileSize("latency.cfg") != 8 || hal.fileRead("latency.cfg", 0, b, 8) != 8) return;
    if (b[0] != 'F' || b[1] != 'L' || b[2] != '0' || b[3] != '1') return;
    latencyFrames = clampi((int32_t)((uint32_t)b[4] | ((uint32_t)b[5] << 8) | ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24)), 0, 4000);
}

//--------------------------------------------------------------------------------------------
// 6.1d Chord sets (Chords, B2): 8 slots on the card. A set stores the key, scale, style, sound with its
// tweaks, voicing, octave, latched modifiers and play style; not recorded Parts.

void App::setsFileRead()
{
    for (int i = 0; i < 8; i++) { setUsed[i] = false; setNames[i][0] = 0; }
    const uint32_t rec = 1 + 14 + 138;
    const uint32_t want = 4 + 8 * rec;
    if (hal.fileSize("sets.cfg") != want) return;
    static uint8_t buf[4 + 8 * (1 + 14 + 138)];
    if (hal.fileRead("sets.cfg", 0, buf, want) != want) return;
    if (buf[0] != 'S' || buf[1] != 'E' || buf[2] != 'T' || buf[3] != '1') return;
    for (int i = 0; i < 8; i++)
    {
        const uint8_t* r = buf + 4 + i * rec;
        setUsed[i] = r[0] != 0;
        for (int k = 0; k < 13; k++) setNames[i][k] = (char)r[1 + k];
        setNames[i][13] = 0;
        for (int k = 0; k < 138; k++) setData[i][k] = r[15 + k];
    }
}

void App::setsFileWrite()
{
    if (!hal.sdPresent()) return;
    const uint32_t rec = 1 + 14 + 138;
    static uint8_t buf[4 + 8 * (1 + 14 + 138)];
    buf[0] = 'S'; buf[1] = 'E'; buf[2] = 'T'; buf[3] = '1';
    for (int i = 0; i < 8; i++)
    {
        uint8_t* r = buf + 4 + i * rec;
        r[0] = setUsed[i] ? 1 : 0;
        for (int k = 0; k < 14; k++) r[1 + k] = (uint8_t)(k < 13 ? setNames[i][k] : 0);
        for (int k = 0; k < 138; k++) r[15 + k] = setData[i][k];
    }
    hal.fileWrite("sets.cfg", buf, 4 + 8 * rec, false);
}

void App::setsOpen()
{
    setsFileRead();
    open(Screen::Sets);
}

void App::setsSummary(const uint8_t* b, char* out, int size) const
{
    const int preset = b[15] < kSynthPresets ? b[15] : 0;
    const bool sample = b[14] >= kSynthPresets;
    fmt(out, size, "%s %s, %s, %s", Chords::keyName(b[0]), Chords::scaleName(b[1]), Chords::styleName(b[2]), sample ? "Sample" : synthPreset(preset).name);
}

void App::setsKey(int k)
{
    if (k == kUp) setSel = clampi(setSel - 1, 0, 7);
    else if (k == kDown) setSel = clampi(setSel + 1, 0, 7);
    else if (k == kB1)
    {
        // save what the pads are now into the slot, named after it
        packSet(setData[setSel]);
        setUsed[setSel] = true;
        fmt(setNames[setSel], sizeof setNames[setSel], "%s %s", Chords::keyName(chords.cfg.key), Chords::styleName(chords.cfg.style));
        setLoaded = setSel;
        setsFileWrite();
    }
    else if (k == kB2 && setUsed[setSel])
    {
        chordsReleaseAll();
        unpackSet(setData[setSel]);
        soundApply();
        setLoaded = setSel;
        soundMark();
    }
    else if (k == kB3 && setUsed[setSel])
    {
        // a name from the set itself (key, scale, patch), until the name entry is wired in
        fmt(setNames[setSel], sizeof setNames[setSel], "%s %s", Chords::keyName(setData[setSel][0]), Chords::scaleName(setData[setSel][1]));
        setsFileWrite();
    }
    else if (k == kB6 && setUsed[setSel])
    {
        setUsed[setSel] = false;
        setNames[setSel][0] = 0;
        if (setLoaded == setSel) setLoaded = -1;
        setsFileWrite();
    }
    else if (k == kB8 || k == kNote) { open(Screen::Chords); return; }
}

void App::setsJog(int delta) { setSel = clampi(setSel + (delta > 0 ? 1 : -1), 0, 7); }
} // namespace ft
