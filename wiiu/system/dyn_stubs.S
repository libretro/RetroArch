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
   bctr;

.align 2;
.section ".text";

#include "../rpl/libcoreinit/exports.h"
#include "../rpl/libnsysnet/exports.h"
#include "../rpl/libgx2/exports.h"
#include "../rpl/libproc_ui/exports.h"
#include "../rpl/libsndcore2/exports.h"
#include "../rpl/libsysapp/exports.h"
#include "../rpl/libvpad/exports.h"
