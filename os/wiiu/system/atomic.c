/* Atomic swaps of a byte, a halfword and a word, for the cores that call
 * them. coreinit's swap is word-wide, so the narrower ones exchange
 * their part of the aligned word around them by compare-and-swap; all
 * of them go through retro_atomic, which on the Wii U is Cafe OS's own
 * read-modify-write and so holds across the cores. The console is big-
 * endian: the byte at offset 0 is the word's top one. */

#include <stdint.h>

#include <retro_atomic.h>
#include <wiiu/os/atomic.h>

static uint32_t swap_in_word(void *ptr, unsigned bits, uint32_t value)
{
   uintptr_t at          = (uintptr_t)ptr;
   retro_atomic_int_t *w = (retro_atomic_int_t*)(at & ~(uintptr_t)3);
   unsigned shift        = (unsigned)(32 - bits - (at & 3) * 8);
   uint32_t mask         = ((1u << bits) - 1u) << shift;
   uint32_t old;

   do
   {
      old = (uint32_t)retro_atomic_load_acquire_int(w);
   } while (!retro_atomic_cas_int(w, (int)old,
            (int)((old & ~mask) | ((value << shift) & mask))));

   return (old & mask) >> shift;
}

uint8_t SwapAtomic8(uint8_t *ptr, uint8_t value)
{
   return (uint8_t)swap_in_word(ptr, 8, value);
}

uint16_t SwapAtomic16(uint16_t *ptr, uint16_t value)
{
   return (uint16_t)swap_in_word(ptr, 16, value);
}

uint32_t SwapAtomic32(uint32_t *ptr, uint32_t value)
{
   return (uint32_t)retro_atomic_exchange_int((retro_atomic_int_t*)ptr,
         (int)value);
}
