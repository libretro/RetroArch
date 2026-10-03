#ifndef PS3_RING_DEVICE_MOCK_H
#define PS3_RING_DEVICE_MOCK_H

#include <retro_atomic.h>

void mock_device_reset(void);

/* Blocks the device took, how many were silence, and how many times a
 * wait on the condition was refused for want of its mutex - the shape
 * of the bug the driver's wait_block() comment records. */
extern retro_atomic_size_t mock_blocks;
extern retro_atomic_size_t mock_silent;
extern retro_atomic_size_t mock_cond_eperm;

#define MOCK_READ(c) retro_atomic_load_acquire_size(&(c))

#endif
