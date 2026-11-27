// Latency measurement: what the recording shows for a known delay
#include "Latency.h"

#include <cstdio>
#include <cstdlib>
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

// What the line input records: the played loop, delayed by `delay` frames, from loop position `startPos`.
static std::vector<int16_t> record(const std::vector<int16_t>& loop, uint32_t startPos, int delay, uint32_t frames, int noise, int gainPct)
{
    std::vector<int16_t> out(frames);
    const uint32_t L = (uint32_t)loop.size();
    for (uint32_t i = 0; i < frames; i++)
    {
        const int64_t src = (int64_t)startPos + i - delay;
        int v = src < 0 ? 0 : loop[(uint64_t)src % L];
        v = v * gainPct / 100;
        if (noise) v += (std::rand() % (2 * noise + 1)) - noise;
        out[i] = (int16_t)v;
    }
    return out;
}

int main()
{
    std::vector<int16_t> loop(kLatencyLoopFrames);
    makeLatencyClicks(loop.data(), kLatencyLoopFrames, kLatencyPulses);
    const uint32_t frames = 154350;
    const int delays[] = { 0, 3, 384, 520, 1000, 2500 };
    const uint32_t starts[] = { 0, 1, 128, 20000, 22049, 22050, 43000 };
    for (int d : delays)
        for (uint32_t sp : starts)
        {
            const std::vector<int16_t> r = record(loop, sp, d, frames, 0, 100);
            const LatencyResult m = measureLatency(r.data(), frames, sp, kLatencyLoopFrames, kLatencyPulses);
            char msg[100];
            std::snprintf(msg, sizeof msg, "delay %d from %u measures %d (ok %d, why %d)", d, sp, m.frames, m.ok, m.why);
            CHECK(m.ok && m.frames == d, msg);
        }
    {   // noise and a weak signal (a quiet line)
        const std::vector<int16_t> r = record(loop, 777, 400, frames, 300, 25);
        const LatencyResult m = measureLatency(r.data(), frames, 777, kLatencyLoopFrames, kLatencyPulses);
        CHECK(m.ok && m.frames >= 398 && m.frames <= 402, "a quiet noisy line still measures about 400 frames");
    }
    {   // nothing connected: silence and hum
        const std::vector<int16_t> r = record(loop, 0, 400, frames, 200, 0);
        const LatencyResult m = measureLatency(r.data(), frames, 0, kLatencyLoopFrames, kLatencyPulses);
        CHECK(!m.ok && m.why == 1, "no cable: no signal");
    }
    {   // something other than our clicks: a steady tone
        std::vector<int16_t> r(frames);
        for (uint32_t i = 0; i < frames; i++) r[i] = (int16_t)((i / 40) % 2 ? 9000 : -9000);
        const LatencyResult m = measureLatency(r.data(), frames, 0, kLatencyLoopFrames, kLatencyPulses);
        CHECK(!m.ok, "a steady tone is not accepted");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
