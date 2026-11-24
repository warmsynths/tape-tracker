#pragma once
#include <stdint.h>
enum { RECTS=9, LINES=3, FTPOINTS=2, BITMAPS=1 };
void API_COLOR(uint32_t); void API_VERTEX2F(int16_t,int16_t); void API_POINT_SIZE(uint16_t); void API_BEGIN(uint8_t); void API_END(void);
