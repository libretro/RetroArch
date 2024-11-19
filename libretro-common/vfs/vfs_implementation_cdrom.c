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

#include <vfs/vfs_implementation.h>
#include <file/file_path.h>
#include <compat/fopen_utf8.h>
#include <string/stdstring.h>
#include <cdrom/cdrom.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

/* TODO/FIXME - static global variable */
static cdrom_toc_t vfs_cdrom_toc = {0};

const cdrom_toc_t* retro_vfs_file_get_cdrom_toc(void)
{
   return &vfs_cdrom_toc;
}

int64_t retro_vfs_file_seek_cdrom(
      libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   const char *ext = path_get_extension(stream->orig_path);

   if (string_is_equal_noncase(ext, "cue"))
   {
      switch (whence)
      {
         case SEEK_SET:
            stream->cdrom.byte_pos = offset;
            break;
         case SEEK_CUR:
            stream->cdrom.byte_pos += offset;
            break;
         case SEEK_END:
            stream->cdrom.byte_pos  = (stream->cdrom.cue_len - 1) + offset;
            break;
      }

#ifdef CDROM_DEBUG
      printf("[CDROM] Seek: Path %s Offset %" PRIu64 " is now at %" PRIu64 "\n",
            stream->orig_path,
            offset,
            stream->cdrom.byte_pos);
      fflush(stdout);
#endif
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
      int lba               = (offset / 2352);
      unsigned char min     = 0;
      unsigned char sec     = 0;
      unsigned char frame   = 0;
#ifdef CDROM_DEBUG
      const char *seek_type = "SEEK_SET";
#endif

      switch (whence)
      {
         case SEEK_CUR:
            {
               unsigned new_lba;
#ifdef CDROM_DEBUG
               seek_type               = "SEEK_CUR";
#endif
               stream->cdrom.byte_pos += offset;
               new_lba                 = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba + (stream->cdrom.byte_pos / 2352);

               cdrom_lba_to_msf(new_lba, &min, &sec, &frame);
            }
            break;
         case SEEK_END:
            {
               ssize_t pregap_lba_len = (vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].audio 
                     ? 0 
                     : (vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba - vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba_start));
               ssize_t lba_len        = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].track_size - pregap_lba_len;
#ifdef CDROM_DEBUG
               seek_type              = "SEEK_END";
#endif

               cdrom_lba_to_msf(lba_len + lba, &min, &sec, &frame);
               stream->cdrom.byte_pos = lba_len * 2352;
            }
            break;
         case SEEK_SET:
         default:
            {
#ifdef CDROM_DEBUG
               seek_type = "SEEK_SET";
#endif
               stream->cdrom.byte_pos = offset;
               cdrom_lba_to_msf(vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba + (stream->cdrom.byte_pos / 2352), &min, &sec, &frame);
            }
            break;
      }

      stream->cdrom.cur_min   = min;
      stream->cdrom.cur_sec   = sec;
      stream->cdrom.cur_frame = frame;
      stream->cdrom.cur_lba   = cdrom_msf_to_lba(min, sec, frame);

#ifdef CDROM_DEBUG
      printf(
            "[CDROM] Seek %s: Path %s Offset %" PRIu64 " is now at %" PRIu64 " (MSF %02u:%02u:%02u) (LBA %u)...\n",
            seek_type,
            stream->orig_path,
            offset,
            stream->cdrom.byte_pos,
            (unsigned)stream->cdrom.cur_min,
            (unsigned)stream->cdrom.cur_sec,
            (unsigned)stream->cdrom.cur_frame,
            stream->cdrom.cur_lba);
      fflush(stdout);
#endif
   }
   else
      return -1;

   return 0;
}

void retro_vfs_file_open_cdrom(
      libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
#if defined(__linux__) && !defined(ANDROID)
   char cdrom_path[]       = "/dev/sg1";
   size_t path_len         = strlen(path);
   const char *ext         = path_get_extension(path);

   stream->cdrom.cur_track = 1;

   if (     !string_is_equal_noncase(ext, "cue") 
         && !string_is_equal_noncase(ext, "bin"))
      return;

   if (path_len >= STRLEN_CONST("drive1-track01.bin"))
   {
      if (!memcmp(path, "drive", STRLEN_CONST("drive")))
      {
         if (!memcmp(path + 6, "-track", STRLEN_CONST("-track")))
         {
            if (sscanf(path + 12, "%02u", (unsigned*)&stream->cdrom.cur_track))
            {
#ifdef CDROM_DEBUG
               printf("[CDROM] Opening track %d\n", stream->cdrom.cur_track);
               fflush(stdout);
#endif
            }
         }
      }
   }

   if (path_len >= STRLEN_CONST("drive1.cue"))
   {
      if (!memcmp(path, "drive", STRLEN_CONST("drive")))
      {
         if (path[5] >= '0' && path[5] <= '9')
         {
            cdrom_path[7]       = path[5];
            stream->cdrom.drive = path[5];
            vfs_cdrom_toc.drive = stream->cdrom.drive;
         }
      }
   }

#ifdef CDROM_DEBUG
   printf("[CDROM] Open: Path %s URI %s\n", cdrom_path, path);
   fflush(stdout);
#endif
   stream->fp = (FILE*)fopen_utf8(cdrom_path, "r+b");

   if (!stream->fp)
      return;

   if (string_is_equal_noncase(ext, "cue"))
   {
      if (stream->cdrom.cue_buf)
      {
         free(stream->cdrom.cue_buf);
         stream->cdrom.cue_buf = NULL;
      }

      cdrom_write_cue(stream,
            &stream->cdrom.cue_buf,
            &stream->cdrom.cue_len,
            stream->cdrom.drive,
            &vfs_cdrom_toc.num_tracks,
            &vfs_cdrom_toc);
      cdrom_get_timeouts(stream, &vfs_cdrom_toc.timeouts);

#ifdef CDROM_DEBUG
      if (string_is_empty(stream->cdrom.cue_buf))
      {
         printf("[CDROM] Error writing cue sheet.\n");
         fflush(stdout);
      }
      else
      {
         printf("[CDROM] CUE Sheet:\n%s\n", stream->cdrom.cue_buf);
         fflush(stdout);
      }
#endif
   }
#endif
#if defined(_WIN32) && !defined(_XBOX)
   char cdrom_path[] = "\\\\.\\D:";
   size_t path_len   = strlen(path);
   const char *ext   = path_get_extension(path);

   if (     !string_is_equal_noncase(ext, "cue") 
         && !string_is_equal_noncase(ext, "bin"))
      return;

   if (path_len >= STRLEN_CONST("d:/drive-track01.bin"))
   {
      if (!memcmp(path + 1, ":/drive-track", STRLEN_CONST(":/drive-track")))
      {
         if (sscanf(path + 14, "%02u", (unsigned*)&stream->cdrom.cur_track))
         {
#ifdef CDROM_DEBUG
            printf("[CDROM] Opening track %d\n", stream->cdrom.cur_track);
            fflush(stdout);
#endif
         }
      }
   }

   if (path_len >= STRLEN_CONST("d:/drive.cue"))
   {
      if (!memcmp(path + 1, ":/drive", STRLEN_CONST(":/drive")))
      {
         if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
         {
            cdrom_path[4]       = path[0];
            stream->cdrom.drive = path[0];
            vfs_cdrom_toc.drive = stream->cdrom.drive;
         }
      }
   }

#ifdef CDROM_DEBUG
   printf("[CDROM] Open: Path %s URI %s\n", cdrom_path, path);
   fflush(stdout);
#endif
   stream->fh = CreateFile(cdrom_path,
         GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE,
         NULL,
         OPEN_EXISTING,
         FILE_ATTRIBUTE_NORMAL,
         NULL);

   if (stream->fh == INVALID_HANDLE_VALUE)
      return;

   if (string_is_equal_noncase(ext, "cue"))
   {
      if (stream->cdrom.cue_buf)
      {
         free(stream->cdrom.cue_buf);
         stream->cdrom.cue_buf = NULL;
      }

      cdrom_write_cue(stream,
            &stream->cdrom.cue_buf,
            &stream->cdrom.cue_len,
            stream->cdrom.drive,
            &vfs_cdrom_toc.num_tracks,
            &vfs_cdrom_toc);
      cdrom_get_timeouts(stream,
            &vfs_cdrom_toc.timeouts);

#ifdef CDROM_DEBUG
      if (string_is_empty(stream->cdrom.cue_buf))
      {
         printf("[CDROM] Error writing cue sheet.\n");
         fflush(stdout);
      }
      else
      {
         printf("[CDROM] CUE Sheet:\n%s\n", stream->cdrom.cue_buf);
         fflush(stdout);
      }
#endif
   }
#endif
   if (vfs_cdrom_toc.num_tracks > 1 && stream->cdrom.cur_track)
   {
      stream->cdrom.cur_min   = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].min;
      stream->cdrom.cur_sec   = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].sec;
      stream->cdrom.cur_frame = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].frame;
      stream->cdrom.cur_lba   = cdrom_msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame);
   }
   else
   {
      stream->cdrom.cur_min   = vfs_cdrom_toc.track[0].min;
      stream->cdrom.cur_sec   = vfs_cdrom_toc.track[0].sec;
      stream->cdrom.cur_frame = vfs_cdrom_toc.track[0].frame;
      stream->cdrom.cur_lba   = cdrom_msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame);
   }
}

int retro_vfs_file_close_cdrom(libretro_vfs_implementation_file *stream)
{
#ifdef CDROM_DEBUG
   printf("[CDROM] Close: Path %s\n", stream->orig_path);
   fflush(stdout);
#endif

#if defined(_WIN32) && !defined(_XBOX)
   if (!stream->fh || !CloseHandle(stream->fh))
      return -1;
#else
   if (!stream->fp || fclose(stream->fp))
      return -1;
#endif

   return 0;
}

int64_t retro_vfs_file_tell_cdrom(libretro_vfs_implementation_file *stream)
{
   const char *ext = NULL;
   if (!stream)
      return -1;

   ext = path_get_extension(stream->orig_path);

   if (string_is_equal_noncase(ext, "cue"))
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] (cue) Tell: Path %s Position %" PRIu64 "\n", stream->orig_path, stream->cdrom.byte_pos);
      fflush(stdout);
#endif
      return stream->cdrom.byte_pos;
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] (bin) Tell: Path %s Position %" PRId64 "\n", stream->orig_path, stream->cdrom.byte_pos);
      fflush(stdout);
#endif
      return stream->cdrom.byte_pos;
   }

   return -1;
}

int64_t retro_vfs_file_read_cdrom(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   int rv;
   const char *ext = path_get_extension(stream->orig_path);

   if (string_is_equal_noncase(ext, "cue"))
   {
      if ((int64_t)len >= (int64_t)stream->cdrom.cue_len 
            - stream->cdrom.byte_pos)
         len = stream->cdrom.cue_len - stream->cdrom.byte_pos - 1;
#ifdef CDROM_DEBUG
      printf(
            "[CDROM] Read: Reading %" PRIu64 " bytes from cuesheet starting at %" PRIu64 "...\n",
            len,
            stream->cdrom.byte_pos);
      fflush(stdout);
#endif
      memcpy(s, stream->cdrom.cue_buf + stream->cdrom.byte_pos, len);
      stream->cdrom.byte_pos += len;

      return len;
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
      unsigned char min    = 0;
      unsigned char sec    = 0;
      unsigned char frame  = 0;
      unsigned char rmin   = 0;
      unsigned char rsec   = 0;
      unsigned char rframe = 0;
      size_t skip          = stream->cdrom.byte_pos % 2352;

      if (stream->cdrom.byte_pos >= 
            vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].track_bytes)
         return 0;

      if (stream->cdrom.byte_pos + len > 
            vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].track_bytes)
         len -= (stream->cdrom.byte_pos + len) 
            - vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].track_bytes;

      cdrom_lba_to_msf(stream->cdrom.cur_lba, &min, &sec, &frame);
      cdrom_lba_to_msf(stream->cdrom.cur_lba 
            - vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba,
            &rmin, &rsec, &rframe);

#ifdef CDROM_DEBUG
      printf(
            "[CDROM] Read: Reading %" PRIu64 " bytes from %s starting at byte offset %" PRIu64 " (rMSF %02u:%02u:%02u aMSF %02u:%02u:%02u) (LBA %u) skip %" PRIu64 "...\n",
            len,
            stream->orig_path,
            stream->cdrom.byte_pos,
            (unsigned)rmin,
            (unsigned)rsec,
            (unsigned)rframe,
            (unsigned)min,
            (unsigned)sec,
            (unsigned)frame,
            stream->cdrom.cur_lba,
            skip);
      fflush(stdout);
#endif

#if 1
      rv = cdrom_read(stream, &vfs_cdrom_toc.timeouts, min, sec,
            frame, s, (size_t)len, skip);
#else
      rv = cdrom_read_lba(stream, stream->cdrom.cur_lba, s,
            (size_t)len, skip);
#endif

      if (rv)
      {
#ifdef CDROM_DEBUG
         printf("[CDROM] Failed to read %" PRIu64 " bytes from CD.\n", len);
         fflush(stdout);
#endif
         return 0;
      }

      stream->cdrom.byte_pos += len;
      stream->cdrom.cur_lba   = 
         vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].lba 
         + (stream->cdrom.byte_pos / 2352);

      cdrom_lba_to_msf(stream->cdrom.cur_lba,
            &stream->cdrom.cur_min,
            &stream->cdrom.cur_sec,
            &stream->cdrom.cur_frame);

#ifdef CDROM_DEBUG
      printf(
            "[CDROM] read %" PRIu64 " bytes, position is now: %" PRIu64 " (MSF %02u:%02u:%02u) (LBA %u)\n",
            len,
            stream->cdrom.byte_pos,
            (unsigned)stream->cdrom.cur_min,
            (unsigned)stream->cdrom.cur_sec,
            (unsigned)stream->cdrom.cur_frame,
            cdrom_msf_to_lba(
               stream->cdrom.cur_min,
               stream->cdrom.cur_sec,
               stream->cdrom.cur_frame)
            );
      fflush(stdout);
#endif

      return len;
   }

   return 0;
}

int retro_vfs_file_error_cdrom(libretro_vfs_implementation_file *stream)
{
   return 0;
}

const vfs_cdrom_t* retro_vfs_file_get_cdrom_position(
      const libretro_vfs_implementation_file *stream)
{
   return &stream->cdrom;
}
