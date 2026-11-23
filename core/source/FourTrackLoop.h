#ifndef FOURTRACKLOOP_H_
#define FOURTRACKLOOP_H_

#include <stdint.h>

// Pure state machine for the first test build: one mono Part that is recorded once and looped.
// No hardware dependencies, so it is tested on a PC (tests/host/four_track_loop_test.cpp).
class FourTrackLoop
{
  public:
    enum class State : uint8_t
    {
        Empty,
        Recording,
        Ready,
        Playing
    };

    explicit FourTrackLoop(uint32_t maxFrames)
      : maxFrames(maxFrames)
    {
    }

    State state() const { return currentState; }
    uint32_t lengthFrames() const { return frames; }
    bool memoryFull() const { return full; }

    // Starts a new recording. Discards any earlier Part. Ignored while already recording.
    void pressRecord();
    // Called while recording with the number of frames captured so far.
    // Stops by itself, keeping what was recorded, at the cap or when memoryFull is true.
    void recordedFrames(uint32_t capturedFrames, bool memoryFull);
    // Recording: keeps the take (or stays empty if nothing was captured). Playing: back to Ready.
    void pressStop();
    // Ready: starts looping. Ignored in every other state.
    void pressPlay();

  private:
    void finishRecording();

    uint32_t maxFrames;
    uint32_t frames = 0;
    bool full = false;
    State currentState = State::Empty;
};

#endif /* FOURTRACKLOOP_H_ */
