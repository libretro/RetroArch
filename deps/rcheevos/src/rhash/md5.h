#ifndef RC_MD5_H
#define RC_MD5_H

/* NOTE: this is NOT the md5.h included in the rcheevos repository. It provides the same
 *       functionality using code already present in RetroArch */

/* android build has libretro-common/include in path, but not the base directory.
 * other builds prioritize rcheevos/include over libretro-common/include.
 * to ensure we get the correct include file, use a complicated relative path */
#include <lrc_hash.h>

#define md5_state_t MD5_CTX
#define md5_byte_t unsigned char
#define md5_init(state) MD5_Init(state)
#define md5_append(state, buffer, size) MD5_Update(state, buffer, size)
#define md5_finish(state, hash) MD5_Final(hash, state)

#endif
