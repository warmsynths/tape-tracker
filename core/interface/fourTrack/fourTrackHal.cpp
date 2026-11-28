#include "fourTrackHal.h"
#include "cr_section_macros.h"

#include "Platform.h"
#include "fileManager.h"
#include "configEditor/configMenuActions.h"
#include "display.h"
#include "Mask.h"
#include "core/interfaceDefs.h"
#include "mtConfig.h"
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
            engine.forceLineInStreamVolume(monitorOn ? DEFAULT_LINE_IN_VOLUME : 0);
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
// Reverb and delay memory (about 300 KB, see core/ft/dsp/SendFx.h)
__NOINIT(EXTERNAL_RAM) static ft::SendFxMemory fxMemoryStore;
ft::SendFxMemory* FourTrackHal::fxMemory() { return &fxMemoryStore; }
void FourTrackHal::playerStop() { engine.getLoopPlayer()->stop(); }
void FourTrackHal::playerSeek(uint32_t frames) { engine.getLoopPlayer()->seek(frames); }
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

// The clicks for the latency test sit in external RAM (88 KB)
__NOINIT(EXTERNAL_RAM) int16_t calibrationClicks[44100];
int16_t* FourTrackHal::calibrationBuffer(uint32_t& frames)
{
    frames = 44100;
    return calibrationClicks;
}

// Lights from the chord pads sit on the back layer: the front layer is the stock hit light
void FourTrackHal::padLight(int pad, int level)
{
    if (pad < 0 || pad >= 48) return;
    padsBacklight.setBackLayer(level > 0 ? 1 : 0, (uint8_t)(level > 31 ? 31 : level), (uint8_t)pad);
}

// The chord pads: the synth, or the pad sample pitched (the sample's own pitch is note 60)
void FourTrackHal::synthSetPatch(const ft::SynthPatch& patch) { engine.getSynthPlayer()->setPatch(patch); }
void FourTrackHal::synthNote(int note, int velocity, bool on)
{
    if (on) engine.getSynthPlayer()->noteOn(note, velocity);
    else engine.getSynthPlayer()->noteOff(note);
}
void FourTrackHal::synthSetLevelDb10(int db10) { engine.getSynthPlayer()->setLevelDb10(db10); }
void FourTrackHal::synthAllOff() { engine.getSynthPlayer()->allNotesOff(); }
void FourTrackHal::synthDrum(int voice, int velocity) { engine.getSynthPlayer()->drum(voice, velocity); }
void FourTrackHal::synthMod(float bend, float vibrato) { engine.getSynthPlayer()->setMod(bend, vibrato); }
int FourTrackHal::synthVoices() { return engine.getSynthPlayer()->voiceLimit(); }
int FourTrackHal::synthLoad() { return engine.getSynthPlayer()->loadPercent(); }
void FourTrackHal::synthSetKit(int kit) { engine.getSynthPlayer()->setKit(kit); }
void FourTrackHal::sampleNote(int id, int note, int velocity, bool on)
{
    if (on) sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, (byte)note, (int8_t)(velocity < 1 ? 1 : velocity > 127 ? 127 : velocity), (int16_t)id);
    else sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, (byte)note, 0, (int16_t)id);
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
    if (current != ft::Source::Pads) engine.forceLineInStreamVolume(on ? DEFAULT_LINE_IN_VOLUME : 0);
}

void FourTrackHal::click(bool accent) { engine.makeMetronomeTick(accent ? 1 : 0); }

//--------------------------------------------------------------------------------------------
// Files: everything lives in the FourTrack folder on the SD card

// Paths that start with '/' run from the card's root (the samples); the others are under FourTrack/.
void FourTrackHal::fullPath(const char* path, char* out, size_t size)
{
    if (path[0] == '/') snprintf(out, size, "%s", path + 1);
    else snprintf(out, size, "FourTrack/%s", path);
}

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
    if (path[0] == '/' && !append)
    {
        // a file on the card outside FourTrack/: make its folder first
        char parent[64];
        snprintf(parent, sizeof parent, "%s", full);
        char* cut = strrchr(parent, '/');
        if (cut != nullptr) { *cut = 0; if (!fs->exists(parent)) fs->mkdir(0, parent, true); }
    }
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
    fs->listDirContents(full, FileSystem::kFiles, [](const char*) { return true; }, buffer, sizeof buffer, 48, count, list);
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

//--------------------------------------------------------------------------------------------
// One pad sample's settings: the fields of the stock instrument that a pad needs

bool FourTrackHal::padSample(int slot, ft::PadSample& out)
{
    if (slot < 0 || slot >= kSampleInstrumentsCount || !mtProject.instrument[slot].isActive) return false;
    const strInstrument& in = mtProject.instrument[slot];
    snprintf(out.name, sizeof out.name, "%s", in.sample.file_name);
    out.frames = in.sample.length;
    out.bytes = in.sample.length * 2;
    out.tuneSt = in.tune;
    out.tuneCt = in.fineTune;
    out.volume = in.volume;
    out.pan = (int8_t)((in.panning - 50) * 2);
    out.loop = in.playMode == playModeLoopForward || in.playMode == playModeLoopBackward || in.playMode == playModePingpong;
    out.start = in.startPoint;
    out.end = in.endPoint;
    return true;
}

void FourTrackHal::setPadSample(int slot, const ft::PadSample& s)
{
    if (slot < 0 || slot >= kSampleInstrumentsCount || !mtProject.instrument[slot].isActive) return;
    strInstrument& in = mtProject.instrument[slot];
    in.tune = s.tuneSt;
    in.fineTune = s.tuneCt;
    in.volume = s.volume;
    in.panning = (int16_t)(s.pan / 2 + 50);
    in.startPoint = s.start;
    in.endPoint = s.end;
    in.loopPoint1 = s.start;
    in.loopPoint2 = s.end;
    in.playMode = s.loop ? playModeLoopForward : playModeSingleShot;
}

int FourTrackHal::padSampleWave(int slot, uint8_t* out, int columns)
{
    if (slot < 0 || slot >= kSampleInstrumentsCount || !mtProject.instrument[slot].isActive) return 0;
    const strInstrument& in = mtProject.instrument[slot];
    const int16_t* data = in.sample.address;
    const uint32_t length = in.sample.length;
    if (data == nullptr || length == 0) return 0;
    for (int c = 0; c < columns; c++)
    {
        const uint32_t from = (uint32_t)(((uint64_t)c * length) / columns);
        const uint32_t to = (uint32_t)(((uint64_t)(c + 1) * length) / columns);
        const uint32_t step = (to - from) > 256 ? (to - from) / 256 : 1;
        int32_t peak = 0;
        for (uint32_t i = from; i < to; i += step)
        {
            const int32_t v = data[i] < 0 ? -(int32_t)data[i] : data[i];
            if (v > peak) peak = v;
        }
        out[c] = (uint8_t)(__builtin_sqrtf((float)peak / 32768.0f) * 255.0f);
    }
    return columns;
}

void FourTrackHal::padAudition(int slot, bool on)
{
    if (slot < 0 || slot >= 48) return;
    if (on) padOn(slot, false);
    else padOff(slot);
}

//--------------------------------------------------------------------------------------------
// The SD card's samples (Load samples)

static bool isWavName(const char* n)
{
    const size_t l = strlen(n);
    return l > 4 && n[0] != '.' && strcasecmp(n + l - 4, ".wav") == 0;
}

uint32_t FourTrackHal::rawRead(const char* path, uint32_t offset, void* buf, uint32_t len) { return fileRead(path, offset, buf, len); }

int FourTrackHal::browse(const char* dir, ft::BrowseEntry* out, int max)
{
    FileSystem* fs = platform->getFileSystem();
    closeFiles();
    if (!fs->isSdCardInserted()) return 0;
    char full[96];
    fullPath(dir, full, sizeof full);
    if (full[0] == 0) snprintf(full, sizeof full, "/");
    static char buffer[48 * 48];
    char* list[48];
    int n = 0;
    // folders first
    int count = max < 48 ? max : 48;
    fs->listDirContents(full, FileSystem::kFolders, [](const char* name) { return name[0] != '.'; }, buffer, sizeof buffer, 48, count, list);
    for (int i = 0; i < count && n < max; i++)
    {
        if (list[i] == nullptr || list[i][0] == 0) continue;
        snprintf(out[n].name, sizeof out[n].name, "%s", list[i]);
        out[n].dir = true;
        out[n].bytes = 0;
        out[n].count = 0;
        n++;
    }
    const int folders = n;
    count = (max - n) < 48 ? (max - n) : 48;
    if (count > 0)
    {
        fs->listDirContents(full, FileSystem::kFiles, isWavName, buffer, sizeof buffer, 48, count, list);
        for (int i = 0; i < count && n < max; i++)
        {
            if (list[i] == nullptr || list[i][0] == 0) continue;
            snprintf(out[n].name, sizeof out[n].name, "%s", list[i]);
            out[n].dir = false;
            char path[128];
            snprintf(path, sizeof path, "%s/%s", full, list[i]);
            out[n].bytes = (uint32_t)fs->getFileSize(path);
            out[n].count = 0;
            n++;
        }
    }
    // how many WAV files each folder holds
    for (int i = 0; i < folders; i++)
    {
        char sub[128];
        snprintf(sub, sizeof sub, "%s/%s", full, out[i].name);
        int c = 48;
        fs->listDirContents(sub, FileSystem::kFiles, isWavName, buffer, sizeof buffer, 48, c, list);
        out[i].count = c > 0 ? (uint32_t)c : 0;
    }
    // sorted by name inside each group
    auto before = [](const ft::BrowseEntry& a, const ft::BrowseEntry& b) { return strcasecmp(a.name, b.name) < 0; };
    for (int a = 1; a < n; a++)
        for (int b = a; b > 0 && out[b].dir == out[b - 1].dir && before(out[b], out[b - 1]); b--)
        {
            const ft::BrowseEntry t = out[b]; out[b] = out[b - 1]; out[b - 1] = t;
        }
    return n;
}

// The stock importer reads a file named by the explorer; it runs in the main loop's update, so
// this waits for it to finish.
bool FourTrackHal::loadSampleFile(const char* path, int slot)
{
    if (slot < 0 || slot >= kSampleInstrumentsCount) return false;
    closeFiles();
    previewFile("", false);
    char folder[128], name[64];
    const char* cut = strrchr(path, '/');
    if (cut == nullptr) return false;
    snprintf(folder, sizeof folder, "%.*s", (int)(cut - path) > 0 ? (int)(cut - path) : 1, path);
    snprintf(name, sizeof name, "%s", cut + 1);
    const size_t l = strlen(name);
    if (l > 4) name[l - 4] = 0; // the importer adds ".wav"
    char* f = folder[0] == '/' && folder[1] != 0 ? folder + 1 : folder;
    if (!newFileManager.importSampleToProject(f, name, (uint8_t)slot)) return false;
    for (int i = 0; i < 20000 && newFileManager.getStatus() != fmIdle; i++)
    {
        newFileManager.update();
        engine.updateSdStream();
    }
    return mtProject.instrument[slot].isActive;
}

void FourTrackHal::previewFile(const char* path, bool on)
{
    if (!on)
    {
        newFileManager.stopPrevievSamplefromSD();
        return;
    }
    const char* cut = strrchr(path, '/');
    if (cut == nullptr) return;
    char* names = newFileManager.getExplorerList()[0];
    delete[] names;
    newFileManager.getExplorerList()[0] = new char[strlen(cut + 1) + 1];
    strcpy(newFileManager.getExplorerList()[0], cut + 1);
    snprintf(newFileManager.getExplorerPath(), 255, "%.*s", (int)(cut - path) > 0 ? (int)(cut - path) - 0 : 1, path[0] == '/' ? path + 1 : path);
    newFileManager.previevSamplefromSD(0, 0, 65535);
}

bool FourTrackHal::previewActive() { return newFileManager.getStatus() == fmPreviewSampleFromSd; }

//--------------------------------------------------------------------------------------------
// Device settings: the same values the stock Config screen edits, so nothing else changes

int FourTrackHal::deviceValue(ft::Dev d)
{
    switch (d)
    {
        case ft::Dev::MidiIn: return mtConfig.midi.notesInMode;
        case ft::Dev::MidiInChannel: return mtConfig.midi.notesInChannel;
        case ft::Dev::MidiOut: return mtConfig.midi.notesOutMode;
        case ft::Dev::UsbAudio: return mtConfig.common.usbAudioMode;
        case ft::Dev::LineInput: return mtConfig.general.lineInOutType;
        case ft::Dev::Volume: return mtConfig.audioCodecConfig.volume;
        case ft::Dev::Brightness: return mtConfig.general.dispBrightness;
    }
    return 0;
}

void FourTrackHal::setDeviceValue(ft::Dev d, int v)
{
    switch (d)
    {
        case ft::Dev::MidiIn: mtConfig.midi.notesInMode = (uint8_t)v; break;
        case ft::Dev::MidiInChannel: mtConfig.midi.notesInChannel = (uint8_t)v; break;
        case ft::Dev::MidiOut: mtConfig.midi.notesOutMode = (uint8_t)v; break;
        case ft::Dev::UsbAudio: mtConfig.common.usbAudioMode = (uint8_t)v; usbAudioChange(); break;
        case ft::Dev::LineInput: mtConfig.general.lineInOutType = (uint8_t)v; setLineInOutType(); break;
        case ft::Dev::Volume: mtConfig.audioCodecConfig.volume = (uint8_t)v; engine.setHeadphonesVolume(mtConfig.audioCodecConfig.volume); break;
        case ft::Dev::Brightness: mtConfig.general.dispBrightness = (uint8_t)v; setDisplayBrightness(); break;
    }
    saveConfigAsap();
}

void FourTrackHal::firmwareVersion(char* out, int size) { snprintf(out, size, "%s", interfaceGlobals.currentFirmwareVersion); }

int FourTrackHal::firmwareFiles(char (*names)[48], int max)
{
    FileSystem* fs = platform->getFileSystem();
    closeFiles();
    if (!fs->isSdCardInserted()) return 0;
    static char buffer[8 * 48];
    char* list[8];
    int count = max < 8 ? max : 8;
    fs->listDirContents("Firmware", FileSystem::kFiles, [](const char* n) { const size_t l = strlen(n); return l >= 15 && strncmp(n, "PolyendTrac", 11) == 0 && strcasecmp(n + l - 4, ".ptf") == 0; }, buffer, sizeof buffer, 48, count, list);
    int n = 0;
    for (int i = 0; i < count && n < max; i++)
    {
        if (list[i] == nullptr || list[i][0] == 0) continue;
        snprintf(names[n++], 48, "%s", list[i]);
    }
    return n;
}

// The stock updater: the bootloader takes over and installs the file from the Firmware folder.
bool FourTrackHal::firmwareInstall(const char* name)
{
    static char file[32];
    snprintf(file, sizeof file, "%s", name);
    platform->runBootloader(file);
    return true;
}

// A band of a waveform bitmap into the display chip's graphics memory (see Mask.h)
void FourTrackHal::maskWrite(int slot, uint32_t byteOffset, const uint8_t* data, uint32_t len)
{
    if (slot < 0 || slot > ft::kArtLastSlot) return;
    const uint32_t addr = ft::maskAddress(slot) + byteOffset;
    display.getDisplayDriver()->writeDataRAMG(data, len, addr);
}
