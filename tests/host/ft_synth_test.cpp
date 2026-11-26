// The synth voices: pitch, release, polyphony, and that nothing blows up.
#include "Synth.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace ft;

static int failures = 0;
#define CHECK(cond, msg)                                        \
    do                                                          \
    {                                                           \
        if (!(cond))                                            \
        {                                                       \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                         \
        }                                                       \
    } while (0)

static const float kRate = 44117.647f;

static std::vector<float> renderSeconds(Synth& s, float seconds)
{
    std::vector<float> out;
    float l[128], r[128];
    for (int done = 0; done < (int)(seconds * kRate); done += 128)
    {
        s.render(l, r, 128);
        for (int i = 0; i < 128; i++) out.push_back(l[i]);
    }
    return out;
}

static float peakOf(const std::vector<float>& v, size_t from, size_t to)
{
    float p = 0;
    for (size_t i = from; i < to && i < v.size(); i++) p = std::fmax(p, std::fabs(v[i]));
    return p;
}

int main()
{
    static Synth s;
    s.init(kRate);

    {   // every preset makes sound, stays finite and bounded, and ends after its release
        for (int pr = 0; pr < kSynthPresets; pr++)
        {
            s.init(kRate);
            s.setPreset(pr);
            s.noteOn(48, 100); s.noteOn(55, 100); s.noteOn(64, 100);
            std::vector<float> a = renderSeconds(s, 1.5f);
            bool finite = true;
            for (float v : a) if (!std::isfinite(v)) finite = false;
            const float pk = peakOf(a, 0, a.size());
            char m[80];
            std::snprintf(m, sizeof m, "preset %d is finite", pr);
            CHECK(finite, m);
            std::snprintf(m, sizeof m, "preset %d makes sound but stays under full scale (peak %.2f)", pr, pk);
            CHECK(pk > 0.02f && pk < 1.0f, m);
            s.allNotesOff();
            std::vector<float> b = renderSeconds(s, 8.0f);
            std::snprintf(m, sizeof m, "preset %d falls silent after the release", pr);
            CHECK(peakOf(b, b.size() - 2000, b.size()) < 0.002f && !s.active(), m);
        }
    }
    {   // pitch: A3 (note 57) is 220 Hz; count rising zero crossings of the low-passed saw
        s.init(kRate);
        SynthPatch p = synthPreset(5); // Pluck: no chorus movement matters little, so use a plain patch
        p.ensemble = 0; p.detune = 0; p.subLevel = 0; p.resonance = 0; p.cutoff = 0.55f; p.envAmount = 0; p.sustain = 1; p.fSustain = 1; p.attack = 0.002f; p.decay = 0.1f;
        p.lfoPitch = 0; p.drive = 0; p.osc = SynthOsc::Shape; p.shape = 0.5f;
        s.setPatch(p);
        s.noteOn(57, 127);
        std::vector<float> a = renderSeconds(s, 1.2f);
        int cross = 0;
        const size_t from = (size_t)(0.2f * kRate), to = (size_t)(1.2f * kRate);
        for (size_t i = from + 1; i < to && i < a.size(); i++) if (a[i - 1] <= 0 && a[i] > 0) cross++;
        const float hz = (float)cross / ((float)(to - from) / kRate);
        CHECK(hz > 215.0f && hz < 225.0f, "a note plays at its pitch (220 Hz)");
    }
    {   // polyphony: seven notes on six voices steals the oldest and still sounds
        s.init(kRate);
        s.setPreset(0);
        for (int n = 0; n < 7; n++) s.noteOn(48 + n, 100);
        std::vector<float> a = renderSeconds(s, 1.0f);
        CHECK(peakOf(a, 0, a.size()) > 0.02f, "seven notes on six voices still sound");
    }
    {   // a mono lead glides: two notes in a row leave one voice sounding
        s.init(kRate);
        s.setPreset(4);
        s.noteOn(60, 100);
        renderSeconds(s, 0.2f);
        s.noteOn(67, 100);
        std::vector<float> a = renderSeconds(s, 0.3f);
        CHECK(peakOf(a, 0, a.size()) > 0.02f, "a mono lead plays its second note");
    }
    {   // the brightness macro opens the filter
        s.init(kRate);
        SynthPatch p = synthPreset(0);
        p.ensemble = 0; p.envAmount = 0; p.attack = 0.002f;
        s.setPatch(p);
        s.setBrightness(0.1f);
        s.noteOn(50, 110);
        std::vector<float> dark = renderSeconds(s, 0.6f);
        s.init(kRate); s.setPatch(p); s.setBrightness(0.9f); s.noteOn(50, 110);
        std::vector<float> bright = renderSeconds(s, 0.6f);
        // the bright one has more energy in the high part: compare the difference signal
        auto hf = [](const std::vector<float>& v) { double e = 0; for (size_t i = 1000; i + 1 < v.size(); i++) { const float d = v[i + 1] - v[i]; e += d * d; } return e; };
        CHECK(hf(bright) > hf(dark) * 2.0, "brightness opens the filter");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
