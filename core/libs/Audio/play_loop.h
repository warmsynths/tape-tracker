#ifndef play_loop_h_
#define play_loop_h_

#include "AudioStream.h"
#include "loop_fill.h"

// Plays one mono 16-bit region of memory in a seamless loop. Used by the four-track
// test build to play back a recorded Part. Output 0 carries the mono signal.
class AudioPlayLoop : public AudioStream
{
  public:
    AudioPlayLoop()
      : AudioStream(0, NULL)
    {
    }

    // Starts looping `lengthFrames` samples at `data` from the beginning.
    void play(const int16_t* data, uint32_t lengthFrames);
    void stop();
    bool isPlaying() const { return playing; }

    virtual void update(void) override;

  private:
    const int16_t* volatile loopData = nullptr;
    volatile uint32_t loopLength = 0;
    uint32_t position = 0;
    volatile bool playing = false;
};

#endif
