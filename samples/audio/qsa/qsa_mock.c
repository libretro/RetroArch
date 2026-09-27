/* The scripted device behind the mock <sys/asoundlib.h>. */
#include <stdlib.h>
#include <stdio.h>
#include "sys/asoundlib.h"

struct snd_pcm { int open; };

static struct snd_pcm g_pcm;
static int    g_handles, g_nonblock, g_paused, g_prepares, g_status;
static int    g_max_fragment = 4096, g_open_error, g_params_error, g_rate;
static size_t g_capacity, g_queued, g_written;

void qsa_mock_reset(void)
{
   memset(&g_pcm, 0, sizeof(g_pcm));
   g_handles = g_nonblock = g_paused = g_prepares = 0;
   g_status  = SND_PCM_STATUS_RUNNING;
   g_max_fragment = 4096; g_open_error = g_params_error = g_rate = 0;
   g_capacity = 16384; g_queued = g_written = 0;
}
void qsa_mock_set_max_fragment(int bytes) { g_max_fragment = bytes; }
void qsa_mock_set_open_error(int err)     { g_open_error = err; }
void qsa_mock_set_params_error(int err)   { g_params_error = err; }
void qsa_mock_set_status(int status)      { g_status = status; }
size_t qsa_mock_written(void)             { return g_written; }
size_t qsa_mock_queued(void)              { return g_queued; }
int  qsa_mock_prepares(void)              { return g_prepares; }
int  qsa_mock_paused(void)                { return g_paused; }
int  qsa_mock_open_handles(void)          { return g_handles; }
int  qsa_mock_device_rate(void)           { return g_rate; }
void qsa_mock_drain(size_t bytes)
{
   g_queued = (bytes >= g_queued) ? 0 : g_queued - bytes;
}

const char *snd_strerror(int errnum) { (void)errnum; return "mock error"; }

int snd_pcm_open_preferred(snd_pcm_t **pcm, int *card, int *dev, int mode)
{
   (void)mode;
   if (g_open_error)
      return g_open_error;
   if (card) *card = 0;
   if (dev)  *dev  = 0;
   g_pcm.open = 1;
   g_handles++;
   *pcm = &g_pcm;
   return EOK;
}

int snd_pcm_close(snd_pcm_t *pcm) { (void)pcm; g_handles--; return EOK; }
int snd_pcm_nonblock_mode(snd_pcm_t *pcm, int nonblock) { (void)pcm; g_nonblock = nonblock; return EOK; }

int snd_pcm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
   (void)pcm;
   info->max_fragment_size = g_max_fragment;
   return EOK;
}

int snd_pcm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t *params)
{
   (void)pcm;
   if (g_params_error)
      return g_params_error;
   g_rate     = params->format.rate;
   /* the device holds frags_max fragments */
   g_capacity = (size_t)params->buf.block.frag_size * params->buf.block.frags_max;
   return EOK;
}

int snd_pcm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t *setup)
{
   (void)pcm;
   setup->buf.block.frag_size = g_max_fragment;
   setup->buf.block.frags     = 8;
   return EOK;
}

int snd_pcm_channel_prepare(snd_pcm_t *pcm, int channel)
{
   (void)pcm; (void)channel;
   g_prepares++;
   g_status = SND_PCM_STATUS_RUNNING;
   g_queued = 0;
   return EOK;
}

int snd_pcm_channel_status(snd_pcm_t *pcm, snd_pcm_channel_status_t *status)
{
   (void)pcm;
   status->status = g_status;
   return EOK;
}

int snd_pcm_playback_pause(snd_pcm_t *pcm)  { (void)pcm; g_paused = 1; return EOK; }
int snd_pcm_playback_resume(snd_pcm_t *pcm) { (void)pcm; g_paused = 0; return EOK; }

long snd_pcm_write(snd_pcm_t *pcm, const void *buf, size_t len)
{
   (void)pcm; (void)buf;
   if (g_paused)
      return -EAGAIN;
   /* a device in any state but running refuses, which is how the
    * driver is told to ask for the status and recover */
   if (g_status != SND_PCM_STATUS_RUNNING)
      return -EIO;
   if (g_queued + len > g_capacity)
   {
      if (g_nonblock)
         return -EAGAIN;
      /* a blocking device would sleep; here it plays what it must */
      qsa_mock_drain(g_queued + len - g_capacity);
   }
   g_queued  += len;
   g_written += len;
   return (long)len;
}
