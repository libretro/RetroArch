#ifndef STUB_PPU_LV2_H
#define STUB_PPU_LV2_H
int lv2syscall1_impl(int n, unsigned long a);
#define lv2syscall1(n, a) lv2syscall1_impl((n), (unsigned long)(a))
#endif
