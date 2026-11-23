#ifndef INTERFACE_FOURTRACK_H_
#define INTERFACE_FOURTRACK_H_

#include "FourTrackLoop.h"
#include "core/modulesBase.h"

// First test build screen: record one mono Part from the line input, then loop it.
// Reached with the Song button in this build. Buttons 1 to 3 under the screen are Rec, Stop, Play.
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

  private:
    void showScreen();
    void refreshStatus(bool force);
    void setDefaultScreenFunct();
    void finishRecordingIfNeeded();

    FourTrackLoop loop;
    hControl label[8]{};
    hControl bgLabel = -1;
    hControl statusLabel = -1;
    hControl hintLabel = -1;

    bool memoryFullFlag = false;
    char statusText[48]{};
    char shownText[48]{};
};

extern cFourTrack fourTrack;

#endif /* INTERFACE_FOURTRACK_H_ */
