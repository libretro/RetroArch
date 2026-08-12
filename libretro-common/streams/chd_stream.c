/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_stream.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>

#include <streams/chd_stream.h>
#include <retro_endianness.h>
#ifdef HAVE_RCHD
#include <formats/rchd.h>
#include <streams/file_stream.h>
#else
#include <libchdr/chd.h>
#endif

#define SECTOR_RAW_SIZE 2352
#define SECTOR_SIZE 2048
#define SUBCODE_SIZE 96
#define TRACK_PAD 4

struct chdstream
{
#ifdef HAVE_RCHD
   rchd_t   *chd;
   /* rchd never touches a file: it names the byte ranges it needs and
    * this supplies them, so the stream owns the handle. */
   RFILE    *file;
   uint32_t  hunk_bytes;
   uint32_t  unit_bytes;
#else
   chd_file *chd;
#endif
   /* Loaded hunk */
   uint8_t *hunkmem;
   /* Byte offset where track data starts (after pregap) */
   size_t track_start;
   /* Byte offset where track data ends */
   size_t track_end;
   /* Byte offset of read cursor */
   size_t offset;
   /* Loaded hunk number */
   int32_t hunknum;
   /* Size of frame taken from each hunk */
   uint32_t frame_size;
   /* Offset of data within frame */
   uint32_t frame_offset;
   /* Number of frames per hunk */
   uint32_t frames_per_hunk;
   /* First frame of track in chd */
   uint32_t track_frame;
   /* Should we swap bytes? */
   bool swab;
};

#ifndef HAVE_RCHD
typedef struct metadata
{
   uint32_t frame_offset;
   uint32_t frames;
   uint32_t pad;
   uint32_t extra;
   uint32_t pregap;
   uint32_t postgap;
   uint32_t track;
   char type[64];
   char subtype[32];
   char pgtype[32];
   char pgsub[32];
} metadata_t;

static uint32_t padding_frames(uint32_t frames)
{
   return ((frames + TRACK_PAD - 1) & ~(TRACK_PAD - 1)) - frames;
}

static bool
chdstream_get_meta(chd_file *chd, int idx, metadata_t *md)
{
   char meta[256];
   uint32_t meta_size = 0;
   chd_error err;

   meta[0] = '\0';

   memset(md, 0, sizeof(*md));

   err = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, idx, meta,
         sizeof(meta), &meta_size, NULL, NULL);

   if (err == CHDERR_NONE)
   {
      const char *ptr = meta;
      size_t remaining = strlen(meta);

      while (remaining)
      {
         size_t klen, vlen;
         const char *val;
         const char *end;
         const char *key = ptr;
         const char *sep = (const char *)memchr(key, ':', remaining);
         if (!sep)
            break;

         klen       = sep - key;
         val        = sep + 1;
         remaining -= klen + 1;

         end        = (const char *)memchr(val, ' ', remaining);
         vlen       = end ? (size_t)(end - val) : remaining;

         if (klen == 5 && !memcmp(key, "TRACK", 5))
            md->track = (unsigned)strtoul(val, NULL, 10);
         else if (klen == 4 && !memcmp(key, "TYPE", 4))
            strlcpy(md->type, val, vlen + 1 < sizeof(md->type) ? vlen + 1 : sizeof(md->type));
         else if (klen == 7 && !memcmp(key, "SUBTYPE", 7))
            strlcpy(md->subtype, val, vlen + 1 < sizeof(md->subtype) ? vlen + 1 : sizeof(md->subtype));
         else if (klen == 6 && !memcmp(key, "FRAMES", 6))
            md->frames = (unsigned)strtoul(val, NULL, 10);
         else if (klen == 6 && !memcmp(key, "PREGAP", 6))
            md->pregap = (unsigned)strtoul(val, NULL, 10);
         else if (klen == 6 && !memcmp(key, "PGTYPE", 6))
            strlcpy(md->pgtype, val, vlen + 1 < sizeof(md->pgtype) ? vlen + 1 : sizeof(md->pgtype));
         else if (klen == 5 && !memcmp(key, "PGSUB", 5))
            strlcpy(md->pgsub, val, vlen + 1 < sizeof(md->pgsub) ? vlen + 1 : sizeof(md->pgsub));
         else if (klen == 7 && !memcmp(key, "POSTGAP", 7))
            md->postgap = (unsigned)strtoul(val, NULL, 10);

         ptr = end ? end + 1 : val + vlen;
         remaining -= end ? vlen + 1 : vlen;
      }

      md->extra = padding_frames(md->frames);
      return true;
   }

   err = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, idx, meta,
         sizeof(meta), &meta_size, NULL, NULL);

   if (err == CHDERR_NONE)
   {
      size_t len;
      const char *start;
      const char *p = meta;

      if (strncmp(p, "TRACK:", 6) != 0)
         return false;
      p += 6;
      md->track = strtoul(p, (char **)&p, 10);

      if (*p++ != ' ' || strncmp(p, "TYPE:", 5) != 0)
         return false;
      p     += 5;
      start  = p;
      while (*p && *p != ' ')
         p++;
      len = p - start;
      if (len == 0 || len >= sizeof(md->type))
         return false;
      memcpy(md->type, start, len);
      md->type[len] = '\0';

      if (*p++ != ' ' || strncmp(p, "SUBTYPE:", 8) != 0)
         return false;
      p += 8;
      start = p;
      while (*p && *p != ' ')
         p++;
      len = p - start;
      if (len == 0 || len >= sizeof(md->subtype))
         return false;
      memcpy(md->subtype, start, len);
      md->subtype[len] = '\0';

      if (*p++ != ' ' || strncmp(p, "FRAMES:", 7) != 0)
         return false;
      p += 7;
      md->frames = strtoul(p, NULL, 10);
      md->extra  = padding_frames(md->frames);
      return true;
   }

   err = chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, idx, meta,
         sizeof(meta), &meta_size, NULL, NULL);

   if (err == CHDERR_NONE)
   {
      char *p = meta;
      while (*p)
      {
         size_t len;

         if (strncmp(p, "TRACK:", 6) == 0)
            md->track = strtoul(p + 6, &p, 10);
         else if (strncmp(p, "TYPE:", 5) == 0)
         {
            p   += 5;
            len  = 0;
            while (p[len] && p[len] != ' ')
               len++;
            /* Pre-this-patch the memcpy/terminator was unbounded.
             * GDROM_TRACK_METADATA_TAG is read from the .chd
             * file (chd_get_metadata above) and the value field
             * after "TYPE:" terminates at the next space; with a
             * malicious .chd containing no space between "TYPE:"
             * and a long run of bytes, len can reach almost the
             * full meta[256] buffer and the memcpy below
             * stack-overflows md->type (char[64]) into adjacent
             * frame fields (subtype, pgtype, pgsub, then return
             * address depending on layout).  Reject as malformed
             * if the value can't fit in the destination.  Same
             * guard as the SCD-format parser at line 166. */
            if (len >= sizeof(md->type))
               return false;
            memcpy(md->type, p, len);
            md->type[len] = '\0';
            p += len;
         }
         else if (strncmp(p, "SUBTYPE:", 8) == 0)
         {
            p   += 8;
            len  = 0;
            while (p[len] && p[len] != ' ')
               len++;
            if (len >= sizeof(md->subtype))
               return false;
            memcpy(md->subtype, p, len);
            md->subtype[len] = '\0';
            p += len;
         }
         else if (strncmp(p, "FRAMES:", 7) == 0)
            md->frames = strtoul(p + 7, &p, 10);
         else if (strncmp(p, "PAD:", 4) == 0)
            md->pad = strtoul(p + 4, &p, 10);
         else if (strncmp(p, "PREGAP:", 7) == 0)
            md->pregap = strtoul(p + 7, &p, 10);
         else if (strncmp(p, "PGTYPE:", 7) == 0)
         {
            p   += 7;
            len  = 0;
            while (p[len] && p[len] != ' ')
               len++;
            if (len >= sizeof(md->pgtype))
               return false;
            memcpy(md->pgtype, p, len);
            md->pgtype[len] = '\0';
            p += len;
         }
         else if (strncmp(p, "PGSUB:", 6) == 0)
         {
            p   += 6;
            len  = 0;
            while (p[len] && p[len] != ' ')
               len++;
            if (len >= sizeof(md->pgsub))
               return false;
            memcpy(md->pgsub, p, len);
            md->pgsub[len] = '\0';
            p += len;
         }
         else if (strncmp(p, "POSTGAP:", 8) == 0)
            md->postgap = strtoul(p + 8, &p, 10);
         else
            p++;
      }
      md->extra = padding_frames(md->frames);
      return true;
   }

   err = chd_get_metadata(chd, DVD_METADATA_TAG, idx, meta,
         sizeof(meta), &meta_size, NULL, NULL);

   if (err == CHDERR_NONE)
   {
      md->track = 1;
      strlcpy(md->type, "DVD", sizeof(md->type));
      return true;
   }

   return false;
}

static bool
chdstream_find_track_number(chd_file *fd, int32_t track, metadata_t *meta)
{
   uint32_t i;
   uint32_t frame_offset = 0;

   for (i = 0; true; ++i)
   {
      if (!chdstream_get_meta(fd, i, meta))
         return false;

      if (track == (int)meta->track)
      {
         meta->frame_offset = frame_offset;
         return true;
      }

      frame_offset += meta->frames + meta->extra;
   }
}

static bool
chdstream_find_special_track(chd_file *fd, int32_t track, metadata_t *meta)
{
   int32_t i;
   metadata_t iter;
   int32_t largest_track = 0;
   uint32_t largest_size = 0;

   for (i = 1; true; ++i)
   {
      if (!chdstream_find_track_number(fd, i, &iter))
      {
         if (track == CHDSTREAM_TRACK_LAST && i > 1)
            return chdstream_find_track_number(fd, i - 1, meta);

         if (track == CHDSTREAM_TRACK_PRIMARY && largest_track != 0)
            return chdstream_find_track_number(fd, largest_track, meta);

         return false;
      }

      switch (track)
      {
         case CHDSTREAM_TRACK_FIRST_DATA:
            if (strcmp(iter.type, "AUDIO"))
            {
               *meta = iter;
               return true;
            }
            break;
         case CHDSTREAM_TRACK_PRIMARY:
            /* DVD tracks have no frame count in metadata but are always
             * the primary data track - select immediately */
            if (strcmp(iter.type, "DVD") == 0)
            {
               *meta = iter;
               return true;
            }
            if (strcmp(iter.type, "AUDIO") && iter.frames > largest_size)
            {
               largest_size  = iter.frames;
               largest_track = iter.track;
            }
            break;
         default:
            break;
      }
   }
}

static bool
chdstream_find_track(chd_file *fd, int32_t track, metadata_t *meta)
{
   if (track < 0)
      return chdstream_find_special_track(fd, track, meta);
   return chdstream_find_track_number(fd, track, meta);
}
#endif  /* !HAVE_RCHD */

#ifdef HAVE_RCHD
/* Drives an rchd step loop, supplying whatever range it asks for from
 * the open file. rchd asks for one range at a time and says so by
 * returning RCHD_PENDING, so this is the whole of the I/O. */
static bool chdstream_pump(RFILE *f, rchd_t *chd, int opening,
      rchd_request_t *rq)
{
   static uint8_t buf[65536];

   for (;;)
   {
      int    e = opening ? rchd_open_step(chd, rq)
                         : rchd_read_step(chd, rq);
      size_t want, got;

      if (e == RCHD_OK)
         return true;
      if (e != RCHD_PENDING)
         return false;

      want = rq->length > sizeof(buf) ? sizeof(buf) : rq->length;
      if (filestream_seek(f, (int64_t)rq->offset,
               RETRO_VFS_SEEK_POSITION_START) < 0)
         return false;
      got = (size_t)filestream_read(f, buf, want);
      if (!got)
         return false;
      if (rchd_feed(chd, buf, got) != RCHD_OK)
         return false;
   }
}

/* The track the caller asked for. Track numbers are 1-based and need
 * not be contiguous, so this searches rather than indexes; the special
 * values match what the libchdr path accepted. */
static const rchd_track_t *chdstream_track_of(rchd_t *chd, int32_t track)
{
   uint32_t n = rchd_track_count(chd), i;

   if (!n)
      return NULL;
   if (track == CHDSTREAM_TRACK_FIRST_DATA)
   {
      for (i = 0; i < n; i++)
      {
         const rchd_track_t *t = rchd_track(chd, i);

         if (t && t->type != RCHD_TRACK_AUDIO)
            return t;
      }
      return NULL;
   }
   if (track == CHDSTREAM_TRACK_PRIMARY)
   {
      const rchd_track_t *best = NULL;

      for (i = 0; i < n; i++)
      {
         const rchd_track_t *t = rchd_track(chd, i);

         if (!t)
            continue;
         /* A DVD image describes no tracks, so rchd makes one standing
          * for the whole of it. That is the primary track by
          * definition, and it is taken without comparing sizes -- the
          * text form this replaces special-cased "DVD" the same way,
          * because a DVD track states no frame count to compare. */
         if (t->synthesised)
            return t;
         if (t->type == RCHD_TRACK_AUDIO)
            continue;
         if (!best || t->frames > best->frames)
            best = t;
      }
      /* No data track at all means no primary track. An audio-only
       * image has none, and answering with its first audio track would
       * hand back something the caller did not ask for. */
      return best;
   }
   if (track == CHDSTREAM_TRACK_LAST)
      return rchd_track(chd, n - 1);

   for (i = 0; i < n; i++)
   {
      const rchd_track_t *t = rchd_track(chd, i);

      if (t && (int32_t)t->track == track)
         return t;
   }
   return NULL;
}
#endif  /* HAVE_RCHD */

#ifdef HAVE_RCHD
chdstream_t *chdstream_open(const char *path, int32_t track)
{
   const rchd_track_t *t;
   const rchd_info_t  *info;
   rchd_request_t      rq;
   chdstream_t        *stream = NULL;
   rchd_t             *chd    = NULL;
   RFILE              *file   = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint32_t            pregap = 0;

   if (!file)
      return NULL;
   if (!(chd = rchd_new()))
      goto error;
   if (!chdstream_pump(file, chd, 1, &rq))
      goto error;
   if (!(t = chdstream_track_of(chd, track)))
      goto error;
   if (!(stream = (chdstream_t*)malloc(sizeof(*stream))))
      goto error;

   info                    = rchd_info(chd);
   stream->chd             = NULL;
   stream->file            = NULL;
   stream->hunk_bytes      = info->hunk_bytes;
   stream->unit_bytes      = info->unit_bytes;
   stream->swab            = false;
   stream->frame_size      = 0;
   stream->frame_offset    = 0;
   stream->frames_per_hunk = 0;
   stream->track_frame     = 0;
   stream->track_start     = 0;
   stream->track_end       = 0;
   stream->offset          = 0;
   stream->hunknum         = -1;
   if (!(stream->hunkmem = (uint8_t*)malloc(info->hunk_bytes)))
      goto error;

   /* rchd reports the track's type rather than the text it was written
    * as, so this is a switch on a number where the libchdr path parsed
    * a string and compared characters of it. */
   switch (t->type)
   {
      case RCHD_TRACK_MODE1_RAW:
      case RCHD_TRACK_MODE2_RAW:
         stream->frame_size = SECTOR_RAW_SIZE;
         break;
      case RCHD_TRACK_MODE1:
         stream->frame_size = SECTOR_SIZE;
         break;
      case RCHD_TRACK_AUDIO:
         stream->frame_size = SECTOR_RAW_SIZE;
         stream->swab       = true;
         break;
      default:
         stream->frame_size = t->data_size ? t->data_size
                                           : info->unit_bytes;
         break;
   }

   /* A pregap is skipped only when the file actually holds those
    * frames. A virtual one is on the disc but not in the image, so the
    * track's data begins where the track begins and skipping would
    * return silence and then read everything after it from the wrong
    * place. */
   if (t->pregap_stored)
      pregap               = t->pregap;

   stream->chd             = chd;
   stream->file            = file;
   stream->frames_per_hunk = info->hunk_bytes / info->unit_bytes;
   stream->track_frame     = (uint32_t)(t->logical_offset
                                        / info->unit_bytes);
   stream->track_start     = (size_t)pregap * stream->frame_size;
   stream->track_end       = stream->track_start
                           + (size_t)t->frames * stream->frame_size;
   return stream;

error:
   if (stream)
   {
      free(stream->hunkmem);
      free(stream);
   }
   if (chd)
      rchd_free(chd);
   if (file)
      filestream_close(file);
   return NULL;
}
#else
chdstream_t *chdstream_open(const char *path, int32_t track)
{
   metadata_t meta;
   uint32_t pregap         = 0;
   uint8_t *hunkmem        = NULL;
   const chd_header *hd    = NULL;
   chdstream_t *stream     = NULL;
   chd_file *chd           = NULL;
   chd_error err           = chd_open(path, CHD_OPEN_READ, NULL, &chd);
   if (err != CHDERR_NONE)
      return NULL;
   if (!chdstream_find_track(chd, track, &meta))
      goto error;
   stream                  = (chdstream_t*)malloc(sizeof(*stream));
   if (!stream)
      goto error;
   stream->chd             = NULL;
   stream->swab            = false;
   stream->frame_size      = 0;
   stream->frame_offset    = 0;
   stream->frames_per_hunk = 0;
   stream->track_frame     = 0;
   stream->track_start     = 0;
   stream->track_end       = 0;
   stream->offset          = 0;
   stream->hunkmem         = NULL;
   stream->hunknum         = -1;
   hd                      = chd_get_header(chd);
   hunkmem                 = (uint8_t*)malloc(hd->hunkbytes);
   if (!hunkmem)
      goto error;
   stream->hunkmem         = hunkmem;
   switch (meta.type[0])
   {
      case 'M':
         if (meta.type[5] == '_')
         {
            if (meta.type[6] == 'R') /* MODE1_RAW or MODE2_RAW */
               stream->frame_size = SECTOR_RAW_SIZE;
            else /* MODE2_FORM... (unhandled, treat like default)*/
               stream->frame_size = hd->unitbytes;
         }
         else /* MODE1 */
            stream->frame_size = SECTOR_SIZE;
         break;
      case 'A': /* AUDIO */
         stream->frame_size   = SECTOR_RAW_SIZE;
         stream->swab         = true;
         break;
      case 'D': /* DVD */
         stream->frame_size   = hd->unitbytes;
         meta.frames          = hd->totalhunks * (hd->hunkbytes / hd->unitbytes);
         break;
      default:
         stream->frame_size   = hd->unitbytes;
         break;
   }
   /* Only include pregap data if it was in the track file */
   if (meta.pgtype[0] != 'V')
      pregap               = meta.pregap;
   stream->chd             = chd;
   stream->frames_per_hunk = hd->hunkbytes / hd->unitbytes;
   stream->track_frame     = meta.frame_offset;
   stream->track_start     = (size_t)pregap * stream->frame_size;
   stream->track_end       = stream->track_start +
                             (size_t)meta.frames * stream->frame_size;
   return stream;
error:
   chdstream_close(stream);
   if (chd)
      chd_close(chd);
   return NULL;
}
#endif  /* HAVE_RCHD */

void chdstream_close(chdstream_t *stream)
{
   if (!stream)
      return;

   if (stream->hunkmem)
      free(stream->hunkmem);
#ifdef HAVE_RCHD
   if (stream->chd)
      rchd_free(stream->chd);
   if (stream->file)
      filestream_close(stream->file);
#else
   if (stream->chd)
      chd_close(stream->chd);
#endif
   free(stream);
}

static bool
chdstream_load_hunk(chdstream_t *stream, uint32_t hunknum)
{
   uint16_t *array;

   if ((int)hunknum == stream->hunknum)
      return true;

#ifdef HAVE_RCHD
   {
      rchd_request_t rq;

      if (rchd_read_hunk_begin(stream->chd, hunknum,
               stream->hunkmem) != RCHD_OK)
         return false;
      if (!chdstream_pump(stream->file, stream->chd, 0, &rq))
         return false;
   }
#else
   if (chd_read(stream->chd, hunknum, stream->hunkmem) != CHDERR_NONE)
      return false;
#endif

   if (stream->swab)
   {
      uint32_t i;
#ifdef HAVE_RCHD
      uint32_t count = stream->hunk_bytes / 2;
#else
      uint32_t count = chd_get_header(stream->chd)->hunkbytes / 2;
#endif
      array          = (uint16_t*)stream->hunkmem;
      for (i = 0; i < count; ++i)
         array[i] = SWAP16(array[i]);
   }

   stream->hunknum = hunknum;
   return true;
}

ssize_t chdstream_read(chdstream_t *stream, void *data, size_t bytes)
{
   size_t end;
   size_t data_offset   = 0;
#ifdef HAVE_RCHD
   uint32_t unit_bytes  = stream->unit_bytes;
#else
   const chd_header *hd = chd_get_header(stream->chd);
   uint32_t unit_bytes  = hd->unitbytes;
#endif
   uint8_t         *out = (uint8_t*)data;

   if (stream->track_end - stream->offset < bytes)
      bytes             = stream->track_end - stream->offset;

   end                  = stream->offset + bytes;

   while (stream->offset < end)
   {
      uint32_t frame_offset = stream->offset % stream->frame_size;
      uint32_t amount       = stream->frame_size - frame_offset;

      if (amount > end - stream->offset)
         amount = (uint32_t)(end - stream->offset);

      /* In pregap */
      if (stream->offset < stream->track_start)
         memset(out + data_offset, 0, amount);
      else
      {
         uint32_t chd_frame   = (uint32_t)(stream->track_frame +
            (stream->offset - stream->track_start) / stream->frame_size);
         uint32_t hunk        = chd_frame / stream->frames_per_hunk;
         uint32_t hunk_offset = (chd_frame % stream->frames_per_hunk)
            * unit_bytes;

         if (!chdstream_load_hunk(stream, hunk))
            return -1;

         memcpy(out + data_offset,
                stream->hunkmem + frame_offset
                + hunk_offset + stream->frame_offset, amount);
      }

      data_offset    += amount;
      stream->offset += amount;
   }

   return bytes;
}

int chdstream_getc(chdstream_t *stream)
{
   char c = 0;

   if (chdstream_read(stream, &c, sizeof(c) != sizeof(c)))
      return EOF;

   return c;
}

char *chdstream_gets(chdstream_t *stream, char *s, size_t len)
{
   int c;
   size_t _len = 0;
   while (_len < len && (c = chdstream_getc(stream)) != EOF)
      s[_len++] = c;
   if (_len < len)
      s[_len]   = '\0';
   return s;
}

uint64_t chdstream_tell(chdstream_t *stream)
{
   return stream->offset;
}

void chdstream_rewind(chdstream_t *stream)
{
   stream->offset = 0;
}

int64_t chdstream_seek(chdstream_t *stream, int64_t offset, int whence)
{
   int64_t new_offset;

   switch (whence)
   {
      case SEEK_SET:
         new_offset = offset;
         break;
      case SEEK_CUR:
         new_offset = stream->offset + offset;
         break;
      case SEEK_END:
         new_offset = stream->track_end + offset;
         break;
      default:
         return -1;
   }

   if (new_offset < 0)
      return -1;

   if ((size_t)new_offset > stream->track_end)
      new_offset = stream->track_end;

   stream->offset = new_offset;
   return 0;
}

ssize_t chdstream_get_size(chdstream_t *stream)
{
   return stream->track_end - stream->track_start;
}

uint32_t chdstream_get_track_start(chdstream_t *stream)
{
#ifdef HAVE_RCHD
   /* rchd places each track, so the one this stream is on is found by
    * where it starts rather than by summing frames and padding. */
   uint32_t n = rchd_track_count(stream->chd), i;

   for (i = 0; i < n; i++)
   {
      const rchd_track_t *t = rchd_track(stream->chd, i);

      /* What the disc has, not what the file stores: this is reported
       * to a caller asking where the track's audio starts on the disc,
       * and the libchdr path returned the pregap unconditionally here
       * even when it did not skip it. */
      if (t && (uint32_t)(t->logical_offset / stream->unit_bytes)
            == stream->track_frame)
         return t->pregap * stream->frame_size;
   }
   return 0;
#else
   uint32_t i;
   metadata_t meta;
   uint32_t frame_offset = 0;

   for (i = 0; chdstream_get_meta(stream->chd, i, &meta); ++i)
   {
      if (stream->track_frame == frame_offset)
         return meta.pregap * stream->frame_size;

      frame_offset += meta.frames + meta.extra;
   }

   return 0;
#endif
}

uint32_t chdstream_get_frame_size(chdstream_t *stream)
{
   return stream->frame_size;
}

uint32_t chdstream_get_first_track_sector(chdstream_t* stream)
{
#ifdef HAVE_RCHD
   uint32_t n = rchd_track_count(stream->chd), i;
   uint32_t sector_offset = 0;

   for (i = 0; i < n; i++)
   {
      const rchd_track_t *t = rchd_track(stream->chd, i);

      if (!t)
         break;
      if ((uint32_t)(t->logical_offset / stream->unit_bytes)
            == stream->track_frame)
         return sector_offset;
      sector_offset += t->frames;
   }
   return 0;
#else
   uint32_t i;
   metadata_t meta;
   uint32_t frame_offset = 0;
   uint32_t sector_offset = 0;

   for (i = 0; chdstream_get_meta(stream->chd, i, &meta); ++i)
   {
      if (stream->track_frame == frame_offset)
         return sector_offset;

      sector_offset += meta.frames;
      frame_offset += meta.frames + meta.extra;
   }

   return 0;
#endif
}
