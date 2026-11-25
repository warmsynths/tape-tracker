#ifndef play_loops_h_
#define play_loops_h_

#include "AudioStream.h"
#include "dsp/Dsp.h"

// Plays up to four mono 16-bit regions of memory as looping tracks that share one loop length
// and one position, so they stay in sync. The tracks go through the shared four-track renderer
// (offsets, trims, glitch effects, pan) and the lo-fi master (core/ft/dsp). Output 0 is the left
// channel, output 1 the right.
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

    // The settings the application changes while playing (see core/ft/dsp/Dsp.h)
    ft::TrackBank& tracks() { return bank; }
    ft::Lofi& master() { return lofi; }

    // Starts all tracks from the beginning, each `lengthFrames` long.
    void start(uint32_t lengthFrames);
    void stop();
    bool isPlaying() const { return playing; }

    // Frame position where the next block starts, and the loop length.
    uint32_t position() const { return currentPosition; }
    uint32_t length() const { return loopLength; }

    // Largest sample of each track since the last call (0 to 32767), then cleared.
    uint16_t takePeak(int track)
    {
        if (track < 0 || track >= kTracks) return 0;
        const uint16_t p = bank.peak[track];
        bank.peak[track] = 0;
        return p;
    }

    virtual void update(void) override;

  private:
    ft::TrackBank bank;
    ft::Lofi lofi;
    volatile uint32_t loopLength = 0;
    volatile uint32_t currentPosition = 0;
    volatile bool playing = false;
};

#endif
