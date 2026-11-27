#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
extern "C++" {
inline float fabsf(float x) { return __builtin_fabsf(x); }
inline float sqrtf(float x) { return __builtin_sqrtf(x); }
inline float floorf(float x) { return __builtin_floorf(x); }
inline float fabs(float x) { return __builtin_fabsf(x); }
inline float sqrt(float x) { return __builtin_sqrtf(x); }
inline float floor(float x) { return __builtin_floorf(x); }
float sinf(float);
float cosf(float);
float tanf(float);
float powf(float, float);
float expf(float);
float logf(float);
}
