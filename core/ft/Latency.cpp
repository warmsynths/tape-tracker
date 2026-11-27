#include "Latency.h"

namespace ft
{
void makeLatencyClicks(int16_t* buf, uint32_t loopLen, uint32_t pulses)
{
    for (uint32_t i = 0; i < loopLen; i++) buf[i] = 0;
    if (pulses == 0) return;
    const uint32_t period = loopLen / pulses;
    for (uint32_t k = 0; k < pulses; k++)
        for (uint32_t i = 0; i < 8; i++) buf[k * period + i] = i < 4 ? 28000 : -28000; // a sharp click, up then down
}

LatencyResult measureLatency(const int16_t* rec, uint32_t frames, uint32_t startPos, uint32_t loopLen, uint32_t pulses)
{
    LatencyResult r;
    if (pulses == 0 || loopLen < pulses) { r.why = 4; return r; }
    const uint32_t period = loopLen / pulses;
    int32_t peak = 0;
    for (uint32_t i = 0; i < frames; i++)
    {
        const int32_t a = rec[i] < 0 ? -(int32_t)rec[i] : rec[i];
        if (a > peak) peak = a;
    }
    if (peak < 1500) { r.why = 1; return r; }
    const int32_t thr = peak / 3;
    int32_t d[24];
    int n = 0;
    uint32_t i = 0;
    while (i < frames && n < 24)
    {
        const int32_t a = rec[i] < 0 ? -(int32_t)rec[i] : rec[i];
        if (a < thr) { i++; continue; }
        // a click: the delay is how far it is past the nearest earlier click position
        int32_t v = (int32_t)((i + startPos) % period);
        if (v > (int32_t)(period * 3 / 4)) v -= (int32_t)period; // a very small or slightly negative delay
        d[n++] = v;
        i += period * 6 / 10; // skip the rest of this click (and its ringing) and look for the next
    }
    r.count = n;
    if (n < 3) { r.why = 4; return r; }
    // ignore the first click (the loop may have been starting) and take the median of the others
    int32_t s[24];
    int m = 0;
    for (int k = 1; k < n; k++) s[m++] = d[k];
    for (int a = 1; a < m; a++)
        for (int b = a; b > 0 && s[b] < s[b - 1]; b--) { const int32_t t = s[b]; s[b] = s[b - 1]; s[b - 1] = t; }
    r.frames = s[m / 2];
    r.spread = s[m - 1] - s[0];
    if (r.spread > 8) { r.why = 2; return r; }
    if (r.frames < 0 || r.frames > 4000) { r.why = 3; return r; }
    r.ok = true;
    return r;
}
} // namespace ft
