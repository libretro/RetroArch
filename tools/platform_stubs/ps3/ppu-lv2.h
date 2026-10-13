/* Compile-only stand-in for PSL1GHT's <ppu-lv2.h>: the one lv2 syscall
 * macro mem_stats.c reaches memory information through. */
#ifndef PS3STUB_PPU_LV2_H
#define PS3STUB_PPU_LV2_H
#include <ppu-types.h>
int lv2syscall1_impl(int n, unsigned long a);
#define lv2syscall1(n, a) lv2syscall1_impl((n), (unsigned long)(a))
#endif
