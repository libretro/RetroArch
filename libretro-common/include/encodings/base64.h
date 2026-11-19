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

/**
 * Decodes @len characters of base64 at @ascii into @out, skipping
 * whitespace (line breaks included) and stopping at '='.
 *
 * @param out At least len / 4 * 3 + 2 bytes. It may be @ascii itself
 * (or any address at or before it) to decode in place.
 * @return The number of bytes written, or (size_t)-1 when the run holds
 * a character outside the alphabet or ends a single character into a
 * group.
 */
size_t unbase64_into(const char *ascii, size_t len, unsigned char *out);

RETRO_END_DECLS

#endif
