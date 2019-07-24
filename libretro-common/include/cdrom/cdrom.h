/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdrom.h).
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

#ifndef __LIBRETRO_SDK_CDROM_H
#define __LIBRETRO_SDK_CDROM_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <vfs/vfs.h>
#include <libretro.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include <boolean.h>

struct string_list;

RETRO_BEGIN_DECLS

typedef struct
{
   unsigned short g1_timeout;
   unsigned short g2_timeout;
   unsigned short g3_timeout;
} cdrom_group_timeouts_t;

typedef struct
{
   unsigned lba_start; /* start of pregap */
   unsigned lba; /* start of data */
   unsigned track_size; /* in LBAs */
   unsigned track_bytes;
   unsigned char track_num;
   unsigned char min; /* start of data */
   unsigned char sec;
   unsigned char frame;
   unsigned char mode;
   bool audio;
} cdrom_track_t;

typedef struct
{
   char drive;
   unsigned char num_tracks;
   cdrom_group_timeouts_t timeouts;
   cdrom_track_t track[99];
} cdrom_toc_t;

void cdrom_lba_to_msf(unsigned lba, unsigned char *min, unsigned char *sec, unsigned char *frame);

unsigned cdrom_msf_to_lba(unsigned char min, unsigned char sec, unsigned char frame);

void increment_msf(unsigned char *min, unsigned char *sec, unsigned char *frame);

int cdrom_read_subq(libretro_vfs_implementation_file *stream, unsigned char *buf, size_t len);

int cdrom_write_cue(libretro_vfs_implementation_file *stream, char **out_buf, size_t *out_len, char cdrom_drive, unsigned char *num_tracks, cdrom_toc_t *toc);

/* needs 32 bytes for full vendor, product and version */
int cdrom_get_inquiry(libretro_vfs_implementation_file *stream, char *model, int len, bool *is_cdrom);

int cdrom_read(libretro_vfs_implementation_file *stream, cdrom_group_timeouts_t *timeouts, unsigned char min, unsigned char sec, unsigned char frame, void *s, size_t len, size_t skip);

int cdrom_set_read_speed(libretro_vfs_implementation_file *stream, unsigned speed);

int cdrom_stop(libretro_vfs_implementation_file *stream);

int cdrom_unlock(libretro_vfs_implementation_file *stream);

int cdrom_open_tray(libretro_vfs_implementation_file *stream);

int cdrom_close_tray(libretro_vfs_implementation_file *stream);

/* must be freed by the caller */
struct string_list* cdrom_get_available_drives(void);

bool cdrom_is_media_inserted(libretro_vfs_implementation_file *stream);

bool cdrom_drive_has_media(const char drive);

void cdrom_get_current_config_core(libretro_vfs_implementation_file *stream);

void cdrom_get_current_config_profiles(libretro_vfs_implementation_file *stream);

void cdrom_get_current_config_cdread(libretro_vfs_implementation_file *stream);

void cdrom_get_current_config_multiread(libretro_vfs_implementation_file *stream);

void cdrom_get_current_config_random_readable(libretro_vfs_implementation_file *stream);

int cdrom_get_sense(libretro_vfs_implementation_file *stream, unsigned char *sense, size_t len);

bool cdrom_set_read_cache(libretro_vfs_implementation_file *stream, bool enabled);

bool cdrom_get_timeouts(libretro_vfs_implementation_file *stream, cdrom_group_timeouts_t *timeouts);

bool cdrom_has_atip(libretro_vfs_implementation_file *stream);

void cdrom_device_fillpath(char *path, size_t len, char drive, unsigned char track, bool is_cue);

RETRO_END_DECLS

#endif
