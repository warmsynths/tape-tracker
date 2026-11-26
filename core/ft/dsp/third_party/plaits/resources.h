// Replaces Plaits' generated resources: only the tables the oscillators and effects used here need.
#ifndef PLAITS_RESOURCES_H_
#define PLAITS_RESOURCES_H_

#include "stmlib/stmlib.h"

namespace plaits {
// One sine period in 512 steps, plus 128 more steps for the quadrature (cosine) lookup and one to interpolate
extern const float lut_sine[];
}  // namespace plaits

#endif  // PLAITS_RESOURCES_H_
