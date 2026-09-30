/* Host stand-in for libctru's 3ds/services/ps.h: the process services
 * random source the crypto's random path draws on. Declarations only,
 * enough to compile; nothing here runs. */
#ifndef PLATFORM_STUB_3DS_PS_H
#define PLATFORM_STUB_3DS_PS_H
#include <stddef.h>
#include <stdint.h>
#include <3ds/types.h>

Result PS_GenerateRandomBytes(void *out, size_t len);
#endif
