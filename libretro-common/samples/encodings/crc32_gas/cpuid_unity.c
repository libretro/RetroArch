/* encoding_rzstd.c and encoding_crc32.c in one translation unit, as
 * griffin.c builds them, compiled by the Makefile against a <cpuid.h>
 * with no include guard, the way clang 12 and older ship it */

#include "../../../encodings/encoding_rzstd.c"
#include "../../../encodings/encoding_crc32.c"
