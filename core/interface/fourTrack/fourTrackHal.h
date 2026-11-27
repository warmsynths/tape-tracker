#ifndef INTERFACE_FOURTRACK_FOURTRACKHAL_H_
#define INTERFACE_FOURTRACK_FOURTRACKHAL_H_

#include "Hal.h"
#include "elapsedMillis.h"
#include <stddef.h>

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
    ft::SendFxMemory* fxMemory() override;
    void playerSeek(uint32_t frames) override;
    bool playerPlaying() override;
    uint32_t playerPosition() override;
    uint16_t playerTakePeak(int track) override;

    ft::TrackBank& bank() override;
    ft::Lofi& lofi() override;

    int gain(ft::Source source) override;
    void setGain(ft::Source source, int percent) override;
    void setMonitor(bool on) override;
    int monitorReductionDb() override { return 0; }

    void click(bool accent) override;

    bool sdPresent() override;
    bool fileWrite(const char* path, const void* data, uint32_t len, bool append) override;
    uint32_t fileRead(const char* path, uint32_t offset, void* buf, uint32_t len) override;
    uint32_t fileSize(const char* path) override;
    bool fileRemove(const char* path) override;
    int fileList(const char* dir, char (*names)[24], int maxNames) override;

    int sampleCount() override;
    bool sampleName(int slot, char* out, int size) override;
    int currentSample() override;
    void setCurrentSample(int slot) override;
    void sampleMemory(uint32_t& usedBytes, uint32_t& totalBytes) override;

    int browse(const char* dir, ft::BrowseEntry* out, int max) override;
    uint32_t rawRead(const char* path, uint32_t offset, void* buf, uint32_t len) override;
    bool loadSampleFile(const char* path, int slot) override;
    void previewFile(const char* path, bool on) override;
    bool previewActive() override;

    bool maskDefault() override { return true; } // tried on a Tracker: works
    bool startsAtSetTempo() override { return true; }
    void maskWrite(int slot, uint32_t byteOffset, const uint8_t* data, uint32_t len) override;
    int deviceValue(ft::Dev d) override;
    void setDeviceValue(ft::Dev d, int value) override;
    void firmwareVersion(char* out, int size) override;
    int firmwareFiles(char (*names)[48], int max) override;
    bool firmwareInstall(const char* name) override;
    int firmwareProgress() override { return -1; }

    bool padSample(int slot, ft::PadSample& out) override;
    void setPadSample(int slot, const ft::PadSample& in) override;
    int padSampleWave(int slot, uint8_t* out, int columns) override;
    void padAudition(int slot, bool on) override;

    void padOn(int pad, bool keysMode) override;
    void padOff(int pad) override;
    void padsReleaseAll() override;
    void padLight(int pad, int level) override;
    int16_t* calibrationBuffer(uint32_t& frames) override;
    void synthSetPatch(const ft::SynthPatch& patch) override;
    void synthNote(int note, int velocity, bool on) override;
    void synthAllOff() override;
    void synthDrum(int voice, int velocity) override;
    void synthSetKit(int kit) override;
    void synthMod(float bend, float vibrato) override;
    void synthSetLevelDb10(int db10) override;
    void sampleNote(int id, int note, int velocity, bool on) override;

    // Called when the screen is closed: put the codec and monitor back as they were.
    void restoreAudio();
    // Sets the Recorder's callbacks; called when the screen opens.
    void connectRecorder();

    volatile uint32_t startPosition = 0;
    volatile bool startKnown = false;
    volatile bool memoryFull = false;

  private:
    void applyGain(ft::Source source);
    void closeFiles();
    void ensureFolders();
    static void fullPath(const char* path, char* out, size_t size);

    Platform*& platform;
    ft::Source current = ft::Source::Line;
    bool monitorOn = true;
    // The file being written or read stays open between calls, because a project is saved and
    // loaded in many small pieces.
    void* writeHandle = nullptr;
    void* readHandle = nullptr;
    char openPath[48] = {};
    uint32_t readPosition = 0;
    elapsedMillis fileIdle;
    bool foldersReady = false;
    elapsedMillis clock;
    elapsedMillis micTimer;
    bool micUnmutePending = false;
    uint8_t padNote[48]{};
};

#endif /* INTERFACE_FOURTRACK_FOURTRACKHAL_H_ */
