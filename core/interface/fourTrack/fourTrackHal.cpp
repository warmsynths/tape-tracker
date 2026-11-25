#include "fourTrackHal.h"

#include "Platform.h"
#include "Recorder.h"
#include "mtAudioEngine.h"
#include "mtConfig.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "mtSequencer.h"
#include "mtStructs.h"

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
    if (micUnmutePending && micTimer > 200)
    {
        micUnmutePending = false;
        platform->getCodecAudio()->setHeadphoneMuted(false);
    }
}

void FourTrackHal::restoreAudio()
{
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
    Sgtl5000* codec = platform->getCodecAudio();
    switch (source)
    {
        case ft::Source::Line:
            codec->setInputType(Sgtl5000::InputType::kLineIn);
            // Same input gain as the Sample Recorder screen (set it there; 0 to 100 percent).
            codec->setLineInLevel(mtProject.values.gainLineIn * 15 / 100);
            // Hear the input while recording, as the stock Sample Recorder does.
            engine.forceLineInStreamVolume(DEFAULT_LINE_IN_VOLUME);
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
