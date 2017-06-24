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
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>

#include <tinyalsa/pcm.h>
#include <tinyalsa/limits.h>

#define PARAM_MAX SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP (1<<2)

static inline int param_is_mask(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline int param_is_interval(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static inline const struct snd_interval *param_get_interval(const struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned int bit)
{
    if (bit >= SNDRV_MASK_MAX)
        return;
    if (param_is_mask(n)) {
        struct snd_mask *m = param_to_mask(p, n);
        m->bits[0] = 0;
        m->bits[1] = 0;
        m->bits[bit >> 5] |= (1 << (bit & 31));
    }
}

static void param_set_min(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
    }
}

static unsigned int param_get_min(const struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        const struct snd_interval *i = param_get_interval(p, n);
        return i->min;
    }
    return 0;
}

static unsigned int param_get_max(const struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
        const struct snd_interval *i = param_get_interval(p, n);
        return i->max;
    }
    return 0;
}

static void param_set_int(struct snd_pcm_hw_params *p, int n, unsigned int val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
        i->max = val;
        i->integer = 1;
    }
}

static unsigned int param_get_int(struct snd_pcm_hw_params *p, int n)
{
    if (param_is_interval(n)) {
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
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            m->bits[0] = ~0;
            m->bits[1] = ~0;
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
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
    switch (format) {

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
    };
}

#define PCM_ERROR_MAX 128

/** A PCM handle.
 * @ingroup libtinyalsa-pcm
 */
struct pcm {
    /** The PCM's file descriptor */
    int fd;
    /** Flags that were passed to @ref pcm_open */
    unsigned int flags;
    /** Whether the PCM is running or not */
    int running:1;
    /** Whether or not the PCM has been prepared */
    int prepared:1;
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

static int oops(struct pcm *pcm, int e, const char *fmt, ...)
{
    va_list ap;
    int sz;

    va_start(ap, fmt);
    vsnprintf(pcm->error, PCM_ERROR_MAX, fmt, ap);
    va_end(ap);
    sz = strlen(pcm->error);

    if (errno)
        snprintf(pcm->error + sz, PCM_ERROR_MAX - sz,
                 ": %s", strerror(e));
    return -1;
}

/** Gets the buffer size of the PCM.
 * @param pcm A PCM handle.
 * @return The buffer size of the PCM.
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_get_buffer_size(const struct pcm *pcm)
{
    return pcm->buffer_size;
}

/** Gets the channel count of the PCM.
 * @param pcm A PCM handle.
 * @return The channel count of the PCM.
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_get_channels(const struct pcm *pcm)
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
const struct pcm_config * pcm_get_config(const struct pcm *pcm)
{
    if (pcm == NULL)
        return NULL;
    return &pcm->config;
}

/** Gets the rate of the PCM.
 * The rate is given in frames per second.
 * @param pcm A PCM handle.
 * @return The rate of the PCM.
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_get_rate(const struct pcm *pcm)
{
    return pcm->config.rate;
}

/** Gets the format of the PCM.
 * @param pcm A PCM handle.
 * @return The format of the PCM.
 * @ingroup libtinyalsa-pcm
 */
enum pcm_format pcm_get_format(const struct pcm *pcm)
{
    return pcm->config.format;
}

/** Gets the file descriptor of the PCM.
 * Useful for extending functionality of the PCM when needed.
 * @param pcm A PCM handle.
 * @return The file descriptor of the PCM.
 * @ingroup libtinyalsa-pcm
 */
int pcm_get_file_descriptor(const struct pcm *pcm)
{
    return pcm->fd;
}

/** Gets the error message for the last error that occured.
 * If no error occured and this function is called, the results are undefined.
 * @param pcm A PCM handle.
 * @return The error message of the last error that occured.
 * @ingroup libtinyalsa-pcm
 */
const char* pcm_get_error(const struct pcm *pcm)
{
    return pcm->error;
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
int pcm_set_config(struct pcm *pcm, const struct pcm_config *config)
{
    if (pcm == NULL)
        return -EFAULT;
    else if (config == NULL) {
        config = &pcm->config;
        pcm->config.channels = 2;
        pcm->config.rate = 48000;
        pcm->config.period_size = 1024;
        pcm->config.period_count = 4;
        pcm->config.format = PCM_FORMAT_S16_LE;
        pcm->config.start_threshold = config->period_count * config->period_size;
        pcm->config.stop_threshold = config->period_count * config->period_size;
        pcm->config.silence_threshold = 0;
    } else
        pcm->config = *config;

    struct snd_pcm_hw_params params;
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

    if (pcm->flags & PCM_NOIRQ) {

        if (!(pcm->flags & PCM_MMAP)) {
            oops(pcm, -EINVAL, "noirq only currently supported with mmap().");
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

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params)) {
        int errno_copy = errno;
        oops(pcm, -errno, "cannot set hw params");
        return -errno_copy;
    }

    /* get our refined hw_params */
    pcm->config.period_size = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    pcm->config.period_count = param_get_int(&params, SNDRV_PCM_HW_PARAM_PERIODS);
    pcm->buffer_size = config->period_count * config->period_size;

    if (pcm->flags & PCM_MMAP) {
        pcm->mmap_buffer = mmap(NULL, pcm_frames_to_bytes(pcm, pcm->buffer_size),
                                PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, pcm->fd, 0);
        if (pcm->mmap_buffer == MAP_FAILED) {
            int errno_copy = errno;
            oops(pcm, -errno, "failed to mmap buffer %d bytes\n",
                 pcm_frames_to_bytes(pcm, pcm->buffer_size));
            return -errno_copy;
        }
    }

    struct snd_pcm_sw_params sparams;
    memset(&sparams, 0, sizeof(sparams));
    sparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
    sparams.period_step = 1;
    sparams.avail_min = 1;

    if (!config->start_threshold) {
        if (pcm->flags & PCM_IN)
            pcm->config.start_threshold = sparams.start_threshold = 1;
        else
            pcm->config.start_threshold = sparams.start_threshold =
                config->period_count * config->period_size / 2;
    } else
        sparams.start_threshold = config->start_threshold;

    /* pick a high stop threshold - todo: does this need further tuning */
    if (!config->stop_threshold) {
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

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sparams)) {
        int errno_copy = errno;
        oops(pcm, -errno, "cannot set sw params");
        return -errno_copy;
    }

    return 0;
}

/** Gets the subdevice on which the pcm has been opened.
 * @param pcm A PCM handle.
 * @return The subdevice on which the pcm has been opened */
unsigned int pcm_get_subdevice(const struct pcm *pcm)
{
    return pcm->subdevice;
}

/** Determines the number of bits occupied by a @ref pcm_format.
 * @param format A PCM format.
 * @return The number of bits associated with @p format
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_format_to_bits(enum pcm_format format)
{
    switch (format) {
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
    };
}

/** Determines how many frames of a PCM can fit into a number of bytes.
 * @param pcm A PCM handle.
 * @param bytes The number of bytes.
 * @return The number of frames that may fit into @p bytes
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_bytes_to_frames(const struct pcm *pcm, unsigned int bytes)
{
    return bytes / (pcm->config.channels *
        (pcm_format_to_bits(pcm->config.format) >> 3));
}

/** Determines how many bytes are occupied by a number of frames of a PCM.
 * @param pcm A PCM handle.
 * @param frames The number of frames of a PCM.
 * @return The bytes occupied by @p frames.
 * @ingroup libtinyalsa-pcm
 */
unsigned int pcm_frames_to_bytes(const struct pcm *pcm, unsigned int frames)
{
    return frames * pcm->config.channels *
        (pcm_format_to_bits(pcm->config.format) >> 3);
}

static int pcm_sync_ptr(struct pcm *pcm, int flags)
{
    if (pcm->sync_ptr) {
        pcm->sync_ptr->flags = flags;
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr) < 0) {
            oops(pcm, errno, "failed to sync mmap ptr");
            return -1;
        }
        return 0;
    }
    return -1;
}

static int pcm_hw_mmap_status(struct pcm *pcm)
{
    if (pcm->sync_ptr)
        return 0;

    int page_size = sysconf(_SC_PAGE_SIZE);
    pcm->mmap_status = mmap(NULL, page_size, PROT_READ, MAP_FILE | MAP_SHARED,
                            pcm->fd, SNDRV_PCM_MMAP_OFFSET_STATUS);
    if (pcm->mmap_status == MAP_FAILED)
        pcm->mmap_status = NULL;
    if (!pcm->mmap_status)
        goto mmap_error;

    pcm->mmap_control = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                             MAP_FILE | MAP_SHARED, pcm->fd, SNDRV_PCM_MMAP_OFFSET_CONTROL);
    if (pcm->mmap_control == MAP_FAILED)
        pcm->mmap_control = NULL;
    if (!pcm->mmap_control) {
        munmap(pcm->mmap_status, page_size);
        pcm->mmap_status = NULL;
        goto mmap_error;
    }
    pcm->mmap_control->avail_min = 1;

    return 0;

mmap_error:

    pcm->sync_ptr = calloc(1, sizeof(*pcm->sync_ptr));
    if (!pcm->sync_ptr)
        return -ENOMEM;
    pcm->mmap_status = &pcm->sync_ptr->s.status;
    pcm->mmap_control = &pcm->sync_ptr->c.control;
    pcm->mmap_control->avail_min = 1;
    pcm_sync_ptr(pcm, 0);

    return 0;
}

static void pcm_hw_munmap_status(struct pcm *pcm) {
    if (pcm->sync_ptr) {
        free(pcm->sync_ptr);
        pcm->sync_ptr = NULL;
    } else {
        int page_size = sysconf(_SC_PAGE_SIZE);
        if (pcm->mmap_status)
            munmap(pcm->mmap_status, page_size);
        if (pcm->mmap_control)
            munmap(pcm->mmap_control, page_size);
    }
    pcm->mmap_status = NULL;
    pcm->mmap_control = NULL;
}

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

static int pcm_mmap_transfer_areas(struct pcm *pcm, char *buf,
                                unsigned int offset, unsigned int size)
{
    void *pcm_areas;
    int commit;
    unsigned int pcm_offset, frames, count = 0;

    while (size > 0) {
        frames = size;
        pcm_mmap_begin(pcm, &pcm_areas, &pcm_offset, &frames);
        pcm_areas_copy(pcm, pcm_offset, buf, offset, frames);
        commit = pcm_mmap_commit(pcm, pcm_offset, frames);
        if (commit < 0) {
            oops(pcm, commit, "failed to commit %d frames\n", frames);
            return commit;
        }

        offset += commit;
        count += commit;
        size -= commit;
    }
    return count;
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
int pcm_get_htimestamp(struct pcm *pcm, unsigned int *avail,
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
int pcm_writei(struct pcm *pcm, const void *data, unsigned int frame_count)
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

    x.buf = (void*)data;
    x.frames = frame_count;
    x.result = 0;
    for (;;) {
        if (!pcm->running) {
            int prepare_error = pcm_prepare(pcm);
            if (prepare_error)
                return prepare_error;
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x))
                return oops(pcm, errno, "cannot write initial data");
            pcm->running = 1;
            return 0;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x)) {
            pcm->prepared = 0;
            pcm->running = 0;
            if (errno == EPIPE) {
                /* we failed to make our window -- try to restart if we are
                 * allowed to do so.  Otherwise, simply allow the EPIPE error to
                 * propagate up to the app level */
                pcm->underruns++;
                if (pcm->flags & PCM_NORESTART)
                    return -EPIPE;
                continue;
            }
            return oops(pcm, errno, "cannot write stream data");
        }
        return x.result;
    }
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
int pcm_readi(struct pcm *pcm, void *data, unsigned int frame_count)
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

    x.buf = data;
    x.frames = frame_count;
    x.result = 0;
    for (;;) {
        if ((!pcm->running) && (pcm_start(pcm) < 0))
            return -errno;
        else if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x)) {
            pcm->prepared = 0;
            pcm->running = 0;
            if (errno == EPIPE) {
                    /* we failed to make our window -- try to restart */
                pcm->underruns++;
                continue;
            }
            return oops(pcm, errno, "cannot read stream data");
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
int pcm_write(struct pcm *pcm, const void *data, unsigned int count)
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
int pcm_read(struct pcm *pcm, void *data, unsigned int count)
{
    return pcm_readi(pcm, data, pcm_bytes_to_frames(pcm, count));
}

static struct pcm bad_pcm = {
    .fd = -1,
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
struct pcm_params *pcm_params_get(unsigned int card, unsigned int device,
                                  unsigned int flags)
{
    struct snd_pcm_hw_params *params;
    char fn[256];
    int fd;

    snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
             flags & PCM_IN ? 'c' : 'p');

    fd = open(fn, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "cannot open device '%s'\n", fn);
        goto err_open;
    }

    params = calloc(1, sizeof(struct snd_pcm_hw_params));
    if (!params)
        goto err_calloc;

    param_init(params);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params)) {
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
void pcm_params_free(struct pcm_params *pcm_params)
{
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;

    if (params)
        free(params);
}

static int pcm_param_to_alsa(enum pcm_param param)
{
    switch (param) {
    case PCM_PARAM_ACCESS:
        return SNDRV_PCM_HW_PARAM_ACCESS;
    case PCM_PARAM_FORMAT:
        return SNDRV_PCM_HW_PARAM_FORMAT;
    case PCM_PARAM_SUBFORMAT:
        return SNDRV_PCM_HW_PARAM_SUBFORMAT;
    case PCM_PARAM_SAMPLE_BITS:
        return SNDRV_PCM_HW_PARAM_SAMPLE_BITS;
        break;
    case PCM_PARAM_FRAME_BITS:
        return SNDRV_PCM_HW_PARAM_FRAME_BITS;
        break;
    case PCM_PARAM_CHANNELS:
        return SNDRV_PCM_HW_PARAM_CHANNELS;
        break;
    case PCM_PARAM_RATE:
        return SNDRV_PCM_HW_PARAM_RATE;
        break;
    case PCM_PARAM_PERIOD_TIME:
        return SNDRV_PCM_HW_PARAM_PERIOD_TIME;
        break;
    case PCM_PARAM_PERIOD_SIZE:
        return SNDRV_PCM_HW_PARAM_PERIOD_SIZE;
        break;
    case PCM_PARAM_PERIOD_BYTES:
        return SNDRV_PCM_HW_PARAM_PERIOD_BYTES;
        break;
    case PCM_PARAM_PERIODS:
        return SNDRV_PCM_HW_PARAM_PERIODS;
        break;
    case PCM_PARAM_BUFFER_TIME:
        return SNDRV_PCM_HW_PARAM_BUFFER_TIME;
        break;
    case PCM_PARAM_BUFFER_SIZE:
        return SNDRV_PCM_HW_PARAM_BUFFER_SIZE;
        break;
    case PCM_PARAM_BUFFER_BYTES:
        return SNDRV_PCM_HW_PARAM_BUFFER_BYTES;
        break;
    case PCM_PARAM_TICK_TIME:
        return SNDRV_PCM_HW_PARAM_TICK_TIME;
        break;

    default:
        return -1;
    }
}

/** Gets a mask from a PCM's hardware parameters.
 * @param pcm_params A PCM's hardware parameters.
 * @param param The parameter to get.
 * @return If @p pcm_params is NULL or @p param is not a mask, NULL is returned.
 *  Otherwise, the mask associated with @p param is returned.
 * @ingroup libtinyalsa-pcm
 */
const struct pcm_mask *pcm_params_get_mask(const struct pcm_params *pcm_params,
                                     enum pcm_param param)
{
    int p;
    struct snd_pcm_hw_params *params = (struct snd_pcm_hw_params *)pcm_params;
    if (params == NULL) {
        return NULL;
    }

    p = pcm_param_to_alsa(param);
    if (p < 0 || !param_is_mask(p)) {
        return NULL;
    }

    return (const struct pcm_mask *)param_to_mask(params, p);
}

/** Get the minimum of a specified PCM parameter.
 * @param pcm_params A PCM parameters structure.
 * @param param The specified parameter to get the minimum of.
 * @returns On success, the parameter minimum.
 *  On failure, zero.
 */
unsigned int pcm_params_get_min(const struct pcm_params *pcm_params,
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

/** Get the maximum of a specified PCM parameter.
 * @param pcm_params A PCM parameters structure.
 * @param param The specified parameter to get the maximum of.
 * @returns On success, the parameter maximum.
 *  On failure, zero.
 */
unsigned int pcm_params_get_max(const struct pcm_params *pcm_params,
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

/** Closes a PCM returned by @ref pcm_open.
 * @param pcm A PCM returned by @ref pcm_open.
 *  May not be NULL.
 * @return Always returns zero.
 * @ingroup libtinyalsa-pcm
 */
int pcm_close(struct pcm *pcm)
{
    if (pcm == &bad_pcm)
        return 0;

    pcm_hw_munmap_status(pcm);

    if (pcm->flags & PCM_MMAP) {
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
struct pcm *pcm_open_by_name(const char *name,
                             unsigned int flags,
                             const struct pcm_config *config)
{
  unsigned int card, device;
  if ((name[0] != 'h')
   || (name[1] != 'w')
   || (name[2] != ':')) {
    return NULL;
  } else if (sscanf(&name[3], "%u,%u", &card, &device) != 2) {
    return NULL;
  }
  return pcm_open(card, device, flags, config);
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
struct pcm *pcm_open(unsigned int card, unsigned int device,
                     unsigned int flags, const struct pcm_config *config)
{
    struct pcm *pcm;
    struct snd_pcm_info info;
    char fn[256];
    int rc;

    pcm = calloc(1, sizeof(struct pcm));
    if (!pcm)
        return &bad_pcm;

    snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, device,
             flags & PCM_IN ? 'c' : 'p');

    pcm->flags = flags;
    pcm->fd = open(fn, O_RDWR);
    if (pcm->fd < 0) {
        oops(pcm, errno, "cannot open device '%s'", fn);
        return pcm;
    }

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_INFO, &info)) {
        oops(pcm, errno, "cannot get info");
        goto fail_close;
    }
    pcm->subdevice = info.subdevice;

    if (pcm_set_config(pcm, config) != 0)
        goto fail_close;

    rc = pcm_hw_mmap_status(pcm);
    if (rc < 0) {
        oops(pcm, rc, "mmap status failed");
        goto fail;
    }

#ifdef SNDRV_PCM_IOCTL_TTSTAMP
    if (pcm->flags & PCM_MONOTONIC) {
        int arg = SNDRV_PCM_TSTAMP_TYPE_MONOTONIC;
        rc = ioctl(pcm->fd, SNDRV_PCM_IOCTL_TTSTAMP, &arg);
        if (rc < 0) {
            oops(pcm, rc, "cannot set timestamp type");
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

/** Checks if a PCM file has been opened without error.
 * @param pcm A PCM handle.
 *  May be NULL.
 * @return If a PCM's file descriptor is not valid or the pointer is NULL, it returns zero.
 *  Otherwise, the function returns one.
 * @ingroup libtinyalsa-pcm
 */
int pcm_is_ready(const struct pcm *pcm)
{
    if (pcm != NULL) {
        return pcm->fd >= 0;
    }
    return 0;
}

/** Links two PCMs.
 * After this function is called, the two PCMs will prepare, start and stop in sync (at the same time).
 * If an error occurs, the error message will be written to @p pcm1.
 * @param pcm1 A PCM handle.
 * @param pcm2 Another PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
int pcm_link(struct pcm *pcm1, struct pcm *pcm2)
{
    int err = ioctl(pcm1->fd, SNDRV_PCM_IOCTL_LINK, pcm2->fd);
    if (err == -1) {
        return oops(pcm1, errno, "cannot link PCM");
    }
    return 0;
}

/** Unlinks a PCM.
 * @see @ref pcm_link
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
int pcm_unlink(struct pcm *pcm)
{
    int err = ioctl(pcm->fd, SNDRV_PCM_IOCTL_UNLINK);
    if (err == -1) {
        return oops(pcm, errno, "cannot unlink PCM");
    }
    return 0;
}

/** Prepares a PCM, if it has not been prepared already.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
int pcm_prepare(struct pcm *pcm)
{
    if (pcm->prepared)
        return 0;

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE) < 0)
        return oops(pcm, errno, "cannot prepare channel");

    pcm->prepared = 1;
    return 0;
}

/** Starts a PCM.
 * If the PCM has not been prepared,
 * it is prepared in this function.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
int pcm_start(struct pcm *pcm)
{
    int prepare_error = pcm_prepare(pcm);
    if (prepare_error)
        return prepare_error;

    if (pcm->flags & PCM_MMAP)
        pcm_sync_ptr(pcm, 0);

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START) < 0)
        return oops(pcm, errno, "cannot start channel");

    pcm->running = 1;
    return 0;
}

/** Stops a PCM.
 * @param pcm A PCM handle.
 * @return On success, zero; on failure, a negative number.
 * @ingroup libtinyalsa-pcm
 */
int pcm_stop(struct pcm *pcm)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DROP) < 0)
        return oops(pcm, errno, "cannot stop channel");

    pcm->prepared = 0;
    pcm->running = 0;
    return 0;
}

static inline int pcm_mmap_playback_avail(struct pcm *pcm)
{
    int avail;

    avail = pcm->mmap_status->hw_ptr + pcm->buffer_size - pcm->mmap_control->appl_ptr;

    if (avail < 0)
        avail += pcm->boundary;
    else if (avail >= (int)pcm->boundary)
        avail -= pcm->boundary;

    return avail;
}

static inline int pcm_mmap_capture_avail(struct pcm *pcm)
{
    int avail = pcm->mmap_status->hw_ptr - pcm->mmap_control->appl_ptr;
    if (avail < 0)
        avail += pcm->boundary;
    return avail;
}

static inline int pcm_mmap_avail(struct pcm *pcm)
{
    pcm_sync_ptr(pcm, SNDRV_PCM_SYNC_PTR_HWSYNC);
    if (pcm->flags & PCM_IN)
        return pcm_mmap_capture_avail(pcm);
    else
        return pcm_mmap_playback_avail(pcm);
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

int pcm_mmap_begin(struct pcm *pcm, void **areas, unsigned int *offset,
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

int pcm_mmap_commit(struct pcm *pcm, unsigned int offset, unsigned int frames)
{
    int ret;

    /* not used */
    (void) offset;

    /* update the application pointer in userspace and kernel */
    pcm_mmap_appl_forward(pcm, frames);
    ret = pcm_sync_ptr(pcm, 0);
    if (ret != 0){
        printf("%d\n", ret);
        return ret;
    }

    return frames;
}

int pcm_avail_update(struct pcm *pcm)
{
    pcm_sync_ptr(pcm, 0);
    return pcm_mmap_avail(pcm);
}

int pcm_state(struct pcm *pcm)
{
    int err = pcm_sync_ptr(pcm, 0);
    if (err < 0)
        return err;

    return pcm->mmap_status->state;
}

/** Waits for frames to be available for read or write operations.
 * @param pcm A PCM handle.
 * @param timeout The maximum amount of time to wait for, in terms of milliseconds.
 * @returns If frames became available, one is returned.
 *  If a timeout occured, zero is returned.
 *  If an error occured, a negative number is returned.
 * @ingroup libtinyalsa-pcm
 */
int pcm_wait(struct pcm *pcm, int timeout)
{
    struct pollfd pfd;
    int err;

    pfd.fd = pcm->fd;
    pfd.events = POLLIN | POLLOUT | POLLERR | POLLNVAL;

    do {
        /* let's wait for avail or timeout */
        err = poll(&pfd, 1, timeout);
        if (err < 0)
            return -errno;

        /* timeout ? */
        if (err == 0)
            return 0;

        /* have we been interrupted ? */
        if (errno == -EINTR)
            continue;

        /* check for any errors */
        if (pfd.revents & (POLLERR | POLLNVAL)) {
            switch (pcm_state(pcm)) {
            case PCM_STATE_XRUN:
                return -EPIPE;
            case PCM_STATE_SUSPENDED:
                return -ESTRPIPE;
            case PCM_STATE_DISCONNECTED:
                return -ENODEV;
            default:
                return -EIO;
            }
        }
    /* poll again if fd not ready for IO */
    } while (!(pfd.revents & (POLLIN | POLLOUT)));

    return 1;
}

int pcm_mmap_transfer(struct pcm *pcm, const void *buffer, unsigned int bytes)
{
    int err = 0, frames, avail;
    unsigned int offset = 0, count;

    if (bytes == 0)
        return 0;

    count = pcm_bytes_to_frames(pcm, bytes);

    while (count > 0) {

        /* get the available space for writing new frames */
        avail = pcm_avail_update(pcm);
        if (avail < 0) {
            fprintf(stderr, "cannot determine available mmap frames");
            return err;
        }

        /* start the audio if we reach the threshold */
        if (!pcm->running &&
            (pcm->buffer_size - avail) >= pcm->config.start_threshold) {
            if (pcm_start(pcm) < 0) {
               fprintf(stderr, "start error: hw 0x%x app 0x%x avail 0x%x\n",
                    (unsigned int)pcm->mmap_status->hw_ptr,
                    (unsigned int)pcm->mmap_control->appl_ptr,
                    avail);
                return -errno;
            }
        }

        /* sleep until we have space to write new frames */
        if (pcm->running &&
            (unsigned int)avail < pcm->mmap_control->avail_min) {
            int time = -1;

            if (pcm->flags & PCM_NOIRQ)
                time = (pcm->buffer_size - avail - pcm->mmap_control->avail_min)
                        / pcm->noirq_frames_per_msec;

            err = pcm_wait(pcm, time);
            if (err < 0) {
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
        if (frames < 0) {
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

int pcm_mmap_write(struct pcm *pcm, const void *data, unsigned int count)
{
    if ((~pcm->flags) & (PCM_OUT | PCM_MMAP))
        return -ENOSYS;

    return pcm_mmap_transfer(pcm, (void *)data, count);
}

int pcm_mmap_read(struct pcm *pcm, void *data, unsigned int count)
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
long pcm_get_delay(struct pcm *pcm)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DELAY, &pcm->pcm_delay) < 0)
        return -1;

    return pcm->pcm_delay;
}

