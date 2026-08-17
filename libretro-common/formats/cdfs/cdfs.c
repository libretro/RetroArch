#include <formats/cdfs.h>

#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CHD
#include <streams/chd_stream.h>
#endif

/* This TU is deliberately kept as a single translation unit but is
 * organised in two layers:
 *
 *  - the pure layer (everything through cdfs_parse_cue): ISO-9660
 *    sector/directory/file logic plus cue-sheet parsing.  It only ever
 *    reads through an already-open track->stream or a caller-supplied
 *    memory buffer, and never opens a path.  Entry points:
 *    cdfs_track_from_stream(), cdfs_parse_cue(), cdfs_open_file() and
 *    the read/seek family.
 *
 *  - the path adapter layer (cdfs_open_cue_track onward): resolves
 *    filenames, opens cue/bin/iso/chd files via intfstream, then hands
 *    the resulting stream to the pure layer.  file_path.h and the VFS
 *    are only used here.  Callers that already hold a stream (or an
 *    in-memory image) can skip this layer entirely. */

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
   if (intfstream_read(track->stream, &buffer, sizeof(buffer)) != (int64_t)sizeof(buffer))
      return;

   /* if this is a CDROM-XA data source, the "CD001" tag will be 25 bytes into the sector */
   if (  buffer[25] == 0x43
      && buffer[26] == 0x44
      && buffer[27] == 0x30
      && buffer[28] == 0x30
      && buffer[29] == 0x31)
   {
      track->stream_sector_size        = 2352;
      track->stream_sector_header_size = 24;
   }
   /* otherwise it should be 17 bytes into the sector */
   else if (buffer[17] == 0x43
      &&    buffer[18] == 0x44
      &&    buffer[19] == 0x30
      &&    buffer[20] == 0x30
      &&    buffer[21] == 0x31)
   {
      track->stream_sector_size = 2352;
      track->stream_sector_header_size = 16;
   }
   else
   {
      /* ISO-9660 says the first twelve bytes of a sector should be the sync pattern 00 FF FF FF FF FF FF FF FF FF FF 00 */
      if (
            buffer[ 0] == 0
         && buffer[ 1] == 0xFF
         && buffer[ 2] == 0xFF
         && buffer[ 3] == 0xFF
         && buffer[ 4] == 0xFF
         && buffer[ 5] == 0xFF
         && buffer[ 6] == 0xFF
         && buffer[ 7] == 0xFF
         && buffer[ 8] == 0xFF
         && buffer[ 9] == 0xFF
         && buffer[10] == 0xFF
         && buffer[11] == 0)
      {
         /* if we didn't find a CD001 tag, this format may predate ISO-9660 */

         /* after the 12 byte sync pattern is three bytes identifying the sector and then one byte for the mode (total 16 bytes) */
         track->stream_sector_size        = 2352;
         track->stream_sector_header_size = 16;
      }
   }
}

static void cdfs_determine_sector_size_from_file_size(cdfs_track_t* track)
{
   /* attempt to determine stream_sector_size from file size */
   size_t _len = intfstream_get_size(track->stream);

   if ((_len % 2352) == 0)
   {
      /* raw tracks use all 2352 bytes and have a 24 byte header */
      track->stream_sector_size        = 2352;
      track->stream_sector_header_size = 24;
   }
   else if ((_len % 2048) == 0)
   {
      /* cooked tracks eliminate all header/footer data */
      track->stream_sector_size        = 2048;
      track->stream_sector_header_size = 0;
   }
   else if ((_len % 2336) == 0)
   {
      /* MODE 2 format without 16-byte sync data */
      track->stream_sector_size        = 2336;
      track->stream_sector_header_size = 8;
   }
}

static void cdfs_seek_track_sector(cdfs_track_t* track, unsigned int sector)
{
   intfstream_seek(track->stream,
           sector * track->stream_sector_size
         + track->stream_sector_header_size
         + track->first_sector_offset, SEEK_SET);
}

void cdfs_seek_sector(cdfs_file_t* file, unsigned int sector)
{
   /* only allowed if open_file was called with a NULL path */
   if (file->first_sector == 0)
   {
      if (file->current_sector != (int)sector)
      {
         file->current_sector      = (int)sector;
         file->sector_buffer_valid = 0;
      }

      file->pos                    = sector * 2048;
      file->current_sector_offset  = 0;
   }
}

uint32_t cdfs_get_num_sectors(cdfs_file_t* file)
{
   uint32_t frame_size = intfstream_get_frame_size(file->track->stream);
   if (frame_size == 0)
   {
      frame_size = file->track->stream_sector_size;
      if (frame_size == 0)
         frame_size = 1; /* prevent divide by 0 error if sector size is unknown */
   }
   return (uint32_t)(intfstream_get_size(file->track->stream) / frame_size);
}

uint32_t cdfs_get_first_sector(cdfs_file_t* file)
{
   return file->track->first_sector_index;
}

static int cdfs_find_file(cdfs_file_t* file, const char* path)
{
   size_t path_length;
   int sector;
   int ret;
   /* Heap-held, one allocation per path component: this function
    * recurses per directory level with the sector buffer live in
    * every frame, so as a local it cost 2 KiB times the path depth
    * on stacks whose whole budget is 8 KiB.  A shared buffer would
    * not do -- the recursive call receives this level's buffer as
    * its path argument. */
   uint8_t *buffer, *tmp;
   const char* slash = strrchr(path, '\\');

   if (!(buffer = (uint8_t*)malloc(2048)))
      return -1;

   if (slash)
   {
      /* navigate the path to the directory record for the file */
      const int dir_length = (int)(slash - path);
      /* buffer is 2048 bytes and gets a NUL at [dir_length], so the
       * directory component must be shorter than that.  path comes
       * from the caller; the in-tree caller passes NULL, but this is
       * a libretro-common entry point other consumers reach with an
       * arbitrary path, and without this a long component overruns
       * the stack buffer. */
      if (dir_length >= 2048)
         { ret = -1; free(buffer); return ret; }
      memcpy(buffer, path, dir_length);
      buffer[dir_length] = '\0';

      sector = cdfs_find_file(file, (const char*)buffer);
      if (sector < 0)
         { ret = sector; free(buffer); return ret; }

      path += dir_length + 1;
   }
   else
   {
      int offset;

      /* find the CD information (always 16 frames in) */
      cdfs_seek_track_sector(file->track, 16);
      intfstream_read(file->track->stream, buffer, 2048);

      /* the directory_record starts at 156 bytes into the sector.
       * the sector containing the root directory contents is a
       * 3 byte value that is 2 bytes into the directory_record. */
      offset = 156 + 2;
      sector = buffer[offset] | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16);
   }

   /* process the contents of the directory */
   cdfs_seek_track_sector(file->track, sector);
   intfstream_read(file->track->stream, buffer, 2048);

   path_length = strlen(path);
   tmp         = buffer;

   /* The directory record layout (ECMA-119 §9.1) is:
    *   byte  0:  record length (1 byte; 0 = end of records)
    *   byte  1:  extended-attr length
    *   bytes 2-9: location-of-extent (LE+BE) -- we use bytes 2..4
    *   bytes 10-17: data length (LE+BE) -- we use bytes 10..13
    *   ...
    *   byte 32:  filename length
    *   bytes 33..32+filename_length: filename
    *
    * Pre-this-patch the read at tmp[33 + path_length] and the
    * subsequent reads at tmp[2..4]/tmp[10..13] could run off the
    * end of the 2048-byte stack buffer when a malformed disc
    * image had a record positioned (or chained via the
    * tmp += tmp[0] advance) so that tmp + 33 + path_length lay
    * past buffer + 2048.  An attacker who can place a
    * crafted sector at the directory offset gets:
    *  - a 1-byte info-leak from adjacent stack via the
    *    tmp[33 + path_length] comparison vs ';' / '\0';
    *  - filename-length bytes leaked via strncasecmp's
    *    short-circuit timing;
    *  - up to 4 bytes of attacker-influenced stack data feeding
    *    `sector` (tmp[2..4]) and `file->size` (tmp[10..13]),
    *    redirecting the next intfstream_read.
    *
    * The advance "tmp += tmp[0]" itself can land tmp anywhere
    * up to 255 bytes ahead.  The guard tmp < buffer + sizeof
    * only catches the case where the loop body re-runs; it does
    * not protect against a single iteration whose body reads
    * past the buffer.
    *
    * Tighten the guard so the entire record (header through the
    * byte at offset 33 + path_length) must fit, and require the
    * record's claimed length to be at least large enough to
    * cover the filename comparison. */
   while (   tmp < buffer + 2048
          && (size_t)(tmp - buffer) + 33 + path_length < 2048)
   {
      /* The first byte of the record is the length of
       * the record - if 0, we reached the end of the data */
      if (!*tmp)
         break;

      /* Reject records whose claimed length cannot accommodate
       * the filename comparison below.  A legitimate ECMA-119
       * directory record has length >= 33 + filename_length
       * + 1 (the version-separator byte). */
      if (tmp[0] < 33 + path_length + 1)
         break;

      /* filename is 33 bytes into the record and
       * the format is "FILENAME;version" or "DIRECTORY" */
      if (        (tmp[33 + path_length] == ';'
               || (tmp[33 + path_length] == '\0'))
               &&  strncasecmp((const char*)(tmp + 33), path, path_length) == 0)
      {
         /* the file size is in bytes 10-13 of the record */
         file->size =
              (tmp[10])
            | (tmp[11] << 8)
            | (tmp[12] << 16)
            | (tmp[13] << 24);

         /* the file contents are in the sector identified
          * in bytes 2-4 of the record */
         sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
         { ret = sector; free(buffer); return ret; }
      }

      /* the first byte of the record is the length of the record */
      tmp += tmp[0];
   }

   { ret = -1; free(buffer); return ret; }
}

int cdfs_open_file(cdfs_file_t* file, cdfs_track_t* track, const char* path)
{
   if (!file || !track)
      return 0;

   memset(file, 0, sizeof(*file));

   file->track          = track;
   file->current_sector = -1;
   file->first_sector   = -1;

   if (path)
      file->first_sector = cdfs_find_file(file, path);
   else if (file->track->stream_sector_size)
   {
      file->first_sector = 0;
      file->size         = (unsigned int)((intfstream_get_size(
               file->track->stream) / file->track->stream_sector_size)
         * 2048);
      return 1;
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

   /* A seek that changed sectors leaves the position mid-sector with
    * no cached data.  Refill the cache for the current sector first so
    * the consume path below starts at current_sector_offset; without
    * this the refill at the bottom of this function copies from the
    * start of the sector regardless of the seek target. */
   if (     !file->sector_buffer_valid
         &&  file->current_sector_offset != 0
         &&  file->current_sector >= file->first_sector)
   {
      cdfs_seek_track_sector(file->track, file->current_sector);
      intfstream_read(file->track->stream, file->sector_buffer, 2048);
      file->sector_buffer_valid = 1;
   }

   if (file->sector_buffer_valid)
   {
      size_t remaining = 2048 - file->current_sector_offset;
      if (remaining > 0)
      {
         if (remaining >= len)
         {
            memcpy(buffer,
                  &file->sector_buffer[file->current_sector_offset],
                  (size_t)len);
            file->current_sector_offset += len;
            /* This early return previously skipped the position
             * accounting at the bottom of the function, leaving
             * file->pos (and thus cdfs_tell / SEEK_CUR) stale after
             * any read served entirely from the sector cache. */
            file->pos                   += (unsigned int)len;
            return len;
         }

         memcpy(buffer,
               &file->sector_buffer[file->current_sector_offset], remaining);
         buffer      = (char*)buffer + remaining;
         bytes_read += remaining;
         len        -= remaining;

         file->current_sector_offset += remaining;
      }

      ++file->current_sector;
      file->current_sector_offset = 0;
      file->sector_buffer_valid   = 0;
   }
   else if (file->current_sector < file->first_sector)
   {
      file->current_sector        = file->first_sector;
      file->current_sector_offset = 0;
   }

   while (len >= 2048)
   {
      cdfs_seek_track_sector(file->track, file->current_sector);
      intfstream_read(file->track->stream, buffer, 2048);

      buffer      = (char*)buffer + 2048;
      bytes_read += 2048;

      ++file->current_sector;

      len        -= 2048;
   }

   if (len > 0)
   {
      cdfs_seek_track_sector(file->track, file->current_sector);
      intfstream_read(file->track->stream, file->sector_buffer, 2048);
      memcpy(buffer, file->sector_buffer, (size_t)len);
      file->current_sector_offset = (unsigned int)len;
      file->sector_buffer_valid   = 1;

      bytes_read += len;
   }

   file->pos += bytes_read;
   return bytes_read;
}

void cdfs_close_file(cdfs_file_t* file)
{
   /* Not really anything to do here, just
    * clear out the first_sector so
    * read() won't do anything */
   if (file)
      file->first_sector = -1;
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

   /* current_sector holds a disc-absolute sector index (it is handed
    * straight to cdfs_seek_track_sector by the read path), so the
    * byte position must be rebased onto the file's first sector.
    * Using the bare file-relative sector here made any seek on a
    * named file (first_sector != 0) compare/store mismatched sector
    * spaces: the subsequent read saw current_sector < first_sector,
    * clamped back to the start of the file and reset the intra-sector
    * offset, returning data from offset 0 instead of the seek target. */
   new_sector = file->first_sector + file->pos / 2048;
   if (new_sector != file->current_sector)
   {
      file->current_sector      = new_sector;
      file->sector_buffer_valid = false;
   }

   return 0;
}

/* --- Pure layer: cue-sheet parsing and stream wrapping ---------------- */

static const char *cdfs_skip_spaces(const char *p, const char *end)
{
   while (p < end && (*p == ' ' || *p == '\t'))
      ++p;
   return p;
}

/* Bounded base-10 parse; advances *p past the digits consumed.
 * Returns 0 when no digit is present (mirrors atoi/strtol behaviour
 * for the inputs this parser sees). */
static unsigned cdfs_parse_unsigned(const char **p, const char *end)
{
   unsigned v    = 0;
   const char *s = *p;

   while (s < end && *s >= '0' && *s <= '9')
   {
      v = v * 10 + (unsigned)(*s - '0');
      ++s;
   }

   *p = s;
   return v;
}

cdfs_track_t* cdfs_track_from_stream(
      intfstream_t* stream,
      unsigned first_sector_offset,
      unsigned first_sector_index)
{
   cdfs_track_t* track = NULL;

   if (!stream)
      return NULL;

   if (!(track = (cdfs_track_t*)calloc(1, sizeof(*track))))
   {
      /* Ownership of the stream transfers on this call; close it so
       * the caller never has to distinguish failure modes. */
      intfstream_close(stream);
      free(stream);
      return NULL;
   }

   track->stream              = stream;
   track->first_sector_offset = first_sector_offset;
   track->first_sector_index  = first_sector_index;

   cdfs_determine_sector_size(track);

   return track;
}

bool cdfs_parse_cue(const char *cue_text, size_t len,
      unsigned track_index, cdfs_cue_info_t *out)
{
   char current_track_path[PATH_MAX_LENGTH];
   const char *p                             = cue_text;
   const char *end                           = cue_text + len;
   unsigned found_track                      = 0;
   unsigned sector_size                      = 0;
   unsigned previous_sector_size             = 0;
   unsigned previous_index_sector_offset     = 0;
   unsigned track_offset                     = 0;

   if (!cue_text || !out)
      return false;

   current_track_path[0] = '\0';
   memset(out, 0, sizeof(*out));

   while (p < end)
   {
      const char *line;
      const char *line_end;

      p        = cdfs_skip_spaces(p, end);
      line     = p;
      while (p < end && *p != '\n')
         ++p;
      line_end = p;
      if (p < end)
         ++p; /* consume the newline */

      if (line_end - line >= 4 && !strncasecmp(line, "FILE", 4))
      {
         const char *file = cdfs_skip_spaces(line + 4, line_end);

         if (file < line_end)
         {
            /* Walk back from the end of the line to the whitespace
             * preceding the trailing type token (e.g. BINARY), leaving
             * [file, file_end) covering the (possibly quoted) name.
             * When the line has no terminator we start on the last
             * character instead of the terminator position. */
            const char *file_end =
                  (line_end < end) ? line_end : (line_end - 1);
            while (     file_end > file
                     && *file_end != ' '
                     && *file_end != '\t')
               --file_end;

            if (     file_end > file
                  && file[0]      == '"'
                  && file_end[-1] == '"')
            {
               ++file;
               --file_end;
            }

            if (file_end >= file)
            {
               size_t name_len = (size_t)(file_end - file);
               if (name_len >= sizeof(current_track_path))
                  name_len = sizeof(current_track_path) - 1;
               memcpy(current_track_path, file, name_len);
               current_track_path[name_len] = '\0';
            }
         }

         previous_sector_size         = 0;
         previous_index_sector_offset = 0;
         track_offset                 = 0;
      }
      else if (line_end - line >= 5 && !strncasecmp(line, "TRACK", 5))
      {
         const char *track     = cdfs_skip_spaces(line + 5, line_end);
         const char *num       = track;
         unsigned track_number = cdfs_parse_unsigned(&num, line_end);

         while (track < line_end && *track != ' ' && *track != '\t')
            ++track;

         previous_sector_size = sector_size;

         track = cdfs_skip_spaces(track, line_end);

         if (line_end - track >= 4 && !strncasecmp(track, "MODE", 4))
         {
            const char *size_str = track + 6;

            /* track_index = 0 means find the first data track */
            if (!track_index || track_index == track_number)
               found_track = track_number;

            if (size_str < line_end)
               sector_size = cdfs_parse_unsigned(&size_str, line_end);
            else
               sector_size = 0;
         }
         else /* assume AUDIO */
            sector_size = 2352;
      }
      else if (line_end - line >= 5 && !strncasecmp(line, "INDEX", 5))
      {
         unsigned sector_offset;
         unsigned min          = 0, sec = 0, frame = 0;
         const char *index     = cdfs_skip_spaces(line + 5, line_end);
         const char *num       = index;
         unsigned index_number = cdfs_parse_unsigned(&num, line_end);

         while (index < line_end && *index != ' ' && *index != '\t')
            ++index;
         index = cdfs_skip_spaces(index, line_end);

         min   = cdfs_parse_unsigned(&index, line_end);
         if (index < line_end && *index == ':')
            ++index;
         sec   = cdfs_parse_unsigned(&index, line_end);
         if (index < line_end && *index == ':')
            ++index;
         frame = cdfs_parse_unsigned(&index, line_end);

         sector_offset                 = ((min * 60) + sec) * 75 + frame;
         sector_offset                -= previous_index_sector_offset;
         track_offset                 += sector_offset * previous_sector_size;
         previous_sector_size          = sector_size;
         previous_index_sector_offset += sector_offset;

         if (found_track && index_number == 1)
         {
            if (!current_track_path[0])
               return false;

            strlcpy(out->bin_name, current_track_path,
                  sizeof(out->bin_name));
            out->track_index         = found_track;
            out->sector_size         = sector_size;
            out->first_sector_offset = track_offset;
            /* NOTE: only valid if all tracks are in the same BIN file.
             * Otherwise we would need to know how many sectors each
             * previous BIN file holds, which a cue sheet does not
             * store.  This affects cdfs_get_first_sector, which
             * luckily isn't used much. */
            out->first_sector_index  = previous_index_sector_offset;
            return true;
         }
      }
   }

   return false;
}

/* --- Path adapter layer ------------------------------------------------ */

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
         cdfs_determine_sector_size_from_file_size(track);
         if (track->stream_sector_size == 0)
         {
            cdfs_close_track(track);
            track = NULL;
         }
      }
   }

   return track;
}

void cdfs_close_track(cdfs_track_t* track)
{
   if (track)
   {
      if (track->stream)
      {
         intfstream_close(track->stream);
         free(track->stream);
      }

      free(track);
   }
}
