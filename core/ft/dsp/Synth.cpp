#include "Synth.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

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

inline float noteToNorm(float note, float rate)
{
    float f = 440.0f * exp2f_((note - 69.0f) / 12.0f) / rate;
    return f > 0.25f ? 0.25f : f;
}
} // namespace


void Synth::init(float sampleRate)
{
    rate = sampleRate;
    ensemble.Init(ensembleBuffer);
    overdrive.Init();
    drums.init(sampleRate);
    for (Voice& v : voice)
    {
        v.shapeA.Init(); v.shapeB.Init();
        v.sawA.Init(); v.sawB.Init();
        v.squareA.Init(); v.squareB.Init();
        v.amp = Env(); v.flt = Env();
        for (float& s : v.y) s = 0.0f;
        v.midi = -1;
        v.gate = false;
    }
    setPreset(0);
}

void Synth::setPatch(const SynthPatch& patch)
{
    p = patch;
}

bool Synth::active() const
{
    for (const Voice& v : voice) if (v.busy()) return true;
    return drums.active();
}

void Synth::startVoice(Voice& v, int note, int velocity)
{
    const bool wasBusy = v.busy();
    v.midi = note;
    v.note = (float)note;
    if (!(p.mono && wasBusy)) v.pitch = (float)note; // a mono lead glides from the last note, the rest start on pitch
    v.vel = (float)velocity / 127.0f;
    v.gate = true;
    v.age = ++clock;
    v.amp.trigger();
    v.flt.trigger();
    if (!wasBusy) { v.amp.value = 0.0f; v.flt.value = 0.0f; for (float& s : v.y) s = 0.0f; }
}

void Synth::noteOn(int note, int velocity)
{
    if (velocity < 1) { noteOff(note); return; }
    if (p.mono)
    {
        startVoice(voice[0], note, velocity);
        for (int i = 1; i < kVoices; i++) voice[i].gate = false, voice[i].amp.release();
        return;
    }
    // retrigger the same note, else a free voice, else the oldest releasing one, else the oldest
    Voice* pick = nullptr;
    for (Voice& v : voice) if (v.gate && v.midi == note) pick = &v;
    if (pick == nullptr) for (Voice& v : voice) if (!v.busy()) { pick = &v; break; }
    if (pick == nullptr) for (Voice& v : voice) if (!v.gate && (pick == nullptr || v.age < pick->age)) pick = &v;
    if (pick == nullptr) { pick = &voice[0]; for (Voice& v : voice) if (v.age < pick->age) pick = &v; }
    startVoice(*pick, note, velocity);
}

void Synth::noteOff(int note)
{
    for (Voice& v : voice)
        if (v.gate && v.midi == note)
        {
            v.gate = false;
            v.amp.release();
            v.flt.release();
        }
}

void Synth::allNotesOff()
{
    for (Voice& v : voice)
    {
        v.gate = false;
        v.amp.release();
        v.flt.release();
    }
}

// One step of an envelope: linear attack, then exponential decay to the sustain level and release to zero
void Synth::advance(Env& e, float attackRate, float decayK, float sustain, float releaseK, bool gate)
{
    switch (e.stage)
    {
        case 1:
            e.value += attackRate;
            if (e.value >= 1.0f) { e.value = 1.0f; e.stage = 2; }
            break;
        case 2:
            e.value += (sustain - e.value) * decayK;
            if (!gate) e.stage = 3;
            break;
        case 3:
            e.value -= e.value * releaseK;
            if (e.value < 0.0004f) { e.value = 0.0f; e.stage = 0; }
            break;
        default: break;
    }
}

void Synth::render(float* left, float* right, int n)
{
    if (n > kMaxBlock) n = kMaxBlock;
    for (int i = 0; i < n; i++) sum[i] = 0.0f;

    // block-rate values
    auto timeK = [&](float seconds) { const float s = seconds < 0.002f ? 0.002f : seconds; const float k = 4.6f / (s * rate); return k > 1.0f ? 1.0f : k; };
    const float aRate = 1.0f / ((p.attack < 0.001f ? 0.001f : p.attack) * rate);
    const float aDecayK = timeK(p.decay), aReleaseK = timeK(p.release);
    const float fRate = 1.0f / ((p.fAttack < 0.001f ? 0.001f : p.fAttack) * rate);
    const float fDecayK = timeK(p.fDecay), fReleaseK = timeK(p.fRelease);
    lfoPhase += p.lfoRate * (float)n / rate;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    const float lfo = plaits::Sine(lfoPhase);
    vibPhase += 5.5f * (float)n / rate;
    if (vibPhase >= 1.0f) vibPhase -= 1.0f;
    const float mod = bendSemis + (vibDepth > 0.0f ? plaits::Sine(vibPhase) * vibDepth : 0.0f);
    const float glideK = p.glide > 0.0f ? timeK(p.glide) * (float)n : 1.0f;
    const float detuneRatio = p.detune != 0.0f ? exp2f_(p.detune / 1200.0f) : 1.0f;
    const float cutoffBase = p.cutoff + (brightness - 0.5f) * 0.6f;
    const float k = p.resonance * 3.9f;
    const float makeup = 1.0f + 0.5f * k;
    float pulse = p.pulse + lfo * p.lfoPulse * 0.4f;
    if (pulse < 0.1f) pulse = 0.1f;
    if (pulse > 0.9f) pulse = 0.9f;

    for (Voice& v : voice)
    {
        if (!v.busy()) continue;
        // pitch: glide in a mono lead, then vibrato
        v.pitch += (v.note - v.pitch) * (glideK > 1.0f ? 1.0f : glideK);
        const float note = v.pitch + lfo * p.lfoPitch + mod;
        const float f = noteToNorm(note, rate);
        const float fB = f * detuneRatio > 0.25f ? 0.25f : f * detuneRatio;

        switch (p.osc)
        {
            case SynthOsc::Shape:
                v.shapeA.Render(f, pulse, p.shape, mixA, (size_t)n);
                if (p.detune != 0.0f) v.shapeB.Render(fB, pulse, p.shape, mixB, (size_t)n);
                break;
            case SynthOsc::Saw:
                v.sawA.Render(f, pulse, p.shape, mixA, (size_t)n);
                if (p.detune != 0.0f) v.sawB.Render(fB, pulse, p.shape, mixB, (size_t)n);
                break;
            case SynthOsc::SuperSquare:
                v.squareA.Render(f, p.shape, mixA, (size_t)n);
                if (p.detune != 0.0f) v.squareB.Render(fB, p.shape, mixB, (size_t)n);
                break;
        }

        const float keyOct = (note - 60.0f) / 12.0f * p.keyTrack;
        const float velScale = 0.4f + 0.6f * v.vel;
        float y0 = v.y[0], y1 = v.y[1], y2 = v.y[2], y3 = v.y[3];
        float sub = v.subPhase;
        for (int i = 0; i < n; i++)
        {
            advance(v.amp, aRate, aDecayK, p.sustain, aReleaseK, v.gate);
            advance(v.flt, fRate, fDecayK, p.fSustain, fReleaseK, v.gate);

            float x = mixA[i];
            if (p.detune != 0.0f) x = (x + mixB[i]) * 0.6f;
            if (p.subLevel > 0.0f)
            {
                sub += f * 0.5f;
                if (sub >= 1.0f) sub -= 1.0f;
                x += p.subLevel * (sub < 0.5f ? 0.5f : -0.5f);
            }
            if (p.noise > 0.0f)
            {
                noiseState = noiseState * 1664525u + 1013904223u;
                x += p.noise * ((float)(int32_t)(noiseState >> 8) / 8388608.0f) * 0.4f;
            }

            // 24 dB per octave low-pass: four one-pole stages with feedback
            const float oct = (cutoffBase - 0.5f) * 8.0f + v.flt.value * p.envAmount + lfo * p.lfoCutoff + keyOct;
            float fc = 800.0f * exp2f_(oct);
            if (fc < 20.0f) fc = 20.0f;
            if (fc > 0.42f * rate) fc = 0.42f * rate;
            const float w = 6.2831853f * fc / rate;
            const float g = w / (1.0f + w);
            const float in = stmlib::SoftClip(x * makeup - k * y3);
            y0 += g * (in - y0);
            y1 += g * (y0 - y1);
            y2 += g * (y1 - y2);
            y3 += g * (y2 - y3);

            sum[i] += y3 * v.amp.value * velScale * 0.5f;
        }
        v.y[0] = y0; v.y[1] = y1; v.y[2] = y2; v.y[3] = y3;
        v.subPhase = sub;
    }

    if (p.drive > 0.0f) overdrive.Process(p.drive, sum, (size_t)n);
    for (int i = 0; i < n; i++)
    {
        left[i] = sum[i] * p.level;
        right[i] = left[i];
    }
    if (p.ensemble > 0.0f)
    {
        ensemble.set_amount(p.ensemble);
        ensemble.set_depth(p.depth);
        ensemble.Process(left, right, (size_t)n);
    }
    drums.render(left, right, n);
}
} // namespace ft
