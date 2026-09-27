/* Compile-only DJGPP stub for the matrix's DOS VGA video lane: the
 * port I/O vga_gfx.c uses, in the shape DJGPP's pc.h gives it. */
#ifndef STUB_DJGPP_PC_H
#define STUB_DJGPP_PC_H

unsigned char  inportb(unsigned short _port);
void           outportb(unsigned short _port, unsigned char _data);

#define inp(_port)          inportb(_port)
#define outp(_port, _data)  outportb((_port), (unsigned char)(_data))

#endif
