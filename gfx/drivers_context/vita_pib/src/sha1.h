/* NOTE: this is NOT the sha1.h from the upstream vita_pib source. It
 *       provides the same entry point using the SHA-1 already present in
 *       libretro-common, which selects a hardware implementation where
 *       the CPU has one.
 */

#ifndef SHA1_H
#define SHA1_H

#include <stddef.h>
#include <stdint.h>

#include <lrc_hash.h>

#define SHA1(hash_out, str, len) \
   do \
   { \
      SHA1Digest((const uint8_t *)(str), (size_t)(len), \
            (uint8_t *)(hash_out)); \
      ((char *)(hash_out))[20] = '\0'; \
   } while (0)

#endif /* SHA1_H */
