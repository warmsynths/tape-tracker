#ifndef play_synth_h_
#define play_synth_h_

#include "AudioStream.h"
#include "dsp/Synth.h"

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
    void setPreset(int index);
    int presetCount() const { return ft::kSynthPresets; }
    const char* presetName(int index) const;
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();

    virtual void update(void) override;

  private:
    ft::Synth synth;
    bool started = false;
};

#endif
