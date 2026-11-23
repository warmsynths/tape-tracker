#include "play_loops.h"

#include "Audio.h"

void AudioPlayLoops::setTrack(int track, const int16_t* data)
{
    if (track < 0 || track >= kTracks)
    {
        return;
    }
    AudioNoInterrupts();
    trackData[track] = data;
    AudioInterrupts();
}

void AudioPlayLoops::setMute(int track, bool mute)
{
    if (track < 0 || track >= kTracks)
    {
        return;
    }
    muted[track] = mute;
}

void AudioPlayLoops::start(uint32_t lengthFrames)
{
    AudioNoInterrupts();
    loopLength = lengthFrames;
    currentPosition = 0;
    playing = (lengthFrames > 0);
    AudioInterrupts();
}

void AudioPlayLoops::stop()
{
    AudioNoInterrupts();
    playing = false;
    AudioInterrupts();
}

void AudioPlayLoops::update(void)
{
    if (!playing)
    {
        return;
    }

    audio_block_t* block = allocate();
    if (block == nullptr)
    {
        return;
    }

    const int16_t* active[kTracks];
    for (int t = 0; t < kTracks; t++)
    {
        active[t] = muted[t] ? nullptr : trackData[t];
    }

    const uint32_t length = loopLength;
    loopMixBlock(active, kTracks, length, currentPosition, block->data, AUDIO_BLOCK_SAMPLES);

    if (length > 0)
    {
        currentPosition = (currentPosition + AUDIO_BLOCK_SAMPLES) % length;
    }

    transmit(block, 0);
    release(block);
}
