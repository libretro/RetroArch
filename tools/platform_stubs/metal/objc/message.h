/* Stand-in for the macOS SDK's <objc/message.h>: the declarations
 * apple_runtime.h uses, for the stub compile lane only. */
#ifndef STUB_OBJC_MESSAGE_H
#define STUB_OBJC_MESSAGE_H

id          objc_msgSend(id self, SEL op, ...);
void        objc_msgSend_stret(id self, SEL op, ...);
long double objc_msgSend_fpret(id self, SEL op, ...);

#endif
