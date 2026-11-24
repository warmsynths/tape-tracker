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

    strControlProperties bg{};
    bg.text = nullptr;
    bg.colors = nullptr;
    bg.value = 255;
    bg.style = controlStyleNoTransparency | controlStyleShow;
    bg.x = 0;
    bg.w = 800;
    bg.y = 425;
    bg.h = 54;
    bgLabel = display.createControl<cBgLabel>(&bg);

    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        strControlProperties tp{};
        tp.colors = display.getTheme().activeLabelsColors;
        tp.text = (char*)"";
        tp.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont4);
        tp.x = 400;
        tp.y = 75 + t * 48;
        tp.w = 780;
        tp.h = 40;
        trackLabel[t] = display.createControl<cLabel>(&tp);
    }

    strControlProperties sp{};
    sp.colors = display.getTheme().activeLabelsColors;
    sp.text = (char*)"";
    sp.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
    sp.x = 400;
    sp.y = 285;
    sp.w = 780;
    sp.h = 36;
    statusLabel = display.createControl<cLabel>(&sp);

    strControlProperties hp{};
    hp.colors = display.getTheme().inactiveLabelsColors;
    hp.text = (char*)"";
    hp.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont1);
    hp.x = 400;
    hp.y = 325;
    hp.w = 780;
    hp.h = 30;
    hintLabel = display.createControl<cLabel>(&hp);

    strControlProperties lp{};
    lp.colors = display.getTheme().activeLabelsColors;
    lp.text = (char*)"";
    lp.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
    lp.x = 400;
    lp.y = 375;
    lp.w = 780;
    lp.h = 36;
    levelLabel = display.createControl<cLabel>(&lp);
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
    display.setControlText(label[3], padsKeysMode ? "Pads: Keys" : "Pads: Pad");
    display.setControlText(label[4], "Track 1");
    display.setControlText(label[5], "Track 2");
    display.setControlText(label[6], "Track 3");
    display.setControlText(label[7], "Track 4");

    titleBar->show();
    titleBar->showRightLabel(false);
    titleBar->setTitle("Four-Track");

    display.setControlText(hintLabel, "Pick a track (press it again to change its source), then Rec.");

    for (int t = 0; t < FourTrackSession::kTracks; t++)
    {
        display.setControlShow(trackLabel[t]);
        display.refreshControl(trackLabel[t]);
    }
    display.setControlShow(statusLabel);
    display.setControlShow(hintLabel);
    display.setControlShow(levelLabel);
    display.setControlText(levelLabel, "");
    display.refreshControl(statusLabel);
    display.refreshControl(hintLabel);
    display.refreshControl(levelLabel);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);
    display.synchronizeRefresh();
}
