/* pcm.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2017      - Charlton Head
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#include <linux/ioctl.h>
#include <linux/types.h>

#include <retro_inline.h>
#include <retro_endianness.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

/* Implementation tinyalsa pcm */

/** A flag that specifies that the PCM is an output.
 * May not be bitwise AND'd with @ref PCM_IN.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_OUT 0x00000000

/** Specifies that the PCM is an input.
 * May not be bitwise AND'd with @ref PCM_OUT.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_IN 0x10000000

/** Specifies that the PCM will use mmap read and write methods.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_MMAP 0x00000001

/** Specifies no interrupt requests.
 * May only be bitwise AND'd with @ref PCM_MMAP.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_NOIRQ 0x00000002

/** When set, calls to @ref pcm_write
 * for a playback stream will not attempt
 * to restart the stream in the case of an
 * underflow, but will return -EPIPE instead.
 * After the first -EPIPE error, the stream
 * is considered to be stopped, and a second
 * call to pcm_write will attempt to restart
 * the stream.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_NORESTART 0x00000004

/** Specifies monotonic timestamps.
 * Used in @ref pcm_open.
 * @ingroup libtinyalsa-pcm
 */
#define PCM_MONOTONIC 0x00000008

/** For inputs, this means the PCM is recording audio samples.
 * For outputs, this means the PCM is playing audio samples.
 * @ingroup libtinyalsa-pcm
 */
#define	PCM_STATE_RUNNING	0x03

/** For inputs, this means an overrun occured.
 * For outputs, this means an underrun occured.
 */
#define	PCM_STATE_XRUN 0x04

/** For outputs, this means audio samples are played.
 * A PCM is in a draining state when it is coming to a stop.
 */
#define	PCM_STATE_DRAINING 0x05

/** Means a PCM is suspended.
 * @ingroup libtinyalsa-pcm
 */
#define	PCM_STATE_SUSPENDED 0x07

/** Means a PCM has been disconnected.
 * @ingroup libtinyalsa-pcm
 */
#define	PCM_STATE_DISCONNECTED 0x08

#define SNDRV_CHMAP_POSITION_MASK	0xffff
#define SNDRV_CHMAP_PHASE_INVERSE	(0x01 << 16)
#define SNDRV_CHMAP_DRIVER_SPEC		(0x02 << 16)
#define SNDRV_PCM_IOCTL_PVERSION	_IOR('A', 0x00, int)
#define SNDRV_PCM_IOCTL_INFO		_IOR('A', 0x01, struct snd_pcm_info)
#define SNDRV_PCM_IOCTL_TSTAMP		_IOW('A', 0x02, int)
#define SNDRV_PCM_IOCTL_TTSTAMP		_IOW('A', 0x03, int)
#define SNDRV_PCM_IOCTL_HW_REFINE	_IOWR('A', 0x10, struct snd_pcm_hw_params)
#define SNDRV_PCM_IOCTL_HW_PARAMS	_IOWR('A', 0x11, struct snd_pcm_hw_params)
#define SNDRV_PCM_IOCTL_HW_FREE		_IO('A', 0x12)
#define SNDRV_PCM_IOCTL_SW_PARAMS	_IOWR('A', 0x13, struct snd_pcm_sw_params)
#define SNDRV_PCM_IOCTL_STATUS		_IOR('A', 0x20, struct snd_pcm_status)
#define SNDRV_PCM_IOCTL_DELAY		_IOR('A', 0x21, snd_pcm_sframes_t)
#define SNDRV_PCM_IOCTL_HWSYNC		_IO('A', 0x22)
#define SNDRV_PCM_IOCTL_SYNC_PTR	_IOWR('A', 0x23, struct snd_pcm_sync_ptr)
#define SNDRV_PCM_IOCTL_CHANNEL_INFO	_IOR('A', 0x32, struct snd_pcm_channel_info)
#define SNDRV_PCM_IOCTL_PREPARE		_IO('A', 0x40)
#define SNDRV_PCM_IOCTL_RESET		_IO('A', 0x41)
#define SNDRV_PCM_IOCTL_START		_IO('A', 0x42)
#define SNDRV_PCM_IOCTL_DROP		_IO('A', 0x43)
#define SNDRV_PCM_IOCTL_DRAIN		_IO('A', 0x44)
#define SNDRV_PCM_IOCTL_PAUSE		_IOW('A', 0x45, int)
#define SNDRV_PCM_IOCTL_REWIND		_IOW('A', 0x46, snd_pcm_uframes_t)
#define SNDRV_PCM_IOCTL_RESUME		_IO('A', 0x47)
#define SNDRV_PCM_IOCTL_XRUN		_IO('A', 0x48)
#define SNDRV_PCM_IOCTL_FORWARD		_IOW('A', 0x49, snd_pcm_uframes_t)
#define SNDRV_PCM_IOCTL_WRITEI_FRAMES	_IOW('A', 0x50, struct snd_xferi)
#define SNDRV_PCM_IOCTL_READI_FRAMES	_IOR('A', 0x51, struct snd_xferi)
#define SNDRV_PCM_IOCTL_WRITEN_FRAMES	_IOW('A', 0x52, struct snd_xfern)
#define SNDRV_PCM_IOCTL_READN_FRAMES	_IOR('A', 0x53, struct snd_xfern)
#define SNDRV_PCM_IOCTL_LINK		_IOW('A', 0x60, int)
#define SNDRV_PCM_IOCTL_UNLINK		_IO('A', 0x61)

#define	SNDRV_PCM_ACCESS_MMAP_INTERLEAVED	(( tinyalsa_snd_pcm_access_t) 0) /* interleaved mmap */
#define	SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED	(( tinyalsa_snd_pcm_access_t) 1) /* noninterleaved mmap */
#define	SNDRV_PCM_ACCESS_MMAP_COMPLEX		(( tinyalsa_snd_pcm_access_t) 2) /* complex mmap */
#define	SNDRV_PCM_ACCESS_RW_INTERLEAVED		(( tinyalsa_snd_pcm_access_t) 3) /* readi/writei */
#define	SNDRV_PCM_ACCESS_RW_NONINTERLEAVED	(( tinyalsa_snd_pcm_access_t) 4) /* readn/writen */
#define	SNDRV_PCM_ACCESS_LAST		SNDRV_PCM_ACCESS_RW_NONINTERLEAVED

#define	SNDRV_PCM_SUBFORMAT_STD		(( tinyalsa_snd_pcm_subformat_t) 0)
#define	SNDRV_PCM_SUBFORMAT_LAST	SNDRV_PCM_SUBFORMAT_STD

#define	SNDRV_PCM_SUBFORMAT_STD		(( tinyalsa_snd_pcm_subformat_t) 0)
#define	SNDRV_PCM_SUBFORMAT_LAST	SNDRV_PCM_SUBFORMAT_STD
#define SNDRV_PCM_INFO_MMAP		0x00000001	/* hardware supports mmap */
#define SNDRV_PCM_INFO_MMAP_VALID	0x00000002	/* period data are valid during transfer */
#define SNDRV_PCM_INFO_DOUBLE		0x00000004	/* Double buffering needed for PCM start/stop */
#define SNDRV_PCM_INFO_BATCH		0x00000010	/* double buffering */
#define SNDRV_PCM_INFO_INTERLEAVED	0x00000100	/* channels are interleaved */
#define SNDRV_PCM_INFO_NONINTERLEAVED	0x00000200	/* channels are not interleaved */
#define SNDRV_PCM_INFO_COMPLEX		0x00000400	/* complex frame organization (mmap only) */
#define SNDRV_PCM_INFO_BLOCK_TRANSFER	0x00010000	/* hardware transfer block of samples */
#define SNDRV_PCM_INFO_OVERRANGE	0x00020000	/* hardware supports ADC (capture) overrange detection */
#define SNDRV_PCM_INFO_RESUME		0x00040000	/* hardware supports stream resume after suspend */
#define SNDRV_PCM_INFO_PAUSE		0x00080000	/* pause ioctl is supported */
#define SNDRV_PCM_INFO_HALF_DUPLEX	0x00100000	/* only half duplex */
#define SNDRV_PCM_INFO_JOINT_DUPLEX	0x00200000	/* playback and capture stream are somewhat correlated */
#define SNDRV_PCM_INFO_SYNC_START	0x00400000	/* pcm support some kind of sync go */
#define SNDRV_PCM_INFO_NO_PERIOD_WAKEUP	0x00800000	/* period wakeup can be disabled */
#define SNDRV_PCM_INFO_HAS_WALL_CLOCK   0x01000000      /* has audio wall clock for audio/system time sync */
#define SNDRV_PCM_INFO_FIFO_IN_FRAMES	0x80000000	/* internal kernel flag - FIFO size is in frames */
#define	SNDRV_PCM_STATE_OPEN		(( tinyalsa_snd_pcm_state_t) 0) /* stream is open */
#define	SNDRV_PCM_STATE_SETUP		(( tinyalsa_snd_pcm_state_t) 1) /* stream has a setup */
#define	SNDRV_PCM_STATE_PREPARED	(( tinyalsa_snd_pcm_state_t) 2) /* stream is ready to start */
#define	SNDRV_PCM_STATE_RUNNING		(( tinyalsa_snd_pcm_state_t) 3) /* stream is running */
#define	SNDRV_PCM_STATE_XRUN		(( tinyalsa_snd_pcm_state_t) 4) /* stream reached an xrun */
#define	SNDRV_PCM_STATE_DRAINING	(( tinyalsa_snd_pcm_state_t) 5) /* stream is draining */
#define	SNDRV_PCM_STATE_PAUSED		(( tinyalsa_snd_pcm_state_t) 6) /* stream is paused */
#define	SNDRV_PCM_STATE_SUSPENDED	(( tinyalsa_snd_pcm_state_t) 7) /* hardware is suspended */
#define	SNDRV_PCM_STATE_DISCONNECTED	(( tinyalsa_snd_pcm_state_t) 8) /* hardware is disconnected */
#define	SNDRV_PCM_STATE_LAST		SNDRV_PCM_STATE_DISCONNECTED

#define	SNDRV_PCM_HW_PARAM_ACCESS	         0	/* Access type */
#define	SNDRV_PCM_HW_PARAM_FORMAT	         1	/* Format */
#define	SNDRV_PCM_HW_PARAM_SUBFORMAT	      2	/* Subformat */
#define	SNDRV_PCM_HW_PARAM_FIRST_MASK	      SNDRV_PCM_HW_PARAM_ACCESS
#define	SNDRV_PCM_HW_PARAM_LAST_MASK	      SNDRV_PCM_HW_PARAM_SUBFORMAT
#define	SNDRV_PCM_HW_PARAM_SAMPLE_BITS	   8	/* Bits per sample */
#define	SNDRV_PCM_HW_PARAM_FRAME_BITS	      9	/* Bits per frame */
#define	SNDRV_PCM_HW_PARAM_CHANNELS	      10	/* Channels */
#define	SNDRV_PCM_HW_PARAM_RATE		         11	/* Approx rate */
#define	SNDRV_PCM_HW_PARAM_PERIOD_TIME	   12	/* Approx distance between interrupts in us */
#define	SNDRV_PCM_HW_PARAM_PERIOD_SIZE	   13	/* Approx frames between interrupts */
#define	SNDRV_PCM_HW_PARAM_PERIOD_BYTES	   14	/* Approx bytes between interrupts */
#define	SNDRV_PCM_HW_PARAM_PERIODS	         15	/* Approx interrupts per buffer */
#define	SNDRV_PCM_HW_PARAM_BUFFER_TIME	   16	/* Approx duration of buffer in us */
#define	SNDRV_PCM_HW_PARAM_BUFFER_SIZE	   17	/* Size of buffer in frames */
#define	SNDRV_PCM_HW_PARAM_BUFFER_BYTES	   18	/* Size of buffer in bytes */
#define	SNDRV_PCM_HW_PARAM_TICK_TIME	      19	/* Approx tick duration in us */
#define	SNDRV_PCM_HW_PARAM_FIRST_INTERVAL	SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define	SNDRV_PCM_HW_PARAM_LAST_INTERVAL	   SNDRV_PCM_HW_PARAM_TICK_TIME
#define SNDRV_PCM_HW_PARAMS_NORESAMPLE	      (1<<0)	/* avoid rate resampling */
#define SNDRV_PCM_HW_PARAMS_EXPORT_BUFFER	   (1<<1)	/* export buffer */
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP	(1<<2)	/* disable period wakeups */

#define	SNDRV_PCM_FORMAT_S8	(( tinyalsa_snd_pcm_format_t) 0)
#define	SNDRV_PCM_FORMAT_U8	(( tinyalsa_snd_pcm_format_t) 1)
#define	SNDRV_PCM_FORMAT_S16_LE	(( tinyalsa_snd_pcm_format_t) 2)
#define	SNDRV_PCM_FORMAT_S16_BE	(( tinyalsa_snd_pcm_format_t) 3)
#define	SNDRV_PCM_FORMAT_U16_LE	(( tinyalsa_snd_pcm_format_t) 4)
#define	SNDRV_PCM_FORMAT_U16_BE	(( tinyalsa_snd_pcm_format_t) 5)
#define	SNDRV_PCM_FORMAT_S24_LE	(( tinyalsa_snd_pcm_format_t) 6) /* low three bytes */
#define	SNDRV_PCM_FORMAT_S24_BE	(( tinyalsa_snd_pcm_format_t) 7) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_LE	(( tinyalsa_snd_pcm_format_t) 8) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_BE	(( tinyalsa_snd_pcm_format_t) 9) /* low three bytes */
#define	SNDRV_PCM_FORMAT_S32_LE	(( tinyalsa_snd_pcm_format_t) 10)
#define	SNDRV_PCM_FORMAT_S32_BE	(( tinyalsa_snd_pcm_format_t) 11)
#define	SNDRV_PCM_FORMAT_U32_LE	(( tinyalsa_snd_pcm_format_t) 12)
#define	SNDRV_PCM_FORMAT_U32_BE	(( tinyalsa_snd_pcm_format_t) 13)
#define	SNDRV_PCM_FORMAT_FLOAT_LE	(( tinyalsa_snd_pcm_format_t) 14) /* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT_BE	(( tinyalsa_snd_pcm_format_t) 15) /* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT64_LE	(( tinyalsa_snd_pcm_format_t) 16) /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT64_BE	(( tinyalsa_snd_pcm_format_t) 17) /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE (( tinyalsa_snd_pcm_format_t) 18) /* IEC-958 subframe, Little Endian */
#define	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE (( tinyalsa_snd_pcm_format_t) 19) /* IEC-958 subframe, Big Endian */
#define	SNDRV_PCM_FORMAT_MU_LAW		(( tinyalsa_snd_pcm_format_t) 20)
#define	SNDRV_PCM_FORMAT_A_LAW		(( tinyalsa_snd_pcm_format_t) 21)
#define	SNDRV_PCM_FORMAT_IMA_ADPCM	(( tinyalsa_snd_pcm_format_t) 22)
#define	SNDRV_PCM_FORMAT_MPEG		(( tinyalsa_snd_pcm_format_t) 23)
#define	SNDRV_PCM_FORMAT_GSM		(( tinyalsa_snd_pcm_format_t) 24)
#define	SNDRV_PCM_FORMAT_SPECIAL	(( tinyalsa_snd_pcm_format_t) 31)
#define	SNDRV_PCM_FORMAT_S24_3LE	(( tinyalsa_snd_pcm_format_t) 32)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S24_3BE	(( tinyalsa_snd_pcm_format_t) 33)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U24_3LE	(( tinyalsa_snd_pcm_format_t) 34)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U24_3BE	(( tinyalsa_snd_pcm_format_t) 35)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S20_3LE	(( tinyalsa_snd_pcm_format_t) 36)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S20_3BE	(( tinyalsa_snd_pcm_format_t) 37)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U20_3LE	(( tinyalsa_snd_pcm_format_t) 38)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U20_3BE	(( tinyalsa_snd_pcm_format_t) 39)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S18_3LE	(( tinyalsa_snd_pcm_format_t) 40)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S18_3BE	(( tinyalsa_snd_pcm_format_t) 41)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U18_3LE	(( tinyalsa_snd_pcm_format_t) 42)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U18_3BE	(( tinyalsa_snd_pcm_format_t) 43)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_G723_24	(( tinyalsa_snd_pcm_format_t) 44) /* 8 samples in 3 bytes */
#define	SNDRV_PCM_FORMAT_G723_24_1B	(( tinyalsa_snd_pcm_format_t) 45) /* 1 sample in 1 byte */
#define	SNDRV_PCM_FORMAT_G723_40	(( tinyalsa_snd_pcm_format_t) 46) /* 8 Samples in 5 bytes */
#define	SNDRV_PCM_FORMAT_G723_40_1B	(( tinyalsa_snd_pcm_format_t) 47) /* 1 sample in 1 byte */
#define	SNDRV_PCM_FORMAT_DSD_U8		(( tinyalsa_snd_pcm_format_t) 48) /* DSD, 1-byte samples DSD (x8) */
#define	SNDRV_PCM_FORMAT_DSD_U16_LE	(( tinyalsa_snd_pcm_format_t) 49) /* DSD, 2-byte samples DSD (x16), little endian */
#define	SNDRV_PCM_FORMAT_DSD_U32_LE	(( tinyalsa_snd_pcm_format_t) 50) /* DSD, 4-byte samples DSD (x32), little endian */
#define	SNDRV_PCM_FORMAT_DSD_U16_BE	(( tinyalsa_snd_pcm_format_t) 51) /* DSD, 2-byte samples DSD (x16), big endian */
#define	SNDRV_PCM_FORMAT_DSD_U32_BE	(( tinyalsa_snd_pcm_format_t) 52) /* DSD, 4-byte samples DSD (x32), big endian */
#define	SNDRV_PCM_FORMAT_LAST		SNDRV_PCM_FORMAT_DSD_U32_BE

#define SNDRV_MASK_MAX	               256

#define SNDRV_PCM_SYNC_PTR_HWSYNC	   (1<<0)	/* execute hwsync */
#define SNDRV_PCM_SYNC_PTR_APPL		   (1<<1)	/* get appl_ptr from driver (r/w op) */
#define SNDRV_PCM_SYNC_PTR_AVAIL_MIN	(1<<2)	/* get avail_min from driver */

#define SNDRV_PCM_MMAP_OFFSET_DATA     0x00000000
#define SNDRV_PCM_MMAP_OFFSET_STATUS   0x80000000
#define SNDRV_PCM_MMAP_OFFSET_CONTROL  0x81000000

/** Audio sample format of a PCM.
 * The first letter specifiers whether the sample is signed or unsigned.
 * The letter 'S' means signed. The letter 'U' means unsigned.
 * The following number is the amount of bits that the sample occupies in memory.
 * Following the underscore, specifiers whether the sample is big endian or little endian.
 * The letters 'LE' mean little endian.
 * The letters 'BE' mean big endian.
 * This enumeration is used in the @ref pcm_config structure.
 * @ingroup libtinyalsa-pcm
 */
enum pcm_format
{
   /** Signed, 8-bit */
   PCM_FORMAT_S8 = 1,
   /** Signed 16-bit, little endian */
   PCM_FORMAT_S16_LE = 0,
   /** Signed, 16-bit, big endian */
   PCM_FORMAT_S16_BE = 2,
   /** Signed, 24-bit (32-bit in memory), little endian */
   PCM_FORMAT_S24_LE,
   /** Signed, 24-bit (32-bit in memory), big endian */
   PCM_FORMAT_S24_BE,
   /** Signed, 24-bit, little endian */
   PCM_FORMAT_S24_3LE,
   /** Signed, 24-bit, big endian */
   PCM_FORMAT_S24_3BE,
   /** Signed, 32-bit, little endian */
   PCM_FORMAT_S32_LE,
   /** Signed, 32-bit, big endian */
   PCM_FORMAT_S32_BE,
   /** Max of the enumeration list, not an actual format. */
   PCM_FORMAT_MAX
};

enum
{
	SNDRV_PCM_TSTAMP_NONE = 0,
	SNDRV_PCM_TSTAMP_ENABLE,
	SNDRV_PCM_TSTAMP_LAST = SNDRV_PCM_TSTAMP_ENABLE
};

/** Enumeration of a PCM's hardware parameters.
 * Each of these parameters is either a mask or an interval.
 * @ingroup libtinyalsa-pcm
 */
enum pcm_param
{
   /** A mask that represents the type of read or write method available (e.g. interleaved, mmap). */
   PCM_PARAM_ACCESS,
   /** A mask that represents the @ref pcm_format available (e.g. @ref PCM_FORMAT_S32_LE) */
   PCM_PARAM_FORMAT,
   /** A mask that represents the subformat available */
   PCM_PARAM_SUBFORMAT,
   /** An interval representing the range of sample bits available (e.g. 8 to 32) */
   PCM_PARAM_SAMPLE_BITS,
   /** An interval representing the range of frame bits available (e.g. 8 to 64) */
   PCM_PARAM_FRAME_BITS,
   /** An interval representing the range of channels available (e.g. 1 to 2) */
   PCM_PARAM_CHANNELS,
   /** An interval representing the range of rates available (e.g. 44100 to 192000) */
   PCM_PARAM_RATE,
   PCM_PARAM_PERIOD_TIME,
   /** The number of frames in a period */
   PCM_PARAM_PERIOD_SIZE,
   /** The number of bytes in a period */
   PCM_PARAM_PERIOD_BYTES,
   /** The number of periods for a PCM */
   PCM_PARAM_PERIODS,
   PCM_PARAM_BUFFER_TIME,
   PCM_PARAM_BUFFER_SIZE,
   PCM_PARAM_BUFFER_BYTES,
   PCM_PARAM_TICK_TIME
}; /* enum pcm_param */

/* channel positions */
enum
{
	SNDRV_CHMAP_UNKNOWN = 0,
	SNDRV_CHMAP_NA,		/* N/A, silent */
	SNDRV_CHMAP_MONO,	/* mono stream */
	/* this follows the alsa-lib mixer channel value + 3 */
	SNDRV_CHMAP_FL,		/* front left */
	SNDRV_CHMAP_FR,		/* front right */
	SNDRV_CHMAP_RL,		/* rear left */
	SNDRV_CHMAP_RR,		/* rear right */
	SNDRV_CHMAP_FC,		/* front center */
	SNDRV_CHMAP_LFE,	/* LFE */
	SNDRV_CHMAP_SL,		/* side left */
	SNDRV_CHMAP_SR,		/* side right */
	SNDRV_CHMAP_RC,		/* rear center */
	/* new definitions */
	SNDRV_CHMAP_FLC,	/* front left center */
	SNDRV_CHMAP_FRC,	/* front right center */
	SNDRV_CHMAP_RLC,	/* rear left center */
	SNDRV_CHMAP_RRC,	/* rear right center */
	SNDRV_CHMAP_FLW,	/* front left wide */
	SNDRV_CHMAP_FRW,	/* front right wide */
	SNDRV_CHMAP_FLH,	/* front left high */
	SNDRV_CHMAP_FCH,	/* front center high */
	SNDRV_CHMAP_FRH,	/* front right high */
	SNDRV_CHMAP_TC,		/* top center */
	SNDRV_CHMAP_TFL,	/* top front left */
	SNDRV_CHMAP_TFR,	/* top front right */
	SNDRV_CHMAP_TFC,	/* top front center */
	SNDRV_CHMAP_TRL,	/* top rear left */
	SNDRV_CHMAP_TRR,	/* top rear right */
	SNDRV_CHMAP_TRC,	/* top rear center */
	/* new definitions for UAC2 */
	SNDRV_CHMAP_TFLC,	/* top front left center */
	SNDRV_CHMAP_TFRC,	/* top front right center */
	SNDRV_CHMAP_TSL,	/* top side left */
	SNDRV_CHMAP_TSR,	/* top side right */
	SNDRV_CHMAP_LLFE,	/* left LFE */
	SNDRV_CHMAP_RLFE,	/* right LFE */
	SNDRV_CHMAP_BC,		/* bottom center */
	SNDRV_CHMAP_BLC,	/* bottom left center */
	SNDRV_CHMAP_BRC,	/* bottom right center */
	SNDRV_CHMAP_LAST = SNDRV_CHMAP_BRC
};

enum
{
   SNDRV_PCM_TSTAMP_TYPE_GETTIMEOFDAY = 0,	/* gettimeofday equivalent */
   SNDRV_PCM_TSTAMP_TYPE_MONOTONIC,	         /* posix_clock_monotonic equivalent */
   SNDRV_PCM_TSTAMP_TYPE_MONOTONIC_RAW,      /* monotonic_raw (no NTP) */
   SNDRV_PCM_TSTAMP_TYPE_LAST = SNDRV_PCM_TSTAMP_TYPE_MONOTONIC_RAW
};

typedef unsigned long snd_pcm_uframes_t;
typedef signed long snd_pcm_sframes_t;
typedef int snd_pcm_hw_param_t;
typedef int __bitwise tinyalsa_snd_pcm_access_t;
typedef int __bitwise tinyalsa_snd_pcm_subformat_t;
typedef int __bitwise tinyalsa_snd_pcm_state_t;
typedef int __bitwise tinyalsa_snd_pcm_format_t;

/** A bit mask of 256 bits (32 bytes) that describes some hardware parameters of a PCM */
struct pcm_mask
{
    /** bits of the bit mask */
    unsigned int bits[32 / sizeof(unsigned int)];
};

union snd_pcm_sync_id
{
	unsigned char id[16];
	unsigned short id16[8];
	unsigned int id32[4];
};

struct snd_pcm_mmap_status
{
	tinyalsa_snd_pcm_state_t state;		/* RO: state - SNDRV_PCM_STATE_XXXX */
	int pad1;			/* Needed for 64 bit alignment */
	snd_pcm_uframes_t hw_ptr;	/* RO: hw ptr (0...boundary-1) */
	struct timespec tstamp;		/* Timestamp */
	tinyalsa_snd_pcm_state_t suspended_state; /* RO: suspended stream state */
	struct timespec audio_tstamp;	/* from sample counter or wall clock */
};

struct snd_pcm_info
{
	unsigned int device;		/* RO/WR (control): device number */
	unsigned int subdevice;		/* RO/WR (control): subdevice number */
	int stream;			/* RO/WR (control): stream direction */
	int card;			/* R: card number */
	unsigned char id[64];		/* ID (user selectable) */
	unsigned char name[80];		/* name of this device */
	unsigned char subname[32];	/* subdevice name */
	int dev_class;			/* SNDRV_PCM_CLASS_* */
	int dev_subclass;		/* SNDRV_PCM_SUBCLASS_* */
	unsigned int subdevices_count;
	unsigned int subdevices_avail;
	union snd_pcm_sync_id sync;	/* hardware synchronization ID */
	unsigned char reserved[64];	/* reserved for future... */
};

struct snd_interval
{
   unsigned int min, max;
   unsigned int openmin:1,
                openmax:1,
                integer:1,
                empty:1;
};

struct snd_mask
{
	__u32 bits[(SNDRV_MASK_MAX+31)/32];
};

struct snd_pcm_sw_params
{
	int tstamp_mode;			/* timestamp mode */
	unsigned int period_step;
	unsigned int sleep_min;			/* min ticks to sleep */
	snd_pcm_uframes_t avail_min;		/* min avail frames for wakeup */
	snd_pcm_uframes_t xfer_align;		/* obsolete: xfer size need to be a multiple */
	snd_pcm_uframes_t start_threshold;	/* min hw_avail frames for automatic start */
	snd_pcm_uframes_t stop_threshold;	/* min avail frames for automatic stop */
	snd_pcm_uframes_t silence_threshold;	/* min distance from noise for silence filling */
	snd_pcm_uframes_t silence_size;		/* silence block size */
	snd_pcm_uframes_t boundary;		/* pointers wrap point */
	unsigned int proto;			/* protocol version */
	unsigned int tstamp_type;		/* timestamp type (req. proto >= 2.0.12) */
	unsigned char reserved[56];		/* reserved for future */
};

struct snd_pcm_hw_params
{
   unsigned int flags;
   struct snd_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK -
      SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
   struct snd_mask mres[5];	/* reserved masks */
   struct snd_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
      SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
   struct snd_interval ires[9];	/* reserved intervals */
   unsigned int rmask;		/* W: requested masks */
   unsigned int cmask;		/* R: changed masks */
   unsigned int info;		/* R: Info flags for returned setup */
   unsigned int msbits;		/* R: used most significant bits */
   unsigned int rate_num;		/* R: rate numerator */
   unsigned int rate_den;		/* R: rate denominator */
   snd_pcm_uframes_t fifo_size;	/* R: chip FIFO size in frames */
   unsigned char reserved[64];	/* reserved for future */
};

/** Encapsulates the hardware and software parameters of a PCM.
 * @ingroup libtinyalsa-pcm
 */
struct pcm_config
{
   /** The number of channels in a frame */
   unsigned int channels;
   /** The number of frames per second */
   unsigned int rate;
   /** The number of frames in a period */
   unsigned int period_size;
   /** The number of periods in a PCM */
   unsigned int period_count;
   /** The sample format of a PCM */
   enum pcm_format format;
   /* Values to use for the ALSA start, stop and silence thresholds.  Setting
    * any one of these values to 0 will cause the default tinyalsa values to be
    * used instead.  Tinyalsa defaults are as follows.
    *
    * start_threshold   : period_count * period_size
    * stop_threshold    : period_count * period_size
    * silence_threshold : 0
    */
   /** The minimum number of frames required to start the PCM */
   unsigned int start_threshold;
   /** The minimum number of frames required to stop the PCM */
   unsigned int stop_threshold;
   /** The minimum number of frames to silence the PCM */
   unsigned int silence_threshold;
};

struct snd_pcm_mmap_control
{
	snd_pcm_uframes_t appl_ptr;	/* RW: appl ptr (0...boundary-1) */
	snd_pcm_uframes_t avail_min;	/* RW: min available frames for wakeup */
};

struct snd_pcm_sync_ptr
{
   unsigned int flags;
   union
   {
      struct snd_pcm_mmap_status status;
      unsigned char reserved[64];
   } s;
   union
   {
      struct snd_pcm_mmap_control control;
      unsigned char reserved[64];
   } c;
};

struct snd_xferi
{
   snd_pcm_sframes_t result;
   void  *buf;
   snd_pcm_uframes_t frames;
};

struct snd_xfern
{
   snd_pcm_sframes_t result;
   void  *  *bufs;
   snd_pcm_uframes_t frames;
};

struct snd_pcm_status
{
   tinyalsa_snd_pcm_state_t state;		/* stream state */
   struct timespec trigger_tstamp;	/* time when stream was started/stopped/paused */
   struct timespec tstamp;		/* reference timestamp */
   snd_pcm_uframes_t appl_ptr;	/* appl ptr */
   snd_pcm_uframes_t hw_ptr;	/* hw ptr */
   snd_pcm_sframes_t delay;	/* current delay in frames */
   snd_pcm_uframes_t avail;	/* number of frames available */
   snd_pcm_uframes_t avail_max;	/* max frames available on hw since last status */
   snd_pcm_uframes_t overrange;	/* count of ADC (capture) overrange detections from last status */
   tinyalsa_snd_pcm_state_t suspended_state; /* suspended stream state */
   __u32 reserved_alignment;	/* must be filled with zero */
   struct timespec audio_tstamp;	/* from sample counter or wall clock */
   unsigned char reserved[56-sizeof(struct timespec)]; /* must be filled with zero */
};

struct pcm_params;

#define TINYALSA_CHANNELS_MAX 32U
#define TINYALSA_CHANNELS_MIN 1U
#define TINYALSA_FRAMES_MAX (ULONG_MAX / (TINYALSA_CHANNELS_MAX * 4))

#define PARAM_MAX SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP (1<<2)

static INLINE int param_is_mask(int p)
{
   return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
      (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static INLINE int param_is_interval(int p)
{
   return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
      (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static INLINE const struct snd_interval *param_get_interval(const struct snd_pcm_hw_params *p, int n)
{
   return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static INLINE struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static INLINE struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned int bit)
{
    if (bit >= SNDRV_MASK_MAX)
        return;
    if (param_is_mask(n))
    {
        struct snd_mask *m = param_to_mask(p, n);
        m->bits[0] = 0;
        m->bits[1] = 0;
        m->bits[bit >> 5] |= (1 << (bit & 31));
    }
}

static void param_set_min(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n))
    {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
    }
}

static void param_set_int(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n))
    {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
        i->max = val;
        i->integer = 1;
    }
}

static unsigned int param_get_int(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n))
    {
        struct snd_interval *i = param_to_interval(p, n);
        if (i->integer)
            return i->max;
    }
    return 0;
}

static void param_init(struct snd_pcm_hw_params *p)
{
   int n;

   memset(p, 0, sizeof(*p));
   for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
   {
      struct snd_mask *m = param_to_mask(p, n);
      m->bits[0] = ~0;
      m->bits[1] = ~0;
   }
   for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++)
   {
      struct snd_interval *i = param_to_interval(p, n);
      i->min = 0;
      i->max = ~0;
   }
   p->rmask = ~0U;
   p->cmask = 0;
   p->info = ~0U;
}

static unsigned int pcm_format_to_alsa(enum pcm_format format)
{
   switch (format)
   {
      case PCM_FORMAT_S8:
         return SNDRV_PCM_FORMAT_S8;

      default:
      case PCM_FORMAT_S16_LE:
         return SNDRV_PCM_FORMAT_S16_LE;
      case PCM_FORMAT_S16_BE:
         return SNDRV_PCM_FORMAT_S16_BE;

      case PCM_FORMAT_S24_LE:
         return SNDRV_PCM_FORMAT_S24_LE;
      case PCM_FORMAT_S24_BE:
         return SNDRV_PCM_FORMAT_S24_BE;

      case PCM_FORMAT_S24_3LE:
         return SNDRV_PCM_FORMAT_S24_3LE;
      case PCM_FORMAT_S24_3BE:
         return SNDRV_PCM_FORMAT_S24_3BE;

      case PCM_FORMAT_S32_LE:
         return SNDRV_PCM_FORMAT_S32_LE;
      case PCM_FORMAT_S32_BE:
         return SNDRV_PCM_FORMAT_S32_BE;
   }
}

#define PCM_ERROR_MAX 128

/** A PCM handle.
 * @ingroup libtinyalsa-pcm
 */
struct pcm
{
   /** The PCM's file descriptor */
   int fd;
   /** Flags that were passed to @ref pcm_open */
   unsigned int flags;
   /** Whether the PCM is running or not */
   unsigned int running:1;
   /** Whether or not the PCM has been prepared */
   unsigned int prepared:1;
   /** The number of underruns that have occured */
   int underruns;
   /** Size of the buffer */
   unsigned int buffer_size;
   /** The boundary for ring buffer pointers */
   unsigned int boundary;
   /** Description of the last error that occured */
   char error[PCM_ERROR_MAX];
   /** Configuration that was passed to @ref pcm_open */
   struct pcm_config config;
   struct snd_pcm_mmap_status *mmap_status;
   struct snd_pcm_mmap_control *mmap_control;
   struct snd_pcm_sync_ptr *sync_ptr;
   void *mmap_buffer;
   unsigned int noirq_frames_per_msec;
   /** The delay of the PCM, in terms of frames */
   long pcm_delay;
   /** The subdevice corresponding to the PCM */
   unsigned int subdevice;
};

/** Gets the buffer size of the PCM.
 * @param pcm A PCM handle.
 * @return The buffer size of the PCM.
 * @ingroup libtinyalsa-pcm
 */
static unsigned int pcm_get_buffer_size(const struct pcm *pcm)
{
   return pcm->buffer_size;
}

#if 0
/* Unused for now */

/** Gets the channel count of the PCM.
 * @param pcm A PCM handle.
 * @return The channel count of the PCM.
 * @ingroup libtinyalsa-pcm
 */
static unsigned int pcm_get_channels(const struct pcm *pcm)
{
   return pcm->config.channels;
}

/** Gets the PCM configuration.
 * @param pcm A PCM handle.
 * @return The PCM configuration.
 *  This function only returns NULL if
 *  @p pcm is NULL.
 * @ingroup libtinyalsa-pcm
 * */
static const struct pcm_config * pcm_get_config(const struct pcm *pcm)
{
   if (!pcm)
      return NULL;
   return &pcm->config;
}

/** Gets the rate of the PCM.
 * The rate is given in frames per second.
 * @param pcm A PCM handle.
 * @return The rate of the PCM.
 * @ingroup libtinyalsa-pcm
 */
static unsigned int pcm_get_rate(const struct pcm *pcm)
{
   return pcm->config.rate;
}

/** Gets the format of the PCM.
 * @param pcm A PCM handle.
 * @return The format of the PCM.
 * @ingroup libtinyalsa-pcm
 */
static enum pcm_format pcm_get_format(const struct pcm *pcm)
{
   return pcm->config.format;
}

/** Gets the file descriptor of the PCM.
 * Useful for extending functionality of the PCM when needed.
 * @param pcm A PCM handle.
 * @return The file descriptor of the PCM.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_get_file_descriptor(const struct pcm *pcm)
{
   return pcm->fd;
}

/** Gets the error message for the last error that occured.
 * If no error occured and this function is called, the results are undefined.
 * @param pcm A PCM handle.
 * @return The error message of the last error that occured.
 * @ingroup libtinyalsa-pcm
 */
static const char* pcm_get_error(const struct pcm *pcm)
{
   return pcm->error;
}

/** Gets the subdevice on which the pcm has been opened.
 * @param pcm A PCM handle.
 * @return The subdevice on which the pcm has been opened */
static unsigned int pcm_get_subdevice(const struct pcm *pcm)
{
    return pcm->subdevice;
}

/** Determines how many frames of a PCM can fit into a number of bytes.
 * @param pcm A PCM handle.
 * @param bytes The number of bytes.
 * @return The number of frames that may fit into @p bytes
 * @ingroup libtinyalsa-pcm
 */
static unsigned int pcm_bytes_to_frames(const struct pcm *pcm, unsigned int bytes)
{
    return bytes / (pcm->config.channels *
        (pcm_format_to_bits(pcm->config.format) >> 3));
}
#endif

/** Determines the number of bits occupied by a @ref pcm_format.
 * @param format A PCM format.
 * @return The number of bits associated with @p format
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_format_to_bits(enum pcm_format format)
{
   switch (format)
   {
      case PCM_FORMAT_S32_LE:
      case PCM_FORMAT_S32_BE:
      case PCM_FORMAT_S24_LE:
      case PCM_FORMAT_S24_BE:
         return 32;
      case PCM_FORMAT_S24_3LE:
      case PCM_FORMAT_S24_3BE:
         return 24;
      default:
      case PCM_FORMAT_S16_LE:
      case PCM_FORMAT_S16_BE:
         return 16;
      case PCM_FORMAT_S8:
         return 8;
   }
}

/** Determines how many bytes are occupied by a number of frames of a PCM.
 * @param pcm A PCM handle.
 * @param frames The number of frames of a PCM.
 * @return The bytes occupied by @p frames.
 * @ingroup libtinyalsa-pcm
 */
static unsigned int pcm_frames_to_bytes(const struct pcm *pcm, unsigned int frames)
{
   return frames * pcm->config.channels *
      (pcm_format_to_bits(pcm->config.format) >> 3);
}

/** Sets the PCM configuration.
 * @param pcm A PCM handle.
 * @param config The configuration to use for the
 *  PCM. This parameter may be NULL, in which case
 *  the default configuration is used.
 * @returns Zero on success, a negative errno value
 *  on failure.
 * @ingroup libtinyalsa-pcm
 * */
static int pcm_set_config(struct pcm *pcm, const struct pcm_config *config)
{
    struct snd_pcm_sw_params sparams;
    struct snd_pcm_hw_params params;

    if (!pcm)
        return -EFAULT;

    if (config)
        pcm->config = *config;
    else
    {
        config = &pcm->config;
        pcm->config.channels = 2;
        pcm->config.rate = 48000;
        pcm->config.period_size = 1024;
        pcm->config.period_count = 4;
        pcm->config.format = PCM_FORMAT_S16_LE;
        pcm->config.start_threshold = config->period_count * config->period_size;
        pcm->config.stop_threshold = config->period_count * config->period_size;
        pcm->config.silence_threshold = 0;
    }

    param_init(&params);
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_FORMAT,
                   pcm_format_to_alsa(config->format));
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                   SNDRV_PCM_SUBFORMAT_STD);
    param_set_min(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, config->period_size);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                  pcm_format_to_bits(config->format));
    param_set_int(&params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                  pcm_format_to_bits(config->format) * config->channels);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_CHANNELS,
                  config->channels);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_PERIODS, config->period_count);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_RATE, config->rate);

    if (pcm->flags & PCM_NOIRQ)
    {
        if (!(pcm->flags & PCM_MMAP))
        {
            RARCH_ERR("[TINYALSA]: noirq only currently supported with mmap().");
            return -EINVAL;
        }

        params.flags |= SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
        pcm->noirq_frames_per_msec = config->rate / 1000;
    }

    if (pcm->flags & PCM_MMAP)
        param_set_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                   SNDRV_PCM_ACCESS_MMAP_INTERLEAVED);
    else
        param_set_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                   SNDRV_PCM_ACCESS_RW_INTERLEAVED);

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params))
    {
        RARCH_ERR("[TINYALSA]: cannot set HW params.");
        return -errno;
    }

    /* get our refined hw_params */
    pcm->config.period_size = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    pcm->config.period_count = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIODS);
    pcm->buffer_size = config->period_count * config->period_size;

    if (pcm->flags & PCM_MMAP)
    {
        pcm->mmap_buffer = mmap(NULL, pcm_frames_to_bytes(pcm, pcm->buffer_size),
                                PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, pcm->fd, 0);
        if (pcm->mmap_buffer == MAP_FAILED)
        {
            RARCH_ERR("[TINYALSA]: failed to mmap buffer %d bytes\n",
                 pcm_frames_to_bytes(pcm, pcm->buffer_size));
            return -errno;
        }
    }

    memset(&sparams, 0, sizeof(sparams));
    sparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
    sparams.period_step = 1;
    sparams.avail_min   = 1;

    if (!config->start_threshold)
    {
        if (pcm->flags & PCM_IN)
            pcm->config.start_threshold = sparams.start_threshold = 1;
        else
            pcm->config.start_threshold = sparams.start_threshold =
                config->period_count * config->period_size / 2;
    } else
        sparams.start_threshold = config->start_threshold;

    /* pick a high stop threshold - todo: does this need further tuning */
    if (!config->stop_threshold)
    {
        if (pcm->flags & PCM_IN)
            pcm->config.stop_threshold = sparams.stop_threshold =
                config->period_count * config->period_size * 10;
        else
            pcm->config.stop_threshold = sparams.stop_threshold =
                config->period_count * config->period_size;
    }
    else
        sparams.stop_threshold = config->stop_threshold;

    sparams.xfer_align = config->period_size / 2; /* needed for old kernels */
    sparams.silence_size = 0;
    sparams.silence_threshold = config->silence_threshold;
    pcm->boundary = sparams.boundary = pcm->buffer_size;

    while (pcm->boundary * 2 <= INT_MAX - pcm->buffer_size)
        pcm->boundary *= 2;

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sparams))
    {
        RARCH_ERR("[TINYALSA]: Cannot set HW params.\n");
        return -errno;
    }

    return 0;
}

static int pcm_sync_ptr(struct pcm *pcm, int flags)
{
   if (pcm->sync_ptr)
   {
      pcm->sync_ptr->flags = flags;
      if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr) >= 0)
         return 0;
   }
   return -1;
}

static int pcm_hw_mmap_status(struct pcm *pcm)
{
   int page_size;
   if (pcm->sync_ptr)
      return 0;

   page_size = sysconf(_SC_PAGE_SIZE);

   pcm->mmap_status = (struct snd_pcm_mmap_status*)
      mmap(NULL, page_size, PROT_READ, MAP_FILE | MAP_SHARED,
            pcm->fd, SNDRV_PCM_MMAP_OFFSET_STATUS);
   if (pcm->mmap_status == MAP_FAILED)
      pcm->mmap_status = NULL;
   if (!pcm->mmap_status)
      goto mmap_error;

   pcm->mmap_control = (struct snd_pcm_mmap_control*)
      mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
            MAP_FILE | MAP_SHARED, pcm->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
   if (pcm->mmap_control == MAP_FAILED)
      pcm->mmap_control = NULL;
   if (!pcm->mmap_control)
   {
      munmap(pcm->mmap_status, page_size);
      pcm->mmap_status = NULL;
      goto mmap_error;
   }
   pcm->mmap_control->avail_min = 1;

   return 0;

mmap_error:

   pcm->sync_ptr = (struct snd_pcm_sync_ptr*)
      calloc(1, sizeof(*pcm->sync_ptr));
   if (!pcm->sync_ptr)
      return -ENOMEM;
   pcm->mmap_status = &pcm->sync_ptr->s.status;
   pcm->mmap_control = &pcm->sync_ptr->c.control;
   pcm->mmap_control->avail_min = 1;
   pcm_sync_ptr(pcm, 0);

   return 0;
}

static void pcm_hw_munmap_status(struct pcm *pcm)
{
    if (pcm->sync_ptr)
    {
        free(pcm->sync_ptr);
        pcm->sync_ptr = NULL;
    }
    else
    {
        int page_size = sysconf(_SC_PAGE_SIZE);
        if (pcm->mmap_status)
            munmap(pcm->mmap_status, page_size);
        if (pcm->mmap_control)
            munmap(pcm->mmap_control, page_size);
    }
    pcm->mmap_status = NULL;
    pcm->mmap_control = NULL;
}

#if 0
/* Unused for now */

static int pcm_areas_copy(struct pcm *pcm, unsigned int pcm_offset,
                          char *buf, unsigned int src_offset,
                          unsigned int frames)
{
    int size_bytes = pcm_frames_to_bytes(pcm, frames);
    int pcm_offset_bytes = pcm_frames_to_bytes(pcm, pcm_offset);
    int src_offset_bytes = pcm_frames_to_bytes(pcm, src_offset);

    /* interleaved only atm */
    if (pcm->flags & PCM_IN)
        memcpy(buf + src_offset_bytes,
               (char*)pcm->mmap_buffer + pcm_offset_bytes,
               size_bytes);
    else
        memcpy((char*)pcm->mmap_buffer + pcm_offset_bytes,
               buf + src_offset_bytes,
               size_bytes);
    return 0;
}
#endif

static INLINE int pcm_mmap_capture_avail(struct pcm *pcm)
{
    int avail = pcm->mmap_status->hw_ptr - pcm->mmap_control->appl_ptr;
    if (avail < 0)
        avail += pcm->boundary;
    return avail;
}

static INLINE int pcm_mmap_playback_avail(struct pcm *pcm)
{
    int avail = pcm->mmap_status->hw_ptr + pcm->buffer_size - pcm->mmap_control->appl_ptr;

    if (avail < 0)
        avail += pcm->boundary;
    else if (avail >= (int)pcm->boundary)
        avail -= pcm->boundary;

    return avail;
}

static INLINE int pcm_mmap_avail(struct pcm *pcm)
{
    pcm_sync_ptr(pcm, SNDRV_PCM_SYNC_PTR_HWSYNC);
    if (pcm->flags & PCM_IN)
        return pcm_mmap_capture_avail(pcm);
    return pcm_mmap_playback_avail(pcm);
}

#if 0
/* Unused for now */

static int pcm_mmap_begin(struct pcm *pcm, void **areas, unsigned int *offset,
      unsigned int *frames)
{
   unsigned int continuous, copy_frames, avail;

   /* return the mmap buffer */
   *areas = pcm->mmap_buffer;

   /* and the application offset in frames */
   *offset = pcm->mmap_control->appl_ptr % pcm->buffer_size;

   avail = pcm_mmap_avail(pcm);
   if (avail > pcm->buffer_size)
      avail = pcm->buffer_size;
   continuous = pcm->buffer_size - *offset;

   /* we can only copy frames if the are availabale and continuos */
   copy_frames = *frames;
   if (copy_frames > avail)
      copy_frames = avail;
   if (copy_frames > continuous)
      copy_frames = continuous;
   *frames = copy_frames;

   return 0;
}

static int pcm_mmap_commit(struct pcm *pcm, unsigned int offset, unsigned int frames)
{
    int ret;

    /* not used */
    (void) offset;

    /* update the application pointer in userspace and kernel */
    pcm_mmap_appl_forward(pcm, frames);
    ret = pcm_sync_ptr(pcm, 0);
    if (ret != 0)
    {
        printf("%d\n", ret);
        return ret;
    }

    return frames;
}

static void pcm_mmap_appl_forward(struct pcm *pcm, int frames)
{
    unsigned int appl_ptr = pcm->mmap_control->appl_ptr;
    appl_ptr += frames;

    /* check for boundary wrap */
    if (appl_ptr > pcm->boundary)
         appl_ptr -= pcm->boundary;
    pcm->mmap_control->appl_ptr = appl_ptr;
}

/** Returns available frames in pcm buffer and corresponding time stamp.
 * The clock is CLOCK_MONOTONIC if flag @ref PCM_MONOTONIC was specified in @ref pcm_open,
 * otherwise the clock is CLOCK_REALTIME.
 * For an input stream, frames available are frames ready for the application to read.
 * For an output stream, frames available are the number of empty frames available for the application to write.
 * Only available for PCMs opened with the @ref PCM_MMAP flag.
 * @param pcm A PCM handle.
 * @param avail The number of available frames
 * @param tstamp The timestamp
 * @return On success, zero is returned; on failure, negative one.
 */
static int pcm_get_htimestamp(struct pcm *pcm, unsigned int *avail,
      struct timespec *tstamp)
{
   int frames;
   int rc;
   snd_pcm_uframes_t hw_ptr;

   if (!pcm_is_ready(pcm))
      return -1;

   rc = pcm_sync_ptr(pcm, SNDRV_PCM_SYNC_PTR_APPL|SNDRV_PCM_SYNC_PTR_HWSYNC);
   if (rc < 0)
      return -1;

   if ((pcm->mmap_status->state != PCM_STATE_RUNNING) &&
         (pcm->mmap_status->state != PCM_STATE_DRAINING))
      return -1;

   *tstamp = pcm->mmap_status->tstamp;
   if (tstamp->tv_sec == 0 && tstamp->tv_nsec == 0)
      return -1;

   hw_ptr = pcm->mmap_status->hw_ptr;
   if (pcm->flags & PCM_IN)
      frames = hw_ptr - pcm->mmap_control->appl_ptr;
   else
      frames = hw_ptr + pcm->buffer_size - pcm->mmap_control->appl_ptr;

   if (frames < 0)
      return -1;

   *avail = (unsigned int)frames;

   return 0;
}
#endif

/** Checks if a PCM file has been opened without error.
 * @param pcm A PCM handle.
 *  May be NULL.
 * @return If a PCM's file descriptor is not valid or the pointer is NULL, it returns zero.
 *  Otherwise, the function returns one.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_is_ready(const struct pcm *pcm)
{
   if (pcm)
      return pcm->fd >= 0;
   return 0;
}

/** Prepares a PCM, if it has not been prepared already.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_prepare(struct pcm *pcm)
{
   if (pcm->prepared)
      return 0;

   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE) < 0)
   {
      RARCH_ERR("[TINYALSA]: Cannot prepare channel.\n");
      return -1;
   }

   pcm->prepared = 1;
   return 0;
}

/** Writes audio samples to PCM.
 * If the PCM has not been started, it is started in this function.
 * This function is only valid for PCMs opened with the @ref PCM_OUT flag.
 * This function is not valid for PCMs opened with the @ref PCM_MMAP flag.
 * @param pcm A PCM handle.
 * @param data The audio sample array
 * @param frame_count The number of frames occupied by the sample array.
 *  This value should not be greater than @ref TINYALSA_FRAMES_MAX
 *  or INT_MAX.
 * @return On success, this function returns the number of frames written; otherwise, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_writei(struct pcm *pcm, const void *data, unsigned int frame_count)
{
   struct snd_xferi x;

   if (pcm->flags & PCM_IN)
      return -EINVAL;
#if UINT_MAX > TINYALSA_FRAMES_MAX
   if (frame_count > TINYALSA_FRAMES_MAX)
      return -EINVAL;
#endif
   if (frame_count > INT_MAX)
      return -EINVAL;

   x.buf    = (void*)data;
   x.frames = frame_count;
   x.result = 0;

restart:
   if (!pcm->running)
   {
      int prepare_error = pcm_prepare(pcm);
      if (prepare_error)
         return prepare_error;
      if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x))
      {
         RARCH_ERR("[TINYALSA]: Cannot write initial data.\n");
         return -1;
      }
      pcm->running = 1;
      return 0;
   }

   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x))
   {
      pcm->prepared = 0;
      pcm->running = 0;
      if (errno == EPIPE)
      {
         /* we failed to make our window -- try to restart if we are
          * allowed to do so.  Otherwise, simply allow the EPIPE error to
          * propagate up to the app level */
         pcm->underruns++;
         if (pcm->flags & PCM_NORESTART)
            return -EPIPE;
         goto restart;
      }
#if 0
      /* This tends to spam a lot */
      RARCH_ERR("[TINYALSA]: Cannot write stream data.\n");
#endif
      return -1;
   }

   return x.result;
}

#if 0
/* Unused for now */
/** Starts a PCM.
 * If the PCM has not been prepared,
 * it is prepared in this function.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_start(struct pcm *pcm)
{
   int prepare_error = pcm_prepare(pcm);
   if (prepare_error)
      return prepare_error;

   if (pcm->flags & PCM_MMAP)
      pcm_sync_ptr(pcm, 0);

   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START) < 0)
   {
      RARCH_ERR("[TINYALSA]: Cannot start channel.\n");
      return -1;
   }

   pcm->running = 1;
   return 0;
}

/** Reads audio samples from PCM.
 * If the PCM has not been started, it is started in this function.
 * This function is only valid for PCMs opened with the @ref PCM_IN flag.
 * This function is not valid for PCMs opened with the @ref PCM_MMAP flag.
 * @param pcm A PCM handle.
 * @param data The audio sample array
 * @param frame_count The number of frames occupied by the sample array.
 *  This value should not be greater than @ref TINYALSA_FRAMES_MAX
 *  or INT_MAX.
 * @return On success, this function returns the number of frames written; otherwise, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_readi(struct pcm *pcm, void *data, unsigned int frame_count)
{
   struct snd_xferi x;

   if (!(pcm->flags & PCM_IN))
      return -EINVAL;
#if UINT_MAX > TINYALSA_FRAMES_MAX
   if (frame_count > TINYALSA_FRAMES_MAX)
      return -EINVAL;
#endif
   if (frame_count > INT_MAX)
      return -EINVAL;

   x.buf    = data;
   x.frames = frame_count;
   x.result = 0;
   for (;;)
   {
      if ((!pcm->running) && (pcm_start(pcm) < 0))
         return -errno;
      else if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x))
      {
         pcm->prepared = 0;
         pcm->running  = 0;
         if (errno == EPIPE)
         {
            /* we failed to make our window -- try to restart */
            pcm->underruns++;
            continue;
         }
         RARCH_ERR("[TINYALSA]: Cannot read stream data.\n");
         return -1;
      }
      return x.result;
   }
}

/** Writes audio samples to PCM.
 * If the PCM has not been started, it is started in this function.
 * This function is only valid for PCMs opened with the @ref PCM_OUT flag.
 * This function is not valid for PCMs opened with the @ref PCM_MMAP flag.
 * @param pcm A PCM handle.
 * @param data The audio sample array
 * @param count The number of bytes occupied by the sample array.
 * @return On success, this function returns zero; otherwise, a negative number.
 * @deprecated
 * @ingroup libtinyalsa-pcm
 */
static int pcm_write(struct pcm *pcm, const void *data, unsigned int count)
{
   return pcm_writei(pcm, data, pcm_bytes_to_frames(pcm, count));
}

/** Reads audio samples from PCM.
 * If the PCM has not been started, it is started in this function.
 * This function is only valid for PCMs opened with the @ref PCM_IN flag.
 * This function is not valid for PCMs opened with the @ref PCM_MMAP flag.
 * @param pcm A PCM handle.
 * @param data The audio sample array
 * @param count The number of bytes occupied by the sample array.
 * @return On success, this function returns zero; otherwise, a negative number.
 * @deprecated
 * @ingroup libtinyalsa-pcm
 */
static int pcm_read(struct pcm *pcm, void *data, unsigned int count)
{
   return pcm_readi(pcm, data, pcm_bytes_to_frames(pcm, count));
}
#endif

static struct pcm bad_pcm = {
    -1                       /* fd */
};

/** Gets the hardware parameters of a PCM, without created a PCM handle.
 * @param card The card of the PCM.
 *  The default card is zero.
 * @param device The device of the PCM.
 *  The default device is zero.
 * @param flags Specifies whether the PCM is an input or output.
 *  May be one of the following:
 *   - @ref PCM_IN
 *   - @ref PCM_OUT
 * @return On success, the hardware parameters of the PCM; on failure, NULL.
 * @ingroup libtinyalsa-pcm
 */
static struct pcm_params *pcm_params_get(unsigned int card, unsigned int device,
      unsigned int flags)
{
   struct snd_pcm_hw_params *params;
   char fn[256];
   int fd;

   snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
         flags & PCM_IN ? 'c' : 'p');

   fd = open(fn, O_RDWR|O_NONBLOCK);
   if (fd < 0)
   {
      fprintf(stderr, "cannot open device '%s'\n", fn);
      goto err_open;
   }

   params = (struct snd_pcm_hw_params*)
      calloc(1, sizeof(struct snd_pcm_hw_params));

   if (!params)
      goto err_calloc;

   param_init(params);
   if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params))
   {
      fprintf(stderr, "SNDRV_PCM_IOCTL_HW_REFINE error (%d)\n", errno);
      goto err_hw_refine;
   }

   close(fd);

   return (struct pcm_params *)params;

err_hw_refine:
   free(params);
err_calloc:
   close(fd);
err_open:
   return NULL;
}

/** Frees the hardware parameters returned by @ref pcm_params_get.
 * @param pcm_params Hardware parameters of a PCM.
 *  May be NULL.
 * @ingroup libtinyalsa-pcm
 */
static void pcm_params_free(struct pcm_params *pcm_params)
{
   struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;

   if (params)
      free(params);
}

#if 0
/* Unused for now */

/** Gets a mask from a PCM's hardware parameters.
 * @param pcm_params A PCM's hardware parameters.
 * @param param The parameter to get.
 * @return If @p pcm_params is NULL or @p param is not a mask, NULL is returned.
 *  Otherwise, the mask associated with @p param is returned.
 * @ingroup libtinyalsa-pcm
 */
static const struct pcm_mask *pcm_params_get_mask(const struct pcm_params *pcm_params,
      enum pcm_param param)
{
    int p;
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    if (!params)
        return NULL;

    p = pcm_param_to_alsa(param);
    if (p < 0 || !param_is_mask(p))
        return NULL;

    return (const struct pcm_mask *)param_to_mask(params, p);
}
#endif

static int pcm_param_to_alsa(enum pcm_param param)
{
   switch (param)
   {
      case PCM_PARAM_ACCESS:
         return SNDRV_PCM_HW_PARAM_ACCESS;
      case PCM_PARAM_FORMAT:
         return SNDRV_PCM_HW_PARAM_FORMAT;
      case PCM_PARAM_SUBFORMAT:
         return SNDRV_PCM_HW_PARAM_SUBFORMAT;
      case PCM_PARAM_SAMPLE_BITS:
         return SNDRV_PCM_HW_PARAM_SAMPLE_BITS;
      case PCM_PARAM_FRAME_BITS:
         return SNDRV_PCM_HW_PARAM_FRAME_BITS;
      case PCM_PARAM_CHANNELS:
         return SNDRV_PCM_HW_PARAM_CHANNELS;
      case PCM_PARAM_RATE:
         return SNDRV_PCM_HW_PARAM_RATE;
      case PCM_PARAM_PERIOD_TIME:
         return SNDRV_PCM_HW_PARAM_PERIOD_TIME;
      case PCM_PARAM_PERIOD_SIZE:
         return SNDRV_PCM_HW_PARAM_PERIOD_SIZE;
      case PCM_PARAM_PERIOD_BYTES:
         return SNDRV_PCM_HW_PARAM_PERIOD_BYTES;
      case PCM_PARAM_PERIODS:
         return SNDRV_PCM_HW_PARAM_PERIODS;
      case PCM_PARAM_BUFFER_TIME:
         return SNDRV_PCM_HW_PARAM_BUFFER_TIME;
      case PCM_PARAM_BUFFER_SIZE:
         return SNDRV_PCM_HW_PARAM_BUFFER_SIZE;
      case PCM_PARAM_BUFFER_BYTES:
         return SNDRV_PCM_HW_PARAM_BUFFER_BYTES;
      case PCM_PARAM_TICK_TIME:
         return SNDRV_PCM_HW_PARAM_TICK_TIME;

      default:
         break;
   }

   return -1;
}

static unsigned int param_get_min(const struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n))
    {
        const struct snd_interval *i = param_get_interval(p, n);
        return i->min;
    }
    return 0;
}

/** Get the minimum of a specified PCM parameter.
 * @param pcm_params A PCM parameters structure.
 * @param param The specified parameter to get the minimum of.
 * @returns On success, the parameter minimum.
 *  On failure, zero.
 */
static unsigned int pcm_params_get_min(const struct pcm_params *pcm_params,
      enum pcm_param param)
{
   struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
   int p;

   if (!params)
      return 0;

   p = pcm_param_to_alsa(param);
   if (p < 0)
      return 0;

   return param_get_min(params, p);
}

static unsigned int param_get_max(const struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n))
    {
        const struct snd_interval *i = param_get_interval(p, n);
        return i->max;
    }
    return 0;
}

/** Get the maximum of a specified PCM parameter.
 * @param pcm_params A PCM parameters structure.
 * @param param The specified parameter to get the maximum of.
 * @returns On success, the parameter maximum.
 *  On failure, zero.
 */
static unsigned int pcm_params_get_max(const struct pcm_params *pcm_params,
      enum pcm_param param)
{
   const struct snd_pcm_hw_params *params = (const struct snd_pcm_hw_params *)pcm_params;
   int p;

   if (!params)
      return 0;

   p = pcm_param_to_alsa(param);
   if (p < 0)
      return 0;

   return param_get_max(params, p);
}

/** Stops a PCM.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_stop(struct pcm *pcm)
{
   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DROP) < 0)
   {
      RARCH_ERR("[TINYALSA]: Cannot stop channel.\n");
      return -1;
   }

   pcm->prepared = 0;
   pcm->running = 0;
   return 0;
}

static int pcm_params_can_pause(const struct pcm_params *pcm_params)
{
   const struct snd_pcm_hw_params *params = (const struct snd_pcm_hw_params *)pcm_params;

   if (!params)
      return 0;

   return (params->info & SNDRV_PCM_INFO_PAUSE) ? 1 : 0;
}

static int pcm_pause(struct pcm *pcm, int enable)
{
   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PAUSE, enable) < 0)
      return -1;

   return 0;
}

/** Closes a PCM returned by @ref pcm_open.
 * @param pcm A PCM returned by @ref pcm_open.
 *  May not be NULL.
 * @return Always returns zero.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_close(struct pcm *pcm)
{
   if (pcm == &bad_pcm)
      return 0;

   pcm_hw_munmap_status(pcm);

   if (pcm->flags & PCM_MMAP)
   {
      pcm_stop(pcm);
      munmap(pcm->mmap_buffer, pcm_frames_to_bytes(pcm, pcm->buffer_size));
   }

   if (pcm->fd >= 0)
      close(pcm->fd);
   pcm->prepared = 0;
   pcm->running = 0;
   pcm->buffer_size = 0;
   pcm->fd = -1;
   free(pcm);
   return 0;
}

/** Opens a PCM.
 * @param card The card that the pcm belongs to.
 *  The default card is zero.
 * @param device The device that the pcm belongs to.
 *  The default device is zero.
 * @param flags Specify characteristics and functionality about the pcm.
 *  May be a bitwise AND of the following:
 *   - @ref PCM_IN
 *   - @ref PCM_OUT
 *   - @ref PCM_MMAP
 *   - @ref PCM_NOIRQ
 *   - @ref PCM_MONOTONIC
 * @param config The hardware and software parameters to open the PCM with.
 * @returns A PCM structure.
 *  If an error occurs allocating memory for the PCM, NULL is returned.
 *  Otherwise, client code should check that the PCM opened properly by calling @ref pcm_is_ready.
 *  If @ref pcm_is_ready, check @ref pcm_get_error for more information.
 * @ingroup libtinyalsa-pcm
 */
static struct pcm *pcm_open(unsigned int card, unsigned int device,
      unsigned int flags, const struct pcm_config *config)
{
   int rc;
   char fn[256];
   struct snd_pcm_info info;
   struct pcm *pcm = (struct pcm*)calloc(1, sizeof(struct pcm));
   if (!pcm)
      return &bad_pcm;

   snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
         flags & PCM_IN ? 'c' : 'p');

   pcm->flags = flags;
   pcm->fd    = open(fn, O_RDWR|O_NONBLOCK);
   if (pcm->fd < 0)
   {
      RARCH_ERR("[TINYALSA]: cannot open device '%s'\n", fn);
      return pcm;
   }

   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_INFO, &info))
   {
      RARCH_ERR("[TINYALSA]: cannot get info.\n");
      goto fail_close;
   }
   pcm->subdevice = info.subdevice;

   if (pcm_set_config(pcm, config) != 0)
      goto fail_close;

   rc = pcm_hw_mmap_status(pcm);
   if (rc < 0)
   {
      RARCH_ERR("[TINYALSA]: mmap status failed.\n");
      goto fail;
   }

#ifdef SNDRV_PCM_IOCTL_TTSTAMP
   if (pcm->flags & PCM_MONOTONIC)
   {
      int arg = SNDRV_PCM_TSTAMP_TYPE_MONOTONIC;
      rc      = ioctl(pcm->fd, SNDRV_PCM_IOCTL_TTSTAMP, &arg);
      if (rc < 0)
      {
         RARCH_ERR("[TINYALSA]: Cannot set timestamp type.\n");
         goto fail;
      }
   }
#endif

   pcm->underruns = 0;
   return pcm;

fail:
   if (flags & PCM_MMAP)
      munmap(pcm->mmap_buffer, pcm_frames_to_bytes(pcm, pcm->buffer_size));
fail_close:
   close(pcm->fd);
   pcm->fd = -1;
   return pcm;
}

#if 0
/* Unused for now */

/** Opens a PCM by it's name.
 * @param name The name of the PCM.
 *  The name is given in the format: <i>hw</i>:<b>card</b>,<b>device</b>
 * @param flags Specify characteristics and functionality about the pcm.
 *  May be a bitwise AND of the following:
 *   - @ref PCM_IN
 *   - @ref PCM_OUT
 *   - @ref PCM_MMAP
 *   - @ref PCM_NOIRQ
 *   - @ref PCM_MONOTONIC
 * @param config The hardware and software parameters to open the PCM with.
 * @returns A PCM structure.
 *  If an error occurs allocating memory for the PCM, NULL is returned.
 *  Otherwise, client code should check that the PCM opened properly by calling @ref pcm_is_ready.
 *  If @ref pcm_is_ready, check @ref pcm_get_error for more information.
 * @ingroup libtinyalsa-pcm
 */
static struct pcm *pcm_open_by_name(const char *name,
      unsigned int flags,
      const struct pcm_config *config)
{
  unsigned int card, device;
  if ((name[0] != 'h')
   || (name[1] != 'w')
   || (name[2] != ':'))
    return NULL;

  if (sscanf(&name[3], "%u,%u", &card, &device) != 2)
    return NULL;

  return pcm_open(card, device, flags, config);
}

/** Links two PCMs.
 * After this function is called, the two PCMs will prepare, start and stop in sync (at the same time).
 * If an error occurs, the error message will be written to @p pcm1.
 * @param pcm1 A PCM handle.
 * @param pcm2 Another PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_link(struct pcm *pcm1, struct pcm *pcm2)
{
   int err = ioctl(pcm1->fd, SNDRV_PCM_IOCTL_LINK, pcm2->fd);
   if (err == -1)
   {
      RARCH_ERR("[TINYALSA]: Cannot link PCM.\n");
      return -1;
   }
   return 0;
}

/** Unlinks a PCM.
 * @see @ref pcm_link
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_unlink(struct pcm *pcm)
{
   int err = ioctl(pcm->fd, SNDRV_PCM_IOCTL_UNLINK);
   if (err == -1)
   {
      RARCH_ERR("[TINYALSA]: Cannot unlink PCM.\n");
      return -1;
   }
   return 0;
}
#endif

static int pcm_avail_update(struct pcm *pcm)
{
   pcm_sync_ptr(pcm, 0);
   return pcm_mmap_avail(pcm);
}

#if 0
/* No longer used */

static int pcm_state(struct pcm *pcm)
{
   int err = pcm_sync_ptr(pcm, 0);
   if (err < 0)
      return err;

   return pcm->mmap_status->state;
}
#endif

/** Waits for frames to be available for read or write operations.
 * @param pcm A PCM handle.
 * @param timeout The maximum amount of time to wait for, in terms of milliseconds.
 * @returns If frames became available, one is returned.
 *  If a timeout occured, zero is returned.
 *  If an error occured, a negative number is returned.
 * @ingroup libtinyalsa-pcm
 */
static int pcm_wait(struct pcm *pcm, int timeout)
{
   struct pollfd pfd;

   pfd.fd     = pcm->fd;
   pfd.events = POLLIN | POLLOUT | POLLERR | POLLNVAL;

   do
   {
      /* let's wait for avail or timeout */
      int err = poll(&pfd, 1, timeout);
      if (err < 0)
         return -errno;

      /* timeout ? */
      if (err == 0)
         return 0;

      /* have we been interrupted ? */
      if (errno == -EINTR)
         continue;

      /* check for any errors */
      if (pfd.revents & (POLLERR | POLLNVAL))
      {
         int cond = -1;

         if (pcm->sync_ptr)
         {
            pcm->sync_ptr->flags = 0;
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr) >= 0)
               cond = pcm->mmap_status->state;
         }

         switch (cond)
         {
            case PCM_STATE_XRUN:
               return -EPIPE;
            case PCM_STATE_SUSPENDED:
               return -ESTRPIPE;
            case PCM_STATE_DISCONNECTED:
               return -ENODEV;
            default:
               break;
         }

         return -EIO;
      }
      /* poll again if fd not ready for IO */
   } while (!(pfd.revents & (POLLIN | POLLOUT)));

   return 1;
}

#if 0
/* Unused for now */

static int pcm_mmap_transfer(struct pcm *pcm, const void *buffer, unsigned int bytes)
{
   int err = 0, frames, avail;
   unsigned int offset = 0, count;

   if (bytes == 0)
      return 0;

   count = pcm_bytes_to_frames(pcm, bytes);

   while (count > 0)
   {
      /* get the available space for writing new frames */
      avail = pcm_avail_update(pcm);
      if (avail < 0)
      {
         fprintf(stderr, "cannot determine available mmap frames");
         return err;
      }

      /* start the audio if we reach the threshold */
      if (!pcm->running &&
            (pcm->buffer_size - avail) >= pcm->config.start_threshold)
      {
         if (pcm_start(pcm) < 0)
         {
            fprintf(stderr, "start error: hw 0x%x app 0x%x avail 0x%x\n",
                  (unsigned int)pcm->mmap_status->hw_ptr,
                  (unsigned int)pcm->mmap_control->appl_ptr,
                  avail);
            return -errno;
         }
      }

      /* sleep until we have space to write new frames */
      if (pcm->running &&
            (unsigned int)avail < pcm->mmap_control->avail_min)
      {
         int time = -1;

         if (pcm->flags & PCM_NOIRQ)
            time = (pcm->buffer_size - avail - pcm->mmap_control->avail_min)
               / pcm->noirq_frames_per_msec;

         err = pcm_wait(pcm, time);
         if (err < 0)
         {
            pcm->prepared = 0;
            pcm->running = 0;
            fprintf(stderr, "wait error: hw 0x%x app 0x%x avail 0x%x\n",
                  (unsigned int)pcm->mmap_status->hw_ptr,
                  (unsigned int)pcm->mmap_control->appl_ptr,
                  avail);
            pcm->mmap_control->appl_ptr = 0;
            return err;
         }
         continue;
      }

      frames = count;
      if (frames > avail)
         frames = avail;

      if (!frames)
         break;

      /* copy frames from buffer */
      frames = pcm_mmap_transfer_areas(pcm, (void *)buffer, offset, frames);
      if (frames < 0)
      {
         fprintf(stderr, "write error: hw 0x%x app 0x%x avail 0x%x\n",
               (unsigned int)pcm->mmap_status->hw_ptr,
               (unsigned int)pcm->mmap_control->appl_ptr,
               avail);
         return frames;
      }

      offset += frames;
      count -= frames;
   }

   return 0;
}

static int pcm_mmap_write(struct pcm *pcm, const void *data, unsigned int count)
{
   if ((~pcm->flags) & (PCM_OUT | PCM_MMAP))
      return -ENOSYS;

   return pcm_mmap_transfer(pcm, (void *)data, count);
}

static int pcm_mmap_read(struct pcm *pcm, void *data, unsigned int count)
{
   if ((~pcm->flags) & (PCM_IN | PCM_MMAP))
      return -ENOSYS;

   return pcm_mmap_transfer(pcm, data, count);
}

/** Gets the delay of the PCM, in terms of frames.
 * @param pcm A PCM handle.
 * @returns On success, the delay of the PCM.
 *  On failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
static long pcm_get_delay(struct pcm *pcm)
{
   if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DELAY, &pcm->pcm_delay) < 0)
      return -1;

   return pcm->pcm_delay;
}

static int pcm_mmap_transfer_areas(struct pcm *pcm, char *buf,
      unsigned int offset, unsigned int size)
{
   void *pcm_areas;
   int commit;
   unsigned int pcm_offset, frames, count = 0;

   while (size > 0)
   {
      frames = size;
      pcm_mmap_begin(pcm, &pcm_areas, &pcm_offset, &frames);
      pcm_areas_copy(pcm, pcm_offset, buf, offset, frames);
      commit = pcm_mmap_commit(pcm, pcm_offset, frames);
      if (commit < 0)
      {
         RARCH_ERR("[TINYALSA}: failed to commit %d frames.\n", frames);
         return commit;
      }

      offset += commit;
      count += commit;
      size -= commit;
   }
   return count;
}
#endif

/* End of implementation tinyalsa pcm */

typedef struct tinyalsa
{
   struct pcm        *pcm;
   struct pcm_params *params;
   size_t            buffer_size;
   bool              nonblock;
   bool              has_float;
   bool              can_pause;
   bool              is_paused;
   unsigned int      frame_bits;
} tinyalsa_t;

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

static void * tinyalsa_init(const char *devicestr, unsigned rate,
      unsigned latency, unsigned block_frames,
      unsigned *new_rate)
{
   unsigned int card            = 0;
   unsigned int device          = 0;
   unsigned int frames_per_ms   = 0;
   unsigned int orig_rate       = rate;
   unsigned int max_rate, min_rate, buffer_size;
   float initial_latency;

   struct pcm_config         config;

   tinyalsa_t *tinyalsa      = (tinyalsa_t*)calloc(1, sizeof(tinyalsa_t));

   if (!tinyalsa)
      return NULL;

   if (devicestr)
      sscanf(devicestr, "%u,%u", &card, &device);

   RARCH_LOG("[TINYALSA]: Using card: %u, device: %u.\n", card, device);

   tinyalsa->params = pcm_params_get(card, device, PCM_OUT);
   if (!tinyalsa->params)
   {
      RARCH_ERR("[TINYALSA]: params: Cannot open audio device.\n");
      goto error;
   }

   if (pcm_params_can_pause(tinyalsa->params))
      tinyalsa->can_pause = true;

   min_rate = pcm_params_get_min(tinyalsa->params, PCM_PARAM_RATE);
   max_rate = pcm_params_get_max(tinyalsa->params, PCM_PARAM_RATE);

   if (!(rate >= min_rate && rate <= max_rate))
   {
      RARCH_WARN("[TINYALSA]: Sample rate cannot be larger than %uHz "\
                 "or smaller than %uHz.\n", max_rate, min_rate);
      RARCH_WARN("[TINYALSA]: Trying to set a valid sample rate.\n");

      if (rate > max_rate)
         rate = max_rate;
      else if (rate < min_rate)
         rate = min_rate;
   }

   if (orig_rate != rate)
      *new_rate = rate;

   config.rate              = rate;
   config.format            = is_little_endian() ?\
                              PCM_FORMAT_S16_LE : PCM_FORMAT_S16_BE;
   config.channels          = 2;
   config.period_size       = 1024;
   config.period_count      = 4;
   config.start_threshold   = config.period_size;
   config.stop_threshold    = 0;
   config.silence_threshold = 0;

   tinyalsa->pcm = pcm_open(card, device, PCM_OUT, &config);

   if (!tinyalsa->pcm)
   {
      RARCH_ERR("[TINYALSA]: Failed to allocate memory for pcm.\n");
      goto error;
   }
   else if (!pcm_is_ready(tinyalsa->pcm))
   {
      RARCH_ERR("[TINYALSA]: Cannot open audio device.\n");
      goto error;
   }

   buffer_size           = pcm_get_buffer_size(tinyalsa->pcm);
   tinyalsa->buffer_size = pcm_frames_to_bytes(tinyalsa->pcm, buffer_size);
   tinyalsa->frame_bits  = pcm_format_to_bits(config.format) * 2;

   initial_latency       = (float)(buffer_size * 1000) / (float)(rate * 4);
   frames_per_ms         = buffer_size / initial_latency;

   if (latency < (unsigned int)initial_latency)
   {
      RARCH_WARN("[TINYALSA]: Cannot have a latency less than %ums. "\
                 "Defaulting to 64ms.\n", (unsigned int)initial_latency);
      latency = 64;
   }

   latency               -= (unsigned int)initial_latency;
   buffer_size           += latency * frames_per_ms;

   tinyalsa->has_float   = false;

   RARCH_LOG("[TINYALSA]: Can pause: %s.\n", tinyalsa->can_pause ? "yes" : "no");
   RARCH_LOG("[TINYALSA]: Audio rate: %uHz.\n", config.rate);
   RARCH_LOG("[TINYALSA]: Buffer size: %u frames.\n", buffer_size);
   RARCH_LOG("[TINYALSA]: Buffer size: %u bytes.\n", (unsigned int)tinyalsa->buffer_size);
   RARCH_LOG("[TINYALSA]: Frame  size: %u bytes.\n", tinyalsa->frame_bits / 8);
   RARCH_LOG("[TINYALSA]: Latency: %ums.\n", buffer_size * 1000 / (rate * 4));

   pcm_params_free(tinyalsa->params);

   return tinyalsa;

error:
   RARCH_ERR("[TINYALSA]: Failed to initialize tinyalsa driver.\n");

   if (tinyalsa->params)
      pcm_params_free(tinyalsa->params);

   if (tinyalsa)
      free(tinyalsa);

   return NULL;
}

static ssize_t
tinyalsa_write(void *data, const void *buf_, size_t size_)
{
   tinyalsa_t *tinyalsa      = (tinyalsa_t*)data;
   const uint8_t *buf        = (const uint8_t*)buf_;
   snd_pcm_sframes_t written = 0;
   snd_pcm_sframes_t size    = BYTES_TO_FRAMES(size_, tinyalsa->frame_bits);
   size_t frames_size        = tinyalsa->has_float ? sizeof(float) : sizeof(int16_t);

   if (tinyalsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames   = pcm_writei(tinyalsa->pcm, buf, size);

         if (frames < 0)
            pcm_stop(tinyalsa->pcm);

         written += frames;
         buf     += (frames << 1) * frames_size;
         size    -= frames;
      }
   }
   else
   {
      while (size)
      {
         snd_pcm_sframes_t frames;
         pcm_wait(tinyalsa->pcm, -1);

         frames   = pcm_writei(tinyalsa->pcm, buf, size);

         if (frames < 0)
            return -1;

         written += frames;
         buf     += (frames << 1) * frames_size;
         size    -= frames;
      }
   }

   return written;

}

static bool
tinyalsa_stop(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa->can_pause && !tinyalsa->is_paused)
   {
		int ret = pcm_pause(tinyalsa->pcm, 1);
		if (ret < 0)
			return false;

		tinyalsa->is_paused = true;
	}

	return true;
}

static bool
tinyalsa_alive(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa)
		return !tinyalsa->is_paused;

	return false;
}

static bool
tinyalsa_start(void *data, bool is_shutdown)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	if (tinyalsa->can_pause && tinyalsa->is_paused)
   {
		int ret = pcm_pause(tinyalsa->pcm, 0);

		if (ret < 0)
      {
			RARCH_ERR("[TINYALSA]: Failed to unpause.\n");
			return false;
		}

		tinyalsa->is_paused = false;
	}

	return true;
}

static void tinyalsa_set_nonblock_state(void *data, bool state)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;
	tinyalsa->nonblock = state;
}

static bool tinyalsa_use_float(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	return tinyalsa->has_float;
}

static void tinyalsa_free(void *data)
{
   tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

   if (tinyalsa)
   {
      if (tinyalsa->pcm)
         pcm_close(tinyalsa->pcm);

      tinyalsa->pcm = NULL;
      free(tinyalsa);
   }
}

static size_t tinyalsa_write_avail(void *data)
{
   tinyalsa_t *alsa        = (tinyalsa_t*)data;
   snd_pcm_sframes_t avail = pcm_avail_update(alsa->pcm);

   if (avail < 0)
      return alsa->buffer_size;

   return FRAMES_TO_BYTES(avail, alsa->frame_bits);
}

static size_t tinyalsa_buffer_size(void *data)
{
	tinyalsa_t *tinyalsa = (tinyalsa_t*)data;

	return tinyalsa->buffer_size;
}

audio_driver_t audio_tinyalsa = {
	tinyalsa_init,               /* AUDIO_init              */
	tinyalsa_write,              /* AUDIO_write             */
	tinyalsa_stop,               /* AUDIO_stop              */
	tinyalsa_start,              /* AUDIO_start             */
	tinyalsa_alive,              /* AUDIO_alive             */
	tinyalsa_set_nonblock_state, /* AUDIO_set_nonblock_sate */
	tinyalsa_free,               /* AUDIO_free              */
	tinyalsa_use_float,          /* AUDIO_use_float         */
	"tinyalsa",                  /* "AUDIO"                 */
	NULL,                        /* AUDIO_device_list_new   */ /*TODO*/
	NULL,                        /* AUDIO_device_list_free  */ /*TODO*/
   tinyalsa_write_avail,        /* AUDIO_write_avail       */ /*TODO*/
	tinyalsa_buffer_size,        /* AUDIO_buffer_size       */ /*TODO*/
};
