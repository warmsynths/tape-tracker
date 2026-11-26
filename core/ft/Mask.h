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

// One more picture, shared by the pages that show a big waveform (Part Editor, Pad sample): 760 x 200,
// slot 12, only one page is open at a time. 19,000 bytes after the four tracks, ending at 327,608.
constexpr int kPageMaskSlot = 12;
constexpr int kPageMaskW = 760, kPageMaskH = 200;
constexpr int kPageMaskStride = kPageMaskW / 8;
inline unsigned maskAddress(int slot) { return slot < kPageMaskSlot ? kMaskBase + (unsigned)(slot / kMaskSizes) * kMaskPerTrack + (unsigned)kMaskOffset[slot % kMaskSizes] : kMaskBase + 4u * kMaskPerTrack; }
inline int maskStride(int slot) { return slot < kPageMaskSlot ? kMaskStride : kPageMaskStride; }
} // namespace ft

#endif /* FT_MASK_H_ */
