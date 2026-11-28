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
    {   // the tweak knobs and the settings list
        const SynthPatch& base = synthPreset(0);
        SynthMacros none;
        const SynthPatch same = synthApply(base, none);
        CHECK(same.cutoff == base.cutoff && same.attack == base.attack && same.ensemble == base.ensemble, "knobs at zero leave the patch alone");
        for (int k = 0; k < kSynthMacros; k++)
        {
            SynthMacros up, down;
            up.v[k] = kMacroRange; down.v[k] = -kMacroRange;
            const SynthPatch a = synthApply(base, up), b = synthApply(base, down);
            const bool differs = a.cutoff != b.cutoff || a.resonance != b.resonance || a.attack != b.attack || a.release != b.release ||
                                 a.detune != b.detune || a.ensemble != b.ensemble || a.drive != b.drive || a.lfoCutoff != b.lfoCutoff;
            char m[64];
            std::snprintf(m, sizeof m, "knob %d (%s) changes the sound", k, synthMacroName(k));
            CHECK(differs, m);
        }
        SynthMacros extreme;
        for (int k = 0; k < kSynthMacros; k++) extreme.v[k] = kMacroRange;
        for (int pr = 0; pr < kSynthPresets; pr++)
        {
            const SynthPatch a = synthApply(synthPreset(pr), extreme);
            CHECK(a.cutoff <= 1.0f && a.resonance <= 0.95f && a.attack <= 6.0f && a.release <= 8.0f && a.ensemble <= 1.0f && a.drive <= 1.0f, "knobs stay in range");
        }
        // every setting has a name, steps both ways inside its range and shows a text
        SynthPatch p = synthPreset(3);
        for (int i = 0; i < kSynthParams; i++)
        {
            char t[32];
            synthParamText(p, i, t, sizeof t);
            CHECK(synthParamName(i)[0] != 0 && t[0] != 0, "a setting has a name and a value");
            for (int n = 0; n < 60; n++) synthParamStep(p, i, 1, n % 2 == 0);
            const float hi = synthParamFraction(p, i);
            for (int n = 0; n < 120; n++) synthParamStep(p, i, -1, n % 2 == 0);
            const float lo = synthParamFraction(p, i);
            CHECK(hi >= lo && hi <= 1.0f && lo >= 0.0f, "stepping stays in range");
        }
        // a patch edited on the Advanced page still plays
        SynthPatch edit = synthPreset(0);
        for (int i = 0; i < kSynthParams; i++) synthParamStep(edit, i, i % 2 ? 1 : -1, false);
        s.init(kRate); s.setPatch(synthApply(edit, extreme)); s.noteOn(60, 100);
        std::vector<float> a = renderSeconds(s, 0.5f);
        bool finite = true;
        for (float v : a) if (!std::isfinite(v)) finite = false;
        CHECK(finite, "an edited patch with every knob up stays finite");
    }
    {   // taking over a sounding voice, and cutting the voice limit while notes play, do not click
        Synth s;
        s.init(kRate);
        s.setPreset(0);
        for (int n = 0; n < 6; n++) s.noteOn(48 + n * 3, 100);
        renderSeconds(s, 1.0f);
        s.setVoiceLimit(2);                      // four voices fade out
        std::vector<float> a = renderSeconds(s, 0.2f);
        float worst = 0;
        for (size_t i = 1; i < a.size(); i++) { const float d = std::fabs(a[i] - a[i - 1]); if (d > worst) worst = d; }
        CHECK(worst < 0.2f, "dropping to two voices does not jump");
        s.noteOn(72, 100); s.noteOn(75, 100);    // both voices are busy: they are taken over
        std::vector<float> b = renderSeconds(s, 0.3f);
        float w2 = 0;
        for (size_t i = 1; i < b.size(); i++) { const float d = std::fabs(b[i] - b[i - 1]); if (d > w2) w2 = d; }
        CHECK(w2 < 0.2f, "taking over a sounding voice does not jump");
        bool sounds = false;
        for (float v : b) if (std::fabs(v) > 0.01f) sounds = true;
        CHECK(sounds, "and the new notes sound");
    }
    {   // the FM, struck-bar and plucked-string patches sound, ring on their own, and play at the right pitch
        for (int pre : { 2, 6, 7 })
        {
            Synth s;
            s.init(kRate);
            s.setPreset(pre);
            s.noteOn(69, 100);
            std::vector<float> a = renderSeconds(s, 0.6f);
            float pk = 0; bool finite = true;
            for (float v : a) { if (!std::isfinite(v)) finite = false; if (std::fabs(v) > pk) pk = std::fabs(v); }
            CHECK(finite && pk > 0.05f, "an engine patch makes sound");
            if (pre == 7)
            {
                // the pitch, from where the sound best matches a copy of itself
                double best = -1; int bestLag = 0;
                for (int lag = 20; lag < 1200; lag++)
                {
                    double c = 0, en = 0;
                    for (size_t i = 2000; i + lag < a.size(); i++) { c += a[i] * a[i + lag]; en += a[i] * a[i]; }
                    const double v = c / (en + 1e-9);
                    if (v > best) { best = v; bestLag = lag; }
                }
                const float hz = (float)kRate / (float)bestLag;
                CHECK(hz > 425.0f && hz < 455.0f, "the plucked string plays A at 440 Hz");
            }
        }
        Synth s;
        s.init(kRate);
        s.setPreset(2);
        s.noteOn(72, 100);
        renderSeconds(s, 0.05f);
        std::vector<float> early = renderSeconds(s, 0.3f);
        s.noteOff(72);
        std::vector<float> late = renderSeconds(s, 3.0f);
        float e = 0, l = 0;
        for (size_t i = 0; i < early.size(); i++) e += early[i] * early[i];
        for (size_t i = late.size() - 4000; i < late.size(); i++) l += late[i] * late[i];
        CHECK(e > 0.01f && l < e * 0.05f, "a struck bar dies away");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
