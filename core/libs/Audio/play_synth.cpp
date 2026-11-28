#include "play_synth.h"

#include "Audio.h"

void AudioPlaySynth::begin(float sampleRate)
{
    AudioNoInterrupts();
    synth.init(sampleRate);
    synth.setPreset(0);
    started = true;
    synth.setVoiceLimit(limitNow);
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

void AudioPlaySynth::drum(int voice, int velocity)
{
    AudioNoInterrupts();
    if (started) synth.drums.trigger(voice, velocity);
    AudioInterrupts();
}

void AudioPlaySynth::setMod(float bend, float vibrato)
{
    AudioNoInterrupts();
    synth.setMod(bend, vibrato);
    AudioInterrupts();
}

void AudioPlaySynth::setKit(int kit)
{
    AudioNoInterrupts();
    synth.drums.setKit(kit);
    AudioInterrupts();
}

// How long a block takes, in processor cycles (the Tracker's cycle counter), so the load can be read off
static inline uint32_t cycleCount()
{
    volatile uint32_t* demcr = (volatile uint32_t*)0xE000EDFC;
    volatile uint32_t* ctrl = (volatile uint32_t*)0xE0001000;
    if ((*demcr & (1u << 24)) == 0) *demcr |= (1u << 24);
    if ((*ctrl & 1u) == 0) *ctrl |= 1u;
    return *(volatile uint32_t*)0xE0001004;
}

int AudioPlaySynth::loadPercent()
{
    const uint32_t peak = peakCycles;
    peakCycles = 0;
    return (int)((uint64_t)peak * 100 / 522000u); // one block is 128 frames: about 522,000 cycles at 180 MHz
}

void AudioPlaySynth::update(void)
{
    if (!started || !synth.active()) return;
    const uint32_t t0 = cycleCount();

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
    ft::liveFilterProcess(ft::liveFilter(), liveState, l, r, AUDIO_BLOCK_SAMPLES);
    const float amp = levelAmp;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
    {
        float a = l[i] * amp * 32767.0f, b = r[i] * amp * 32767.0f;
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
    const uint32_t spent = cycleCount() - t0;
    if (spent > peakCycles) peakCycles = spent;
    // Short of time (more than about half a block): allow fewer voices at once. Plenty of time for a while: allow more.
    // (a single slow block is not enough: the card or the display can steal a block now and then)
    if (spent > 450000u) { calm = 0; over = 0; if (limitNow > 2) { limitNow = 2; synth.setVoiceLimit(2); } } // nearly out of time: drop to two voices at once
    else if (spent > 330000u) { calm = 0; if (++over >= 3 && limitNow > 2) { limitNow--; synth.setVoiceLimit(limitNow); over = 0; } }
    else if (spent < 180000u) { over = 0; if (++calm > 600 && limitNow < ft::Synth::kVoices) { limitNow++; synth.setVoiceLimit(limitNow); calm = 0; } }
    else { calm = 0; over = 0; }
}
