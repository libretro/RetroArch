/* Compile-only stand-in for QNX's <sys/asoundlib.h>, the QSA audio
 * API. Enough of it for audio/drivers/alsa_qsa.c to be held to a
 * syntax gate on a host: QNX headers ship with the toolchain and
 * nothing else in the tree compiles that driver.
 *
 * Shapes and spellings follow QSA, not ALSA. Values are arbitrary
 * where the driver only compares or assigns them. */
#ifndef QNXSTUB_SYS_ASOUNDLIB_H
#define QNXSTUB_SYS_ASOUNDLIB_H

#include <stddef.h>
#include <stdint.h>

#define EOK 0

#define SND_PCM_OPEN_PLAYBACK      0x0001
#define SND_PCM_CHANNEL_PLAYBACK   0
#define SND_PCM_CHANNEL_CAPTURE    1
#define SND_PCM_MODE_BLOCK         0
#define SND_PCM_START_FULL         2
#define SND_PCM_STOP_STOP          0

#define SND_PCM_SFMT_S16_LE        2
#define SND_PCM_SFMT_FLOAT_LE      12

#define SND_PCM_FMT_S16_LE         (1 << 1)
#define SND_PCM_FMT_FLOAT_LE       (1 << 2)

#define SND_PCM_STATUS_RUNNING     1
#define SND_PCM_STATUS_UNDERRUN    2
#define SND_PCM_STATUS_OVERRUN     3
#define SND_PCM_STATUS_UNSECURE    5
#define SND_PCM_STATUS_CHANGE      7

typedef struct snd_pcm snd_pcm_t;
typedef long snd_pcm_sframes_t;

typedef struct
{
   int32_t  channel;
   uint32_t formats;
   uint32_t rates;
   int32_t  min_rate;
   int32_t  max_rate;
   int32_t  min_voices;
   int32_t  max_voices;
   int32_t  max_fragment_size;
} snd_pcm_channel_info_t;

typedef struct
{
   int32_t format;
   int32_t interleave;
   int32_t rate;
   int32_t voices;
} snd_pcm_format_t;

typedef struct
{
   int32_t channel;
   int32_t mode;
   int32_t start_mode;
   int32_t stop_mode;
   union
   {
      struct
      {
         int32_t frag_size;
         int32_t frags_min;
         int32_t frags_max;
      } block;
   } buf;
   snd_pcm_format_t format;
} snd_pcm_channel_params_t;

typedef struct
{
   int32_t          channel;
   int32_t          mode;
   snd_pcm_format_t format;
   union
   {
      struct
      {
         int32_t frag_size;
         int32_t frags;
      } block;
   } buf;
} snd_pcm_channel_setup_t;

typedef struct
{
   int32_t channel;
   int32_t status;
   uint32_t scount;
   int32_t free;
   int32_t underrun;
   int32_t overrun;
} snd_pcm_channel_status_t;

int snd_pcm_open_preferred(snd_pcm_t **handle, int *card, int *device, int mode);
int snd_pcm_close(snd_pcm_t *handle);
int snd_pcm_nonblock_mode(snd_pcm_t *handle, int nonblock);
int snd_pcm_channel_info(snd_pcm_t *handle, snd_pcm_channel_info_t *info);
int snd_pcm_channel_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params);
int snd_pcm_channel_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup);
int snd_pcm_channel_status(snd_pcm_t *handle, snd_pcm_channel_status_t *status);
int snd_pcm_channel_prepare(snd_pcm_t *handle, int channel);
int snd_pcm_playback_pause(snd_pcm_t *handle);
int snd_pcm_playback_resume(snd_pcm_t *handle);
snd_pcm_sframes_t snd_pcm_write(snd_pcm_t *handle, const void *buf, size_t size);
const char *snd_strerror(int errnum);

#endif
