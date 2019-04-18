/* devkitPPC is missing a few functions that are kinda needed for some cores.
 * This should add them back in.
 */

#include <wiiu/os/atomic.h>

/**
 * Performs an atomic swap of a single piece of data. In each function,
 * value is written to ptr, and the previous value is returned.
 *
 * From GCC docs:
 *
 * "This builtin is not a full barrier, but rather a *release* barrier. This
 *  means that references after the builtin cannot move to (or be speculated
 *  to) before the builtin, but previous memory stores may not be globally
 *  visible yet, and previous memory loads may not yet be satisfied."
 *
 * https://gcc.gnu.org/onlinedocs/gcc-4.5.0/gcc/Atomic-Builtins.html
 *
 * This builtin seems to be implemented in the Wii U GCC toolchain. But since
 * this is GCC-specific, I'm not going to put it into a more general-use
 * location.
 */
uint8_t SwapAtomic8(uint8_t *ptr, uint8_t value)
{
   return __sync_lock_test_and_set(ptr, value);
}
uint16_t SwapAtomic16(uint16_t *ptr, uint16_t value)
{
   return __sync_lock_test_and_set(ptr, value);
}
uint32_t SwapAtomic32(uint32_t *ptr, uint32_t value)
{
   return __sync_lock_test_and_set(ptr, value);
}
