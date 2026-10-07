/* Stand-in for the macOS SDK's <objc/runtime.h>: the declarations
 * apple_runtime.h uses, for the stub compile lane only. */
#ifndef STUB_OBJC_RUNTIME_H
#define STUB_OBJC_RUNTIME_H

Class objc_getClass(const char *name);
SEL   sel_registerName(const char *str);

#endif
