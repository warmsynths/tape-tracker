#pragma once

// Code the audio interrupt runs all the time. On the Tracker it is copied into RAM at start-up, where the processor
// runs it without the wait states of the flash. Elsewhere (the emulator, the tests) this does nothing.
#if defined(__arm__)
#define FT_FASTRUN __attribute__((section(".fastrun"), noinline, noclone))
#else
#define FT_FASTRUN
#endif
