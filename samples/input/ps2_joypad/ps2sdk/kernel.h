/* Stand-in for the PS2 SDK's kernel.h: what ps2_joypad.c and the
 * headers it reaches (retro_atomic.h's EE backend) use of it. The
 * test runs on one thread, so masking interrupts is a no-op. */
#ifndef PS2_JOYPAD_TEST_KERNEL_H
#define PS2_JOYPAD_TEST_KERNEL_H
#define ALIGNED(x) __attribute__((aligned(x)))
static __inline int DIntr(void) { return 0; }
static __inline int EIntr(void) { return 0; }
#endif
