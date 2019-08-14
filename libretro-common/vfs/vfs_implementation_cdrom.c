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
#include <vfs/vfs_implementation_cdrom.h>
#include <file/file_path.h>
#include <compat/fopen_utf8.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CDROM
#include <cdrom/cdrom.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

/*#define CDROM_CUE_PARSE_DEBUG*/

#ifdef HAVE_CDROM
static cdrom_toc_t vfs_cdrom_toc = {0};

const cdrom_toc_t* retro_vfs_file_get_cdrom_toc(void)
{
   return &vfs_cdrom_toc;
}
#endif

static void retro_vfs_file_seek_cdrom_track_sector(libretro_vfs_implementation_file* stream, unsigned sector)
{
   retro_vfs_file_seek_cdrom_track(stream, sector * stream->track->sector_size, SEEK_SET);
}

static unsigned char retro_vs_file_get_sector_header_size(libretro_vfs_implementation_file* track_stream)
{
   unsigned char buffer[32];
   unsigned char sector_header_size = 0;

   if (!track_stream || !track_stream->track)
      return 0;

   /* MODE information is normally found in the CUE sheet, but we can try to determine it from the raw data.
    *
    *   MODE1/2048 - CDROM Mode1 Data (cooked) [no header, no footer]
    *   MODE1/2352 - CDROM Mode1 Data (raw)    [16 byte header, 288 byte footer]
    *   MODE2/2336 - CDROM-XA Mode2 Data       [8 byte header, 280 byte footer]
    *   MODE2/2352 - CDROM-XA Mode2 Data       [24 byte header, 280 byte footer]
    *
    * Note that MODE is actually a property on each sector and can change between 1 and 2 depending on how much error
    * correction the author desired. To support that, the data format must be "/2352" to include the full header and
    * data without error correction information, at which point the CUE sheet information becomes just a hint.
    * For simplicitly, we currently only handle "/2352" modes
    */

   /* The boot record or primary volume descriptor is always at sector 16 and will contain a "CD001" marker */
   retro_vfs_file_seek_cdrom_track_sector(track_stream, 16);
   if (retro_vfs_file_read_cdrom_track(track_stream, buffer, sizeof(buffer)) == 0)
      return 0;

   /* ISO-9660 says the first twelve bytes of a sector should be the sync pattern 00 FF FF FF FF FF FF FF FF FF FF 00
    * if it's not, then assume the track is audio data and has no header or MODE2/2336 which we don't support */
   if (buffer[0] == 0 && buffer[1] == 0xFF && buffer[2] == 0xFF && buffer[3] == 0xFF &&
      buffer[4] == 0xFF && buffer[5] == 0xFF && buffer[6] == 0xFF && buffer[7] == 0xFF &&
      buffer[8] == 0xFF && buffer[9] == 0xFF && buffer[10] == 0xFF && buffer[11] == 0)
   {
      /* after the 12 byte sync pattern is three bytes identifying the sector and then one byte for the mode (total 16 bytes) */
      sector_header_size = 16;
      track_stream->track->mode = buffer[15];

      /* if this is a CDROM-XA data source, the "CD001" tag will be offset by an additional 8 bytes */
      if (buffer[25] == 0x43 && buffer[26] == 0x44 &&
         buffer[27] == 0x30 && buffer[28] == 0x30 && buffer[29] == 0x31)
      {
         sector_header_size = 24;
      }
   }

   return sector_header_size;
}

#ifdef HAVE_CDROM

static bool retro_vfs_file_open_cdrom_handle(libretro_vfs_implementation_file* stream, const char* path, char drive)
{
#ifdef CDROM_DEBUG
   printf("[CDROM] Open: Path %s\n", path);
   fflush(stdout);
#endif

#if defined(__linux__) && !defined(ANDROID)
   char cdrom_path[] = "/dev/sg1";
   cdrom_path[7] = drive;

   stream->fp = (FILE*)fopen_utf8(cdrom_path, "r+b");

   if (!stream->fp)
      return false;
#elif defined(_WIN32) && !defined(_XBOX)
   char cdrom_path[] = "\\\\.\\D:";
   cdrom_path[4] = drive;

   stream->fh = CreateFile(cdrom_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   if (stream->fh == INVALID_HANDLE_VALUE)
   {
      stream->fh = NULL;
      return false;
   }
#endif

   stream->orig_path = strdup(path);

   if (cdrom_is_media_inserted(stream))
   {
      cdrom_write_cue(stream, &stream->cdrom.cue_buf, &stream->cdrom.cue_len, stream->cdrom.drive, &vfs_cdrom_toc.num_tracks, &vfs_cdrom_toc);
      cdrom_get_timeouts(stream, &vfs_cdrom_toc.timeouts);
   }

   return true;
}

#endif

bool retro_vfs_file_open_cdrom(
   libretro_vfs_implementation_file* stream,
   const char* path, unsigned mode, unsigned hints)
{
   size_t path_len = strlen(path);
   const char* ext = path_get_extension(path);

   if (string_is_equal_noncase(ext, "cue"))
   {
      stream->cdrom.drive = '\0';

#ifdef HAVE_CDROM
#if defined(__linux__) && !defined(ANDROID)
      if (path_len >= strlen("drive1.cue"))
      {
         if (!memcmp(path, "drive", strlen("drive")))
         {
            if (path[5] >= '0' && path[5] <= '9')
            {
               stream->cdrom.drive = path[5];
               vfs_cdrom_toc.drive = stream->cdrom.drive;

               return retro_vfs_file_open_cdrom_handle(stream, path, stream->cdrom.drive);
            }
         }
      }
#elif defined(_WIN32) && !defined(_XBOX)
      if (path_len >= strlen("d:/drive.cue"))
      {
         if (!memcmp(path + 1, ":/drive", strlen(":/drive")))
         {
            if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
            {
               stream->cdrom.drive = path[0];
               vfs_cdrom_toc.drive = stream->cdrom.drive;

               return retro_vfs_file_open_cdrom_handle(stream, path, stream->cdrom.drive);
            }
         }
      }
#endif
#endif

      stream->fp = (FILE*)fopen_utf8(path, "r+b");
      if (stream->fp)
      {
         stream->scheme = VFS_SCHEME_CUE;
         return true;
      }
   }
#ifdef HAVE_CDROM
   else if (string_is_equal_noncase(ext, "bin"))
   {
      unsigned track = 0;

#if defined(__linux__) && !defined(ANDROID)
      if (path_len >= strlen("drive1-track01.bin"))
      {
         if (!memcmp(path, "drive", strlen("drive")))
         {
            if (!memcmp(path + 6, "-track", strlen("-track")))
               sscanf(path + 12, "%02u", (unsigned*)& track);

            stream->cdrom.drive = path[5];
            if (!retro_vfs_file_open_cdrom_handle(stream, path, stream->cdrom.drive))
               return false;
         }
      }
#elif defined(_WIN32) && !defined(_XBOX)
      if (path_len >= strlen("d:/drive-track01.bin"))
      {
         if (!memcmp(path + 1, ":/drive-track", strlen(":/drive-track")))
         {
            sscanf(path + 14, "%02u", (unsigned*)& track);

            stream->cdrom.drive = path[0];
            vfs_cdrom_toc.drive = stream->cdrom.drive;

            if (!retro_vfs_file_open_cdrom_handle(stream, path, stream->cdrom.drive))
               return false;
         }
      }
#endif

      if (track)
      {
#ifdef CDROM_DEBUG
         printf("[CDROM] Opening track %u\n", track);
         fflush(stdout);
#endif

         if (track > vfs_cdrom_toc.num_tracks)
            track = 1;

         stream->scheme = VFS_SCHEME_CDROM_TRACK;
         stream->parent = stream;
         stream->size = vfs_cdrom_toc.track[track - 1].track_bytes;
         stream->track = (vfs_cdrom_track_t*)calloc(1, sizeof(*stream->track));
         stream->track->sector_size = 2352;
         stream->track->cur_track = track;
         stream->track->cur_min = vfs_cdrom_toc.track[track - 1].min;
         stream->track->cur_sec = vfs_cdrom_toc.track[track - 1].sec;
         stream->track->cur_frame = vfs_cdrom_toc.track[track - 1].frame;
         stream->track->cur_lba = cdrom_msf_to_lba(stream->track->cur_min, stream->track->cur_sec, stream->track->cur_frame);
         stream->track->last_frame_lba = (unsigned)-1;
         return true;
      }
   }
#endif

   return false;
}

int retro_vfs_file_close_cdrom(libretro_vfs_implementation_file *stream)
{
#ifdef HAVE_CDROM
#ifdef CDROM_DEBUG
   printf("[CDROM] Close: Path %s\n", stream->orig_path);
   fflush(stdout);
#endif

#if defined(_WIN32) && !defined(_XBOX)
   if (stream->fh && !CloseHandle(stream->fh))
      return -1;
#else
   if (!stream->fp || fclose(stream->fp))
      return -1;
#endif
#endif

   return 0;
}

int64_t retro_vfs_file_seek_cdrom(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
{
   switch (whence)
   {
      case SEEK_SET:
         stream->cdrom.cue_pos = offset;
         break;

      case SEEK_CUR:
         stream->cdrom.cue_pos += offset;
         break;

      case SEEK_END:
         stream->cdrom.cue_pos = stream->cdrom.cue_len - offset;
         break;
   }

   if (stream->cdrom.cue_pos > stream->cdrom.cue_len)
      stream->cdrom.cue_pos = stream->cdrom.cue_len;
   if (stream->cdrom.cue_pos < 0)
      stream->cdrom.cue_pos = 0;

   return 0;
}

int64_t retro_vfs_file_tell_cdrom(libretro_vfs_implementation_file *stream)
{
   return stream->cdrom.cue_pos;
}

int64_t retro_vfs_file_read_cdrom(libretro_vfs_implementation_file* stream,
   void* s, uint64_t len)
{
   size_t bytes_to_copy;

   if (!stream->cdrom.cue_buf)
      return 0;

   bytes_to_copy = stream->cdrom.cue_len - stream->cdrom.cue_pos;
   if (bytes_to_copy > len)
       bytes_to_copy = len;
   if (bytes_to_copy > 0)
   {
      memcpy(s, &stream->cdrom.cue_buf[stream->cdrom.cue_pos], bytes_to_copy);
      stream->cdrom.cue_pos += bytes_to_copy;
   }

   return bytes_to_copy;
}

int retro_vfs_file_error_cdrom(libretro_vfs_implementation_file *stream)
{
   return 0;
}

static void retro_vfs_skip_spaces(const char **buf, size_t len)
{
   if (!buf || !*buf)
      return;

   while (len-- && (**buf == ' ' || **buf == '\t'))
      ++(*buf);
}

static bool retro_vfs_file_get_cue_track_path(const char* cue_contents, const char* cue_path, unsigned track, char* track_path, size_t max_track_path, int* track_pregap_bytes, int* sector_size)
{
   const char *line = NULL;
   const char* cue = cue_contents;
   char current_track_path[PATH_MAX_LENGTH] = {0};
   char track_mode[11] = {0};
   bool found_file = false;
   unsigned found_track = 0;

   if (!cue_contents)
      return false;

   track_path[0] = '\0';

   while (*cue)
   {
      size_t len = 0;

      while (*cue && (*cue == ' ' || *cue == '\t'))
         ++cue;
      line = cue;
      while (*cue && *cue != '\n')
         ++cue;
      len = cue - line;
      if (*cue)
         ++cue;
      if (len == 0)
         continue;

      if (!found_file && !strncasecmp(line, "FILE", 4))
      {
         const char *file = line + 4;
         retro_vfs_skip_spaces(&file, len - 4);

         if (!string_is_empty(file))
         {
            const char *file_end = NULL;
            size_t file_len = 0;
            bool quoted = false;

            if (file[0] == '"')
            {
               quoted = true;
               file++;
            }

            if (quoted)
               file_end = strchr(file, '\"');
            else
               file_end = strchr(file, ' ');

            if (file_end)
            {
               file_len = file_end - file;
               memcpy(current_track_path, file, file_len);
               found_file = true;
#ifdef CDROM_CUE_PARSE_DEBUG
               printf("Found file: %s\n", current_track_path);
               fflush(stdout);
#endif
            }
         }
      }
      else if (found_file && !found_track && !strncasecmp(line, "TRACK", 5))
      {
         const char *track = line + 5;
         retro_vfs_skip_spaces(&track, len - 5);

         if (!string_is_empty(track))
         {
            unsigned track_number = 0;
            sscanf(track, "%u", &track_number);
#ifdef CDROM_CUE_PARSE_DEBUG
            printf("Found track: %d\n", track_number);
            fflush(stdout);
#endif
            track++;

            if (track[0] && track[0] != ' ' && track[0] != '\t')
               track++;

            if (!string_is_empty(track))
            {
               retro_vfs_skip_spaces(&track, strlen(track));
#ifdef CDROM_CUE_PARSE_DEBUG
               printf("Found track type: %s\n", track);
               fflush(stdout);
#endif
               if (!strncasecmp(track, "MODE", 4))
               {
                  found_track = track_number;
                  strlcpy(track_mode, track, sizeof(track_mode));
               }
               else
                  found_track = 0;
            }
         }
      }
      else if (found_file && found_track == track && !strncasecmp(line, "INDEX", 5))
      {
         const char *index = line + 5;
         retro_vfs_skip_spaces(&index, len - 5);

         if (!string_is_empty(index))
         {
            unsigned index_number = 0;
            sscanf(index, "%u", &index_number);

            if (index_number == 1)
            {
               const char *pregap = index + 1;
               if (pregap[0] && pregap[0] != ' ' && pregap[0] != '\t')
                  pregap++;

               if (strstr(current_track_path, "/") || strstr(current_track_path, "\\"))
               {
                  strncpy(track_path, current_track_path, max_track_path);
#ifdef CDROM_CUE_PARSE_DEBUG
                  printf("using path %s\n", track_path);
                  fflush(stdout);
#endif
               }
               else
               {
                  fill_pathname_basedir(track_path, cue_path, max_track_path);
                  strlcat(track_path, current_track_path, max_track_path);
#ifdef CDROM_CUE_PARSE_DEBUG
                  printf("using absolute path %s\n", track_path);
                  fflush(stdout);
#endif
               }

               if (!string_is_empty(pregap))
               {
                  retro_vfs_skip_spaces(&pregap, strlen(pregap));
                  found_file = false;
                  found_track = false;

                  if (!string_is_empty(track_mode))
                  {
                     unsigned track_sector_size = 0;
                     unsigned track_mode_number = 0;

                     if (strlen(track_mode) == 10)
                     {
                        sscanf(track_mode, "MODE%u/%u", &track_mode_number, &track_sector_size);
#ifdef CDROM_CUE_PARSE_DEBUG
                        printf("Found track mode %d with sector size %d\n", track_mode_number, track_sector_size);
                        fflush(stdout);
#endif
                        if ((track_mode_number == 1 || track_mode_number == 2) && track_sector_size)
                        {
                           unsigned min = 0;
                           unsigned sec = 0;
                           unsigned frame = 0;
                           sscanf(pregap, "%02u:%02u:%02u", &min, &sec, &frame);

                           if (min || sec || frame || strstr(pregap, "00:00:00"))
                           {
                              if (sector_size)
                                 *sector_size = track_sector_size;
                              if (track_pregap_bytes)
                                 *track_pregap_bytes = ((min * 60 + sec) * 75 + frame) * track_sector_size;
#ifdef CDROM_CUE_PARSE_DEBUG
                              printf("Found pregap of %02u:%02u:%02u (bytes: %" PRIu64 ")\n", min, sec, frame, data_track_pregap_bytes);
                              fflush(stdout);
#endif
                              break;
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   return !string_is_empty(track_path);
}

libretro_vfs_implementation_file* retro_vfs_file_open_cdrom_track(libretro_vfs_implementation_file* stream, const char* track)
{
   char track_path[PATH_MAX_LENGTH];
   unsigned track_index = 0;

   if (!stream->cdrom.cue_buf)
   {
      switch (stream->scheme)
      {
         case VFS_SCHEME_CUE:
            stream->cdrom.cue_len = stream->size;
            stream->cdrom.cue_buf = (char*)malloc(stream->size + 1);
            if (stream->cdrom.cue_buf)
            {
               retro_vfs_file_seek_impl(stream, 0, RETRO_VFS_SEEK_POSITION_START);
               retro_vfs_file_read_impl(stream, stream->cdrom.cue_buf, stream->cdrom.cue_len);
               stream->cdrom.cue_buf[stream->cdrom.cue_len] = '\0';
            }
            break;

#ifdef HAVE_CDROM
         case VFS_SCHEME_CDROM:
            cdrom_write_cue(stream, &stream->cdrom.cue_buf, &stream->cdrom.cue_len, stream->cdrom.drive, &vfs_cdrom_toc.num_tracks, &vfs_cdrom_toc);
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
            break;
#endif

         default:
            return NULL;
      }

      if (!stream->cdrom.cue_buf)
         return NULL;
   }

   while (*track && (*track < '0' || *track > '9'))
      ++track;

   sscanf(track, "%u", &track_index);

   if (track_index > 0)
   {
      int pregap_bytes = 0;
      int sector_size = 2352;
      if (retro_vfs_file_get_cue_track_path(stream->cdrom.cue_buf, stream->orig_path, track_index, track_path, PATH_MAX_LENGTH, &pregap_bytes, &sector_size))
      {
         libretro_vfs_implementation_file* track_stream;
         if (stream->scheme == VFS_SCHEME_CDROM)
         {
#ifdef HAVE_CDROM
            track_stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));
            track_stream->size = vfs_cdrom_toc.track[track_index - 1].track_bytes;
#endif
         }
         else
         {
            track_stream = retro_vfs_file_open_impl(track_path, RETRO_VFS_FILE_ACCESS_READ, stream->hints);
         }

         if (track_stream)
         {
            track_stream->scheme = VFS_SCHEME_CDROM_TRACK;
            track_stream->cdrom.drive = stream->cdrom.drive;
            track_stream->parent = stream;
            track_stream->parent_offset = pregap_bytes;

            track_stream->track = (vfs_cdrom_track_t*)calloc(1, sizeof(*track_stream->track));
            track_stream->track->cur_track = track_index;
            track_stream->track->last_frame_lba = (unsigned)-1;
            track_stream->track->sector_size = sector_size;
            track_stream->track->sector_header_size = retro_vs_file_get_sector_header_size(track_stream);

            return track_stream;
         }
      }
   }

   return NULL;
}

int retro_vfs_file_close_cdrom_track(libretro_vfs_implementation_file* stream)
{
   if (stream->track)
      free(stream->track);

   if (stream->parent == stream)
      retro_vfs_file_close_cdrom(stream);

   return 0;
}

int64_t retro_vfs_file_seek_cdrom_track(libretro_vfs_implementation_file* stream, int64_t offset, int whence)
{
   if (!stream->parent)
      return -1;

   switch (whence)
   {
      case SEEK_SET:
         stream->track->byte_pos = offset;
         break;

      case SEEK_CUR:
         stream->track->byte_pos += offset;
         break;

      case SEEK_END:
         stream->track->byte_pos = stream->size - offset;
         break;
   }

   if (stream->track->byte_pos >= stream->size)
      stream->track->byte_pos = stream->size - 1;
   if (stream->track->byte_pos < 0)
      stream->track->byte_pos = 0;

   if (stream->parent->scheme == VFS_SCHEME_CUE)
   {
      int64_t rv;

      /* temporarily change scheme to prevent infinite recusion */
      stream->scheme = VFS_SCHEME_NONE;
      rv = retro_vfs_file_seek_impl(stream, stream->track->byte_pos + stream->parent_offset, SEEK_SET);
      stream->scheme = VFS_SCHEME_CUE_BIN;

      if (rv >= 0)
         stream->track->cur_lba = stream->track->byte_pos / stream->track->sector_size;

      return rv;
   }

#ifdef HAVE_CDROM
   stream->track->cur_lba = vfs_cdrom_toc.track[stream->track->cur_track - 1].lba + (stream->track->byte_pos / stream->track->sector_size);
   cdrom_lba_to_msf(stream->track->cur_lba, &stream->track->cur_min, &stream->track->cur_sec, &stream->track->cur_frame);

#ifdef CDROM_DEBUG
   printf("[CDROM] Seek: Path %s Offset %" PRIu64 " is now at %" PRIu64 " (MSF %02u:%02u:%02u) (LBA %u)...\n",
      stream->orig_path, offset, stream->track->byte_pos, (unsigned)stream->track->cur_min, (unsigned)stream->track->cur_sec, (unsigned)stream->track->cur_frame, stream->track->cur_lba);
   fflush(stdout);
#endif
#endif

   return 0;
}

static void retro_vfs_file_read_cdrom_track_sector(libretro_vfs_implementation_file* stream, void* s, uint64_t len)
{
   if (stream->parent->scheme == VFS_SCHEME_CUE)
   {
      /* temporarily change the scheme to prevent recursion */
      stream->scheme = VFS_SCHEME_NONE;
      retro_vfs_file_read_impl(stream, s, len);
      stream->scheme = VFS_SCHEME_CUE_BIN;
   }
#ifdef HAVE_CDROM
   else
   {
      int rv;
      unsigned char min = 0;
      unsigned char sec = 0;
      unsigned char frame = 0;
      unsigned char rmin = 0;
      unsigned char rsec = 0;
      unsigned char rframe = 0;
      uint64_t byte_pos = stream->track->cur_lba * stream->track->sector_size;

      if (byte_pos >= vfs_cdrom_toc.track[stream->track->cur_track - 1].track_bytes)
         return;

      if (byte_pos + len > vfs_cdrom_toc.track[stream->track->cur_track - 1].track_bytes)
         len -= (byte_pos + len) - vfs_cdrom_toc.track[stream->track->cur_track - 1].track_bytes;

      cdrom_lba_to_msf(stream->track->cur_lba, &min, &sec, &frame);
      cdrom_lba_to_msf(stream->track->cur_lba - vfs_cdrom_toc.track[stream->track->cur_track - 1].lba, &rmin, &rsec, &rframe);

#ifdef CDROM_DEBUG
      printf("[CDROM] Read: Reading %" PRIu64 " bytes from %s starting at byte offset %" PRIu64 " (rMSF %02u:%02u:%02u aMSF %02u:%02u:%02u) (LBA %u)...\n", len, stream->orig_path, byte_pos, (unsigned)rmin, (unsigned)rsec, (unsigned)rframe, (unsigned)min, (unsigned)sec, (unsigned)frame, stream->track->cur_lba);
      fflush(stdout);
#endif

      rv = cdrom_read(stream, &vfs_cdrom_toc.timeouts, min, sec, frame, s, (size_t)len, 0);
      /*rv = cdrom_read_lba(stream, stream->cdrom.cur_lba, s, (size_t)len, skip);*/

      if (rv)
      {
#ifdef CDROM_DEBUG
         printf("[CDROM] Failed to read %" PRIu64 " bytes from CD.\n", len);
         fflush(stdout);
#endif
      }
   }
#endif
}

int64_t retro_vfs_file_read_cdrom_track(libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
   uint64_t bytes_read = 0;

   if (!stream->parent)
      return -1;

   if (stream->track->last_frame_lba == stream->track->cur_lba)
   {
      const int used = stream->track->byte_pos % stream->track->sector_size;
      int remaining = stream->track->sector_size - used;
      if (remaining > len)
      {
         memcpy(s, stream->track->last_frame + used, len);
         stream->track->byte_pos += len;
         return len;
      }

      memcpy(s, stream->track->last_frame + used, remaining);
      s = (char*)s + remaining;
      len -= remaining;
      bytes_read += remaining;

      stream->track->cur_lba++;
      retro_vfs_file_seek_cdrom_track_sector(stream, stream->track->cur_lba);
   }

   while (len >= stream->track->sector_size)
   {
      retro_vfs_file_read_cdrom_track_sector(stream, s, stream->track->sector_size);
      s = (char*)s + 2352;

      stream->track->cur_lba++;

      bytes_read += 2352;
      len -= 2352;
   }

   stream->track->byte_pos = stream->track->cur_lba * stream->track->sector_size;

   if (len > 0)
   {
      retro_vfs_file_read_cdrom_track_sector(stream, stream->track->last_frame, stream->track->sector_size);
      memcpy(s, stream->track->last_frame, len);
      bytes_read += len;

      stream->track->byte_pos += len;
      stream->track->last_frame_lba = stream->track->cur_lba;

#ifdef HAVE_CDROM
      cdrom_lba_to_msf(stream->track->cur_lba, &stream->track->cur_min, &stream->track->cur_sec, &stream->track->cur_frame);
#endif
   }

   return bytes_read;
}

int64_t retro_vfs_file_tell_cdrom_track(libretro_vfs_implementation_file* stream)
{
   return stream->track->byte_pos;
}

static int retro_vfs_file_find_cdrom_file_sector(libretro_vfs_implementation_file* track_stream, const char* path, unsigned* file_size)
{
   uint8_t buffer[2352], *tmp;
   int sector, path_length;

   const char* slash = strrchr(path, '\\');
   if (slash)
   {
      /* navigate the path to the directory record for the file */
      const int dir_length = (int)(slash - path);
      memcpy(buffer, path, dir_length);
      buffer[dir_length] = '\0';

      sector = retro_vfs_file_find_cdrom_file_sector(track_stream, (const char*)buffer, NULL);
      if (sector < 0)
         return sector;

      path += dir_length + 1;
   }
   else
   {
      int offset;

      /* find the cd information (always 16 frames in) */
      retro_vfs_file_seek_cdrom_track_sector(track_stream, 16);
      retro_vfs_file_read_cdrom_track(track_stream, buffer, sizeof(buffer));

      /* the directory_record starts at 156 bytes into the sector.
      * the sector containing the table of contents is a 3 byte value that is 2 bytes into the directory_record. */
      offset = track_stream->track->sector_header_size + 156 + 2;
      sector = buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16);
   }

   /* process the table of contents */
   retro_vfs_file_seek_cdrom_track_sector(track_stream, sector);
   retro_vfs_file_read_cdrom_track(track_stream, buffer, sizeof(buffer));

   path_length = strlen(path);
   tmp = buffer + track_stream->track->sector_header_size;
   while (tmp < buffer + sizeof(buffer))
   {
      /* the first byte of the record is the length of the record - if 0, we reached the end of the data */
      if (!*tmp)
         break;

      /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
      if ((tmp[33 + path_length] == ';' || tmp[33 + path_length] == '\0') &&
         strncasecmp((const char*)(tmp + 33), path, path_length) == 0)
      {
         /* the file contents are in the sector identified in bytes 2-4 of the record */
         sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
         retro_vfs_file_seek_cdrom_track_sector(track_stream, sector);

         /* the file size is in bytes 10-13 of the record */
         if (file_size)
            *file_size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

#ifdef CDROM_DEBUG
         {
            unsigned char min, sec, frame;
            cdrom_lba_to_msf(sector, &min, &sec, &frame);
            printf("[CDROM] found %s (MSF %02u:%02u:%02u) (LBA %u)\n", path, (unsigned)min, (unsigned)sec, (unsigned)frame, sector);
            fflush(stdout);
         }
#endif

         return sector;
      }

      /* the first byte of the record is the length of the record */
      tmp += tmp[0];
   }

#ifdef CDROM_DEBUG
   printf("[CDROM] did not find %s\n", path);
   fflush(stdout);
#endif

   return -1;
}

libretro_vfs_implementation_file* retro_vfs_file_open_cdrom_file(libretro_vfs_implementation_file* track_stream, const char* path)
{
   libretro_vfs_implementation_file* file_stream;
   unsigned file_size = 0;

   int sector = retro_vfs_file_find_cdrom_file_sector(track_stream, path, &file_size);
   if (sector < 0)
      return NULL;

   file_stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*file_stream));
   file_stream->scheme = (track_stream->scheme == VFS_SCHEME_CDROM_TRACK) ? VFS_SCHEME_CDROM_FILE : VFS_SCHEME_CUE_BIN_FILE;
   file_stream->parent = track_stream;
   file_stream->parent_offset = sector * 2352 + track_stream->track->sector_header_size;
   file_stream->orig_path = strdup(path);
   file_stream->size = file_size;

   return file_stream;
}

int64_t retro_vfs_file_tell_cdrom_file(libretro_vfs_implementation_file* stream)
{
   if (stream->parent != NULL && stream->parent->track)
   {
      const int64_t current_offset_raw = stream->parent->track->byte_pos - stream->parent_offset;
      return (current_offset_raw / stream->parent->track->sector_size) * 2048 + (current_offset_raw % stream->parent->track->sector_size);
   }

   return -1;
}

int64_t retro_vfs_file_seek_cdrom_file(libretro_vfs_implementation_file* stream, int64_t offset, int whence)
{
   int64_t offset_raw;
   switch (whence)
   {
      case SEEK_CUR:
      {
         const int64_t current_offset_raw = stream->parent->track->byte_pos - stream->parent_offset;
         const int64_t current_offset = (current_offset_raw / stream->parent->track->sector_size) * 2048 + (current_offset_raw % stream->parent->track->sector_size);
         offset = current_offset + offset;
         break;
      }

      case SEEK_END:
         offset = stream->size - offset;
         break;
   }

   offset_raw = (offset / 2048) * stream->parent->track->sector_size + (offset % 2048);

   return retro_vfs_file_seek_cdrom_track(stream->parent, offset_raw, SEEK_SET);
}

int64_t retro_vfs_file_read_cdrom_file(libretro_vfs_implementation_file* stream, void* s, uint64_t len)
{
   uint8_t buffer[2352];
   int64_t bytes_read = 0;

   if (!stream || !stream->parent || !stream->parent->track)
      return 0;

   if (stream->parent->track->last_frame_lba == stream->parent->track->cur_lba)
   {
      const int used = stream->parent->track->byte_pos % stream->parent->track->sector_size - stream->parent->track->sector_header_size;
      int remaining = 2048 - used;
      if (remaining > 0)
      {
         if (remaining > len)
         {
            retro_vfs_file_read_cdrom_track(stream->parent, s, len);
            return len;
         }

         retro_vfs_file_read_cdrom_track(stream->parent, s, remaining);
         s = (char*)s + remaining;
         bytes_read += remaining;
      }

      stream->parent->track->cur_lba = stream->parent->track->last_frame_lba + 1;
   }

   while (len >= 2048)
   {
      retro_vfs_file_read_cdrom_track_sector(stream->parent, buffer, stream->parent->track->sector_size);
      stream->parent->track->cur_lba++;

      memcpy(s, buffer + stream->parent->track->sector_header_size, 2048);
      s = (char*)s + 2048;
      bytes_read += 2048;
      len -= 2048;
   }

   stream->parent->track->byte_pos = stream->parent->track->cur_lba * stream->parent->track->sector_size;

   if (len > 0)
   {
      retro_vfs_file_read_cdrom_track(stream->parent, buffer, len + stream->parent->track->sector_header_size);
      memcpy(s, buffer + stream->parent->track->sector_header_size, len);

      stream->parent->track->byte_pos += len;
      bytes_read += len;
   }

   return bytes_read;
}
