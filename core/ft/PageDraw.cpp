// Drawing of the pages (every screen except the main one) into a Canvas, laid out exactly as
// in the design (docs/design, frames 4.2 to 4.13). Coordinates are the design's.
#include "App.h"
#include "Pen.h"
#include "loop_fill.h"

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
    p.cell(516, 270, 270, 58, set_ && sRow == 1, pal.cell);
    p.text(528, 278, "Count-in", F12, pal.dim);
    p.text(528, 296, set.countIn ? "On" : "Off", F17B, pal.text);
    p.text(570, 299, "four clicks", F12, pal.dim);
    p.text(14, 344, "Free: the first take sets the Loop, and the tempo is worked out from its length.", F12, pal.dim);
    p.text(14, 362, "Set: four clicks of count-in play, then the first take lands on the grid.", F12, pal.dim);
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
    uint8_t h[150];
    for (int i = 0; i < 150; i++) h[i] = (uint8_t)(2 + editWave[i] * 170 / 255);
    p.g.color(pal.wave);
    p.g.bars(25, 164, 5, 4, h, 150);
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
    p.text(14, 246, set.monitorLine ? "On" : "Off", F17B, pal.text);
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
    p.buttons(lab, (uint8_t)((s == Source::Pads ? 16 : (1u << (int)s)) | (set.monitorLine ? 8 : 0)));
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
    const char* l[8] = { "New", "Open", "Save", "Save as", "Rename", "Export", "", "Back" };
    p.buttons(l);
}

// Rename and Save as: a row of character cells and a picker
void App::drawNameEntry(Pen& p)
{
    heading(p, model, nameForSaveAs ? "Save project as" : "Rename project");
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
    fmt(path, sizeof path, "SD / FourTrack / Projects / %s", nameEdit);
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
    const char* names[10] = { "Loop Mode", "Click", "Grid", "Snap cuts to zero", "Alignment window", "Pre-roll", "Tempo rounding", "Input source", "Gain", "Monitoring default" };
    char v[10][24];
    fmt(v[0], 24, "%s", set.loopOnce ? "Play once" : "Repeat");
    fmt(v[1], 24, "%s", set.click ? "On" : "Off");
    fmt(v[2], 24, "%s", gridName(set.gridIdx));
    fmt(v[3], 24, "%s", set.snapZero ? "On" : "Off");
    fmt(v[4], 24, "%s", kWindowName[set.windowIdx]);
    fmt(v[5], 24, "%s", kPreRollName[set.preRollIdx]);
    fmt(v[6], 24, "%s", kRoundName[set.roundIdx]);
    fmt(v[7], 24, "%s", kSourceName[(int)trackSource[session.selected()]]);
    const Source s = trackSource[session.selected()];
    if (s == Source::Pads) fmt(v[8], 24, "-");
    else fmt(v[8], 24, "+%d dB", hal.gain(s) * (s == Source::Line ? 22 : s == Source::MicLow ? 25 : 44) / 100);
    fmt(v[9], 24, "%s", set.monitorLine ? "Line on - Mic off" : "Off");
    for (int i = 0; i < 10; i++)
    {
        if (i == 4) { p.text(14, 212, "This device - every project", F12, pal.dim); p.rect(0, 231, 800, 1, pal.frame); }
        const int y = 90 + i * 28 + (i >= 4 ? 28 : 0);
        if (i == sRow) { p.rect(0, y - 6, 800, 28, pal.sel); p.rect(0, y - 6, 800, 2, pal.text); }
        p.text(24, y, names[i], F14, pal.text);
        p.text(330, y - 1, v[i], F17B, pal.text);
    }
    static const char* const help[10] = {
        "Repeat wraps a shifted Part round the Loop; Play once cuts what passes the end.",
        "A click plays with the loop, and a count-in plays before the first take.",
        "The steps for moving a Part and cutting in the Part Editor.",
        "Cuts in the Part Editor land on the nearest zero crossing, so they do not click.",
        "A new Part whose first sound lands this close to the loop start is moved onto it.",
        "How long before the loop start a Part begins recording, so nothing is missed.",
        "Rounds the tempo worked out from the first take.",
        "Where the selected track records from.",
        "The input gain for that source.",
        "Monitoring on or off when a source is chosen." };
    p.text(14, 412, help[sRow], F14, pal.text);
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
        const char* l[8] = { "Start", "", "", "", "", "", "", "Cancel" };
        p.buttons(l);
        return;
    }
    if (job == Job::Done)
    {
        p.rect(14, 80, 10, 10, pal.meter);
        p.text(32, 76, "Done", F17B, pal.text);
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
    fmt(b, sizeof b, "%u.0 s", (unsigned)(secs > 24 ? 24 : secs));
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
            head = "Replace Part?"; line1 = l1; line2 = "Recording replaces it from the next loop start. Shift+Up brings it back.";
            b7 = "Cancel"; b8 = "Replace"; red = true; break;
        case Popup::NoSound:
            fmt(l1, sizeof l1, "The take on Track %d was silent.", popupArg > 0 ? popupArg : session.selected() + 1);
            head = "No sound"; line1 = l1; line2 = "Check the source and gain in Input setup.";
            b7 = "Cancel"; b8 = "Retry"; break;
        case Popup::NotArmed: head = "Nothing to record"; line1 = "Record a first take, then choose a track."; break;
        case Popup::SdMissing: head = "SD card missing"; line1 = "Put the card in to save, open or export."; b7 = "Back"; b8 = "Retry"; break;
        case Popup::Unsaved: head = "Unsaved changes"; line1 = "This project has changes that are not saved."; line2 = "Save them first?"; b7 = "Don't save"; b8 = "Save"; break;
        case Popup::Message:
            head = popupArg == 3 ? "Cannot open" : popupArg == 4 ? "Nothing to export" : popupArg == 1 ? "Memory full" : popupArg == 2 ? "Nothing to undo" : "Notice";
            line1 = popupArg == 3 ? "This file is not a Four-Track project." : popupArg == 4 ? "Record something first." : popupArg == 1 ? "No room for another Part." : popupArg == 2 ? "There is no earlier take on this track." : "";
            break;
        default: break;
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
} // namespace ft
