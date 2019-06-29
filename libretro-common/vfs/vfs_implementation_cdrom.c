/* Copyright  (C) 2010-2019 The RetroArch team
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

#ifdef _WIN32
#include <windows.h>
#endif

static cdrom_toc_t vfs_cdrom_toc = {0};

const cdrom_toc_t* retro_vfs_file_get_cdrom_toc(void)
{
   return &vfs_cdrom_toc;
}

int64_t retro_vfs_file_seek_cdrom(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
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
            stream->cdrom.byte_pos = (stream->cdrom.cue_len - 1) + offset;
            break;
      }

#ifdef CDROM_DEBUG
      printf("CDROM Seek: Path %s Offset %" PRIu64 " is now at %" PRIu64 "\n", stream->orig_path, offset, stream->cdrom.byte_pos);
      fflush(stdout);
#endif
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
      unsigned frames = (offset / 2352);
      unsigned char min = 0;
      unsigned char sec = 0;
      unsigned char frame = 0;
      const char *seek_type = "SEEK_SET";

      lba_to_msf(frames, &min, &sec, &frame);

      switch (whence)
      {
         case SEEK_CUR:
         {
            min += stream->cdrom.cur_min;
            sec += stream->cdrom.cur_sec;
            frame += stream->cdrom.cur_frame;

            stream->cdrom.byte_pos += offset;
            seek_type = "SEEK_CUR";

            break;
         }
         case SEEK_END:
         {
            unsigned char end_min = 0;
            unsigned char end_sec = 0;
            unsigned char end_frame = 0;
            size_t end_lba = (vfs_cdrom_toc.track[vfs_cdrom_toc.num_tracks - 1].lba_start + vfs_cdrom_toc.track[vfs_cdrom_toc.num_tracks - 1].track_size) - 1;

            lba_to_msf(end_lba, &min, &sec, &frame);

            min += end_min;
            sec += end_sec;
            frame += end_frame;

            stream->cdrom.byte_pos = end_lba * 2352;
            seek_type = "SEEK_END";

            break;
         }
         case SEEK_SET:
         default:
         {
            seek_type = "SEEK_SET";
            stream->cdrom.byte_pos = offset;

            break;
         }
      }

      stream->cdrom.cur_min = min;
      stream->cdrom.cur_sec = sec;
      stream->cdrom.cur_frame = frame;

#ifdef CDROM_DEBUG
      printf("CDROM Seek %s: Path %s Offset %" PRIu64 " is now at %" PRIu64 " (MSF %02d:%02d:%02d) (LBA %u)...\n", seek_type, stream->orig_path, offset, stream->cdrom.byte_pos, stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame, msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame));
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
#ifdef __linux__
   char model[32] = {0};
   char cdrom_path[] = "/dev/sg1";
   size_t path_len = strlen(path);
   const char *ext = path_get_extension(path);

   if (!string_is_equal_noncase(ext, "cue") && !string_is_equal_noncase(ext, "bin"))
      return;

   if (path_len >= strlen("drive1-track01.bin"))
   {
      if (!memcmp(path, "drive", strlen("drive")))
      {
         if (!memcmp(path + 6, "-track", strlen("-track")))
         {
            if (sscanf(path + 12, "%02hhd", &stream->cdrom.cur_track))
            {
#ifdef CDROM_DEBUG
               printf("CDROM: Opening track %d\n", stream->cdrom.cur_track);
               fflush(stdout);
#endif
            }
         }
      }
   }

   if (path_len >= strlen("drive1.cue"))
   {
      if (!memcmp(path, "drive", strlen("drive")))
      {
         if (path[5] >= '0' && path[5] <= '9')
         {
            cdrom_path[7] = path[5];
            stream->cdrom.drive = path[5];
            vfs_cdrom_toc.drive = stream->cdrom.drive;
         }
      }
   }

#ifdef CDROM_DEBUG
   printf("CDROM Open: Path %s URI %s\n", cdrom_path, path);
   fflush(stdout);
#endif
   stream->fp = (FILE*)fopen_utf8(cdrom_path, "r+b");

   if (stream->fp)
   {
      if (!cdrom_get_inquiry(stream, model, sizeof(model)))
      {
#ifdef CDROM_DEBUG
         printf("CDROM Model: %s\n", model);
         fflush(stdout);
#endif
      }
   }
   else
      return;

   if (string_is_equal_noncase(ext, "cue"))
   {
      if (stream->cdrom.cue_buf)
      {
         free(stream->cdrom.cue_buf);
         stream->cdrom.cue_buf = NULL;
      }

      cdrom_write_cue(stream, &stream->cdrom.cue_buf, &stream->cdrom.cue_len, stream->cdrom.drive, &vfs_cdrom_toc.num_tracks, &vfs_cdrom_toc);

#ifdef CDROM_DEBUG
      if (string_is_empty(stream->cdrom.cue_buf))
      {
         printf("Error writing cue sheet.\n");
         fflush(stdout);
      }
      else
      {
         printf("CDROM CUE Sheet:\n%s\n", stream->cdrom.cue_buf);
         fflush(stdout);
      }
#endif
   }
#endif
#ifdef _WIN32
   char model[32] = {0};
   char cdrom_path[] = "\\\\.\\D:";
   size_t path_len = strlen(path);
   const char *ext = path_get_extension(path);

   if (!string_is_equal_noncase(ext, "cue") && !string_is_equal_noncase(ext, "bin"))
      return;

   if (path_len >= strlen("d:/drive-track01.bin"))
   {
      if (!memcmp(path + 1, ":/drive-track", strlen(":/drive-track")))
      {
         if (sscanf(path + 14, "%02hhd", &stream->cdrom.cur_track))
         {
#ifdef CDROM_DEBUG
            printf("CDROM: Opening track %d\n", stream->cdrom.cur_track);
            fflush(stdout);
#endif
         }
      }
   }

   if (path_len >= strlen("d:/drive.cue"))
   {
      if (!memcmp(path + 1, ":/drive", strlen(":/drive")))
      {
         if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
         {
            cdrom_path[4] = path[0];
            stream->cdrom.drive = path[0];
            vfs_cdrom_toc.drive = stream->cdrom.drive;
         }
      }
   }

#ifdef CDROM_DEBUG
   printf("CDROM Open: Path %s URI %s\n", cdrom_path, path);
   fflush(stdout);
#endif
   stream->fh = CreateFile(cdrom_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   if (stream->fh == INVALID_HANDLE_VALUE)
      return;
   else
   {
      if (!cdrom_get_inquiry(stream, model, sizeof(model)))
      {
         size_t len = 0;

#ifdef CDROM_DEBUG
         printf("CDROM Model: %s\n", model);
         fflush(stdout);
#endif
      }
   }

   if (string_is_equal_noncase(ext, "cue"))
   {
      if (stream->cdrom.cue_buf)
      {
         free(stream->cdrom.cue_buf);
         stream->cdrom.cue_buf = NULL;
      }

      cdrom_write_cue(stream, &stream->cdrom.cue_buf, &stream->cdrom.cue_len, stream->cdrom.drive, &vfs_cdrom_toc.num_tracks, &vfs_cdrom_toc);

#ifdef CDROM_DEBUG
      if (string_is_empty(stream->cdrom.cue_buf))
      {
         printf("Error writing cue sheet.\n");
         fflush(stdout);
      }
      else
      {
         printf("CDROM CUE Sheet:\n%s\n", stream->cdrom.cue_buf);
         fflush(stdout);
      }
#endif
   }
#endif
   if (vfs_cdrom_toc.num_tracks > 1 && stream->cdrom.cur_track)
   {
      stream->cdrom.cur_min = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].min;
      stream->cdrom.cur_sec = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].sec;
      stream->cdrom.cur_frame = vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].frame;
   }
   else
   {
      stream->cdrom.cur_min = vfs_cdrom_toc.track[0].min;
      stream->cdrom.cur_sec = vfs_cdrom_toc.track[0].sec;
      stream->cdrom.cur_frame = vfs_cdrom_toc.track[0].frame;
   }
}

int retro_vfs_file_close_cdrom(libretro_vfs_implementation_file *stream)
{
#ifdef CDROM_DEBUG
   printf("CDROM Close: Path %s\n", stream->orig_path);
   fflush(stdout);
#endif

#ifdef _WIN32
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
   if (!stream)
      return -1;

   const char *ext = path_get_extension(stream->orig_path);

   if (string_is_equal_noncase(ext, "cue"))
   {
#ifdef CDROM_DEBUG
      printf("CDROM (cue) Tell: Path %s Position %" PRIu64 "\n", stream->orig_path, stream->cdrom.byte_pos);
      fflush(stdout);
#endif
      return stream->cdrom.byte_pos;
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
      unsigned lba = msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame);

#ifdef CDROM_DEBUG
      printf("CDROM (bin) Tell: Path %s Position %u\n", stream->orig_path, lba * 2352);
      fflush(stdout);
#endif
      return lba * 2352;
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
      if (len < stream->cdrom.cue_len - stream->cdrom.byte_pos)
      {
#ifdef CDROM_DEBUG
         printf("CDROM Read: Reading %" PRIu64 " bytes from cuesheet starting at %" PRIu64 "...\n", len, stream->cdrom.byte_pos);
         fflush(stdout);
#endif
         memcpy(s, stream->cdrom.cue_buf + stream->cdrom.byte_pos, len);
         stream->cdrom.byte_pos += len;

         return len;
      }
      else
      {
#ifdef CDROM_DEBUG
         printf("CDROM Read: Reading %" PRIu64 " bytes from cuesheet starting at %" PRIu64 " failed.\n", len, stream->cdrom.byte_pos);
         fflush(stdout);
#endif
         return 0;
      }
   }
   else if (string_is_equal_noncase(ext, "bin"))
   {
      unsigned frames = len / 2352;
      unsigned i;
      size_t skip = stream->cdrom.byte_pos % 2352;
      unsigned char min = 0;
      unsigned char sec = 0;
      unsigned char frame = 0;
      unsigned lba_cur = 0;
      unsigned lba_start = 0;

      lba_cur = msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame);

      if (vfs_cdrom_toc.num_tracks > 1 && stream->cdrom.cur_track)
         lba_start = msf_to_lba(vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].min, vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].sec, vfs_cdrom_toc.track[stream->cdrom.cur_track - 1].frame);
      else
         lba_start = msf_to_lba(vfs_cdrom_toc.track[0].min, vfs_cdrom_toc.track[0].sec, vfs_cdrom_toc.track[0].frame);

      lba_to_msf(lba_start + lba_cur, &min, &sec, &frame);

#ifdef CDROM_DEBUG
      printf("CDROM Read: Reading %" PRIu64 " bytes from %s starting at byte offset %" PRIu64 " (MSF %02d:%02d:%02d) (LBA %u) skip %" PRIu64 "...\n", len, stream->orig_path, stream->cdrom.byte_pos, min, sec, frame, msf_to_lba(min, sec, frame), skip);
      fflush(stdout);
#endif

      rv = cdrom_read(stream, min, sec, frame, s, (size_t)len, skip);

      if (rv)
      {
#ifdef CDROM_DEBUG
         printf("Failed to read %" PRIu64 " bytes from CD.\n", len);
         fflush(stdout);
#endif
         return 0;
      }

      stream->cdrom.byte_pos += len;

      for (i = 0; i < frames; i++)
      {
         increment_msf(&stream->cdrom.cur_min, &stream->cdrom.cur_sec, &stream->cdrom.cur_frame);
      }

#ifdef CDROM_DEBUG
      printf("CDROM read %" PRIu64 " bytes, position is now: %" PRIu64 " (MSF %02d:%02d:%02d) (LBA %u)\n", len, stream->cdrom.byte_pos, stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame, msf_to_lba(stream->cdrom.cur_min, stream->cdrom.cur_sec, stream->cdrom.cur_frame));
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
