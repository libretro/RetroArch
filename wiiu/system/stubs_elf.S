/*#define IMPORT(name) \
   .global name; \
   name: \
      lis %r11, addr_##name@h; \
      lwz %r11, addr_##name@l(%r11); \
      mtctr %r11; \
      bctr*/

#define IMPORT(name) \
   .global name; \
   name: \
   lis %r11, addr_##name@h; \
   ori %r11, %r11, addr_##name@l; \
   lwz %r11, 0(%r11); \
   mtctr %r11; \
   bctr

#define IMPORT_BEGIN(lib)
#define IMPORT_END()

.align 2;
.section ".text";

#include "imports.h"

.section ".text"
.globl SC0x25_KernelCopyData
SC0x25_KernelCopyData:
li r0, 0x2500
sc
blr
