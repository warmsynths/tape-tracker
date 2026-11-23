#include "FourTrackLoop.h"

void FourTrackLoop::pressRecord()
{
    if (currentState == State::Recording)
    {
        return;
    }
    frames = 0;
    full = false;
    currentState = State::Recording;
}

void FourTrackLoop::recordedFrames(uint32_t capturedFrames, bool memoryFull)
{
    if (currentState != State::Recording)
    {
        return;
    }

    frames = capturedFrames;
    if (frames >= maxFrames)
    {
        frames = maxFrames;
        full = true;
    }
    else if (memoryFull)
    {
        full = true;
    }

    if (full)
    {
        finishRecording();
    }
}

void FourTrackLoop::pressStop()
{
    if (currentState == State::Recording)
    {
        finishRecording();
    }
    else if (currentState == State::Playing)
    {
        currentState = State::Ready;
    }
}

void FourTrackLoop::pressPlay()
{
    if (currentState == State::Ready)
    {
        currentState = State::Playing;
    }
}

void FourTrackLoop::finishRecording()
{
    currentState = (frames > 0) ? State::Ready : State::Empty;
}
