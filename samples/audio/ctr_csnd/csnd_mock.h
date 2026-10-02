#ifndef CTR_CSND_MOCK_H
#define CTR_CSND_MOCK_H

#include "3ds.h"

/* The system tick svcGetSystemTick() returns. The driver counts one
 * output sample per 8192 ticks (CSND's 2048 at a quarter of the CPU
 * clock), so the test plays n samples by adding n * 8192. */
extern u64 mock_tick;

#define MOCK_TICKS_PER_SAMPLE 8192u

#endif
