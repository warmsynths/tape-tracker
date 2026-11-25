#include "fourTrack.h"
#include "fourTrackView.h"

void cFourTrack::initDisplayControls()
{
    // Created first = lowest handle = drawn last, so on top: popup, then the question, then the
    // playhead and level bars above the footer, then the page or the lanes underneath.
    strControlProperties prop{};
    prop.data = app;
    prop.style = controlStyleShow;
    const int order[6] = { 5, 3, 1, 2, 4, 0 };
    for (int layer : order)
    {
        prop.value = layer;
        view[layer] = display.createControl<cFourTrackView>(&prop);
        display.setControlHide(view[layer]);
    }
}

void cFourTrack::showScreen()
{
    titleBar->hide(); // the screen has its own top bar
    for (int l = 0; l < 6; l++) shown[l] = false;
    application().setMeasure(fourTrackTextMeter());
}
