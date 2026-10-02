/* A stand-in for libxenon's <time/time.h>: the delay the driver polls
 * with. The mock's queue plays while it waits. */
#ifndef XENON360_MOCK_TIME_H
#define XENON360_MOCK_TIME_H

void udelay(int us);

#endif
