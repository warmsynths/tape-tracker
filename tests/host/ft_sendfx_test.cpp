// Reverb and delay sends, and the live filter
#include "SendFx.h"
#include "Dsp.h"

#include <cmath>
#include <cstdio>
#include <memory>
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

static float energy(const std::vector<float>& v, size_t a, size_t b)
{
    float e = 0;
    for (size_t i = a; i < b && i < v.size(); i++) e += v[i] * v[i];
    return e;
}

// Runs the effects over `frames` with an impulse of 1.0 at frame 0 on the chosen sends
static void run(SendFx& fx, bool rev, bool del, int frames, std::vector<float>& l, std::vector<float>& r)
{
    l.assign(frames, 0.0f);
    r.assign(frames, 0.0f);
    std::vector<float> sr(frames, 0.0f), sd(frames, 0.0f);
    if (rev) sr[0] = 1.0f;
    if (del) sd[0] = 1.0f;
    for (int off = 0; off < frames; off += 128)
    {
        const int n = frames - off < 128 ? frames - off : 128;
        fx.process(sr.data() + off, sd.data() + off, l.data() + off, r.data() + off, n);
    }
}

int main()
{
    std::unique_ptr<SendFxMemory> mem(new SendFxMemory);
    SendFx fx;
    fx.init(mem.get());
    std::vector<float> l, r;

    run(fx, true, true, 20000, l, r);
    CHECK(energy(l, 0, 20000) == 0.0f && energy(r, 0, 20000) == 0.0f, "with both off nothing is added");

    // reverb: a tail that rings and then dies away
    fx.init(mem.get());
    fx.cfg.reverbOn = true;
    fx.cfg.reverbSize = 60;
    run(fx, true, false, 88200, l, r);
    const float early = energy(l, 2000, 12000) + energy(r, 2000, 12000);
    const float late = energy(l, 70000, 80000) + energy(r, 70000, 80000);
    CHECK(early > 0.0f, "the reverb rings after an impulse");
    CHECK(late < early, "and it dies away");
    // a larger size rings longer
    fx.init(mem.get());
    fx.cfg.reverbSize = 100;
    run(fx, true, false, 88200, l, r);
    const float lateBig = energy(l, 70000, 80000) + energy(r, 70000, 80000);
    CHECK(lateBig > late, "a larger size rings longer");
    float peak = 0;
    for (float v : l) if (std::fabs(v) > peak) peak = std::fabs(v);
    CHECK(peak < 2.0f, "the reverb at full size stays bounded");

    // delay: the echo arrives after the delay time, on the left first and then the right
    fx.init(mem.get());
    fx.cfg.reverbOn = false;
    fx.cfg.delayOn = true;
    fx.cfg.delayFrames = 10000;
    fx.cfg.delayFeedback = 50;
    fx.cfg.delayTone = 100;
    fx.cfg.delayReturn = 100;
    run(fx, false, true, 40000, l, r);
    CHECK(energy(l, 0, 9990) == 0.0f, "nothing before the delay time");
    CHECK(energy(l, 9990, 10010) > 0.5f && energy(r, 9990, 10010) < 0.01f, "the first echo is on the left");
    CHECK(energy(r, 19990, 20010) > 0.01f, "the second echo crosses to the right");
    CHECK(energy(r, 19990, 20010) < energy(l, 9990, 10010), "and is quieter");

    // the delay times
    CHECK(delayDivBeats(0) == 0.125f && delayDivBeats(kDelayDivs - 1) == 2.0f, "delay times run from 1/32 to 1/2");
    CHECK(delayDivName(5)[0] == '1' && delayDivName(5)[2] == '8', "the names are note values");

    // sends through a track bank: a track with no send adds nothing to the effects
    {
        std::vector<int16_t> data(44100, 8000);
        std::unique_ptr<TrackBank> bank(new TrackBank);
        bank->fx.init(mem.get());
        bank->fx.cfg.reverbOn = true;
        bank->fx.cfg.reverbReturn = 100;
        bank->data[0] = data.data();
        bank->loop = 44100;
        bank->cfg[0].levelDb10 = 0;
        std::vector<float> a(128), b(128), c(128), d(128);
        bank->render(0, 128, a.data(), b.data());
        bank->cfg[0].sendReverb = 100;
        bank->fx.init(mem.get());
        bank->fx.cfg.reverbOn = true;
        bank->fx.cfg.reverbReturn = 100;
        for (int blocks = 0; blocks < 40; blocks++) bank->render(blocks * 128, 128, c.data(), d.data());
        bank->render(40 * 128, 128, c.data(), d.data());
        bank->cfg[0].sendReverb = 0;
        bank->fx.init(mem.get());
        bank->fx.cfg.reverbOn = true;
        bank->fx.cfg.reverbReturn = 100;
        for (int blocks = 0; blocks < 40; blocks++) bank->render(blocks * 128, 128, a.data(), b.data());
        bank->render(40 * 128, 128, a.data(), b.data());
        CHECK(std::fabs(c[100] - a[100]) > 0.01f, "a send changes the mix");
    }

    // live filter
    {
        LiveFilterCfg cfg;
        LiveFilterState st;
        std::vector<float> sig(4096), sigL, sigR;
        for (size_t i = 0; i < sig.size(); i++) sig[i] = (i % 2) ? 0.5f : -0.5f; // the highest frequency there is
        sigL = sig; sigR = sig;
        liveFilterProcess(cfg, st, sigL.data(), sigR.data(), 4096);
        CHECK(energy(sigL, 0, 4096) == energy(sig, 0, 4096), "an inactive filter changes nothing");
        cfg.held = true;
        cfg.set = 10; // a closed low-pass
        sigL = sig; sigR = sig;
        for (int off = 0; off < 4096; off += 128) liveFilterProcess(cfg, st, sigL.data() + off, sigR.data() + off, 128);
        CHECK(energy(sigL, 2048, 4096) < 0.01f * energy(sig, 2048, 4096), "a low-pass takes the highs away");
        cfg.set = 50; // the middle of the sweep is open
        LiveFilterState st2;
        sigL = sig; sigR = sig;
        liveFilterProcess(cfg, st2, sigL.data(), sigR.data(), 4096);
        CHECK(energy(sigL, 0, 4096) == energy(sig, 0, 4096), "the middle of the sweep is open");
        cfg.set = 90; // a high-pass lets the highs through
        LiveFilterState st3;
        sigL = sig; sigR = sig;
        for (int off = 0; off < 4096; off += 128) liveFilterProcess(cfg, st3, sigL.data() + off, sigR.data() + off, 128);
        CHECK(energy(sigL, 2048, 4096) > 0.5f * energy(sig, 2048, 4096), "a high-pass keeps the highs");
        bool low = true, open = false;
        cfg.mode = 1; cfg.set = 100;
        liveFilterCutoff(cfg, &low, &open);
        CHECK(open, "a low-pass all the way up is open");
    }

    if (failures == 0) std::printf("all passed\n");
    return failures ? 1 : 0;
}
