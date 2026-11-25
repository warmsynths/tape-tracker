#include "fourTrack.h"
#include "fourTrackView.h"

void cFourTrack::initDisplayControls()
{
    // Created first = lowest handle = drawn last, so on top. The question sits above everything,
    // then the playhead and level bars above the lanes.
    strControlProperties prop{};
    prop.data = app;
    prop.style = controlStyleShow;
    prop.value = 3; // the "How many bars?" question
    viewPrompt = display.createControl<cFourTrackView>(&prop);
    display.setControlHide(viewPrompt);
    prop.value = 1; // playhead, position, level bars
    viewDynamic = display.createControl<cFourTrackView>(&prop);
    prop.value = 2; // hint line and button row
    viewFooter = display.createControl<cFourTrackView>(&prop);
    prop.value = 0; // top bar, status, ruler and lanes
    viewStatic = display.createControl<cFourTrackView>(&prop);
}

void cFourTrack::showScreen()
{
    titleBar->hide(); // the screen has its own top bar

    display.setControlShow(viewStatic);
    display.setControlShow(viewDynamic);
    display.setControlShow(viewFooter);
    display.refreshControl(viewStatic);
    display.refreshControl(viewDynamic);
    display.refreshControl(viewFooter);
    display.synchronizeRefresh();
}
