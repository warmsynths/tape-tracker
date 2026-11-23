#ifndef play_loops_h_
#define play_loops_h_

#include "AudioStream.h"
#include "loop_fill.h"

// Plays up to four mono 16-bit regions of memory as looping tracks that share one loop length
// and one position, so they stay in sync. Output 0 carries the mono mix.
class AudioPlayLoops : public AudioStream
{
  public:
    static constexpr int kTracks = 4;

    AudioPlayLoops()
      : AudioStream(0, NULL)
    {
    }

    // `data` is the start of the track's samples, or nullptr for an empty track.
    void setTrack(int track, const int16_t* data);
    void setMute(int track, bool mute);

    // Starts all tracks from the beginning, each `lengthFrames` long.
    void start(uint32_t lengthFrames);
    void stop();
    bool isPlaying() const { return playing; }

    // Frame position where the next block starts, and the loop length.
    uint32_t position() const { return currentPosition; }
    uint32_t length() const { return loopLength; }

    virtual void update(void) override;

  private:
    const int16_t* volatile trackData[kTracks] = {};
    volatile bool muted[kTracks] = {};
    volatile uint32_t loopLength = 0;
    volatile uint32_t currentPosition = 0;
    volatile bool playing = false;
};

#endif
