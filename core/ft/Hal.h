#ifndef FT_HAL_H_
#define FT_HAL_H_

#include <stdint.h>

namespace ft
{
// Where a track records from. Each track has its own source.
enum class Source : uint8_t
{
    Line,
    MicLow,
    MicHigh,
    Pads
};

// Everything the application needs from the machine. The Tracker firmware implements it with the
// audio engine and the recorder; the web emulator with the browser's audio.
class Hal
{
  public:
    virtual ~Hal() {}

    virtual uint32_t sampleRate() const = 0;
    virtual uint32_t millis() = 0;
    virtual void tick() {} // called every pass of the main loop, for hardware housekeeping

    // Memory of one track: mono 16-bit samples, `regionFrames()` long.
    virtual int16_t* region(int track) = 0;
    virtual uint32_t regionFrames() const = 0;

    // Input
    virtual void applySource(Source source) = 0;
    virtual void inputLevels(float& left, float& right) = 0; // 0 to 1

    // Recording of one take into a track's memory
    virtual void recordStart(int track, Source source) = 0;
    virtual void recordStop() = 0;
    virtual uint32_t recordLength() = 0;      // frames recorded so far
    virtual bool recordActive() = 0;          // false once it has stopped, even by itself
    virtual bool recordFull() = 0;            // stopped because memory ran out
    virtual bool recordStartCaptured() = 0;   // the first block has arrived
    virtual uint32_t recordStartPosition() = 0; // loop position when the first block arrived

    // The looping player (all four tracks share one position)
    virtual void playerSetTrack(int track, const int16_t* data) = 0;
    virtual void playerStart(uint32_t frames) = 0;
    virtual void playerStop() = 0;
    virtual bool playerPlaying() = 0;
    virtual uint32_t playerPosition() = 0;
    virtual uint16_t playerTakePeak(int track) = 0; // largest sample since last call, 0 to 32767

    // Pads
    virtual void padOn(int pad, bool keysMode) = 0;
    virtual void padOff(int pad) = 0;
    virtual void padsReleaseAll() = 0;
};
} // namespace ft

#endif /* FT_HAL_H_ */
