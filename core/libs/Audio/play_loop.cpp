#include "play_loop.h"

#include "Audio.h"

void AudioPlayLoop::play(const int16_t* data, uint32_t lengthFrames)
{
    AudioNoInterrupts();
    loopData = data;
    loopLength = lengthFrames;
    position = 0;
    playing = (data != nullptr && lengthFrames > 0);
    AudioInterrupts();
}

void AudioPlayLoop::stop()
{
    AudioNoInterrupts();
    playing = false;
    AudioInterrupts();
}

void AudioPlayLoop::update(void)
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

    loopFillBlock(loopData, loopLength, position, block->data, AUDIO_BLOCK_SAMPLES);
    transmit(block, 0);
    release(block);
}
