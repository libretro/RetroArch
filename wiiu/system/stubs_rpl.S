
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
#define IMPORT_END(lib)

#include "imports.h"


#undef IMPORT_BEGIN
#undef IMPORT
#undef IMPORT_END

#define IMPORT_BEGIN(lib) \
.section ".data.rplFuncStubs"; \
.align 2; \
   fstubBegin_##lib:

#define IMPORT(name) \
   .global name##_stub; \
   name##_stub: \
   .long name;

#define IMPORT_END(lib) \
   fstubEnd_##lib: \
.section ".rodata.rplNames"; \
   lib##_name: \
   .string #lib; \
.section ".lib.rplLibs"; \
   .align 2; \
   .long lib##_name; \
   .long fstubBegin_##lib; \
   .long fstubEnd_##lib

#include "imports.h"
