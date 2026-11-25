#ifndef FT_APP_H_
#define FT_APP_H_

#include "FourTrackSession.h"
#include "Gfx.h"
#include "Hal.h"
#include "View.h"
#include "loop_bars.h"

namespace ft
{
// Buttons and keys of the Tracker that the application reacts to. The eight buttons under the
// screen are B1 to B8.
enum Key
{
    kB1, kB2, kB3, kB4, kB5, kB6, kB7, kB8,
    kUp, kDown, kLeft, kRight,
    kRec, kPlay, kStop, kShift,
    kKeyCount
};

// The Four-Track application: the main screen and everything behind it. No hardware in here: it
// talks to the machine only through Hal and draws only through Gfx. The Tracker firmware and
// the web emulator both run exactly this code.
class App
{
  public:
    App(Hal& hal, const Palette& palette);

    // The screen was opened or closed.
    void enter();
    void leave();

    // Input
    void key(int key);
    void jog(int delta);
    void pad(int pad, bool down);

    // One pass of the main loop.
    void tick();

    // Drawing. A layer is drawn in stages. Returns true if the layer has to be redrawn since the
    // last call (it clears the flag). Layer 3 is only visible while a question is open.
    bool takeRedraw(int layer);
    bool layerVisible(int layer) const;
    void draw(int layer, int stage, Gfx& gfx) const;
    const View& view() const { return model; }
    void setPalette(const Palette& palette) { pal = palette; }

    const FourTrackSession& state() const { return session; }

  private:
    void pressRecord();
    void pressStop();
    void pressPlay();
    void selectTrack(int track);
    void moveSelection(int delta);
    void promptMove(int delta);
    void promptConfirm();
    void promptCancel();
    void togglePadMode();

    void applySource(Source source);
    void startPlayer();
    void syncPlayer();
    void onFirstTakeFinished();
    void serviceFirstTake();
    void serviceArmed();
    void servicePartRecording();
    void finishPartEarly();
    void cancelRecording();

    bool updateTrackMeters();
    void computeWave(int track);
    void fillView();
    void refresh(bool force);

    static const char* sourceName(Source source);

    Hal& hal;
    Palette pal;
    FourTrackSession session;
    View model{};

    Source trackSource[FourTrackSession::kTracks] = { Source::Line, Source::Line, Source::Line, Source::Line };
    const int16_t* partData[FourTrackSession::kTracks]{};
    int appliedSource = -1;
    bool padsKeysMode = true;

    bool promptShown = false;
    int promptBars[kBarChoiceMax]{};
    int promptCount = 0;
    int promptSelected = 0;

    uint32_t staticSignature = 0;
    bool waveDirty[FourTrackSession::kTracks] = { true, true, true, true };
    uint8_t waveVersion[FourTrackSession::kTracks]{};
    uint32_t meterTimer = 0;
    int trackDb[FourTrackSession::kTracks] = { -60, -60, -60, -60 };
    bool redraw[kLayerCount] = { true, true, true, false };
    bool visibleNow = false;
};
} // namespace ft

#endif /* FT_APP_H_ */
