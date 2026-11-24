# View preview (on a computer, not the device)

Draws a screen control with a stand-in for the display chip, so the layout can be checked
without flashing. The text uses a different font, so widths are approximate.

```bash
g++ -std=c++17 -Itools/view-preview -Icore/source -Icore/interface/fourTrack \
  tools/view-preview/main.cpp core/interface/fourTrack/fourTrackView.cpp core/source/Themes.cpp -o /tmp/view_mock
/tmp/view_mock /tmp/scene0.txt 0   # scene 0: playing; scene 1: recording the first take
```
The output lists draw commands (R rectangle, L line, P point, T text). Render them with any
script that draws them onto an 800 x 480 image.
