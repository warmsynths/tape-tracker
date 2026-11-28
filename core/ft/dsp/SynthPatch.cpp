#include "SynthPatch.h"

#include "MiniFmt.h"

namespace ft
{
namespace
{
float exp2f_(float x)
{
    if (x < -30.0f) x = -30.0f;
    if (x > 30.0f) x = 30.0f;
    const int xi = (int)(x < 0 ? x - 1.0f : x);
    const float f = x - (float)xi;
    const float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    union { float f; uint32_t u; } bits;
    bits.u = (uint32_t)(xi + 127) << 23;
    return bits.f * p;
}
float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
int roundi(float x) { return (int)(x < 0 ? x - 0.5f : x + 0.5f); }

constexpr int kPresetCount = 12;

const SynthPatch kPresets[kPresetCount] = {
    // name, kind, blurb, osc, shape, pulse, detune, sub, noise, cutoff, res, envAmt, key,
    //   attack, decay, sustain, release, fAttack, fDecay, fSustain, fRelease,
    //   lfoRate, lfoPitch, lfoCutoff, lfoPulse, drive, ensemble, depth, glide, mono, level
    { "Warm Pad", "Pad", "Slow, soft pad with a gentle chorus.", SynthOsc::Shape, 0.50f, 0.50f, 7.0f, 0.20f, 0.00f, 0.38f, 0.10f, 1.0f, 0.5f,
      0.35f, 0.80f, 0.80f, 1.20f, 0.50f, 1.20f, 0.50f, 1.00f,
      0.40f, 0.04f, 0.10f, 0.20f, 0.00f, 0.50f, 0.40f, 0.0f, false, 0.7f },
    { "Juno Clean", "Pad", "Clean, wide saw pad with a bright chorus: synthwave.", SynthOsc::Saw, 0.50f, 0.50f, 9.0f, 0.25f, 0.00f, 0.62f, 0.08f, 0.8f, 0.6f,
      0.06f, 0.80f, 0.90f, 0.55f, 0.05f, 1.20f, 0.70f, 0.50f,
      0.50f, 0.00f, 0.02f, 0.00f, 0.00f, 0.60f, 0.40f, 0.0f, false, 0.7f },
    { "Music Box", "Bell", "Struck metal teeth that ring and fade. Shape moves from bar to bell.", SynthOsc::Modal, 0.62f, 0.35f, 0.0f, 0.00f, 0.00f, 0.85f, 0.00f, 0.0f, 0.6f,
      0.002f, 2.20f, 1.00f, 0.40f, 0.002f, 0.50f, 0.80f, 0.40f,
      0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.25f, 0.25f, 0.0f, false, 0.9f, 0.1f },
    { "Tape Choir", "Pad", "A hazy, detuned pad that drifts like an old tape: nostalgic.", SynthOsc::Shape, 0.30f, 0.50f, 16.0f, 0.15f, 0.05f, 0.30f, 0.10f, 1.2f, 0.4f,
      1.40f, 1.00f, 0.85f, 1.80f, 1.40f, 2.00f, 0.60f, 1.80f,
      0.30f, 0.08f, 0.12f, 0.00f, 0.00f, 0.50f, 0.50f, 0.0f, false, 0.8f, 1.0f },
    { "Dream Keys", "Keys", "Soft electric-piano keys with a long tail.", SynthOsc::Shape, 0.15f, 0.50f, 9.0f, 0.10f, 0.00f, 0.40f, 0.10f, 1.6f, 0.5f,
      0.01f, 0.90f, 0.35f, 0.80f, 0.02f, 0.60f, 0.20f, 0.60f,
      0.50f, 0.03f, 0.05f, 0.00f, 0.00f, 0.40f, 0.35f, 0.0f, false, 0.7f },
    { "Glass Pad", "Pad", "Bright, airy pad with a breath of noise.", SynthOsc::SuperSquare, 0.30f, 0.50f, 6.0f, 0.00f, 0.04f, 0.78f, 0.05f, 0.3f, 0.7f,
      0.50f, 0.80f, 0.90f, 1.40f, 0.30f, 1.00f, 0.80f, 1.20f,
      0.80f, 0.01f, 0.10f, 0.00f, 0.00f, 0.40f, 0.30f, 0.0f, false, 0.7f },
    { "FM Rhodes", "Keys", "A real two-operator FM electric piano: a bell-like strike and a soft body.", SynthOsc::Fm, 0.12f, 0.55f, 3.0f, 0.12f, 0.01f, 0.62f, 0.00f, 0.3f, 0.6f,
      0.003f, 1.40f, 0.25f, 0.50f, 0.003f, 0.55f, 0.15f, 0.40f,
      4.50f, 0.00f, 0.12f, 0.00f, 0.12f, 0.30f, 0.30f, 0.0f, false, 1.2f, 0.2f },
    { "Harp", "Pluck", "A plucked string: harp or nylon guitar. Decay is how long it rings.", SynthOsc::String, 0.55f, 0.50f, 0.0f, 0.00f, 0.00f, 0.80f, 0.00f, 0.0f, 0.5f,
      0.002f, 2.00f, 1.00f, 0.25f, 0.002f, 0.50f, 0.80f, 0.30f,
      0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.20f, 0.20f, 0.0f, false, 1.4f, 0.0f },
    { "Dark Moog Bass", "Bass", "Round, menacing mono bass with a quick filter snap.", SynthOsc::Shape, 0.75f, 0.50f, 0.0f, 0.50f, 0.00f, 0.26f, 0.22f, 2.0f, 0.3f,
      0.005f, 0.30f, 0.45f, 0.18f, 0.005f, 0.25f, 0.40f, 0.15f,
      0.00f, 0.00f, 0.00f, 0.00f, 0.25f, 0.00f, 0.50f, 0.03f, true, 1.9f, 0.15f },
    { "Sub Bass", "Bass", "Deep, round sub for the lowest notes.", SynthOsc::Shape, 0.00f, 0.50f, 0.0f, 0.90f, 0.00f, 0.25f, 0.00f, 0.5f, 0.2f,
      0.005f, 0.30f, 0.80f, 0.15f, 0.00f, 0.30f, 0.70f, 0.15f,
      0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.00f, 0.50f, 0.03f, true, 1.7f },
    { "Wobbly Lead", "Lead", "A warped, singing mono lead with a drunk vibrato.", SynthOsc::Shape, 0.45f, 0.50f, 7.0f, 0.00f, 0.01f, 0.50f, 0.12f, 0.8f, 0.5f,
      0.08f, 0.30f, 0.80f, 0.40f, 0.05f, 0.40f, 0.60f, 0.40f,
      5.00f, 0.22f, 0.05f, 0.00f, 0.08f, 0.25f, 0.25f, 0.09f, true, 1.9f, 0.8f },
    { "Poly Brass", "Brass", "Bright saw brass for big chord stabs: the synthwave hit.", SynthOsc::Saw, 0.50f, 0.50f, 9.0f, 0.15f, 0.00f, 0.30f, 0.05f, 2.4f, 0.5f,
      0.04f, 0.50f, 0.70f, 0.25f, 0.08f, 0.50f, 0.45f, 0.25f,
      0.30f, 0.00f, 0.00f, 0.00f, 0.05f, 0.35f, 0.30f, 0.0f, false, 1.2f, 0.1f }
};

const char* const kMacroNames[kSynthMacros] = { "Bright", "Reson", "Attack", "Release", "Thick", "Chorus", "Drive", "Motion" };

// The raw settings: a name, where it lives, its range, the step of one jog click, and how it shows
enum Unit : uint8_t { uPercent, uSeconds, uHertz, uCents, uSemitones, uOctaves, uOsc, uSwitch };
struct ParamDef { const char* name; float SynthPatch::*f; float lo, hi, step, fine; Unit unit; };
const ParamDef kParams[kSynthParams] = {
    { "Oscillator", nullptr, 0, 5, 1, 1, uOsc },
    { "Shape", &SynthPatch::shape, 0, 1, 0.05f, 0.01f, uPercent },
    { "Pulse", &SynthPatch::pulse, 0, 1, 0.05f, 0.01f, uPercent },
    { "Detune", &SynthPatch::detune, 0, 40, 1.0f, 0.25f, uCents },
    { "Sub oscillator", &SynthPatch::subLevel, 0, 1, 0.05f, 0.01f, uPercent },
    { "Noise", &SynthPatch::noise, 0, 1, 0.05f, 0.01f, uPercent },
    { "Cutoff", &SynthPatch::cutoff, 0, 1, 0.03f, 0.005f, uPercent },
    { "Resonance", &SynthPatch::resonance, 0, 0.95f, 0.05f, 0.01f, uPercent },
    { "Filter envelope", &SynthPatch::envAmount, -4, 4, 0.25f, 0.05f, uOctaves },
    { "Key tracking", &SynthPatch::keyTrack, 0, 1, 0.1f, 0.02f, uPercent },
    { "Attack", &SynthPatch::attack, 0.001f, 6, 1.25f, 1.05f, uSeconds },
    { "Decay", &SynthPatch::decay, 0.01f, 6, 1.25f, 1.05f, uSeconds },
    { "Sustain", &SynthPatch::sustain, 0, 1, 0.05f, 0.01f, uPercent },
    { "Release", &SynthPatch::release, 0.01f, 8, 1.25f, 1.05f, uSeconds },
    { "Filter attack", &SynthPatch::fAttack, 0.001f, 6, 1.25f, 1.05f, uSeconds },
    { "Filter decay", &SynthPatch::fDecay, 0.01f, 6, 1.25f, 1.05f, uSeconds },
    { "Filter sustain", &SynthPatch::fSustain, 0, 1, 0.05f, 0.01f, uPercent },
    { "Filter release", &SynthPatch::fRelease, 0.01f, 8, 1.25f, 1.05f, uSeconds },
    { "LFO rate", &SynthPatch::lfoRate, 0.05f, 20, 1.2f, 1.04f, uHertz },
    { "LFO vibrato", &SynthPatch::lfoPitch, 0, 2, 0.01f, 0.002f, uSemitones },
    { "LFO filter", &SynthPatch::lfoCutoff, 0, 2, 0.05f, 0.01f, uOctaves },
    { "LFO pulse", &SynthPatch::lfoPulse, 0, 1, 0.05f, 0.01f, uPercent },
    { "Drive", &SynthPatch::drive, 0, 1, 0.05f, 0.01f, uPercent },
    { "Chorus", &SynthPatch::ensemble, 0, 1, 0.05f, 0.01f, uPercent },
    { "Chorus depth", &SynthPatch::depth, 0, 1, 0.05f, 0.01f, uPercent },
    { "Glide", &SynthPatch::glide, 0, 1, 0.01f, 0.002f, uSeconds },
    { "Mono", nullptr, 0, 1, 1, 1, uSwitch },
    { "Level", &SynthPatch::level, 0, 2, 0.05f, 0.01f, uPercent }
};

// the part of the screen text that shows a number with a sign and one or two decimals
void signedDecimal(char* out, int size, int hundredths, const char* unit)
{
    const bool neg = hundredths < 0;
    const int a = neg ? -hundredths : hundredths;
    fmt(out, size, "%s%d.%02d %s", neg ? "-" : "", a / 100, a % 100, unit);
}
} // namespace

const SynthPatch& synthPreset(int index)
{
    return kPresets[index < 0 ? 0 : (index >= kPresetCount ? kPresetCount - 1 : index)];
}

float synthLevelAmp(int db10) { return exp2f_((float)db10 / 10.0f / 6.0206f); }

const char* synthMacroHint(int index)
{
    static const char* const h[kSynthMacros] = { "Filter opening", "Filter edge", "Fade in", "Fade out", "Detune and sub", "Stereo width", "Grit", "Wobble" };
    return index >= 0 && index < kSynthMacros ? h[index] : "";
}

const char* synthMacroName(int index) { return index >= 0 && index < kSynthMacros ? kMacroNames[index] : ""; }

SynthPatch synthApply(const SynthPatch& base, const SynthMacros& m)
{
    SynthPatch p = base;
    const float bright = (float)m.v[0], reson = (float)m.v[1], attack = (float)m.v[2], release = (float)m.v[3];
    const float thick = (float)m.v[4], chorus = (float)m.v[5], drive = (float)m.v[6], motion = (float)m.v[7];
    p.cutoff = clampf(p.cutoff + bright * 0.04f, 0.02f, 1.0f);
    p.resonance = clampf(p.resonance + reson * 0.06f, 0.0f, 0.95f);
    p.attack = clampf(p.attack * exp2f_(attack * 0.4f), 0.001f, 6.0f);
    p.fAttack = clampf(p.fAttack * exp2f_(attack * 0.4f), 0.001f, 6.0f);
    p.release = clampf(p.release * exp2f_(release * 0.35f), 0.01f, 8.0f);
    p.fRelease = clampf(p.fRelease * exp2f_(release * 0.35f), 0.01f, 8.0f);
    p.detune = clampf(p.detune + thick * 1.2f, 0.0f, 40.0f);
    p.subLevel = clampf(p.subLevel + thick * 0.04f, 0.0f, 1.0f);
    p.depth = clampf(p.depth + thick * 0.03f, 0.0f, 1.0f);
    p.ensemble = clampf(p.ensemble + chorus * 0.1f, 0.0f, 1.0f);
    p.drive = clampf(p.drive + drive * 0.08f, 0.0f, 1.0f);
    p.lfoCutoff = clampf(p.lfoCutoff + motion * 0.04f, 0.0f, 2.0f);
    p.lfoPulse = clampf(p.lfoPulse + motion * 0.04f, 0.0f, 1.0f);
    p.lfoPitch = clampf(p.lfoPitch + motion * 0.01f, 0.0f, 2.0f);
    return p;
}

const char* synthParamName(int i) { return i >= 0 && i < kSynthParams ? kParams[i].name : ""; }

void synthParamStep(SynthPatch& p, int i, int dir, bool fine)
{
    if (i < 0 || i >= kSynthParams) return;
    const ParamDef& d = kParams[i];
    if (d.unit == uOsc)
    {
        int o = (int)p.osc + dir;
        p.osc = (SynthOsc)(o < 0 ? 0 : o > 5 ? 5 : o);
        return;
    }
    if (d.unit == uSwitch)
    {
        p.mono = dir > 0;
        return;
    }
    float& v = p.*(d.f);
    if (d.unit == uSeconds && d.step > 1.0f) // times and the LFO rate move by ratios
    {
        const float r = fine ? d.fine : d.step;
        v = dir > 0 ? v * r : v / r;
    }
    else if (d.unit == uHertz)
    {
        const float r = fine ? d.fine : d.step;
        v = dir > 0 ? v * r : v / r;
    }
    else v += (fine ? d.fine : d.step) * (float)dir;
    v = clampf(v, d.lo, d.hi);
}

float synthParamGet(const SynthPatch& p, int i)
{
    if (i < 0 || i >= kSynthParams) return 0.0f;
    const ParamDef& d = kParams[i];
    if (d.unit == uOsc) return (float)(int)p.osc;
    if (d.unit == uSwitch) return p.mono ? 1.0f : 0.0f;
    return p.*(d.f);
}

void synthParamSet(SynthPatch& p, int i, float value)
{
    if (i < 0 || i >= kSynthParams) return;
    const ParamDef& d = kParams[i];
    if (d.unit == uOsc) { const int o = (int)(value + 0.5f); p.osc = (SynthOsc)(o < 0 ? 0 : o > 5 ? 5 : o); return; }
    if (d.unit == uSwitch) { p.mono = value > 0.5f; return; }
    if (!(value == value)) value = d.lo; // not a number
    p.*(d.f) = clampf(value, d.lo, d.hi);
}

float synthParamFraction(const SynthPatch& p, int i)
{
    if (i < 0 || i >= kSynthParams) return 0.0f;
    const ParamDef& d = kParams[i];
    if (d.unit == uOsc) return (float)(int)p.osc / 5.0f;
    if (d.unit == uSwitch) return p.mono ? 1.0f : 0.0f;
    const float v = p.*(d.f);
    return clampf((v - d.lo) / (d.hi - d.lo), 0.0f, 1.0f);
}

void synthParamText(const SynthPatch& p, int i, char* out, int size)
{
    out[0] = 0;
    if (i < 0 || i >= kSynthParams) return;
    const ParamDef& d = kParams[i];
    if (d.unit == uOsc)
    {
        fmt(out, size, "%s", p.osc == SynthOsc::Shape ? "Shape" : p.osc == SynthOsc::Saw ? "Saw" : p.osc == SynthOsc::SuperSquare ? "Super square" : p.osc == SynthOsc::Fm ? "FM" : p.osc == SynthOsc::Modal ? "Modal" : "String");
        return;
    }
    if (d.unit == uSwitch)
    {
        fmt(out, size, "%s", p.mono ? "On" : "Off");
        return;
    }
    const float v = p.*(d.f);
    switch (d.unit)
    {
        case uPercent: fmt(out, size, "%d%%", roundi(v * 100.0f)); break;
        case uSeconds:
            if (v < 0.995f) fmt(out, size, "%d ms", roundi(v * 1000.0f));
            else signedDecimal(out, size, roundi(v * 100.0f), "s");
            break;
        case uHertz: signedDecimal(out, size, roundi(v * 100.0f), "Hz"); break;
        case uCents: signedDecimal(out, size, roundi(v * 100.0f), "ct"); break;
        case uSemitones: signedDecimal(out, size, roundi(v * 100.0f), "st"); break;
        default: signedDecimal(out, size, roundi(v * 100.0f), "oct"); break;
    }
}
} // namespace ft
