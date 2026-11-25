#include "fourTrackHal.h"

#include "Platform.h"
#include "Recorder.h"
#include "mtAudioEngine.h"
#include "mtConfig.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include <stdio.h>
#include <string.h>

extern int16_t externalRamSampleMemory[];

static FourTrackHal* gHal = nullptr;

uint32_t FourTrackHal::sampleRate() const { return kFourTrackSampleRate; }

int16_t* FourTrackHal::region(int track)
{
    // Four regions at the very top of the sample memory, one per track.
    return externalRamSampleMemory +
           (SAMPLE_MEMORY_SIZE - (uint32_t)(track + 1) * kFourTrackRegionBytes) / sizeof(int16_t);
}

uint32_t FourTrackHal::regionFrames() const { return kFourTrackRegionFrames; }

void FourTrackHal::connectRecorder()
{
    gHal = this;
    memoryFull = false;
    startKnown = false;
    recorder.setFullMemoryCallback([]() { gHal->memoryFull = true; });
    recorder.setFirstBlockCallback([]() {
        gHal->startPosition = engine.getLoopPlayer()->position();
        gHal->startKnown = true;
    });
}

void FourTrackHal::tick()
{
    if ((writeHandle != nullptr || readHandle != nullptr) && fileIdle > 400)
    {
        closeFiles();
    }
    if (micUnmutePending && micTimer > 200)
    {
        micUnmutePending = false;
        platform->getCodecAudio()->setHeadphoneMuted(false);
    }
}

void FourTrackHal::restoreAudio()
{
    closeFiles();
    if (micUnmutePending)
    {
        micUnmutePending = false;
        platform->getCodecAudio()->setHeadphoneMuted(false);
    }
    engine.refreshLineInStreamVolume(); // back to the normal project setting
}

// Sets up the audio input for a source.
void FourTrackHal::applySource(ft::Source source)
{
    current = source;
    Sgtl5000* codec = platform->getCodecAudio();
    switch (source)
    {
        case ft::Source::Line:
            codec->setInputType(Sgtl5000::InputType::kLineIn);
            // Same input gain as the Sample Recorder screen (set it there; 0 to 100 percent).
            codec->setLineInLevel(mtProject.values.gainLineIn * 15 / 100);
            // Hear the input while recording, as the stock Sample Recorder does.
            engine.forceLineInStreamVolume(monitorOn ? DEFAULT_LINE_IN_VOLUME : 0);
            break;
        case ft::Source::MicLow:
        case ft::Source::MicHigh:
            // Mute the headphones briefly while switching, so there is no pop (as the stock
            // Sample Recorder does). Mic monitoring starts off.
            codec->setHeadphoneMuted(true);
            codec->setInputType(Sgtl5000::InputType::kMic);
            codec->setMicGain(source == ft::Source::MicLow ? mtProject.values.gainMicLow * 25.0f / 100.0f
                                                           : mtProject.values.gainMicHigh * 44.0f / 100.0f);
            micTimer = 0;
            micUnmutePending = true;
            engine.forceLineInStreamVolume(0);
            break;
        case ft::Source::Pads:
            // Records the pads, not the input: do not hear the line input meanwhile.
            engine.forceLineInStreamVolume(0);
            break;
    }
}

void FourTrackHal::inputLevels(float& left, float& right)
{
    static float l = 0.0f, r = 0.0f;
    AudioAnalyzeRMS* rmsL = engine.getAudioInput()->getRecordingRmsI2s(0);
    AudioAnalyzeRMS* rmsR = engine.getAudioInput()->getRecordingRmsI2s(1);
    if (rmsL != nullptr && rmsL->available()) l = rmsL->read();
    if (rmsR != nullptr && rmsR->available()) r = rmsR->read();
    left = l;
    right = r;
}

void FourTrackHal::recordStart(int track, ft::Source source)
{
    Recorder::Config config;
    config.destinationBufferAddress = region(track);
    config.destinationBufferSize = kFourTrackRegionBytes;
    config.destinationChannels = 1;
    config.sourceChannels = 2;
    // Both input channels, as the stock Sample Recorder does for a mono recording. The Config
    // setting "Line input" decides what ends up on the track: Stereo is summed to mono, Mono
    // Left and Mono Right use that channel.
    config.source = (source == ft::Source::Pads) ? Recorder::Config::Source::kPadBus
                                                 : Recorder::Config::Source::kI2s;
    recorder.setConfig(config);
    startKnown = false;
    memoryFull = false;
    recorder.record();
}

void FourTrackHal::recordStop() { recorder.stop(); }
uint32_t FourTrackHal::recordLength() { return recorder.getLength(); }
bool FourTrackHal::recordActive() { return recorder.isRecordingActive(); }

void FourTrackHal::playerSetTrack(int track, const int16_t* data) { engine.getLoopPlayer()->setTrack(track, data); }
void FourTrackHal::playerStart(uint32_t frames) { engine.getLoopPlayer()->start(frames); }
void FourTrackHal::playerStop() { engine.getLoopPlayer()->stop(); }
bool FourTrackHal::playerPlaying() { return engine.getLoopPlayer()->isPlaying(); }
uint32_t FourTrackHal::playerPosition() { return engine.getLoopPlayer()->position(); }
uint16_t FourTrackHal::playerTakePeak(int track) { return engine.getLoopPlayer()->takePeak(track); }

void FourTrackHal::padOn(int pad, bool keysMode)
{
    padsBacklight.setFrontLayer(1, mtConfig.values.padsLightFront, pad);

    uint8_t note;
    if (keysMode)
    {
        const bool isBeatSlice =
          mtProject.instrument[mtProject.values.lastUsedInstrument].playMode == playModeBeatSlice;
        note = mtPadBoard.getNoteFromPad(pad, isBeatSlice);
    }
    else
    {
        // Pad N plays sample slot N at its own pitch (the preview note).
        mtProject.values.lastUsedInstrument = pad;
        note = Sequencer::STEP_NOTE_DEFAULT;
    }
    padNote[pad] = note;
    sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, note, 127, pad);
}

void FourTrackHal::padOff(int pad)
{
    padsBacklight.setFrontLayer(0, 0, pad);
    sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, padNote[pad], 0, pad);
}

void FourTrackHal::padsReleaseAll()
{
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
}

//--------------------------------------------------------------------------------------------
// Sound

ft::TrackBank& FourTrackHal::bank() { return engine.getLoopPlayer()->tracks(); }
ft::Lofi& FourTrackHal::lofi() { return engine.getLoopPlayer()->master(); }

int FourTrackHal::gain(ft::Source source)
{
    switch (source)
    {
        case ft::Source::Line: return mtProject.values.gainLineIn;
        case ft::Source::MicLow: return mtProject.values.gainMicLow;
        case ft::Source::MicHigh: return mtProject.values.gainMicHigh;
        default: return 0;
    }
}

void FourTrackHal::applyGain(ft::Source source)
{
    Sgtl5000* codec = platform->getCodecAudio();
    if (source == ft::Source::Line) codec->setLineInLevel(mtProject.values.gainLineIn * 15 / 100);
    else if (source == ft::Source::MicLow) codec->setMicGain(mtProject.values.gainMicLow * 25.0f / 100.0f);
    else if (source == ft::Source::MicHigh) codec->setMicGain(mtProject.values.gainMicHigh * 44.0f / 100.0f);
}

void FourTrackHal::setGain(ft::Source source, int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (source == ft::Source::Line) mtProject.values.gainLineIn = percent;
    else if (source == ft::Source::MicLow) mtProject.values.gainMicLow = percent;
    else if (source == ft::Source::MicHigh) mtProject.values.gainMicHigh = percent;
    else return;
    if (source == current) applyGain(source);
}

void FourTrackHal::setMonitor(bool on)
{
    monitorOn = on;
    if (current == ft::Source::Line) engine.forceLineInStreamVolume(on ? DEFAULT_LINE_IN_VOLUME : 0);
}

void FourTrackHal::click(bool accent) { engine.makeMetronomeTick(accent ? 1 : 0); }

//--------------------------------------------------------------------------------------------
// Files: everything lives in the FourTrack folder on the SD card

void FourTrackHal::fullPath(const char* path, char* out, size_t size) { snprintf(out, size, "FourTrack/%s", path); }

bool FourTrackHal::sdPresent() { return platform->getFileSystem()->isSdCardInserted(); }

void FourTrackHal::closeFiles()
{
    FileSystem* fs = platform->getFileSystem();
    if (writeHandle != nullptr) { fs->closeFile(writeHandle); writeHandle = nullptr; }
    if (readHandle != nullptr) { fs->closeFile(readHandle); readHandle = nullptr; }
    openPath[0] = 0;
}

void FourTrackHal::ensureFolders()
{
    if (foldersReady) return;
    FileSystem* fs = platform->getFileSystem();
    static const char* const dirs[] = { "FourTrack", "FourTrack/Projects", "FourTrack/Exports", "FourTrack/undo" };
    for (const char* d : dirs)
        if (!fs->exists(d)) fs->mkdir(0, d);
    foldersReady = true;
}

bool FourTrackHal::fileWrite(const char* path, const void* data, uint32_t len, bool append)
{
    FileSystem* fs = platform->getFileSystem();
    if (!fs->isSdCardInserted()) return false;
    ensureFolders();
    char full[64];
    fullPath(path, full, sizeof full);
    if (readHandle != nullptr) { fs->closeFile(readHandle); readHandle = nullptr; }
    if (writeHandle != nullptr && (!append || strcmp(openPath, full) != 0))
    {
        fs->closeFile(writeHandle);
        writeHandle = nullptr;
    }
    if (writeHandle == nullptr)
    {
        auto info = fs->openFile(full, append ? FileSystem::kWriteAppend : FileSystem::kWriteCreateNew);
        if (!std::get<0>(info)) return false;
        writeHandle = std::get<1>(info);
        snprintf(openPath, sizeof openPath, "%s", full);
    }
    const bool ok = fs->writeFile(data, len, writeHandle) == len;
    fileIdle = 0;
    return ok;
}

uint32_t FourTrackHal::fileRead(const char* path, uint32_t offset, void* buf, uint32_t len)
{
    FileSystem* fs = platform->getFileSystem();
    if (!fs->isSdCardInserted()) return 0;
    char full[64];
    fullPath(path, full, sizeof full);
    if (writeHandle != nullptr) { fs->closeFile(writeHandle); writeHandle = nullptr; }
    if (readHandle != nullptr && strcmp(openPath, full) != 0)
    {
        fs->closeFile(readHandle);
        readHandle = nullptr;
    }
    if (readHandle == nullptr)
    {
        auto info = fs->openFile(full, FileSystem::kRead);
        if (!std::get<0>(info)) return 0;
        readHandle = std::get<1>(info);
        snprintf(openPath, sizeof openPath, "%s", full);
        readPosition = 0;
    }
    if (offset != readPosition)
    {
        if (!fs->seekFile(offset, readHandle)) return 0;
        readPosition = offset;
    }
    const size_t n = fs->readFile(buf, len, readHandle);
    readPosition += (uint32_t)n;
    fileIdle = 0;
    return (uint32_t)n;
}

uint32_t FourTrackHal::fileSize(const char* path)
{
    FileSystem* fs = platform->getFileSystem();
    closeFiles();
    char full[64];
    fullPath(path, full, sizeof full);
    return fs->exists(full) ? (uint32_t)fs->getFileSize(full) : 0;
}

bool FourTrackHal::fileRemove(const char* path)
{
    FileSystem* fs = platform->getFileSystem();
    closeFiles();
    char full[64];
    fullPath(path, full, sizeof full);
    return fs->remove(full);
}

int FourTrackHal::fileList(const char* dir, char (*names)[24], int maxNames)
{
    FileSystem* fs = platform->getFileSystem();
    closeFiles();
    if (!fs->isSdCardInserted()) return 0;
    ensureFolders();
    char full[64];
    fullPath(dir, full, sizeof full);
    static char buffer[16 * 28];
    char* list[16];
    int count = maxNames < 16 ? maxNames : 16;
    fs->listDirContents(full, FileSystem::kFiles, [](const char*) { return true; }, buffer, sizeof buffer, 28, count, list);
    int n = 0;
    for (int i = 0; i < count && n < maxNames; i++)
    {
        if (list[i] == nullptr || list[i][0] == 0) continue;
        snprintf(names[n++], 24, "%s", list[i]);
    }
    return n;
}

//--------------------------------------------------------------------------------------------
// The samples on the pads

int FourTrackHal::sampleCount()
{
    int n = 0;
    for (int i = 0; i < kSampleInstrumentsCount; i++)
        if (mtProject.instrument[i].isActive) n++;
    return n;
}

bool FourTrackHal::sampleName(int slot, char* out, int size)
{
    if (slot < 0 || slot >= kSampleInstrumentsCount || !mtProject.instrument[slot].isActive) return false;
    snprintf(out, size, "%s", mtProject.instrument[slot].sample.file_name);
    return true;
}

int FourTrackHal::currentSample() { return mtProject.values.lastUsedInstrument; }
void FourTrackHal::setCurrentSample(int slot)
{
    if (slot >= 0 && slot < kSampleInstrumentsCount) mtProject.values.lastUsedInstrument = slot;
}

void FourTrackHal::sampleMemory(uint32_t& usedBytes, uint32_t& totalBytes)
{
    usedBytes = mtProject.used_memory;
    totalBytes = kSampleMemoryLimit;
}
