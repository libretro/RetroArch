/* Hermetic stand-in for the PS3 SDK's <sys/timer.h>: what
 * retro_timers.h names. */
#ifndef STUB_SYS_TIMER_H
#define STUB_SYS_TIMER_H
int sys_timer_usleep(unsigned int usec);
#endif
