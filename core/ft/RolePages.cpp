// Track roles: the role chooser, the sound list, and the pads of Bass, Melody and Drums.
#include "App.h"
#include "Pen.h"
#include "MiniFmt.h"
#include "dsp/Drums.h"

namespace ft
{
namespace
{
int clampv(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

const uint8_t kBassSounds[] = { 8, 9, 7, 6 };
const uint8_t kMelodySounds[] = { 10, 11, 7, 4, 5, 3 };

void noteLabel(int note, char* out, int size)
{
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (note % 12 == 0) fmt(out, size, "C%d", note / 12 - 1);
    else fmt(out, size, "%s", names[note % 12]);
}
} // namespace

//--------------------------------------------------------------------------------------------
// Sounds of each role

int App::roleSoundCount(TrackRole r) const
{
    switch (r)
    {
        case TrackRole::Bass: return (int)(sizeof kBassSounds);
        case TrackRole::Melody: return (int)(sizeof kMelodySounds);
        case TrackRole::Drums: return DrumKit::kKits;
        default: return kSynthPresets;
    }
}

int App::roleSoundId(TrackRole r, int i) const
{
    switch (r)
    {
        case TrackRole::Bass: return kBassSounds[clampv(i, 0, (int)sizeof kBassSounds - 1)];
        case TrackRole::Melody: return kMelodySounds[clampv(i, 0, (int)sizeof kMelodySounds - 1)];
        default: return clampv(i, 0, roleSoundCount(r) - 1);
    }
}

const char* App::roleSoundName(TrackRole r, int id) const
{
    return r == TrackRole::Drums ? DrumKit::kitName(id) : synthPreset(clampv(id, 0, kSynthPresets - 1)).name;
}

// Loads the role's sound so the pads (and a take) play it
void App::roleSoundApply(TrackRole r)
{
    const int id = roleSound[(int)r];
    if (r == TrackRole::Drums) { hal.synthSetKit(id); return; }
    if (r == TrackRole::Audio) return;
    sndPreset = clampv(id, 0, kSynthPresets - 1);
    sndBase = synthPreset(sndPreset);
    sndMacros = SynthMacros();
    if (r == TrackRole::Chords) chSound = sndPreset;
    // Glide and Mono shape the patch for Bass and Melody
    SynthPatch patch = synthApply(sndBase, sndMacros);
    if (r == TrackRole::Melody && roleMono) patch.mono = true;
    if ((r == TrackRole::Bass || r == TrackRole::Melody) && roleGlide) { patch.mono = true; patch.glide = 0.08f; }
    hal.synthSetPatch(patch);
    hal.synthSetLevelDb10(synthDb10);
}

void App::applyRole(int t, TrackRole r)
{
    trackRole[t] = r;
    roleChosen[t] = true;
    lastRole = r;
    if (r == TrackRole::Audio)
    {
        if (trackSource[t] == Source::Pads) trackSource[t] = Source::Line;
        if (padsChords) chordsOff();
    }
    else
    {
        trackSource[t] = Source::Pads;
        if (!padsChords) { hal.padsReleaseAll(); padsChords = true; }
    }
    roleAllOff();
    if (session.mode() == FourTrackSession::Mode::Idle) applySource(trackSource[t]);
    roleSoundApply(r);
    unsaved = true;
}

//--------------------------------------------------------------------------------------------
// 6.5a Role chooser and 6.5b sound list

void App::roleKey(int k)
{
    if (k >= kB1 && k <= kB5)
    {
        const TrackRole r = (TrackRole)(k - kB1);
        const int t = session.selected();
        applyRole(t, r);
        if (r == TrackRole::Audio) { back(); pressRecord(); return; } // a line or mic take: Input setup is Shift + B4
        roleList = 0;
        const int id = roleSound[(int)r];
        for (int i = 0; i < roleSoundCount(r); i++) if (roleSoundId(r, i) == id) roleList = i;
        open(Screen::RoleSound);
        return;
    }
    if (k == kB8) { back(); return; }
    refresh(true);
}

void App::roleSoundJog(int delta)
{
    const TrackRole r = selectedRole();
    roleList = clampv(roleList + (delta > 0 ? 1 : -1), 0, roleSoundCount(r) - 1);
    roleSound[(int)r] = (uint8_t)roleSoundId(r, roleList);
    roleAllOff();
    roleSoundApply(r);
    unsaved = true;
    refresh(true);
}

void App::roleSoundKey(int k)
{
    if (k == kUp) roleSoundJog(-1);
    else if (k == kDown) roleSoundJog(1);
    else if (k == kB7) { open(Screen::Role); return; }
    else if (k == kB8) { roleAllOff(); back(); pressRecord(); return; } // arms the track
}

void App::drawRole(Pen& p)
{
    p.topBar(model);
    p.text(14, 42, "What do you want to record?", F17B, pal.text);
    p.text(14, 70, "The pads change to fit. Each role starts with a sound you can change next.", F14, pal.dim);
    for (int i = 0; i < kRoleCount; i++)
    {
        const int x = 14 + i * 154, y = 110, w = 146, h = 210;
        const bool last = i == (int)lastRole;
        p.card(x, y, w, h, last ? pal.sel : pal.panel);
        if (last) p.rect(x + 8, y, w - 16, 2, pal.text);
        p.text(x + 12, y + 14, roleName(i), F17B, pal.text);
        p.text(x + 12, y + 44, roleTag(i), F12, pal.dim);
        // what the pads become: the blurb in short lines
        {
            const char* b = roleBlurb(i);
            char line[24];
            int n = 0, ly = y + 70;
            for (const char* c = b;; c++)
            {
                const bool end = *c == 0;
                if ((end || *c == ' ') && n >= 12) { line[n] = 0; p.text(x + 12, ly, line, F12, pal.text); ly += 16; n = 0; if (end) break; continue; }
                if (end) { if (n > 0) { line[n] = 0; p.text(x + 12, ly, line, F12, pal.text); } break; }
                if (n < 22) line[n++] = *c;
            }
        }
        char snd[40];
        if (i == (int)TrackRole::Audio) fmt(snd, sizeof snd, "Input setup: Shift + B4");
        else fmt(snd, sizeof snd, "%s", roleSoundName((TrackRole)i, roleSound[i]));
        p.text(x + 12, y + h - 28, snd, F12, pal.dim);
    }
    p.text(14, 412, "Choose a role. Shift + Rec skips this and records with the last one.", F14, pal.text);
    const char* l[8] = { "Chords", "Bass", "Melody", "Drums", "Audio", "", "", "Cancel" };
    p.buttons(l, (uint8_t)(1u << (int)lastRole));
}

void App::drawRoleSound(Pen& p)
{
    p.topBar(model);
    const TrackRole r = selectedRole();
    char t[40];
    fmt(t, sizeof t, "%s: choose the sound", roleName((int)r));
    p.text(14, 42, t, F17B, pal.text);
    const int count = roleSoundCount(r);
    const int rows = 12;
    int first = roleList - rows / 2;
    if (first < 0) first = 0;
    if (first + rows > count) first = count - rows > 0 ? count - rows : 0;
    for (int i = 0; i < rows && first + i < count; i++)
    {
        const int idx = first + i, id = roleSoundId(r, idx);
        const int y = 76 + i * 26;
        const bool focus = idx == roleList;
        p.rect(14, y, 360, 24, focus ? pal.sel : pal.panel);
        if (focus) p.rect(14, y, 360, 2, pal.text);
        p.text(26, y + 4, roleSoundName(r, id), focus ? F14B : F14, pal.text);
        p.textRight(362, y + 6, r == TrackRole::Drums ? DrumKit::kitStyle(id) : synthPreset(id).kind, F12, pal.dim);
    }
    p.text(400, 80, roleBlurb((int)r), F14, pal.text);
    p.text(400, 110, r == TrackRole::Drums ? "Play the right-hand pads to hear the kit." : "Play the pads to hear it.", F14, pal.dim);
    p.text(400, 134, "Jog or Up and Down change the sound.", F14, pal.dim);
    p.text(14, 412, "B8 arms the track. B7 goes back to the roles.", F14, pal.text);
    const char* l[8] = { "", "", "", "", "", "", "Roles", "Arm" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// The pads

void App::roleAllOff()
{
    repPad = -1;
    for (int p = 0; p < 48; p++)
    {
        if (roleSounding[p] > 0) hal.synthNote(roleSounding[p], 0, false);
        roleSounding[p] = 0;
        roleLatched[p] = false;
        roleOffAt[p] = 0;
        if (selectedRole() != TrackRole::Chords) heldCount[p] = 0;
    }
    roleMods = 0;
    drumRateHeld = -1;
    hal.synthMod(0.0f, 0.0f);
    hal.synthAllOff();
}

int App::roleLight(int p) const
{
    const TrackRole r = selectedRole();
    const RolePad rp = rolePad(r, p);
    if (heldCount[p] != 0) return 31;
    switch (rp.kind)
    {
        case RoleKind::Note:
        {
            uint8_t ch[kMaxChordNotes];
            const int n = roleCurrentChord(ch);
            if (rp.octave == 3) return 22;
            const int note = roleNote(chords.cfg, r, r == TrackRole::Melody ? roleMode : 0, rp.octave, rp.index, roleOct[(int)r]);
            for (int i = 0; i < n; i++) if (ch[i] % 12 == note % 12) return 22;
            return 5;
        }
        case RoleKind::Kit: return rp.index == roleSound[(int)TrackRole::Drums] ? 31 : 8;
        case RoleKind::Rate: return (drumRate == rp.index) ? 31 : 8;
        case RoleKind::Drum: return 14;
        default: return 8;
    }
}

void App::rolePadPress(int p, bool down, int velocity)
{
    const TrackRole r = selectedRole();
    const RolePad rp = rolePad(r, p);
    const uint32_t now = hal.millis();
    if (rp.kind == RoleKind::None) return;
    if (!down)
    {
        if (heldCount[p] == 0) return;
        heldCount[p] = 0;
        hitAt[p] = now;
        if (rp.kind == RoleKind::Rate) { drumRateHeld = -1; return; }
        if (rp.kind == RoleKind::Func)
        {
            if (rp.index == FnAccent) roleMods &= (uint8_t)~1u;
            else if (rp.index == FnGhost) roleMods &= (uint8_t)~2u;
            else if (rp.index == FnShort) roleMods &= (uint8_t)~4u;
            else if (rp.index == FnSoft) roleMods &= (uint8_t)~8u;
            else if (rp.index == FnBendUp || rp.index == FnBendDown || rp.index == FnVib) hal.synthMod(0.0f, 0.0f);
        }
        if (repPad == p) repPad = -1;
        if (roleSounding[p] > 0)
        {
            if (roleHold) roleLatched[p] = true;
            else { hal.synthNote(roleSounding[p], 0, false); roleSounding[p] = 0; }
        }
        return;
    }
    heldCount[p] = 1;
    hitAt[p] = now;
    const bool pitched = r == TrackRole::Bass || r == TrackRole::Melody;
    int base = chords.cfg.padVelocity && velocity >= 1 ? (velocity > 127 ? 127 : velocity) : 100;
    if (roleMods & 1) base = 127;
    if (roleMods & 2) base = 55;
    if (roleMods & 8 || drumSoft) base = 50;
    switch (rp.kind)
    {
        case RoleKind::Kit:
            roleSound[(int)TrackRole::Drums] = rp.index;
            hal.synthSetKit(rp.index);
            unsaved = true;
            return;
        case RoleKind::Rate:
            drumRateHeld = rp.index;
            if (drumLatch) drumRate = (drumRate == rp.index) ? -1 : rp.index;
            return;
        case RoleKind::Drum:
            hal.synthDrum(rp.index, base);
            if (drumRateHeld >= 0 || drumRate >= 0)
            {
                repPad = p;
                repStep = 0;
                const double beat = 60000.0 / currentBpm();
                repNext = now + (uint32_t)(beat * roleRepeatBeats(drumRateHeld >= 0 ? drumRateHeld : drumRate));
            }
            return;
        case RoleKind::Func: break;
        case RoleKind::Note: break;
        default: return;
    }
    if (rp.kind == RoleKind::Func)
    {
        switch (rp.index)
        {
            case FnOctDown: roleOct[(int)r] = clampv(roleOct[(int)r] - 1, -2, 2); return;
            case FnOctUp: roleOct[(int)r] = clampv(roleOct[(int)r] + 1, -2, 2); return;
            case FnGlide: roleGlide = !roleGlide; roleSoundApply(r); return;
            case FnMono: roleMono = !roleMono; roleSoundApply(r); return;
            case FnHold:
                roleHold = !roleHold;
                if (!roleHold)
                    for (int q = 0; q < 48; q++) if (roleLatched[q]) { hal.synthNote(roleSounding[q], 0, false); roleSounding[q] = 0; roleLatched[q] = false; }
                return;
            case FnAccent: roleMods |= 1u; return;
            case FnGhost: roleMods |= 2u; return;
            case FnShort: roleMods |= 4u; return;
            case FnSoft: roleMods |= 8u; return;
            case FnBendUp: hal.synthMod(2.0f, 0.0f); return;
            case FnBendDown: hal.synthMod(-2.0f, 0.0f); return;
            case FnVib: hal.synthMod(0.0f, 0.35f); return;
            case FnScale: roleMode = 0; return;
            case FnPent: roleMode = 1; return;
            case FnBlues: roleMode = 2; return;
            case FnChordLock: roleChordLock = !roleChordLock; return;
            case FnRepeat: case FnArpUp: case FnArpDown: roleRepeat = !roleRepeat; return;
            case FnRest: return;
            case FnSwing: drumSwing = (uint8_t)(drumSwing >= 70 ? 50 : drumSwing + 10); return;
            case FnLatch: drumLatch = !drumLatch; if (!drumLatch) drumRate = -1; return;
            case FnUndo: undoTake(); return;
            default: break;
        }
    }
    if (!pitched) return;
    // a pitched pad: a scale note, or a bass function from the chord that sounds
    uint8_t chord[kMaxChordNotes];
    const int count = roleCurrentChord(chord);
    int note = -1;
    if (rp.kind == RoleKind::Note)
    {
        note = rp.octave == 3 ? roleSafeNote(r, chord, count, rp.index, roleOct[(int)r])
                              : roleNote(chords.cfg, r, r == TrackRole::Melody ? roleMode : 0, rp.octave, rp.index, roleOct[(int)r]);
        if (roleChordLock && r == TrackRole::Melody && rp.octave != 3)
        {
            // keep to the chord: the nearest note of the chord
            int best = note, bestD = 99;
            for (int i = 0; i < count; i++)
                for (int o = -1; o <= 1; o++)
                {
                    const int cand = (note / 12 + o) * 12 + chord[i] % 12;
                    const int d = cand > note ? cand - note : note - cand;
                    if (d < bestD) { bestD = d; best = cand; }
                }
            note = best;
        }
    }
    else note = roleFuncNote(r, rp.index, chord, count, chords.cfg, roleOct[(int)r]);
    if (note < 0 || note > 127) return;
    // a new note ends the ones Hold kept
    for (int q = 0; q < 48; q++) if (roleLatched[q]) { hal.synthNote(roleSounding[q], 0, false); roleSounding[q] = 0; roleLatched[q] = false; }
    if (roleSounding[p] > 0) hal.synthNote(roleSounding[p], 0, false);
    roleSounding[p] = (int8_t)note;
    hal.synthNote(note, base, true);
    if (roleMods & 4u) roleOffAt[p] = now + 120;
    if (roleRepeat && r == TrackRole::Melody)
    {
        repPad = p;
        repStep = 0;
        repNext = now + (uint32_t)(60000.0 / currentBpm() * 0.25);
    }
}

void App::serviceRoleRepeat(uint32_t now)
{
    const TrackRole r = selectedRole();
    for (int p = 0; p < 48; p++)
        if (roleOffAt[p] != 0 && (int32_t)(now - roleOffAt[p]) >= 0)
        {
            roleOffAt[p] = 0;
            if (roleSounding[p] > 0) { hal.synthNote(roleSounding[p], 0, false); roleSounding[p] = 0; }
        }
    if (repPad < 0 || (int32_t)(now - repNext) < 0) return;
    const RolePad rp = rolePad(r, repPad);
    const double beat = 60000.0 / currentBpm();
    int rate = drumRateHeld >= 0 ? drumRateHeld : drumRate;
    if (rp.kind == RoleKind::Drum)
    {
        if (rate < 0) { repPad = -1; return; }
        const bool odd = (repStep & 1) != 0;
        const double step = beat * roleRepeatBeats(rate);
        repStep++;
        // swing delays every other hit; the next hit is measured from the straight grid
        const int base = drumSoft ? 50 : 100;
        hal.synthDrum(rp.index, odd ? base - 15 : base);
        repNext = repNext + (uint32_t)(step * (odd ? (200 - drumSwing) / 100.0 : drumSwing / 100.0) * 1.0);
        return;
    }
    if (rp.kind == RoleKind::Note || rp.kind == RoleKind::Func)
    {
        // melody repeat: the held note again at 1/16, alternating an octave when an Arp pad was chosen
        if (roleSounding[repPad] <= 0) { repPad = -1; return; }
        const int n = roleSounding[repPad];
        hal.synthNote(n, 0, false);
        hal.synthNote(n, 90, true);
        repNext += (uint32_t)(beat * 0.25);
        return;
    }
    repPad = -1;
}

//--------------------------------------------------------------------------------------------
// Drawing the pads of a role (the Play view)

void App::drawRolePad(Pen& p, int pad, int px, int py, int pw, int ph)
{
    const TrackRole r = selectedRole();
    const RolePad rp = rolePad(r, pad);
    const bool held = heldCount[pad] != 0;
    char name[24] = "";
    uint32_t fill = pal.panel, ink = pal.text;
    int font = F14;
    bool stroke = false, chosen = false;
    char caption[12] = "";
    uint8_t chord[kMaxChordNotes];
    const int count = roleCurrentChord(chord);
    switch (rp.kind)
    {
        case RoleKind::Note:
        {
            int note;
            if (rp.octave == 3) note = roleSafeNote(r, chord, count, rp.index, roleOct[(int)r]);
            else note = roleNote(chords.cfg, r, r == TrackRole::Melody ? roleMode : 0, rp.octave, rp.index, roleOct[(int)r]);
            noteLabel(note, name, sizeof name);
            bool tone = rp.octave == 3;
            for (int i = 0; i < count; i++) if (chord[i] % 12 == note % 12) tone = true;
            font = tone ? F17B : F17;
            ink = tone ? pal.text : pal.dim;
            stroke = tone;
            if (held) { fill = pal.text; ink = pal.bg; stroke = false; }
            break;
        }
        case RoleKind::Kit:
            fmt(name, sizeof name, "%s", DrumKit::kitName(rp.index));
            chosen = rp.index == roleSound[(int)TrackRole::Drums];
            font = chosen ? F14B : F14;
            ink = chosen ? pal.marker : pal.dim;
            if (chosen) fill = pal.sel;
            break;
        case RoleKind::Rate:
        {
            static const char* const rn[4] = { "1/8", "1/16", "1/32", "1/16T" };
            fmt(name, sizeof name, "%s", rn[rp.index & 3]);
            chosen = drumRate == rp.index || drumRateHeld == rp.index;
            font = chosen ? F14B : F14;
            if (chosen) { fill = pal.sel; ink = pal.marker; }
            else ink = pal.dim;
            break;
        }
        case RoleKind::Drum:
            fmt(name, sizeof name, "%s", DrumKit::voiceName(rp.index));
            for (int i = 0; name[i] != 0; i++) if (name[i] == ' ' && name[i + 1] == 'a') { name[i] = 0; fmt(caption, sizeof caption, "alt"); break; } // "Kick alt": the word, and alt under it
            font = F14B;
            if (held) { fill = pal.text; ink = pal.bg; }
            break;
        case RoleKind::Func:
        {
            fmt(name, sizeof name, "%s", roleFuncName(rp.index));
            bool on = false;
            switch (rp.index)
            {
                case FnGlide: on = roleGlide; break;
                case FnMono: on = roleMono; break;
                case FnHold: on = roleHold; break;
                case FnScale: on = roleMode == 0; break;
                case FnPent: on = roleMode == 1; break;
                case FnBlues: on = roleMode == 2; break;
                case FnChordLock: on = roleChordLock; break;
                case FnRepeat: case FnArpUp: case FnArpDown: on = roleRepeat; break;
                case FnLatch: on = drumLatch; break;
                case FnSoft: on = drumSoft || (roleMods & 8); break;
                case FnSwing: on = drumSwing > 50; break;
                case FnAccent: on = (roleMods & 1) != 0; break;
                case FnGhost: on = (roleMods & 2) != 0; break;
                case FnShort: on = (roleMods & 4) != 0; break;
                default: break;
            }
            if (rp.index == FnSwing) fmt(caption, sizeof caption, "%d%%", (int)drumSwing);
            // what a bass function plays now
            if (r == TrackRole::Bass && rp.index <= FnRest && rp.index != FnRest && rp.index != FnWalk)
            {
                const int n = roleFuncNote(r, rp.index, chord, count, chords.cfg, roleOct[(int)r]);
                if (n >= 0) { char nm[8]; noteLabel(n, nm, sizeof nm); static const char* const pcs[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; fmt(caption, sizeof caption, "%s", pcs[n % 12]); (void)nm; }
            }
            font = on || held ? F14B : F14;
            if (on || held) { fill = pal.sel; ink = (on && !held) ? pal.marker : pal.text; }
            break;
        }
        default: break;
    }
    p.card(px, py, pw, ph, fill);
    int ty = py + ph / 2 - 9;
    if (stroke || caption[0] != 0) ty = py + ph / 2 - 14;
    p.textMid(px, pw, ty, name, font, ink);
    if (caption[0] != 0) p.textMid(px, pw, py + ph - 18, caption, F12, pal.dim);
    else if (stroke) p.rect(px + pw / 2 - 11, py + ph - 12, 22, 4, lastFamily >= 0 ? pal.t[lastFamily] : pal.dim);
}
} // namespace ft
