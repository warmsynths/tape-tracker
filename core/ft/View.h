#ifndef FT_VIEW_H_
#define FT_VIEW_H_

#include "Gfx.h"
#include <stdint.h>

namespace ft
{
// Everything the Four-Track main screen shows. The application fills it in, the screens draw it.
// Layout follows docs/design/screen-requirements.md section 4.1 (800 x 480).
struct Lane
{
    char name[12];
    char tag[10];  // source: LINE, MIC LOW, ...
    char info[20]; // second line, for example "Offset +522 ms"
    char msg1[32]; // shown in the waveform area when there is no Part
    char msg2[24]; // bigger second line in that area
    bool hasPart;
    bool muted;
    bool selected;
    bool glitch;
    bool active; // being recorded or armed
    uint8_t waveCount;
    uint8_t wave[120]; // 0..255 height of each waveform column
    uint8_t meter;     // 0..100
};

// The "How many bars?" question shown over the main screen after a first take.
struct Prompt
{
    bool active;
    bool memoryFull;
    uint8_t count;
    uint8_t selected;
    uint8_t bars[12];
    uint16_t bpm10[12]; // tempo of each choice in tenths of a BPM
    char played[80];
    char note[48];
    char suggestion[32];
};

struct View
{
    char project[20];
    bool unsaved;
    char tempo[10];
    char bars[6];
    char pos[10];
    char input[12];
    char inputLevels[20];
    char fb[12]; // feedback protection, for example FB -6 dB; empty when it is not acting
    bool monitor;
    char status[20];
    uint8_t statusKind; // 0 stopped, 1 playing, 2 recording, 3 armed
    char statusNote[24];
    uint32_t loopFrames; // 0: no loop yet
    uint32_t posFrames;
    uint8_t barCount;
    char hint[72];
    char loopMode[8];
    char padsMode[6];
    char btn[8][10];
    uint8_t btnKind[8]; // 0 text, 1 Rec dot, 2 Stop square, 3 Play glyph
    int8_t btnSelected; // -1 none
    Lane lane[4];
    Prompt prompt;
};

// The main screen is drawn as layers, each in stages that are small enough for the display chip
// (the firmware draws one stage at a time). Layer 0: top bar, status, ruler, lanes (5 stages).
// Layer 1: playhead, position, counters, level bars (1 stage). Layer 2: hint line and button row
// (2 stages). Layer 3: the "How many bars?" question, drawn on top of everything (2 stages).
constexpr int kLayerCount = 4;
int layerStages(int layer);
void drawLayer(int layer, int stage, const View& view, const Palette& palette, Gfx& gfx);
} // namespace ft

#endif /* FT_VIEW_H_ */
