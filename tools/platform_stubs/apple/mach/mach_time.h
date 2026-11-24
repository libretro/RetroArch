/* Stand-in for <mach/mach_time.h>: the declarations rtime.c uses, so
 * its Apple branches get a host syntax pass. */
#ifndef PLATFORM_STUB_MACH_TIME_H
#define PLATFORM_STUB_MACH_TIME_H

#include <stdint.h>

typedef int kern_return_t;

typedef struct mach_timebase_info
{
   uint32_t numer;
   uint32_t denom;
} mach_timebase_info_data_t, *mach_timebase_info_t;

kern_return_t mach_timebase_info(mach_timebase_info_t info);
uint64_t mach_absolute_time(void);
kern_return_t mach_wait_until(uint64_t deadline);

#endif
