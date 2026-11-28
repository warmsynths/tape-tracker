#include "SoundArt.h"

#include <math.h>

namespace ft
{
namespace
{
const float kPi = 3.14159265f;

inline void setPx(uint8_t* out, int stride, int w, int h, int x, int y)
{
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    out[y * stride + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
}

// The shape of one cycle (phase 0 to 1) from -1 to 1
float cycleValue(const SynthPatch& p, float ph)
{
    auto tri = [](float x) { return 1.0f - 4.0f * fabsf(x - 0.5f) * 1.0f + 0.0f; };
    auto saw = [](float x) { return 2.0f * x - 1.0f; };
    switch (p.osc)
    {
        case SynthOsc::Shape:
        {
            const float sq = ph < (p.pulse < 0.05f ? 0.05f : (p.pulse > 0.95f ? 0.95f : p.pulse)) ? 1.0f : -1.0f;
            const float s = p.shape;
            if (s < 0.5f) { const float a = s * 2.0f; return tri(ph) * (1.0f - a) + saw(ph) * a; }
            const float a = (s - 0.5f) * 2.0f;
            return saw(ph) * (1.0f - a) + sq * a;
        }
        case SynthOsc::Saw:
        {
            // a saw with a notch: the notch moves with Shape
            const float notch = p.shape;
            const float v = saw(ph);
            const float d = fabsf(ph - 0.5f);
            return d < notch * 0.25f ? v * 0.2f : v;
        }
        case SynthOsc::Fm:
            return sinf(2.0f * kPi * ph + 2.0f * sinf(2.0f * kPi * ph * (1.0f + p.shape * 4.0f)));
        case SynthOsc::Modal:
            return 0.6f * sinf(2.0f * kPi * ph) + 0.3f * sinf(2.0f * kPi * ph * 2.76f) + 0.15f * sinf(2.0f * kPi * ph * 5.4f);
        case SynthOsc::String:
            return (2.0f * ph - 1.0f) * (0.5f + 0.5f * sinf(2.0f * kPi * ph * 3.0f));
        default:
        {
            // a thick pulse: a square with a second, slightly shifted one on top
            const float w = 0.3f + p.pulse * 0.4f;
            const float a = ph < w ? 1.0f : -1.0f;
            const float b = ph < w + 0.1f ? 1.0f : -1.0f;
            return (a + b) * 0.5f;
        }
    }
}
} // namespace

void artDialRing(uint8_t* out)
{
    for (int i = 0; i < kDialBytes; i++) out[i] = 0;
    const int c = kDialPx / 2;
    for (int deg = -225; deg <= 45; deg++)
    {
        const float a = (float)deg * kPi / 180.0f;
        const int x = c + ftRound(26.0f * cosf(a));
        const int y = c + ftRound(26.0f * sinf(a));
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) setPx(out, kDialPx / 8, kDialPx, kDialPx, x + dx, y + dy);
    }
}

int artDialAngle(int value)
{
    const int v = value < -10 ? -10 : (value > 10 ? 10 : value);
    return -90 + (v * 135) / 10;
}

void artPatchCycle(const SynthPatch& p, uint8_t* out)
{
    for (int i = 0; i < kCycleBytes; i++) out[i] = 0;
    int prev = -1;
    for (int x = 0; x < kCycleW; x++)
    {
        const float v = cycleValue(p, (float)x / (float)kCycleW);
        const int y = (kCycleH / 2) - ftRound(v * 7.0f);
        const int from = prev < 0 ? y : prev;
        const int lo = from < y ? from : y, hi = from < y ? y : from;
        for (int yy = lo; yy <= hi; yy++) { setPx(out, kCycleW / 8, kCycleW, kCycleH, x, yy); setPx(out, kCycleW / 8, kCycleW, kCycleH, x, yy + 1); }
        prev = y;
    }
}

int artNote(const SynthPatch& p, uint8_t* wave, uint8_t* silhouette)
{
    for (int i = 0; i < kNoteBytes; i++) { wave[i] = 0; silhouette[i] = 0; }
    const int stride = kNoteW / 8, mid = kNoteH / 2, amp = 22;
    const int holdX = kNoteW * 55 / 100;
    // time is squeezed (square roots) so a 10 ms attack and a 2 s release both show
    const float a = sqrtf(p.attack), d = sqrtf(p.decay), r = sqrtf(p.release);
    const float pre = a + d + 0.12f;
    const int attackX = 2 + (int)((float)(holdX - 4) * (a + 0.04f) / (pre + 0.04f));
    const int decayX = attackX + (int)((float)(holdX - attackX - 2) * d / (d + 0.12f));
    float rel = r / 1.6f;
    rel = rel < 0.06f ? 0.06f : (rel > 1.0f ? 1.0f : rel);
    const int endX = holdX + (int)((float)(kNoteW - 4 - holdX) * rel);
    const float sus = p.sustain;
    for (int x = 0; x < kNoteW; x++)
    {
        float env;
        if (x < 2) env = 0.0f;
        else if (x < attackX) env = (float)(x - 2) / (float)(attackX - 2 > 0 ? attackX - 2 : 1);
        else if (x < decayX) env = 1.0f - (1.0f - sus) * (float)(x - attackX) / (float)(decayX - attackX > 0 ? decayX - attackX : 1);
        else if (x < holdX) env = sus;
        else if (x < endX) { const float t = (float)(x - holdX) / (float)(endX - holdX > 0 ? endX - holdX : 1); env = sus * (1.0f - t) * (1.0f - t); }
        else env = 0.0f;
        const int h = ftRound(env * (float)amp);
        for (int y = mid - h; y <= mid + h; y++) setPx(silhouette, stride, kNoteW, kNoteH, x, y);
        if (h > 0)
        {
            // the oscillator, about 30 cycles across the note, as a filled trace under the envelope
            const float ph = (float)x * 30.0f / (float)kNoteW;
            const float v = cycleValue(p, ph - floorf(ph));
            const int top = mid - ftRound(v * (float)h);
            const int lo = top < mid ? top : mid, hi = top < mid ? mid : top;
            for (int y = lo; y <= hi; y++) setPx(wave, stride, kNoteW, kNoteH, x, y);
        }
    }
    return holdX < endX ? holdX : endX;
}
} // namespace ft
