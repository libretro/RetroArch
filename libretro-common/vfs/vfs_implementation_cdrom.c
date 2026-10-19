/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_cdrom.c).
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

#include <stdlib.h>
#include <string.h>

#include <vfs/vfs_implementation.h>
#include <vfs/vfs_implementation_cdrom.h>
#include <file/file_path.h>
#include <compat/fopen_utf8.h>
#include <cdrom/cdrom.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

#if defined(__APPLE__)
/* Implemented in cdrom.c; intentionally not in the public cdrom.h. */
int cdrom_macos_open(int index, void **out_plugin, void **out_mmc, int *out_fd);
void cdrom_macos_close(void *plugin, void *mmc, int fd);
#endif

const cdrom_toc_t* retro_vfs_file_get_cdrom_toc(
      const libretro_vfs_implementation_file *stream)
{
   return stream && stream->cdrom ? stream->cdrom->toc : NULL;
}

int64_t retro_vfs_file_seek_cdrom(
      libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   const char *ext = path_get_extension(stream->orig_path);
   const cdrom_toc_t *toc = retro_vfs_file_get_cdrom_toc(stream);
   if (stream->cdrom->device_only)
      return offset == 0 && (whence == SEEK_SET || whence == SEEK_CUR
            || whence == SEEK_END) ? 0 : -1;
   if (stream->cdrom->logical_blocks)
   {
      int64_t base;
      switch (whence)
      {
         case SEEK_SET:
            base = 0;
            break;
         case SEEK_CUR:
            base = stream->cdrom->byte_pos;
            break;
         case SEEK_END:
            base = (int64_t)stream->cdrom->disc_bytes;
            break;
         default:
            return -1;
      }
      if (offset < -base || offset > INT64_MAX - base)
         return -1;
      stream->cdrom->byte_pos = base + offset;
      return 0;
   }

   if (     (ext[0] == 'c' || ext[0] == 'C')
         && (ext[1] == 'u' || ext[1] == 'U')
         && (ext[2] == 'e' || ext[2] == 'E')
         &&  ext[3] == '\0')
   {
      int64_t base;
      switch (whence)
      {
         case SEEK_SET:
            base = 0;
            break;
         case SEEK_CUR:
            base = stream->cdrom->byte_pos;
            break;
         case SEEK_END:
            /* cue_len counts the buffer's terminating NUL. */
            base = stream->cdrom->cue_len
               ? (int64_t)stream->cdrom->cue_len - 1 : 0;
            break;
         default:
            return -1;
      }
      if (offset < -base || offset > INT64_MAX - base)
         return -1;
      stream->cdrom->byte_pos = base + offset;
#ifdef CDROM_DEBUG
      printf("[CDROM] Seek: Path %s Offset %" PRIu64 " is now at %" PRIu64 "\n",
            stream->orig_path,
            offset,
            stream->cdrom->byte_pos);
      fflush(stdout);
#endif
   }
   else if ( (ext[0] == 'b' || ext[0] == 'B')
         &&  (ext[1] == 'i' || ext[1] == 'I')
         &&  (ext[2] == 'n' || ext[2] == 'N')
         &&   ext[3] == '\0')
   {
      int64_t base, pos;
      unsigned origin;
      const cdrom_track_t *track;
      if (!toc || !stream->cdrom->cur_track
            || stream->cdrom->cur_track > toc->num_tracks)
         return -1;
      track = &toc->track[stream->cdrom->cur_track - 1];
      switch (whence)
      {
         case SEEK_SET:
            base = 0;
            break;
         case SEEK_CUR:
            base = stream->cdrom->byte_pos;
            break;
         case SEEK_END:
            base = track->track_bytes;
            break;
         default:
            return -1;
      }
      if (offset < -base || offset > INT64_MAX - base)
         return -1;
      pos = base + offset;
      origin = track->audio ? track->lba_start : track->lba;
      if ((uint64_t)(pos / 2352) > UINT32_MAX - origin)
         return -1;
      stream->cdrom->byte_pos = pos;
      stream->cdrom->cur_lba = origin + (unsigned)(pos / 2352);
      cdrom_lba_to_msf(stream->cdrom->cur_lba, &stream->cdrom->cur_min,
            &stream->cdrom->cur_sec, &stream->cdrom->cur_frame);
   }
   else
      return -1;

   return 0;
}

static bool retro_vfs_cdrom_parse_path(const char *path, vfs_cdrom_t *cdrom)
{
   const char *tail;
#if defined(_WIN32) && !defined(_XBOX)
   if (!path || strlen(path) < 8 || memcmp(path + 1, ":/drive", 7)
         || !((path[0] >= 'A' && path[0] <= 'Z')
            || (path[0] >= 'a' && path[0] <= 'z')))
      return false;
   cdrom->drive = path[0];
   tail = path + 8;
#elif (defined(__linux__) && !defined(ANDROID)) || defined(__APPLE__)
   if (!path || strlen(path) < 6 || memcmp(path, "drive", 5)
         || path[5] < '0' || path[5] > '9')
      return false;
   cdrom->drive = path[5];
   tail = path + 6;
#else
   (void)path;
   (void)cdrom;
   return false;
#endif
   cdrom->cur_track = 1;
   cdrom->device_only = strcmp(tail, ".device") == 0;
   if (cdrom->device_only || strcmp(tail, ".cue") == 0)
      return true;
#if !defined(__APPLE__)
   if (strcmp(tail, ".iso") == 0)
      return true;
#endif
   if (strlen(tail) != 12 || memcmp(tail, "-track", 6)
         || tail[6] < '0' || tail[6] > '9'
         || tail[7] < '0' || tail[7] > '9' || strcmp(tail + 8, ".bin"))
      return false;
   cdrom->cur_track = (unsigned char)((tail[6] - '0') * 10 + tail[7] - '0');
   return cdrom->cur_track != 0;
}

void retro_vfs_file_open_cdrom(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   unsigned origin;
   const cdrom_track_t *track;
   const char *ext;
   (void)hints;
   if (!stream || !stream->cdrom || mode != RETRO_VFS_FILE_ACCESS_READ
         || !retro_vfs_cdrom_parse_path(path, stream->cdrom))
      return;
   ext = path_get_extension(path);
#if defined(__linux__) && !defined(ANDROID)
   {
      char device[] = "/dev/sg1";
      device[7] = stream->cdrom->drive;
      stream->fp = (FILE*)fopen_utf8(device, "r+b");
      if (!stream->fp)
         return;
   }
#elif defined(_WIN32) && !defined(_XBOX)
   {
      char device[] = "\\\\.\\D:";
      device[4] = stream->cdrom->drive;
      stream->fh = CreateFile(device, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
      if (stream->fh == INVALID_HANDLE_VALUE)
      {
         stream->fh = NULL;
         return;
      }
   }
#elif defined(__APPLE__)
   if (cdrom_macos_open(stream->cdrom->drive - '0', &stream->iokit_plugin,
            &stream->iokit_mmc, &stream->fd) != 0)
      return;
#else
   return;
#endif
   if (stream->cdrom->device_only)
      return;
   if (strcmp(ext, "iso") == 0)
   {
      uint64_t blocks = 0;
      uint32_t size = 0;
      if (cdrom_get_capacity(stream, &blocks, &size) || size != 2048)
         goto error;
      stream->cdrom->logical_blocks = true;
      stream->cdrom->disc_bytes = blocks * size;
      return;
   }
   stream->cdrom->toc = (cdrom_toc_t*)calloc(1, sizeof(*stream->cdrom->toc));
   if (!stream->cdrom->toc || cdrom_write_cue(stream,
            &stream->cdrom->cue_buf, &stream->cdrom->cue_len,
            stream->cdrom->drive, &stream->cdrom->toc->num_tracks,
            stream->cdrom->toc))
      goto error;
   if (stream->cdrom->cur_track > stream->cdrom->toc->num_tracks)
      goto error;
   cdrom_get_timeouts(stream, &stream->cdrom->toc->timeouts);
   if (strcmp(ext, "bin") == 0)
   {
      free(stream->cdrom->cue_buf);
      stream->cdrom->cue_buf = NULL;
      stream->cdrom->cue_len = 0;
   }
   track = &stream->cdrom->toc->track[stream->cdrom->cur_track - 1];
   origin = track->audio ? track->lba_start : track->lba;
   stream->cdrom->cur_lba = origin;
   cdrom_lba_to_msf(origin, &stream->cdrom->cur_min,
         &stream->cdrom->cur_sec, &stream->cdrom->cur_frame);
   return;
error:
   retro_vfs_file_close_cdrom(stream);
}

int retro_vfs_file_close_cdrom(libretro_vfs_implementation_file *stream)
{
   int rv = 0;
#if defined(_WIN32) && !defined(_XBOX)
   if (!stream->fh || stream->fh == INVALID_HANDLE_VALUE || !CloseHandle(stream->fh))
      rv = -1;
   stream->fh = NULL;
#elif defined(__APPLE__)
   if (!stream->iokit_mmc)
      rv = -1;
   else
      cdrom_macos_close(stream->iokit_plugin, stream->iokit_mmc, stream->fd);
   stream->iokit_plugin = NULL;
   stream->iokit_mmc = NULL;
   stream->fd = -1;
#else
   if (!stream->fp || fclose(stream->fp))
      rv = -1;
   stream->fp = NULL;
#endif
   if (stream->cdrom)
   {
      free(stream->cdrom->toc);
      stream->cdrom->toc = NULL;
      free(stream->cdrom->cue_buf);
      stream->cdrom->cue_buf = NULL;
      stream->cdrom->cue_len = 0;
      stream->cdrom->last_frame_valid = false;
   }
   return rv;
}

int64_t retro_vfs_file_tell_cdrom(libretro_vfs_implementation_file *stream)
{
   const char *ext = NULL;
   if (!stream)
      return -1;

   if (stream->cdrom->logical_blocks || stream->cdrom->device_only)
      return stream->cdrom->byte_pos;

   ext = path_get_extension(stream->orig_path);

   if (     (ext[0] == 'c' || ext[0] == 'C')
         && (ext[1] == 'u' || ext[1] == 'U')
         && (ext[2] == 'e' || ext[2] == 'E')
         &&  ext[3] == '\0')
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] (cue) Tell: Path %s Position %" PRIu64 "\n", stream->orig_path, stream->cdrom->byte_pos);
      fflush(stdout);
#endif
      return stream->cdrom->byte_pos;
   }
   else if ( (ext[0] == 'b' || ext[0] == 'B')
         &&  (ext[1] == 'i' || ext[1] == 'I')
         &&  (ext[2] == 'n' || ext[2] == 'N')
         &&   ext[3] == '\0')
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] (bin) Tell: Path %s Position %" PRId64 "\n", stream->orig_path, stream->cdrom->byte_pos);
      fflush(stdout);
#endif
      return stream->cdrom->byte_pos;
   }

   return -1;
}

int64_t retro_vfs_file_read_cdrom(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   int rv;
   const char *ext = path_get_extension(stream->orig_path);
   const cdrom_toc_t *toc = retro_vfs_file_get_cdrom_toc(stream);
   if (!len)
      return 0;
   if (stream->cdrom->media_changed)
   {
      stream->cdrom->io_error = true;
      return -1;
   }
   if (stream->cdrom->device_only)
   {
      if (len)
      {
         stream->cdrom->io_error = true;
         return -1;
      }
      return 0;
   }
   if (stream->cdrom->logical_blocks)
   {
      unsigned char *dst = (unsigned char*)s;
      uint64_t done = 0;
      uint64_t pos = (uint64_t)stream->cdrom->byte_pos;
      if (!len || pos >= stream->cdrom->disc_bytes)
         return 0;
      if (!s)
         return -1;
      if (len > stream->cdrom->disc_bytes - pos)
         len = stream->cdrom->disc_bytes - pos;
      if (len > SIZE_MAX)
         len = SIZE_MAX;
      while (done < len)
      {
         uint32_t lba = (uint32_t)(pos / 2048);
         size_t skip = (size_t)(pos % 2048);
         size_t n;
         if (skip || len - done < 2048
               || (stream->cdrom->last_frame_valid
                  && stream->cdrom->last_frame_lba == lba))
         {
            if (!stream->cdrom->last_frame_valid
                  || stream->cdrom->last_frame_lba != lba)
            {
               stream->cdrom->last_frame_valid = false;
               if (cdrom_read_blocks(stream, lba, 1,
                        stream->cdrom->last_frame))
                  break;
               stream->cdrom->last_frame_lba = lba;
               stream->cdrom->last_frame_valid = true;
            }
            n = 2048 - skip;
            if (n > len - done)
               n = (size_t)(len - done);
            memcpy(dst + (size_t)done, stream->cdrom->last_frame + skip, n);
         }
         else
         {
            uint32_t blocks = (uint32_t)((len - done) / 2048 > 32
                  ? 32 : (len - done) / 2048);
            n = (size_t)blocks * 2048;
            if (cdrom_read_blocks(stream, lba, blocks, dst + (size_t)done))
               break;
         }
         done += n;
         pos  += n;
      }
      stream->cdrom->byte_pos += (int64_t)done;
      if (done != len)
      {
         stream->cdrom->io_error = true;
         stream->cdrom->last_frame_valid = false;
         if (!done)
            return -1;
      }
      return (int64_t)done;
   }

   if (     (ext[0] == 'c' || ext[0] == 'C')
         && (ext[1] == 'u' || ext[1] == 'U')
         && (ext[2] == 'e' || ext[2] == 'E')
         &&  ext[3] == '\0')
   {
      /* Guard against byte_pos being outside the cue buffer (can
       * happen after a seek -- seek does not clamp).  Treat as
       * end-of-file. */
      if (     stream->cdrom->byte_pos < 0
            || (size_t)stream->cdrom->byte_pos >= stream->cdrom->cue_len)
         return 0;
      /* Clamp len against the remaining bytes using unsigned
       * subtraction.  Computing (byte_pos + len) and comparing
       * against cue_len wraps uint64_t when len is attacker-
       * controlled.  The -1 preserves legacy behaviour of stopping
       * one byte before cue_len so consumers that rely on the cue
       * buffer being NUL-terminated don't re-read the terminator. */
      {
         size_t remaining = stream->cdrom->cue_len
                          - (size_t)stream->cdrom->byte_pos;
         if (remaining > 0)
            remaining -= 1;
         if (len > (uint64_t)remaining)
            len = (uint64_t)remaining;
      }
#ifdef CDROM_DEBUG
      printf(
            "[CDROM] Read: Reading %" PRIu64 " bytes from cuesheet starting at %" PRIu64 "...\n",
            len,
            stream->cdrom->byte_pos);
      fflush(stdout);
#endif
      memcpy(s, stream->cdrom->cue_buf + stream->cdrom->byte_pos, (size_t)len);
      stream->cdrom->byte_pos += len;

      return (int64_t)len;
   }
   else if ( (ext[0] == 'b' || ext[0] == 'B')
         &&  (ext[1] == 'i' || ext[1] == 'I')
         &&  (ext[2] == 'n' || ext[2] == 'N')
         &&   ext[3] == '\0')
   {
      unsigned char min    = 0;
      unsigned char sec    = 0;
      unsigned char frame  = 0;
      unsigned char rmin   = 0;
      unsigned char rsec   = 0;
      unsigned char rframe = 0;
      size_t skip          = stream->cdrom->byte_pos % 2352;

      /* Guard against cur_track == 0 (would index track[-1]) and
       * against an out-of-range byte_pos.  cur_track can become
       * 0 via a crafted VFS path like "driveN-track00.bin" unless
       * the parser rejects it (see retro_vfs_file_open_cdrom). */
      if (!toc || stream->cdrom->cur_track == 0
            || stream->cdrom->cur_track > toc->num_tracks)
         return -1;
      {
         size_t track_bytes =
            toc->track[stream->cdrom->cur_track - 1].track_bytes;
         if (     stream->cdrom->byte_pos < 0
               || (size_t)stream->cdrom->byte_pos >= track_bytes)
            return 0;
         /* Clamp len against remaining bytes using unsigned
          * subtraction.  Computing (byte_pos + len) first is a
          * uint64_t wrap hazard when len is attacker-controlled. */
         {
            size_t remaining = track_bytes - (size_t)stream->cdrom->byte_pos;
            if (len > (uint64_t)remaining)
               len = (uint64_t)remaining;
         }
      }

      cdrom_lba_to_msf(stream->cdrom->cur_lba, &min, &sec, &frame);
      cdrom_lba_to_msf(stream->cdrom->cur_lba 
            - toc->track[stream->cdrom->cur_track - 1].lba,
            &rmin, &rsec, &rframe);

#ifdef CDROM_DEBUG
      printf(
            "[CDROM] Read: Reading %" PRIu64 " bytes from %s starting at byte offset %" PRIu64 " (rMSF %02u:%02u:%02u aMSF %02u:%02u:%02u) (LBA %u) skip %" PRIu64 "...\n",
            len,
            stream->orig_path,
            stream->cdrom->byte_pos,
            (unsigned)rmin,
            (unsigned)rsec,
            (unsigned)rframe,
            (unsigned)min,
            (unsigned)sec,
            (unsigned)frame,
            stream->cdrom->cur_lba,
            skip);
      fflush(stdout);
#endif

#if 1
      rv = cdrom_read(stream, &stream->cdrom->toc->timeouts, min, sec,
            frame, s, (size_t)len, skip);
#else
      rv = cdrom_read_lba(stream, stream->cdrom->cur_lba, s,
            (size_t)len, skip);
#endif

      if (rv)
      {
#ifdef CDROM_DEBUG
         printf("[CDROM] Failed to read %" PRIu64 " bytes from CD.\n", len);
         fflush(stdout);
#endif
         stream->cdrom->io_error = true;
         return -1;
      }

      stream->cdrom->byte_pos += len;
      stream->cdrom->cur_lba   = 
         (toc->track[stream->cdrom->cur_track - 1].audio
          ? toc->track[stream->cdrom->cur_track - 1].lba_start
          : toc->track[stream->cdrom->cur_track - 1].lba)
         + (unsigned)(stream->cdrom->byte_pos / 2352);

      cdrom_lba_to_msf(stream->cdrom->cur_lba,
            &stream->cdrom->cur_min,
            &stream->cdrom->cur_sec,
            &stream->cdrom->cur_frame);

#ifdef CDROM_DEBUG
      printf(
            "[CDROM] read %" PRIu64 " bytes, position is now: %" PRIu64 " (MSF %02u:%02u:%02u) (LBA %u)\n",
            len,
            stream->cdrom->byte_pos,
            (unsigned)stream->cdrom->cur_min,
            (unsigned)stream->cdrom->cur_sec,
            (unsigned)stream->cdrom->cur_frame,
            cdrom_msf_to_lba(
               stream->cdrom->cur_min,
               stream->cdrom->cur_sec,
               stream->cdrom->cur_frame)
            );
      fflush(stdout);
#endif

      return len;
   }

   return 0;
}

int retro_vfs_file_error_cdrom(libretro_vfs_implementation_file *stream)
{
   return stream && stream->cdrom && stream->cdrom->io_error ? 1 : 0;
}

const vfs_cdrom_t* retro_vfs_file_get_cdrom_position(
      const libretro_vfs_implementation_file *stream)
{
   return stream->cdrom;
}
