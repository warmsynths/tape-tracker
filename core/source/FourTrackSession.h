#ifndef FOURTRACKSESSION_H_
#define FOURTRACKSESSION_H_

#include <stdint.h>

// Pure logic for the four-track test build: which track is selected, what Rec, Stop and Play
// do in each mode, and when a Part exists. No hardware, so it is tested on a PC
// (tests/host/four_track_session_test.cpp). The engine glue reports events to it.
class FourTrackSession
{
  public:
    static constexpr int kTracks = 4;

    enum class Mode : uint8_t
    {
        Idle,
        RecordingFirst, // free length; its length becomes the loop
        Armed,          // waiting for the loop to come round
        RecordingPart,  // recording exactly one loop
        ChoosingBars    // the first take is done; the person picks how many bars it is
    };

    explicit FourTrackSession(uint32_t maxLoopFrames)
      : maxFrames(maxLoopFrames)
    {
    }

    Mode mode() const { return currentMode; }
    bool playing() const { return isPlaying; }
    int selected() const { return selectedTrack; }
    uint32_t loopFrames() const { return loop; }
    bool memoryFull() const { return full; }
    // Bars of the loop once chosen (0 until then), and the length of a take waiting for its bars.
    int bars() const { return barCount; }
    uint32_t pendingFrames() const { return pending; }
    bool hasPart(int track) const { return track >= 0 && track < kTracks && parts[track]; }
    int partCount() const;
    // The track being recorded or armed, or -1.
    int recordingTrack() const { return currentMode == Mode::Idle ? -1 : activeTrack; }

    // Only changes the selection while idle.
    void selectTrack(int track);

    // Idle with no loop, or re-recording the only Part: a new first take (free length).
    // Idle with a loop: arm the selected track (starts the loop if stopped).
    void pressRecord();
    // First take: keeps what was recorded and starts the loop. Armed or recording a Part: cancels,
    // keeping the old Part. Idle: stops the loop.
    void pressStop();
    // Idle with a loop: starts it.
    void pressPlay();

    // Events from the engine side.
    void firstRecordedFrames(uint32_t frames, bool memoryFull); // during RecordingFirst
    void armWindowReached();                                    // Armed -> RecordingPart
    void partRecorded();                                        // RecordingPart -> Idle

    // After a first take: the loop is set to the take and the bars are recorded, and it plays.
    void confirmBars(int bars);
    // Throws the first take away. With no other Part there is no loop again.
    void cancelBars();

    // A new, empty project; or one that was opened with a loop of `loopFrames` and `bars`, and
    // Parts on the tracks set in `partMask` (bit 0 is track 1).
    void reset();
    // The length the loop will get when the bars are confirmed (the take trimmed or padded to a grid).
    void setPendingFrames(uint32_t frames) { if (currentMode == Mode::ChoosingBars) pending = frames; }
    void restore(uint32_t loopFrames, int bars, uint8_t partMask);
    // A later take was cancelled or replaced: forget the Part on a track.
    void dropPart(int track);
    void clearPart(int track); // like dropPart, but the Loop and tempo stay

  private:
    void finishFirst(uint32_t frames);

    uint32_t maxFrames;
    uint32_t loop = 0;
    uint32_t firstFrames = 0;
    uint32_t pending = 0;
    int barCount = 0;
    bool full = false;
    bool isPlaying = false;
    bool parts[kTracks] = { false, false, false, false };
    int selectedTrack = 0;
    int activeTrack = 0;
    Mode currentMode = Mode::Idle;
};

#endif /* FOURTRACKSESSION_H_ */
