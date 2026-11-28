// Drawing of the pages (every screen except the main one) into a Canvas, laid out exactly as
// in the design (docs/design, frames 4.2 to 4.13). Coordinates are the design's.
#include "App.h"
#include "Pen.h"
#include "loop_fill.h"
#include "BuildId.h"
#include "Mask.h"
#include "dsp/Drums.h"
#include "SoundArt.h"
#include <math.h>

namespace ft
{
namespace
{
const char* const kWindowName[5] = { "Off", "30 ms", "60 ms", "100 ms", "150 ms" };
const char* const kPreRollName[5] = { "0 ms", "60 ms", "120 ms", "250 ms", "500 ms" };
const char* const kRoundName[3] = { "Off", "0.1 BPM", "1 BPM" };
const char* const kFxName[(int)FxType::kCount] = { "Empty slot", "Jump back", "Jump back 2", "Stutter", "Reverse", "Pitch", "Bit-crush", "Overdrive", "Pan", "Low-pass", "High-pass", "Volume", "Random jumps", "Pitch steps" };
const char* const kSourceName[4] = { "Line", "Mic low", "Mic high", "Pads" };

void fxValue(const FxSlot& s, char* out, size_t n)
{
    char detail[24] = {};
    switch (s.type)
    {
        case FxType::Pitch: fmt(detail, sizeof detail, "%s%d st", s.semis < 0 ? "-" : "+", s.semis < 0 ? -s.semis : s.semis); break;
        case FxType::PitchSteps:
        {
            static const char* const names[5] = { "Mixed", "Fifths", "Octaves", "Thirds", "Sevenths" };
            fmt(detail, sizeof detail, "%s", names[s.semis < 0 || s.semis > 4 ? 0 : s.semis]);
            break;
        }
        case FxType::Stutter: fmt(detail, sizeof detail, "1/%d", s.amount < 25 ? 2 : s.amount < 50 ? 4 : s.amount < 75 ? 8 : 16); break;
        case FxType::Crush: case FxType::Drive: case FxType::LowPass: case FxType::HighPass: case FxType::Volume:
            fmt(detail, sizeof detail, "%d%%", s.amount); break;
        case FxType::Pan: { const int p = (int)s.amount - 100; fmt(detail, sizeof detail, "%s%d", p < 0 ? "L " : (p > 0 ? "R " : "C"), p < 0 ? -p : p); break; }
        default: break;
    }
    if (s.type == FxType::None) { out[0] = 0; return; }
    if (s.chance >= 100) fmt(out, n, "%s%s", detail, detail[0] ? ", always" : "always");
    else fmt(out, n, "%s%s%d%% of slices", detail, detail[0] ? ", " : "", s.chance);
}

void ms(char* out, size_t n, uint32_t frames, uint32_t rate)
{
    fmt(out, n, "%u ms", (unsigned)((uint64_t)frames * 1000 / (rate ? rate : 1)));
}

void dbText(char* out, size_t n, int tenths)
{
    const int a = tenths < 0 ? -tenths : tenths;
    fmt(out, n, "%s%d.%d dB", tenths < 0 ? "-" : "", a / 10, a % 10);
}
} // namespace

void App::buildPage()
{
    page.clear();
    Pen p(page, pal);
    if (current != Screen::Tempo) p.rect(0, 0, 800, 480, pal.bg);
    switch (current)
    {
        case Screen::Tempo: drawTempo(p); break;
        case Screen::Editor: drawEditor(p); break;
        case Screen::Glitch: drawGlitch(p); break;
        case Screen::Master: drawMaster(p); break;
        case Screen::Input: drawInput(p); break;
        case Screen::Project: drawProject(p); break;
        case Screen::NameEntry: drawNameEntry(p); break;
        case Screen::Settings: drawSettings(p); break;
        case Screen::Export: drawExport(p); break;
        case Screen::Pads: drawPads(p); break;
        case Screen::Memory: drawMemory(p); break;
        case Screen::Mixer: drawMixer(p); break;
        case Screen::PadSample: drawPadSample(p); break;
        case Screen::Chords: drawChords(p); break;
        case Screen::Play: drawPlayView(p); break;
        case Screen::Fx: drawFx(p); break;
        case Screen::ArmLevel: drawArmLevel(p); break;
        case Screen::Role: drawRole(p); break;
        case Screen::RoleSound: drawRoleSound(p); break;
        case Screen::Filter: drawFilter(p); break;
        case Screen::Sound: drawSound(p); break;
        case Screen::Calibrate: drawCalibrate(p); break;
        case Screen::Sets: drawSets(p); break;
        case Screen::SoundAdv: drawSoundAdv(p); break;
        case Screen::Load: drawLoad(p); break;
        case Screen::Resample: drawResample(p); break;
        case Screen::Device: if (fwUpdating) drawFirmwareUpdate(p); else drawDevice(p); break;
        default: break;
    }
    if (popupKind != Popup::None)
    {
        popupPage.clear();
        Pen pn(popupPage, pal);
        drawPopup(pn);
    }
}

static void heading(Pen& p, const View& v, const char* name, int y = 40)
{
    p.topBar(v);
    p.text(14, y, name, F17B, p.p.text);
}

//--------------------------------------------------------------------------------------------
// 4.2 Starting tempo: a panel over the home screen
void App::drawTempo(Pen& p)
{
    const bool set_ = set.tempoTenths > 0;
    p.rect(0, 228, 800, 172, pal.panel);
    p.rect(0, 228, 800, 1, pal.frame);
    p.rect(0, 229, 800, 2, pal.text);
    p.text(14, 242, "Starting tempo", F17B, pal.text);
    p.cell(1, 274, 97, 40, !set_, pal.cell);
    p.textMid(1, 97, 285, "Free", F14, pal.text);
    p.cell(101, 274, 97, 40, set_, pal.cell);
    p.textMid(101, 97, 285, "Set", F14, pal.text);
    const bool tempoFocus = set_ && sRow == 0;
    p.cell(220, 270, 280, 58, tempoFocus, pal.cell);
    p.text(232, 278, "Tempo", F12, pal.dim);
    char b[24];
    if (set_) fmt(b, sizeof b, "%u.%u BPM", set.tempoTenths / 10, set.tempoTenths % 10);
    else fmt(b, sizeof b, "Free");
    p.text(232, 296, b, F17B, set_ ? pal.text : pal.dim);
    if (tempoFocus) p.text(452, 278, "JOG", F12, pal.text);
    static const char* const countName[5] = { "Off", "1 beat", "2 beats", "4 beats", "8 beats" };
    p.cell(516, 270, 270, 58, sRow == 1, pal.cell);
    p.text(528, 278, "Count-in", F12, pal.dim);
    p.text(528, 296, countName[set.countIdx > 4 ? 4 : set.countIdx], F17B, pal.text);
    if (sRow == 1) p.text(740, 278, "JOG", F12, pal.text);
    p.text(528 + 90, 299, "clicks before the first take", F12, pal.dim);
    p.cell(1, 322, 197, 52, sRow == 2, pal.cell);
    p.text(13, 328, "Wait for sound", F12, pal.dim);
    char w[24];
    if (set.waitSound) fmt(w, sizeof w, "On at %d dB", (int)set.waitDb);
    else fmt(w, sizeof w, "Off");
    p.text(13, 344, w, F17B, pal.text);
    if (sRow == 2) p.text(150, 328, "JOG", F12, pal.text);
    const bool barsOn = set_;
    p.cell(220, 322, 150, 52, set_ && sRow == 3, pal.cell);
    p.text(232, 328, "Bars", F12, pal.dim);
    char bb[24];
    if (!set_) fmt(bb, sizeof bb, "-");
    else if (set.bars == 0) fmt(bb, sizeof bb, "Off");
    else fmt(bb, sizeof bb, "%d  (%d s)", (int)set.bars, (int)((set.bars * 2400u + set.tempoTenths / 2) / set.tempoTenths));
    p.text(232, 344, bb, F17B, barsOn ? pal.text : pal.dim);
    if (set_ && sRow == 3) p.text(330, 328, "JOG", F12, pal.text);
    p.text(386, 330, "Auto-stop: records the bars from your first sound.", F12, pal.dim);
    p.text(386, 348, "Off: Stop ends it, rounded to whole bars at the tempo.", F12, pal.dim);
    p.text(386, 366, "B3 switches Wait for sound. Left/Right choose a box, jog changes it.", F12, pal.dim);
}

//--------------------------------------------------------------------------------------------
// 4.4 Part Editor
void App::drawEditor(Pen& p)
{
    heading(p, model, "Part Editor");
    const TrackCfg& c = hal.bank().cfg[editTrack];
    const uint32_t rate = hal.sampleRate();
    const uint32_t loop = session.loopFrames();
    char b[64];
    fmt(b, sizeof b, "Track %d - Part 1", editTrack + 1);
    const int endx = p.text(120, 42, b, F14, pal.text);
    const int tw = p.tag(endx + 10, 41, sourceName(trackSource[editTrack]), false, 0, 0);
    static const char* const zoomName[4] = { "whole loop", "1/4", "1/16", "1/64" };
    fmt(b, sizeof b, "Grid %s - Snap 0 %s - Zoom %s", gridName(set.gridIdx), set.snapZero ? "on" : "off", editFieldIdx >= 4 ? zoomName[0] : zoomName[editZoom]);
    p.text(endx + 10 + tw + 12, 44, b, F12, pal.dim);
    p.text(600, 42, "Loop Mode", F12, pal.dim);
    p.text(670, 40, set.loopOnce ? "Play once" : "Repeat", F14, pal.text);

    // waveform, zoomed around the field being edited
    if (editWaveDirty) const_cast<App*>(this)->computeEditWave();
    p.rect(20, 64, 760, 200, pal.waveBg);
    p.g.color(pal.wave);
    if (set.waveBitmap) p.g.mask(kPageMaskSlot, 20, 64, kPageMaskW, kPageMaskH);
    else
    {
        uint8_t h[150];
        for (int i = 0; i < 150; i++)
        {
            int m = 0;
            for (int k = 0; k < 5; k++) if (editWave[i * 5 + k] > m) m = editWave[i * 5 + k];
            h[i] = (uint8_t)(2 + m * 170 / 255);
        }
        p.g.bars(25, 164, 5, 4, h, 150);
    }
    if (loop > 0)
    {
        const uint32_t ws = zoomStart(), span = zoomSpan();
        auto xOf = [&](int64_t pos) -> int {
            int64_t x = 20 + (pos - (int64_t)ws) * 760 / (int64_t)span;
            return x < 20 ? 20 : (x > 780 ? 780 : (int)x);
        };
        if (set.gridIdx > 0)
        {
            const uint32_t gs = gridFrames();
            if (span / gs <= 140)
                for (uint32_t pos = ((ws + gs - 1) / gs) * gs; pos < ws + span; pos += gs) p.rect(xOf(pos), 258, 1, 6, pal.dim);
        }
        const uint32_t end = c.trimEnd > loop ? loop : c.trimEnd;
        const int x0 = xOf(c.trimStart), x1 = xOf(end);
        p.rect(20, 64, x0 - 20 > 0 ? x0 - 20 : 0, 200, pal.bg);
        p.rect(x1, 64, 780 - x1 > 0 ? 780 - x1 : 0, 200, pal.bg);
        if (c.trimStart >= ws && c.trimStart <= ws + span) p.rect(x0, 64, 2, 200, pal.marker);
        if (end >= ws && end <= ws + span) p.rect(x1 - 2, 64, 2, 200, pal.marker);
        if (c.fadeIn > 0 && c.trimStart + c.fadeIn <= ws + span && c.trimStart + c.fadeIn >= ws) p.rect(xOf(c.trimStart + c.fadeIn), 64, 1, 200, pal.marker);
        if (c.fadeOut > 0 && end > c.fadeOut && end - c.fadeOut >= ws && end - c.fadeOut <= ws + span) p.rect(xOf(end - c.fadeOut), 64, 1, 200, pal.marker);
    }
    p.text(30, 247, "Loop start", F12, pal.dim);
    p.textRight(774, 247, "Loop end", F12, pal.dim);

    const int32_t off = userOffset[editTrack];
    const int offMs = (int)((off < 0 ? -(int64_t)off : (int64_t)off) * 1000 / (int)rate);
    if (set.loopOnce) fmt(b, sizeof b, "Offset %s%d ms. In Play once, anything past the Loop end is cut off.", off < 0 ? "-" : "+", offMs);
    else fmt(b, sizeof b, "Offset %s%d ms. In Repeat, the end of the Part wraps to the start.", off < 0 ? "-" : "+", offMs);
    p.text(14, 272, b, F14, pal.text);
    fmt(b, sizeof b, "Undo steps: %d", editHist);
    p.textRight(786, 274, b, F12, pal.dim);

    const char* names[6] = { "Trim start", "Trim end", "Fade in", "Fade out", "Normalize", "Offset" };
    char v[6][24];
    ms(v[0], 24, c.trimStart, rate);
    ms(v[1], 24, (c.trimEnd > loop ? loop : c.trimEnd), rate);
    ms(v[2], 24, c.fadeIn, rate);
    ms(v[3], 24, c.fadeOut, rate);
    fmt(v[4], 24, "%s", (c.gain > 1.005f || c.gain < 0.995f) ? "Applied" : "Not applied");
    fmt(v[5], 24, "%s%d ms", off < 0 ? "-" : "+", offMs);
    for (int i = 0; i < 6; i++)
    {
        const int x = 14 + (i % 3) * 258, y = 296 + (i / 3) * 72;
        p.cell(x, y, 254, 68, i == editFieldIdx, pal.panel);
        p.text(x + 12, y + 10, names[i], F12, pal.dim);
        p.text(x + 12, y + 28, v[i], F17B, pal.text);
    }
    // normalize: what pressing B2 would do
    {
        const int16_t* d = partData[editTrack];
        uint16_t pk = (d != nullptr && loop > 0) ? loopPeakBlock(d, loop, 0, (int)loop) : 0;
        char s[40];
        fmt(s, sizeof s, "Action - B2 - peak now %d dB", pk > 0 ? ampToDb((float)pk * c.gain / 32768.0f) : -60);
        p.text(284, 418, s, F12, pal.dim);
    }
    if (loop > 0 && session.bars() > 0)
    {
        const int32_t beat16 = (int32_t)(loop / ((uint32_t)session.bars() * 16)); // a sixteenth of a bar
        const int64_t thou = beat16 > 0 ? ((int64_t)off * 1000) / ((int64_t)beat16 * 16) : 0; // thousandths of a bar
        const int64_t a = thou < 0 ? -thou : thou;
        fmt(b, sizeof b, "%s%d.%03d bars", thou < 0 ? "-" : "+", (int)(a / 1000), (int)(a % 1000));
        p.text(662, 396, b, F17, pal.text);
    }
    const char* l[8] = { "Audition", "Normalize", "Undo", "Redo", set.snapZero ? "Snap 0 on" : "Snap 0 off", "Trim silence", "Cancel", "Done" };
    p.buttons(l, auditioning ? 1 : 0);
}

//--------------------------------------------------------------------------------------------
// 4.5 Glitch and live effects: four columns, four slots each
void App::drawGlitch(Pen& p)
{
    p.topBar(model);
    const uint32_t loop = session.loopFrames();
    for (int t = 0; t < 4; t++)
    {
        const int x = t * 200;
        const TrackCfg& c = hal.bank().cfg[t];
        const bool has = session.hasPart(t);
        p.rect(x + 1, 32, 199, 64, t == gTrack ? pal.sel : pal.panel);
        if (t > 0) p.rect(x, 32, 1, 368, pal.frame);
        char b[24];
        fmt(b, sizeof b, "%d Track %d", t + 1, t + 1);
        p.text(x + 12, 38, b, F17B, pal.text);
        if (has)
        {
            int tx = x + 12;
            tx += p.tag(tx, 62, sourceName(trackSource[t]), false, 0, 0) + 4;
            bool any = false;
            for (int s = 0; s < 4; s++) if (c.slot[s].on && c.slot[s].type != FxType::None) any = true;
            if (any) tx += p.tag(tx, 62, "GLT", true, pal.meter, pal.bg) + 6;
            p.text(tx, 64, c.granular ? "Pitch: Grain" : "Pitch: Tape", F12, pal.dim);
            p.rect(x + 12, 90, 176, 1, pal.track);
            if (loop > 0) p.rect(x + 12 + (int)((uint64_t)model.posFrames % loop * 176 / loop), 86, 2, 9, pal.text);
        }
        else p.text(x + 14, 64, "No Part", F12, pal.dim);
        for (int s = 0; s < 4; s++)
        {
            const FxSlot& f = c.slot[s];
            const int y = 107 + s * 75;
            const bool focus = (t == gTrack && s == gSlot);
            if (focus) { p.rect(x + 1, y - 7, 199, 72, pal.sel); p.rect(x + 1, y - 7, 199, 2, pal.text); }
            char n[4];
            fmt(n, sizeof n, "%d", s + 1);
            p.text(x + 12, y + 2, n, F12, pal.dim);
            p.text(x + 28, y, has ? kFxName[(int)f.type] : "Empty slot", F14, (f.on || c.held[s]) ? pal.text : pal.dim);
            if (f.type == FxType::None || !has) { p.rect(x + 12, y + 31, 176, 4, pal.track); continue; }
            if (f.on || c.held[s]) p.rect(x + 162, y, 26, 18, pal.meter), p.text(x + 168, y + 2, c.held[s] ? "HLD" : "ON", F12, pal.bg);
            else { p.frame(x + 155, y, 33, 18, pal.dim); p.text(x + 162, y + 2, "OFF", F12, pal.dim); }
            p.bar(x + 12, y + 31, 176, 4, f.chance, (f.on || c.held[s]) ? pal.meter : pal.waveOff, pal.track);
            char v[40];
            fxValue(f, v, sizeof v);
            p.text(x + 12, y + 43, v, F12, (f.on || c.held[s]) ? pal.text : pal.dim);
        }
    }
    char b[96];
    fmt(b, sizeof b, "Track %d, slot %d. B5-B8 track. Jog chance. Insert latch. Hold B1-B4 now. Shift+B1/2/3: clear, all, pitch.", gTrack + 1, gSlot + 1);
    p.hint(b);
    const char* l[8] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Track 1", "Track 2", "Track 3", "Track 4" };
    uint8_t on = (uint8_t)(1u << (4 + gTrack));
    for (int s = 0; s < 4; s++) if (hal.bank().cfg[gTrack].held[s]) on |= (uint8_t)(1u << s);
    p.buttons(l, on);
}

//--------------------------------------------------------------------------------------------
// C3 Pad sample settings (Sample Play button)
void App::drawPadSample(Pen& p)
{
    heading(p, model, "Pad sample");
    const int slot = hal.currentSample();
    PadSample s;
    const bool has = hal.padSample(slot, s);
    char b[64];
    if (!has)
    {
        p.text(116, 42, "No sample on this pad", F14, pal.dim);
        p.text(14, 100, "Load a sample with Sample Load, or press another pad to look at its sample.", F14, pal.text);
        const char* l[8] = { "", "Keys", "Pad", "", "", "", "", "Back" };
        p.buttons(l, padsKeysMode ? 2 : 4);
        return;
    }
    fmt(b, sizeof b, "Slot %d - %s", slot + 1, s.name);
    const int ex = p.text(116, 42, b, F14, pal.text);
    const uint32_t hund = (uint32_t)((uint64_t)s.frames * 100 / (hal.sampleRate() ? hal.sampleRate() : 1));
    fmt(b, sizeof b, "%u.%02u s - %u KB", hund / 100, hund % 100, s.bytes / 1000);
    p.text(ex + 12, 43, b, F12, pal.dim);
    p.text(676, 43, "Pads", F12, pal.dim);
    p.frame(708, 41, p.width(padsKeysMode ? "KEYS" : "PAD", F12) + 8, 18, pal.dim);
    p.text(712, 43, padsKeysMode ? "KEYS" : "PAD", F12, pal.text);
    // waveform with Start and End
    p.rect(20, 64, 760, 200, pal.waveBg);
    p.g.color(pal.wave);
    if (set.waveBitmap)
    {
        // drawn once per sample: the picture is only rebuilt when the slot changes
        if (padWaveFor != slot)
        {
            hal.padSampleWave(slot, padWave, 760);
            uploadPageMask(padWave, 86);
            padWaveFor = slot;
            editWaveDirty = true; // the editor shares the picture
        }
        p.g.mask(kPageMaskSlot, 20, 64, kPageMaskW, kPageMaskH);
    }
    else
    {
        uint8_t h[150];
        hal.padSampleWave(slot, h, 150);
        for (int i = 0; i < 150; i++) h[i] = (uint8_t)(2 + h[i] * 170 / 255);
        p.g.bars(25, 164, 5, 4, h, 150);
    }
    const int xs = 20 + (int)((uint32_t)s.start * 760 / 65535), xe = 20 + (int)((uint32_t)s.end * 760 / 65535);
    p.rect(20, 64, xs - 20, 200, pal.bg);
    p.rect(xe, 64, 780 - xe, 200, pal.bg);
    p.rect(xs, 64, 1, 200, pal.marker);
    p.rect(xe, 64, 1, 200, pal.marker);
    if (s.loop) p.rect(xs, 64, xe - xs, 3, pal.marker);
    p.text(xs + 6, 247, "Start", F12, pal.dim);
    p.textRight(xe - 6, 247, "End", F12, pal.dim);
    p.text(14, 272, s.loop ? "Loop: plays Start to End and repeats while the pad is held." : "One-shot: plays Start to End once.", F14, pal.text);
    p.textRight(786, 274, padsKeysMode ? "Keys: one sample across the 48 pads" : "Pad: each pad plays its own sample", F12, pal.dim);
    // the six controls
    const char* names[6] = { "Tune", "Volume", "Pan", "Play mode", "Start", "End" };
    char v[6][24];
    fmt(v[0], 24, "%s%d st", s.tuneSt < 0 ? "-" : "+", s.tuneSt < 0 ? -s.tuneSt : s.tuneSt);
    { const int db10 = s.volume == 0 ? -600 : ampToDb((float)s.volume / 100.0f) * 10; const int a = db10 < 0 ? -db10 : db10; if (s.volume == 0) fmt(v[1], 24, "-inf dB"); else fmt(v[1], 24, "%s%d.%d dB", db10 < 0 ? "-" : "", a / 10, a % 10); }
    if (s.pan == 0) fmt(v[2], 24, "Centre"); else fmt(v[2], 24, "%s %d", s.pan < 0 ? "L" : "R", s.pan < 0 ? -s.pan : s.pan);
    fmt(v[3], 24, "%s", s.loop ? "Loop" : "One-shot");
    { const uint32_t t1 = (uint32_t)((uint64_t)s.start * s.frames / 65535 * 1000 / hal.sampleRate()); fmt(v[4], 24, "%u.%03u s", t1 / 1000, t1 % 1000); }
    { const uint32_t t2 = (uint32_t)((uint64_t)s.end * s.frames / 65535 * 1000 / hal.sampleRate()); fmt(v[5], 24, "%u.%03u s", t2 / 1000, t2 % 1000); }
    for (int i = 0; i < 6; i++)
    {
        const int x = 14 + (i % 3) * 258, y = 296 + (i / 3) * 72;
        p.cell(x, y, 254, 68, i == psField, pal.panel);
        p.text(x + 12, y + 10, names[i], F12, pal.dim);
        p.text(x + 12, y + 28, v[i], F17B, pal.text);
    }
    fmt(b, sizeof b, "%s%d ct", s.tuneCt < 0 ? "-" : "+", s.tuneCt < 0 ? -s.tuneCt : s.tuneCt);
    p.text(14 + 98 + 12, 324, b, F17, pal.text);
    const char* l[8] = { "Audition", "Keys", "Pad", "", "", "", "", "Back" };
    p.buttons(l, (uint8_t)((psAuditioning ? 1 : 0) | (padsKeysMode ? 2 : 4)));
}

//--------------------------------------------------------------------------------------------
// C2 Load samples (Sample Load button)
static void tenthsMb(char* out, int size, uint32_t bytes)
{
    const uint32_t t = bytes / 100000;
    fmt(out, size, "%u.%u", t / 10, t % 10);
}

void App::drawLoad(Pen& p)
{
    heading(p, model, "Load samples");
    char b[96];
    // the folder, as a path from the card
    {
        int n = fmt(b, sizeof b, "SD");
        const char* d = ldDir;
        while (*d != 0 && n < 90)
        {
            if (*d == '/') { if (d[1] != 0) { b[n++] = ' '; b[n++] = '/'; b[n++] = ' '; } }
            else b[n++] = *d;
            d++;
        }
        b[n] = 0;
        p.text(140, 42, ldCard ? b : "SD", F12, pal.dim);
    }
    p.rect(480, 32, 1, 368, pal.frame);
    p.rect(481, 32, 319, 368, pal.panel);
    const bool haveFile = ldCard && ldCount > 0 && !ldEntry[ldSel].dir;
    // ---- the list on the left
    if (ldCard && ldCount > 0)
    {
        p.text(44, 63, "Name", F12, pal.dim);
        p.text(290, 63, "Length", F12, pal.dim);
        p.text(390, 63, "Size", F12, pal.dim);
        for (int r = 0; r < kLdRows; r++)
        {
            const int i = ldTop + r;
            if (i >= ldCount) break;
            const int y = 88 + r * 34;
            const bool sel = i == ldSel;
            if (sel) { p.rect(0, y - 8, 480, 32, pal.sel); p.rect(0, y - 8, 4, 32, pal.text); }
            // names that are too long lose their end
            char nm[52];
            int len = 0;
            for (; len < 47 && ldEntry[i].name[len] != 0; len++) nm[len] = ldEntry[i].name[len];
            nm[len] = 0;
            while (len > 3 && p.width(nm, sel ? F14B : F14) > 236) { nm[--len] = 0; nm[len - 1] = '.'; }
            p.text(44, y, nm, sel ? F14B : F14, pal.text);
            if (ldEntry[i].dir)
            {
                p.frame(24, y + 4, 12, 9, pal.dim);
                fmt(b, sizeof b, "%u file%s", ldEntry[i].count, ldEntry[i].count == 1 ? "" : "s");
                p.text(290, y, b, F14, pal.dim);
            }
            else
            {
                if (ldChannels[i] != 0) { fmt(b, sizeof b, "%u.%02u s", ldMs[i] / 1000, (ldMs[i] % 1000) / 10); p.text(290, y, b, F14, pal.text); }
                else p.text(290, y, "?", F14, pal.dim);
                fmt(b, sizeof b, "%u KB", (ldEntry[i].bytes + 500) / 1000);
                p.text(390, y, b, F14, pal.text);
            }
        }
    }
    else if (ldCard)
    {
        p.text(24, 83, "No WAV files in this folder.", F14, pal.text);
        p.text(24, 105, "B3 goes up a folder.", F12, pal.dim);
    }
    // ---- the right panel
    p.text(494, 41, "Preview", F12, pal.dim);
    p.rect(494, 78, 292, 90, pal.waveBg);
    if (!ldCard) p.textMid(494, 292, 116, "No card", F14, pal.dim);
    else if (ldCount == 0) p.textMid(494, 292, 116, "Nothing to preview", F14, pal.dim);
    else
    {
        char nm[52];
        int len = 0;
        for (; len < 47 && ldEntry[ldSel].name[len] != 0; len++) nm[len] = ldEntry[ldSel].name[len];
        nm[len] = 0;
        while (len > 3 && p.width(nm, F14B) > 290) { nm[--len] = 0; nm[len - 1] = '.'; }
        p.text(494, 57, nm, F14B, pal.text);
        if (haveFile && ldWaveOk)
        {
            uint8_t h[58];
            for (int c = 0; c < 58; c++) h[c] = (uint8_t)(2 + ldWave[c] * 76 / 255);
            p.g.color(pal.wave);
            p.g.bars(496, 123, 5, 4, h, 58);
            if (ldPreviewing && ldMs[ldSel] > 0)
            {
                uint32_t at = (hal.millis() - ldPreviewAt) * 292 / ldMs[ldSel];
                if (at > 291) at = 291;
                p.rect(494 + (int)at, 78, 1, 90, pal.text);
            }
        }
        else if (haveFile) p.textMid(494, 292, 116, "Cannot read this file", F14, pal.dim);
        if (haveFile && ldPreviewing)
        {
            for (int d = -6; d <= 6; d++) p.rect(495, 184 + d, 13 - (d < 0 ? -d : d) * 2, 1, pal.text);
            fmt(b, sizeof b, "Previewing - %u.%02u s", ldMs[ldSel] / 1000, (ldMs[ldSel] % 1000) / 10);
            p.text(512, 176, b, F12, pal.dim);
        }
        else if (haveFile)
        {
            fmt(b, sizeof b, "%u.%02u s - %s", ldMs[ldSel] / 1000, (ldMs[ldSel] % 1000) / 10, ldChannels[ldSel] == 1 ? "mono" : "stereo");
            p.text(494, 176, b, F12, pal.dim);
        }
        else
        {
            fmt(b, sizeof b, "%u WAV file%s inside", ldEntry[ldSel].count, ldEntry[ldSel].count == 1 ? "" : "s");
            p.text(494, 176, b, F12, pal.dim);
        }
    }
    p.rect(481, 200, 319, 1, pal.frame);
    p.text(494, 211, "To slot", F12, pal.dim);
    fmt(b, sizeof b, "%d", ldSlot + 1);
    const int ex = p.text(494, 229, b, F17B, pal.text);
    char old[28];
    const bool used = ldCard && hal.sampleName(ldSlot, old, sizeof old);
    p.text(ex + 16, 231, used ? old : "Empty", F14, used ? pal.text : pal.dim);
    if (used && haveFile) p.text(494, 255, "Loading replaces it", F12, pal.warn);
    else p.text(494, 255, "Left/Right change the slot", F12, pal.dim);
    p.rect(481, 280, 319, 1, pal.frame);
    p.text(494, 291, "Memory left", F12, pal.dim);
    uint32_t usedBytes = 0, totalBytes = 0;
    hal.sampleMemory(usedBytes, totalBytes);
    char l1[16], l2[16];
    if (ldCard)
    {
        tenthsMb(l1, sizeof l1, totalBytes - usedBytes);
        tenthsMb(l2, sizeof l2, totalBytes);
        fmt(b, sizeof b, "%s of %s MB", l1, l2);
        p.text(494, 309, b, F14, pal.text);
    }
    else p.text(494, 309, "-", F14, pal.text);
    p.rect(494, 334, 292, 6, pal.track);
    if (ldCard && totalBytes > 0) p.rect(494, 334, (int)((uint64_t)usedBytes * 292 / totalBytes), 6, pal.text);
    if (ldCard && haveFile && ldChannels[ldSel] != 0)
    {
        PadSample was;
        const uint32_t freed = hal.padSample(ldSlot, was) ? was.bytes : 0;
        const uint32_t need = ldMs[ldSel] * 88u;
        const int64_t left = (int64_t)totalBytes - (int64_t)usedBytes + (int64_t)freed - (int64_t)need;
        if (left >= 0) { tenthsMb(l1, sizeof l1, (uint32_t)left); fmt(b, sizeof b, "After loading: %s MB", l1); p.text(494, 349, b, F12, pal.dim); }
        else p.text(494, 349, "Does not fit", F12, pal.warn);
    }
    // ---- the hint and the buttons
    p.hint(!ldCard ? "Insert the SD card to list samples." : ldCount == 0 ? "Only WAV files are listed." : "Up/Down choose. Jog scrolls. Left/Right change the slot.");
    const char* l[8] = { "Load", "Preview", "Up folder", "Load next", "", "To track", "", "Back" };
    uint8_t dim = 1u << 5;
    if (!haveFile) dim |= (1u << 0) | (1u << 1) | (1u << 3);
    p.buttons(l, ldPreviewing ? 2 : 0, -1, dim);
}

//--------------------------------------------------------------------------------------------
// C4 Resample (Sample Rec button)
static int levelPx(int db)
{
    const int v = (db + 48) * 772 / 48;
    return v < 0 ? 0 : (v > 772 ? 772 : v);
}

void App::drawResample(Pen& p)
{
    heading(p, model, "Resample", 41);
    const uint32_t rate = hal.sampleRate() ? hal.sampleRate() : 44100;
    const uint32_t loop = session.loopFrames();
    const uint32_t frames = loop * (uint32_t)rsLoops;
    uint32_t used = 0, total = 0;
    hal.sampleMemory(used, total);
    const uint32_t freeB = rsFreeBytes();
    Rs st = rsState;
    if ((st == Rs::Ready || st == Rs::Full)) st = (loop > 0 && frames * 2 > freeB) ? Rs::Full : Rs::Ready;
    const bool rec = st == Rs::Recording, full = st == Rs::Full, saved = st == Rs::Saved;
    char b[96];
    // the state tag next to the title
    {
        const char* t = rec ? "RECORDING" : saved ? "SAVED" : full ? "MEMORY FULL" : "READY";
        const int w = p.width(t, F12) + 6;
        if (rec)
        {
            p.dot(113, 50, 5, pal.rec);
            p.rect(121, 41, w, 18, pal.rec);
            p.text(124, 43, t, F12, pal.bg);
        }
        else if (saved || full)
        {
            p.rect(108, 41, w, 18, saved ? pal.meter : pal.warn);
            p.text(111, 43, t, F12, pal.bg);
        }
        else
        {
            p.frame(108, 41, w, 18, pal.dim);
            p.text(111, 43, t, F12, pal.text);
        }
    }
    // source, slot, name
    char old[28];
    const bool used_ = hal.sampleName(rsSlot, old, sizeof old);
    for (int c = 0; c < 3; c++)
    {
        const int x = 14 + c * 258;
        p.cell(x, 72, 254, 68, c == 1 && !rec, pal.panel);
        p.text(x + 12, 83, c == 0 ? "Source" : c == 1 ? "To slot" : "Name", F12, pal.dim);
    }
    p.text(26, 101, "Mix", F17B, pal.text);
    p.text(26, 123, "After the lo-fi master", F12, pal.dim);
    fmt(b, sizeof b, "%d", rsSlot + 1);
    p.text(284, 101, b, F17B, pal.text);
    if (used_ && !saved) p.text(284, 123, old, F12, pal.warn);
    else p.text(284, 123, used_ ? old : "Empty", F12, pal.dim);
    p.text(542, 101, rsName, F17B, pal.text);
    p.text(542, 123, "B3 renames", F12, pal.dim);
    // length
    p.text(14, 159, "Length", F12, pal.dim);
    {
        const uint32_t secT = (uint32_t)((uint64_t)frames * 100 / rate);
        const uint32_t doneT = (uint32_t)((uint64_t)jobDone * 100 / rate);
        if (rec) fmt(b, sizeof b, "%u.%02u of %u.%02u s", doneT / 100, doneT % 100, secT / 100, secT % 100);
        else if (saved)
        {
            const uint32_t t10 = rsBytesSaved / 100000;
            fmt(b, sizeof b, "%u.%02u s - %u.%u MB", secT / 100, secT % 100, t10 / 10, t10 % 10);
        }
        else fmt(b, sizeof b, "%d Loop%s - %u.%02u s", rsLoops, rsLoops == 1 ? "" : "s", secT / 100, secT % 100);
        p.text(14, 177, b, F17B, pal.text);
    }
    if (!rec && !saved) p.text(320, 180, rsJogSlot ? "Jog: slot" : "Jog: 1 to 4 Loops", F12, pal.dim);
    p.rect(14, 206, 772, 6, pal.track);
    if (rec) p.rect(14, 206, (int)((uint64_t)jobDone * 772 / (jobTotal ? jobTotal : 1)), 6, pal.rec);
    else if (saved) p.rect(14, 206, 772, 6, pal.text);
    // level: both channels, from -48 to 0 dB
    p.text(14, 227, "Level", F12, pal.dim);
    int dl = -60, dr = -60;
    if (rec) { dl = rsLevelL; dr = rsLevelR; }
    else if (!saved && !full && hal.playerPlaying())
    {
        for (int t = 0; t < 4; t++) if (session.hasPart(t) && trackDb[t] > dl) dl = dr = trackDb[t];
    }
    p.rect(14, 244, 772, 10, pal.track);
    p.rect(14, 258, 772, 10, pal.track);
    p.rect(14, 244, levelPx(dl), 10, rec ? pal.rec : pal.meter);
    p.rect(14, 258, levelPx(dr), 10, rec ? pal.rec : pal.meter);
    p.rect(690, 240, 1, 32, pal.dim);
    p.text(696, 277, "-6", F12, pal.dim);
    // memory
    p.text(14, 303, "Memory left", F12, pal.dim);
    {
        const uint32_t secF = freeB / (rate * 2);
        char a[16];
        const uint32_t t1 = freeB / 100000, t2 = total / 100000;
        if (secF >= 60) fmt(a, sizeof a, "%u min %02u s", secF / 60, secF % 60);
        else fmt(a, sizeof a, "%u.%u s", secF, (uint32_t)((uint64_t)freeB * 10 / (rate * 2)) % 10);
        if (full) fmt(b, sizeof b, "%u.%u MB - fits %s", t1 / 10, t1 % 10, a);
        else fmt(b, sizeof b, "%u.%u of %u.%u MB - fits %s", t1 / 10, t1 % 10, t2 / 10, t2 % 10, a);
        p.text(14, 321, b, F14, full ? pal.warn : pal.text);
    }
    p.rect(14, 346, 772, 6, pal.track);
    if (total > 0) p.rect(14, 346, full ? 772 : (int)((uint64_t)(total - (freeB < total ? freeB : total)) * 772 / total), 6, full ? pal.warn : pal.text);
    // hint
    if (rec) fmt(b, sizeof b, "Recording the mix into slot %d. B1 cancels and keeps nothing.", rsSlot + 1);
    else if (saved) fmt(b, sizeof b, "Saved to slot %d as %s. It plays on the pads now.", rsSlot + 1, rsName);
    else if (full)
    {
        const uint32_t t = (uint32_t)((uint64_t)loop * 100 / rate);
        fmt(b, sizeof b, "One Loop needs %u.%02u s. Free memory in Load samples or Memory.", t / 100, t % 100);
    }
    else fmt(b, sizeof b, "B1 records the mix from the loop start to the loop end.");
    p.rect(0, 400, 800, 1, pal.frame);
    p.text(14, 413, b, F14, full ? pal.warn : pal.text);
    // buttons: Record or Stop, Slot, Name, Back
    const char* l[8] = { "", "Slot", "Name", "", "", "", "", "Back" };
    uint8_t dim = 0;
    if (rec) dim = (1u << 1) | (1u << 2) | (1u << 7);
    p.buttons(l, rec ? 1 : 0, -1, dim);
    const bool b1dim = full;
    if (rec) { p.rect(17, 455, 10, 10, pal.text); p.text(35, 451, "Cancel", F14, pal.text); }
    else { if (!b1dim) p.dot(22, 460, 5, pal.rec); p.text(35, 451, "Record", F14, b1dim ? pal.dim : pal.text); }
}

//--------------------------------------------------------------------------------------------
// C5 Device settings (Config button)
void App::drawDevice(Pen& p)
{
    heading(p, model, "Device settings", 39);
    p.textRight(786, 42, "Saved on the device, for every project", F12, pal.dim);
    static const char* const groups[9] = { "MIDI", "USB audio", "Line input", "Volume", "Display", "Four-Track", "Chords", "Firmware", "About" };
    for (int g = 0; g < 9; g++)
    {
        const int y = 64 + g * 34;
        if (g == dvGroup)
        {
            p.rect(0, y, 200, 32, pal.sel);
            if (!dvRows) p.rect(0, y, 4, 32, pal.text);
        }
        p.text(16, y + 8, groups[g], g == dvGroup ? F14B : F14, g == dvGroup ? pal.text : pal.dim);
    }
    p.rect(200, 64, 1, 300, pal.frame);
    char name[100], val[100], help[100];
    const int rows = deviceRowCount(dvGroup);
    char focusName[80] = "", focusHelp[100] = "";
    for (int r = 0; r < rows; r++)
    {
        deviceRow(dvGroup, r, name, val, help, 100);
        const int y = 64 + r * 34;
        const bool focus = dvRows && r == dvRow;
        if (focus)
        {
            p.rect(201, y, 599, 32, pal.sel);
            p.rect(201, y, 599, 2, pal.text);
        }
        p.text(216, y + 8, name, focus ? F14B : F14, pal.text);
        p.text(500, y + 7, val, F17B, pal.text);
        if (r == (dvRows ? dvRow : 0)) { fmt(focusName, sizeof focusName, "%s", name); fmt(focusHelp, sizeof focusHelp, "%s", help); }
    }
    p.rect(0, 376, 800, 1, pal.frame);
    char title[120];
    fmt(title, sizeof title, "%s - %s", focusName, groups[dvGroup]);
    p.text(14, 387, title, F17B, pal.text);
    p.text(14, 411, focusHelp, F14, pal.text);
    const char* l[8] = { dvGroup == 7 && fwCount > 0 ? "Update" : (dvGroup == 5 && dvRow == 4 ? "Calibrate" : (dvGroup == 5 && dvRow == 3 ? "Set level" : "")), "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

void App::drawFirmwareUpdate(Pen& p)
{
    heading(p, model, "Updating firmware", 41);
    char b[100];
    fmt(b, sizeof b, "Installing from %s", fwNames[fwSel]);
    p.text(14, 77, b, F14, pal.text);
    const int pc = hal.firmwareProgress();
    p.rect(14, 112, 680, 10, pal.track);
    if (pc > 0) p.rect(14, 112, 680 * (pc > 100 ? 100 : pc) / 100, 10, pal.text);
    if (pc >= 0) { fmt(b, sizeof b, "%d%%", pc); p.textRight(786, 107, b, F17, pal.text); }
    p.text(14, 143, fwInstalled ? "Writing" : "Starting the updater", F12, pal.dim);
    p.rect(0, 180, 800, 1, pal.frame);
    p.text(14, 197, "Do not switch off or take out the card.", F17B, pal.warn);
    p.text(14, 225, "The Tracker restarts by itself when the update is done.", F14, pal.text);
    if (fwSavedFirst) fmt(b, sizeof b, "%s was saved before the update started.", projectName);
    else fmt(b, sizeof b, "%s had no changes to save.", projectName);
    p.text(14, 245, b, F14, pal.text);
    const char* l[8] = { "", "", "", "", "", "", "", "" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// C1 Track mixer (Params button)
void App::drawMixer(Pen& p)
{
    p.topBar(model);
    bool anySolo = false;
    for (int t = 0; t < 4; t++) if (hal.bank().cfg[t].solo && session.hasPart(t)) anySolo = true;
    for (int t = 0; t < 4; t++)
    {
        const int x = t * 200;
        const TrackCfg& c = hal.bank().cfg[t];
        const bool has = session.hasPart(t);
        const bool off = !has || c.mute || (anySolo && !c.solo);
        const bool sel = t == mxTrack;
        const uint32_t tc = off ? pal.dim : pal.text;
        const uint32_t fill = off ? pal.waveOff : pal.text;
        if (t > 0) p.rect(x, 32, 1, 368, pal.frame);
        p.rect(x + 1, 32, 199, 48, sel ? pal.sel : pal.panel);
        if (sel) p.rect(x + 1, 32, 4, 48, pal.text);
        char b[40];
        fmt(b, sizeof b, "%d Track %d", t + 1, t + 1);
        p.text(x + 14, 46, b, F17B, tc);
        if (c.mute) { p.rect(x + 142, 46, 20, 20, pal.marker); p.text(x + 146, 48, "M", F14, pal.bg); }
        else { p.frame(x + 142, 46, 20, 20, pal.dim); p.text(x + 146, 48, "M", F14, pal.dim); }
        if (c.solo) { p.rect(x + 168, 46, 20, 20, pal.text); p.text(x + 173, 48, "S", F14, pal.bg); }
        else { p.frame(x + 168, 46, 20, 20, pal.dim); p.text(x + 173, 48, "S", F14, pal.dim); }
        // focus cells
        if (sel && mxField == 0) { p.rect(x + 1, 84, 199, 96, pal.sel); p.rect(x + 1, 84, 199, 2, pal.text); }
        if (sel && mxField == 1) { p.rect(x + 1, 182, 199, 56, pal.sel); p.rect(x + 1, 182, 199, 2, pal.text); }
        if (sel && mxField == 2) { p.rect(x + 1, 240, 199, 56, pal.sel); p.rect(x + 1, 240, 199, 2, pal.text); }
        if (sel && mxField == 3) { p.rect(x + 1, 298, 199, 48, pal.sel); p.rect(x + 1, 298, 199, 2, pal.text); }
        if (sel && mxField == 4) { p.rect(x + 1, 348, 199, 50, pal.sel); p.rect(x + 1, 348, 199, 2, pal.text); }
        // level fader, zero mark and live meter
        p.rect(x + 20, 100, 16, 70, pal.track);
        int h = 117 + (int)c.levelDb10 * 3 / 10;
        h = h < 0 ? 0 : (h > 146 ? 146 : h);
        h = h * 70 / 146;
        p.rect(x + 20, 100 + 70 - h, 16, h, fill);
        p.rect(x + 14, 114, 40, 1, pal.dim);
        p.rect(x + 42, 100, 6, 70, pal.track);
        const int mh = has ? model.lane[t].meter * 70 / 100 : 0;
        p.rect(x + 42, 100 + 70 - mh, 6, mh, pal.meter);
        const int db = (int)c.levelDb10;
        const int adb = db < 0 ? -db : db;
        if (db <= -600) fmt(b, sizeof b, "-inf dB"); else fmt(b, sizeof b, "%s%d.%d dB", db < 0 ? "-" : "", adb / 10, adb % 10);
        p.text(x + 64, 100, b, F17B, tc);
        if (c.mute) p.text(x + 64, 124, "Muted", F12, pal.dim);
        else if (!has) p.text(x + 64, 124, "No Part", F12, pal.dim);
        else if (anySolo && !c.solo) p.text(x + 64, 124, "Off by solo", F12, pal.dim);
        // pan
        if (c.pan == 0) fmt(b, sizeof b, "Centre"); else fmt(b, sizeof b, "%s %d", c.pan < 0 ? "L" : "R", c.pan < 0 ? -c.pan : c.pan);
        p.text(x + 14, 190, b, F17B, tc);
        p.textRight(x + 188, 193, "Pan", F12, pal.dim);
        p.rect(x + 14, 222, 174, 4, pal.track);
        p.rect(x + 100, 216, 1, 16, pal.dim);
        p.rect(x + 99 + (int)c.pan * 9 / 10, 216, 3, 16, fill);
        // tone
        const int hz = toneCutoffHz(c.tone);
        if (c.tone == 0) fmt(b, sizeof b, "Flat");
        else if (hz >= 1000) fmt(b, sizeof b, "%s %d.%d kHz", c.tone < 0 ? "LP" : "HP", hz / 1000, (hz % 1000) / 100);
        else fmt(b, sizeof b, "%s %d Hz", c.tone < 0 ? "LP" : "HP", hz);
        p.text(x + 14, 248, b, F17B, tc);
        p.textRight(x + 188, 251, "Tone", F12, pal.dim);
        p.rect(x + 14, 280, 174, 4, pal.track);
        p.rect(x + 100, 274, 1, 16, pal.dim);
        p.rect(x + 99 + (int)c.tone * 9 / 10, 274, 3, 16, fill);
        // EQ: low and high shelves
        for (int e = 0; e < 2; e++)
        {
            const int v = e == 0 ? c.eqLo : c.eqHi;
            const int ty = e == 0 ? 306 : 356;
            fmt(b, sizeof b, "%s%d dB", v > 0 ? "+" : "", v);
            p.text(x + 14, ty, b, F14B, tc);
            p.textRight(x + 188, ty + 2, e == 0 ? "Low" : "High", F12, pal.dim);
            p.rect(x + 14, ty + 26, 174, 3, pal.track);
            p.rect(x + 100, ty + 22, 1, 11, pal.dim);
            p.rect(x + 99 + v * 7, ty + 22, 3, 11, fill);
        }
    }
    char hint[96];
    static const char* const fieldName[5] = { "Level", "Pan", "Tone", "Low EQ", "High EQ" };
    static const char* const fieldHelp[5] = { "Jog: 0.5 dB steps. Shift + jog: 0.1 dB.", "Jog: steps of 5. Shift + jog: 1.", "Left of centre darkens, right of centre thins the sound.", "Jog: 1 dB. Boosts or cuts the bass (about 120 Hz and below).", "Jog: 1 dB. Boosts or cuts the treble (about 5 kHz and above)." };
    fmt(hint, sizeof hint, "Track %d - %s. %s", mxTrack + 1, fieldName[mxField], fieldHelp[mxField]);
    p.hint(hint);
    const TrackCfg& sc = hal.bank().cfg[mxTrack];
    const char* l[8] = { sc.mute ? "Unmute" : "Mute", sc.solo ? "Unsolo" : "Solo", "Reset", "", "", "", "", "Back" };
    p.buttons(l, (uint8_t)((sc.mute ? 1 : 0) | (sc.solo ? 2 : 0)));
}

//--------------------------------------------------------------------------------------------
// 4.6 Lo-fi master
void App::drawMaster(Pen& p)
{
    p.topBar(model);
    const LofiCfg& c = hal.lofi().cfg;
    const bool byp = c.bypass;
    const char* names[5] = { "Glue", "Saturation", "Wow & flutter", "Bit-crush", "Master volume" };
    for (int r = 0; r < 5; r++)
    {
        const int y = 36 + r * 72;
        const bool on = r == 0 ? c.glueOn : r == 1 ? c.satOn : r == 2 ? c.wowOn : r == 3 ? c.crushOn : true;
        const bool lit = on && !byp;
        if (r == mRow) { p.rect(0, y, 800, 70, pal.sel); p.rect(0, y, 800, 2, pal.text); }
        const uint32_t tc = lit ? pal.text : pal.dim;
        p.text(14, y + 10, names[r], F17B, tc);
        if (on) { if (byp) { p.frame(14, y + 38, 26, 18, pal.dim); p.text(20, y + 40, "ON", F12, pal.dim); } else { p.rect(14, y + 38, 26, 18, pal.meter); p.text(19, y + 40, "ON", F12, pal.bg); } }
        else { p.frame(14, y + 38, 33, 18, pal.dim); p.text(21, y + 40, "OFF", F12, pal.dim); }
        int pct = r == 0 ? c.glue : r == 1 ? c.sat : r == 2 ? c.wowDepth : r == 3 ? (16 - c.crushBits) * 100 / 12 : c.volume;
        p.bar(200, y + 30, 270, 6, pct, lit ? pal.text : pal.waveOff, pal.track);
        char v[24];
        if (r == 2) fmt(v, sizeof v, "Depth %d%%", c.wowDepth);
        else if (r == 3) fmt(v, sizeof v, "%d bit", c.crushBits);
        else if (r == 4) dbText(v, sizeof v, -(100 - c.volume) * 480 / 100);
        else fmt(v, sizeof v, "%d%%", pct);
        p.text(486, y + 23, v, F17B, tc);
        if (r == 0)
        {
            p.text(630, y + 10, "Gain reduction", F12, pal.dim);
            const int red = hal.lofi().gainReductionDb();
            fmt(v, sizeof v, "%d dB", red);
            p.text(630, y + 26, v, F17, pal.text);
            p.bar(630, y + 52, 150, 4, -red * 100 / 12, pal.meter, pal.track);
        }
        else if (r == 2) { fmt(v, sizeof v, "Speed %d.%d Hz", c.wowSpeed / 10, c.wowSpeed % 10); p.text(630, y + 23, v, F17, tc); }
        else if (r == 4)
        {
            p.text(630, y + 10, "Out", F12, pal.dim);
            p.bar(630, y + 30, 150, 4, model.master, pal.meter, pal.track);
            p.bar(630, y + 38, 150, 4, model.master, pal.meter, pal.track);
            p.rect(758, y + 26, 1, 20, pal.dim);
        }
    }
    if (byp)
    {
        p.rect(0, 400, 800, 40, pal.warn);
        p.text(14, 409, "BYPASS", F17B, pal.bg);
        p.text(100, 411, "Dry mix. B1 again switches the lo-fi chain back on.", F14, pal.bg);
    }
    else p.hint("Up/Down choose. Jog changes. Insert switches the focused control on or off.");
    const char* l[8] = { "Bypass all", "", "", "", "", "", "", "Back" };
    p.buttons(l, byp ? 1 : 0);
}

//--------------------------------------------------------------------------------------------
// 4.7 Input setup
void App::drawInput(Pen& p)
{
    heading(p, model, "Input setup");
    const int sel = session.selected();
    const Source s = trackSource[sel];
    p.text(14, 66, "Source", F12, pal.dim);
    const char* srcLabel[3] = { "Line", "Mic low", "Mic high" };
    for (int i = 0; i < 3; i++)
    {
        p.cell(1 + i * 100, 84, 97, 40, (int)s == i, pal.cell);
        p.textMid(1 + i * 100, 97, 95, srcLabel[i], F14, pal.text);
    }
    if (s == Source::Pads) p.text(330, 86, "Pads are recorded from the pad bus", F14, pal.text);
    else
    {
        const int gain = hal.gain(s);
        const int maxDb = s == Source::Line ? 22 : s == Source::MicLow ? 25 : 44;
        char b[24];
        fmt(b, sizeof b, "+%d dB", gain * maxDb / 100);
        p.text(330, 66, "Gain", F12, pal.dim);
        p.text(330, 86, b, F17B, pal.text);
        fmt(b, sizeof b, "0 to +%d dB", maxDb);
        p.text(330, 108, b, F12, pal.dim);
        p.bar(440, 96, 340, 6, gain, pal.text, pal.track);
    }
    float l = 0, r = 0;
    hal.inputLevels(l, r);
    const float m = l > r ? l : r;
    int db = m > 0.0004f ? ampToDb(m) : -60;
    if (db < -48) db = -48;
    const int lx = 14 + (db + 48) * 772 / 48;
    static const int ticks[5] = { -48, -24, -18, -6, 0 };
    p.text(14, 142, "Input level", F12, pal.dim);
    p.text(503, 142, "aim between -18 and -6", F12, pal.dim);
    p.rect(14, 160, 772, 16, pal.track);
    p.rect(14, 160, lx - 14, 16, pal.meter);
    if (db > inputPeakDb) inputPeakDb = db; else if (inputPeakDb > -48) inputPeakDb--;
    const int pk = inputPeakDb;
    p.rect(14 + (pk + 48) * 772 / 48 - 2, 160, 2, 16, pal.text);
    if (set.waitSound)
    {
        const int wx = 14 + ((int)set.waitDb + 48) * 772 / 48;
        p.rect(wx, 156, 2, 24, pal.warn);
        char wb[40];
        fmt(wb, sizeof wb, "Wait for sound: starts at %d dB (Shift+jog changes it)", (int)set.waitDb);
        p.text(14, 198, wb, F12, pal.warn);
    }
    for (int i = 0; i < 5; i++)
    {
        const int x = 14 + (ticks[i] + 48) * 772 / 48;
        if (ticks[i] == -18 || ticks[i] == -6) p.rect(x, 156, 1, 24, pal.dim);
        char t[8];
        fmt(t, sizeof t, "%d", ticks[i]);
        p.text(i == 4 ? 776 : x, 182, t, F12, pal.dim);
    }
    p.rect(0, 213, 800, 1, pal.frame);
    p.text(14, 228, "Monitoring", F12, pal.dim);
    p.text(14, 246, monitorFor(s) ? "On" : "Off", F17B, pal.text);
    p.text(14, 272, "Line starts with monitoring on.", F12, pal.dim);
    p.text(14, 288, "Both mic sources start with it off.", F12, pal.dim);
    const int red = hal.monitorReductionDb();
    p.text(400, 228, "Feedback protection", F12, pal.dim);
    p.text(400, 246, red < 0 ? "Acting" : "Standing by", F17B, red < 0 ? pal.warn : pal.text);
    p.text(400, 272, "Always on while monitoring is on. No off switch.", F12, pal.dim);
    p.text(400, 288, "It turns down only what you hear, never the recording.", F12, pal.dim);
    p.text(400, 318, "Monitor turned down by", F12, pal.dim);
    p.rect(400, 340, 240, 6, pal.track);
    if (red < 0) p.rect(400, 340, -red * 240 / 24, 6, pal.warn);
    char b[16];
    fmt(b, sizeof b, "%d dB", red);
    p.text(656, 332, b, F17, pal.text);
    const char* lab[8] = { "Line", "Mic low", "Mic high", "Monitor", "Pads", "", "", "Done" };
    p.buttons(lab, (uint8_t)((s == Source::Pads ? 16 : (1u << (int)s)) | (monitorFor(s) ? 8 : 0)));
}

//--------------------------------------------------------------------------------------------
// 4.8 Project
void App::drawProject(Pen& p)
{
    heading(p, model, "Project", 38);
    p.textRight(786, 41, "SD / FourTrack / Projects", F12, pal.dim);
    p.text(24, 64, "Name", F12, pal.dim);
    p.text(360, 64, "Bars", F12, pal.dim);
    p.text(440, 64, "Tempo", F12, pal.dim);
    const int perPage = 8;
    const int first = (listSel / perPage) * perPage;
    for (int i = first; i < listCount && i < first + perPage; i++)
    {
        const int y = 82 + (i - first) * 38;
        if (i == listSel) { p.rect(0, y, 800, 36, pal.sel); p.rect(0, y, 4, 36, pal.text); }
        p.text(24, y + 9, list[i], F14, pal.text);
        char b[12];
        if (listBars[i] > 0) { fmt(b, sizeof b, "%d", listBars[i]); p.text(360, y + 10, b, F14, pal.text); }
        if (listTempo[i] > 0) { fmt(b, sizeof b, "%u.%u", listTempo[i] / 10, listTempo[i] % 10); p.text(440, y + 10, b, F14, pal.text); }
        else if (listBars[i] > 0) p.text(440, y + 10, "Free", F14, pal.text);
    }
    if (listCount == 0) p.text(24, 90, "No saved projects yet. Save to make one.", F14, pal.dim);
    char b[64];
    const int pages = listCount > 0 ? (listCount + perPage - 1) / perPage : 1;
    fmt(b, sizeof b, "Page %d of %d. Up/Down choose, jog scrolls.", first / perPage + 1, pages);
    p.text(24, 400, b, F12, pal.dim);
    const char* l[8] = { "New", "Open", "Save", "Save as", "Rename", "Export", "Settings", "Back" };
    p.buttons(l);
}

// Rename and Save as: a row of character cells and a picker
void App::drawNameEntry(Pen& p)
{
    heading(p, model, nameForResample ? "Name the sample" : nameForSaveAs ? "Save project as" : "Rename project");
    p.text(14, 70, "Name", F12, pal.dim);
    char b[24];
    fmt(b, sizeof b, "%d of 16", nameCursor + 1);
    p.textRight(786, 70, b, F12, pal.dim);
    for (int i = 0; i < 16; i++)
    {
        const int x = 14 + i * 48;
        const bool focus = i == nameCursor;
        p.rect(x, 88, 44, 50, focus ? pal.sel : pal.panel);
        if (focus) p.rect(x, 88, 44, 2, pal.text);
        else p.rect(x, 136, 44, 2, pal.track);
        char ch[2] = { nameEdit[i], 0 };
        if (ch[0] != 0 && ch[0] != ' ') p.textMid(x, 44, 102, ch, F17B, pal.text);
    }
    p.text(14, 160, "Character", F12, pal.dim);
    static const char kPick[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_";
    const int n = (int)sizeof(kPick) - 1;
    int cur = 0;
    for (int i = 0; i < n; i++) if (kPick[i] == nameEdit[nameCursor]) cur = i;
    for (int j = 0; j < 15; j++)
    {
        const int x = 14 + j * 52;
        const bool focus = j == 5;
        p.rect(x, 178, 48, 36, focus ? pal.sel : pal.cell);
        if (focus) p.rect(x, 178, 48, 2, pal.text);
        char ch[2] = { kPick[((cur + j - 5) % n + n) % n], 0 };
        if (ch[0] == ' ') ch[0] = '_';
        p.textMid(x, 48, focus ? 187 : 187, ch, focus ? F17B : F14, focus ? pal.text : pal.dim);
    }
    p.text(14, 236, "Jog chooses the character. Left and Right move the cursor.", F14, pal.text);
    p.text(14, 256, "Up and Down jump between letters, digits and symbols.", F14, pal.text);
    p.rect(0, 297, 800, 1, pal.frame);
    p.text(14, 312, "Saves to", F12, pal.dim);
    char path[48];
    fmt(path, sizeof path, nameForResample ? "SD / Samples / Resamples / %s" : "SD / FourTrack / Projects / %s", nameEdit);
    p.text(14, 330, path, F14, pal.text);
    const char* l[8] = { "Delete", "Space", "", "", "", "", "Cancel", "Save" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// 4.9 Four-Track settings
void App::drawSettings(Pen& p)
{
    heading(p, model, "Four-Track settings", 38);
    char b[64];
    fmt(b, sizeof b, "This project - saved with %s", projectName);
    p.text(14, 68, b, F12, pal.dim);
    p.rect(0, 87, 800, 1, pal.frame);
    const char* names[13] = { "Loop Mode", "Click", "Count-in", "Wait for sound", "Grid", "Snap cuts to zero", "Alignment window", "Pre-roll", "Tempo rounding", "Wait level", "Input source", "Gain", "Monitoring default" };
    static const char* const countName[5] = { "Off", "1 beat", "2 beats", "4 beats", "8 beats" };
    char v[13][24];
    fmt(v[0], 24, "%s", set.loopOnce ? "Play once" : "Repeat");
    fmt(v[1], 24, "%s", set.click ? "On" : "Off");
    fmt(v[2], 24, "%s", countName[set.countIdx > 4 ? 4 : set.countIdx]);
    fmt(v[3], 24, "%s", set.waitSound ? "On" : "Off");
    fmt(v[4], 24, "%s", gridName(set.gridIdx));
    fmt(v[5], 24, "%s", set.snapZero ? "On" : "Off");
    fmt(v[6], 24, "%s", kWindowName[set.windowIdx]);
    fmt(v[7], 24, "%s", kPreRollName[set.preRollIdx]);
    fmt(v[8], 24, "%s", kRoundName[set.roundIdx]);
    fmt(v[9], 24, "%d dB", (int)set.waitDb);
    fmt(v[10], 24, "%s", kSourceName[(int)trackSource[session.selected()]]);
    const Source s = trackSource[session.selected()];
    if (s == Source::Pads) fmt(v[11], 24, "-");
    else fmt(v[11], 24, "+%d dB", hal.gain(s) * (s == Source::Line ? 22 : s == Source::MicLow ? 25 : 44) / 100);
    fmt(v[12], 24, "Line %s - Mic %s", set.monitorLine ? "on" : "off", set.monitorMic ? "on" : "off");
    for (int i = 0; i < 13; i++)
    {
        if (i == 6) { p.text(14, 90 + 6 * 22 + 2, "This device - every project", F12, pal.dim); p.rect(0, 90 + 6 * 22 + 21, 800, 1, pal.frame); }
        const int y = 90 + i * 22 + (i >= 6 ? 24 : 0);
        if (i == sRow) { p.rect(0, y - 4, 800, 22, pal.sel); p.rect(0, y - 4, 800, 2, pal.text); }
        p.text(24, y, names[i], F14, pal.text);
        p.text(330, y - 1, v[i], F17B, pal.text);
    }
    static const char* const help[13] = {
        "Repeat wraps a shifted Part round the Loop; Play once cuts what passes the end.",
        "A click plays with the loop (Shift + Play switches it too).",
        "How many clicks play before the first take. Free tempo counts at 120 BPM.",
        "The first take waits: Rec arms it and recording begins when the input is loud enough.",
        "The steps for moving a Part and cutting in the Part Editor.",
        "Cuts in the Part Editor land on the nearest zero crossing, so they do not click.",
        "A new Part whose first sound lands this close to the loop start is moved onto it.",
        "How long before the loop start a Part begins recording, so nothing is missed.",
        "Rounds the tempo worked out from the first take.",
        "How loud the input has to be to start a take that is waiting for sound.",
        "Where the selected track records from.",
        "The input gain for that source.",
        "Monitoring on or off when a source is chosen." };
    p.text(14, 412, help[sRow], F14, pal.text);
    p.textRight(786, 44, FT_BUILD_ID, F12, pal.dim); // the firmware and the emulator are built from the same sources and carry the same id
    const char* l[8] = { "", "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// 4.10 Export WAV (also shows saving and opening a project)
void App::drawExport(Pen& p)
{
    const bool saving = job == Job::Save, loading = job == Job::Load;
    heading(p, model, saving ? "Saving project" : loading ? "Opening project" : "Export WAV");
    const uint32_t rate = hal.sampleRate();
    const uint32_t loop = session.loopFrames();
    char b[64];
    if (job == Job::None)
    {
        p.text(14, 72, "Loop repeats", F12, pal.dim);
        p.textRight(786, 72, "Default 1", F12, pal.dim);
        for (int i = 0; i < 16; i++)
        {
            const int x = 14 + i * 48;
            const bool focus = i + 1 == exportRepeats;
            p.rect(x, 90, 44, 40, focus ? pal.sel : pal.cell);
            if (focus) p.rect(x, 90, 44, 2, pal.text);
            fmt(b, sizeof b, "%d", i + 1);
            p.textMid(x, 44, 101, b, focus ? F17B : F14, focus ? pal.text : pal.dim);
        }
        p.text(14, 152, "Length", F12, pal.dim);
        const uint32_t hund = (uint32_t)((uint64_t)loop * 100 / (rate ? rate : 1));
        const uint32_t tot = hund * (uint32_t)exportRepeats;
        fmt(b, sizeof b, "%d x %u.%02u s = %u.%u s", exportRepeats, hund / 100, hund % 100, tot / 100, (tot % 100) / 10);
        p.text(14, 170, b, F17B, pal.text);
        p.text(14, 210, "File", F12, pal.dim);
        fmt(b, sizeof b, "SD / FourTrack / Exports / %s.wav", projectName);
        p.text(14, 228, b, F14, pal.text);
        p.text(14, 270, "Jog chooses the repeats. B1 starts.", F12, pal.dim);
        p.text(14, 292, "Record mix (B2) records everything you hear, live, until you stop it. No length limit.", F12, pal.dim);
        const char* l[8] = { "Start", "Record mix", "", "", "", "", "", "Cancel" };
        p.buttons(l);
        return;
    }
    if (job == Job::MixRec || job == Job::MixFinish)
    {
        const uint32_t secs = rate ? mixFrames / rate : 0;
        if (job == Job::MixRec)
        {
            p.rect(14, 80, 10, 10, pal.rec);
            p.text(32, 76, "Recording the mix", F17B, pal.text);
            fmt(b, sizeof b, "%u:%02u", secs / 60, secs % 60);
            p.text(14, 112, b, F17B, pal.text);
            p.text(14, 150, "Play, jam and change things. Everything you hear is recorded.", F14, pal.dim);
            p.text(14, 172, "Stop ends it and saves the file.", F12, pal.dim);
            const char* l[8] = { "", "", "", "", "", "", "", "Stop" };
            p.buttons(l);
        }
        else
        {
            p.text(14, 80, "Saving the file", F17B, pal.text);
            fmt(b, sizeof b, "%d%%", exportPercent);
            p.text(14, 112, b, F14, pal.text);
            const char* l[8] = {};
            p.buttons(l);
        }
        return;
    }
    if (job == Job::Done)
    {
        p.rect(14, 80, 10, 10, pal.meter);
        p.text(32, 76, "Done", F17B, pal.text);
        if (mixFrames > 0 && mixCopied >= mixFrames * 4)
        {
            const uint32_t secs = rate ? mixFrames / rate : 0;
            p.text(14, 106, mixWav + 8, F14, pal.text);
            fmt(b, sizeof b, "SD / FourTrack / Exports - %u:%02u, stereo", secs / 60, secs % 60);
            p.text(14, 128, b, F12, pal.dim);
            const char* l2[8] = { "", "", "", "", "", "", "", "Close" };
            p.buttons(l2);
            return;
        }
        fmt(b, sizeof b, "%s.wav", projectName);
        p.text(14, 106, b, F14, pal.text);
        const uint32_t tot = (uint32_t)((uint64_t)loop * exportRepeats * 10 / (rate ? rate : 1));
        fmt(b, sizeof b, "SD / FourTrack / Exports - %u.%u s - %d repeats", tot / 10, tot % 10, exportRepeats);
        p.text(14, 128, b, F12, pal.dim);
        const char* l[8] = { "", "", "", "", "", "", "", "Close" };
        p.buttons(l);
        return;
    }
    fmt(b, sizeof b, "%s %s", job == Job::Export ? "Writing" : (saving ? "Saving" : "Opening"), projectName);
    p.text(14, 80, b, F14, pal.text);
    if (job == Job::Export)
    {
        const uint32_t tot = (uint32_t)((uint64_t)loop * exportRepeats * 10 / (rate ? rate : 1));
        fmt(b, sizeof b, "%d repeats - %u.%u s", exportRepeats, tot / 10, tot % 10);
        p.text(14, 100, b, F12, pal.dim);
    }
    p.bar(14, 132, 680, 10, exportPercent, pal.text, pal.track);
    fmt(b, sizeof b, "%d%%", exportPercent);
    p.text(710, 126, b, F17B, pal.text);
    const char* l[8] = { "", "", "", "", "", "", "", "Cancel" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// 4.11 Pads
void App::drawPads(Pen& p)
{
    heading(p, model, "Pads");
    const int slot = hal.currentSample();
    char name[32] = "No sample";
    const bool has = hal.sampleName(slot, name, sizeof name);
    p.rect(0, 70, 800, 60, pal.sel);
    p.rect(0, 70, 800, 2, pal.text);
    p.text(14, 80, "Sample on the pads", F12, pal.dim);
    p.text(14, 98, has ? name : "No sample", F17B, has ? pal.text : pal.dim);
    p.rect(0, 132, 800, 60, pal.panel);
    p.text(14, 142, "Pad mode", F12, pal.dim);
    const bool keys = padsKeysMode;
    p.text(14, 160, keys ? "Keys" : "Pad", F17B, pal.text);
    p.text(100, 163, keys ? "One sample across the 48 pads at different pitches. Select it with Up and Down." : "Pad N plays sample N at its own pitch. Select the sample with Up and Down.", F12, pal.dim);
    uint32_t used = 0, total = 0;
    hal.sampleMemory(used, total);
    char b[48];
    p.text(14, 214, "Samples loaded", F12, pal.dim);
    fmt(b, sizeof b, "%d", hal.sampleCount());
    p.text(14, 232, b, F17B, pal.text);
    p.text(200, 214, "Memory used", F12, pal.dim);
    fmt(b, sizeof b, "%u.%u of %u.%u MB", used / 1000000, (used % 1000000) / 100000, total / 1000000, (total % 1000000) / 100000);
    p.text(200, 232, b, F17B, pal.text);
    p.bar(14, 266, 772, 6, total ? (int)((uint64_t)used * 100 / total) : 0, pal.text, pal.track);
    p.text(14, 290, "MIDI controllers play the same sounds as the pads.", F12, pal.dim);
    const char* l[8] = { "Keys", "Pad", "", "", "", "", "", "Back" };
    p.buttons(l, keys ? 1 : 2);
}

//--------------------------------------------------------------------------------------------
// 4.12 Memory
void App::drawMemory(Pen& p)
{
    heading(p, model, "Memory");
    char b[64];
    p.text(14, 76, "Recording time left for the next Part", F12, pal.dim);
    const uint32_t secs = hal.regionFrames() / hal.sampleRate();
    fmt(b, sizeof b, "%u.0 s", (unsigned)(secs > kMaxTakeSeconds ? kMaxTakeSeconds : secs));
    p.text(14, 94, b, F17B, pal.text);
    p.text(14, 120, "The limit for one Part.", F12, pal.dim);
    p.text(420, 76, "Samples loaded", F12, pal.dim);
    fmt(b, sizeof b, "%d", hal.sampleCount());
    p.text(420, 94, b, F17B, pal.text);
    uint32_t used = 0, total = 0;
    hal.sampleMemory(used, total);
    p.text(14, 166, "Memory used", F12, pal.dim);
    p.bar(14, 186, 772, 14, total ? (int)((uint64_t)used * 100 / total) : 0, pal.text, pal.track);
    fmt(b, sizeof b, "%u.%u of %u.%u MB", used / 1000000, (used % 1000000) / 100000, total / 1000000, (total % 1000000) / 100000);
    p.text(14, 208, b, F14, pal.text);
    const char* l[8] = { "", "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

//--------------------------------------------------------------------------------------------
// 4.13 Popups and notices: a box over whatever is on the screen
void App::drawPopup(Pen& p)
{
    const char* head = "";
    const char* line1 = "";
    const char* line2 = "";
    const char* b7 = "";
    const char* b8 = "OK";
    bool red = false;
    char l1[48];
    switch (popupKind)
    {
        case Popup::Replace:
            fmt(l1, sizeof l1, "Track %d already has a Part.", session.selected() + 1);
            head = "Replace Part?"; line1 = l1; line2 = "Recording replaces it from the next loop start. Shift + Stop brings it back.";
            b7 = "Cancel"; b8 = "Replace"; red = true; break;
        case Popup::NoSound:
            fmt(l1, sizeof l1, "The take on Track %d was silent.", popupArg > 0 ? popupArg : session.selected() + 1);
            head = "No sound"; line1 = l1; line2 = "Check the source and gain in Input setup.";
            b7 = "Cancel"; b8 = "Retry"; break;
        case Popup::NotArmed: head = "Nothing to record"; line1 = "Record a first take, then choose a track."; break;
        case Popup::SdMissing:
            head = "SD card missing";
            line1 = current == Screen::Load ? "Samples cannot be listed or loaded." : "Put the card in to save, open or export.";
            line2 = current == Screen::Load ? "Insert the card and press Retry." : "";
            b7 = "Back"; b8 = "Retry"; break;
        case Popup::ReplaceSlot: case Popup::Copy: case Popup::Clear: case Popup::Busy: case Popup::Bounce: break; // drawn below
        case Popup::Unsaved: head = "Unsaved changes"; line1 = "This project has changes that are not saved."; line2 = "Save them first?"; b7 = "Don't save"; b8 = "Save"; break;
        case Popup::Message:
            head = popupArg == 3 ? "Cannot open" : popupArg == 4 ? "Nothing to export" : (popupArg == 1 || popupArg == 5) ? "Memory full" : popupArg == 2 ? "Nothing to undo" : popupArg == 6 ? "Cannot load" : popupArg == 7 ? "Nothing to resample" : popupArg == 8 ? "Nothing to copy or undo" : popupArg == 9 ? "Nothing to clear" : popupArg == 10 ? "Cannot double" : popupArg == 11 ? "Too long to double" : "Notice";
            line1 = popupArg == 3 ? "This file is not a Four-Track project." : popupArg == 4 ? "Record something first." : popupArg == 1 ? "No room for another Part." : popupArg == 2 ? "There is no earlier take on this track." : popupArg == 5 ? "This sample does not fit in the sample memory." : popupArg == 6 ? "Only 8, 16 and 24 bit PCM WAV files load." : popupArg == 7 ? "Record a Part first." : popupArg == 8 ? "Select a track with a Part. Shift + Copy undoes the last copy or clear." : popupArg == 9 ? "There is no Part to clear." : popupArg == 10 ? "A loop of whole bars is needed, with nothing recording." : popupArg == 11 ? "Doubled it would be longer than 16 s." : popupArg == 12 ? "Bounce needs an empty track. Clear one first." : "";
            break;
        default: break;
    }
    if (popupKind == Popup::Busy)
    {
        p.rect(150, 157, 500, 122, pal.panel);
        p.frame(150, 157, 500, 122, pal.frame);
        p.rect(150, 157, 500, 2, pal.text);
        p.text(174, 177, "Copying", F17B, pal.text);
        char t[60];
        fmt(t, sizeof t, "Keeping the old Part on Track %d so Shift + Copy can bring it back.", bkDst + 1);
        p.text(174, 213, t, F14, pal.text);
        p.rect(174, 240, 452, 8, pal.track);
        p.rect(174, 240, 452 * popupArg / 100, 8, pal.text);
        fmt(t, sizeof t, "%d%%", popupArg);
        p.textRight(626, 256, t, F12, pal.dim);
        const char* none[8] = {};
        p.buttons(none);
        return;
    }
    if (popupKind == Popup::Copy)
    {
        const int src = session.selected();
        p.rect(150, 104, 500, 236, pal.panel);
        p.frame(150, 104, 500, 236, pal.frame);
        p.rect(150, 104, 500, 2, pal.text);
        char t[40];
        fmt(t, sizeof t, "Copy Track %d to ...", src + 1);
        p.text(174, 125, t, F17B, pal.text);
        for (int i = 0; i < 4; i++)
        {
            const int x = 174 + i * 114;
            const bool source = i == src;
            p.cell(x, 160, 110, 62, !source && i == copyTarget, pal.cell);
            fmt(t, sizeof t, "%d %s", i + 1, model.lane[i].name);
            p.text(x + 10, 171, t, F14B, source ? pal.dim : pal.text);
            p.text(x + 10, 195, source ? "Source" : (session.hasPart(i) ? "Has a Part" : "Empty"), F12, pal.dim);
        }
        if (session.hasPart(copyTarget))
        {
            const int w = p.width("REPLACES", F12) + 6;
            p.rect(174, 242, w, 18, pal.warn);
            p.text(177, 244, "REPLACES", F12, pal.bg);
            fmt(t, sizeof t, "Track %d has a Part. Copying replaces it.", copyTarget + 1);
            p.text(250, 243, t, F14, pal.warn);
        }
        p.text(174, 271, "The copy keeps trims, fades and Offset.", F12, pal.dim);
        p.text(174, 289, "Level, pan and effect slots stay with the target track.", F12, pal.dim);
        p.text(174, 307, "Shift + Copy undoes it. Bounce mixes tracks onto an empty one.", F12, pal.dim);
        const char* lc[8] = { "Track 1", "Track 2", "Track 3", "Track 4", "Bounce", "", "Cancel", "Copy" };
        p.buttons(lc, (uint8_t)(1u << copyTarget), -1, (uint8_t)(1u << src));
        return;
    }
    if (popupKind == Popup::Bounce)
    {
        p.rect(150, 104, 500, 236, pal.panel);
        p.frame(150, 104, 500, 236, pal.frame);
        p.rect(150, 104, 500, 2, pal.text);
        char t[48];
        fmt(t, sizeof t, "Bounce onto Track %d", bounceTarget + 1);
        p.text(174, 125, t, F17B, pal.text);
        for (int i = 0; i < 4; i++)
        {
            const int x = 174 + i * 114;
            const bool tgt = i == bounceTarget;
            p.cell(x, 160, 110, 62, ((bounceSel >> i) & 1) != 0, pal.cell);
            fmt(t, sizeof t, "%d %s", i + 1, model.lane[i].name);
            p.text(x + 10, 171, t, F14B, tgt || !session.hasPart(i) ? pal.dim : pal.text);
            p.text(x + 10, 195, tgt ? "Target" : !session.hasPart(i) ? "Empty" : (((bounceSel >> i) & 1) ? "Mixed in" : "Left out"), F12, pal.dim);
        }
        p.text(174, 240, "Tracks 1 to 4 pick what is mixed. Level, mute, trims, fades and", F12, pal.dim);
        p.text(174, 258, "Offset are baked in. The result is mono; sends and tone are not.", F12, pal.dim);
        p.text(174, 280, bounceClear ? "The mixed tracks are cleared afterwards." : "The mixed tracks stay as they are.", F14, pal.text);
        p.text(174, 305, "Shift + Copy undoes it.", F12, pal.dim);
        const char* lc[8] = { "1", "2", "3", "4", bounceClear ? "Clear: on" : "Clear: off", "", "Cancel", "Bounce" };
        p.buttons(lc, bounceSel, -1, bounceTarget >= 0 ? (uint8_t)(1u << bounceTarget) : 0);
        return;
    }
    if (popupKind == Popup::Clear)
    {
        p.rect(150, 144, 500, 148, pal.panel);
        p.frame(150, 144, 500, 148, pal.frame);
        p.rect(150, 144, 500, 2, pal.text);
        char t[60];
        const int sel = session.selected();
        if (clearAll) p.text(174, 165, "Clear all tracks?", F17B, pal.text);
        else { fmt(t, sizeof t, "Clear Track %d?", sel + 1); p.text(174, 165, t, F17B, pal.text); }
        const uint32_t rate = hal.sampleRate() ? hal.sampleRate() : 44100;
        const uint32_t hund = (uint32_t)((uint64_t)session.loopFrames() * 100 / rate);
        int parts = 0;
        for (int i = 0; i < 4; i++) if (session.hasPart(i)) parts++;
        if (clearAll) fmt(t, sizeof t, "%d Part%s, %u.%02u s each", parts, parts == 1 ? "" : "s", hund / 100, hund % 100);
        else fmt(t, sizeof t, "%s - %u.%02u s", model.lane[sel].name, hund / 100, hund % 100);
        p.text(174, 201, t, F14, pal.text);
        p.text(174, 221, "The Loop and tempo stay as they are.", F14, pal.text);
        if (clearAll)
        {
            p.text(174, 243, "Clear keeps the Loop, tempo, glitch and mixer settings. Reset all", F12, pal.dim);
            p.text(174, 259, "forgets those too and starts a new project. Shift + Copy undoes Clear.", F12, pal.dim);
        }
        else p.text(174, 251, "Shift + Copy brings it back. Shift + Delete clears all tracks.", F12, pal.dim);
        const char* lc[8] = { "", "", "", "", "", clearAll ? "Reset all" : "", "Cancel", "Clear" };
        p.buttons(lc, 0, 7);
        return;
    }
    if (popupKind == Popup::ReplaceSlot)
    {
        char n[28], a[48], b[56];
        int len = 0;
        hal.sampleName(ldSlot, n, sizeof n);
        fmt(a, sizeof a, "Slot %d has %s.", ldSlot + 1, n);
        char nm[28];
        for (; len < 27 && ldEntry[ldSel].name[len] != 0; len++) nm[len] = ldEntry[ldSel].name[len];
        nm[len] = 0;
        if (len > 4 && nm[len - 4] == '.') nm[len - 4] = 0;
        fmt(b, sizeof b, "%s takes its place on the pads.", nm);
        p.rect(150, 144, 500, 148, pal.panel);
        p.frame(150, 144, 500, 148, pal.frame);
        p.rect(150, 144, 500, 2, pal.text);
        char h[28];
        fmt(h, sizeof h, "Replace slot %d?", ldSlot + 1);
        p.text(174, 165, h, F17B, pal.text);
        p.text(174, 201, a, F14, pal.text);
        p.text(174, 221, b, F14, pal.text);
        p.text(174, 251, "Parts already recorded from the pads do not change.", F12, pal.dim);
        const char* l2[8] = { "", "", "", "", "", "", "Cancel", "Replace" };
        p.buttons(l2);
        return;
    }
    p.rect(150, 157, 500, 122, pal.panel);
    p.frame(150, 157, 500, 122, pal.frame);
    p.rect(150, 157, 500, 2, pal.text);
    int hx = 174;
    if (red) { p.dot(179, 188, 5, pal.rec); hx = 192; }
    p.text(hx, 177, head, F17B, pal.text);
    p.text(174, 213, line1, F14, pal.text);
    p.text(174, 233, line2, F14, pal.text);
    const char* l[8] = { "", "", "", "", "", popupKind == Popup::Unsaved ? "Cancel" : "", b7, b8 };
    p.buttons(l, 0, red ? 7 : -1);
}
//--------------------------------------------------------------------------------------------
// C10 Chords (Note button)
// The chord pad map as the design draws it: left hand columns 1 to 8, right hand 9 to 12
static int slen(const char* s) { int n = 0; while (s[n] != 0) n++; return n; }
static const int kPadX[12] = { 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729 };
static const int kPadY[4] = { 136, 200, 274, 330 };
static const int kPadH[4] = { 64, 64, 56, 56 };

void App::drawChordPad(Pen& p, int pad, int px, int py, int pw, int ph)
{
    const PadInfo pi = Chords::info(pad);
    const bool held = heldCount[pad] != 0;
    char name[24], out[8];
    chords.label(pad, name, sizeof name);
    const int len = slen(name);
    uint32_t fill = pal.panel, ink = pal.text;
    int font = F17B;
    bool stroke = false;
    uint32_t strokeColour = pal.dim;
    switch (pi.role)
    {
        case Role::Chord:
        {
            const uint32_t fam = pal.t[(int)chords.family(pad)];
            font = len >= 5 ? F14B : F17B;
            stroke = true;
            strokeColour = fam;
            if (held) { fill = fam; ink = pal.bg; stroke = false; }
            break;
        }
        case Role::Note:
        {
            const bool tone = chords.isChordTone(pad);
            font = tone ? F17B : F17;
            ink = tone ? pal.text : pal.dim;
            stroke = tone;
            strokeColour = lastFamily >= 0 ? pal.t[lastFamily] : pal.dim;
            if (held) { fill = pal.text; ink = pal.bg; stroke = false; }
            break;
        }
        case Role::Mod:
        {
            const bool latched = (latchedMods >> pi.index) & 1;
            const bool on = ((chords.mods >> pi.index) & 1) != 0;
            font = on ? F17B : F17;
            if (on) { fill = pal.sel; ink = latched ? pal.marker : pal.text; }
            break;
        }
        case Role::Style:
            font = chords.styleChosen(pi.index) ? F14B : F14;
            if (len > 6) font = F12;
            if (chords.styleChosen(pi.index)) { fill = pal.sel; ink = pal.marker; }
            break;
        case Role::Hold:
            font = F14;
            if (chHold) { fill = pal.marker; ink = pal.bg; font = F14B; }
            break;
        default:
            font = held ? F14B : F14;
            if (held) fill = pal.sel;
            break;
    }
    p.card(px, py, pw, ph, fill);
    int ty = py + ph / 2 - 9;
    if (stroke) ty = py + ph / 2 - 12;
    p.textMid(px, pw, ty, name, font, ink);
    if ((usedPads[session.selected() & 3] >> pad) & 1) p.rect(px + pw - 9, py + 4, 5, 5, held ? pal.bg : pal.marker);
    if (pi.role == Role::Chord)
    {
        chords.outsideName(pad, out, sizeof out);
        if (out[0] != 0) p.textMid(px, pw, py + ph - 20, out, F12, pal.dim);
        else if (stroke) p.rect(px + pw / 2 - 11, py + ph - 14, 22, 4, strokeColour);
    }
    else if (stroke) p.rect(px + pw / 2 - 11, py + ph - 12, 22, 4, strokeColour);
}

void App::drawChords(Pen& p)
{
    p.topBar(model);
    const ChordCfg& c = chords.cfg;
    char v[7][24];
    fmt(v[0], 24, "%s", Chords::keyName(c.key));
    fmt(v[1], 24, "%s", Chords::scaleName(c.scale));
    fmt(v[2], 24, "%s", Chords::styleName(c.style));
    fmt(v[3], 24, "%s%s", chSound < kSynthPresets ? synthPreset(chSound).name : "Sample", chSound < kSynthPresets && (sndMacros.any() || soundEdited()) ? " *" : "");
    fmt(v[4], 24, "%s", Chords::voicingName((int)c.voicing));
    fmt(v[5], 24, "%s%d", c.octave > 0 ? "+" : "", c.octave);
    fmt(v[6], 24, "%d%%", (int)c.humanise);
    static const char* const names[7] = { "Key", "Scale", "Style", "Sound", "Voicing", "Octave", "Humanise" };
    static const int widths[7] = { 64, 126, 106, 126, 106, 76, 90 };
    int x = 14;
    for (int i = 0; i < 7; i++)
    {
        p.cell(x, 40, widths[i], 46, i == chField, pal.panel);
        p.text(x + 10, 45, names[i], F12, pal.dim);
        p.text(x + 10, 61, v[i], F17B, pal.text);
        x += widths[i] + 6;
    }
    // what is sounding, in a sentence
    char b[100];
    if (lastChordName[0] != 0 && lastFamily >= 0)
    {
        const char* tonic = Chords::keyName(c.key);
        if (lastFamily == 2) fmt(b, sizeof b, "%s is a Tension chord: it wants to go home to %s.", lastChordName, tonic);
        else if (lastFamily == 1) fmt(b, sizeof b, "%s is an Away chord: it moves away from home (%s).", lastChordName, tonic);
        else fmt(b, sizeof b, "%s is a Home chord: it sits in %s.", lastChordName, tonic);
    }
    else fmt(b, sizeof b, "Colours: Home, Away and Tension chords. Hold a modifier on the right to change every chord.");
    p.text(14, 96, b, F14, pal.dim);
    // the numerals over the chord columns, in the chord family colours
    static const char* const roman[8] = { "I", "ii", "iii", "IV", "V", "vi", "vii", "I" };
    static const int famOf[7] = { 0, 1, 0, 1, 2, 0, 2 };
    for (int col = 0; col < 8; col++) p.textMid(kPadX[col], 57, 122, roman[col], F12, pal.t[famOf[col % 7]]);
    static const char* const rowName[4] = { "7ths", "Triads", "Notes", "Notes" };
    for (int r = 0; r < 4; r++) p.text(14, kPadY[r] + kPadH[r] / 2 - 6, rowName[r], F12, pal.dim);
    for (int pad = 0; pad < 48; pad++)
    {
        const int col = pad % 12, row = pad / 12;
        drawChordPad(p, pad, kPadX[col], kPadY[row], 57, kPadH[row] - 4);
    }
    p.rect(534, 136, 1, 250, pal.frame);
    p.text(14, 412, "Hold a modifier on the right (double-tap to latch). Shift + jog: arp rate or strum speed.", F14, pal.text);
    const char* l[8] = { padsChords ? "Pads: on" : "Pads: off", "Sets", "Reset", "Sound", "", "", "Play", "Back" };
    p.buttons(l, padsChords ? 1 : 0);
}

// 6.4 Play view: the selected Part's lane, the other tracks in one line, and the chord pads
void App::drawPlayView(Pen& p)
{
    static const int px[12] = { 14, 77, 139, 202, 264, 327, 389, 452, 536, 599, 661, 724 };
    static const int py[4] = { 172, 230, 298, 350 };
    static const int ph[4] = { 58, 58, 52, 52 };
    drawLayer(0, 0, model, pal, p.g); // top bar, status and the ruler, as on the home screen
    p.rect(780, 56, 10, 342, pal.bg);   // the home lane scroll bar does not belong here
    p.text(362, 7, model.pos, F17B, pal.topText);
    const int sel = session.selected();
    const Lane& L = model.lane[sel];
    const int ly = 58, lh = 80, wx = 170, ww = 600;
    p.card(14, ly, 772, lh, pal.panel);
    p.rect(14, ly + 8, 3, lh - 16, pal.text); // the selected track's edge, in text colour (track hues are muted here)
    char b[16];
    fmt(b, sizeof b, "%d", sel + 1);
    p.text(26, ly + 8, b, F17B, pal.dim);
    p.text(46, ly + 8, L.name, F17B, pal.text);
    if (L.tag[0] != 0) p.text(26, ly + 34, L.tag, F12, pal.dim);
    if (L.active && session.mode() == FourTrackSession::Mode::RecordingPart)
    {
        // what is being recorded, drawn as it comes in
        p.g.color(pal.rec);
        for (int i = 0; i < 120; i++)
        {
            if (liveWave[i] == 0) continue;
            const int h = 2 + liveWave[i] * (lh - 14) / 255;
            p.g.rect(wx + i * ww / 120, ly + lh / 2 - h / 2, ww / 120, h);
        }
    }
    else if (L.hasPart && L.waveCount > 0)
    {
        if (L.bitmap)
        {
            p.g.color(pal.text);
            p.g.mask(sel * 3, wx, ly, ww, lh);
        }
        else
        {
            p.g.color(pal.text);
            for (int i = 0; i < L.waveCount; i++)
            {
                const int h = 2 + L.wave[i] * (lh - 14) / 255;
                p.g.rect(wx + i * ww / L.waveCount, ly + lh / 2 - h / 2, ww / L.waveCount - 1, h);
            }
        }
    }
    else
    {
        p.text(wx + 8, ly + 14, L.msg1[0] != 0 ? L.msg1 : "Empty track", F17B, L.msg1[0] != 0 ? pal.text : pal.dim);
        if (L.msg2[0] != 0) p.text(wx + 8, ly + 44, L.msg2, F14, pal.text);
    }
    const int bars = model.barCount;
    for (int i = 1; i < bars && i < 16; i++) p.rect(wx + i * ww / bars, ly, 1, lh, pal.frame);
    if (model.loopFrames > 0)
    {
        const int hx = wx + (int)((uint64_t)model.posFrames * ww / model.loopFrames);
        p.g.color(0x000000);
        p.g.alpha(110);
        p.g.rect(hx, ly, wx + ww - hx, lh); // what the playhead has not reached is darker
        p.g.alpha(255);
        p.rect(hx, ly, 2, lh, pal.text);
    }
    if (L.muted) { p.g.color(0x000000); p.g.alpha(150); p.g.rect(wx, ly, ww, lh); p.g.alpha(255); }
    // the other tracks in one line, the chord at the right
    int x = 14;
    for (int t = 0; t < 4; t++)
    {
        if (t == sel) continue;
        const Lane& O = model.lane[t];
        p.card(x, 144, 124, 22, pal.panel);
        fmt(b, sizeof b, "%d", t + 1);
        p.text(x + 8, 148, b, F12, pal.dim);
        p.text(x + 24, 148, O.hasPart ? (O.tag[0] != 0 ? O.tag : "Part") : "empty", F12, O.hasPart ? pal.text : pal.dim);
        if (O.muted) { p.card(x + 98, 147, 20, 16, pal.sel); p.text(x + 103, 148, "M", F12, pal.text); }
        x += 130;
    }
    const TrackRole role = selectedRole();
    const bool roleMap = role == TrackRole::Bass || role == TrackRole::Melody || role == TrackRole::Drums;
    if (roleMap)
    {
        // the role's own pad map: row labels at the left, 57 px pads (design 6.5)
        static const int rx[12] = { 84, 141, 197, 254, 311, 367, 424, 481, 559, 616, 673, 729 };
        static const int ry[4] = { 172, 224, 276, 338 };
        char st[40];
        fmt(st, sizeof st, "%s - %s", roleName((int)role), roleSoundName(role, roleSound[(int)role]));
        p.text(590, 148, st, F12, pal.dim);
        static const char* const labels[3][4] = { { "HIGH", "MID", "LOW", "SAFE" }, { "HIGH", "MID", "LOW", "SAFE" }, { "HIP-HOP", "DANCE", "LIVE", "REPEAT" } };
        const int li = role == TrackRole::Bass ? 0 : role == TrackRole::Melody ? 1 : 2;
        for (int r = 0; r < 4; r++) p.text(14, ry[r] + 20, labels[li][r], F12, pal.dim);
        p.rect(534, 172, 1, 218, pal.frame);
        for (int pad = 0; pad < 48; pad++) drawRolePad(p, pad, rx[pad % 12], ry[pad / 12], 57, 52);
    }
    else
    {
        if (lastChordName[0] != 0 && lastFamily >= 0)
        {
            p.text(590, 142, lastChordName, F17B, pal.t[lastFamily]);
            p.text(670, 148, lastFamily == 2 ? "Tension" : lastFamily == 1 ? "Away" : "Home", F12, pal.dim);
        }
        else p.text(590, 148, "Play a chord", F12, pal.dim);
        p.rect(524, 172, 1, 230, pal.frame);
        for (int pad = 0; pad < 48; pad++) drawChordPad(p, pad, px[pad % 12], py[pad / 12], 63, ph[pad / 12]);
    }
    drawLayer(2, 0, model, pal, p.g);
    drawLayer(2, 1, model, pal, p.g);
}

void App::drawSets(Pen& p)
{
    heading(p, model, "Chord sets");
    for (int i = 0; i < 8; i++)
    {
        const int y = 66 + i * 41;
        const bool focus = i == setSel;
        p.rect(14, y, 372, 38, focus ? pal.sel : pal.panel);
        if (focus) p.rect(14, y, 372, 2, pal.text);
        char b[48];
        fmt(b, sizeof b, "%d", i + 1);
        p.text(24, y + 10, b, F14B, setLoaded == i ? pal.marker : pal.text);
        if (setUsed[i])
        {
            p.text(54, y + 5, setNames[i], F14B, setLoaded == i ? pal.marker : pal.text);
            char sm[64];
            setsSummary(setData[i], sm, sizeof sm);
            p.text(54, y + 22, sm, F12, pal.dim);
        }
        else p.text(54, y + 10, "Empty", F14, pal.dim);
    }
    p.text(414, 70, "Save stores", F12, pal.dim);
    char cur[64];
    uint8_t blob[138];
    packSet(blob);
    setsSummary(blob, cur, sizeof cur);
    p.text(414, 90, cur, F14B, pal.text);
    p.text(414, 120, "Key, scale, style, sound with its tweaks,", F14, pal.dim);
    p.text(414, 140, "voicing, octave, latched modifiers and play style.", F14, pal.dim);
    p.text(414, 170, "Recorded Parts are not stored.", F14, pal.dim);
    p.text(14, 412, "Jog or Up and Down choose a slot. B1 saves the pads' settings there, B2 loads it.", F14, pal.text);
    const char* l[8] = { "Save", "Load", "Name", "", "", "Clear", "", "Back" };
    p.buttons(l, 0, -1, (uint8_t)(setUsed[setSel] ? 0 : ((1u << 1) | (1u << 2) | (1u << 5))));
}

//--------------------------------------------------------------------------------------------
// C11 Sound (Instr button)
static uint32_t patchSig(const SynthPatch& q)
{
    auto f = [](float v) { return (uint32_t)(v * 1000.0f + 1000.0f); };
    uint32_t h = (uint32_t)q.osc * 7919u;
    h = h * 31u + f(q.attack); h = h * 31u + f(q.decay); h = h * 31u + f(q.sustain); h = h * 31u + f(q.release);
    h = h * 31u + f(q.shape); h = h * 31u + f(q.pulse);
    return h;
}

void App::soundArtSync()
{
    if (!hal.maskDefault()) return;
    if (!artStatic)
    {
        artStatic = true;
        uint8_t ring[kDialBytes];
        artDialRing(ring);
        hal.maskWrite(kArtDialSlot, 0, ring, kDialBytes);
        for (int i = 0; i < kSynthPresets; i++)
        {
            uint8_t c[kCycleBytes];
            artPatchCycle(synthPreset(i), c);
            hal.maskWrite(kArtCycleSlot + i, 0, c, kCycleBytes);
        }
        artSig = 0;
    }
    const SynthPatch now = synthApply(sndBase, sndMacros);
    const uint32_t sig = patchSig(now) | 1u;
    if (sig != artSig)
    {
        artSig = sig;
        uint8_t wave[kNoteBytes], sil[kNoteBytes];
        artNote(now, wave, sil);
        hal.maskWrite(kArtNoteSlot, 0, wave, kNoteBytes);
        hal.maskWrite(kArtSilSlot, 0, sil, kNoteBytes);
    }
}

// 6.2 Sound: twelve patch tiles, the one-note picture and eight dials
void App::drawSound(Pen& p)
{
    p.topBar(model);
    soundArtSync();
    const bool art = hal.maskDefault();
    const SynthPatch cur = synthApply(sndBase, sndMacros);
    // the patch tiles, 3 x 4
    for (int i = 0; i < kSynthPresets; i++)
    {
        const int x = 14 + (i % 3) * 128, y = 60 + (i / 3) * 84;
        const bool on = i == sndPreset;
        p.card(x, y, 124, 80, on ? pal.sel : pal.panel);
        if (on && sndField == 0) p.rect(x + 8, y, 108, 2, pal.text);
        p.text(x + 12, y + 10, synthPreset(i).kind, F12, pal.dim);
        p.text(x + 12, y + 25, synthPreset(i).name, F17B, pal.text);
        if (art)
        {
            p.g.color(on ? pal.text : pal.wave);
            p.g.mask(kArtCycleSlot + i, x + 30, y + 53, kCycleW, kCycleH);
        }
    }
    // what the patch is
    const int nx = p.text(414, 60, cur.name, F17B, pal.text);
    const bool tweaked = sndMacros.any() || soundEdited();
    int kx = p.text(nx + 10, 66, synthPreset(sndPreset).kind, F12, pal.dim);
    if (tweaked)
    {
        p.rect(kx + 12, 67, 6, 6, pal.marker);
        p.text(kx + 22, 66, soundEdited() ? "Edited" : "Tweaked", F12, pal.marker);
    }
    p.text(414, 84, cur.blurb, F14, pal.dim);
    // one note of it
    p.text(414, 112, "ONE NOTE", F12, pal.dim);
    p.text(414, 176, "press, hold, release", F12, pal.dim);
    if (art)
    {
        p.g.color(tweaked ? pal.marker : pal.text);
        p.g.alpha(41);
        p.g.mask(kArtSilSlot, 412, 122, kNoteW, kNoteH);
        p.g.alpha(255);
        p.g.mask(kArtNoteSlot, 412, 122, kNoteW, kNoteH);
        p.rect(414 + 376 * 55 / 100, 122, 1, kNoteH, pal.frame);
    }
    // the eight dials, 4 x 2
    for (int i = 0; i < kSynthMacros; i++)
    {
        const int x = 414 + (i % 4) * 94, y = 206 + (i / 4) * 98;
        const bool focus = sndField == i + 1;
        const int m = sndMacros.v[i];
        if (focus) { p.card(x, y - 6, 90, 92, pal.sel); p.rect(x + 8, y - 6, 74, 2, pal.text); }
        const int cx = x + 45, cy = y + 30;
        if (art)
        {
            p.g.color(pal.track);
            p.g.mask(kArtDialSlot, cx - 32, cy - 32, kDialPx, kDialPx);
        }
        if (m != 0)
        {
            // the moved part of the ring: dots from the top to the value
            p.g.color(pal.marker);
            const int a1 = artDialAngle(m), a0 = -90;
            const int step = m > 0 ? 6 : -6;
            for (int a = a0; m > 0 ? a <= a1 : a >= a1; a += step)
            {
                const float r = (float)a * 3.14159265f / 180.0f;
                p.g.dot(cx + ftRound(26.0f * cosf(r)), cy + ftRound(26.0f * sinf(r)), 2);
            }
        }
        // the pointer
        {
            const float r = (float)artDialAngle(m) * 3.14159265f / 180.0f;
            p.g.color(pal.text);
            p.g.line(cx + ftRound(17.0f * cosf(r)), cy + ftRound(17.0f * sinf(r)), cx + ftRound(29.0f * cosf(r)), cy + ftRound(29.0f * sinf(r)));
        }
        char v[8];
        if (m == 0) fmt(v, sizeof v, "0"); else fmt(v, sizeof v, "%s%d", m > 0 ? "+" : "-", m > 0 ? m : -m);
        p.textMid(x, 90, y + 19, v, F17B, m == 0 ? pal.text : pal.marker);
        p.textMid(x, 90, y + 60, synthMacroName(i), F14, pal.text);
        p.textMid(x, 90, y + 74, synthMacroHint(i), F12, pal.dim);
    }
    p.text(14, 412, "Jog changes the patch or knob. Hold B1 to hear it. B4 opens Humanise and the other feel settings.", F14, pal.text);
    const char* l[8] = { "Hear", "Knobs to 0", "Reset patch", "Humanise", "", "", "Advanced", "Back" };
    p.buttons(l, hearPad >= 0 ? 1 : 0);
}

void App::drawSoundAdv(Pen& p)
{
    heading(p, model, "Sound: every setting");
    const int nx = p.text(250, 42, synthPreset(sndPreset).name, F14, pal.dim);
    if (sndMacros.any()) p.text(nx + 10, 43, "(the knobs sit on top of these)", F12, pal.dim);
    const SynthPatch& orig = synthPreset(sndPreset);
    const int rows = kSynthParams / 2;
    for (int i = 0; i < kSynthParams; i++)
    {
        const int x = 14 + (i / rows) * 394, y = 64 + (i % rows) * 24;
        const bool focus = i == advField;
        p.cell(x, y, 386, 23, focus, pal.panel);
        const bool changed = synthParamGet(orig, i) != synthParamGet(sndBase, i);
        p.text(x + 10, y + 3, synthParamName(i), F14, pal.text);
        char v[24];
        synthParamText(sndBase, i, v, sizeof v);
        p.textRight(x + 376, y + 3, v, F14B, changed ? pal.marker : pal.text);
        p.rect(x + 160, y + 17, 100, 2, pal.track);
        p.rect(x + 160, y + 17, (int)(synthParamFraction(sndBase, i) * 100.0f), 2, pal.meter);
    }
    p.text(14, 412, "Jog changes the setting (Shift: finer). A changed setting is orange. B2 puts one back.", F14, pal.text);
    const char* l[8] = { "Hear", "Reset one", "Reset all", "", "", "", "", "Back" };
    p.buttons(l, hearPad >= 0 ? 1 : 0);
}

//--------------------------------------------------------------------------------------------
// Input latency test
void App::drawCalibrate(Pen& p)
{
    heading(p, model, "Calibrate input latency");
    char b[100];
    const uint32_t rate = hal.sampleRate() ? hal.sampleRate() : 44100;
    const char* l[8] = { "", "", "", "", "", "", "", "" };
    switch (calState)
    {
        case CalState::Ready:
        {
            p.text(14, 70, "How late does audio from the line input arrive? This measures it,", F14, pal.text);
            p.text(14, 90, "and then line and mic Parts are moved back by that much.", F14, pal.text);
            p.text(14, 128, "1", F17B, pal.dim);
            p.text(44, 128, "Connect a cable from the Tracker's output to its line input.", F17, pal.text);
            p.text(14, 162, "2", F17B, pal.dim);
            p.text(44, 162, "Turn the volume to about 40. The test plays short clicks.", F17, pal.text);
            p.text(14, 196, "3", F17B, pal.dim);
            p.text(44, 196, "Press Start. It takes about 4 seconds.", F17, pal.text);
            const int t10 = (int)((int64_t)latencyFrames * 10000 / (int64_t)rate);
            fmt(b, sizeof b, "Now: %d.%d ms", t10 / 10, t10 % 10);
            p.text(14, 240, b, F17B, pal.text);
            if (!calibrateAllowed()) p.text(14, 280, "Clear all Parts first (Shift + Delete), then come back. The test uses Track 4.", F14, pal.warn);
            l[6] = "Cancel";
            l[7] = "Start";
            p.buttons(l, 0, -1, calibrateAllowed() ? 0 : (uint8_t)(1u << 7));
            break;
        }
        case CalState::Running:
            p.text(14, 76, "Measuring. Please wait and do not touch anything.", F17B, pal.text);
            p.bar(14, 112, 680, 10, calPercent, pal.meter, pal.track);
            fmt(b, sizeof b, "%d%%", calPercent);
            p.textRight(786, 107, b, F17, pal.text);
            p.text(14, 150, "The clicks go out of the output and come back in through the cable.", F14, pal.dim);
            l[6] = "Cancel";
            p.buttons(l);
            break;
        case CalState::Done:
        {
            const int t10 = (int)((int64_t)calResult.frames * 10000 / (int64_t)rate);
            fmt(b, sizeof b, "%d.%d ms", t10 / 10, t10 % 10);
            p.text(14, 76, "Input latency", F12, pal.dim);
            p.text(14, 96, b, F17B, pal.text);
            fmt(b, sizeof b, "%d frames, %d clicks heard, spread %d frames", (int)calResult.frames, calResult.count, (int)calResult.spread);
            p.text(14, 130, b, F14, pal.dim);
            p.text(14, 170, "Keep it and line and mic Parts will be moved earlier by this much when they are recorded.", F14, pal.text);
            p.text(14, 194, "Pad and synth Parts have no input delay and are not moved.", F14, pal.text);
            l[6] = "Retry";
            l[7] = "Keep";
            p.buttons(l);
            break;
        }
        case CalState::Failed:
        {
            const char* why = calWhy == 1 ? "Heard nothing. Is the cable in the line input, and the volume up?"
                            : calWhy == 2 ? "The clicks did not agree. Check the cable and try again."
                            : calWhy == 3 ? "The result was out of range (over 90 ms). Check the cable."
                            : calWhy == 4 ? "Too few clicks were heard. Turn the volume up a little and try again."
                            : calWhy == 5 ? "This build cannot run the test."
                                          : "The recording did not start. Try again.";
            p.text(14, 76, "The test did not work", F17B, pal.warn);
            p.text(14, 112, why, F14, pal.text);
            l[6] = "Back";
            l[7] = "Retry";
            p.buttons(l);
            break;
        }
    }
}
} // namespace ft
