#include "play_synth.h"

#include "Audio.h"

void AudioPlaySynth::begin(float sampleRate)
{
    AudioNoInterrupts();
    synth.init(sampleRate);
    synth.setPreset(0);
    started = true;
    AudioInterrupts();
}

void AudioPlaySynth::setPatch(const ft::SynthPatch& patch)
{
    AudioNoInterrupts();
    synth.setPatch(patch);
    AudioInterrupts();
}

void AudioPlaySynth::noteOn(int note, int velocity)
{
    AudioNoInterrupts();
    if (started) synth.noteOn(note, velocity);
    AudioInterrupts();
}

void AudioPlaySynth::noteOff(int note)
{
    AudioNoInterrupts();
    if (started) synth.noteOff(note);
    AudioInterrupts();
}

void AudioPlaySynth::allNotesOff()
{
    AudioNoInterrupts();
    if (started) synth.allNotesOff();
    AudioInterrupts();
}

void AudioPlaySynth::update(void)
{
    if (!started || !synth.active()) return;

    audio_block_t* left = allocate();
    audio_block_t* right = allocate();
    if (left == nullptr || right == nullptr)
    {
        if (left != nullptr) release(left);
        if (right != nullptr) release(right);
        return;
    }
    float l[AUDIO_BLOCK_SAMPLES], r[AUDIO_BLOCK_SAMPLES];
    synth.render(l, r, AUDIO_BLOCK_SAMPLES);
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
    transmit(left, 0);
    transmit(right, 1);
    release(left);
    release(right);
}
