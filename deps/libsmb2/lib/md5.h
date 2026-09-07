/* NOTE: this is NOT the md5.h included in the libsmb2 repository. It provides
 *       the same interface using code already present in RetroArch, so that
 *       the tree carries one MD5 rather than one per vendored library.
 */

#ifndef SMB2_MD5_H
#define SMB2_MD5_H

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <lrc_hash.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UWORD32_DEFINED
#define UWORD32_DEFINED
typedef uint32_t UWORD32;
#endif

#define md5byte unsigned char

struct MD5Context
{
   MD5_CTX ctx;
};

#define MD5Init(context)             MD5_Init(&(context)->ctx)
#define MD5Update(context, buf, len) MD5_Update(&(context)->ctx, (buf), (len))
#define MD5Final(digest, context)    MD5_Final((digest), &(context)->ctx)

#ifdef __cplusplus
}
#endif

#endif /* SMB2_MD5_H */
