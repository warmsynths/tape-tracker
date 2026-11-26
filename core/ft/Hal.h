#ifndef FT_HAL_H_
#define FT_HAL_H_

#include "dsp/Dsp.h"
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

// How one pad sample plays (the fields of a stock instrument that a pad needs)
struct PadSample
{
    char name[24];
    uint32_t frames;     // length in sample frames
    uint32_t bytes;      // size in memory
    int8_t tuneSt;       // semitones, -24 to +24
    int8_t tuneCt;       // cents, -100 to +100
    uint8_t volume;      // 0 to 100
    int8_t pan;          // -100 (left) to +100 (right)
    bool loop;           // false: one-shot
    uint16_t start;      // 0 to 65535 along the sample
    uint16_t end;
};

// One line of an SD card folder listing (Load samples)
struct BrowseEntry
{
    char name[28];
    bool dir;
    uint32_t bytes;   // file size (files)
    uint32_t count;   // WAV files inside (folders)
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

    // The looping player (all four tracks share one position). The application sets up the
    // tracks (offsets, trims, glitch effects) and the lo-fi master directly in `bank()` and
    // `lofi()`; the audio side renders from them.
    virtual void playerSetTrack(int track, const int16_t* data) = 0;
    virtual void playerStart(uint32_t frames) = 0;
    virtual void playerStop() = 0;
    virtual bool playerPlaying() = 0;
    virtual uint32_t playerPosition() = 0;
    virtual uint16_t playerTakePeak(int track) = 0; // largest sample since last call, 0 to 32767
    virtual TrackBank& bank() = 0;
    virtual Lofi& lofi() = 0;

    // Input gain, 0 to 100 percent, per source; and monitoring of the live input
    virtual int gain(Source source) = 0;
    virtual void setGain(Source source, int percent) = 0;
    virtual void setMonitor(bool on) = 0;
    virtual int monitorReductionDb() = 0; // 0, or negative while feedback protection acts

    // Clicks for the metronome and the count-in
    virtual void click(bool accent) = 0;

    // Files (the SD card on the Tracker, browser storage in the emulator). Paths use '/'.
    virtual bool sdPresent() = 0;
    virtual bool fileWrite(const char* path, const void* data, uint32_t len, bool append) = 0;
    virtual uint32_t fileRead(const char* path, uint32_t offset, void* buf, uint32_t len) = 0;
    virtual uint32_t fileSize(const char* path) = 0; // 0 if it does not exist
    virtual bool fileRemove(const char* path) = 0;
    virtual int fileList(const char* dir, char (*names)[24], int maxNames) = 0; // file names only

    // The samples on the pads
    virtual int sampleCount() = 0;                       // loaded samples
    virtual bool sampleName(int slot, char* out, int size) = 0; // false if the slot is empty
    virtual int currentSample() = 0;                     // the sample Keys mode plays, -1 if none
    virtual void setCurrentSample(int slot) = 0;
    virtual void sampleMemory(uint32_t& usedBytes, uint32_t& totalBytes) = 0;

    // One pad sample's settings, its picture, and playing it for a moment
    virtual bool padSample(int slot, PadSample& out) = 0;      // false if the slot is empty
    virtual void setPadSample(int slot, const PadSample& in) = 0;
    virtual int padSampleWave(int slot, uint8_t* out, int columns) = 0; // peaks 0..255, returns columns (0 if empty)
    virtual void padAudition(int slot, bool on) = 0;

    // The SD card's samples. Paths start with '/' and run from the card's root ("/Samples/Drums").
    virtual int browse(const char* dir, BrowseEntry* out, int max) = 0;   // folders first, then .wav files, each sorted by name
    virtual uint32_t rawRead(const char* path, uint32_t offset, void* buf, uint32_t len) = 0;
    virtual bool loadSampleFile(const char* path, int slot) = 0;          // false: not readable or no memory left
    virtual void previewFile(const char* path, bool on) = 0;
    virtual bool previewActive() = 0;

    // Pads
    virtual void padOn(int pad, bool keysMode) = 0;
    virtual void padOff(int pad) = 0;
    virtual void padsReleaseAll() = 0;
};
} // namespace ft

#endif /* FT_HAL_H_ */
