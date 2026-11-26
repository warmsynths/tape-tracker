#ifndef SOURCE_REMOVEDSCREENS_H_
#define SOURCE_REMOVEDSCREENS_H_

#include <stdint.h>

// The stock pattern editor and performance screens are gone. The sequencer code that still asks
// them what is selected gets these answers: no edit mode, no selection.
inline uint8_t isEditMode() { return 0; }
inline uint8_t isMultiSelection() { return 0; }
inline int16_t getActualTrack() { return 0; }
inline void cbkTurnOffPerformanceMode(void) {}
inline void sendSelection() {}

#endif /* SOURCE_REMOVEDSCREENS_H_ */
