/* Copyright  (C) 2010-2020 The RetroArch team
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

typedef struct cdrom_toc
{
   cdrom_track_t track[99];         /* unsigned alignment */
   cdrom_group_timeouts_t timeouts; /* unsigned short alignment */
   unsigned char num_tracks;
   char drive;
} cdrom_toc_t;

/* Legacy helpers use absolute MSF frames (device LBA + 150). */
void cdrom_lba_to_msf(unsigned lba, unsigned char *min, unsigned char *sec, unsigned char *frame);

unsigned cdrom_msf_to_lba(unsigned char min, unsigned char sec, unsigned char frame);

void increment_msf(unsigned char *min, unsigned char *sec, unsigned char *frame);

int cdrom_read_subq(libretro_vfs_implementation_file *stream, unsigned char *s, size_t len);

int cdrom_write_cue(libretro_vfs_implementation_file *stream, char **out_buf, size_t *out_len, char cdrom_drive, unsigned char *num_tracks, cdrom_toc_t *toc);

/* needs 32 bytes for full vendor, product and version */
int cdrom_get_inquiry(libretro_vfs_implementation_file *stream, char *s, size_t len, bool *is_cdrom);

/* Returns 0 only for a complete read. On error the destination may be
 * partly filled and must be discarded; VFS leaves the position unchanged. */
int cdrom_read(libretro_vfs_implementation_file *stream, cdrom_group_timeouts_t *timeouts, unsigned char min, unsigned char sec, unsigned char frame, void *s, size_t len, size_t skip);

/* Cooked optical-disc access (DVD uses 2048-byte logical blocks).
 * Supported transports: Linux SG_IO and Windows SCSI pass-through.
 * Capacity is a block count, including the last LBA.  Returns 0 on
 * success, 1 on error/unsupported transport; outputs stay zero on error.
 * READ CAPACITY (10)'s saturated capacity is rejected, not truncated. */
/* MMC current media profile, zeroed on error. DVD profiles are 0x10-0x17,
 * 0x1a, 0x1b, 0x2a and 0x2b; CD profiles are 0x08-0x0a. */
int cdrom_get_media_profile(libretro_vfs_implementation_file *stream,
      uint16_t *profile);

int cdrom_get_capacity(libretro_vfs_implementation_file *stream,
      uint64_t *blocks, uint32_t *block_size);

/* Read count DVD blocks from a 32-bit LBA.  The caller supplies count*2048
 * writable bytes.  Aligned buffers go directly to the device; other buffers
 * use a bounded bounce buffer.  Returns 0 only after the complete transfer.
 * On failure the destination may contain a partially completed transfer.
 * Does not retry or sleep: recovery belongs to the off-thread caller. */
int cdrom_read_blocks(libretro_vfs_implementation_file *stream,
      uint32_t lba, uint32_t count, void *s);

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

/* Metadata/control handle: no track or filesystem data is exposed. */
size_t cdrom_device_filldevicepath(char *s, size_t len, char drive);

/* Build a cooked-image URI for a Linux/Windows drive. Returns required
 * length excluding NUL, or zero for invalid/unsupported arguments. */
size_t cdrom_device_fillimagepath(char *s, size_t len, char drive);

/* Build a cue or track URI for a drive. Same contract as
 * cdrom_device_fillimagepath: output is always NUL-terminated and the
 * required length excluding NUL is returned, so a result >= len means
 * the path was truncated. */
size_t cdrom_device_fillpath(char *s, size_t len, char drive, unsigned char track, bool is_cue);

RETRO_END_DECLS

#endif
