/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rvz.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RVZ_H
#define __LIBRETRO_SDK_FORMAT_RVZ_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* RVZ, the GameCube and Wii disc image format, read as the disc image
 * it stands for: every read returns the octets an ISO of the same
 * disc holds at that offset. Wii partitions, stored decrypted and
 * without their hashes, come back hashed and encrypted, with the
 * file's hash exceptions applied; padding stored as a generator seed
 * comes back as the padding.
 *
 * Groups stored uncompressed or with Zstandard are read; a file whose
 * groups use another method opens, and reads of those groups fail.
 * Zstandard needs HAVE_RZSTD. */

typedef struct rvz rvz_t;

#define RVZ_DISC_GAMECUBE 1
#define RVZ_DISC_WII      2

/**
 * rvz_open:
 *
 * Returns: the image, or NULL if @path is not one this reads.
 **/
rvz_t *rvz_open(const char *path);

void rvz_close(rvz_t *rvz);

/* RVZ_DISC_GAMECUBE, RVZ_DISC_WII, or 0 when the file does not say */
int rvz_disc_type(const rvz_t *rvz);

/* the size of the disc image */
uint64_t rvz_disc_size(const rvz_t *rvz);

/**
 * rvz_read:
 *
 * Reads @len octets of the disc image at @off into @dst.
 *
 * Returns: the octets read, short at the end of the image, or -1 when
 * a group cannot be read.
 **/
int64_t rvz_read(rvz_t *rvz, uint64_t off, void *dst, size_t len);

RETRO_END_DECLS

#endif
