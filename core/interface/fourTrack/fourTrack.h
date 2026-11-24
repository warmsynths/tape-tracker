#ifndef INTERFACE_FOURTRACK_H_
#define INTERFACE_FOURTRACK_H_

#include "FourTrackSession.h"
#include "core/modulesBase.h"

// Four-track test screen, opened with the Song button in this build.
// Buttons under the screen: 1 Rec, 2 Stop, 3 Play, 5 to 8 select track 1 to 4.
// The first Part recorded sets the loop. Later Parts wait for the loop to come round and record
// exactly one loop, then all tracks play in sync.
class cFourTrack : public ModuleBase
{
  public:
    virtual void update() override;
    virtual void start(uint32_t options) override;
    virtual void stop() override;

    virtual void initDisplayControls() override;
    virtual void destroyDisplayControls() override {}

    cFourTrack();

    void pressRecord();
    void pressStop();
    void pressPlay();
    void selectTrack(int track);

    // Pads play loaded samples. Two modes:
    //  Keys: the last used sample is spread across the pads at different pitches (stock behaviour).
    //  Pad:  pad N plays sample slot N at its own pitch (like drum pads).
    // The mode is not saved yet; it starts as Keys.
    void padEvent(uint8_t pad, uint8_t state);
    void togglePadMode();

    // True while any track holds a recorded Part. The Sample Editor uses the same memory as the
    // track regions, so it must not be opened then.
    bool hasParts() const { return session.partCount() > 0; }

    // Set from the audio interrupt when the first block of a recording arrives.
    volatile uint32_t recordStartPosition = 0;
    volatile bool recordStartKnown = false;
    volatile bool memoryFullFlag = false;

  private:
    void showScreen();
    void setDefaultScreenFunct();
    void refreshTexts(bool force);
    void refreshLevel();

    void configureRecorder(int track);
    void syncPlayer();
    void startPlayer();
    void onFirstTakeFinished();
    void serviceFirstTake();
    void serviceArmed();
    void servicePartRecording();
    void finishPartEarly();
    void cancelRecording();

    FourTrackSession session;
    const int16_t* partData[FourTrackSession::kTracks]{};

    hControl label[8]{};
    hControl bgLabel = -1;
    hControl trackLabel[FourTrackSession::kTracks]{};
    hControl statusLabel = -1;
    hControl hintLabel = -1;
    hControl levelLabel = -1;

    bool padsKeysMode = true;
    uint8_t padNote[48]{}; // the note each pad started, so its release matches

    char trackText[FourTrackSession::kTracks][40]{};
    char shownTrackText[FourTrackSession::kTracks][40]{};
    char statusText[64]{};
    char shownStatusText[64]{};
    char levelText[64]{};
    char shownLevelText[64]{};
};

extern cFourTrack fourTrack;

#endif /* INTERFACE_FOURTRACK_H_ */
