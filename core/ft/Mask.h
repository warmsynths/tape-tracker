#ifndef FT_MASK_H_
#define FT_MASK_H_

// Waveform bitmaps (plan D). Each Part's waveform is drawn once into a one-bit picture per lane
// size and kept in the display chip's graphics memory; the home screen then draws it with one
// command in the track colour. The pictures are 600 px wide (one column per pixel).
namespace ft
{
constexpr int kMaskW = 600;
constexpr int kMaskStride = kMaskW / 8; // bytes per row
constexpr int kMaskSizes = 3;           // 0 overview lane, 1 focus lane, 2 strip
constexpr int kMaskHeight[kMaskSizes] = { 84, 250, 28 };
constexpr int kMaskBytes[kMaskSizes] = { kMaskStride * 84, 18752, kMaskStride * 28 }; // the focus one is padded to a multiple of 4
constexpr int kMaskOffset[kMaskSizes] = { 0, kMaskStride * 84, kMaskStride * 84 + 18752 };
constexpr int kMaskPerTrack = kMaskStride * 84 + 18752 + kMaskStride * 28; // 27,152 bytes
// Where the bitmaps start in the display chip's graphics memory: above the stock images (they end near
// 128,800) and below the picture viewer's area (350,000). 4 tracks of 27,152 bytes end at 308,608.
constexpr unsigned kMaskBase = 200000;
inline int maskSlot(int track, int size) { return track * kMaskSizes + size; }
} // namespace ft

#endif /* FT_MASK_H_ */
