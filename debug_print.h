#pragma once

#include <iostream>

#define QUOTE(x) #x

#ifdef VERBOSE_TIMING
#define TIME_PRINT(x) std::cout << (x) << "\n"
#else
#define TIME_PRINT(x)
#endif

