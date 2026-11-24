#include "fourTrack.h"
#include "fourTrackView.h"

void cFourTrack::initDisplayControls()
{
    // Two copies of the same view. The one created first has the lower handle and so is drawn on
    // top: the playhead and level bars sit over the lanes.
    strControlProperties prop{};
    prop.data = &view;
    prop.style = controlStyleShow;
    prop.value = 1; // layer 1: playhead, position, level bars
    viewDynamic = display.createControl<cFourTrackView>(&prop);
    prop.value = 0; // layer 0: everything that changes rarely
    viewStatic = display.createControl<cFourTrackView>(&prop);
}

void cFourTrack::showScreen()
{
    titleBar->hide(); // the screen has its own top bar

    display.setControlShow(viewStatic);
    display.setControlShow(viewDynamic);
    display.refreshControl(viewStatic);
    display.refreshControl(viewDynamic);
    display.synchronizeRefresh();
}
