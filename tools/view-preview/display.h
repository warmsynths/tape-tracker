#pragma once
#include "Themes.h"
#include "displayControls.h"
struct DisplayMock { Theme& getTheme(); };
extern DisplayMock display;
extern strFont fonts[];
