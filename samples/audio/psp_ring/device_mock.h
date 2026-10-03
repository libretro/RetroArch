#ifndef PSP_RING_DEVICE_MOCK_H
#define PSP_RING_DEVICE_MOCK_H

#include <retro_atomic.h>

void mock_device_reset(void);

/* Periods the device took, how many were silence, and windows whose
 * frames did not continue the previous one. The driver's worker is
 * what raises these, and the test reads them while it runs. */
extern retro_atomic_size_t mock_periods;
extern retro_atomic_size_t mock_silent;
extern retro_atomic_size_t mock_breaks;

#define MOCK_READ(c) retro_atomic_load_acquire_size(&(c))

/* Set to refuse the drivers' next sthread_create. */
extern int mock_thread_fail;

#endif
