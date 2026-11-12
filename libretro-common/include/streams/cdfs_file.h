/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdfs_file.h).
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


#ifndef __LIBRETRO_SDK_STREAMS_CDFS_FILE_H__
#define __LIBRETRO_SDK_STREAMS_CDFS_FILE_H__

#include <retro_common_api.h>
#include <formats/cdfs.h>

RETRO_BEGIN_DECLS

/* cdfs tracks opened by filename: the path is resolved and the file
 * opened through intfstream, then handed to cdfs_track_from_stream.
 * Close the track with cdfs_close_track. */

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
 *             BIN handles obtain their own validated geometry.
 *   bin     - path will point to the bin file
 *   iso     - path will point to the iso file
 */
cdfs_track_t* cdfs_open_raw_track(const char* path);

RETRO_END_DECLS

#endif
