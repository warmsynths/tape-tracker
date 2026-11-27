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
  , session(h.sampleRate() * kMaxTakeSeconds)
{
    for (int i = 0; i < 48; i++) padModeKeys[i] = true;
    chords.init();
    sndBase = synthPreset(0);
    for (int i = 0; i < 48; i++) lightSent[i] = 255;
    set.waveBitmap = hal.maskDefault();
    hal.bank().fx.init(hal.fxMemory());
    if (hal.startsAtSetTempo()) { newProjectDefaults(); set.roleFlow = true; }
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
    soundLoad();
    latencyLoad();
    loopCfgLoad();
    soundApply();
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
        if (current == Screen::Main && padsChords && job == Job::None && session.mode() != FourTrackSession::Mode::ChoosingBars) open(Screen::Play); // Pattern again: the Play view
        else if (current != Screen::Main) back();
        else refresh(true);
        return;
    }
    if (k == kPlay && shift && job == Job::None && !fwUpdating && current != Screen::NameEntry && current != Screen::Tempo)
    {
        // Shift + Play switches the click (metronome) on or off
        set.click = !set.click;
        unsaved = true;
        lastClickBeat = -1;
        refresh(true);
        return;
    }
    if (noteHint[0] != 0) { noteHint[0] = 0; refresh(true); } // a note in the hint line lasts until the next button
    if ((k == kCopy || k == kDelete) && !onHome() && job == Job::None && !fwUpdating && current != Screen::NameEntry && current != Screen::Tempo)
    {
        back();
        key(k);
        return;
    }
    if (!onHome() && job == Job::None && !fwUpdating && current != Screen::NameEntry && current != Screen::Tempo)
    {
        // A button that opens a page also works from another page: go home, then open it
        struct Open { int key; Screen page; };
        static const Open pages[] = { { kSampleEdit, Screen::Editor }, { kPerformance, Screen::Glitch }, { kMaster, Screen::Master }, { kFile, Screen::Project },
                                      { kParams, Screen::Mixer }, { kSamplePlay, Screen::PadSample }, { kSampleLoad, Screen::Load }, { kSampleRec, Screen::Resample }, { kConfig, Screen::Device }, { kNote, Screen::Chords }, { kInstr, Screen::Sound }, { kFx1, Screen::Fx }, { kFx2, Screen::Filter } };
        for (const Open& o : pages)
            if (o.key == k && o.page != current)
            {
                back();
                key(k);
                return;
            }
    }
    if (!onHome())
    {
        // Play and Stop work on the pages that sit beside the recorder
        const bool transport = current == Screen::Glitch || current == Screen::Master || current == Screen::Editor ||
                               current == Screen::Input || current == Screen::Pads || current == Screen::Memory || current == Screen::Mixer || current == Screen::PadSample || current == Screen::Load || current == Screen::Resample ||
                               current == Screen::Settings || current == Screen::Chords || current == Screen::Sound || current == Screen::SoundAdv || current == Screen::Sets || current == Screen::Fx || current == Screen::Filter;
        if (transport && k == kPlay) { togglePlay(); refresh(true); return; }
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
    if (k == kCopy) { if (shift) undoOp(); else copyOpen(); return; }
    if (k == kDelete) { clearOpen(shift); return; }
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
            case kStop: if (session.mode() == FourTrackSession::Mode::Idle) { undoTake(); return; } break;
            case kUp:
            case kDown:
            {
                // Shift + Up or Down: the level of the selected track, 1 dB a click
                if (session.mode() != FourTrackSession::Mode::Idle) return;
                const int t = session.selected();
                TrackCfg& c = hal.bank().cfg[t];
                { int v = c.levelDb10 + (k == kUp ? 10 : -10); c.levelDb10 = (int16_t)(v < -600 ? -600 : v > 60 ? 60 : v); }
                unsaved = true;
                const int a = c.levelDb10 < 0 ? -c.levelDb10 : c.levelDb10;
                if (c.levelDb10 <= -600) fmt(noteHint, sizeof noteHint, "Track %d level off. Shift + Up or Down changes it.", t + 1);
                else fmt(noteHint, sizeof noteHint, "Track %d level %s%d dB. Shift + Up or Down changes it.", t + 1, c.levelDb10 < 0 ? "-" : "+", a / 10);
                refresh(true);
                return;
            }
            case kLeft:
            case kRight:
            {
                // Shift + Left or Right: the level of the synth (the chord pads)
                synthLevelStep(k == kRight ? 1 : -1);
                fmt(noteHint, sizeof noteHint, "Synth level %s%d dB. Shift + Left or Right changes it.", synthDb10 < 0 ? "-" : "+", (synthDb10 < 0 ? -synthDb10 : synthDb10) / 10);
                refresh(true);
                return;
            }
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
        case kPlay: togglePlay(); break;
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
        case kLeft:
        case kRight:
        {
            // one beat back or forward
            stepBeat(k == kRight ? 1 : -1);
            break;
        }
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
        case kParams: mxTrack = session.selected(); open(Screen::Mixer); break;
        case kSamplePlay: psField = 0; padWaveFor = -1; open(Screen::PadSample); break;
        case kSampleLoad: open(Screen::Load); loadOpen(); break;
        case kSampleRec: resampleOpen(); open(Screen::Resample); break;
        case kConfig: deviceOpen(); open(Screen::Device); break;
        case kNote: if (!padsChords) { hal.padsReleaseAll(); padsChords = true; } open(Screen::Chords); break;
        case kInstr:
            if (!padsChords) { hal.padsReleaseAll(); padsChords = true; }
            if (chSound >= kSynthPresets) chSound = sndPreset; // this page is for the synth
            open(Screen::Sound);
            break;
        case kFile: refreshProjectList(); open(Screen::Project); break;
        case kFx1: open(Screen::Fx); break;
        case kFx2: open(Screen::Filter); break;
        default: break;
    }
}

void App::jog(int delta)
{
    if (delta == 0) return;
    if (popupKind == Popup::Copy) { popupKey(delta > 0 ? kRight : kLeft); return; }
    if (popupKind != Popup::None) return;
    if (!onHome())
    {
        jogScreen(delta);
        return;
    }
    if (waitingSound && session.mode() == FourTrackSession::Mode::RecordingFirst)
    {
        // the level the take starts at, set while you watch the input: above the buzz, below your note
        set.waitDb = (int8_t)(set.waitDb + (delta > 0 ? 1 : -1) * (shift ? 3 : 1));
        if (set.waitDb < -60) set.waitDb = -60;
        if (set.waitDb > -12) set.waitDb = -12;
        refresh(true);
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
    if (shift && (session.mode() == FourTrackSession::Mode::Armed || session.mode() == FourTrackSession::Mode::RecordingPart) && layerPass[session.recordingTrack() < 0 ? 0 : session.recordingTrack()])
    {
        set.fadePct = (uint8_t)(set.fadePct + (delta > 0 ? 5 : -5) < 40 ? 40 : set.fadePct + (delta > 0 ? 5 : -5) > 100 ? 100 : set.fadePct + (delta > 0 ? 5 : -5));
        latencyMark();
        fmt(noteHint, sizeof noteHint, "Fade %d%%: older layers stay at this level under each new pass.", (int)set.fadePct);
        refresh(true);
        return;
    }
    // the jog scrubs the position, stopped or playing; with Shift a bar at a time
    if (!actionMenu && session.mode() == FourTrackSession::Mode::Idle)
    {
        scrub(delta > 0 ? 1 : -1, shift);
        return;
    }
}

void App::pad(int p, bool down, int velocity)
{
    if (p < 0 || p >= 48) return;
    if (padsChords && current != Screen::PadSample && selectedRole() != TrackRole::Audio)
    {
        if (selectedRole() == TrackRole::Bass || selectedRole() == TrackRole::Melody || selectedRole() == TrackRole::Drums) rolePadPress(p, down, velocity);
        else chordPad(p, down, velocity);
        if (current == Screen::Chords || current == Screen::Play) refresh(true);
        return;
    }
    if (current == Screen::PadSample && down) { hal.setCurrentSample(p); refresh(true); } // a pad chooses its sample here
    if (down) hal.padOn(p, padModeKeys[hal.currentSample() >= 0 ? hal.currentSample() : 0] ? padsKeysMode : false);
    else hal.padOff(p);
}

void App::chordNote(int pad, int index, int note, int velocity, bool on)
{
    if (chSound < kSynthPresets) hal.synthNote(note, velocity, on);
    else hal.sampleNote(200 + pad * kMaxChordNotes + index, note, velocity, on);
}

int App::rnd(int n)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return n > 0 ? (int)(rng % (uint32_t)n) : 0;
}

// Humanise lowers a velocity by up to a third at most, a little differently every time
int App::humanVelocity(int base)
{
    const int h = chords.cfg.humanise;
    int v = base - rnd(base * h * 35 / 10000 + 1);
    if (v < 20) v = 20;
    return v > 127 ? 127 : v;
}

void App::startNote(int p, int index, int note, int velocity, uint32_t delayMs)
{
    if (delayMs == 0)
    {
        chordNote(p, index, note, velocity, true);
        soundMask[p] |= (uint8_t)(1u << index);
        return;
    }
    if (pendCount < 64) pend[pendCount++] = Pending{ hal.millis() + delayMs, (uint8_t)p, (uint8_t)index, (uint8_t)note, (uint8_t)velocity };
}

double App::currentBpm()
{
    if (session.loopFrames() > 0 && session.bars() > 0)
        return 60.0 * (double)hal.sampleRate() * (double)(session.bars() * 4) / (double)session.loopFrames();
    return (set.tempoTenths > 0 ? (double)set.tempoTenths : 1200.0) / 10.0;
}

void App::stopArp()
{
    if (arpPad >= 0 && arpNote >= 0) chordNote(arpPad, arpVoice, arpNote, 0, false);
    arpNote = -1;
    arpPad = -1;
}

void App::startArp(int p, const uint8_t* chord, int count, int velocity)
{
    stopArp();
    arpLen = Chords::arpSequence(chord, count, chords.cfg.playStyle, chords.cfg.arpRange, arpSeq, 24);
    if (arpLen == 0) return;
    arpPad = p;
    arpStep = 0;
    arpVel = velocity;
    arpPeriod = 60000.0 / currentBpm() * (double)Chords::arpRateBeats(chords.cfg.arpRate);
    // while a loop plays, the first step waits for the next step of the loop's grid
    double wait = 0;
    if (hal.playerPlaying() && session.loopFrames() > 0 && hal.sampleRate() > 0)
    {
        const double stepFrames = arpPeriod * (double)hal.sampleRate() / 1000.0;
        const double into = (double)(hal.playerPosition() % session.loopFrames());
        double rem = into - (double)((uint64_t)(into / stepFrames)) * stepFrames;
        if (rem > stepFrames * 0.125) wait = (stepFrames - rem) * 1000.0 / (double)hal.sampleRate(); // just past the step: play now
    }
    arpDue = (double)hal.millis() + wait;
    if (wait == 0) stepArp(hal.millis()); // the first note is not late
}

void App::stepArp(uint32_t now)
{
    if (arpNote >= 0) chordNote(arpPad, arpVoice, arpNote, 0, false);
    const int idx = chords.cfg.playStyle == StyleArpRandom ? rnd(arpLen) : arpStep % arpLen;
    arpStep++;
    arpVoice = (arpVoice + 1) % kMaxChordNotes;
    arpNote = arpSeq[idx];
    chordNote(arpPad, arpVoice, arpNote, humanVelocity(arpVel), true);
    const int h = chords.cfg.humanise;
    double gate = arpPeriod * (double)chords.cfg.gate / 100.0 * (1.0 + (double)(rnd(21) - 10) * (double)h / 1000.0);
    if (gate < 20.0) gate = 20.0;
    arpOffAt = now + (uint32_t)gate;
    arpDue += arpPeriod;
    if (arpDue < (double)now - arpPeriod) arpDue = (double)now + arpPeriod; // far behind: do not catch up in a burst
}

void App::serviceChordNotes()
{
    const uint32_t now = hal.millis();
    for (int i = 0; i < pendCount;)
    {
        if ((int32_t)(now - pend[i].at) >= 0)
        {
            const Pending e = pend[i];
            pend[i] = pend[--pendCount];
            if (heldCount[e.pad] != 0)
            {
                chordNote(e.pad, e.index, e.note, e.velocity, true);
                soundMask[e.pad] |= (uint8_t)(1u << e.index);
            }
        }
        else i++;
    }
    if (arpPad >= 0)
    {
        if (arpNote >= 0 && (int32_t)(now - arpOffAt) >= 0) { chordNote(arpPad, arpVoice, arpNote, 0, false); arpNote = -1; }
        if (arpDue <= (double)now) stepArp(now);
    }
}

void App::releasePad(int p)
{
    if (arpPad == p) stopArp();
    for (int i = 0; i < pendCount;)
    {
        if (pend[i].pad == p) pend[i] = pend[--pendCount];
        else i++;
    }
    for (int i = 0; i < heldCount[p]; i++)
        if ((soundMask[p] >> i) & 1) chordNote(p, i, heldNote[p][i], 0, false);
    soundMask[p] = 0;
    if (heldCount[p] != 0) hitAt[p] = hal.millis();
    heldCount[p] = 0;
    latched[p] = false;
}

void App::releaseLatched()
{
    for (int p = 0; p < 48; p++) if (latched[p]) releasePad(p);
}

void App::updateMods()
{
    uint8_t groupsHeld = 0;
    for (int b = 0; b < kMods; b++)
        if ((heldMods >> b) & 1) groupsHeld |= Chords::groupOf((uint8_t)(1u << b));
    chords.mods = (uint8_t)(heldMods | (latchedMods & ~groupsHeld));
}

void App::chordPad(int p, bool down, int velocity)
{
    const PadInfo pi = Chords::info(p);
    ChordCfg& c = chords.cfg;
    if (pi.role == Role::None) return;
    if (pi.role != Role::Chord && pi.role != Role::Note)
    {
        // the right hand: modifiers (hold, or double-tap to latch), play styles, octave and Hold
        const uint32_t now = hal.millis();
        if (down)
        {
            heldCount[p] = 1;
            hitAt[p] = now;
            if (pi.role == Role::Mod)
            {
                const uint8_t bit = (uint8_t)(1u << pi.index);
                const uint8_t group = Chords::groupOf(bit);
                if (latchedMods & bit) latchedMods &= (uint8_t)~bit;                    // tapping a latched one lets it go
                else if (modTapAt[pi.index] != 0 && now - modTapAt[pi.index] < 350)       // a double tap latches it
                {
                    latchedMods = (uint8_t)((latchedMods & ~group) | bit);
                    heldMods &= (uint8_t)~group;
                }
                else { heldMods = (uint8_t)((heldMods & ~group) | bit); latchedMods &= (uint8_t)~group; }
                modTapAt[pi.index] = now;
                updateMods();
            }
            else if (pi.role == Role::Style) { c.playStyle = pi.index; soundMark(); }
            else if (pi.role == Role::OctDown) { c.octave = (int8_t)(c.octave > -2 ? c.octave - 1 : c.octave); soundMark(); }
            else if (pi.role == Role::OctUp) { c.octave = (int8_t)(c.octave < 2 ? c.octave + 1 : c.octave); soundMark(); }
            else if (pi.role == Role::Hold)
            {
                chHold = !chHold;
                if (!chHold) releaseLatched();
            }
        }
        else
        {
            heldCount[p] = 0;
            if (pi.role == Role::Mod)
            {
                const uint8_t bit = (uint8_t)(1u << pi.index);
                heldMods &= (uint8_t)~bit;
                updateMods();
            }
        }
        return;
    }
    if (down)
    {
        releaseLatched(); // a new chord ends the ones Hold kept
        if (heldCount[p] != 0) return;
        uint8_t n[kMaxChordNotes];
        const int count = chords.notes(p, n);
        chords.played(p);
        if (count == 0) return;
        if (pi.role == Role::Chord)
        {
            chords.label(p, lastChordName, sizeof lastChordName);
            lastFamily = (int)chords.family(p);
            // during a take the chord is written to the chord track, for Bass and Melody to follow
            const FourTrackSession::Mode m = session.mode();
            if ((m == FourTrackSession::Mode::RecordingFirst || m == FourTrackSession::Mode::RecordingPart) && chordEvCount < kMaxChordEvents && count <= kMaxChordNotes)
            {
                ChordEv& e = chordEv[chordEvCount++];
                e.frame = m == FourTrackSession::Mode::RecordingFirst ? (hal.recordLength() > takeOffset ? hal.recordLength() - takeOffset : 0) : hal.playerPosition();
                e.count = (uint8_t)count;
                for (int i = 0; i < count; i++) e.notes[i] = n[i];
            }
        }
        const bool chord = pi.role == Role::Chord;
        const int base = c.padVelocity && velocity >= 1 ? (velocity > 127 ? 127 : velocity) : 100;
        for (int i = 0; i < count; i++) heldNote[p][i] = n[i];
        heldCount[p] = (uint8_t)count;
        const int style = chord ? c.playStyle : StyleBlock;
        if (Chords::isArp(style)) { startArp(p, n, count, base); return; }
        const int h = c.humanise;
        for (int i = 0; i < count; i++)
        {
            // a strum plays the notes in turn, low to high (dn) or high to low (up); a block gets a few
            // milliseconds of looseness when humanise is up
            const int order = style == StyleStrumUp ? count - 1 - i : i;
            uint32_t delay = 0;
            if (style == StyleStrumDown || style == StyleStrumUp) delay = (uint32_t)(order * c.strumMs);
            else delay = (uint32_t)(i * h * 15 / 100);
            if (h > 0 && delay > 0) delay += (uint32_t)rnd(h / 10 + 1);
            int v = humanVelocity(base);
            if (order == 0 && (style == StyleStrumDown || style == StyleStrumUp) && v < 119) v += 8; // the first note of a strum leans in
            startNote(p, i, n[i], v, delay);
        }
    }
    else
    {
        if (heldCount[p] == 0) return;
        if (chHold) { latched[p] = true; return; }
        releasePad(p);
    }
}

void App::chordsReleaseAll()
{
    chHold = false;
    stopArp();
    pendCount = 0;
    for (int p = 0; p < 48; p++)
    {
        const Role r = Chords::info(p).role;
        if (r == Role::Chord || r == Role::Note) releasePad(p);
        else heldCount[p] = 0;
    }
    heldMods = 0;
    updateMods();
    hal.synthAllOff();
}

void App::chordsOff()
{
    chordsReleaseAll();
    padsChords = false;
    serviceLights();
}

void App::togglePadMode()
{
    if (padsChords) chordsOff();
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
    // a line or mic take arrived late by the input latency: move it back (a pad take has none)
    if (active >= 0 && trackSource[active] != Source::Pads) alignShift[active] = -latencyFrames;
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

void App::scrub(int steps, bool bars)
{
    const uint32_t loop = session.loopFrames();
    if (loop == 0 || session.mode() != FourTrackSession::Mode::Idle) return;
    const uint32_t beat = session.bars() > 0 ? loop / ((uint32_t)session.bars() * 4) : loop / 16;
    const uint32_t unit = bars ? beat * 4 : (beat >= 4 ? beat / 4 : 1);
    const bool live = hal.playerPlaying();
    int64_t pos = live ? hal.playerPosition() : scrubPos;
    pos += (int64_t)steps * unit;
    pos = ((pos % (int64_t)loop) + (int64_t)loop) % (int64_t)loop;
    if (live) hal.playerSeek((uint32_t)pos);
    else scrubPos = (uint32_t)pos;
    lastClickBeat = -1;
    refresh(true);
}

void App::stepBeat(int dir)
{
    const uint32_t loop = session.loopFrames();
    if (loop == 0 || session.mode() != FourTrackSession::Mode::Idle) return;
    const uint32_t beat = session.bars() > 0 ? loop / ((uint32_t)session.bars() * 4) : loop / 16;
    if (beat == 0) return;
    const uint32_t cur = hal.playerPlaying() ? hal.playerPosition() : scrubPos;
    // land on a beat: the next one on, or the one before
    int64_t idx = dir > 0 ? (int64_t)(cur / beat) + 1 : (int64_t)((cur + beat - 1) / beat) - 1;
    const int64_t beats = (int64_t)(loop / beat);
    idx = ((idx % beats) + beats) % beats;
    const uint32_t pos = (uint32_t)(idx * (int64_t)beat);
    if (hal.playerPlaying()) hal.playerSeek(pos);
    else scrubPos = pos;
    lastClickBeat = -1;
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
        focusUser = !focusUser; // pressing the selected track again opens it in focus, and again closes it
    else
        session.selectTrack(track);
    if (idle) applySource(trackSource[session.selected()]);
    if (idle && set.roleFlow && roleChosen[session.selected()]) { roleAllOff(); roleSoundApply(trackRole[session.selected()]); if (trackRole[session.selected()] != TrackRole::Audio && !padsChords) { hal.padsReleaseAll(); padsChords = true; } }
    refresh(true);
}

//--------------------------------------------------------------------------------------------
// Pressing Rec: the replace question, the count-in, then recording

void App::pressRecord()
{
    finishTail(true);
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
    if (set.roleFlow && !session.hasPart(t) && !roleChosen[t])
    {
        // Rec on an empty track asks what to record (Shift + Rec skips the question and uses the last role)
        if (shift) applyRole(t, lastRole);
        else { roleList = 0; open(Screen::Role); return; }
    }
    const bool layerNow = set.layer && session.loopFrames() > 0 && session.hasPart(t) && hal.sdPresent() && partData[t] != nullptr && layerScratchTrack(t) >= 0;
    if (session.loopFrames() > 0 && session.hasPart(t) && !replaceConfirmed && !layerNow)
    {
        showPopup(Popup::Replace, t);
        return;
    }
    replaceConfirmed = false;
    // With the chord pads on, Rec records them: the track's source becomes Pads (change it back in Input)
    if (padsChords && trackSource[t] != Source::Pads && trackRole[t] != TrackRole::Audio)
    {
        trackSource[t] = Source::Pads;
        applySource(Source::Pads);
        unsaved = true;
        fmt(noteHint, sizeof noteHint, "Track %d records the chord pads (source: Pads).", t + 1);
    }
    // A first take at a set tempo starts with a bar of clicks
    const bool firstTake = session.loopFrames() == 0 || (session.partCount() == 1 && session.hasPart(t) && !layerNow);
    layerPass[t] = layerNow;
    if (firstTake && set.countBeats() > 0 && set.tempoTenths < 2400 && !set.waitSound) // waiting for the first note replaces the count-in
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
    finishTail(true);
    lastClickBeat = -1;
    const int t = session.selected();
    if ((lastOp.mask >> t) & 1) lastOp = LastOp(); // the memory behind this track is about to be written over
    // Keep the take that is about to be replaced, so it can be brought back
    if (session.hasPart(t) && session.loopFrames() > 0 && hal.sdPresent())
    {
        const uint32_t frames = session.loopFrames();
        const int32_t trailer[2] = { alignShift[t], userOffset[t] };
        char path[24];
        fmt(path, sizeof(path), "undo/t%d.raw", t);
        if (hal.fileWrite(path, partData[t], frames * 2, false)) { hal.fileWrite(path, trailer, 8, true); hasUndoTake[t] = true; }
        else layerPass[t] = false; // the old audio could not be kept, so it cannot be laid under the new pass
    }
    else layerPass[t] = false;
    if (padsChords && trackRole[t] == TrackRole::Chords && !layerPass[t]) chordEvCount = 0; // a new Chords take writes a new chord track
    layerScratch[t] = layerPass[t] ? layerScratchTrack(t) : -1;
    if (layerScratch[t] < 0) layerPass[t] = false;
    session.pressRecord(layerPass[t]);
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
    focusUser = false; // Stop returns to the overview
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
        scrubPos = 0;
    }
    syncPlayer();
    refresh(true);
}

// The Play button starts the loop, and stops it when it is already playing
void App::togglePlay()
{
    if (session.mode() == FourTrackSession::Mode::Idle && session.playing()) pressStop();
    else pressPlay();
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
    fxPush();
}

void App::startPlayer()
{
    pushBank();
    for (int t = 0; t < FourTrackSession::kTracks; t++) hal.playerSetTrack(t, session.hasPart(t) ? partData[t] : nullptr);
    hal.playerStart(session.loopFrames());
    if (scrubPos > 0 && scrubPos < session.loopFrames() && session.mode() == FourTrackSession::Mode::Idle) hal.playerSeek(scrubPos); // Play starts where the playhead was left
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
        const int heard = findFirstOnset(hal.region(rec) + takeOffset, 0, frames, hal.sampleRate());
        const bool onGrid = autoFrames() > 0; // the take starts on the bar line: nothing before the first note is cut
        const int onset = onGrid && heard >= 0 ? 0 : heard;
        if (onset < 0)
        {
            // Nothing was heard: throw it away
            finishTail(false);
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

//--------------------------------------------------------------------------------------------
// The first take with the bars set up front: it records exactly that many bars and stops itself on the bar line

void App::newProjectDefaults()
{
    set.tempoTenths = 800;
    set.bars = 4;
    set.countIdx = 3; // four beats: one bar of count-in
}

uint32_t App::autoFrames() const
{
    if (set.tempoTenths == 0 || set.bars == 0) return 0;
    const uint32_t rate = hal.sampleRate();
    uint32_t frames = (uint32_t)(((uint64_t)set.bars * 4u * 60u * 10u * rate + set.tempoTenths / 2) / set.tempoTenths);
    const uint32_t limit = hal.regionFrames() - kPostFrames - 2000;
    return frames > limit ? limit : frames;
}

int App::barsFit(int tempoTenths) const
{
    return tempoTenths / 150; // 16 s at 4 beats a bar: bars * 2400 / tempo <= 16
}

int App::barsStep(int from, int dir) const
{
    static const int options[9] = { 0, 1, 2, 3, 4, 6, 8, 12, 16 };
    const int fit = barsFit(set.tempoTenths > 0 ? set.tempoTenths : 1200);
    int idx = 0;
    for (int i = 0; i < 9; i++) if (options[i] <= from) idx = i;
    for (int i = idx + dir; i >= 0 && i < 9; i += dir)
        if (options[i] <= fit) return options[i];
    return from;
}

void App::finishTail(bool all)
{
    if (tailTrack < 0) return;
    const int t = tailTrack;
    tailTrack = -1;
    const uint32_t got = hal.recordLength();
    if (hal.recordActive()) hal.recordStop();
    if (!all || got <= tailAt || !session.hasPart(t) || partData[t] == nullptr) return;
    uint32_t n = got - tailAt;
    if (n > tailWant) n = tailWant;
    // what rang on past the end of the loop joins the start of the loop, fading out as the take did not
    int16_t* d = const_cast<int16_t*>(partData[t]);
    for (uint32_t i = 0; i < n; i++)
    {
        const int32_t v = d[i] + (int32_t)((int64_t)d[tailAt + i] * (int64_t)(n - i) / (int64_t)n);
        d[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    waveDirty[t] = true;
}

void App::serviceTail()
{
    if (tailTrack < 0) return;
    if (!hal.recordActive() || hal.recordLength() >= tailAt + tailWant) finishTail(true);
}

void App::serviceFirstTake()
{
    const uint32_t want = autoFrames();
    if (want > 0 && hal.recordActive() && hal.recordLength() >= takeOffset + want)
    {
        const int rec = session.recordingTrack();
        const uint32_t room = hal.regionFrames() > want + kPostFrames ? hal.regionFrames() - want - kPostFrames : 0;
        tailWant = set.tailWrap ? (hal.sampleRate() * 2 < room ? hal.sampleRate() * 2 : room) : 0;
        if (tailWant > 0) { tailTrack = rec; tailAt = want; }
        else hal.recordStop();
        session.firstRecordedFrames(want, false);
        if (session.mode() == FourTrackSession::Mode::RecordingFirst) session.pressStop();
        onFirstTakeFinished();
        if (session.mode() == FourTrackSession::Mode::ChoosingBars)
        {
            promptCount = 1;
            promptBars[0] = (uint8_t)set.bars;
            promptSelected = 0;
            promptConfirm();
        }
        return;
    }
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
        { const int rt = session.recordingTrack(); layerMixed = 0; hal.recordStart(layerPass[rt] && layerScratch[rt] >= 0 ? layerScratch[rt] : rt, trackSource[rt]); }
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
    // line and mic audio arrives late by the input latency; a pad Part has none
    const int32_t lat = trackSource[track] == Source::Pads ? 0 : latencyFrames;
    const int32_t rel = onset - (int32_t)firstSample - lat; // first note, relative to the loop start
    const uint32_t window = windowFrames();
    alignShift[track] = -lat;
    if (window > 0 && (uint32_t)(rel < 0 ? -rel : rel) <= window) alignShift[track] = -rel - lat;
}

void App::finishPart(int track, uint32_t firstSample, bool early)
{
    if (layerPass[track])
    {
        layerPass[track] = false;
        finishLayer(track, firstSample);
        return;
    }
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

// A pass laid over the Part that was there. The take is recorded in the memory of an empty track while the old
// Part keeps playing; as it comes in, each frame is added into the old Part (which is first lowered to the Fade
// level), so the loop grows layer by layer while it plays and nothing is lost or silent during the pass.
int App::layerScratchTrack(int track) const
{
    for (int s = 0; s < FourTrackSession::kTracks; s++)
        if (s != track && !session.hasPart(s)) return s;
    return -1;
}

void App::mixLayer(int track, uint32_t firstSample, uint32_t recorded)
{
    const uint32_t length = session.loopFrames();
    if (length == 0 || layerScratch[track] < 0 || partData[track] == nullptr) return;
    const int16_t* src = hal.region(layerScratch[track]);
    int16_t* dst = const_cast<int16_t*>(partData[track]);
    const int32_t lat = trackSource[track] == Source::Pads ? 0 : latencyFrames;
    const int64_t shift = (int64_t)alignShift[track] + userOffset[track];
    const int fade = set.fadePct < 40 ? 40 : (set.fadePct > 100 ? 100 : set.fadePct);
    uint32_t end = recorded < firstSample + length ? recorded : firstSample + length;
    uint32_t k = layerMixed > firstSample ? layerMixed : firstSample;
    for (; k < end; k++)
    {
        int64_t j = ((int64_t)(k - firstSample) - lat - shift) % (int64_t)length;
        if (j < 0) j += (int64_t)length;
        const int32_t v = (int32_t)dst[j] * fade / 100 + (int32_t)src[k];
        dst[j] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    layerMixed = k;
}

void App::finishLayer(int track, uint32_t firstSample)
{
    mixLayer(track, firstSample, hal.recordLength());
    session.partRecorded();
    noteAt[track] = -2;
    waveDirty[track] = true;
    unsaved = true;
    pushBank();
    hal.playerSetTrack(track, partData[track]);
    layerScratch[track] = -1;
    refresh(true);
}

void App::servicePartRecording()
{
    if (!hal.recordStartCaptured()) return;
    const uint32_t length = session.loopFrames();
    const uint32_t firstSample = (length - (hal.recordStartPosition() % length)) % length;
    const uint32_t needed = firstSample + length + kPostFrames;
    if (layerPass[session.recordingTrack()]) mixLayer(session.recordingTrack(), firstSample, hal.recordLength());
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

// The beat the recorder is on, or -1 when nothing is running
int App::beatIndex()
{
    if (session.mode() == FourTrackSession::Mode::RecordingFirst && session.loopFrames() == 0)
    {
        const uint32_t tenths = set.tempoTenths > 0 ? set.tempoTenths : 1200u;
        const uint32_t beatFrames = (uint32_t)((uint64_t)hal.sampleRate() * 600 / tenths);
        return beatFrames > 0 ? (int)(hal.recordLength() / beatFrames) : -1;
    }
    if (!hal.playerPlaying() || session.bars() <= 0 || session.loopFrames() == 0) return -1;
    const uint32_t beat = session.loopFrames() / ((uint32_t)session.bars() * 4);
    return beat == 0 ? -1 : (int)(hal.playerPosition() / beat);
}

// The pad lights while the pads play chords: what each pad is (tiers), a flash that fades when it is
// played, and a small pulse on the tonic at every beat (a stronger one on the bar). The lights only
// have brightness, no colour, so the tiers are steps of brightness.
void App::serviceLights()
{
    const uint32_t now = hal.millis();
    if (!padsChords)
    {
        if (lightsOn)
        {
            for (int p = 0; p < 48; p++) { hal.padLight(p, 0); lightSent[p] = 255; }
            lightsOn = false;
        }
        return;
    }
    if (lightsOn && now - lightTimer < 30) return;
    lightTimer = now;
    lightsOn = true;
    const int beat = beatIndex();
    if (beat >= 0 && beat != lightBeat) beatAt = now | 1u;
    lightBeat = beat;
    const uint32_t sinceBeat = beatAt != 0 ? now - beatAt : 100000u;
    const int pulse = sinceBeat < 110 ? (lightBeat % 4 == 0 ? 10 : 5) * (int)(110 - sinceBeat) / 110 : 0;
    for (int p = 0; p < 48; p++)
    {
        // brightness: held, latched or chosen 100%; in-key chords 40%; chord tones of the sounding chord 70%;
        // other modifiers and styles 25%; free pads off
        int base = 0;
        const TrackRole sr = selectedRole();
        if (sr == TrackRole::Bass || sr == TrackRole::Melody || sr == TrackRole::Drums)
        {
            const int lv = roleLight(p);
            if (lightSent[p] != (uint8_t)lv) { lightSent[p] = (uint8_t)lv; hal.padLight(p, lv); }
            continue;
        }
        const PadInfo pi = Chords::info(p);
        switch (pi.role)
        {
            case Role::Chord: base = chords.inKey(p) ? 12 : 6; if (chords.isTonic(p)) base += pulse; break;
            case Role::Note: base = chords.isChordTone(p) ? 22 : 4; break;
            case Role::Mod: base = (chords.mods >> pi.index) & 1 ? 31 : 8; break;
            case Role::Style: base = chords.styleChosen(pi.index) ? 31 : 8; break;
            case Role::Hold: base = chHold ? 31 : 8; break;
            case Role::OctDown: case Role::OctUp: base = 8; break;
            default: break;
        }
        int level = base;
        if (heldCount[p] != 0) level = 31;
        else if (now - hitAt[p] < 260 && hitAt[p] != 0)
        {
            const int fade = 31 - (int)(now - hitAt[p]) * (31 - base) / 260;
            if (fade > level) level = fade;
        }
        if (level > 31) level = 31;
        if (lightSent[p] != (uint8_t)level)
        {
            lightSent[p] = (uint8_t)level;
            hal.padLight(p, level);
        }
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
    serviceTail();
    serviceCountIn();
    serviceClick();
    serviceChordNotes();
    serviceRoleRepeat(hal.millis());
    serviceCalibrate();
    if (latencyDirty && hal.millis() - latencyDirtyAt > 1500) { latencyDirty = false; latencySave(); loopCfgSave(); }
    serviceLights();
    if (soundDirty && hal.millis() - soundDirtyAt > 1500) { soundDirty = false; soundSave(); }
    serviceJob();
    serviceFirmware();
    refresh(false);
}

void App::serviceFirmware()
{
    if (!fwUpdating) return;
    if (!fwInstalled && hal.millis() - fwStartMs > 700) { fwInstalled = true; hal.firmwareInstall(fwNames[fwSel]); } // after the screen has been drawn
    if (fwInstalled && hal.firmwareProgress() >= 100) { fwUpdating = false; fwInstalled = false; back(); } // only the emulator gets here: a Tracker restarts
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
        maskOk[track] = false;
        return;
    }
    // 600 columns, one per pixel; the 120 columns of plan A are the peaks of five of them
    const int columns = 600;
    const int32_t off = alignShift[track] + userOffset[track];
    for (int c = 0; c < columns; c++)
    {
        // The waveform shows the loop as it plays, so a shifted Part is drawn shifted
        int64_t from = (int64_t)(((uint64_t)c * frames) / columns) - off;
        const int64_t L = (int64_t)frames;
        from = ((from % L) + L) % L;
        const int64_t to = from + (int64_t)(frames / columns);
        uint16_t peak = 0;
        const uint32_t span = (uint32_t)(to - from);
        if ((uint32_t)from + span <= frames) peak = loopPeakBlock(data + from, span, 0, (int)span);
        else
        {
            peak = loopPeakBlock(data + from, frames - (uint32_t)from, 0, (int)(frames - (uint32_t)from));
        }
        const float norm = squareRoot((float)peak / 32767.0f);
        const int h = (int)(norm * 255.0f);
        wave600[track][c] = (uint8_t)(h > 255 ? 255 : h);
    }
    for (int c = 0; c < 120; c++)
    {
        uint8_t m = 0;
        for (int k = 0; k < 5; k++) if (wave600[track][c * 5 + k] > m) m = wave600[track][c * 5 + k];
        lane.wave[c] = m;
    }
    lane.waveCount = 120;
    if (set.waveBitmap) uploadMasks(track);
    else maskOk[track] = false;
}

// The three bitmaps of a track's waveform, drawn from the 600 peaks and sent to the display chip a
// band of rows at a time (so only a small buffer is needed)
void App::uploadMasks(int track)
{
    constexpr int kBand = 16;
    uint8_t buf[kBand * kMaskStride];
    for (int size = 0; size < kMaskSizes; size++)
    {
        const int h = kMaskHeight[size];
        const int mid = h / 2;
        for (int r0 = 0; r0 < h; r0 += kBand)
        {
            const int rows = (h - r0) < kBand ? (h - r0) : kBand;
            for (int i = 0; i < rows * kMaskStride; i++) buf[i] = 0;
            for (int c = 0; c < kMaskW; c++)
            {
                // half the height of this column in pixels, at least one pixel each side of the centre
                const int half = 1 + (wave600[track][c] * (mid - 1)) / 255;
                for (int r = 0; r < rows; r++)
                {
                    const int row = r0 + r;
                    const int dist = row < mid ? mid - 1 - row : row - mid;
                    if (dist < half) buf[r * kMaskStride + (c >> 3)] |= (uint8_t)(0x80 >> (c & 7));
                }
            }
            hal.maskWrite(maskSlot(track, size), (uint32_t)(r0 * kMaskStride), buf, (uint32_t)(rows * kMaskStride));
        }
    }
    maskOk[track] = true;
}

// The take being recorded, as columns that grow behind the playhead
void App::computeLive(int track)
{
    Lane& L = model.lane[track];
    const uint32_t rate = hal.sampleRate();
    const uint32_t loop = session.loopFrames();
    const bool first = session.mode() == FourTrackSession::Mode::RecordingFirst;
    const uint32_t span = first ? rate * kMaxTakeSeconds : loop;
    const uint32_t per = span / 600; // one column per pixel
    if (per == 0) { L.liveCount = 0; return; }
    const uint32_t len = hal.recordLength();
    const uint32_t start = first ? takeOffset : 0;
    const int16_t* d = hal.region(track);
    int cols = (int)((len > start ? len - start : 0) / per);
    if (cols > 600) cols = 600;
    const int from = liveDone > 1 ? liveDone - 1 : 0;
    for (int c = from; c < cols; c++)
    {
        const uint16_t peak = loopPeakBlock(d + start + (uint32_t)c * per, per, 0, (int)per);
        const int h = (int)(squareRoot((float)peak / 32767.0f) * 255.0f);
        live600[c] = (uint8_t)(h > 255 ? 255 : h);
    }
    // the 120 columns of plan A are the peaks of five of them
    const int cols120 = cols / 5;
    for (int c = 0; c < cols120; c++)
    {
        uint8_t m = 0;
        for (int k = 0; k < 5; k++) if (live600[c * 5 + k] > m) m = live600[c * 5 + k];
        L.live[c] = m;
    }
    L.liveCount = (uint8_t)cols120;
    // plan D: the lane's big picture grows with the take; only the new columns are sent
    L.liveBitmap = false;
    if (set.waveBitmap)
    {
        const int slot = maskSlot(track, 1);
        if (liveDone == 0)
        {
            uint8_t zero[16 * kMaskStride] = {};
            for (int r0 = 0; r0 < kMaskHeight[1]; r0 += 16)
            {
                const int rows = (kMaskHeight[1] - r0) < 16 ? (kMaskHeight[1] - r0) : 16;
                hal.maskWrite(slot, (uint32_t)(r0 * kMaskStride), zero, (uint32_t)(rows * kMaskStride));
            }
            maskOk[track] = false; // the lane's finished picture is replaced when the take ends
        }
        if (cols > from)
        {
            const int b0 = from >> 3, b1 = (cols - 1) >> 3;
            const int mid = kMaskHeight[1] / 2;
            uint8_t row[kMaskStride];
            for (int r = 0; r < kMaskHeight[1]; r++)
            {
                const int dist = r < mid ? mid - 1 - r : r - mid;
                for (int b = b0; b <= b1; b++)
                {
                    uint8_t v = 0;
                    for (int k = 0; k < 8; k++)
                    {
                        const int c = b * 8 + k;
                        if (c < cols && dist < 1 + (live600[c] * 89) / 255) v |= (uint8_t)(0x80 >> k);
                    }
                    row[b - b0] = v;
                }
                hal.maskWrite(slot, (uint32_t)(r * kMaskStride + b0), row, (uint32_t)(b1 - b0 + 1));
            }
        }
        L.liveBitmap = true;
    }
    liveDone = cols;
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
    v.filter = liveFilter().active();
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
    v.gridMode = set.homeGrid;
    {
        const bool armedNow = recTrack >= 0 && (mode == FourTrackSession::Mode::RecordingFirst || mode == FourTrackSession::Mode::RecordingPart || mode == FourTrackSession::Mode::Armed);
        v.focusLane = (int8_t)(armedNow ? recTrack : (focusUser ? session.selected() : -1));
        const uint32_t now = hal.millis();
        if (now - blinkAt >= 500) { blinkAt = now; blinkOn = !blinkOn; }
        v.blink = blinkOn;
        float l = 0, r = 0;
        hal.inputLevels(l, r);
        v.inL = (uint8_t)0; v.inR = (uint8_t)0;
        const int dl = ampToDb(l), dr = ampToDb(r);
        v.inL = (uint8_t)(dl < -60 ? 0 : dl + 60);
        v.inR = (uint8_t)(dr < -60 ? 0 : dr + 60);
        v.waitDb = set.waitSound ? set.waitDb : 0;
        v.waitNow = (int8_t)waitLevelDb;
        fmt(v.inputName, sizeof(v.inputName), "%s", sourceName(trackSource[recTrack >= 0 ? recTrack : session.selected()]));
    }

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
        v.posFrames = hal.playerPlaying() ? hal.playerPosition() : scrubPos;
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
        if (!L.hasPart) { L.waveCount = 0; maskOk[t] = false; }
        L.bitmap = set.waveBitmap && maskOk[t] && L.hasPart;

        fmt(L.tag, sizeof(L.tag), "%s", set.roleFlow && roleChosen[t] ? (trackRole[t] == TrackRole::Audio ? sourceName(trackSource[t]) : roleTag((int)trackRole[t])) : sourceName(trackSource[t])); // every track shows where it records from, or its role
        if (L.active && mode == FourTrackSession::Mode::RecordingFirst && waitingSound)
        {
            fmt(L.msg1, sizeof(L.msg1), "Recording starts at the orange line");
            fmt(L.msg2, sizeof(L.msg2), "Jog moves the line");
        }
        else if (L.active && mode == FourTrackSession::Mode::RecordingFirst)
        {
            fmt(L.msg1, sizeof(L.msg1), "First take");
            const uint32_t took = hal.recordLength() > takeOffset ? hal.recordLength() - takeOffset : 0;
            formatTime(L.msg2, sizeof(L.msg2), took, rate);
            const uint32_t want = autoFrames();
            if (want > 0)
            {
                // the bars were set up front: say where in them the take is
                const uint32_t sixteenths = (uint32_t)((uint64_t)(took < want ? took : want) * set.bars * 16 / want);
                fmt(L.msg1, sizeof(L.msg1), "Recording %d bars", (int)set.bars);
                fmt(L.msg2, sizeof(L.msg2), "bar %u.%u of %d", (unsigned)(sixteenths / 16 + 1), (unsigned)((sixteenths % 16) / 4 + 1), (int)set.bars);
            }
            const uint32_t maxFrames = rate * kMaxTakeSeconds;
            const uint32_t got = took < maxFrames ? took : maxFrames;
            const uint32_t leftTenths = (maxFrames - got) * 10 / rate;
            L.progress = (uint8_t)(got * 100 / maxFrames);
            L.warn = leftTenths <= 30;
            fmt(L.side1, sizeof(L.side1), "%s", L.warn ? "Memory nearly full" : "Up to 16 s per Part");
            fmt(L.side2, sizeof(L.side2), "%u.%u s left", (unsigned)(leftTenths / 10), (unsigned)(leftTenths % 10));
            if (want > 0)
            {
                const uint32_t toGo = want > took ? want - took : 0;
                L.progress = (uint8_t)((took < want ? took : want) * 100 / want);
                L.warn = false;
                fmt(L.side1, sizeof(L.side1), "Stops on the bar line");
                fmt(L.side2, sizeof(L.side2), "%u.%u s left", (unsigned)(toGo * 10 / rate / 10), (unsigned)(toGo * 10 / rate % 10));
            }
        }
        else if (L.active && mode == FourTrackSession::Mode::Armed)
        {
            fmt(L.msg1, sizeof(L.msg1), "Recording starts at the loop start");
            const uint32_t pos = hal.playerPosition();
            const uint32_t toEnd = (loop > pos) ? (loop - pos) : 0;
            const uint32_t tenths = toEnd * 10 / rate;
            fmt(L.msg2, sizeof(L.msg2), "in %u.%u s - records one Loop, then stops", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
            const uint32_t beat = session.bars() > 0 ? loop / ((uint32_t)session.bars() * 4) : 0;
            if (beat > 0 && toEnd <= beat * 4)
            {
                // the last four beats before the Part starts: a countdown on the screen
                fmt(L.msg1, sizeof(L.msg1), "Get ready");
                fmt(L.msg2, sizeof(L.msg2), "%u", (unsigned)((toEnd + beat - 1) / beat));
            }
        }
        else if (L.active && mode == FourTrackSession::Mode::RecordingPart)
        {
            fmt(L.msg1, sizeof(L.msg1), "Recording one Loop");
            const uint32_t pos = hal.playerPosition();
            const int bars = session.bars() > 0 ? session.bars() : 1;
            const uint32_t sixteenths = loop > 0 ? (uint32_t)(((uint64_t)pos * bars * 16) / loop) : 0;
            fmt(L.msg2, sizeof(L.msg2), "bar %u.%u of %d", (unsigned)(sixteenths / 16 + 1), (unsigned)((sixteenths % 16) / 4 + 1), bars);
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

    for (int t = 0; t < FourTrackSession::kTracks; t++) { v.lane[t].liveCount = 0; v.lane[t].liveBitmap = false; }
    if (recTrack >= 0 && (mode == FourTrackSession::Mode::RecordingFirst || mode == FourTrackSession::Mode::RecordingPart) && !waitingSound) computeLive(recTrack);
    else liveDone = 0;

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
        if (waitingSound) fmt(v.hint, sizeof(v.hint), "Waiting for your first note. Jog sets the level it starts at (%d dB). Stop cancels.", (int)set.waitDb);
        else if (v.lane[session.recordingTrack() < 0 ? 0 : session.recordingTrack()].warn) fmt(v.hint, sizeof(v.hint), "At 16 s the take stops by itself and keeps everything.");
        else if (autoFrames() > 0) fmt(v.hint, sizeof(v.hint), "The take stops by itself on the bar line. Stop ends it early.");
        else fmt(v.hint, sizeof(v.hint), "B2 or Stop ends the take. Then: How many bars?");
    }
    else if (mode == FourTrackSession::Mode::Armed) fmt(v.hint, sizeof(v.hint), "Records one Loop from the loop start, then stops. B1 again disarms.");
    else if (mode == FourTrackSession::Mode::RecordingPart) fmt(v.hint, sizeof(v.hint), "Stop keeps what was recorded and fills the rest of the Loop with silence.");
    else if (loop == 0) fmt(v.hint, sizeof(v.hint), "B1 records the first Part. B2: tempo, bars and Wait for sound.");
    else if (session.playing()) fmt(v.hint, sizeof(v.hint), "B2 stops. Jog or Up/Down select a track. Insert opens actions.");
    else fmt(v.hint, sizeof(v.hint), "B1 arms Track %d. It records %s from the next loop start.", session.selected() + 1, sourceName(trackSource[session.selected()]));
    if (mode == FourTrackSession::Mode::Idle && loop > 0 && session.hasPart(session.selected()))
        fmt(v.hint, sizeof(v.hint), "Track %d: Insert for actions. Shift+Insert move. Shift+B%d mute.", session.selected() + 1, session.selected() + 5);
    if (actionMenu) fmt(v.hint, sizeof(v.hint), "Track %d: choose an action. Insert or B8 closes.", session.selected() + 1);
    if (moveMode) fmt(v.hint, sizeof(v.hint), "Move Track %d: jog or arrows step by the grid, Shift+arrows by 1 ms.", session.selected() + 1);
    if (noteHint[0] != 0) fmt(v.hint, sizeof(v.hint), "%s", noteHint);
    fmt(v.loopMode, sizeof(v.loopMode), "%s", set.loopOnce ? "Play once" : "Repeat");
    fmt(v.padsMode, sizeof(v.padsMode), "%s", padsChords ? (set.roleFlow ? roleTag((int)selectedRole()) : "CHORD") : padsKeysMode ? "KEYS" : "PAD");

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
    sig = hashBytes(sig, &v.focusLane, sizeof(v.focusLane));
    sig = hashBytes(sig, &v.waitDb, sizeof(v.waitDb));
    sig = hashBytes(sig, &v.waitNow, sizeof(v.waitNow));
    sig = hashBytes(sig, v.inputName, sizeof(v.inputName));
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
        sig = hashBytes(sig, &L.bitmap, sizeof(L.bitmap));
        sig = hashBytes(sig, &waveVersion[t], sizeof(waveVersion[t]));
    }

    const bool wantPrompt = (session.mode() == FourTrackSession::Mode::ChoosingBars);
    if (wantPrompt && current == Screen::Play) { current = Screen::Main; dirtyAll(); } // the question is drawn on the home screen
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
    if (current == Screen::Load && ldPreviewing && !hal.previewActive()) ldPreviewing = false;
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
        case kPlay: togglePlay(); break;
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
        case kPlay: togglePlay(); break;
        case kStop: pressStop(); break;
        default: break;
    }
    if (!session.hasPart(session.selected())) actionMenu = false;
    refresh(true);
}
} // namespace ft
