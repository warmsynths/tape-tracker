#include "fourTrack.h"

void cFourTrack::initDisplayControls()
{
    strControlProperties prop{};
    for (uint8_t i = 0; i < 8; i++)
    {
        prop.colors = display.getTheme().activeLabelsColors;
        prop.text = (char*)"";
        prop.style =
          (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        prop.x = (800 / 8) * i + (800 / 16);
        prop.y = 424;
        prop.w = 800 / 8 - 6;
        prop.h = 55;
        prop.value = 0;
        label[i] = display.createControl<cLabel>(&prop);
    }

    strControlProperties prop2{};
    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.value = 255;
    prop2.style = controlStyleNoTransparency | controlStyleShow;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel = display.createControl<cBgLabel>(&prop2);

    strControlProperties prop3{};
    prop3.colors = display.getTheme().activeLabelsColors;
    prop3.text = (char*)"";
    prop3.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont4);
    prop3.x = 400;
    prop3.y = 200;
    prop3.w = 780;
    prop3.h = 60;
    statusLabel = display.createControl<cLabel>(&prop3);

    strControlProperties prop4{};
    prop4.colors = display.getTheme().inactiveLabelsColors;
    prop4.text = (char*)"";
    prop4.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont1);
    prop4.x = 400;
    prop4.y = 280;
    prop4.w = 780;
    prop4.h = 40;
    hintLabel = display.createControl<cLabel>(&prop4);
}

void cFourTrack::showScreen()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlText(label[i], "");
        display.setControlColors(label[i], display.getTheme().activeLabelsColors);
        display.setControlStyle(
          label[i],
          controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
    }

    display.setControlText(label[0], "Rec");
    display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
    display.setControlText(label[1], "Stop");
    display.setControlText(label[2], "Play");

    titleBar->show();
    titleBar->showRightLabel(false);
    titleBar->setTitle("Four-Track");

    display.setControlText(hintLabel, "Line input, one Part, up to 30 s. Set input gain on Sample Recorder first.");
    display.setControlShow(statusLabel);
    display.setControlShow(hintLabel);
    display.refreshControl(hintLabel);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);
    display.synchronizeRefresh();
}
