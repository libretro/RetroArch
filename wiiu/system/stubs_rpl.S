
/*#define IMPORT_BEGIN(lib)  .align 2; .section .fimport_##lib,"ax";*/
#define IMPORT_BEGIN(lib)

/*#define IMPORT(name) \
   .global name; \
   name: \
   nop; \
   nop*/

#define IMPORT(name) \
.align 2; \
.section ".rplTramp.text","ax"; \
   .global name; \
   name: \
      lis %r0, name##_stub@h; \
      ori %r0, %r0, name##_stub@l; \
      mtctr %r0; \
      bctr;

/*#define IMPORT_END() .align 2;*/
#define IMPORT_END()

#include "imports.h"

#undef IMPORT_BEGIN
#undef IMPORT
#undef IMPORT_END

#define IMPORT_BEGIN(lib) \
.section ".rodata.rplNames"; \
   1: \
   .string #lib; \
.section ".data.rplFuncStubs"; \
.align 2; \
   2:

#define IMPORT(name) \
   .global name##_stub; \
   name##_stub: \
   .long name;

#define IMPORT_END() \
   3: \
.section ".lib.rplLibs"; \
   .align 2; \
   .long 1b; \
   .long 2b; \
   .long 3b

#include "imports.h"

.section ".text"
.globl SC0x25_KernelCopyData
SC0x25_KernelCopyData:
li r0, 0x2500
sc
blr
