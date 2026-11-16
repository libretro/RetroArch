/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdfs_file.c).
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


/* The path side of cdfs: formats/cdfs/cdfs.c reads only through the
 * stream a track holds, and this resolves the filename, opens cue, bin,
 * iso and chd files and hands the stream in. */

#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#ifdef HAVE_CHD
#include <streams/chd_stream.h>
#endif
#include <streams/cdfs_file.h>

static cdfs_track_t* cdfs_open_cue_track(
      const char* path, unsigned int track_index)
{
   cdfs_cue_info_t info;
   char track_path[PATH_MAX_LENGTH];
   int64_t _len;
   bool parsed              = false;
   char *cue_contents       = NULL;
   cdfs_track_t *track      = NULL;
   intfstream_t *cue_stream = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!cue_stream)
      return NULL;

   /* A cue sheet is a small text file: read it with one bulk read and
    * release the handle before the (potentially slow) bin open below,
    * instead of interleaving parsing with per-line stream reads. */
   _len = intfstream_get_size(cue_stream);
   if (_len > 0 && (cue_contents = (char*)malloc((size_t)_len)))
      parsed = (intfstream_read(cue_stream, cue_contents, _len) == _len);

   intfstream_close(cue_stream);
   free(cue_stream);

   if (parsed)
      parsed = cdfs_parse_cue(cue_contents, (size_t)_len,
            track_index, &info);

   if (cue_contents)
      free(cue_contents);

   if (!parsed)
      return NULL;

   /* Resolve the FILE name from the cue against the cue's own
    * directory unless it already carries a path. */
   if (     strchr(info.bin_name, '/')
         || strchr(info.bin_name, '\\'))
      strlcpy(track_path, info.bin_name, sizeof(track_path));
   else
   {
      fill_pathname_basedir(track_path, path, sizeof(track_path));
      strlcat(track_path, info.bin_name, sizeof(track_path));
   }

   track = cdfs_track_from_stream(intfstream_open_file(
            track_path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE),
         info.first_sector_offset, info.first_sector_index);

   if (track && track->stream_sector_size == 0)
   {
      track->stream_sector_size = info.sector_size;

      if (info.sector_size == 2352)
         track->stream_sector_header_size = 16;
      else if (info.sector_size == 2336)
         track->stream_sector_header_size = 8;
   }

   return track;
}

#ifdef HAVE_CHD
static cdfs_track_t* cdfs_open_chd_track(const char* path, int32_t track_index)
{
   cdfs_track_t *track;
   intfstream_t *intf_stream = intfstream_open_chd_track(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE,
         track_index);
   if (!intf_stream)
      return NULL;

   track = cdfs_track_from_stream(intf_stream,
         intfstream_get_offset_to_start(intf_stream),
         intfstream_get_first_sector(intf_stream));

   if (track && track->stream_sector_header_size == 0)
   {
      track->stream_sector_size = intfstream_get_frame_size(intf_stream);

      if (track->stream_sector_size == 2352)
         track->stream_sector_header_size = 16;
      else if (track->stream_sector_size == 2336)
         track->stream_sector_header_size = 8;
   }

   return track;
}
#endif

struct cdfs_track_t* cdfs_open_track(const char* path,
      unsigned int track_index)
{
   const char* ext = path_get_extension(path);

   if (string_is_equal_noncase(ext, "cue"))
      return cdfs_open_cue_track(path, track_index);

#ifdef HAVE_CHD
   if (string_is_equal_noncase(ext, "chd"))
      return cdfs_open_chd_track(path, track_index);
#endif

   /* if opening track 1, try opening as a raw track */
   if (track_index == 1)
      return cdfs_open_raw_track(path);

   /* unsupported file type */
   return NULL;
}

struct cdfs_track_t* cdfs_open_data_track(const char* path)
{
   const char* ext = path_get_extension(path);

   if (string_is_equal_noncase(ext, "cue"))
      return cdfs_open_cue_track(path, 0);

#ifdef HAVE_CHD
   if (string_is_equal_noncase(ext, "chd"))
      return cdfs_open_chd_track(path, CHDSTREAM_TRACK_PRIMARY);
#endif

   /* unsupported file type - try opening as a raw track */
   return cdfs_open_raw_track(path);
}

cdfs_track_t* cdfs_open_raw_track(const char* path)
{
   const char *ext     = path_get_extension(path);
   cdfs_track_t *track = NULL;

   if (     string_is_equal_noncase(ext, "bin")
         || string_is_equal_noncase(ext, "iso"))
   {
      intfstream_t* file = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

      track = cdfs_track_from_stream(file, 0, 0);
      if (track && track->stream_sector_size == 0)
      {
         cdfs_track_sector_size_from_length(track);
         if (track->stream_sector_size == 0)
         {
            cdfs_close_track(track);
            track = NULL;
         }
      }
   }

   return track;
}
