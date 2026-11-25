#ifndef INTERFACE_FOURTRACK_FOURTRACKHAL_H_
#define INTERFACE_FOURTRACK_FOURTRACKHAL_H_

#include "Hal.h"
#include "elapsedMillis.h"

class Platform;

// The Tracker's side of the Four-Track application (core/ft): recording through the Recorder,
// playing through the audio engine's loop player, the codec for the input source, and the pads
// through the sequencer. The web emulator provides the same interface with the browser's audio.
class FourTrackHal : public ft::Hal
{
  public:
    explicit FourTrackHal(Platform*& platformRef)
      : platform(platformRef)
    {
    }

    uint32_t sampleRate() const override;
    uint32_t millis() override { return (uint32_t)clock; }
    void tick() override;

    int16_t* region(int track) override;
    uint32_t regionFrames() const override;

    void applySource(ft::Source source) override;
    void inputLevels(float& left, float& right) override;

    void recordStart(int track, ft::Source source) override;
    void recordStop() override;
    uint32_t recordLength() override;
    bool recordActive() override;
    bool recordFull() override { return memoryFull; }
    bool recordStartCaptured() override { return startKnown; }
    uint32_t recordStartPosition() override { return startPosition; }

    void playerSetTrack(int track, const int16_t* data) override;
    void playerStart(uint32_t frames) override;
    void playerStop() override;
    bool playerPlaying() override;
    uint32_t playerPosition() override;
    uint16_t playerTakePeak(int track) override;

    void padOn(int pad, bool keysMode) override;
    void padOff(int pad) override;
    void padsReleaseAll() override;

    // Called when the screen is closed: put the codec and monitor back as they were.
    void restoreAudio();
    // Sets the Recorder's callbacks; called when the screen opens.
    void connectRecorder();

    volatile uint32_t startPosition = 0;
    volatile bool startKnown = false;
    volatile bool memoryFull = false;

  private:
    Platform*& platform;
    elapsedMillis clock;
    elapsedMillis micTimer;
    bool micUnmutePending = false;
    uint8_t padNote[48]{};
};

#endif /* INTERFACE_FOURTRACK_FOURTRACKHAL_H_ */
