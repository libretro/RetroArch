/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rrar_archive.h).
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* RAR archive reader for libretro-common.
 *
 * The container and the decoder follow libarchive's RAR reader
 * (archive_read_support_format_rar.c, Tim Kientzle and Andres Mejia,
 * 2-clause BSD - see rrar_archive.c for its notice); the PPMd model next
 * to it (rrar_ppmd7.c) is an implementation of Dmitry Shkarin's
 * public-domain PPMd var.H.
 *
 * Deliberately not a general RAR implementation. libretro-common opens
 * archives of ROMs, so this covers what those are packed as and rejects
 * everything else rather than half-supporting it:
 *
 *   - the RAR 1.5 to 4.x container ("Rar!\x1a\x07\x00") and the RAR 5
 *     one ("Rar!\x1a\x07\x01\x00")
 *   - stored members of any version
 *   - members packed by RAR 2.9 to 4.x (unpack version 29 and 36), all
 *     five methods: the LZ coder with its Huffman tables, PPMd blocks,
 *     and the standard filters (delta, x86 E8 and E8/E9, RGB, audio)
 *   - members packed by RAR 5 and later in the RAR 5 format, all five
 *     methods, with its filters (delta, x86 E8 and E8/E9, ARM)
 *
 * Not supported, and said so per entry or at open: members packed by
 * RAR 1.5 or 2.x, the RAR 7 format for dictionaries over 4 GB, solid
 * archives (but for their first member), encryption, multi-volume and
 * self-extracting archives, and for RAR 2.9 filter programs other than
 * the standard ones (there is no RAR virtual machine here).
 *
 * A RAR 5 archive can be made with BLAKE2 hashes in place of CRC-32s.
 * Its members are unpacked and not checked (has_crc is 0): there is no
 * BLAKE2 here.
 *
 * ------------------------------------------------------------------
 * On untrusted input
 * ------------------------------------------------------------------
 *
 * Everything here comes from the file. Header sizes, member sizes,
 * match distances and lengths, filter positions and the code tables are
 * all the archive's to claim. So:
 *
 *   - every header is checked against the archive's length before it is
 *     read, and against its own checksum
 *   - a member is decoded into a buffer of the size its header gives and
 *     never past it; a match that reaches back before the start of the
 *     member reads zeros
 *   - a code table that cannot be a prefix code is refused
 *   - filters are checked against the member's size before they run
 *   - what comes out is checked against the member's CRC-32
 */

#ifndef __LIBRETRO_SDK_RRAR_ARCHIVE_H
#define __LIBRETRO_SDK_RRAR_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RRAR_OK                  0
#define RRAR_ERROR_DATA        (-1)
#define RRAR_ERROR_PARAM       (-2)
#define RRAR_ERROR_MEM         (-3)
#define RRAR_ERROR_UNSUPPORTED (-4)
#define RRAR_ERROR_CRC         (-5)
/* A watcher asked for the decode to stop: rrar_archive_extract_to(). */
#define RRAR_ERROR_CANCELLED   (-6)

/* Signature length at the head of every archive (a RAR 5 one has an
 * eighth byte, zero). */
#define RRAR_SIGNATURE_SIZE 7

/* One entry in the archive. */
typedef struct rrar_entry
{
   /* UTF-8 name, '/' between its parts, NUL terminated. Owned by the
    * archive. */
   const char *name;
   uint64_t    size;         /* unpacked */
   uint64_t    packed_size;
   uint64_t    data_offset;  /* of the packed bytes in the archive */
   uint32_t    crc;
   uint8_t     method;       /* 0x30 stored, 0x31 to 0x35 packed */
   uint8_t     version;      /* of RAR needed to unpack: 29 is 2.9, 50 is 5.0 */
   uint8_t     is_dir;
   /* 0: this reader cannot extract the entry (see the list above) */
   uint8_t     supported;
   /* 0: the archive has no CRC-32 for the entry (a RAR 5 archive made to
    * carry BLAKE2 hashes only), and extraction checks nothing */
   uint8_t     has_crc;
   /* How far back the packed data can refer: its dictionary size. */
   uint64_t    window;
} rrar_entry_t;

typedef struct rrar_archive rrar_archive_t;

/**
 * rrar_archive_open:
 * @out        : receives the opened archive
 * @data       : the whole archive file in memory
 * @len        : length of @data
 *
 * Parses the archive's headers. @data must stay valid and unmodified
 * until rrar_archive_close(); the archive borrows it rather than
 * copying.
 *
 * Returns: RRAR_OK, or a negative RRAR_ERROR_* code. A malformed archive
 * is an error, never a partial success; an archive of a kind this reader
 * does not read (multi-volume, encrypted headers) is
 * RRAR_ERROR_UNSUPPORTED.
 */
int rrar_archive_open(rrar_archive_t **out,
      const uint8_t *data, size_t len);

/* Positioned read of @len bytes at @off into @dst; returns the bytes
 * read, 0 at the end, negative on error. */
typedef int64_t (*rrar_read_t)(void *ud, uint64_t off, void *dst,
      size_t len);

/**
 * rrar_archive_open_read:
 * @out        : receives the opened archive
 * @len        : length of the archive file
 * @read_cb    : positioned reads of it
 * @ud         : handed to @read_cb
 *
 * The same, for an archive that is not in memory and is not to be put
 * there whole: its headers are read one at a time at open, a stored
 * member is read straight into the buffer it is extracted to, and a
 * compressed one has its packed bytes read in while it is decoded.
 *
 * Returns: RRAR_OK, or a negative RRAR_ERROR_* code.
 */
int rrar_archive_open_read(rrar_archive_t **out, uint64_t len,
      rrar_read_t read_cb, void *ud);

/**
 * rrar_archive_close:
 * @a          : archive, may be NULL
 *
 * Releases everything rrar_archive_open() allocated.
 */
void rrar_archive_close(rrar_archive_t *a);

/**
 * rrar_archive_num_entries:
 * @a          : opened archive
 *
 * Returns: the number of entries, including directories.
 */
uint32_t rrar_archive_num_entries(const rrar_archive_t *a);

/**
 * rrar_archive_entry:
 * @a          : opened archive
 * @index      : entry index, below rrar_archive_num_entries()
 *
 * Returns: the entry, or NULL if @index is out of range.
 */
const rrar_entry_t *rrar_archive_entry(const rrar_archive_t *a,
      uint32_t index);

/**
 * rrar_archive_extract:
 * @a          : opened archive
 * @index      : entry index
 * @out        : receives a malloc'd buffer holding the entry's data
 * @out_len    : receives the entry's size
 *
 * Unpacks one entry and checks it against the archive's CRC-32, if the
 * archive has one for it. The
 * caller frees *@out. An empty entry yields a valid one-byte buffer and
 * a length of zero.
 *
 * Returns: RRAR_OK, or a negative RRAR_ERROR_* code, in which case
 * *@out is NULL.
 */
int rrar_archive_extract(rrar_archive_t *a, uint32_t index,
      uint8_t **out, size_t *out_len);

/* Told, on the thread that decodes, how much of a member is final:
 * @done bytes from its start, which no later call takes back. Returns
 * nonzero to have the decode stop. See rrar_archive_extract_to(). */
typedef struct rrar_watch
{
   int  (*progress)(void *ud, size_t done);
   void  *ud;
} rrar_watch_t;

/**
 * rrar_archive_extract_to:
 * @a          : opened archive
 * @index      : entry index
 * @dst        : where the entry's data goes
 * @dst_size   : how much room there is at @dst
 * @watch      : who is told how far the decode has got, or NULL
 *
 * The same as rrar_archive_extract(), into memory the caller has, and
 * with a watcher: for a member that takes long to decode, which another
 * thread can read from @dst as it is decoded. The watcher is called
 * about every 64 KiB with the count of bytes that are final - behind
 * the decoder by a filter's block and the dictionary where the member
 * has filters, since a filter is run over its block only once nothing
 * can refer back into it - and with the whole size once the CRC has
 * been checked. A member is decoded in the calling thread from start to
 * end; an archive may have several decoded at once, each on a thread.
 *
 * Returns: RRAR_OK, RRAR_ERROR_CANCELLED if the watcher stopped it, or
 * another negative RRAR_ERROR_* code. On an error what the watcher was
 * told is final was never checked.
 */
int rrar_archive_extract_to(rrar_archive_t *a, uint32_t index,
      uint8_t *dst, size_t dst_size, const rrar_watch_t *watch);

RETRO_END_DECLS

#endif
