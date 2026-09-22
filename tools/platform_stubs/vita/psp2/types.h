/* Compile-only Vita stub. The scalar types live here, as they do
 * in the real headers, so the stubs that need them do not each
 * declare their own and collide under -pedantic. */
#ifndef STUB_PSP2_TYPES
#define STUB_PSP2_TYPES
#include <stdint.h>

typedef void *ScePVoid;
typedef unsigned int SceSize;
typedef int SceUID;
typedef uint32_t SceUInt32;
typedef uint8_t SceUInt8;

#endif
