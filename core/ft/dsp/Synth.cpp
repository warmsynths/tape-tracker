#include "Synth.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace ft
{
namespace
{
template<int N> struct Tag { static constexpr int value = N; };
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
        v.phA = v.phB = 0.0f;
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

void Synth::setVoiceLimit(int n)
{
    limit = n < 1 ? 1 : (n > kVoices ? kVoices : n);
    for (int i = limit; i < kVoices; i++)
    {
        // a voice that is no longer allowed fades out over the next block, so there is no click
        Voice& v = voice[i];
        v.gate = false;
        if (v.busy()) v.pendNote = kDying;
    }
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
    for (int i = 0; i < limit; i++) if (voice[i].gate && voice[i].midi == note) pick = &voice[i];
    if (pick == nullptr) for (int i = 0; i < limit; i++) if (!voice[i].busy()) { pick = &voice[i]; break; }
    if (pick == nullptr) for (int i = 0; i < limit; i++) if (!voice[i].gate && (pick == nullptr || voice[i].age < pick->age)) pick = &voice[i];
    if (pick == nullptr) { pick = &voice[0]; for (int i = 0; i < limit; i++) if (voice[i].age < pick->age) pick = &voice[i]; }
    if (pick->busy() && pick->amp.value > 0.02f && pick->pendNote == kNoPending)
    {
        // taking over a sounding voice: it fades out over one block, then the new note starts on it
        pick->pendNote = note;
        pick->pendVel = velocity;
        return;
    }
    if (pick->busy() && pick->pendNote != kNoPending) { pick->pendNote = note; pick->pendVel = velocity; return; }
    startVoice(*pick, note, velocity);
}

void Synth::noteOff(int note)
{
    for (Voice& v : voice)
    {
        if (v.pendNote == note) v.pendNote = v.busy() ? kDying : kNoPending; // released before it ever started
        if (v.gate && v.midi == note)
        {
            v.gate = false;
            v.amp.release();
            v.flt.release();
        }
    }
}

void Synth::allNotesOff()
{
    for (Voice& v : voice)
    {
        v.gate = false;
        v.pendNote = v.busy() ? kDying : kNoPending;
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

    for (int vi = 0; vi < kVoices; vi++)
    {
        Voice& v = voice[vi];
        if (!v.busy()) continue;
        if (vi >= limit && v.pendNote != kDying) continue;
        const bool fading = v.pendNote != kNoPending;
        float fadeMul = 1.0f;
        const float fadeStep = fading ? 1.0f / (float)n : 0.0f;
        // pitch: glide in a mono lead, then vibrato
        v.pitch += (v.note - v.pitch) * (glideK > 1.0f ? 1.0f : glideK);
        const float note = v.pitch + lfo * p.lfoPitch + mod;
        const float f = noteToNorm(note, rate);
        const float fB = f * detuneRatio > 0.25f ? 0.25f : f * detuneRatio;

        const float keyOct = (note - 60.0f) / 12.0f * p.keyTrack;
        const float velScale = 0.4f + 0.6f * v.vel;
        float y0 = v.y[0], y1 = v.y[1], y2 = v.y[2], y3 = v.y[3];
        float sub = v.subPhase;
        float phA = v.phA, phB = v.phB, g = v.g, gTarget = v.g;
        const bool two = p.detune != 0.0f;
        const float shape = p.shape;
        const SynthOsc oscType = p.osc;
        // band-limited steps (polyBLEP): the cheap way to a clean saw and pulse
        auto blep = [](float t, float dt) -> float {
            if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
            if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
            return 0.0f;
        };
        auto pulseWave = [&](float ph, float freq, float width) -> float {
            float d = ph - width;
            if (d < 0.0f) d += 1.0f;
            return (ph < width ? 1.0f : -1.0f) + blep(ph, freq) - blep(d, freq);
        };
        // the loop is written once and built six ways (oscillator type, one or two oscillators), so no choice is made per frame
        auto run = [&](auto oscTag, auto twoTag) {
            constexpr int OSC = decltype(oscTag)::value;
            constexpr bool TWO = decltype(twoTag)::value != 0;
            auto osc = [&](float& ph, float freq) -> float {
                ph += freq;
                if (ph >= 1.0f) ph -= 1.0f;
                if constexpr (OSC == 0)
                {
                    const float saw = 2.0f * ph - 1.0f - blep(ph, freq);
                    if (shape <= 0.5f)
                    {
                        const float d = ph < 0.5f ? 0.5f - ph : ph - 0.5f;
                        const float a = shape * 2.0f;
                        return (1.0f - 4.0f * d) * (1.0f - a) + saw * a;
                    }
                    const float a = (shape - 0.5f) * 2.0f;
                    return saw * (1.0f - a) + pulseWave(ph, freq, pulse) * a;
                }
                else if constexpr (OSC == 1) return 2.0f * ph - 1.0f - blep(ph, freq);
                else return pulseWave(ph, freq, pulse); // SuperSquare: the thick part is the second pulse below
            };
            for (int i = 0; i < n; i++)
            {
                if ((i & 3) == 0) // the envelopes move every fourth frame
                {
                    advance(v.amp, aRate * 4.0f, aDecayK * 4.0f > 1.0f ? 1.0f : aDecayK * 4.0f, p.sustain, aReleaseK * 4.0f > 1.0f ? 1.0f : aReleaseK * 4.0f, v.gate);
                    advance(v.flt, fRate * 4.0f, fDecayK * 4.0f > 1.0f ? 1.0f : fDecayK * 4.0f, p.fSustain, fReleaseK * 4.0f > 1.0f ? 1.0f : fReleaseK * 4.0f, v.gate);
                }

                float x = osc(phA, f);
                if constexpr (OSC == 2)
                {
                    // two pulses a little apart in pitch and width
                    const float fb = TWO ? fB : f * 1.004f;
                    phB += fb;
                    if (phB >= 1.0f) phB -= 1.0f;
                    x = (x + pulseWave(phB, fb, 1.0f - pulse)) * 0.5f;
                }
                else if constexpr (TWO) x = (x + osc(phB, fB)) * 0.6f;
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

                // 24 dB per octave low-pass: four one-pole stages with feedback; the cutoff is worked out every 8 frames
                if ((i & 7) == 0)
                {
                    const float oct = (cutoffBase - 0.5f) * 8.0f + v.flt.value * p.envAmount + lfo * p.lfoCutoff + keyOct;
                    float fc = 800.0f * exp2f_(oct);
                    if (fc < 20.0f) fc = 20.0f;
                    if (fc > 0.42f * rate) fc = 0.42f * rate;
                    const float w = 6.2831853f * fc / rate;
                    gTarget = w / (1.0f + w);
                }
                g += (gTarget - g) * 0.2f; // glide to it, so a quick sweep does not zipper
                float in = x * makeup - k * y3;
                in = in > 1.5f ? 1.5f : (in < -1.5f ? -1.5f : in);
                in = in * (1.0f - 0.148148f * in * in); // a cubic clip: soft, and no division
                y0 += g * (in - y0);
                y1 += g * (y0 - y1);
                y2 += g * (y1 - y2);
                y3 += g * (y2 - y3);

                fadeMul -= fadeStep;
                sum[i] += y3 * v.amp.value * velScale * 0.5f * fadeMul;
            }
        };
        if (oscType == SynthOsc::Shape) { if (two) run(Tag<0>{}, Tag<1>{}); else run(Tag<0>{}, Tag<0>{}); }
        else if (oscType == SynthOsc::Saw) { if (two) run(Tag<1>{}, Tag<1>{}); else run(Tag<1>{}, Tag<0>{}); }
        else { if (two) run(Tag<2>{}, Tag<1>{}); else run(Tag<2>{}, Tag<0>{}); }
        v.phA = phA; v.phB = phB; v.g = g;
        v.y[0] = y0; v.y[1] = y1; v.y[2] = y2; v.y[3] = y3;
        v.subPhase = sub;
        if (fading)
        {
            const int next = v.pendNote, vel = v.pendVel;
            v.pendNote = kNoPending;
            v.amp.value = 0.0f; v.amp.stage = 0; v.flt.value = 0.0f; v.flt.stage = 0;
            for (float& s : v.y) s = 0.0f;
            if (next >= 0) startVoice(v, next, vel);
        }
    }

    if (p.drive > 0.0f) overdrive.Process(p.drive, sum, (size_t)n);
    for (int i = 0; i < n; i++)
    {
        left[i] = sum[i] * p.level;
        right[i] = left[i];
    }
    if (p.ensemble > 0.0f)
    {
        // the chorus is kept tame: at full strength it warbles like a broken tape
        ensemble.set_amount(p.ensemble > 0.7f ? 0.7f : p.ensemble);
        ensemble.set_depth(p.depth > 0.7f ? 0.7f : p.depth);
        ensemble.Process(left, right, (size_t)n);
    }
    // keep the sum inside the range: a gentle limiter, so a full chord of a thick patch rounds off and does not clip
    for (int i = 0; i < n; i++)
    {
        left[i] = stmlib::SoftLimit(left[i]);
        right[i] = stmlib::SoftLimit(right[i]);
    }
    drums.render(left, right, n);
}
} // namespace ft
