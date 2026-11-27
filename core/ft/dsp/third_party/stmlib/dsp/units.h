// Replacement for stmlib/dsp/units.h (MIT, Emilie Gillet): the original reads pitch tables that this
// project does not carry. Only what the drum voices use is here, computed instead of looked up.
#ifndef STMLIB_DSP_UNITS_H_
#define STMLIB_DSP_UNITS_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

namespace stmlib {

// 2^(pitch / 12), good to about 0.1 % for pitches between -60 and 60 semitones
inline float SemitonesToRatio(float pitch) {
  float x = pitch * (1.0f / 12.0f);
  if (x > 20.0f) x = 20.0f;
  if (x < -20.0f) x = -20.0f;
  const int xi = static_cast<int>(x < 0.0f ? x - 1.0f : x);
  const float f = x - static_cast<float>(xi);
  const float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
  union { float f; uint32_t u; } bits;
  bits.u = static_cast<uint32_t>(xi + 127) << 23;
  return bits.f * p;
}

}  // namespace stmlib

#endif  // STMLIB_DSP_UNITS_H_
