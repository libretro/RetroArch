/* A stand-in for QNX's QSA (<sys/asoundlib.h>), enough of it to build
 * and run audio/drivers/alsa_qsa.c on a host: the types, constants and
 * entry points the driver uses, over a scripted device that counts what
 * it was given and drains on a clock. The real header is QNX's; this
 * exists so the driver has coverage on a machine that is not one.
 *
 * The behaviours modelled are the ones the driver depends on: a
 * preferred card, a fragment size the device chooses, non-blocking
 * writes that return -EAGAIN when the device is full, underrun and
 * change statuses, and pause/resume. */
#ifndef QSA_MOCK_ASOUNDLIB_H
#define QSA_MOCK_ASOUNDLIB_H

#include <stddef.h>
#include <string.h>
#include <errno.h>

#ifndef EOK
#define EOK 0
#endif

#define SND_PCM_OPEN_PLAYBACK      0
#define SND_PCM_CHANNEL_PLAYBACK   0
#define SND_PCM_MODE_BLOCK         0
#define SND_PCM_SFMT_S16_LE        2
#define SND_PCM_START_FULL         1
#define SND_PCM_STOP_STOP          0

#define SND_PCM_STATUS_RUNNING     0
#define SND_PCM_STATUS_UNDERRUN    1
#define SND_PCM_STATUS_OVERRUN     2
#define SND_PCM_STATUS_CHANGE      3
#define SND_PCM_STATUS_UNSECURE    4

typedef struct snd_pcm snd_pcm_t;

typedef struct
{
   int channel;
   int max_fragment_size;
} snd_pcm_channel_info_t;

typedef struct
{
   int channel;
   int mode;
   int start_mode;
   int stop_mode;
   struct { int interleave; int format; int rate; int voices; } format;
   struct { struct { int frag_size; int frags_min; int frags_max; } block; } buf;
} snd_pcm_channel_params_t;

typedef struct
{
   int channel;
   struct { struct { int frag_size; int frags; } block; } buf;
} snd_pcm_channel_setup_t;

typedef struct
{
   int channel;
   int status;
} snd_pcm_channel_status_t;

/* the mock's own controls, for the harness */
void qsa_mock_reset(void);
void qsa_mock_set_max_fragment(int bytes);
void qsa_mock_set_open_error(int err);
void qsa_mock_set_params_error(int err);
void qsa_mock_drain(size_t bytes);          /* the device plays this much */
size_t qsa_mock_written(void);              /* bytes the device accepted */
size_t qsa_mock_queued(void);               /* bytes it still holds */
void qsa_mock_set_status(int status);       /* the next status report */
int  qsa_mock_prepares(void);               /* times the driver prepared */
int  qsa_mock_paused(void);
int  qsa_mock_open_handles(void);           /* opened less closed */
int  qsa_mock_device_rate(void);            /* the rate the params asked for */

const char *snd_strerror(int errnum);
int snd_pcm_open_preferred(snd_pcm_t **pcm, int *card, int *dev, int mode);
int snd_pcm_close(snd_pcm_t *pcm);
int snd_pcm_nonblock_mode(snd_pcm_t *pcm, int nonblock);
int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info);
int snd_pcm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params);
int snd_pcm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup);
int snd_pcm_channel_prepare(snd_pcm_t *pcm, int channel);
int snd_pcm_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status);
int snd_pcm_playback_pause(snd_pcm_t *pcm);
int snd_pcm_playback_resume(snd_pcm_t *pcm);
long snd_pcm_write(snd_pcm_t *pcm, const void *buf, size_t len);

#endif
