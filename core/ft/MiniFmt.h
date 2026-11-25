#ifndef FT_MINIFMT_H_
#define FT_MINIFMT_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

namespace ft
{
// A small printf for the screens. It exists so the firmware, the tests and the web emulator all
// format text identically and nothing depends on a C library. Supports %s %d %u, an optional
// zero-pad width as in %02u, and %%.
inline int fmt(char* out, size_t size, const char* f, ...)
{
    if (size == 0) return 0;
    va_list ap;
    va_start(ap, f);
    size_t n = 0;
    auto put = [&](char c) { if (n + 1 < size) out[n] = c; n++; };
    for (; *f != 0; f++)
    {
        if (*f != '%')
        {
            put(*f);
            continue;
        }
        f++;
        bool zero = false;
        int width = 0;
        if (*f == '0') { zero = true; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }
        if (*f == 's')
        {
            const char* s = va_arg(ap, const char*);
            if (s == nullptr) s = "";
            while (*s != 0) put(*s++);
        }
        else if (*f == 'd' || *f == 'u')
        {
            uint32_t value;
            bool negative = false;
            if (*f == 'd')
            {
                int32_t d = va_arg(ap, int);
                negative = d < 0;
                value = negative ? (uint32_t)(-(int64_t)d) : (uint32_t)d;
            }
            else
            {
                value = va_arg(ap, unsigned);
            }
            char digits[12];
            int count = 0;
            do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value != 0);
            int total = count + (negative ? 1 : 0);
            if (negative && zero) put('-');
            for (int i = total; i < width; i++) put(zero ? '0' : ' ');
            if (negative && !zero) put('-');
            while (count > 0) put(digits[--count]);
        }
        else if (*f == '%')
        {
            put('%');
        }
        else if (*f == 0)
        {
            break;
        }
    }
    va_end(ap);
    out[n < size ? n : size - 1] = 0;
    return (int)n;
}

// Level in dB (-60 to 0) of an amplitude from 0 to 1, without a maths library.
inline int ampToDb(float amplitude)
{
    if (!(amplitude > 0.001f)) return -60;
    union { float f; uint32_t u; } bits = { amplitude };
    const int exponent = (int)((bits.u >> 23) & 255) - 127;
    bits.u = (bits.u & 0x007FFFFFu) | 0x3F800000u; // mantissa in 1..2
    const float m = bits.f;
    // log2(m) for m in 1..2, error below 0.01
    const float log2m = (m - 1.0f) * (1.4425f - 0.4425f * (m - 1.0f) * 0.9f);
    const float db = 6.0206f * ((float)exponent + log2m);
    int result = (int)db;
    if (result < -60) result = -60;
    if (result > 0) result = 0;
    return result;
}
} // namespace ft

#endif /* FT_MINIFMT_H_ */
