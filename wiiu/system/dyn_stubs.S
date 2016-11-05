/*#define EXPORT(name) \
   .global name; \
   name: \
      lis %r11, addr_##name@h; \
      lwz %r11, addr_##name@l(%r11); \
      mtctr %r11; \
      bctr*/

#define EXPORT(name) \
   .global name; \
   name: \
   lis %r11, addr_##name@h; \
   ori %r11, %r11, addr_##name@l; \
   lwz %r11, 0(%r11); \
   mtctr %r11; \
   bctr

#define EXPORT_BEGIN(lib)
#define EXPORT_END()

.align 2;
.section ".text";

#include "exports/all.h"

