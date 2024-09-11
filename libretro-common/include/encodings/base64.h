#ifndef _LIBRETRO_ENCODINGS_BASE64_H
#define _LIBRETRO_ENCODINGS_BASE64_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Encodes binary data into a \c NULL-terminated base64 string.
 *
 * @param binaryData The data to encode.
 * Behavior is undefined if \c NULL.
 * @param len The length of the data to encode.
 * @param flen Pointer to the length of the returned string.
 * @return Pointer to the base64-encoded string, or \c NULL on failure.
 * The returned string is owned by the caller and must be released with \c free().
 * @see unbase64
 */
char* base64(const void* binaryData, int len, int *flen);

/**
 * Decodes a base64-encoded string into binary data.
 *
 * @param ascii The base64 string to decode, in ASCII format.
 * @param len Length of the string to decode.
 * @param flen Pointer to the length of the returned data.
 * @return The decoded binary data, or \c NULL on failure.
 * The returned buffer is owned by the caller and must be released with \c free().
 * @see base64
 */
unsigned char* unbase64(const char* ascii, int len, int *flen);

RETRO_END_DECLS

#endif
