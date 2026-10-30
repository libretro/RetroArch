/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file_zlib.c).
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

#include <retro_posix_source.h>

#include <stdlib.h>
#include <string.h>

#include <file/archive_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>

#include <limits.h>

/* The ZIP DEFLATE backend can be built against zlib or against the
 * clean-room inflate implementation in encodings/deflate.h.  Define
 * ARCHIVE_HAVE_ZLIB to use zlib, ARCHIVE_USE_BUILTIN_DEFLATE to use the
 * built-in decoder; when neither is set the choice follows HAVE_ZLIB.  Only
 * the raw inflate path differs -- everything else (ZIP parsing, STORED mode,
 * CRC32, zstd) is unchanged. */
#if !defined(ARCHIVE_HAVE_ZLIB) && !defined(ARCHIVE_USE_BUILTIN_DEFLATE)
#if defined(HAVE_ZLIB)
#define ARCHIVE_HAVE_ZLIB 1
#else
#define ARCHIVE_USE_BUILTIN_DEFLATE 1
#endif
#endif

#ifdef ARCHIVE_HAVE_ZLIB
#include <zlib.h>
#endif
/* Deflate64, and DEFLATE without zlib */
#include <encodings/deflate.h>

/* Either decoder will do for a member stored with Zstandard; rzstd is
 * preferred where a build has it, and the reference remains available
 * until every platform has moved. */
#ifdef HAVE_RZSTD
#define ZIP_HAVE_ZSTD 1
#include <encodings/rzstd.h>
#endif

#ifdef HAVE_RBZIP2
#include <encodings/rbzip2.h>

/* How far a streaming bzip2 member is read ahead of its decoder, in a
 * ring of twice it */
#ifndef ZIP_BZIP2_AHEAD
#define ZIP_BZIP2_AHEAD RBZIP2_AHEAD
#endif
#endif

/* LZMA members decode through the 7z backend's LZMA decoder. */
#ifdef HAVE_7ZIP
#define ZIP_HAVE_LZMA 1
#include <7z/r7z_lzma.h>
#include <7z/r7z_lzma_stream.h>
#endif

#ifndef CENTRAL_FILE_HEADER_SIGNATURE
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#endif

#ifndef END_OF_CENTRAL_DIR_SIGNATURE
#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#endif

/* ZIP64: the end of the directory, and where to find it */
#define ZIP64_END_OF_CENTRAL_DIR_SIGNATURE         0x06064b50
#define ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIGNATURE 0x07064b50
/* the extra field that holds a member's 64-bit sizes and offset */
#define ZIP64_EXTRA_ID 0x0001

#define _READ_CHUNK_SIZE   (128*1024)   /* Read 128KiB compressed chunks */
/* Output window a deflated member is inflated through when it goes
 * to a file: written out each time it fills, instead of the whole
 * member being held in memory and written in one go */
#define ZIP_STREAM_WINDOW  (256*1024)

enum file_archive_compression_mode
{
   ZIP_MODE_STORED    = 0,
   ZIP_MODE_DEFLATED  = 8,
   ZIP_MODE_DEFLATE64 = 9,
   ZIP_MODE_BZIP2     = 12,
   ZIP_MODE_LZMA      = 14,
   ZIP_MODE_ZSTD      = 93
};

/* The modes decoded whole, in one call, into a buffer of the member's
 * size. */
#ifdef ZIP_HAVE_ZSTD
#define ZIP_WHOLE_ZSTD(m)  ((m) == ZIP_MODE_ZSTD)
#else
#define ZIP_WHOLE_ZSTD(m)  0
#endif
#ifdef HAVE_RBZIP2
#define ZIP_WHOLE_BZIP2(m) ((m) == ZIP_MODE_BZIP2)
#else
#define ZIP_WHOLE_BZIP2(m) 0
#endif
#ifdef ZIP_HAVE_LZMA
#define ZIP_WHOLE_LZMA(m)  ((m) == ZIP_MODE_LZMA)
#else
#define ZIP_WHOLE_LZMA(m)  0
#endif
#define ZIP_MODE_WHOLE(m)  (     (m) == ZIP_MODE_DEFLATE64 \
                              || ZIP_WHOLE_ZSTD(m) || ZIP_WHOLE_BZIP2(m) \
                              || ZIP_WHOLE_LZMA(m))

typedef struct
{
   struct file_archive_transfer *state;
   uint8_t *directory;
   uint8_t *directory_entry;
   uint8_t *directory_end;
   uint64_t fdoffset;
   uint64_t entry_offset;  /* local header of the entry in hand */
   uint64_t boffset, csize, usize;
   uint64_t out_off;       /* bytes of output produced so far */
   size_t   win_len;       /* size of decompressed_data */
   size_t   win_pos;       /* bytes of it filled (rinflate) */
   uint32_t entry_crc;     /* central-directory CRC of the entry in hand */
   uint32_t expect_crc;    /* CRC the member in progress has to match */
   uint32_t crc;           /* CRC of the output produced so far */
   unsigned cmode;
#ifdef ARCHIVE_HAVE_ZLIB
   z_stream *zstream;      /* kept across members */
#else
   void *zstream;          /* rinflate stream handle, kept across members */
   int    out_bound;       /* output buffer already handed to the decoder */
#endif
   uint8_t *tmpbuf;
   size_t   tmpbuf_len;
   void    *bz;            /* the bzip2 decoder, made at the first
                              bzip2 member and kept */
   void    *z64;           /* a streaming Deflate64 member's decoder */
   void    *zs;            /* a streaming Zstandard member's decoder */
   void    *lz;            /* a streaming LZMA member's decoder, */
   uint16_t *lz_probs;     /* and its probabilities */
   size_t   in_have;       /* input in tmpbuf not yet taken */
   /* The whole member, or a ZIP_STREAM_WINDOW of it when streaming */
   uint8_t *decompressed_data;
   bool     inflating;     /* a DEFLATE member is in progress */
   bool     begun;         /* a streaming bzip2 member's first block is
                              decoded */
   bool     streaming;     /* output goes to state->pending_sink */
} zip_context_t;

static INLINE uint32_t read_le(const uint8_t *data, size_t len)
{
   unsigned i;
   uint32_t val = 0;
   len *= 8;
   for (i = 0; i < len; i += 8)
      val |= (uint32_t)*data++ << i;
   return val;
}

static INLINE uint64_t read_le64(const uint8_t *data)
{
   return (uint64_t)read_le(data, 4) | ((uint64_t)read_le(data + 4, 4) << 32);
}

static void zip_context_free_stream(
      zip_context_t *zip_context, bool keep_decompressed)
{
   if (zip_context->zstream)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      inflateEnd(zip_context->zstream);
      free(zip_context->zstream);
#else
      rinflate_free(zip_context->zstream);
      zip_context->out_bound = 0;
#endif
      zip_context->zstream = NULL;
   }
   zip_context->inflating = false;
   zip_context->fdoffset  = 0;
   zip_context->csize     = 0;
   zip_context->usize     = 0;
   if (zip_context->tmpbuf)
   {
      free(zip_context->tmpbuf);
      zip_context->tmpbuf     = NULL;
      zip_context->tmpbuf_len = 0;
   }
   if (zip_context->decompressed_data && !keep_decompressed)
   {
      free(zip_context->decompressed_data);
      zip_context->decompressed_data = NULL;
   }
}

/* Drops the member in progress, if any. The inflate stream and the
 * read buffer stay for the next member. */
static void zip_context_reset_member(zip_context_t *zip_context)
{
   if (zip_context->z64)
   {
      rinflate64_free(zip_context->z64);
      zip_context->z64 = NULL;
   }
#ifdef ZIP_HAVE_ZSTD
   rzstd_blocks_free((rzstd_blocks_t*)zip_context->zs);
#endif
   zip_context->zs        = NULL;
   free(zip_context->lz);
   free(zip_context->lz_probs);
   zip_context->lz        = NULL;
   zip_context->lz_probs  = NULL;
   zip_context->begun     = false;
   zip_context->in_have   = 0;
   zip_context->inflating = false;
   zip_context->streaming = false;
   zip_context->win_len   = 0;
   zip_context->win_pos   = 0;
#ifndef ARCHIVE_HAVE_ZLIB
   zip_context->out_bound = 0;
#endif
   zip_context->fdoffset  = 0;
   zip_context->csize     = 0;
   zip_context->usize     = 0;
   if (zip_context->decompressed_data)
   {
      free(zip_context->decompressed_data);
      zip_context->decompressed_data = NULL;
   }
}

static bool zip_context_reserve_tmpbuf(zip_context_t *zip_context,
      size_t len)
{
   if (zip_context->tmpbuf_len >= len)
      return true;
   free(zip_context->tmpbuf);
   zip_context->tmpbuf_len = 0;
   if (!(zip_context->tmpbuf = (uint8_t*)malloc(len)))
      return false;
   zip_context->tmpbuf_len = len;
   return true;
}

#ifdef HAVE_RBZIP2
/* The bzip2 member read on into the ring of @cap at tmpbuf, up to
 * where the decoder has @taken it to, a whole ring ahead, or its end */
static bool zip_bzip2_fill(struct file_archive_transfer *state,
      zip_context_t *zip_context, uint64_t taken, size_t cap)
{
   uint64_t to = taken + cap;
   if (to > zip_context->csize)
      to = zip_context->csize;
   filestream_seek(state->archive_file,
         (int64_t)(zip_context->fdoffset + zip_context->boffset),
         RETRO_VFS_SEEK_POSITION_START);
   while (zip_context->boffset < to)
   {
      size_t at = (size_t)(zip_context->boffset % cap);
      size_t n  = cap - at;
      if (n > to - zip_context->boffset)
         n = (size_t)(to - zip_context->boffset);
      if (filestream_read(state->archive_file, zip_context->tmpbuf + at,
               (int64_t)n) != (int64_t)n)
         return false;
      zip_context->boffset += n;
   }
   return true;
}
#endif

/* Up to @len octets of member data at @off: lent from the stream
 * where it can, otherwise read into @buf. A lent pointer holds until
 * the next call on @f. Callers keep @len to a slice whose output is
 * still in cache for the CRC. */
static int64_t zip_member_input(RFILE *f, int64_t off, int64_t len,
      uint8_t *buf, const uint8_t **ptr)
{
   int64_t rd;
   filestream_seek(f, off, RETRO_VFS_SEEK_POSITION_START);
   if ((rd = filestream_view(f, len, ptr)) > 0)
      return rd;
   *ptr = buf;
   return filestream_read(f, buf, len);
}

#if defined(ZIP_HAVE_ZSTD) || defined(HAVE_RBZIP2)
/* All @csize octets of a member at @off where they lie: in the mapping,
 * or what the stream lends when it holds all of them; NULL when they
 * would have to be read */
static const uint8_t *zip_member_lent(struct file_archive_transfer *state,
      int64_t off, uint64_t csize)
{
   const uint8_t *src = NULL;
   if (csize > (uint64_t)((size_t)-1))
      return NULL;
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data)
      return state->archive_mmap_data + (size_t)off;
#endif
   filestream_seek(state->archive_file, off, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_view(state->archive_file, csize, &src) == (int64_t)csize)
      return src;
   return NULL;
}
#endif

/* All @csize octets of a member at @off that is decoded whole: from
 * the mapping, from what the stream lends when it holds all of it, or
 * else read into *@buf, grown to *@cap as need be. NULL on failure. */
static const uint8_t *zip_member_whole(struct file_archive_transfer *state,
      int64_t off, uint64_t csize, uint8_t **buf, size_t *cap)
{
   const uint8_t *src = NULL;
   if (csize > (uint64_t)((size_t)-1))
      return NULL;
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data)
      return state->archive_mmap_data + (size_t)off;
#endif
   filestream_seek(state->archive_file, off, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_view(state->archive_file, csize, &src) == (int64_t)csize)
      return src;
   if (*cap < csize)
   {
      uint8_t *p = (uint8_t*)realloc(*buf, csize ? (size_t)csize : 1);
      if (!p)
         return NULL;
      *buf = p;
      *cap = (size_t)csize;
   }
   filestream_seek(state->archive_file, off, RETRO_VFS_SEEK_POSITION_START);
   if (filestream_read(state->archive_file, *buf, csize) != (int64_t)csize)
      return NULL;
   return *buf;
}

/* A member of a mode decoded whole, @csize octets at @src, into @dst,
 * which holds exactly its @usize; true when it decodes to that and to
 * @crc. *@bz is made at the first bzip2 member. */
static bool zip_decode_whole(unsigned cmode, void **bz, uint8_t *dst,
      size_t usize, const uint8_t *src, size_t csize, uint32_t crc)
{
   size_t result = 0;
   bool   ok     = false;
   (void)bz;
   if (cmode == ZIP_MODE_DEFLATE64)
      ok = rinflate64_decode(dst, usize, src, csize, &result)
         == RDEFLATE_PROCESS_END;
#ifdef ZIP_HAVE_LZMA
   /* the LZMA SDK's version, the properties' size, the properties,
    * then the stream */
   if (cmode == ZIP_MODE_LZMA)
   {
      rlzma_dec_t *dec;
      if (     csize < 4 + RLZMA_PROPS_SIZE
            || src[2] != RLZMA_PROPS_SIZE || src[3] != 0
            || !(dec = (rlzma_dec_t*)malloc(sizeof(*dec))))
         return false;
      ok = rlzma_dec_init(dec, src + 4) == RLZMA_OK
         && rlzma_dec_decode(dec, dst, usize, src + 4 + RLZMA_PROPS_SIZE,
               csize - 4 - RLZMA_PROPS_SIZE) == RLZMA_OK;
      free(dec);
      result = usize;
   }
#endif
#ifdef ZIP_HAVE_ZSTD
   if (cmode == ZIP_MODE_ZSTD)
      ok = rzstd_decode(dst, usize, src, csize, &result) == RZSTD_PROCESS_END;
#endif
#ifdef HAVE_RBZIP2
   if (cmode == ZIP_MODE_BZIP2)
   {
      if (!*bz && !(*bz = rbzip2_new()))
         return false;
      ok = rbzip2_decode((rbzip2_t*)*bz, dst, usize, src, csize, &result)
         == RBZIP2_OK;
   }
#endif
   return ok && result == usize && encoding_crc32(0, dst, usize) == crc;
}

static bool zlib_stream_decompress_data_to_file_init(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size)
{
   int64_t offsetData;
   uint8_t *local_header;
   uint32_t offsetNL, offsetEL;
   uint8_t local_header_buf[4];
   bool mapped                = false;
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;
   /* The local header of the entry the walk is on. cdata carries it
    * too, but a pointer cannot hold an offset past 4 GiB on a 32-bit
    * host. */
   uint64_t local                      = zip_context->entry_offset;

   (void)cdata;

   /* free previous data if left unfinished */
   zip_context_reset_member(zip_context);

   if (local > (uint64_t)state->archive_size - 30
         || state->archive_size < 30)
      return false;

#ifdef VFS_HAVE_FILE_MAPPING
   mapped = (state->archive_mmap_data != NULL);
#endif

   /* seek past most of the local directory header */
#ifdef VFS_HAVE_FILE_MAPPING
   if (state->archive_mmap_data)
   {
      /* The offset is the central directory's word: the 30-byte
       * local header (26 skipped + 4 read here) was bounded against
       * the archive above, before anything is read from the mapping. */
      local_header = state->archive_mmap_data + (size_t)local + 26;
   }
   else
#endif
   {
      filestream_seek(state->archive_file, (int64_t)local + 26, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
         return false;
      local_header = local_header_buf;
   }

   offsetNL = read_le(local_header,     2); /* file name length */
   offsetEL = read_le(local_header + 2, 2); /* extra field length */
   offsetData = (int64_t)local + 26 + 4 + offsetNL + offsetEL;

   /* offsetData and the member sizes come from the archive and drive
    * the mmap reads in the iterate step below (memcpy of usize bytes
    * for STORED, csize-bounded chunks for DEFLATED).  Reject a member
    * whose data would extend past the mapping now, rather than reading
    * out of bounds later.  The non-mmap path is naturally bounded - a
    * seek past EOF followed by a read simply returns short - so this
    * only needs to hold for the mapped case, but checking it always is
    * harmless and keeps the two paths consistent.
    *
    * Only csize describes the member's on-disk extent; size is the
    * decompressed length and bounds the output heap buffer, not the
    * archive.  For any compressed mode size is routinely far larger
    * than the whole archive, so it must not be bounded against
    * archive_size - doing so rejects every normally-compressed member.
    * STORED is the one mode that reads usize bytes straight out of the
    * archive, and there it is bounded below. */
   if (       offsetData < 0
         ||   offsetData          > state->archive_size
         ||   csize > (uint64_t)(state->archive_size - offsetData)
         || (      cmode == ZIP_MODE_STORED
               &&   size  > (uint64_t)(state->archive_size - offsetData)))
      return false;

   zip_context->fdoffset              = offsetData;
   zip_context->usize                 = size;
   zip_context->csize                 = csize;
   zip_context->boffset               = 0;
   zip_context->expect_crc            = zip_context->entry_crc;
   zip_context->crc                   = 0;
   zip_context->out_off               = 0;
   zip_context->cmode                 = cmode;

   /* A member bound for a file is written out as it goes, a stored one
    * however small, from where it lies; anything else is decoded whole
    * into memory that has to hold it */
   zip_context->streaming = state->pending_sink
         && (size > ZIP_STREAM_WINDOW || cmode == ZIP_MODE_STORED);
   if (!zip_context->streaming && size > (uint64_t)((size_t)-1))
      return false;
#ifdef ARCHIVE_HAVE_ZLIB
   /* zlib's avail_out holds 32 bits */
   if (     !zip_context->streaming && cmode == ZIP_MODE_DEFLATED
         && size > 0xFFFFFFFFu)
      return false;
#endif
   zip_context->win_len   = zip_context->streaming
         ? (size < ZIP_STREAM_WINDOW ? (size_t)size : ZIP_STREAM_WINDOW)
         : (size_t)size;
   zip_context->win_pos   = 0;

#ifdef ZIP_HAVE_LZMA
   /* LZMA's window is its dictionary, which its matches reach back
    * across: the size its properties ask for, or the member's own */
   if (cmode == ZIP_MODE_LZMA && zip_context->streaming)
   {
      uint8_t  hdr[4 + RLZMA_PROPS_SIZE];
      uint32_t dict, lc, lp, d;
#ifdef VFS_HAVE_FILE_MAPPING
      if (mapped)
         memcpy(hdr, state->archive_mmap_data + (size_t)offsetData,
               csize < sizeof(hdr) ? 0 : sizeof(hdr));
      else
#endif
      {
         filestream_seek(state->archive_file, offsetData,
               RETRO_VFS_SEEK_POSITION_START);
         if (     csize < sizeof(hdr)
               || filestream_read(state->archive_file, hdr, sizeof(hdr))
               != (int64_t)sizeof(hdr))
            return false;
      }
      if (     csize < sizeof(hdr)
            || hdr[2] != RLZMA_PROPS_SIZE || hdr[3] != 0
            || (d = hdr[4]) >= 9 * 5 * 5)
         return false;
      lc   = d % 9;
      lp   = (d / 9) % 5;
      dict = read_le(hdr + 5, 4);
      if (dict < 4096)
         dict = 4096;
      if (lc + lp > RLZMA_STREAM_LCLP_MAX)
         return false;
      zip_context->win_len = (uint64_t)dict < size ? (size_t)dict
         : (size_t)size;
      zip_context->boffset = sizeof(hdr);
      if (     !(zip_context->lz = malloc(sizeof(rlzma_stream_t)))
            || !(zip_context->lz_probs = (uint16_t*)malloc(
                  RLZMA_STREAM_NUM_PROBS(lc, lp) * sizeof(uint16_t)))
            || !(zip_context->decompressed_data =
                  (uint8_t*)malloc(zip_context->win_len))
            || rlzma_stream_init((rlzma_stream_t*)zip_context->lz, hdr + 4,
                  zip_context->lz_probs, zip_context->decompressed_data,
                  zip_context->win_len) != RLZMA_OK
            || (!mapped && !zip_context_reserve_tmpbuf(zip_context,
                  _READ_CHUNK_SIZE)))
      {
         zip_context_reset_member(zip_context);
         return false;
      }
      rlzma_stream_reset((rlzma_stream_t*)zip_context->lz);
      return true;
   }
#endif

   /* NULL-check the decompressed_data malloc: the iterate step and
    * the inflate output binding below dereference it. */
   if (!(zip_context->decompressed_data = (uint8_t*)malloc(
               zip_context->win_len ? zip_context->win_len : 1)))
      return false;

   if (cmode == ZIP_MODE_DEFLATE64 && zip_context->streaming)
   {
      /* read in chunks behind what the decoder left of the last,
       * unless the archive is mapped */
      if (     !(zip_context->z64 = rinflate64_new())
            || (!mapped && !zip_context_reserve_tmpbuf(zip_context,
                  _READ_CHUNK_SIZE + RINFLATE64_LOOKAHEAD)))
      {
         zip_context_reset_member(zip_context);
         return false;
      }
   }
   else if (cmode == ZIP_MODE_DEFLATED)
   {
      /* A mapped archive is inflated straight out of the mapping. */
      if (!mapped && !zip_context_reserve_tmpbuf(zip_context,
               _READ_CHUNK_SIZE))
      {
         zip_context_reset_member(zip_context);
         return false;
      }

#ifdef ARCHIVE_HAVE_ZLIB
      if (!zip_context->zstream)
      {
         if (!(zip_context->zstream = (z_stream*)calloc(1,
                     sizeof(z_stream))))
         {
            zip_context_reset_member(zip_context);
            return false;
         }
         if (inflateInit2(zip_context->zstream, -MAX_WBITS) != Z_OK)
         {
            free(zip_context->zstream);
            zip_context->zstream = NULL;
            zip_context_reset_member(zip_context);
            return false;
         }
      }
      else if (inflateReset(zip_context->zstream) != Z_OK)
      {
         zip_context_reset_member(zip_context);
         return false;
      }

      zip_context->zstream->next_in   = NULL;
      zip_context->zstream->avail_in  = 0;
      zip_context->zstream->next_out  = zip_context->decompressed_data;
      zip_context->zstream->avail_out = (uInt)zip_context->win_len;
#else
      /* The output buffer (the full decompressed_data) is bound once
       * on the first iterate call; only compressed input is fed
       * incrementally after that. */
      if (!zip_context->zstream)
      {
         if (!(zip_context->zstream = rinflate_new(-15)))
         {
            zip_context_reset_member(zip_context);
            return false;
         }
      }
      else
         rinflate_reset(zip_context->zstream, -15);
#endif
      zip_context->inflating = true;
   }

   return true;
}

/* Writes the filled part of a streaming member's window to the
 * pending sink and empties the window */
static bool zip_context_flush_window(zip_context_t *zip_context)
{
   size_t filled;
#ifdef ARCHIVE_HAVE_ZLIB
   filled = zip_context->win_len - zip_context->zstream->avail_out;
#else
   filled = zip_context->win_pos;
#endif
   if (filled && filestream_write(zip_context->state->pending_sink,
            zip_context->decompressed_data, filled) != (int64_t)filled)
      return false;
#ifdef ARCHIVE_HAVE_ZLIB
   zip_context->zstream->next_out  = zip_context->decompressed_data;
   zip_context->zstream->avail_out = (uInt)zip_context->win_len;
#else
   zip_context->win_pos = 0;
   rinflate_set_out(zip_context->zstream,
         zip_context->decompressed_data, zip_context->win_len);
#endif
   return true;
}

static int zlib_stream_decompress_data_to_file_iterate(
      void *context, file_archive_file_handle_t *handle)
{
   int64_t rd;
   zip_context_t *zip_context = (zip_context_t *)context;
   struct file_archive_transfer *state = zip_context->state;

   if (zip_context->cmode == ZIP_MODE_STORED && zip_context->streaming)
   {
      /* A window's worth to the sink per call: straight from the
       * mapping where there is one, or what the stream lends, through
       * the window otherwise */
      const uint8_t *src;
      uint64_t left = zip_context->usize - zip_context->out_off;
      size_t   n    = left < zip_context->win_len
         ? (size_t)left : zip_context->win_len;
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
         src = state->archive_mmap_data
            + (size_t)(zip_context->fdoffset + zip_context->out_off);
      else
#endif
      if (zip_member_input(state->archive_file,
               (int64_t)(zip_context->fdoffset + zip_context->out_off),
               (int64_t)n, zip_context->decompressed_data, &src)
            != (int64_t)n)
         return -1;
      zip_context->crc      = encoding_crc32(zip_context->crc, src, n);
      if (filestream_write(state->pending_sink, src, n) != (int64_t)n)
         return -1;
      zip_context->out_off += n;
      if (zip_context->out_off < zip_context->usize)
         return 0;
      if (zip_context->crc != zip_context->expect_crc)
         return -1;
      handle->data = NULL;
      return 1;
   }
   else if (zip_context->cmode == ZIP_MODE_STORED)
   {
#ifdef VFS_HAVE_FILE_MAPPING
      /* Simply copy the data to the output buffer */
      if (zip_context->state->archive_mmap_data)
         memcpy(zip_context->decompressed_data,
               zip_context->state->archive_mmap_data + (size_t)zip_context->fdoffset,
               (size_t)zip_context->usize);
      else
#endif
      {
         /* Read the entire file to memory */
         filestream_seek(state->archive_file, zip_context->fdoffset, RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file,
                             zip_context->decompressed_data,
                             zip_context->usize) != (int64_t)zip_context->usize)
            return -1;
      }

      if (encoding_crc32(0, zip_context->decompressed_data,
               (size_t)zip_context->usize) != zip_context->expect_crc)
         return -1;

      handle->data = zip_context->decompressed_data;
      return 1;
   }
   else if (zip_context->cmode == ZIP_MODE_DEFLATED)
   {
      const uint8_t *dptr;
      uint64_t left    = zip_context->csize - zip_context->boffset;
      int64_t  to_read = left < _READ_CHUNK_SIZE
         ? (int64_t)left : _READ_CHUNK_SIZE;
      /* File was uncompressed or decompression finished before */
      if (!zip_context->inflating)
         return 1;

#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
      {
         /* Decompress from the mapped file */
         dptr = state->archive_mmap_data
            + (size_t)(zip_context->fdoffset + zip_context->boffset);
         rd   = to_read;
      }
      else
#endif
      {
         rd = zip_member_input(state->archive_file,
               (int64_t)(zip_context->fdoffset + zip_context->boffset),
               to_read, zip_context->tmpbuf, &dptr);
         /* Unexpected EOF before csize: returning 0 would spin forever
          * in file_archive_walk's tight iterate loop (seen with short
          * or stalled VFS backends such as SMB). */
         if (rd <= 0)
            return -1;
      }

      zip_context->boffset           += rd;

#ifdef ARCHIVE_HAVE_ZLIB
      zip_context->zstream->next_in   = (Bytef*)dptr;
      zip_context->zstream->avail_in  = (uInt)rd;

      for (;;)
      {
         /* The CRC runs over each slice's output while it is still
          * in cache. */
         int zret;
         uint8_t *out;

         /* A full window is written out before inflating more */
         if (     zip_context->streaming
               && zip_context->zstream->avail_out == 0
               && !zip_context_flush_window(zip_context))
            return -1;

         out  = zip_context->zstream->next_out;
         zret = inflate(zip_context->zstream, 0);
         zip_context->crc      = encoding_crc32(zip_context->crc, out,
               (size_t)(zip_context->zstream->next_out - out));
         zip_context->out_off += (uint64_t)(zip_context->zstream->next_out - out);

         /* With the whole member as output, one call takes all the
          * input */
         if (!zip_context->streaming)
         {
            if (zret < 0)
               return -1;
            break;
         }

         /* Streaming: input used up and nothing more to drain */
         if (zret == Z_BUF_ERROR && zip_context->zstream->avail_in == 0)
            break;
         if (zret < 0)
            return -1;
         if (zret == Z_STREAM_END)
            break;
         if (     zip_context->zstream->avail_in  == 0
               && zip_context->zstream->avail_out != 0)
            break;
      }
#else
      /* Bind the output buffer once, then feed this compressed chunk.  The
       * decoder keeps its own output cursor across calls, so set_out is only
       * issued the first time. */
      if (!zip_context->out_bound)
      {
         rinflate_set_out(zip_context->zstream,
               zip_context->decompressed_data, zip_context->win_len);
         zip_context->out_bound = 1;
         zip_context->win_pos   = 0;
      }
      rinflate_set_in(zip_context->zstream, dptr, (size_t)rd);

      for (;;)
      {
         size_t got_in = 0, got_out = 0;
         int    st;

         /* A full window is written out before inflating more; the
          * decoder keeps its own back-reference history */
         if (     zip_context->streaming
               && zip_context->win_pos == zip_context->win_len
               && !zip_context_flush_window(zip_context))
            return -1;

         st = rinflate_process(zip_context->zstream, &got_in, &got_out);
         if (st == RDEFLATE_PROCESS_ERROR)
            return -1;
         zip_context->crc      = encoding_crc32(zip_context->crc,
               zip_context->decompressed_data + zip_context->win_pos,
               got_out);
         zip_context->win_pos += got_out;
         zip_context->out_off += got_out;
         /* END: stream finished.  NEXT with no further progress means this
          * input chunk is drained; fetch the next one on the outer loop. */
         if (st == RDEFLATE_PROCESS_END)
            break;
         if (got_in == 0 && got_out == 0)
            break;
      }
#endif

      if (zip_context->boffset >= zip_context->csize)
      {
         zip_context->inflating = false;
#ifndef ARCHIVE_HAVE_ZLIB
         zip_context->out_bound = 0;
#endif
         if (     zip_context->out_off != zip_context->usize
               || zip_context->crc     != zip_context->expect_crc)
            return -1;

         /* A streamed member is already in the sink, whole */
         if (zip_context->streaming)
         {
            if (!zip_context_flush_window(zip_context))
               return -1;
            handle->data = NULL;
            return 1;
         }

         handle->data = zip_context->decompressed_data;
         return 1;
      }

      return 0;   /* still more data to process */
   }
   else if (zip_context->cmode == ZIP_MODE_DEFLATE64 && zip_context->streaming)
   {
      /* A window's worth to the sink per call, matches reaching back
       * past it into the decoder's own 64 KiB */
      const uint8_t *in;
      size_t in_len, used = 0, got = 0;
      int    final, st;
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
      {
         in     = state->archive_mmap_data
            + (size_t)(zip_context->fdoffset + zip_context->boffset);
         in_len = (size_t)(zip_context->csize - zip_context->boffset);
         final  = 1;
      }
      else
#endif
      {
         /* the next chunk behind what the last call left */
         uint64_t left = zip_context->csize - zip_context->boffset;
         size_t   room = zip_context->tmpbuf_len - zip_context->in_have;
         size_t   k    = left < room ? (size_t)left : room;
         if (k)
         {
            filestream_seek(state->archive_file,
                  (int64_t)(zip_context->fdoffset + zip_context->boffset),
                  RETRO_VFS_SEEK_POSITION_START);
            if (filestream_read(state->archive_file,
                     zip_context->tmpbuf + zip_context->in_have, (int64_t)k)
                  != (int64_t)k)
               return -1;
            zip_context->boffset += k;
            zip_context->in_have += k;
         }
         in     = zip_context->tmpbuf;
         in_len = zip_context->in_have;
         final  = zip_context->boffset == zip_context->csize;
      }
      st = rinflate64_process(zip_context->z64, in, in_len, final, &used,
            zip_context->decompressed_data, zip_context->win_len, &got);
      if (st == RDEFLATE_PROCESS_ERROR)
         return -1;
      zip_context->crc      = encoding_crc32(zip_context->crc,
            zip_context->decompressed_data, got);
      zip_context->out_off += got;
      if (     zip_context->out_off > zip_context->usize
            || (got && filestream_write(state->pending_sink,
                  zip_context->decompressed_data, (int64_t)got)
               != (int64_t)got))
         return -1;
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
         zip_context->boffset += used;
      else
#endif
      {
         memmove(zip_context->tmpbuf, zip_context->tmpbuf + used,
               in_len - used);
         zip_context->in_have = in_len - used;
      }
      if (st == RDEFLATE_PROCESS_END)
      {
         if (     zip_context->out_off != zip_context->usize
               || zip_context->crc     != zip_context->expect_crc)
            return -1;
         handle->data = NULL;
         return 1;
      }
      /* nothing taken, nothing made, and nothing more to come */
      if (!got && !used && final)
         return -1;
      return 0;
   }
#ifdef ZIP_HAVE_LZMA
   else if (zip_context->cmode == ZIP_MODE_LZMA && zip_context->streaming)
   {
      /* Up to a window's worth into the dictionary per call, and out
       * of it to the sink from where it lies */
      rlzma_stream_t *lz = (rlzma_stream_t*)zip_context->lz;
      const uint8_t  *in;
      size_t in_len, used, limit, made;
      int    final, status = 0;
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
      {
         in     = state->archive_mmap_data
            + (size_t)(zip_context->fdoffset + zip_context->boffset);
         in_len = (size_t)(zip_context->csize - zip_context->boffset);
         final  = 1;
      }
      else
#endif
      {
         uint64_t left = zip_context->csize - zip_context->boffset;
         size_t   room = zip_context->tmpbuf_len - zip_context->in_have;
         size_t   k    = left < room ? (size_t)left : room;
         if (k)
         {
            filestream_seek(state->archive_file,
                  (int64_t)(zip_context->fdoffset + zip_context->boffset),
                  RETRO_VFS_SEEK_POSITION_START);
            if (filestream_read(state->archive_file,
                     zip_context->tmpbuf + zip_context->in_have, (int64_t)k)
                  != (int64_t)k)
               return -1;
            zip_context->boffset += k;
            zip_context->in_have += k;
         }
         in     = zip_context->tmpbuf;
         in_len = zip_context->in_have;
         final  = zip_context->boffset == zip_context->csize;
      }
      limit = zip_context->win_len - lz->dic_pos;
      if (limit > ZIP_STREAM_WINDOW)
         limit = ZIP_STREAM_WINDOW;
      if ((uint64_t)limit > zip_context->usize - zip_context->out_off)
         limit = (size_t)(zip_context->usize - zip_context->out_off);
      used = in_len;
      if (rlzma_stream_decode(lz, lz->dic_pos + limit, in, &used, final,
               &status) != RLZMA_OK)
         return -1;
      made = lz->dic_pos - zip_context->win_pos;
      zip_context->crc      = encoding_crc32(zip_context->crc,
            zip_context->decompressed_data + zip_context->win_pos, made);
      zip_context->out_off += made;
      if (made && filestream_write(state->pending_sink,
               zip_context->decompressed_data + zip_context->win_pos,
               (int64_t)made) != (int64_t)made)
         return -1;
      /* the dictionary wraps where it ends */
      zip_context->win_pos = lz->dic_pos;
      if (lz->dic_pos == zip_context->win_len)
      {
         lz->dic_pos          = 0;
         zip_context->win_pos = 0;
      }
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
         zip_context->boffset += used;
      else
#endif
      {
         memmove(zip_context->tmpbuf, zip_context->tmpbuf + used,
               in_len - used);
         zip_context->in_have = in_len - used;
      }
      if (zip_context->out_off == zip_context->usize)
      {
         if (zip_context->crc != zip_context->expect_crc)
            return -1;
         handle->data = NULL;
         return 1;
      }
      /* nothing taken, nothing made, and nothing more to come */
      if (!made && !used && final)
         return -1;
      return 0;
   }
#endif
#ifdef ZIP_HAVE_ZSTD
   else if (zip_context->cmode == ZIP_MODE_ZSTD && zip_context->streaming)
   {
      /* A block to the sink per call, from where the decoder leaves
       * it: the frame decoded where it lies whole, in the mapping or
       * what the stream lends, or else read a block at a time */
      const uint8_t *blk = NULL;
      size_t         len = 0;
      int            e;
      if (!zip_context->zs)
      {
         const uint8_t *src = zip_member_lent(state, zip_context->fdoffset,
               zip_context->csize);
         zip_context->boffset = 0;
         if (src)
            zip_context->zs = rzstd_blocks_begin(src,
                  (size_t)zip_context->csize);
         else if (zip_context_reserve_tmpbuf(zip_context, RZSTD_WANT_MAX))
         {
            zip_context->zs    = rzstd_blocks_open();
            zip_context->begun = true;
         }
         if (!zip_context->zs)
            return -1;
      }
      if (!zip_context->begun)
         e = rzstd_blocks_step((rzstd_blocks_t*)zip_context->zs, &blk, &len);
      else
      {
         size_t want = rzstd_blocks_want((rzstd_blocks_t*)zip_context->zs);
         e = RZSTD_PROCESS_NEXT;
         if (!want)
            e = zip_context->boffset == zip_context->csize
               ? RZSTD_PROCESS_END : RZSTD_PROCESS_ERROR;
         else
         {
            filestream_seek(state->archive_file,
                  (int64_t)(zip_context->fdoffset + zip_context->boffset),
                  RETRO_VFS_SEEK_POSITION_START);
            if (     want > zip_context->csize - zip_context->boffset
                  || filestream_read(state->archive_file, zip_context->tmpbuf,
                     (int64_t)want) != (int64_t)want
                  || rzstd_blocks_give((rzstd_blocks_t*)zip_context->zs,
                     zip_context->tmpbuf, want, &blk, &len)
                  != RZSTD_PROCESS_NEXT)
               e = RZSTD_PROCESS_ERROR;
         }
         zip_context->boffset += want;
      }
      if (e == RZSTD_PROCESS_ERROR)
         return -1;
      if (len)
      {
         zip_context->crc      = encoding_crc32(zip_context->crc, blk, len);
         zip_context->out_off += len;
         if (     zip_context->out_off > zip_context->usize
               || filestream_write(state->pending_sink, blk, (int64_t)len)
               != (int64_t)len)
            return -1;
      }
      if (e == RZSTD_PROCESS_NEXT)
         return 0;
      if (     zip_context->out_off != zip_context->usize
            || zip_context->crc     != zip_context->expect_crc)
         return -1;
      rzstd_blocks_free((rzstd_blocks_t*)zip_context->zs);
      zip_context->zs    = NULL;
      zip_context->begun = false;
      /* A whole-member buffer is not kept past its member. */
      if (zip_context->tmpbuf_len > _READ_CHUNK_SIZE)
      {
         free(zip_context->tmpbuf);
         zip_context->tmpbuf     = NULL;
         zip_context->tmpbuf_len = 0;
      }
      handle->data = NULL;
      return 1;
   }
#endif
#ifdef HAVE_RBZIP2
   else if (zip_context->cmode == ZIP_MODE_BZIP2 && zip_context->streaming)
   {
      /* A block to the sink per call, from where the decoder leaves
       * it: the stream decoded where it lies whole, in the mapping or
       * what the stream lends, or else read through a ring that holds
       * at least a block's most ahead of the decoder */
      rbzip2_t      *bz;
      const uint8_t *blk;
      size_t         len;
      int            e;
      if (!zip_context->begun)
      {
         const uint8_t *src = zip_member_lent(state, zip_context->fdoffset,
               zip_context->csize);
         size_t         cap = 2 * (size_t)ZIP_BZIP2_AHEAD;
         zip_context->boffset = 0;
         if (     !zip_context->bz
               && !(zip_context->bz = rbzip2_new()))
            return -1;
         if (!src)
         {
            if (zip_context->csize < cap)
               cap = zip_context->csize ? (size_t)zip_context->csize : 1;
            if (!zip_context_reserve_tmpbuf(zip_context, cap))
               return -1;
            if (!zip_bzip2_fill(state, zip_context, 0, cap))
               return -1;
            src = zip_context->tmpbuf;
         }
         if (rbzip2_begin_ring((rbzip2_t*)zip_context->bz, src,
                  src == zip_context->tmpbuf
                  ? (size_t)zip_context->boffset
                  : (size_t)zip_context->csize, NULL, 0) != RBZIP2_OK)
            return -1;
         zip_context->begun = true;
      }
      bz = (rbzip2_t*)zip_context->bz;
      if (     zip_context->boffset
            && zip_context->boffset < zip_context->csize
            && zip_context->boffset - rbzip2_taken(bz) < ZIP_BZIP2_AHEAD)
      {
         /* the ring filled again up to what the decoder still needs,
          * and that handed over again, in two where it wraps */
         size_t   cap  = 2 * (size_t)ZIP_BZIP2_AHEAD;
         uint64_t t    = rbzip2_taken(bz);
         size_t   from = (size_t)(t % cap);
         size_t   have;
         if (!zip_bzip2_fill(state, zip_context, t, cap))
            return -1;
         have = (size_t)(zip_context->boffset - t);
         if (have > cap - from)
            rbzip2_input(bz, zip_context->tmpbuf + from, cap - from,
                  zip_context->tmpbuf, have - (cap - from));
         else
            rbzip2_input(bz, zip_context->tmpbuf + from, have, NULL, 0);
      }
      e = rbzip2_step_block(bz, &blk, &len);
      if (e == RBZIP2_PENDING)
      {
         zip_context->crc      = encoding_crc32(zip_context->crc, blk, len);
         zip_context->out_off += len;
         if (     zip_context->out_off > zip_context->usize
               || filestream_write(state->pending_sink, blk, (int64_t)len)
               != (int64_t)len)
            return -1;
         return 0;
      }
      if (     e != RBZIP2_OK
            || zip_context->out_off != zip_context->usize
            || zip_context->crc     != zip_context->expect_crc)
         return -1;
      /* A whole-member buffer is not kept past its member. */
      if (zip_context->tmpbuf_len > _READ_CHUNK_SIZE)
      {
         free(zip_context->tmpbuf);
         zip_context->tmpbuf     = NULL;
         zip_context->tmpbuf_len = 0;
      }
      handle->data = NULL;
      return 1;
   }
#endif
   else if (ZIP_MODE_WHOLE(zip_context->cmode))
   {
      const uint8_t *src = zip_member_whole(state, zip_context->fdoffset,
            zip_context->csize, &zip_context->tmpbuf,
            &zip_context->tmpbuf_len);
      if (     !src
            || !zip_decode_whole(zip_context->cmode, &zip_context->bz,
               zip_context->decompressed_data, (size_t)zip_context->usize,
               src, (size_t)zip_context->csize, zip_context->expect_crc))
         return -1;

      /* A whole-member buffer is not kept past its member. */
      if (zip_context->tmpbuf_len > _READ_CHUNK_SIZE)
      {
         free(zip_context->tmpbuf);
         zip_context->tmpbuf     = NULL;
         zip_context->tmpbuf_len = 0;
      }

      handle->data = zip_context->decompressed_data;
      return 1;
   }

   /* No idea what kind of compression this is */
   return -1;
}

static bool zip_file_decompressed_handle(
      file_archive_transfer_t *transfer,
      file_archive_file_handle_t* handle,
      const uint8_t *cdata, unsigned cmode, uint64_t csize,
      uint64_t size, uint32_t crc32)
{
   int ret   = 0;

   transfer->backend = &zlib_backend;

   if (!transfer->backend->stream_decompress_data_to_file_init(
            transfer->context, handle, cdata, cmode, csize, size))
      return false;

   do
   {
      ret = transfer->backend->stream_decompress_data_to_file_iterate(
            transfer->context, handle);
      if (ret < 0)
         return false;
   }while (ret == 0);

   return true;
}

typedef struct
{
   char *opt_file;
   char *needle;
   void **buf;
   size_t size;
   bool found;
   bool failed;   /* found, and it would not decode */
} decomp_state_t;

/* Extract the relative path (needle) from a
 * ZIP archive (path) and allocate a buffer for it to write it in.
 *
 * optional_outfile if not NULL will be used to extract the file to.
 * buf will be 0 then.
 */

static int zip_file_decompressed(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint64_t csize, uint64_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   decomp_state_t* decomp_state = (decomp_state_t*)userdata->cb_data;
   size_t name_len              = name ? strlen(name) : 0;
   char last_char;
   /* Reject empty or NULL name -- strlen-1 on empty would read
    * name[SIZE_MAX].  Malformed archives can have 0-length filename
    * entries. */
   if (name_len == 0)
      return 1;
   last_char = name[name_len - 1];
   /* Ignore directories. */
   if (last_char == '/' || last_char == '\\')
      return 1;

   if (string_is_equal(name, decomp_state->needle))
   {
      file_archive_file_handle_t handle = {0};

      /* Called in case core has need_fullpath enabled: written out as
       * it is decoded, a stored member from where it lies */
      if (decomp_state->opt_file)
      {
         if (file_archive_perform_mode(decomp_state->opt_file, valid_exts,
                  cdata, cmode, csize, size, crc32, userdata))
            decomp_state->size = 0;
         else
            decomp_state->failed = true;
         decomp_state->found = true;
         return 1;
      }

      /* Called in case core has need_fullpath disabled: the decoded
       * member is handed over as RetroArch's ROM buffer */
      if (zip_file_decompressed_handle(userdata->transfer,
               &handle, cdata, cmode, csize, size, crc32))
      {
         zip_context_t *zip_context = (zip_context_t *)userdata->transfer->context;
         *decomp_state->buf             = handle.data;
         decomp_state->size             = size;
         /* We keep the data, prevent its deallocation during free */
         zip_context->decompressed_data = NULL;
         handle.data = NULL;
      }
      else
         decomp_state->failed = true;

      decomp_state->found = true;
   }

   return 1;
}

static int64_t zip_file_read(
      const char *path,
      const char *needle, void **buf,
      const char *optional_outfile)
{
   file_archive_transfer_t state            = {0};
   decomp_state_t decomp                    = {0};
   struct archive_extract_userdata userdata = {0};
   int ret                                  = 0;

   if (needle)
      decomp.needle          = strdup(needle);
   if (optional_outfile)
      decomp.opt_file        = strdup(optional_outfile);

   /* NULL-check strdups so allocation failure cannot be mistaken for
    * an omitted member or output path.  Bail out of the extraction;
    * caller treats -1 as 'not found / failed'.  Free whatever strdup
    * succeeded to avoid a leak on partial-success OOM. */
   if ((needle && !decomp.needle) ||
       (optional_outfile && !decomp.opt_file))
   {
      free(decomp.needle);
      free(decomp.opt_file);
      return -1;
   }

   state.type                = ARCHIVE_TRANSFER_INIT;
   userdata.transfer         = &state;
   userdata.cb_data          = &decomp;
   decomp.buf                = buf;

   do
   {
      bool returnerr = true;
      ret = file_archive_parse_file_iterate(&state, &returnerr, path,
            "", zip_file_decompressed, &userdata);
      if (!returnerr)
         break;
   }while (ret == 0 && !decomp.found);

   file_archive_parse_file_iterate_stop(&state);

   if (decomp.opt_file)
      free(decomp.opt_file);
   if (decomp.needle)
      free(decomp.needle);

   if (!decomp.found || decomp.failed)
      return -1;

   return (int64_t)decomp.size;
}

/* TorrentZip names itself in the end record's comment: TORRENTZIPPED-
 * and eight hex digits. */
static bool zip_comment_torrentzip(const uint8_t *comment, unsigned len)
{
   return len == 22 && !memcmp(comment, "TORRENTZIPPED-", 14);
}

static int zip_parse_file_init(file_archive_transfer_t *state,
      const char *file)
{
   uint8_t footer_buf[1024];
   uint8_t loc_buf[20], rec_buf[56];
   /* A mapped archive is read where it lies: the end record, the ZIP64
    * records and the central directory alike. */
#ifdef VFS_HAVE_FILE_MAPPING
   uint8_t *map = state->archive_mmap_data;
#else
   uint8_t *map = NULL;
#endif
   const uint8_t *footer = footer_buf;
   int64_t read_pos = state->archive_size;
   int64_t read_block = MIN(read_pos, (ssize_t)sizeof(footer_buf));
   int64_t eocd_at = 0;
   int64_t directory_size, directory_offset;
   uint64_t entries;
   zip_context_t *zip_context = NULL;

   state->torrentzip = false;

   /* Minimal ZIP file size is 22 bytes */
   if (read_block < 22)
      return -1;

   /* Find the end of central directory record by scanning
    * the file from the end towards the beginning.
    */
   if (map)
   {
      for (footer = map + (size_t)state->archive_size - 22; ; footer--)
      {
         if (read_le(footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
         {
            unsigned comment_len = read_le(footer + 20, 2);
            if ((int64_t)(footer - map) + 22 + comment_len
                  == state->archive_size)
            {
               state->torrentzip = zip_comment_torrentzip(footer + 22,
                     comment_len);
               break;
            }
         }
         if (footer == map)
            return -1;
      }
      eocd_at = (int64_t)(footer - map);
   }
   else
   {
      for (;;)
      {
         if (--footer < footer_buf)
         {
            if (read_pos <= 0)
               return -1; /* reached beginning of file */

            /* Read 21 bytes of overlaps except on the first block. */
            if (read_pos == state->archive_size)
               read_pos = read_pos - read_block;
            else
               read_pos = MAX(read_pos - read_block + 21, 0);

            /* Seek to read_pos and read read_block bytes. */
            filestream_seek(state->archive_file, read_pos, RETRO_VFS_SEEK_POSITION_START);
            if (filestream_read(state->archive_file, footer_buf, read_block) != read_block)
               return -1;

            footer = footer_buf + read_block - 22;
         }
         if (read_le(footer, 4) == END_OF_CENTRAL_DIR_SIGNATURE)
         {
            unsigned comment_len = read_le(footer + 20, 2);
            if (read_pos + (footer - footer_buf) + 22 + comment_len == state->archive_size)
            {
               eocd_at = read_pos + (footer - footer_buf);
               /* Only a comment inside this block is looked at. */
               state->torrentzip =
                     (int64_t)(footer - footer_buf) + 22 + comment_len
                        <= read_block
                  && zip_comment_torrentzip(footer + 22, comment_len);
               break; /* found it! */
            }
         }
      }
   }

   /* Read directory info and do basic sanity checks. */
   directory_size   = read_le(footer + 12, 4);
   directory_offset = read_le(footer + 16, 4);
   entries          = read_le(footer + 10, 2);

   /* A field at its largest says the ZIP64 record holds it: found
    * through the locator, the 20 octets before this record */
   if (     entries          == 0xFFFFu
         || directory_size   == 0xFFFFFFFFu
         || directory_offset == 0xFFFFFFFFu)
   {
      const uint8_t *loc = loc_buf, *rec = rec_buf;
      int64_t at = eocd_at;
      uint64_t rec_off;
      if (at < 20)
         return -1;
      if (map)
         loc = map + (size_t)(at - 20);
      else
      {
         filestream_seek(state->archive_file, at - 20,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file, loc_buf, 20) != 20)
            return -1;
      }
      if (read_le(loc, 4) != ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIGNATURE)
         return -1;
      rec_off = read_le64(loc + 8);
      if (rec_off > (uint64_t)(at - 20) || (uint64_t)(at - 20) - rec_off < 56)
         return -1;
      if (map)
         rec = map + (size_t)rec_off;
      else
      {
         filestream_seek(state->archive_file, (int64_t)rec_off,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file, rec_buf, 56) != 56)
            return -1;
      }
      if (read_le(rec, 4) != ZIP64_END_OF_CENTRAL_DIR_SIGNATURE)
         return -1;
      entries = read_le64(rec + 32);
      if (     read_le64(rec + 40) > (uint64_t)state->archive_size
            || read_le64(rec + 48) > (uint64_t)state->archive_size)
         return -1;
      directory_size   = (int64_t)read_le64(rec + 40);
      directory_offset = (int64_t)read_le64(rec + 48);
   }

   if (directory_size > state->archive_size
         || directory_offset > state->archive_size)
      return -1;

   /* Combined sanity check: the directory must fit entirely within
    * the archive.  Without this, offset + size could wrap or point
    * past EOF, producing a large bogus allocation and a short read. */
   if ((int64_t)directory_offset + (int64_t)directory_size > state->archive_size)
      return -1;

   /* Reject sizes that would overflow the allocation on 32-bit hosts.
    * size_t is 32-bit on 3DS / Vita / PSP / Wii / Wii U, where a
    * directory_size near UINT32_MAX would wrap
    *     sizeof(zip_context_t) + (size_t)directory_size
    * to a tiny value, after which directory_end runs off the end of
    * the allocation. */
   if ((size_t)directory_size > SIZE_MAX - sizeof(zip_context_t))
      return -1;

   /* This is a ZIP file, allocate one block of memory for both the
    * context and the entire directory, then read the directory; a
    * mapped one is walked in place.
    */
   zip_context = (zip_context_t*)malloc(sizeof(zip_context_t)
         + (map ? 0 : (size_t)directory_size));
   if (!zip_context)
      return -1;
   zip_context->state             = state;
   zip_context->directory         = map ? map + (size_t)directory_offset
                                        : (uint8_t*)(zip_context + 1);
   zip_context->directory_entry   = zip_context->directory;
   zip_context->directory_end     = zip_context->directory + (size_t)directory_size;
   zip_context->zstream           = NULL;
#ifndef ARCHIVE_HAVE_ZLIB
   zip_context->out_bound         = 0;
#endif
   zip_context->tmpbuf            = NULL;
   zip_context->tmpbuf_len        = 0;
   zip_context->bz                = NULL;
   zip_context->z64               = NULL;
   zip_context->zs                = NULL;
   zip_context->lz                = NULL;
   zip_context->lz_probs          = NULL;
   zip_context->begun             = false;
   zip_context->in_have           = 0;
   zip_context->entry_crc         = 0;
   zip_context->expect_crc        = 0;
   zip_context->crc               = 0;
   zip_context->out_off           = 0;
   zip_context->decompressed_data = NULL;
   zip_context->inflating         = false;
   zip_context->fdoffset          = 0;
   zip_context->entry_offset      = 0;
   zip_context->csize             = 0;
   zip_context->usize             = 0;

   if (!map)
   {
      filestream_seek(state->archive_file, directory_offset, RETRO_VFS_SEEK_POSITION_START);
      if (filestream_read(state->archive_file, zip_context->directory, directory_size) != directory_size)
      {
         free(zip_context);
         return -1;
      }
   }

   state->context = zip_context;
   state->step_total = entries > (uint64_t)UINT_MAX
      ? UINT_MAX : (unsigned)entries;

   return 0;
}

/* macOS Archive Utility writes a resource-fork sidecar for every member
 * it stores, parked under a top-level "__MACOSX" directory and named
 * after the member with a "._" prefix.  These are never content, and
 * they sort ahead of the real file, so anything picking the first entry
 * out of a Mac-made archive picks the sidecar.
 *
 * The central-directory name length is already in hand at the only call
 * site, so this needs neither strlen nor a scan of the whole entry name
 * to find the basename. */
static int zip_entry_is_appledouble(const uint8_t *name, uint32_t len)
{
   uint32_t i;

   if (     len >= 8
         && memcmp(name, "__MACOSX", 8) == 0
         && (len == 8 || name[8] == '/' || name[8] == '\\'))
      return 1;

   i = len;
   while (i > 0 && name[i - 1] != '/' && name[i - 1] != '\\')
      i--;

   return (len - i >= 2 && name[i] == '.' && name[i + 1] == '_');
}

/* The ZIP64 extra field among @extra's @len octets: each of *@size,
 * *@csize and *@offset that is at its 32-bit largest is read from it,
 * in that order. false when one is wanted and the field does not hold
 * it. */
static bool zip64_extra(const uint8_t *extra, uint32_t len,
      uint64_t *size, uint64_t *csize, uint64_t *offset)
{
   uint64_t *want[3];
   unsigned  n = 0, i;
   if (*size   == 0xFFFFFFFFu)
      want[n++] = size;
   if (*csize  == 0xFFFFFFFFu)
      want[n++] = csize;
   if (*offset == 0xFFFFFFFFu)
      want[n++] = offset;
   if (!n)
      return true;
   while (len >= 4)
   {
      uint32_t id = read_le(extra, 2), sz = read_le(extra + 2, 2);
      if (sz > len - 4)
         return false;
      if (id == ZIP64_EXTRA_ID)
      {
         if (sz < 8 * n)
            return false;
         for (i = 0; i < n; i++)
            *want[i] = read_le64(extra + 4 + 8 * i);
         return true;
      }
      extra += 4 + sz;
      len   -= 4 + sz;
   }
   return false;
}

static int zip_parse_file_iterate_step_internal(
      zip_context_t * zip_context, char *filename,
      const uint8_t **cdata,
      unsigned *cmode, uint64_t *size, uint64_t *csize,
      uint32_t *checksum, unsigned *payback)
{
   uint8_t *entry;
   uint32_t signature, namelength, extralength, commentlength;
   uint64_t offset;

again:
   entry = zip_context->directory_entry;

   if (entry < zip_context->directory || entry >= zip_context->directory_end)
      return 0;

   /* Central-directory fixed header is 46 bytes (highest-offset read
    * is at +42..+45).  Reject a truncated trailing entry before any
    * out-of-bounds read. */
   if ((size_t)(zip_context->directory_end - entry) < 46)
      return -1;

   signature = read_le(zip_context->directory_entry + 0, 4);

   if (signature != CENTRAL_FILE_HEADER_SIGNATURE)
      return 0;

   /* 0 store, 8 deflate, 9 deflate64, 12 bzip2, 14 lzma, 93 zstd */
   *cmode         = read_le(zip_context->directory_entry + 10, 2);
   *checksum      = read_le(zip_context->directory_entry + 16, 4); /* CRC32 */
   *csize         = read_le(zip_context->directory_entry + 20, 4); /* compressed size */
   *size          = read_le(zip_context->directory_entry + 24, 4); /* uncompressed size */

   namelength     = read_le(zip_context->directory_entry + 28, 2); /* file name length */
   extralength    = read_le(zip_context->directory_entry + 30, 2); /* extra field length */
   commentlength  = read_le(zip_context->directory_entry + 32, 2); /* file comment length */

   if (namelength >= PATH_MAX_LENGTH)
      return -1;

   /* Variable-length fields (name, extra, comment) follow the 46-byte
    * fixed header.  Reject if the declared sizes would run past the
    * end of the directory block. */
   if ((size_t)(zip_context->directory_end - entry)
         < (size_t)46 + namelength + extralength + commentlength)
      return -1;

   /* Drop AppleDouble sidecars before the name is even copied out.
    * Doing it here rather than in the listing callback covers every
    * consumer of the walk - listing, extraction and the incremental
    * entry source alike - and keeps the whole check off the callback
    * hot path.  The end-of-central-directory record counted these
    * entries, so take them back out of the progress denominator. */
   if (zip_entry_is_appledouble(zip_context->directory_entry + 46, namelength))
   {
      zip_context->directory_entry += 46 + namelength
         + extralength + commentlength;

      if (zip_context->state && zip_context->state->step_total > 0)
         zip_context->state->step_total--;

      goto again;
   }

   memcpy(filename, zip_context->directory_entry + 46, namelength); /* file name */
   filename[namelength] = '\0';

   offset   = read_le(zip_context->directory_entry + 42, 4); /* relative offset of local file header */

   if (!zip64_extra(zip_context->directory_entry + 46 + namelength,
            extralength, size, csize, &offset))
      return -1;

   /* the offset is kept whole for the member's init; the pointer
    * carries it where it fits */
   zip_context->entry_offset = offset;
   *cdata   = (uint8_t*)(size_t)offset;

   *payback = 46 + namelength + extralength + commentlength;

   return 1;
}

static int zip_parse_file_iterate_step(void *context,
      const char *valid_exts, struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   const uint8_t *cdata       = NULL;
   uint32_t checksum          = 0;
   uint64_t size              = 0;
   uint64_t csize             = 0;
   unsigned cmode             = 0;
   unsigned payload           = 0;
   int ret                    = zip_parse_file_iterate_step_internal(zip_context,
         userdata->current_file_path, &cdata, &cmode, &size, &csize, &checksum, &payload);

   if (ret != 1)
      return ret;

   userdata->crc  = checksum;
   userdata->size = size;
   zip_context->entry_crc = checksum;

   if (file_cb && !file_cb(userdata->current_file_path, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      return 0;

   zip_context->directory_entry += payload;

   return 1;
}

static void zip_parse_file_free(void *context)
{
   zip_context_t *zip_context = (zip_context_t *)context;
   zip_context_free_stream(zip_context, false);
   rinflate64_free(zip_context->z64);
#ifdef ZIP_HAVE_ZSTD
   rzstd_blocks_free((rzstd_blocks_t*)zip_context->zs);
#endif
   free(zip_context->lz);
   free(zip_context->lz_probs);
#ifdef HAVE_RBZIP2
   rbzip2_free((rbzip2_t*)zip_context->bz);
#endif
   free(zip_context);
}

/* ---- incremental archive entry source (see archive_file.h) ---- */

struct file_archive_entry_source
{
   file_archive_transfer_t state;      /* keeps the mapping/file open */
   int64_t  fdoffset;                  /* entry data start in archive */
   uint64_t csize, usize;
   uint64_t in_off, out_off;
   uint32_t crc;                       /* the member's, recorded      */
   unsigned cmode;
   void    *z;                         /* z_stream / rinflate handle  */
   void    *bz;                        /* the bzip2 decoder           */
   uint8_t *tmpbuf;                    /* input chunks without mmap   */
   size_t   tmpbuf_len;
   uint8_t *out_expect;                /* rinflate cursor guard       */
   const char *needle;                 /* entry name, during open only */
   uint8_t  found;
};

/* capture callback: record the needle entry's location, extract nothing */
static int file_archive_entry_source_capture(
      const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode,
      uint64_t csize, uint64_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata)
{
   file_archive_entry_source_t *s =
         (file_archive_entry_source_t*)userdata->cb_data;
   size_t name_len = name ? strlen(name) : 0;
   if (name_len == 0)
      return 1;
   if (name[name_len - 1] == '/' || name[name_len - 1] == '\\')
      return 1;
   if (!string_is_equal(name, s->needle))
      return 1;
   /* the local-header hop, both arms, mirroring the extract init: the
    * offset and the sizes are the archive's word, and the reads made
    * from the mapping are bounded by them */
   {
      struct file_archive_transfer *state = &s->state;
      /* the walk's own word for where the entry is: cdata cannot hold
       * an offset past 4 GiB on a 32-bit host */
      uint64_t local = ((zip_context_t*)state->context)->entry_offset;
      uint8_t local_header_buf[4];
      uint8_t *local_header;
      uint32_t nl, el;
      (void)cdata;
      if (state->archive_size < 30
            || local > (uint64_t)state->archive_size - 30)
         return -1;
#ifdef VFS_HAVE_FILE_MAPPING
      if (state->archive_mmap_data)
         local_header = state->archive_mmap_data + (size_t)local + 26;
      else
#endif
      {
         filestream_seek(state->archive_file, (int64_t)local + 26,
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(state->archive_file, local_header_buf, 4) != 4)
            return -1;
         local_header = local_header_buf;
      }
      nl          = read_le(local_header,     2);
      el          = read_le(local_header + 2, 2);
      s->fdoffset = (int64_t)local + 26 + 4 + nl + el;
      if (     s->fdoffset > state->archive_size
            || csize > (uint64_t)(state->archive_size - s->fdoffset)
            || (     cmode == ZIP_MODE_STORED
                  && size > (uint64_t)(state->archive_size - s->fdoffset)))
         return -1;
   }
   s->csize = csize;
   s->usize = size;
   s->crc   = crc32;
   s->cmode = cmode;
   s->found = 1;
   return 1;
}

file_archive_entry_source_t *file_archive_entry_source_open(
      const char *path, int64_t *usize)
{
   file_archive_entry_source_t *s   = NULL;
   char *archive                    = NULL;
   const char *delim                = NULL;
   struct archive_extract_userdata userdata = {0};
   int ret                          = 0;

   if (file_archive_get_file_backend(path) != &zlib_backend)
      return NULL;                  /* 7z solid blocks: classic path */
   if (!(delim = path_get_archive_delim(path)) || !delim[1])
      return NULL;
   if (!(archive = (char*)malloc((size_t)(delim - path) + 1)))
      return NULL;
   memcpy(archive, path, (size_t)(delim - path));
   archive[delim - path] = '\0';
   if (!(s = (file_archive_entry_source_t*)calloc(1, sizeof(*s))))
      goto error;

   s->state.type         = ARCHIVE_TRANSFER_INIT;
   userdata.transfer     = &s->state;
   userdata.cb_data      = s;
   s->needle             = delim + 1;

   do
   {
      bool returnerr = true;
      ret = file_archive_parse_file_iterate(&s->state, &returnerr,
            archive, "",
            file_archive_entry_source_capture, &userdata);
      if (!returnerr)
         break;
   } while (ret == 0 && !s->found);

   if (!s->found)
      goto error_stop;

   /* what is decoded or bound whole has to fit in memory */
   if (s->cmode != ZIP_MODE_STORED && s->usize > (uint64_t)((size_t)-1))
      goto error_stop;

   if (s->cmode == ZIP_MODE_DEFLATED)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      z_stream *z = (z_stream*)calloc(1, sizeof(z_stream));
      if (!z || inflateInit2(z, -MAX_WBITS) != Z_OK)
      {
         free(z);
         goto error_stop;
      }
      s->z = z;
#else
      if (!(s->z = rinflate_new(-15)))
         goto error_stop;
#endif
#ifdef VFS_HAVE_FILE_MAPPING
      if (!s->state.archive_mmap_data)
#endif
      {
         if (!(s->tmpbuf = (uint8_t*)malloc(_READ_CHUNK_SIZE)))
            goto error_stop;
         s->tmpbuf_len = _READ_CHUNK_SIZE;
      }
   }
   else if (s->cmode != ZIP_MODE_STORED && !ZIP_MODE_WHOLE(s->cmode))
      goto error_stop;               /* unknown method */

   free(archive);
   s->needle = NULL;               /* borrowed from caller's path */
   if (usize)
      *usize = (int64_t)s->usize;
   return s;

error_stop:
   file_archive_parse_file_iterate_stop(&s->state);
error:
   if (s)
   {
      free(s->tmpbuf);
      free(s);
   }
   free(archive);
   return NULL;
}

int64_t file_archive_entry_source_read(file_archive_entry_source_t *s,
      uint8_t *dst, int64_t n)
{
   int64_t remain_out = (int64_t)s->usize - (int64_t)s->out_off;
   if (n > remain_out)
      n = remain_out;
   if (n <= 0)
      return 0;

   if (s->cmode == ZIP_MODE_STORED)
   {
#ifdef VFS_HAVE_FILE_MAPPING
      if (s->state.archive_mmap_data)
         memcpy(dst, s->state.archive_mmap_data
               + (size_t)(s->fdoffset + s->out_off), (size_t)n);
      else
#endif
      {
         filestream_seek(s->state.archive_file,
               (int64_t)(s->fdoffset + s->out_off),
               RETRO_VFS_SEEK_POSITION_START);
         if (filestream_read(s->state.archive_file, dst, n) != n)
            return -1;
      }
      s->out_off += (uint64_t)n;
      return n;
   }

   /* The modes decoded whole do not stop part way: the first read
    * decodes all of it straight into dst, which has room to the
    * member's end */
   if (ZIP_MODE_WHOLE(s->cmode))
   {
      const uint8_t *src;
      if (s->out_off)
         return -1;
      if (     !(src = zip_member_whole(&s->state, s->fdoffset, s->csize,
                  &s->tmpbuf, &s->tmpbuf_len))
            || !zip_decode_whole(s->cmode, &s->bz, dst, (size_t)s->usize,
                  src, (size_t)s->csize, s->crc))
         return -1;
      s->out_off = s->usize;
      return (int64_t)s->usize;
   }

   /* DEFLATED: inflate directly into dst until n produced or end */
#ifdef ARCHIVE_HAVE_ZLIB
   {
      z_stream *z = (z_stream*)s->z;
      /* zlib counts in 32 bits */
      if (n > 0x40000000)
         n = 0x40000000;
      z->next_out  = dst;
      z->avail_out = (uInt)n;
      while (z->avail_out > 0)
      {
         if (z->avail_in == 0)
         {
            uint64_t chunk = s->csize - s->in_off;
            if (chunk == 0)
               break;
#ifdef VFS_HAVE_FILE_MAPPING
            if (s->state.archive_mmap_data)
            {
               if (chunk > 0x40000000)
                  chunk = 0x40000000;
               z->next_in  = s->state.archive_mmap_data
                     + (size_t)(s->fdoffset + s->in_off);
               z->avail_in = (uInt)chunk;
            }
            else
#endif
            {
               const uint8_t *iptr;
               int64_t rd;
               if (chunk > _READ_CHUNK_SIZE)
                  chunk = _READ_CHUNK_SIZE;
               rd = zip_member_input(s->state.archive_file,
                     (int64_t)(s->fdoffset + s->in_off), (int64_t)chunk,
                     s->tmpbuf, &iptr);
               if (rd <= 0)
                  return -1;
               z->next_in  = (Bytef*)iptr;
               z->avail_in = (uInt)rd;
               chunk       = (uint64_t)rd;
            }
            s->in_off += chunk;
         }
         {
            int zr = inflate(z, 0);
            if (zr == Z_STREAM_END)
               break;
            if (zr < 0)
               return -1;
         }
      }
      {
         int64_t produced = n - (int64_t)z->avail_out;
         s->out_off += (uint64_t)produced;
         return produced;
      }
   }
#else
   {
      /* The built-in inflate binds its whole output window once and
       * keeps its own cursor, so per-call production is capped by
       * input granularity, not by n: this arm may overshoot the
       * pacing hint (the API permits it - dst always has room to the
       * entry's end).  Verify the caller honours the sequential-
       * contiguous dst contract before trusting the binding. */
      int64_t produced = 0;
      if (!s->out_expect)
      {
         rinflate_set_out(s->z, dst, (size_t)s->usize);
         s->out_expect = dst;
      }
      else if (dst != s->out_expect)
         return -1;
      while (produced < n)
      {
         uint64_t chunk = s->csize - s->in_off;
         const uint8_t *iptr;
         if (chunk == 0)
            break;
#ifdef VFS_HAVE_FILE_MAPPING
         if (s->state.archive_mmap_data)
         {
            if (chunk > _READ_CHUNK_SIZE)
               chunk = _READ_CHUNK_SIZE;
            iptr = s->state.archive_mmap_data
                  + (size_t)(s->fdoffset + s->in_off);
         }
         else
#endif
         {
            int64_t rd;
            if (chunk > _READ_CHUNK_SIZE)
               chunk = _READ_CHUNK_SIZE;
            rd = zip_member_input(s->state.archive_file,
                  (int64_t)(s->fdoffset + s->in_off), (int64_t)chunk,
                  s->tmpbuf, &iptr);
            if (rd <= 0)
               return -1;
            chunk = (uint64_t)rd;
         }
         rinflate_set_in(s->z, iptr, (size_t)chunk);
         s->in_off += chunk;
         for (;;)
         {
            size_t got_in = 0, got_out = 0;
            int st = rinflate_process(s->z, &got_in, &got_out);
            produced += (int64_t)got_out;
            if (st == RDEFLATE_PROCESS_ERROR)
               return -1;
            if (st == RDEFLATE_PROCESS_END)
            {
               s->in_off = s->csize;   /* drained */
               break;
            }
            if (got_in == 0 && got_out == 0)
               break;                  /* chunk consumed */
         }
      }
      s->out_off    += (uint64_t)produced;
      s->out_expect += produced;
      return produced;
   }
#endif
}

void file_archive_entry_source_close(file_archive_entry_source_t *s)
{
   if (!s)
      return;
   if (s->z)
   {
#ifdef ARCHIVE_HAVE_ZLIB
      inflateEnd((z_stream*)s->z);
      free(s->z);
#else
      rinflate_free(s->z);
#endif
   }
#ifdef HAVE_RBZIP2
   rbzip2_free((rbzip2_t*)s->bz);
#endif
   free(s->tmpbuf);
   file_archive_parse_file_iterate_stop(&s->state);
   free(s);
}

const struct file_archive_file_backend zlib_backend = {
   zip_parse_file_init,
   zip_parse_file_iterate_step,
   zip_parse_file_free,
   zlib_stream_decompress_data_to_file_init,
   zlib_stream_decompress_data_to_file_iterate,
   encoding_crc32,
   zip_file_read,
   "zlib"
};
