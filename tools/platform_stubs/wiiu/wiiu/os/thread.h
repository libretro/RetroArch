/* Hermetic stand-in for wut's <wiiu/os/thread.h>: what
 * retro_timers.h names. */
#ifndef STUB_WIIU_OS_THREAD_H
#define STUB_WIIU_OS_THREAD_H
#include <stdint.h>
void OSSleepTicks(int64_t ticks);
#define ms_to_ticks(ms) ((int64_t)(ms) * 62156250 / 1000)
#endif
