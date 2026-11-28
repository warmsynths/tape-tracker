#ifndef play_synth_h_
#define play_synth_h_

#include "AudioStream.h"
#include "dsp/Synth.h"
#include "dsp/SendFx.h"

// The Four-Track synth as an audio source: the shared synth (core/ft/dsp/Synth.*, Plaits
// oscillators and ensemble) rendered one block at a time. Output 0 is the left channel, output 1
// the right. Notes are set from the main loop, so they are applied with the audio paused.
class AudioPlaySynth : public AudioStream
{
  public:
    AudioPlaySynth()
      : AudioStream(0, NULL)
    {
    }

    void begin(float sampleRate);
    void setPatch(const ft::SynthPatch& patch);
    void setLevelDb10(int db10) { levelAmp = ft::synthLevelAmp(db10); }
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();
    void drum(int voice, int velocity);
    void setMod(float bend, float vibrato);
    int voiceLimit() const { return limitNow; }
    int loadPercent(); // the most one block took since the last call, as a percent of a block's time (at 180 MHz)
    void setKit(int kit);

    virtual void update(void) override;

  private:
    ft::Synth synth;
    ft::LiveFilterState liveState;
    bool started = false;
    volatile float levelAmp = 1.0f;
    volatile uint32_t peakCycles = 0;
    int limitNow = 3;      // voices allowed at once: lowered when the processor is short of time
    uint32_t calm = 0, over = 0;
};

#endif
