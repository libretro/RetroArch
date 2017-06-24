/* pcm.h
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

/** @file */

/** @defgroup libtinyalsa-pcm PCM Interface
 * @brief All macros, structures and functions that make up the PCM interface.
 */

#ifndef TINYALSA_PCM_H
#define TINYALSA_PCM_H

#include <sys/time.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

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
enum pcm_format {
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

/** A bit mask of 256 bits (32 bytes) that describes some hardware parameters of a PCM */
struct pcm_mask {
    /** bits of the bit mask */
    unsigned int bits[32 / sizeof(unsigned int)];
};

/** Encapsulates the hardware and software parameters of a PCM.
 * @ingroup libtinyalsa-pcm
 */
struct pcm_config {
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
    PCM_PARAM_TICK_TIME,
}; /* enum pcm_param */

struct pcm_params;

struct pcm_params *pcm_params_get(unsigned int card, unsigned int device,
                                  unsigned int flags);

void pcm_params_free(struct pcm_params *pcm_params);

const struct pcm_mask *pcm_params_get_mask(const struct pcm_params *pcm_params, enum pcm_param param);

unsigned int pcm_params_get_min(const struct pcm_params *pcm_params, enum pcm_param param);

unsigned int pcm_params_get_max(const struct pcm_params *pcm_params, enum pcm_param param);

struct pcm;

struct pcm *pcm_open(unsigned int card,
                     unsigned int device,
                     unsigned int flags,
                     const struct pcm_config *config);

struct pcm *pcm_open_by_name(const char *name,
                             unsigned int flags,
                             const struct pcm_config *config);

int pcm_close(struct pcm *pcm);

int pcm_is_ready(const struct pcm *pcm);

unsigned int pcm_get_channels(const struct pcm *pcm);

const struct pcm_config * pcm_get_config(const struct pcm *pcm);

unsigned int pcm_get_rate(const struct pcm *pcm);

enum pcm_format pcm_get_format(const struct pcm *pcm);

int pcm_get_file_descriptor(const struct pcm *pcm);

const char *pcm_get_error(const struct pcm *pcm);

int pcm_set_config(struct pcm *pcm, const struct pcm_config *config);

unsigned int pcm_format_to_bits(enum pcm_format format);

unsigned int pcm_get_buffer_size(const struct pcm *pcm);

unsigned int pcm_frames_to_bytes(const struct pcm *pcm, unsigned int frames);

unsigned int pcm_bytes_to_frames(const struct pcm *pcm, unsigned int bytes);

int pcm_get_htimestamp(struct pcm *pcm, unsigned int *avail, struct timespec *tstamp);

unsigned int pcm_get_subdevice(const struct pcm *pcm);

int pcm_writei(struct pcm *pcm, const void *data, unsigned int frame_count);

int pcm_readi(struct pcm *pcm, void *data, unsigned int frame_count);

#ifdef __GNUC__

int pcm_write(struct pcm *pcm, const void *data, unsigned int count) __attribute((deprecated));

int pcm_read(struct pcm *pcm, void *data, unsigned int count) __attribute((deprecated));

#else

int pcm_write(struct pcm *pcm, const void *data, unsigned int count);

int pcm_read(struct pcm *pcm, void *data, unsigned int count);

#endif

int pcm_mmap_write(struct pcm *pcm, const void *data, unsigned int count);

int pcm_mmap_read(struct pcm *pcm, void *data, unsigned int count);

int pcm_mmap_begin(struct pcm *pcm, void **areas, unsigned int *offset, unsigned int *frames);

int pcm_mmap_commit(struct pcm *pcm, unsigned int offset, unsigned int frames);

int pcm_link(struct pcm *pcm1, struct pcm *pcm2);

int pcm_unlink(struct pcm *pcm);

int pcm_prepare(struct pcm *pcm);

int pcm_start(struct pcm *pcm);

int pcm_stop(struct pcm *pcm);

int pcm_wait(struct pcm *pcm, int timeout);

long pcm_get_delay(struct pcm *pcm);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif

