#ifndef _LIBRETRO_ENCODINGS_BASE64_H
#define _LIBRETRO_ENCODINGS_BASE64_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

char* base64(const void* binaryData, int len, int *flen);
unsigned char* unbase64(const char* ascii, int len, int *flen);

RETRO_END_DECLS

#endif
