/* Hermetic stand-in for pspsdk's <pspthreadman.h>: the one call
 * retro_timers.h names. */
#ifndef STUB_PSPTHREADMAN_H
#define STUB_PSPTHREADMAN_H
#include <time.h>
int sceKernelDelayThread(unsigned int delay);
#endif
