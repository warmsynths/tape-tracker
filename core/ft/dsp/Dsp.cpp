#include "Dsp.h"

namespace ft
{
namespace
{
inline float absf(float x) { return x < 0 ? -x : x; }
inline int32_t abs32(int32_t x) { return x < 0 ? -x : x; }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// 2^x for x between -20 and 20, error below 0.1 %
float exp2Approx(float x)
{
    x = clampf(x, -20.0f, 20.0f);
    const int xi = (int)(x < 0 ? x - 1.0f : x); // floor
    const float f = x - (float)xi;               // 0..1
    // 2^f on 0..1
    const float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    union { float f; uint32_t u; } bits;
    bits.u = (uint32_t)(xi + 127) << 23;
    return bits.f * p;
}

float tanhApprox(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// sine of a phase in turns (0..1), parabolic, error below 1 %
float sineTurns(float p)
{
    p -= (float)(int)p;
    if (p < 0) p += 1.0f;
    return p < 0.5f ? 16.0f * p * (0.5f - p) : -16.0f * (p - 0.5f) * (1.0f - p);
}

float semitoneRatio(int semis)
{
    return exp2Approx((float)semis / 12.0f);
}

constexpr uint32_t kRate = 44100;
} // namespace

//------------------------------------------------------------------------------------------
int findFirstOnset(const int16_t* x, uint32_t from, uint32_t to, uint32_t rate)
{
    constexpr uint32_t cell = 32;
    if (to <= from) return -1;
    const uint32_t n = (to - from) / cell;
    if (n < 8) return -1;

    auto cellPeak = [&](uint32_t i) -> int32_t {
        int32_t m = 0;
        const int16_t* p = x + from + i * cell;
        for (uint32_t j = 0; j < cell; j++)
        {
            const int32_t a = abs32(p[j]);
            if (a > m) m = a;
        }
        return m;
    };

    // The noise floor is the 10th percentile of the cell peaks (a histogram, no sorting).
    static uint32_t histogram[1024];
    for (int i = 0; i < 1024; i++) histogram[i] = 0;
    int32_t maxPeak = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        const int32_t cp = cellPeak(i);
        if (cp > maxPeak) maxPeak = cp;
        histogram[cp >> 5]++;
    }
    uint32_t cumulative = 0, floorPeak = 0;
    for (int b = 0; b < 1024; b++)
    {
        cumulative += histogram[b];
        if (cumulative * 10 >= n)
        {
            floorPeak = (uint32_t)b << 5;
            break;
        }
    }
    int32_t thr = (int32_t)(floorPeak * 4);
    if (thr > maxPeak / 2) thr = maxPeak / 2; // a signal that is loud from the start counts as a note at the start
    if (thr < 103) thr = 103; // -50 dBFS

    uint32_t conf = rate * 5 / 1000 / cell;
    if (conf < 1) conf = 1;
    for (uint32_t i = 0; i + conf < n; i++)
    {
        if (cellPeak(i) < thr) continue;
        uint32_t above = 0;
        for (uint32_t k = 0; k <= conf; k++)
        {
            if (cellPeak(i + k) >= thr) above++;
        }
        if (above < (conf + 2) / 2) continue;

        const uint32_t s = from + i * cell;
        const uint32_t back = rate * 15 / 1000;
        const uint32_t lim = (s > from + back) ? s - back : from;
        uint32_t p = s;
        while (p > lim && abs32(x[p]) >= thr / 4) p--;
        uint32_t q = p;
        while (q < s + cell && abs32(x[q]) < thr / 4) q++;
        const uint32_t zc = rate / 1000;
        uint32_t z = q;
        const uint32_t low = (q > zc + from + 1) ? q - zc : from + 1;
        for (uint32_t k = q; k > low; k--)
        {
            if ((x[k - 1] <= 0 && x[k] > 0) || (x[k - 1] >= 0 && x[k] < 0))
            {
                z = k;
                break;
            }
        }
        return (int)z;
    }
    return -1;
}

//------------------------------------------------------------------------------------------
TrackBank::TrackBank() {}

void TrackBank::resetState()
{
    for (int t = 0; t < 4; t++)
    {
        lp[t] = hp[t] = hpPrev[t] = crushHold[t] = 0.0f;
        crushCount[t] = 0;
    }
    pass = 0;
    lastPos = 0;
}

float TrackBank::readAt(int t, float srcIndex) const
{
    const int16_t* d = data[t];
    float f = srcIndex;
    const float L = (float)loop;
    while (f < 0.0f) f += L;
    while (f >= L) f -= L;
    uint32_t i0 = (uint32_t)f;
    const float frac = f - (float)i0;
    uint32_t i1 = i0 + 1;
    if (i1 >= loop) i1 = 0;
    return ((float)d[i0] * (1.0f - frac) + (float)d[i1] * frac) * (1.0f / 32768.0f);
}

// A repeatable pseudo-random number for a slice of a track on one pass of the loop
static uint32_t sliceHash(int track, uint32_t pass, uint32_t slice, uint32_t salt)
{
    uint32_t x = (uint32_t)track * 0x9E3779B1u + pass * 0x85EBCA6Bu + slice * 0xC2B2AE35u + salt * 0x27D4EB2Fu;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
    return x;
}

void TrackBank::render(uint32_t pos, int n, float* left, float* right)
{
    for (int i = 0; i < n; i++) left[i] = right[i] = 0.0f;
    if (loop == 0) return;
    if (pos < lastPos) pass++;
    lastPos = pos;
    const uint32_t sl = slice ? slice : (loop / 16 > 0 ? loop / 16 : 1);
    const float grainLen = (float)(sl / 2 < kRate * 40 / 1000 ? (sl / 2 > 0 ? sl / 2 : 1) : kRate * 40 / 1000);

    for (int t = 0; t < 4; t++)
    {
        if (data[t] == nullptr || cfg[t].mute) continue;
        TrackCfg& c = cfg[t];
        bool act[4];
        bool any = false;
        for (int s = 0; s < 4; s++)
        {
            act[s] = c.slot[s].type != FxType::None && (c.slot[s].on || c.held[s]);
            any = any || act[s];
        }
        const uint32_t trimEnd = c.trimEnd < loop ? c.trimEnd : loop;
        uint16_t pk = peak[t];
        uint32_t P = pos % loop;

        for (int i = 0; i < n; i++, P = (P + 1 >= loop) ? 0 : P + 1)
        {
            uint32_t q = P;
            bool pitchOn = false;
            float ratio = 1.0f;
            // Slot effects, in order
            float volume = 1.0f, pan = 0.0f;
            int crushAmt = -1, driveAmt = -1, lpAmt = -1, hpAmt = -1;
            if (any)
            {
                for (int s = 0; s < 4; s++)
                {
                    if (!act[s]) continue;
                    const FxSlot& f = c.slot[s];
                    // Held: always. Latched: on a random share of the slices, set by the chance.
                    if (!c.held[s] && f.chance < 100 && (sliceHash(t, pass, P / sl, 10 + s) % 100) >= f.chance) continue;
                    const uint32_t ss = (q / sl) * sl;
                    switch (f.type)
                    {
                        case FxType::JumpBack: q = (q + loop - (sl % loop)) % loop; break;
                        case FxType::JumpBack2: q = (q + loop - ((2 * sl) % loop)) % loop; break;
                        case FxType::Stutter:
                        {
                            const uint32_t div = f.amount < 25 ? 2 : f.amount < 50 ? 4 : f.amount < 75 ? 8 : 16;
                            uint32_t k = sl / div;
                            if (k < 1) k = 1;
                            q = ss + ((q - ss) % k);
                            break;
                        }
                        case FxType::Reverse:
                        {
                            uint32_t within = q - ss;
                            uint32_t span = sl;
                            if (ss + span > loop) span = loop - ss;
                            q = ss + (span - 1 - (within < span ? within : span - 1));
                            break;
                        }
                        case FxType::RandomJumps:
                        {
                            const uint32_t h = sliceHash(t, pass, P / sl, 1);
                            const uint32_t kind = (h >> 8) & 3;
                            if (kind == 0) q = (q + loop - (sl % loop)) % loop;
                            else if (kind == 3) q = (q + loop - ((2 * sl) % loop)) % loop;
                            else if (kind == 1)
                            {
                                uint32_t k = sl / 4;
                                if (k < 1) k = 1;
                                q = ss + ((q - ss) % k);
                            }
                            else
                            {
                                uint32_t within = q - ss;
                                uint32_t span = sl;
                                if (ss + span > loop) span = loop - ss;
                                q = ss + (span - 1 - (within < span ? within : span - 1));
                            }
                            break;
                        }
                        case FxType::PitchSteps:
                        {
                            const uint32_t h = sliceHash(t, pass, P / sl, 2);
                            static const int8_t sets[5][5] = { { 7, -7, 12, 4, -12 }, { 7, -5, 7, -7, 5 }, { 12, -12, 12, -12, 12 }, { 3, 4, -3, -4, 3 }, { 10, 11, -10, -11, 10 } };
                            const int8_t st = sets[f.semis < 0 || f.semis > 4 ? 0 : f.semis][(h >> 8) % 5];
                            pitchOn = true;
                            ratio = semitoneRatio(st);
                            break;
                        }
                        case FxType::Pitch: pitchOn = true; ratio = semitoneRatio(f.semis); break;
                        case FxType::Crush: crushAmt = f.amount; break;
                        case FxType::Drive: driveAmt = f.amount; break;
                        case FxType::Pan: pan = ((float)f.amount - 100.0f) / 100.0f; break;
                        case FxType::LowPass: lpAmt = f.amount; break;
                        case FxType::HighPass: hpAmt = f.amount; break;
                        case FxType::Volume: volume = exp2Approx(-(float)f.amount * 48.0f / 100.0f / 6.0206f); break;
                        default: break;
                    }
                }
            }

            // Read the Part: offset, trim, then pitch
            float sample;
            if (!pitchOn)
            {
                const int64_t idx = (int64_t)q - c.offset;
                int64_t w = idx;
                if (playOnce)
                {
                    if (w < 0 || w >= (int64_t)loop) continue;
                }
                else
                {
                    w %= (int64_t)loop;
                    if (w < 0) w += loop;
                }
                if ((uint32_t)w < c.trimStart || (uint32_t)w >= trimEnd) continue;
                sample = (float)data[t][w] * (1.0f / 32768.0f);
                if (c.fadeIn && (uint32_t)w - c.trimStart < c.fadeIn) sample *= (float)((uint32_t)w - c.trimStart) / (float)c.fadeIn;
                if (c.fadeOut && trimEnd - (uint32_t)w < c.fadeOut) sample *= (float)(trimEnd - (uint32_t)w) / (float)c.fadeOut;
            }
            else
            {
                const uint32_t ss = (q / sl) * sl;
                const float phase = (float)(q - ss);
                float v;
                if (!c.granular)
                {
                    v = readAt(t, (float)ss + phase * ratio - (float)c.offset);
                }
                else
                {
                    // Two overlapping triangular grains read at the new rate, so the length stays
                    float sum = 0.0f;
                    for (int g = 0; g < 2; g++)
                    {
                        const float off = g == 0 ? 0.0f : grainLen * 0.5f;
                        const float u = phase + off;
                        const float k = (float)(int)(u / grainLen);
                        const float gs = k * grainLen - off;
                        const float local = phase - gs;
                        float x01 = local / grainLen;
                        const float w = 1.0f - absf(2.0f * x01 - 1.0f);
                        sum += w * readAt(t, (float)ss + gs + local * ratio - (float)c.offset);
                    }
                    v = sum;
                }
                sample = v * (c.gain >= 0 ? 1.0f : 1.0f);
                if (playOnce)
                {
                    const int64_t idx = (int64_t)q - c.offset;
                    if (idx < 0 || idx >= (int64_t)loop) continue;
                }
            }
            sample *= c.gain;

            if (crushAmt >= 0)
            {
                const int hold = 1 + crushAmt / 25;
                if (crushCount[t] == 0)
                {
                    const float bits = 16.0f - (float)crushAmt * 12.0f / 100.0f;
                    const float levels = exp2Approx(bits - 1.0f);
                    int32_t qv = (int32_t)(sample * levels);
                    crushHold[t] = (float)qv / levels;
                }
                crushCount[t] = (uint8_t)((crushCount[t] + 1) % hold);
                sample = crushHold[t];
            }
            if (driveAmt >= 0)
            {
                const float g = 1.0f + (float)driveAmt * 0.07f;
                sample = tanhApprox(sample * g) / tanhApprox(g);
            }
            if (lpAmt >= 0)
            {
                const float fc = 100.0f * exp2Approx((float)lpAmt * 7.0f / 100.0f);
                const float a = clampf(6.2832f * fc / (float)kRate, 0.0f, 1.0f);
                lp[t] += a * (sample - lp[t]);
                sample = lp[t];
            }
            if (hpAmt >= 0)
            {
                const float fc = 100.0f * exp2Approx((float)hpAmt * 7.0f / 100.0f);
                const float a = clampf(6.2832f * fc / (float)kRate, 0.0f, 1.0f);
                hp[t] += a * (sample - hp[t]);
                sample = sample - hp[t];
            }
            sample *= volume;

            const float aa = absf(sample);
            const uint16_t p16 = (uint16_t)(aa >= 1.0f ? 32767 : (int)(aa * 32767.0f));
            if (p16 > pk) pk = p16;
            const float gl = pan > 0 ? 1.0f - pan : 1.0f;
            const float gr = pan < 0 ? 1.0f + pan : 1.0f;
            left[i] += sample * gl;
            right[i] += sample * gr;
        }
        peak[t] = pk;
    }
}

//------------------------------------------------------------------------------------------
Lofi::Lofi() { reset(); }

void Lofi::reset()
{
    for (int i = 0; i < kDelay; i++) dl[i] = dr[i] = 0.0f;
    writePos = 0;
    phase = phase2 = 0.0f;
    env = 0.0f;
    gain = 1.0f;
    holdL = holdR = 0.0f;
    holdCount = 0;
    grDb = 0;
}

void Lofi::process(float* l, float* r, int n, uint32_t rate)
{
    const float masterVol = cfg.volume >= 100 ? 1.0f : exp2Approx(-(100.0f - (float)cfg.volume) * 48.0f / 100.0f / 6.0206f);
    if (cfg.bypass)
    {
        grDb = 0;
        for (int i = 0; i < n; i++)
        {
            l[i] *= masterVol;
            r[i] *= masterVol;
        }
        return;
    }
    const float wowInc = ((float)cfg.wowSpeed / 10.0f) / (float)rate;
    const float flutInc = wowInc * 9.0f;
    const float depthSamples = (float)cfg.wowDepth / 100.0f * (float)rate * 0.004f; // up to 4 ms
    const float baseDelay = 2.0f * depthSamples + 8.0f;
    const int hold = cfg.crushOn ? (cfg.crushBits >= 14 ? 1 : cfg.crushBits >= 10 ? 2 : cfg.crushBits >= 7 ? 3 : 5) : 1;
    const float levels = exp2Approx((float)cfg.crushBits - 1.0f);
    const float drive = 1.0f + (float)cfg.sat * 0.05f;
    const float satNorm = 1.0f / tanhApprox(drive);
    const float thr = 1.0f - (float)cfg.glue * 0.007f; // 1.0 .. 0.3
    const float attack = 1.0f / ((float)rate * 0.008f);
    const float release = 1.0f / ((float)rate * 0.16f);
    float minGain = 1.0f;

    for (int i = 0; i < n; i++)
    {
        float x = l[i], y = r[i];
        if (cfg.wowOn && cfg.wowDepth > 0)
        {
            dl[writePos] = x;
            dr[writePos] = y;
            phase += wowInc;
            phase2 += flutInc;
            const float mod = sineTurns(phase) * depthSamples + sineTurns(phase2) * depthSamples * 0.15f;
            float rp = (float)writePos - (baseDelay + mod);
            while (rp < 0) rp += (float)kDelay;
            const int i0 = (int)rp;
            const float fr = rp - (float)i0;
            const int i1 = (i0 + 1) % kDelay;
            x = dl[i0 % kDelay] * (1.0f - fr) + dl[i1] * fr;
            y = dr[i0 % kDelay] * (1.0f - fr) + dr[i1] * fr;
            writePos = (writePos + 1) % kDelay;
        }
        if (cfg.crushOn)
        {
            if (holdCount == 0)
            {
                holdL = (float)(int32_t)(x * levels) / levels;
                holdR = (float)(int32_t)(y * levels) / levels;
            }
            holdCount = (holdCount + 1) % hold;
            x = holdL;
            y = holdR;
        }
        if (cfg.satOn)
        {
            x = tanhApprox(x * drive) * satNorm;
            y = tanhApprox(y * drive) * satNorm;
        }
        if (cfg.glueOn)
        {
            const float level = absf(x) > absf(y) ? absf(x) : absf(y);
            env += (level > env ? attack : release) * (level - env);
            float target = 1.0f;
            if (env > thr) target = (thr + (env - thr) / 3.0f) / env;
            gain += (target < gain ? attack * 4.0f : release) * (target - gain);
            if (gain < minGain) minGain = gain;
            const float makeup = 1.0f + (1.0f - thr) * 0.4f;
            x *= gain * makeup;
            y *= gain * makeup;
        }
        l[i] = x * masterVol;
        r[i] = y * masterVol;
    }
    // gain reduction in whole dB: 20 log10(gain) from the amplitude conversion in MiniFmt-free form
    int db = 0;
    float g = minGain;
    while (g < 0.891f && db > -24) { g /= 0.891f; db--; }
    grDb = cfg.glueOn ? db : 0;
}

//------------------------------------------------------------------------------------------
size_t wavHeader(uint8_t* o, uint32_t dataBytes, uint32_t rate)
{
    auto w32 = [&](size_t at, uint32_t v) { o[at] = (uint8_t)v; o[at + 1] = (uint8_t)(v >> 8); o[at + 2] = (uint8_t)(v >> 16); o[at + 3] = (uint8_t)(v >> 24); };
    auto w16 = [&](size_t at, uint16_t v) { o[at] = (uint8_t)v; o[at + 1] = (uint8_t)(v >> 8); };
    const char* riff = "RIFF"; const char* wave = "WAVEfmt "; const char* data = "data";
    for (int i = 0; i < 4; i++) o[i] = (uint8_t)riff[i];
    w32(4, 36 + dataBytes);
    for (int i = 0; i < 8; i++) o[8 + i] = (uint8_t)wave[i];
    w32(16, 16);
    w16(20, 1);
    w16(22, 1);
    w32(24, rate);
    w32(28, rate * 2);
    w16(32, 2);
    w16(34, 16);
    for (int i = 0; i < 4; i++) o[36 + i] = (uint8_t)data[i];
    w32(40, dataBytes);
    return 44;
}
} // namespace ft
