/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdfs.h).
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

#ifndef __RARCH_CDFS_H
#define __RARCH_CDFS_H

#include <stddef.h>

#include <streams/interface_stream.h>
#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

/* these functions provide an interface for locating and reading files within a data track
 * of a CD (following the ISO-9660 directory structure definition)
 */

typedef struct cdfs_track_t
{
   intfstream_t* stream;
   unsigned int stream_sector_size;
   unsigned int stream_sector_header_size;
   unsigned int first_sector_offset;
   unsigned int first_sector_index;
} cdfs_track_t;

typedef struct cdfs_file_t
{
   struct cdfs_track_t* track;
   int first_sector;
   int current_sector;
   int sector_buffer_valid;
   unsigned int current_sector_offset;
   unsigned int size;
   unsigned int pos;
   uint8_t sector_buffer[2048];
} cdfs_file_t;

/* opens the specified file within the CD or virtual CD.
 * if path is NULL, will open the raw CD (useful for 
 * reading CD without having to worry about sector sizes,
 * headers, or checksum data)
 */
int cdfs_open_file(cdfs_file_t* file, cdfs_track_t* stream, const char* path);

void cdfs_close_file(cdfs_file_t* file);

int64_t cdfs_read_file(cdfs_file_t* file, void* buffer, uint64_t len);

int64_t cdfs_get_size(cdfs_file_t* file);

int64_t cdfs_tell(cdfs_file_t* file);

int64_t cdfs_seek(cdfs_file_t* file, int64_t offset, int whence);

void cdfs_seek_sector(cdfs_file_t* file, unsigned int sector);

uint32_t cdfs_get_num_sectors(cdfs_file_t* file);

uint32_t cdfs_get_first_sector(cdfs_file_t* file);

/* --- Pure API: no path or VFS I/O ------------------------------------- */

/* Wraps an already-open, seekable stream (file, memory, CHD, custom)
 * holding a single track's data and returns a track handle for use with
 * cdfs_open_file.  The track takes ownership of the stream: it is
 * closed and freed by cdfs_close_track, and also on failure of this
 * call (except when stream is NULL, in which case NULL is returned and
 * there is nothing to own).
 *
 * first_sector_offset is the byte offset of the track's first sector
 * within the stream; first_sector_index is that sector's absolute
 * index on the disc (both 0 for a plain single-track image).
 *
 * The sector layout is probed from the ISO-9660 volume descriptor at
 * sector 16, assuming raw 2352-byte sectors.  If the probe fails
 * (e.g. a cooked 2048-byte image), stream_sector_size is left 0 and
 * the caller must fill in stream_sector_size / stream_sector_header_size
 * itself (the path adapters below derive them from the file size or
 * the cue sheet). */
cdfs_track_t* cdfs_track_from_stream(
      intfstream_t* stream,
      unsigned first_sector_offset,
      unsigned first_sector_index);

/* Result of parsing one track out of a cue sheet held in memory. */
typedef struct cdfs_cue_info
{
   /* Name from the FILE line, verbatim (usually relative to the cue's
    * directory - resolution against the cue path is the caller's job). */
   char bin_name[PATH_MAX_LENGTH];
   unsigned track_index;          /* track number actually selected      */
   unsigned sector_size;          /* from MODEx/nnnn, 0 if not stated    */
   unsigned first_sector_offset;  /* byte offset of INDEX 01 in the bin  */
   unsigned first_sector_index;   /* absolute sector index of INDEX 01   */
} cdfs_cue_info_t;

/* Parses a cue sheet from a memory buffer (need not be NUL-terminated)
 * and locates data track @track_index (0 = first data track).  Performs
 * no I/O and no path resolution.  Returns true and fills *out when the
 * track is found; the caller then opens out->bin_name (resolved against
 * the cue's location) and passes the stream to cdfs_track_from_stream
 * with out->first_sector_offset / out->first_sector_index.
 *
 * NOTE: first_sector_index is only valid when all tracks live in the
 * same BIN file; a cue sheet does not store how many sectors earlier
 * BIN files contain.  This affects cdfs_get_first_sector only. */
bool cdfs_parse_cue(const char *cue_text, size_t len,
      unsigned track_index, cdfs_cue_info_t *out);

/* --- Path adapters: open by filename, then wrap ------------------------ */

/* opens the specified track in a CD or virtual CD file - the resulting stream should be passed to
 * cdfs_open_file to get access to a file within the CD.
 *
 * supported files:
 *   real CD - path will be in the form "cdrom://drive1.cue" or "cdrom://d:/drive.cue"
 *   bin/cue - path will point to the cue file
 *   chd     - path will point to the chd file
 *
 * for bin/cue files, the following storage modes are supported:
 *   MODE2/2352
 *   MODE1/2352
 *   MODE1/2048 - untested
 *   MODE2/2336 - untested
 */
cdfs_track_t* cdfs_open_track(const char* path, unsigned int track_index);

/* opens the first data track in a CD or virtual CD file. see cdfs_open_track for supported file formats
 */
cdfs_track_t* cdfs_open_data_track(const char* path);

/* opens a raw track file for a CD or virtual CD.
 *
 * supported files:
 *   real CD - path will be in the form "cdrom://drive1-track01.bin" or "cdrom://d:/drive-track01.bin"
 *             NOTE: cue file for CD must be opened first to populate vfs_cdrom_toc.
 *   bin     - path will point to the bin file
 *   iso     - path will point to the iso file
 */
cdfs_track_t* cdfs_open_raw_track(const char* path);

/* closes the CD or virtual CD track and frees the associated memory */
void cdfs_close_track(cdfs_track_t* track);

RETRO_END_DECLS

#endif /* __RARCH_CDFS_H */
