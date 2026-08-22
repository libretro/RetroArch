/* From RFC1204  HMAC-MD5
 */

#ifndef HMAC_MD5_H
#define HMAC_MD5_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/types.h>

#include <stdint.h>

#if !defined(__PS2__) && !defined(PICO_PLATFORM)
#ifndef UWORD32_DEFINED
#define UWORD32_DEFINED
typedef uint32_t UWORD32;
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

void
smb2_hmac_md5(unsigned char *text, int text_len, unsigned char *key, unsigned int key_len,
	 unsigned char *digest);

#ifdef __cplusplus
}
#endif

#endif /* !HMAC_MD5_H */
