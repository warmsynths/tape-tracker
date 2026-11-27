#include "SendFx.h"

namespace ft
{
namespace
{
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

float exp2Approx(float x)
{
    x = clampf(x, -20.0f, 20.0f);
    const int xi = (int)(x < 0 ? x - 1.0f : x);
    const float f = x - (float)xi;
    const float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    union { float f; uint32_t u; } bits;
    bits.u = (uint32_t)(xi + 127) << 23;
    return bits.f * p;
}

inline int16_t toS16(float v)
{
    v *= 32767.0f;
    return (int16_t)(v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v));
}

const char* const kDivNames[kDelayDivs] = { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2" };
const float kDivBeats[kDelayDivs] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f };
const int kRevLens[kRevLines] = { 1103, 1277, 1523, 1801 };
const int kApLens[2] = { 347, 113 };
} // namespace

const char* delayDivName(int i) { return kDivNames[i < 0 ? 0 : (i >= kDelayDivs ? kDelayDivs - 1 : i)]; }
float delayDivBeats(int i) { return kDivBeats[i < 0 ? 0 : (i >= kDelayDivs ? kDelayDivs - 1 : i)]; }

void SendFx::init(SendFxMemory* memory)
{
    mem = nullptr; // the audio side sees nothing while the memory is cleared
    if (memory == nullptr) return;
    for (int c = 0; c < 2; c++) for (int i = 0; i < kDelayMax; i++) memory->delay[c][i] = 0;
    for (int k = 0; k < kRevLines; k++) { for (int i = 0; i < kRevLen; i++) memory->rev[k][i] = 0.0f; revPos[k] = 0; revLp[k] = 0.0f; }
    for (int a = 0; a < 2; a++) { for (int i = 0; i < kApLen; i++) memory->ap[a][i] = 0.0f; apPos[a] = 0; }
    dPos = 0;
    dLpL = dLpR = 0.0f;
    mem = memory;
}

void SendFx::process(const float* sendReverb, const float* sendDelay, float* left, float* right, int n)
{
    if (mem == nullptr) return;
    if (cfg.reverbOn)
    {
        const float g = 0.6f + 0.37f * (float)cfg.reverbSize / 100.0f;
        const float damp = 1.0f - 0.9f * (float)cfg.reverbDamp / 100.0f;
        const float ret = (float)cfg.reverbReturn / 100.0f * 1.5f;
        for (int i = 0; i < n; i++)
        {
            // input diffusion: two Schroeder allpasses
            float x = sendReverb[i] * 0.5f;
            for (int a = 0; a < 2; a++)
            {
                float* b = mem->ap[a];
                const float d = b[apPos[a]];
                const float w = x - 0.6f * d;
                b[apPos[a]] = w;
                x = d + 0.6f * w;
                if (++apPos[a] >= kApLens[a]) apPos[a] = 0;
            }
            float y[kRevLines];
            for (int k = 0; k < kRevLines; k++)
            {
                const float raw = mem->rev[k][revPos[k]];
                revLp[k] += damp * (raw - revLp[k]);
                y[k] = revLp[k];
            }
            // a Hadamard matrix mixes the four lines
            const float h0 = 0.5f * (y[0] + y[1] + y[2] + y[3]);
            const float h1 = 0.5f * (y[0] - y[1] + y[2] - y[3]);
            const float h2 = 0.5f * (y[0] + y[1] - y[2] - y[3]);
            const float h3 = 0.5f * (y[0] - y[1] - y[2] + y[3]);
            mem->rev[0][revPos[0]] = x + g * h0;
            mem->rev[1][revPos[1]] = x + g * h1;
            mem->rev[2][revPos[2]] = x + g * h2;
            mem->rev[3][revPos[3]] = x + g * h3;
            for (int k = 0; k < kRevLines; k++) if (++revPos[k] >= kRevLens[k]) revPos[k] = 0;
            left[i] += (y[0] + y[2] - 0.5f * y[1]) * 0.6f * ret;
            right[i] += (y[1] + y[3] - 0.5f * y[2]) * 0.6f * ret;
        }
    }
    if (cfg.delayOn)
    {
        int t = (int)cfg.delayFrames;
        t = t < 1 ? 1 : (t >= kDelayMax ? kDelayMax - 1 : t);
        const float fb = (float)(cfg.delayFeedback > 95 ? 95 : cfg.delayFeedback) / 100.0f;
        const float tone = 0.05f + 0.95f * (float)cfg.delayTone / 100.0f;
        const float ret = (float)cfg.delayReturn / 100.0f * 1.2f;
        for (int i = 0; i < n; i++)
        {
            int r = dPos - t;
            if (r < 0) r += kDelayMax;
            const float outL = (float)mem->delay[0][r] * (1.0f / 32768.0f);
            const float outR = (float)mem->delay[1][r] * (1.0f / 32768.0f);
            // ping-pong: the input goes in on the left, each repeat crosses over, the repeats darken a little
            dLpL += tone * (outR - dLpL);
            dLpR += tone * (outL - dLpR);
            mem->delay[0][dPos] = toS16(sendDelay[i] + fb * dLpL);
            mem->delay[1][dPos] = toS16(fb * dLpR);
            if (++dPos >= kDelayMax) dPos = 0;
            left[i] += outL * ret;
            right[i] += outR * ret;
        }
    }
}

//------------------------------------------------------------------------------------------
LiveFilterCfg& liveFilter()
{
    static LiveFilterCfg cfg;
    return cfg;
}

float liveFilterCutoff(const LiveFilterCfg& c, bool* low, bool* open)
{
    const float s = (float)c.set / 100.0f;
    bool isLow = true, isOpen = false;
    float hz = 18000.0f;
    switch (c.mode)
    {
        case 1: // low-pass: all the way up is open
            hz = 80.0f * exp2Approx(s * 7.8f);
            isOpen = c.set >= 100;
            break;
        case 2: // high-pass: all the way down is open
            isLow = false;
            hz = 20.0f * exp2Approx(s * 8.6f);
            isOpen = c.set == 0;
            break;
        case 3: // band: a pass band that moves up
            hz = 100.0f * exp2Approx(s * 6.3f);
            isOpen = false;
            break;
        default:
            if (c.set < 50) { hz = 80.0f * exp2Approx((float)c.set / 50.0f * 7.8f); isOpen = c.set >= 49; }
            else { isLow = false; hz = 20.0f * exp2Approx((float)(c.set - 50) / 50.0f * 8.6f); isOpen = c.set <= 51; }
            break;
    }
    if (low != nullptr) *low = isLow;
    if (open != nullptr) *open = isOpen;
    return clampf(hz, 20.0f, 12000.0f);
}

void liveFilterProcess(const LiveFilterCfg& c, LiveFilterState& st, float* left, float* right, int n)
{
    bool low = true, open = false;
    const float hz = liveFilterCutoff(c, &low, &open);
    const float target = (c.active() && !open) ? 1.0f : 0.0f;
    if (st.wet == 0.0f && target == 0.0f) return;
    const float x = 3.14159265f * hz / 44100.0f;
    const float x2 = x * x;
    const float gTarget = x * (1.0f + x2 * (0.33333333f + x2 * (0.13333333f + x2 * 0.05396825f))); // tan(x) for x up to about 0.9
    const float k = c.mode == 3 ? 0.4f : 0.7f; // a little resonance
    for (int i = 0; i < n; i++)
    {
        st.wet += (target - st.wet) * 0.002f; // about 10 ms
        st.g += (gTarget - st.g) * 0.01f;
        const float g = st.g;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        float* ch[2] = { left, right };
        for (int c2 = 0; c2 < 2; c2++)
        {
            const float in = ch[c2][i];
            const float v3 = in - st.ic2[c2];
            const float v1 = a1 * st.ic1[c2] + a2 * v3;
            const float v2 = st.ic2[c2] + a2 * st.ic1[c2] + a3 * v3;
            st.ic1[c2] = 2.0f * v1 - st.ic1[c2];
            st.ic2[c2] = 2.0f * v2 - st.ic2[c2];
            float out;
            if (c.mode == 3) out = v1 * 1.6f * k;        // band
            else out = low ? v2 : in - k * v1 - v2;      // low-pass or high-pass
            ch[c2][i] = in + (out - in) * st.wet;
        }
    }
    if (st.wet < 0.0005f && target == 0.0f) { st.wet = 0.0f; for (int c2 = 0; c2 < 2; c2++) st.ic1[c2] = st.ic2[c2] = 0.0f; }
}
} // namespace ft
