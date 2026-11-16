/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdfs_file_test.c).
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

/* streams/cdfs_file.c: tracks opened by filename.
 *
 * - a cooked .iso and a raw .bin with no volume descriptor take their
 *   sector layout from their length (2048 and 2352 bytes a sector);
 * - a cue sheet in a directory of its own names its bin relatively, is
 *   resolved against its own directory, and gives the MODE1/2352 layout
 *   and the INDEX 01 offset of the track it selects;
 * - cdfs_open_data_track takes a cue's first data track, an .iso as raw;
 * - each opened track reads back the marker written in its sector 3;
 * - an unknown extension opens only as track 1, and then only if it is
 *   a bin or an iso.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/cdfs.h>
#include <file/file_path.h>
#include <streams/cdfs_file.h>
#include <streams/file_stream.h>

#define SECTORS 24

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: check failed: %s\n", \
      __FILE__, __LINE__, #c); failures++; } } while (0)

static const char dir[]     = "cdfs_file_tmp";
static const char iso[]     = "cdfs_file_tmp/cooked.iso";
static const char bin[]     = "cdfs_file_tmp/raw.bin";
static const char cue[]     = "cdfs_file_tmp/disc.cue";
static const char pregap[]  = "cdfs_file_tmp/pregap.bin";
static const char pre_cue[] = "cdfs_file_tmp/pregap.cue";
static const char odd[]     = "cdfs_file_tmp/disc.img";

/* @sectors of @size bytes, user data at @header; sector s's data
 * starts with "S<s>" and the rest of every byte is s */
static int write_image(const char *path, unsigned sectors, unsigned size,
      unsigned header, unsigned lead)
{
   uint8_t buf[2352];
   unsigned s;
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
      return 0;
   memset(buf, 0xEE, sizeof(buf));
   for (s = 0; s < lead; s++)
      filestream_write(f, buf, size);
   for (s = 0; s < sectors; s++)
   {
      memset(buf, (int)s, size);
      sprintf((char*)buf + header, "S%u", s);
      if (filestream_write(f, buf, size) != (int64_t)size)
      {
         filestream_close(f);
         return 0;
      }
   }
   filestream_close(f);
   return 1;
}

static int write_text(const char *path, const char *text)
{
   return filestream_write_file(path, text, (int64_t)strlen(text));
}

/* Sector 3 of the track, through the cdfs raw view */
static void reads_marker(cdfs_track_t *track, const char *what)
{
   cdfs_file_t file;
   char        got[8];
   (void)what;
   memset(got, 0, sizeof(got));
   CHECK(cdfs_open_file(&file, track, NULL) == 1);
   cdfs_seek_sector(&file, 3);
   CHECK(cdfs_read_file(&file, got, 2) == 2);
   CHECK(memcmp(got, "S3", 2) == 0);
   cdfs_close_file(&file);
}

int main(void)
{
   cdfs_track_t *t;

   path_mkdir(dir);
   CHECK(write_image(iso, SECTORS, 2048, 0, 0));
   CHECK(write_image(bin, SECTORS, 2352, 16, 0));
   /* two sectors ahead of INDEX 01 */
   CHECK(write_image(pregap, SECTORS, 2352, 16, 2));
   CHECK(write_image(odd, SECTORS, 2048, 0, 0));
   CHECK(write_text(cue,
         "FILE \"raw.bin\" BINARY\n"
         "  TRACK 01 MODE1/2352\n"
         "    INDEX 01 00:00:00\n"));
   CHECK(write_text(pre_cue,
         "FILE \"pregap.bin\" BINARY\n"
         "  TRACK 01 MODE1/2352\n"
         "    INDEX 00 00:00:00\n"
         "    INDEX 01 00:00:02\n"));

   /* a cooked iso: 2048 from the length */
   if ((t = cdfs_open_raw_track(iso)))
   {
      CHECK(t->stream_sector_size == 2048 && t->stream_sector_header_size == 0);
      reads_marker(t, "iso");
      cdfs_close_track(t);
   }
   else
      failures++;

   /* a raw bin: 2352 from the length */
   if ((t = cdfs_open_raw_track(bin)))
   {
      CHECK(t->stream_sector_size == 2352);
      cdfs_close_track(t);
   }
   else
      failures++;

   /* the cue, its bin found beside it */
   if ((t = cdfs_open_track(cue, 1)))
   {
      CHECK(t->stream_sector_size == 2352 && t->stream_sector_header_size == 16);
      CHECK(t->first_sector_offset == 0);
      reads_marker(t, "cue");
      cdfs_close_track(t);
   }
   else
      failures++;

   /* INDEX 01 two sectors in */
   if ((t = cdfs_open_track(pre_cue, 1)))
   {
      CHECK(t->first_sector_offset == 2 * 2352);
      reads_marker(t, "cue pregap");
      cdfs_close_track(t);
   }
   else
      failures++;

   /* first data track: of the cue, and an iso as raw */
   if ((t = cdfs_open_data_track(cue)))
   {
      CHECK(t->stream_sector_size == 2352);
      cdfs_close_track(t);
   }
   else
      failures++;
   if ((t = cdfs_open_data_track(iso)))
   {
      CHECK(t->stream_sector_size == 2048);
      cdfs_close_track(t);
   }
   else
      failures++;

   /* not a bin, iso, cue or chd */
   CHECK(cdfs_open_track(odd, 2) == NULL);
   CHECK(cdfs_open_track(odd, 1) == NULL);
   CHECK(cdfs_open_track("cdfs_file_tmp/missing.cue", 1) == NULL);

   remove(iso); remove(bin); remove(pregap); remove(odd);
   remove(cue); remove(pre_cue);
   remove(dir);
   printf("cdfs file: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
