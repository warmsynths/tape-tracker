// Drawing of the pages (every screen except the main one) into a Canvas.
#include "App.h"
#include "Pen.h"

namespace ft
{
namespace
{
const char* const kWindowName[5] = { "Off", "30 ms", "60 ms", "100 ms", "150 ms" };
const char* const kPreRollName[5] = { "0 ms", "60 ms", "120 ms", "250 ms", "500 ms" };
const char* const kRoundName[3] = { "Off", "0.1 BPM", "1 BPM" };
const char* const kFxName[(int)FxType::kCount] = { "Empty", "Jump back", "Jump back 2", "Stutter", "Reverse", "Pitch", "Bit-crush", "Overdrive", "Pan", "Low-pass", "High-pass", "Volume" };
const char* const kSourceName[4] = { "Line in", "Mic low", "Mic high", "Pads" };

void fxValue(const FxSlot& s, char* out, size_t n)
{
    switch (s.type)
    {
        case FxType::Pitch: fmt(out, n, "%s%d st", s.semis < 0 ? "-" : "+", s.semis < 0 ? -s.semis : s.semis); break;
        case FxType::None: out[0] = 0; break;
        case FxType::JumpBack: case FxType::JumpBack2: fmt(out, n, "%d%%", s.amount); break;
        default: fmt(out, n, "%d%%", s.amount); break;
    }
}

void ms(char* out, size_t n, uint32_t frames, uint32_t rate)
{
    const uint32_t t = (uint32_t)((uint64_t)frames * 10000 / (rate ? rate : 1)); // tenths of ms
    fmt(out, n, "%u.%u ms", t / 10, t % 10);
}
} // namespace

void App::buildPage()
{
    page.clear();
    Pen p(page, pal);
    p.rect(0, 0, 800, 480, pal.bg);
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

static void title(Pen& p, const View& v, const char* name)
{
    p.topBar(v.project, v.unsaved, v.tempo, v.bars, v.pos, v.input, v.fb);
    p.text(12, 44, name, F17B, p.p.text);
    p.rect(12, 70, 776, 1, p.p.frame);
}

void App::drawTempo(Pen& p)
{
    title(p, model, "Starting tempo");
    char b[24];
    if (set.tempoTenths == 0) fmt(b, sizeof b, "Free");
    else fmt(b, sizeof b, "%u.%u BPM", set.tempoTenths / 10, set.tempoTenths % 10);
    p.rect(12, 90, 776, 70, sRow == 0 ? pal.sel : pal.panel);
    p.text(28, 98, "Tempo", F14, pal.dim);
    p.text(28, 122, b, F17B, pal.text);
    p.rect(12, 170, 776, 56, sRow == 1 ? pal.sel : pal.panel);
    p.text(28, 178, "Count-in", F14, pal.dim);
    p.text(28, 198, set.countIn ? "On: one bar of clicks before the first take" : "Off", F14, pal.text);
    p.text(12, 250, "With a tempo set, the first take is a whole number of bars and the click can play.", F14, pal.dim);
    p.text(12, 272, "Free: record first, then say how many bars it was.", F14, pal.dim);
    const char* l[8] = { "Free", "", "", "", "", "", "", "OK" };
    p.buttons(l);
}

void App::drawEditor(Pen& p)
{
    title(p, model, "Part Editor");
    char b[32];
    fmt(b, sizeof b, "Track %d", editTrack + 1);
    p.textRight(788, 46, b, F14, pal.dim);
    // waveform
    if (editWaveDirty) const_cast<App*>(this)->computeEditWave();
    p.rect(12, 84, 776, 150, pal.waveBg);
    uint8_t h[100];
    for (int i = 0; i < 100; i++) h[i] = (uint8_t)(1 + editWave[i] * 70 / 255);
    p.g.color(pal.wave);
    p.g.bars(16, 159, 7, 5, h, 100);
    const TrackCfg& c = hal.bank().cfg[editTrack];
    const uint32_t loop = session.loopFrames();
    if (loop > 0)
    {
        const int x0 = 16 + (int)((uint64_t)c.trimStart * 700 / loop);
        const uint32_t end = c.trimEnd > loop ? loop : c.trimEnd;
        const int x1 = 16 + (int)((uint64_t)end * 700 / loop);
        p.rect(12, 84, x0 - 12 > 0 ? x0 - 12 : 0, 150, pal.bg);
        p.rect(x1, 84, 788 - x1 > 0 ? 788 - x1 : 0, 150, pal.bg);
        p.rect(x0, 84, 2, 150, pal.marker);
        p.rect(x1, 84, 2, 150, pal.marker);
    }
    // six fields
    const uint32_t rate = hal.sampleRate();
    const char* names[6] = { "Start", "End", "Fade in", "Fade out", "Level", "Offset" };
    char v[6][20];
    ms(v[0], 20, c.trimStart, rate);
    ms(v[1], 20, (c.trimEnd > loop ? loop : c.trimEnd), rate);
    ms(v[2], 20, c.fadeIn, rate);
    ms(v[3], 20, c.fadeOut, rate);
    fmt(v[4], 20, "%d dB", ampToDb(c.gain));
    const int32_t off = userOffset[editTrack];
    fmt(v[5], 20, "%s%d ms", off < 0 ? "-" : "+", (int)((off < 0 ? -off : off) * 1000LL / (int)rate));
    for (int i = 0; i < 6; i++)
    {
        const int x = 12 + (i % 3) * 262, y = 250 + (i / 3) * 84;
        p.rect(x, y, 252, 74, i == editFieldIdx ? pal.sel : pal.panel);
        p.text(x + 14, y + 10, names[i], F14, pal.dim);
        p.text(x + 14, y + 36, v[i], F17B, pal.text);
    }
    const char* l[8] = { auditioning ? "Stop solo" : "Solo", "Normalize", "Undo", "Redo", "", "", "Cancel", "Done" };
    p.buttons(l);
}

void App::drawGlitch(Pen& p)
{
    title(p, model, "Glitch");
    for (int t = 0; t < 4; t++)
    {
        const int x = 12 + t * 194;
        const TrackCfg& c = hal.bank().cfg[t];
        char b[16];
        fmt(b, sizeof b, "Track %d", t + 1);
        p.rect(x, 82, 186, 28, t == gTrack ? pal.sel : pal.panel);
        p.text(x + 10, 88, b, F14B, t == gTrack ? pal.text : pal.dim);
        for (int s = 0; s < 4; s++)
        {
            const FxSlot& sl = c.slot[s];
            const int y = 118 + s * 76;
            const bool cur = (t == gTrack && s == gSlot);
            p.rect(x, y, 186, 68, cur ? pal.sel : pal.panel);
            if (cur) p.frame(x, y, 186, 68, pal.text);
            char n[8];
            fmt(n, sizeof n, "B%d", s + 1);
            p.text(x + 10, y + 6, n, F12, pal.dim);
            p.text(x + 40, y + 6, kFxName[(int)sl.type], F14B, sl.type == FxType::None ? pal.dim : pal.text);
            char v[20];
            fxValue(sl, v, sizeof v);
            p.text(x + 10, y + 32, v, F14, pal.dim);
            if (sl.on) p.tag(x + 10, y + 46, "ON", true, pal.meter, pal.bg);
            if (c.held[s]) p.tag(x + 56, y + 46, "HOLD", true, pal.marker, pal.bg);
        }
    }
    p.text(12, 422, "Hold B1 to B4 to play a slot. Jog changes the amount, Shift+jog the effect, jog press keeps it on.", F12, pal.dim);
    const char* l[8] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Clear track", "Clear all", hal.bank().cfg[gTrack].granular ? "Pitch: grain" : "Pitch: tape", "Back" };
    p.buttons(l);
}

void App::drawMaster(Pen& p)
{
    title(p, model, "Master lo-fi");
    const LofiCfg& c = hal.lofi().cfg;
    p.tag(700, 44, c.bypass ? "BYPASS" : "ACTIVE", c.bypass, pal.warn, pal.bg);
    struct Row { const char* name; bool on; int a; const char* aName; int b; const char* bName; };
    char crush[12];
    fmt(crush, sizeof crush, "%d bit", c.crushBits);
    const char* names[5] = { "Glue compressor", "Tape saturation", "Wow and flutter", "Bit-crush", "Master volume" };
    for (int r = 0; r < 5; r++)
    {
        const int y = 84 + r * 66;
        p.rect(12, y, 776, 58, r == mRow ? pal.sel : pal.panel);
        const bool on = r == 0 ? c.glueOn : r == 1 ? c.satOn : r == 2 ? c.wowOn : r == 3 ? c.crushOn : true;
        p.text(28, y + 8, names[r], F14B, on ? pal.text : pal.dim);
        if (r < 4) p.tag(28, y + 32, on ? "ON" : "OFF", on, pal.meter, pal.bg);
        int pct = r == 0 ? c.glue : r == 1 ? c.sat : r == 2 ? c.wowDepth : r == 3 ? (16 - c.crushBits) * 100 / 12 : c.volume;
        p.bar(300, y + 22, 300, 12, pct, on ? pal.meter : pal.waveOff, pal.track);
        if (r == 3) p.text(620, y + 16, crush, F14, pal.text);
        else { char v[10]; fmt(v, sizeof v, "%d%%", pct); p.text(620, y + 16, v, F14, pal.text); }
        if (r == 2)
        {
            char sp[12];
            fmt(sp, sizeof sp, "Speed %d", c.wowSpeed);
            p.text(690, y + 16, sp, F14, mSub == 1 ? pal.text : pal.dim);
        }
    }
    const int red = hal.lofi().gainReductionDb();
    char g[20];
    fmt(g, sizeof g, "Gain reduction %d dB", red);
    p.text(12, 420, g, F12, pal.dim);
    const char* l[8] = { c.bypass ? "Use lo-fi" : "Bypass", "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

void App::drawInput(Pen& p)
{
    title(p, model, "Input setup");
    const int sel = session.selected();
    for (int t = 0; t < 4; t++)
    {
        const int x = 12 + t * 194;
        char b[16];
        fmt(b, sizeof b, "Track %d", t + 1);
        p.rect(x, 82, 186, 70, t == sel ? pal.sel : pal.panel);
        p.text(x + 10, 90, b, F14B, t == sel ? pal.text : pal.dim);
        p.text(x + 10, 118, kSourceName[(int)trackSource[t]], F14, pal.text);
    }
    const Source s = trackSource[sel];
    p.rect(12, 166, 776, 80, pal.panel);
    p.text(28, 176, "Gain", F14, pal.dim);
    if (s == Source::Pads) p.text(28, 206, "Pads are recorded straight from the pad bus", F14, pal.text);
    else
    {
        const int gain = hal.gain(s);
        p.bar(28, 210, 500, 14, gain, pal.meter, pal.track);
        char v[10];
        fmt(v, sizeof v, "%d%%", gain);
        p.text(548, 206, v, F14, pal.text);
    }
    float l = 0, r = 0;
    hal.inputLevels(l, r);
    p.rect(12, 258, 776, 100, pal.panel);
    p.text(28, 268, "Level", F14, pal.dim);
    p.text(28, 294, "L", F12, pal.dim);
    p.bar(48, 296, 600, 10, (int)(l * 100), l > 0.9f ? pal.rec : pal.meter, pal.track);
    p.text(28, 320, "R", F12, pal.dim);
    p.bar(48, 322, 600, 10, (int)(r * 100), r > 0.9f ? pal.rec : pal.meter, pal.track);
    p.rect(12, 370, 776, 56, pal.panel);
    p.text(28, 380, "Monitor", F14, pal.dim);
    p.text(28, 400, set.monitorLine ? "On: hear the input while recording" : "Off", F14, pal.text);
    const int red = hal.monitorReductionDb();
    if (red < 0)
    {
        char v[24];
        fmt(v, sizeof v, "Feedback protection %d dB", red);
        p.tag(560, 392, v, true, pal.warn, pal.bg);
    }
    const char* lab[8] = { "Line in", "Mic low", "Mic high", set.monitorLine ? "Monitor off" : "Monitor on", "Pads", "", "", "Back" };
    p.buttons(lab, (int)s == 0 ? 0 : (int)s == 1 ? 1 : (int)s == 2 ? 2 : 4);
}

void App::drawProject(Pen& p)
{
    title(p, model, "Project");
    if (listCount == 0) p.text(28, 100, "No saved projects yet", F14, pal.dim);
    for (int i = 0; i < listCount && i < 12; i++)
    {
        const int y = 82 + i * 28;
        p.rect(12, y, 776, 26, i == listSel ? pal.sel : pal.panel);
        p.text(24, y + 4, list[i], F14, i == listSel ? pal.text : pal.dim);
    }
    const char* l[8] = { "New", "Open", "Save", "Save as", "Rename", "Export WAV", "", "Back" };
    p.buttons(l);
}

void App::drawNameEntry(Pen& p)
{
    title(p, model, nameForSaveAs ? "Save project as" : "Rename project");
    p.rect(12, 100, 776, 70, pal.panel);
    int x = 28;
    for (int i = 0; i < 16; i++)
    {
        char ch[2] = { nameEdit[i] ? nameEdit[i] : ' ', 0 };
        if (i == nameCursor) p.rect(x - 2, 112, 28, 46, pal.sel);
        p.text(x + 4, 120, ch, F17B, pal.text);
        if (i == nameCursor) p.rect(x - 2, 156, 28, 2, pal.text);
        x += 30;
    }
    p.text(12, 200, "Jog changes the letter, left and right move, up and down switch letters, digits and symbols.", F14, pal.dim);
    const char* l[8] = { "Delete", "Space", "", "", "", "", "Cancel", "Save" };
    p.buttons(l);
}

void App::drawSettings(Pen& p)
{
    title(p, model, "Four-Track settings");
    const char* names[8] = { "Loop mode", "Click while playing", "Alignment window", "Pre-roll", "Tempo rounding", "Source of this track", "Input gain", "Monitor line" };
    char v[8][20];
    fmt(v[0], 20, "%s", set.loopOnce ? "Play once" : "Repeat");
    fmt(v[1], 20, "%s", set.click ? "On" : "Off");
    fmt(v[2], 20, "%s", kWindowName[set.windowIdx]);
    fmt(v[3], 20, "%s", kPreRollName[set.preRollIdx]);
    fmt(v[4], 20, "%s", kRoundName[set.roundIdx]);
    fmt(v[5], 20, "%s", kSourceName[(int)trackSource[session.selected()]]);
    const Source s = trackSource[session.selected()];
    if (s == Source::Pads) fmt(v[6], 20, "-"); else fmt(v[6], 20, "%d%%", hal.gain(s));
    fmt(v[7], 20, "%s", set.monitorLine ? "On" : "Off");
    p.text(12, 76, "This project", F12, pal.dim);
    for (int i = 0; i < 8; i++)
    {
        if (i == 2) p.text(12, 76 + 2 * 40 + 6 + 4, "This device", F12, pal.dim);
        const int y = 92 + i * 40 + (i >= 2 ? 22 : 0);
        p.rect(12, y, 776, 36, i == sRow ? pal.sel : pal.panel);
        p.text(24, y + 8, names[i], F14, pal.text);
        p.textRight(776, y + 8, v[i], F14B, pal.text);
    }
    const char* l[8] = { "", "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

void App::drawExport(Pen& p)
{
    const bool saving = job == Job::Save, loading = job == Job::Load;
    title(p, model, saving ? "Saving project" : loading ? "Opening project" : "Export WAV");
    if (job == Job::None)
    {
        p.text(12, 90, "Repeats of the loop in the file", F14, pal.dim);
        for (int i = 0; i < 16; i++)
        {
            const int x = 12 + (i % 8) * 97, y = 120 + (i / 8) * 64;
            char n[4];
            fmt(n, sizeof n, "%d", i + 1);
            p.rect(x, y, 91, 56, i + 1 == exportRepeats ? pal.sel : pal.panel);
            if (i + 1 == exportRepeats) p.frame(x, y, 91, 56, pal.text);
            p.textMid(x, 91, y + 16, n, F17B, i + 1 == exportRepeats ? pal.text : pal.dim);
        }
        char f[48];
        fmt(f, sizeof f, "Exports/%s.wav", projectName);
        p.text(12, 270, f, F14, pal.dim);
        const char* l[8] = { "Export", "", "", "", "", "", "", "Back" };
        p.buttons(l);
        return;
    }
    p.rect(12, 140, 776, 28, pal.track);
    p.rect(12, 140, 776 * exportPercent / 100, 28, pal.meter);
    char b[12];
    fmt(b, sizeof b, "%d%%", exportPercent);
    p.textMid(12, 776, 190, b, F17B, pal.text);
    if (job == Job::Done) p.textMid(12, 776, 230, "Done", F17B, pal.text);
    const char* l[8] = { "", "", "", "", "", "", "", job == Job::Done || job == Job::Export ? "Back" : "" };
    p.buttons(l);
}

void App::drawPads(Pen& p)
{
    title(p, model, "Pads");
    for (int i = 0; i < 12; i++)
    {
        const int idx = (padsSel / 12) * 12 + i;
        const int y = 80 + i * 28;
        char nm[24] = {};
        const bool has = hal.sampleName(idx, nm, sizeof nm);
        char row[40];
        fmt(row, sizeof row, "%02d  %s", idx + 1, has ? nm : "empty");
        p.rect(12, y, 776, 26, idx == padsSel ? pal.sel : pal.panel);
        p.text(24, y + 4, row, F14, has ? pal.text : pal.dim);
        p.textRight(776, y + 4, padModeKeys[idx] ? "Keys" : "Pad", F14, pal.dim);
    }
    const char* l[8] = { "Keys", "Pad", "", "", "", "", "", "Back" };
    p.buttons(l);
}

void App::drawMemory(Pen& p)
{
    title(p, model, "Memory");
    uint32_t used = 0, total = 0;
    hal.sampleMemory(used, total);
    char b[48];
    p.text(12, 90, "Sample memory", F14, pal.dim);
    p.bar(12, 116, 776, 20, total ? (int)((uint64_t)used * 100 / total) : 0, pal.meter, pal.track);
    fmt(b, sizeof b, "%u.%u of %u.%u MB used", used / 1000000, (used % 1000000) / 100000, total / 1000000, (total % 1000000) / 100000);
    p.text(12, 144, b, F14, pal.text);
    const uint32_t loop = session.loopFrames();
    const uint32_t cap = hal.regionFrames();
    p.text(12, 190, "Tracks", F14, pal.dim);
    for (int t = 0; t < 4; t++)
    {
        const int y = 216 + t * 40;
        fmt(b, sizeof b, "Track %d", t + 1);
        p.text(12, y + 6, b, F14, pal.text);
        p.bar(120, y + 8, 500, 14, session.hasPart(t) && cap ? (int)((uint64_t)loop * 100 / cap) : 0, pal.wave, pal.track);
        if (session.hasPart(t)) { fmt(b, sizeof b, "%u.%u s", loop / hal.sampleRate(), (loop * 10 / hal.sampleRate()) % 10); p.text(640, y + 6, b, F14, pal.dim); }
        else p.text(640, y + 6, "empty", F14, pal.dim);
    }
    fmt(b, sizeof b, "Each track holds up to %u seconds", cap / hal.sampleRate());
    p.text(12, 390, b, F14, pal.dim);
    const char* l[8] = { "", "", "", "", "", "", "", "Back" };
    p.buttons(l);
}

void App::drawPopup(Pen& p)
{
    const char* head = "";
    const char* body = "";
    const char* b7 = "";
    const char* b8 = "OK";
    const char* b6 = "";
    switch (popupKind)
    {
        case Popup::Replace: head = "Replace this take?"; body = "The old take is kept so you can undo it with Shift+Up."; b7 = "Cancel"; b8 = "Replace"; break;
        case Popup::NoSound: head = "No sound found"; body = "Nothing was heard on the input. Record again?"; b7 = "Cancel"; b8 = "Record again"; break;
        case Popup::NotArmed: head = "Nothing to record"; body = "Record a first take, then choose a track."; break;
        case Popup::SdMissing: head = "No SD card"; body = "Put the card in and try again."; b7 = "Cancel"; b8 = "Retry"; break;
        case Popup::Unsaved: head = "Save your changes?"; body = "This project has changes that are not saved."; b6 = "Back"; b7 = "Discard"; b8 = "Save"; break;
        case Popup::Message:
            head = popupArg == 3 ? "Cannot open" : popupArg == 4 ? "Nothing to export" : popupArg == 1 ? "Memory full" : "Notice";
            body = popupArg == 3 ? "This file is not a Four-Track project." : popupArg == 4 ? "Record something first." : popupArg == 1 ? "No room for another Part." : "";
            break;
        default: break;
    }
    p.rect(160, 150, 480, 160, pal.panel);
    p.frame(160, 150, 480, 160, pal.text);
    p.textMid(160, 480, 170, head, F17B, pal.text);
    p.textMid(160, 480, 214, body, F14, pal.dim);
    const char* l[8] = { "", "", "", "", "", b6, b7, b8 };
    p.buttons(l);
}
} // namespace ft
