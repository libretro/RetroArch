/* Compile-only DJGPP stub for the matrix's DOS VGA video lane.
 *
 * vga_gfx.c reaches a compiler only inside the DJGPP toolchain, so a
 * change to it is green everywhere else until that job runs.  This
 * carries the DPMI names the driver uses - the register block and the
 * real-mode interrupt call - plus dosmemput, which DJGPP's dpmi.h
 * brings in through sys/movedata.h, each in the shape DJGPP gives it.
 * Add anything else from the real header rather than inventing it. */
#ifndef STUB_DJGPP_DPMI_H
#define STUB_DJGPP_DPMI_H

#include <stddef.h>

typedef union
{
   struct
   {
      unsigned long edi, esi, ebp, res, ebx, edx, ecx, eax;
   } d;
   struct
   {
      unsigned short di, di_hi, si, si_hi, bp, bp_hi, res, res_hi;
      unsigned short bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi;
      unsigned short flags, es, ds, fs, gs, ip, cs, sp, ss;
   } x;
   struct
   {
      unsigned char edi[4], esi[4], ebp[4], res[4];
      unsigned char bl, bh, ebx_b2, ebx_b3, dl, dh, edx_b2, edx_b3;
      unsigned char cl, ch, ecx_b2, ecx_b3, al, ah, eax_b2, eax_b3;
   } h;
} __dpmi_regs;

int  __dpmi_int(int _vector, __dpmi_regs *_regs);
void dosmemput(const void *_buffer, size_t _length, unsigned long _offset);

#endif
