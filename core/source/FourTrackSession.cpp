#include "FourTrackSession.h"

int FourTrackSession::partCount() const
{
    int n = 0;
    for (int i = 0; i < kTracks; i++)
    {
        if (parts[i])
        {
            n++;
        }
    }
    return n;
}

void FourTrackSession::selectTrack(int track)
{
    if (currentMode != Mode::Idle || track < 0 || track >= kTracks)
    {
        return;
    }
    selectedTrack = track;
}

void FourTrackSession::pressRecord()
{
    if (currentMode != Mode::Idle)
    {
        return;
    }

    activeTrack = selectedTrack;
    const bool onlyPartIsThis = (partCount() == 1 && parts[selectedTrack]);
    if (loop == 0 || onlyPartIsThis)
    {
        currentMode = Mode::RecordingFirst;
        firstFrames = 0;
        full = false;
        isPlaying = false;
    }
    else
    {
        currentMode = Mode::Armed;
        isPlaying = true;
    }
}

void FourTrackSession::pressStop()
{
    switch (currentMode)
    {
        case Mode::RecordingFirst: finishFirst(firstFrames); break;
        case Mode::Armed:
        case Mode::RecordingPart: currentMode = Mode::Idle; break;
        case Mode::ChoosingBars: break; // answer the question first
        case Mode::Idle: isPlaying = false; break;
    }
}

void FourTrackSession::pressPlay()
{
    if (currentMode == Mode::Idle && loop > 0)
    {
        isPlaying = true;
    }
}

void FourTrackSession::firstRecordedFrames(uint32_t frames, bool memoryFull)
{
    if (currentMode != Mode::RecordingFirst)
    {
        return;
    }

    firstFrames = frames;
    if (firstFrames >= maxFrames)
    {
        firstFrames = maxFrames;
        full = true;
    }
    else if (memoryFull)
    {
        full = true;
    }

    if (full)
    {
        finishFirst(firstFrames);
    }
}

void FourTrackSession::armWindowReached()
{
    if (currentMode == Mode::Armed)
    {
        currentMode = Mode::RecordingPart;
    }
}

void FourTrackSession::partRecorded()
{
    if (currentMode == Mode::RecordingPart)
    {
        parts[activeTrack] = true;
        currentMode = Mode::Idle;
    }
}

void FourTrackSession::finishFirst(uint32_t frames)
{
    currentMode = Mode::Idle;
    if (frames == 0)
    {
        // Nothing recorded: keep whatever was there before.
        isPlaying = (loop > 0);
        return;
    }

    // Wait for the person to say how many bars this is before the loop is set.
    pending = frames;
    currentMode = Mode::ChoosingBars;
    isPlaying = false;
}

void FourTrackSession::confirmBars(int bars)
{
    if (currentMode != Mode::ChoosingBars || bars <= 0)
    {
        return;
    }
    loop = pending;
    barCount = bars;
    for (int i = 0; i < kTracks; i++)
    {
        parts[i] = (i == activeTrack);
    }
    currentMode = Mode::Idle;
    isPlaying = true;
}

void FourTrackSession::cancelBars()
{
    if (currentMode != Mode::ChoosingBars)
    {
        return;
    }
    parts[activeTrack] = false;
    if (partCount() == 0)
    {
        loop = 0;
        barCount = 0;
    }
    currentMode = Mode::Idle;
    isPlaying = (loop > 0);
}
