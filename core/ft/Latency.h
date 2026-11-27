#ifndef FT_LATENCY_H_
#define FT_LATENCY_H_

#include <stdint.h>

namespace ft
{
// Measuring the input latency with a cable from the Tracker's output to its line input. A one second
// loop with a few sharp clicks is played through the normal loop player while the line input is recorded;
// the gap between where each click was played and where it shows up in the recording is the latency.
// Everything is in frames, and relative to the loop position the recorder latched at its first block,
// the same way a normal Part is placed, so the result is what a recorded Part needs to be moved by.

struct LatencyResult
{
    bool ok = false;
    int32_t frames = 0;   // the median delay
    int32_t spread = 0;   // the largest minus the smallest delay seen
    int count = 0;        // how many clicks were found
    int why = 0;          // when not ok: 1 no signal, 2 clicks do not agree, 3 out of range, 4 too few clicks
};

constexpr uint32_t kLatencyLoopFrames = 44100;
constexpr uint32_t kLatencyPulses = 2; // clicks per loop

// Fills `buf` (loopLen frames) with `pulses` clicks, evenly spaced from frame 0.
void makeLatencyClicks(int16_t* buf, uint32_t loopLen, uint32_t pulses);
// `startPos` is the loop position when the first recorded block arrived.
LatencyResult measureLatency(const int16_t* rec, uint32_t frames, uint32_t startPos, uint32_t loopLen, uint32_t pulses);
} // namespace ft

#endif /* FT_LATENCY_H_ */
