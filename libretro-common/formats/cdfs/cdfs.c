#include <formats/cdfs.h>

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CHD
#include <streams/chd_stream.h>
#endif

static void cdfs_determine_sector_size(cdfs_track_t* track)
{
   uint8_t buffer[32];
   const int toc_sector = 16;

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
    */

   /* The boot record or primary volume descriptor is always at sector 16 and will contain a "CD001" marker */
   intfstream_seek(track->stream, toc_sector * 2352 + track->first_sector_offset, SEEK_SET);
   if (intfstream_read(track->stream, buffer, sizeof(buffer)) < sizeof(buffer))
      return;

   /* if this is a CDROM-XA data source, the "CD001" tag will be 25 bytes into the sector */
   if (buffer[25] == 0x43 && buffer[26] == 0x44 &&
      buffer[27] == 0x30 && buffer[28] == 0x30 && buffer[29] == 0x31)
   {
      track->stream_sector_size = 2352;
      track->stream_sector_header_size = 24;
   }
   /* otherwise it should be 17 bytes into the sector */
   else if (buffer[17] == 0x43 && buffer[18] == 0x44 &&
      buffer[19] == 0x30 && buffer[20] == 0x30 && buffer[21] == 0x31)
   {
      track->stream_sector_size = 2352;
      track->stream_sector_header_size = 16;
   }
   else
   {
      /* ISO-9660 says the first twelve bytes of a sector should be the sync pattern 00 FF FF FF FF FF FF FF FF FF FF 00 */
      if (buffer[0] == 0 && buffer[1] == 0xFF && buffer[2] == 0xFF && buffer[3] == 0xFF &&
         buffer[4] == 0xFF && buffer[5] == 0xFF && buffer[6] == 0xFF && buffer[7] == 0xFF &&
         buffer[8] == 0xFF && buffer[9] == 0xFF && buffer[10] == 0xFF && buffer[11] == 0)
      {
         /* if we didn't find a CD001 tag, this format may predate ISO-9660 */

         /* after the 12 byte sync pattern is three bytes identifying the sector and then one byte for the mode (total 16 bytes) */
         track->stream_sector_size = 2352;
         track->stream_sector_header_size = 16;
      }
   }
}

static void cdfs_seek_track_sector(cdfs_track_t* track, unsigned int sector)
{
   intfstream_seek(track->stream, sector * track->stream_sector_size + track->stream_sector_header_size + track->first_sector_offset, SEEK_SET);
}

void cdfs_seek_sector(cdfs_file_t* file, unsigned int sector)
{
   /* only allowed if open_file was called with a NULL path */
   if (file->first_sector == 0)
   {
      if (sector != file->current_sector)
      {
         file->current_sector = sector;
         file->sector_buffer_valid = 0;
      }

      file->pos = file->current_sector * 2048;
      file->current_sector_offset = 0;
   }
}

static int cdfs_find_file(cdfs_file_t* file, const char* path)
{
   uint8_t buffer[2048], *tmp;
   int sector, path_length;

   const char* slash = strrchr(path, '\\');
   if (slash)
   {
      /* navigate the path to the directory record for the file */
      const int dir_length = (int)(slash - path);
      memcpy(buffer, path, dir_length);
      buffer[dir_length] = '\0';

      sector = cdfs_find_file(file, (const char*)buffer);
      if (sector < 0)
         return sector;

      path += dir_length + 1;
   }
   else
   {
      int offset;

      /* find the cd information (always 16 frames in) */
      cdfs_seek_track_sector(file->track, 16);
      intfstream_read(file->track->stream, buffer, sizeof(buffer));

      /* the directory_record starts at 156 bytes into the sector.
       * the sector containing the root directory contents is a 3 byte value that is 2 bytes into the directory_record. */
      offset = 156 + 2;
      sector = buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16);
   }

   /* process the contents of the directory */
   cdfs_seek_track_sector(file->track, sector);
   intfstream_read(file->track->stream, buffer, sizeof(buffer));

   path_length = strlen(path);
   tmp = buffer;
   while (tmp < buffer + sizeof(buffer))
   {
      /* the first byte of the record is the length of the record - if 0, we reached the end of the data */
      if (!*tmp)
         break;

      /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
      if ((tmp[33 + path_length] == ';' || tmp[33 + path_length] == '\0') &&
         strncasecmp((const char*)(tmp + 33), path, path_length) == 0)
      {
         /* the file size is in bytes 10-13 of the record */
         file->size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

         /* the file contents are in the sector identified in bytes 2-4 of the record */
         sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
         return sector;
      }

      /* the first byte of the record is the length of the record */
      tmp += tmp[0];
   }

   return -1;
}

int cdfs_open_file(cdfs_file_t* file, cdfs_track_t* track, const char* path)
{
   if (!file || !track)
      return 0;

   memset(file, 0, sizeof(*file));

   file->track = track;

   file->current_sector = -1;
   if (path != NULL)
   {
      file->first_sector = cdfs_find_file(file, path);
   }
   else if (file->track->stream_sector_size)
   {
      file->first_sector = 0;
      file->size = (intfstream_get_size(file->track->stream) / file->track->stream_sector_size) * 2048;
   }
   else
   {
      file->first_sector = -1;
   }

   return (file->first_sector >= 0);
}

int64_t cdfs_read_file(cdfs_file_t* file, void* buffer, uint64_t len)
{
   int bytes_read = 0;

   if (!file || file->first_sector < 0 || !buffer)
      return 0;

   if (len > file->size - file->pos)
      len = file->size - file->pos;

   if (len == 0)
      return 0;

   if (file->sector_buffer_valid)
   {
      size_t remaining = 2048 - file->current_sector_offset;
      if (remaining > 0)
      {
         if (remaining >= len)
         {
            memcpy(buffer, &file->sector_buffer[file->current_sector_offset], len);
            file->current_sector_offset += len;
            return len;
         }

         memcpy(buffer, &file->sector_buffer[file->current_sector_offset], remaining);
         buffer = (char*)buffer + remaining;
         bytes_read += remaining;
         len -= remaining;

         file->current_sector_offset += remaining;
      }

      ++file->current_sector;
      file->current_sector_offset = 0;
      file->sector_buffer_valid = 0;
   }
   else if (file->current_sector < file->first_sector)
   {
      file->current_sector = file->first_sector;
      file->current_sector_offset = 0;
   }

   while (len >= 2048)
   {
      cdfs_seek_track_sector(file->track, file->current_sector);
      intfstream_read(file->track->stream, buffer, 2048);

      buffer = (char*)buffer + 2048;
      bytes_read += 2048;
      ++file->current_sector;

      len -= 2048;
   }

   if (len > 0)
   {
      cdfs_seek_track_sector(file->track, file->current_sector);
      intfstream_read(file->track->stream, file->sector_buffer, 2048);
      memcpy(buffer, file->sector_buffer, len);
      file->current_sector_offset = len;
      file->sector_buffer_valid   = 1;

      bytes_read += len;
   }

   file->pos += bytes_read;
   return bytes_read;
}

void cdfs_close_file(cdfs_file_t* file)
{
   if (file)
   {
      /* not really anything to do here, just clear out the first_sector so read() won't do anything */
      file->first_sector = -1;
   }
}

int64_t cdfs_get_size(cdfs_file_t* file)
{
   if (!file || file->first_sector < 0)
      return 0;

   return file->size;
}

int64_t cdfs_tell(cdfs_file_t* file)
{
   if (!file || file->first_sector < 0)
      return -1;

   return file->pos;
}

int64_t cdfs_seek(cdfs_file_t* file, int64_t offset, int whence)
{
   int64_t new_pos;
   int new_sector;

   if (!file || file->first_sector < 0)
      return -1;

   switch (whence)
   {
      case SEEK_SET:
         new_pos = offset;
         break;

      case SEEK_CUR:
         new_pos = file->pos + offset;
         break;

      case SEEK_END:
         new_pos = file->size - offset;
         break;

      default:
         return -1;
   }

   if (new_pos < 0)
      return -1;
   else if (new_pos > file->size)
      return -1;

   file->pos = (unsigned int)new_pos;
   file->current_sector_offset = file->pos % 2048;

   new_sector = file->pos / 2048;
   if (new_sector != file->current_sector)
   {
      file->current_sector = new_sector;
      file->sector_buffer_valid = false;
   }

   return 0;
}

static void cdfs_skip_spaces(const char** ptr)
{
   while (**ptr && (**ptr == ' ' || **ptr == '\t'))
      ++(*ptr);
}

static cdfs_track_t* cdfs_wrap_stream(intfstream_t* stream, unsigned first_sector_offset)
{
   cdfs_track_t* track;

   if (stream == NULL)
      return NULL;

   track = (cdfs_track_t*)calloc(1, sizeof(*track));
   track->stream = stream;
   track->first_sector_offset = first_sector_offset;
   cdfs_determine_sector_size(track);
   return track;
}

static cdfs_track_t* cdfs_open_cue_track(const char* path, unsigned int track_index)
{
   char* cue                                = NULL;
   const char* line                         = NULL;
   int found_track                          = 0;
   char current_track_path[PATH_MAX_LENGTH] = {0};
   char track_path[PATH_MAX_LENGTH]         = {0};
   unsigned int sector_size                 = 0;
   unsigned int previous_sector_size        = 0;
   unsigned int previous_index_sector_offset= 0;
   unsigned int track_offset                = 0;
   intfstream_t *cue_stream                 = intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t stream_size                      = intfstream_get_size(cue_stream);
   char *cue_contents                       = (char*)malloc(stream_size + 1);
   cdfs_track_t* track                      = NULL;

   if (!cue_contents)
   {
      intfstream_close(cue_stream);
      return NULL;
   }

   intfstream_read(cue_stream, cue_contents, stream_size);
   intfstream_close(cue_stream);

   cue_contents[stream_size] = '\0';

   cue = cue_contents;
   while (*cue)
   {
      cdfs_skip_spaces((const char**)&cue);
      line = cue;

      while (*cue && *cue != '\n')
         ++cue;
      if (*cue)
         *cue++ = '\0';

      if (!strncasecmp(line, "FILE", 4))
      {
         const char *file = line + 4;
         cdfs_skip_spaces(&file);

         if (file[0])
         {
            const char *file_end = cue - 1;
            while (file_end > file && *file_end != ' ' && *file_end != '\t')
               --file_end;

            if (file[0] == '"' && file_end[-1] == '"')
            {
               ++file;
               --file_end;
            }

            memcpy(current_track_path, file, file_end - file);
            current_track_path[file_end - file] = '\0';
         }

         previous_sector_size = 0;
         previous_index_sector_offset = 0;
         track_offset = 0;
      }
      else if (!strncasecmp(line, "TRACK", 5))
      {
         unsigned track_number = 0;

         const char *track = line + 5;
         cdfs_skip_spaces(&track);

         sscanf(track, "%d", &track_number);
         while (*track && *track != ' ' && *track != '\n')
            ++track;

         previous_sector_size = sector_size;

         cdfs_skip_spaces(&track);

         if (!strncasecmp(track, "MODE", 4))
         {
            /* track_index = 0 means find the first data track */
            if (!track_index || track_index == track_number)
               found_track = track_number;

            sector_size = atoi(track + 6);
         }
         else
         {
            /* assume AUDIO */
            sector_size = 2352;
         }
      }
      else if (!strncasecmp(line, "INDEX", 5))
      {
         unsigned min = 0, sec = 0, frame = 0;
         unsigned index_number = 0;
         unsigned sector_offset;
         const char *index = line + 5;

         cdfs_skip_spaces(&index);
         sscanf(index, "%u", &index_number);
         while (*index && *index != ' ' && *index != '\n')
            ++index;
         cdfs_skip_spaces(&index);

         sscanf(index, "%u:%u:%u", &min, &sec, &frame);
         sector_offset = ((min * 60) + sec) * 75 + frame;
         sector_offset -= previous_index_sector_offset;
         track_offset += sector_offset * previous_sector_size;
         previous_sector_size = sector_size;
         previous_index_sector_offset += sector_offset;

         if (found_track && index_number == 1)
         {
            if (strstr(current_track_path, "/") || strstr(current_track_path, "\\"))
            {
               strncpy(track_path, current_track_path, sizeof(track_path));
            }
            else
            {
               fill_pathname_basedir(track_path, path, sizeof(track_path));
               strlcat(track_path, current_track_path, sizeof(track_path));
            }

            break;
         }
      }
   }

   free(cue_contents);

   if (string_is_empty(track_path))
      return NULL;

   track = cdfs_wrap_stream(intfstream_open_file(track_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE), track_offset);
   if (track && track->stream_sector_size == 0)
   {
      track->stream_sector_size = sector_size;

      if (sector_size == 2352)
         track->stream_sector_header_size = 16;
      else if (sector_size == 2336)
         track->stream_sector_header_size = 8;
   }

   return track;
}

#ifdef HAVE_CHD
static cdfs_track_t* cdfs_open_chd_track(const char* path, int32_t track_index)
{
   intfstream_t* intf_stream;
   cdfs_track_t* track;

   intf_stream = intfstream_open_chd_track(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE, track_index);
   if (!intf_stream)
      return NULL;

   track = cdfs_wrap_stream(intf_stream, intfstream_get_offset_to_start(intf_stream));

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

struct cdfs_track_t* cdfs_open_track(const char* path, unsigned int track_index)
{
   const char* ext = path_get_extension(path);

   if (string_is_equal_noncase(ext, "cue"))
      return cdfs_open_cue_track(path, track_index);

#ifdef HAVE_CHD
   if (string_is_equal_noncase(ext, "chd"))
      return cdfs_open_chd_track(path, track_index);
#endif

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
   const char* ext = path_get_extension(path);

   if (string_is_equal_noncase(ext, "bin") || string_is_equal_noncase(ext, "iso"))
      return cdfs_wrap_stream(intfstream_open_file(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE), 0);

   /* unsupported file type */
   return NULL;
}

void cdfs_close_track(cdfs_track_t* track)
{
   if (track)
   {
      intfstream_close(track->stream);
      free(track);
   }
}
