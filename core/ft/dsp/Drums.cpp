#include "Drums.h"

uint32_t stmlib::Random::rng_state_ = 0x21; // the random generator of the vendored stmlib

namespace ft
{
namespace
{
struct KitDef
{
    const char* name;
    uint8_t kickHz;        // 40 to 80
    uint8_t kickDecay;     // 0 to 100
    uint8_t kickTone;      // 0 to 100
    bool kickSynthetic;    // the analog (808) model, or the synthetic (digital) one
    uint8_t snareHz;       // in units of 2 Hz: 70 is 140 Hz
    uint8_t snareDecay;
    uint8_t snareSnappy;
    bool snareSynthetic;
    uint8_t hatTone;       // 0 to 100: how bright
    uint8_t hatDecay;      // closed hat length
    uint8_t openDecay;     // open hat length
    uint8_t tomHz;         // 70 to 160
};

const KitDef kKitTable[DrumKit::kKits] = {
    // hip-hop
    { "808",    45, 85, 30, false,  95, 45, 55, false, 55, 12, 55, 100 },
    { "Dusty",  52, 55, 45, true,   90, 40, 45, true,  35, 15, 45,  95 },
    { "Lo-fi",  50, 60, 35, true,   92, 50, 40, true,  30, 18, 50,  90 },
    { "Trap",   42, 90, 25, false, 110, 30, 70, false, 70,  8, 60, 110 },
    { "Drill",  44, 80, 30, false, 105, 35, 65, false, 65, 10, 55, 105 },
    { "Phonk",  47, 75, 55, true,  100, 50, 60, true,  50, 14, 50, 100 },
    { "Soul",   55, 45, 50, true,   85, 55, 50, true,  40, 20, 55,  85 },
    { "Funk",   58, 40, 60, false,  95, 45, 60, false, 55, 16, 50,  95 },
    // dance
    { "House",  55, 45, 50, true,  100, 35, 65, false, 60, 10, 50, 110 },
    { "Techno", 52, 55, 55, true,  105, 30, 70, true,  65, 10, 45, 115 },
    { "Garage", 56, 40, 50, false,  95, 40, 60, false, 60, 12, 50, 105 },
    { "DnB",    58, 35, 45, true,  120, 25, 75, true,  70,  8, 40, 120 },
    { "Electro",50, 65, 40, false,  98, 45, 55, false, 55, 12, 55, 100 },
    { "Breaks", 57, 40, 55, true,  100, 40, 65, true,  50, 14, 45, 100 },
    { "Disco",  54, 45, 55, false,  92, 50, 50, false, 60, 16, 60,  95 },
    { "Dub",    48, 70, 35, false,  88, 60, 40, false, 40, 20, 65,  85 },
    // live
    { "Rock",   62, 35, 65, true,   85, 45, 70, true,  50, 18, 50,  90 },
    { "Jazz",   60, 30, 55, true,   90, 35, 45, true,  45, 22, 60,  95 },
    { "Brush",  60, 28, 40, true,   88, 55, 80, true,  35, 25, 65,  90 },
    { "Latin",  58, 35, 55, false,  98, 30, 55, false, 55, 14, 45, 115 },
    { "Afro",   56, 40, 50, false,  96, 35, 50, false, 50, 16, 50, 120 },
    { "Reggae", 54, 45, 45, true,   92, 40, 50, false, 45, 18, 55,  85 },
    { "Live",   60, 35, 60, true,   90, 40, 60, true,  50, 18, 55,  95 },
    { "Tape",   52, 55, 40, true,   88, 50, 45, true,  30, 20, 55,  90 },
};

const char* const kVoiceNames[DrumKit::kVoices] = { "Shaker", "Tamb", "Open alt", "Open", "Conga", "Rim", "Hat alt", "Hat", "Clap", "Bell", "Snare alt", "Snare", "Tom 1", "Tom 2", "Kick alt", "Kick" };

enum Voice { Shaker, Tamb, OpenAlt, Open, Conga, Rim, HatAlt, Hat, Clap, Bell, SnareAlt, Snare, Tom1, Tom2, KickAlt, Kick };

inline float clamp1(float x) { return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x); }
} // namespace

const char* DrumKit::kitName(int k) { return kKitTable[k < 0 ? 0 : (k >= DrumKit::kKits ? DrumKit::kKits - 1 : k)].name; }
const char* DrumKit::kitStyle(int k) { return k < 8 ? "HIP-HOP" : (k < 16 ? "DANCE" : "LIVE"); }
const char* DrumKit::voiceName(int v) { return kVoiceNames[v < 0 ? 0 : (v >= kVoices ? kVoices - 1 : v)]; }

void DrumKit::init(float sampleRate)
{
    rate = sampleRate;
    analogKick.Init(); analogKickAlt.Init(); analogTom1.Init(); analogTom2.Init();
    syntheticKick.Init(); syntheticKickAlt.Init();
    analogSnare.Init(); analogSnareAlt.Init(); syntheticSnare.Init(); syntheticSnareAlt.Init();
    hatA.Init(); openA.Init(); hatB.Init(); openB.Init();
    reset();
    setKit(0);
}

void DrumKit::reset()
{
    for (int i = 0; i < kVoices; i++) { simple[i] = Simple(); accent[i] = 0.0f; pending[i] = false; }
    activeFor = 0;
    chokeTarget = chokeGain = 1.0f;
}

void DrumKit::setKit(int k) { kitIndex = k < 0 ? 0 : (k >= kKits ? kKits - 1 : k); }

void DrumKit::trigger(int voice, int velocity)
{
    if (voice < 0 || voice >= kVoices) return;
    accent[voice] = (float)(velocity < 1 ? 1 : (velocity > 127 ? 127 : velocity)) / 127.0f;
    pending[voice] = true;
    activeFor = (int)(rate * 2.5f / (float)kMaxBlock); // about 2.5 s of sound after a hit
}

// The small voices: percussion made of noise and sines
void DrumKit::renderSimple(int v, float* out, int n)
{
    Simple& s = simple[v];
    const float dt = 1.0f / rate;
    for (int i = 0; i < n; i++)
    {
        noise = noise * 1664525u + 1013904223u;
        const float w = (float)(int32_t)noise * (1.0f / 2147483648.0f); // -1..1
        float o = 0.0f;
        switch (v)
        {
            case Shaker: // noise that swells and dies in about 70 ms, highs only
            {
                const float t = s.age; s.age += dt;
                const float e = (t < 0.006f ? t / 0.006f : 1.0f) * s.env;
                s.env *= 0.99985f - 0.0006f; // about 75 ms
                s.hp += 0.55f * (w - s.hp);
                o = (w - s.hp) * e * 0.5f;
                break;
            }
            case Tamb: // two quick bursts of bright noise with a ring
            {
                const float t = s.age; s.age += dt;
                s.env *= 0.9994f;
                const float burst = (t < 0.012f || (t > 0.03f && t < 0.06f)) ? 1.0f : 0.5f;
                s.hp += 0.4f * (w - s.hp);
                s.phase += 7400.0f * dt; s.phase -= (float)(int)s.phase;
                const float ring = (s.phase < 0.5f ? 1.0f : -1.0f) * 0.12f;
                o = ((w - s.hp) * 0.6f + ring) * s.env * burst * 0.5f;
                break;
            }
            case Conga: // a sine that drops from 260 to 190 Hz
            {
                const float t = s.age; s.age += dt;
                const float f = 190.0f + 70.0f * (t < 0.05f ? (1.0f - t / 0.05f) * (1.0f - t / 0.05f) : 0.0f);
                s.phase += f * dt; s.phase -= (float)(int)s.phase;
                const float sine = (s.phase < 0.5f ? 16.0f * s.phase * (0.5f - s.phase) : -16.0f * (s.phase - 0.5f) * (1.0f - s.phase)) * 1.0f;
                o = (sine + (t < 0.003f ? w * 0.6f : 0.0f)) * s.env * 0.8f;
                s.env *= 0.99984f - 0.00022f;
                break;
            }
            case Rim: // a click: a high sine and a puff of noise, over in 25 ms
            {
                s.age += dt;
                s.phase += 1700.0f * dt; s.phase -= (float)(int)s.phase;
                const float sine = s.phase < 0.5f ? 16.0f * s.phase * (0.5f - s.phase) : -16.0f * (s.phase - 0.5f) * (1.0f - s.phase);
                o = (sine * 0.7f + w * 0.3f) * s.env;
                s.env *= 0.9965f;
                break;
            }
            case Clap: // three short bursts and a tail, through a band around 1.2 kHz
            {
                const float t = s.age; s.age += dt;
                float e = 0.0f;
                if (t < 0.027f) { const float u = t - 0.009f * (float)(int)(t / 0.009f); e = 1.0f - u / 0.009f * 0.7f; }
                else e = s.gain; // the tail
                if (t >= 0.027f) s.gain *= 0.9992f;
                s.hp += 0.15f * (w - s.hp);       // low-pass
                s.lp += 0.02f * (s.hp - s.lp);    // low-pass again: the band is what is left of the difference
                o = (s.hp - s.lp) * e * s.env * 2.2f;
                break;
            }
            case Bell: // two inharmonic sines, a long ring
            {
                s.age += dt;
                s.phase += 540.0f * dt; s.phase -= (float)(int)s.phase;
                s.phase2 += 1490.0f * dt; s.phase2 -= (float)(int)s.phase2;
                const float a = s.phase < 0.5f ? 16.0f * s.phase * (0.5f - s.phase) : -16.0f * (s.phase - 0.5f) * (1.0f - s.phase);
                const float b = s.phase2 < 0.5f ? 16.0f * s.phase2 * (0.5f - s.phase2) : -16.0f * (s.phase2 - 0.5f) * (1.0f - s.phase2);
                o = (a * 0.6f + b * 0.4f) * s.env * 0.6f;
                s.env *= 0.99990f;
                break;
            }
            default: break;
        }
        out[i] += o;
    }
    if (s.env < 0.0005f) s.env = 0.0f;
}

void DrumKit::render(float* left, float* right, int n)
{
    if (activeFor <= 0) return;
    activeFor--;
    const KitDef& k = kKitTable[kitIndex];
    float mix[kMaxBlock];
    for (int i = 0; i < n; i++) mix[i] = 0.0f;
    float a[kMaxBlock], t1[kMaxBlock], t2[kMaxBlock];
    auto clear = [&](float* b) { for (int i = 0; i < n; i++) b[i] = 0.0f; };
    const float sr = 44100.0f;

    // kick and kick alt: the alt is the other model, a little higher
    {
        const bool trig = pending[Kick]; const float acc = accent[Kick]; pending[Kick] = false;
        const float tone = (float)k.kickTone / 100.0f, decay = (float)k.kickDecay / 100.0f;
        clear(a);
        if (k.kickSynthetic) syntheticKick.Render(false, trig, acc, (float)k.kickHz / sr, tone, decay, 0.3f, 0.5f, 0.5f, a, (size_t)n);
        else analogKick.Render(false, trig, acc, (float)k.kickHz / sr, tone, decay, 0.5f, 0.5f, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.9f;
        const bool trigB = pending[KickAlt]; const float accB = accent[KickAlt]; pending[KickAlt] = false;
        clear(a);
        if (!k.kickSynthetic) syntheticKickAlt.Render(false, trigB, accB, (float)(k.kickHz + 8) / sr, 0.5f, decay * 0.8f, 0.3f, 0.5f, 0.5f, a, (size_t)n);
        else analogKickAlt.Render(false, trigB, accB, (float)(k.kickHz + 8) / sr, 0.4f, decay * 0.8f, 0.5f, 0.5f, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.9f;
    }
    // snare and snare alt
    {
        const bool trig = pending[Snare]; const float acc = accent[Snare]; pending[Snare] = false;
        const float decay = (float)k.snareDecay / 100.0f, snappy = (float)k.snareSnappy / 100.0f;
        clear(a);
        if (k.snareSynthetic) syntheticSnare.Render(false, trig, acc, (float)k.snareHz * 2.0f / sr, 0.5f, decay, snappy, a, (size_t)n);
        else analogSnare.Render(false, trig, acc, (float)k.snareHz * 2.0f / sr, 0.5f, decay, snappy, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.8f;
        const bool trigB = pending[SnareAlt]; const float accB = accent[SnareAlt]; pending[SnareAlt] = false;
        clear(a);
        if (!k.snareSynthetic) syntheticSnareAlt.Render(false, trigB, accB, (float)(k.snareHz + 20) * 2.0f / sr, 0.6f, decay * 0.7f, snappy, a, (size_t)n);
        else analogSnareAlt.Render(false, trigB, accB, (float)(k.snareHz + 20) * 2.0f / sr, 0.6f, decay * 0.7f, snappy, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.8f;
    }
    // toms
    {
        const bool t1 = pending[Tom1]; const float acc1 = accent[Tom1]; pending[Tom1] = false;
        clear(a);
        analogTom1.Render(false, t1, acc1, (float)k.tomHz / sr, 0.7f, 0.35f, 0.4f, 0.4f, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.8f;
        const bool t2 = pending[Tom2]; const float acc2 = accent[Tom2]; pending[Tom2] = false;
        clear(a);
        analogTom2.Render(false, t2, acc2, (float)(k.tomHz + 40) / sr, 0.7f, 0.3f, 0.4f, 0.4f, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.8f;
    }
    // hats: closed and alt, then open and open alt; a closed hat chokes the open ones
    {
        const float tone = (float)k.hatTone / 100.0f;
        const bool trig = pending[Hat]; const float acc = accent[Hat]; pending[Hat] = false;
        const bool trigB = pending[HatAlt]; const float accB = accent[HatAlt]; pending[HatAlt] = false;
        if (trig || trigB) chokeTarget = 0.0f;
        clear(a);
        hatA.Render(false, trig, acc, 414.0f / sr, tone, (float)k.hatDecay / 100.0f, 0.2f, t1, t2, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.5f;
        clear(a);
        hatB.Render(false, trigB, accB, 480.0f / sr, tone, (float)k.hatDecay / 100.0f, 0.3f, t1, t2, a, (size_t)n);
        for (int i = 0; i < n; i++) mix[i] += a[i] * 0.5f;
        const bool trigO = pending[Open]; const float accO = accent[Open]; pending[Open] = false;
        const bool trigOB = pending[OpenAlt]; const float accOB = accent[OpenAlt]; pending[OpenAlt] = false;
        if (trigO || trigOB) chokeTarget = 1.0f;
        clear(a);
        openA.Render(false, trigO, accO, 414.0f / sr, tone, (float)k.openDecay / 100.0f, 0.2f, t1, t2, a, (size_t)n);
        float b2[kMaxBlock];
        clear(b2);
        openB.Render(false, trigOB, accOB, 480.0f / sr, tone, (float)k.openDecay / 100.0f, 0.3f, t1, t2, b2, (size_t)n);
        for (int i = 0; i < n; i++)
        {
            chokeGain += (chokeTarget - chokeGain) * 0.02f; // a few milliseconds
            mix[i] += (a[i] + b2[i]) * 0.5f * chokeGain;
        }
    }
    // the small voices
    for (int v = 0; v < kVoices; v++)
    {
        if (v != Shaker && v != Tamb && v != Conga && v != Rim && v != Clap && v != Bell) continue;
        if (pending[v])
        {
            Simple& s = simple[v];
            s.env = accent[v]; s.age = 0.0f; s.phase = s.phase2 = 0.0f; s.gain = 1.0f; s.lp = s.hp = 0.0f;
            pending[v] = false;
        }
        if (simple[v].env > 0.0f) renderSimple(v, mix, n);
    }
    for (int i = 0; i < n; i++)
    {
        const float o = clamp1(mix[i] * 0.7f);
        left[i] += o;
        right[i] += o;
    }
}
} // namespace ft
