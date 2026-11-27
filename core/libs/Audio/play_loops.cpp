#include "play_loops.h"

#include "Audio.h"

void AudioPlayLoops::setTrack(int track, const int16_t* data)
{
    if (track < 0 || track >= kTracks)
    {
        return;
    }
    AudioNoInterrupts();
    bank.data[track] = data;
    AudioInterrupts();
}

void AudioPlayLoops::start(uint32_t lengthFrames)
{
    AudioNoInterrupts();
    loopLength = lengthFrames;
    bank.loop = lengthFrames;
    bank.resetState();
    lofi.reset();
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

    audio_block_t* left = allocate();
    audio_block_t* right = allocate();
    if (left == nullptr || right == nullptr)
    {
        if (left != nullptr) release(left);
        if (right != nullptr) release(right);
        return;
    }

    float l[AUDIO_BLOCK_SAMPLES], r[AUDIO_BLOCK_SAMPLES];
    const uint32_t length = loopLength;
    bank.render(currentPosition, AUDIO_BLOCK_SAMPLES, l, r);
    lofi.process(l, r, AUDIO_BLOCK_SAMPLES, 44100);
    ft::liveFilterProcess(ft::liveFilter(), liveState, l, r, AUDIO_BLOCK_SAMPLES);
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
        float a = l[i] * 32767.0f, b = r[i] * 32767.0f;
        if (a > 32767.0f) a = 32767.0f;
        if (a < -32768.0f) a = -32768.0f;
        if (b > 32767.0f) b = 32767.0f;
        if (b < -32768.0f) b = -32768.0f;
        left->data[i] = (int16_t)a;
        right->data[i] = (int16_t)b;
    }

    if (length > 0)
    {
        currentPosition = (currentPosition + AUDIO_BLOCK_SAMPLES) % length;
    }

    transmit(left, 0);
    transmit(right, 1);
    release(left);
    release(right);
}
