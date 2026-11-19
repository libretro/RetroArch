/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Real null PCM, scripted acceptance, device delay and queue-loss reports. */
#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_alsa;

static bool presentation_script;
static snd_pcm_state_t presentation_state = SND_PCM_STATE_RUNNING;
static snd_pcm_sframes_t presentation_delay;
static int presentation_delay_error, presentation_write, presentation_recoveries;

static void presentation_require(bool condition, const char *message)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", message);
      exit(EXIT_FAILURE);
   }
}

snd_pcm_state_t __real_snd_pcm_state(snd_pcm_t *pcm);
snd_pcm_state_t __wrap_snd_pcm_state(snd_pcm_t *pcm)
{
   return presentation_script ? presentation_state : __real_snd_pcm_state(pcm);
}

int __real_snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delay);
int __wrap_snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delay)
{
   if (!presentation_script) return __real_snd_pcm_delay(pcm, delay);
   *delay = presentation_delay;
   return presentation_delay_error;
}

snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t *pcm, const void *data, snd_pcm_uframes_t frames);
snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t *pcm, const void *data, snd_pcm_uframes_t frames)
{
   int result = presentation_write;
   if (!presentation_script) return __real_snd_pcm_writei(pcm, data, frames);
   (void)pcm;
   (void)data;
   presentation_write = -EAGAIN;
   if (result < 0) return result;
   return (snd_pcm_sframes_t)frames < result ? (snd_pcm_sframes_t)frames : result;
}

int __wrap_snd_pcm_recover(snd_pcm_t *pcm, int error, int silent)
{
   (void)pcm;
   (void)error;
   (void)silent;
   presentation_state = SND_PCM_STATE_PREPARED;
   presentation_recoveries++;
   return 0;
}

static audio_presentation_position_t presentation_position(void *device)
{
   audio_presentation_position_t position;
   presentation_require(audio_alsa.presentation_position(device, &position), "ALSA presentation unsupported");
   return position;
}

int main(void)
{
   static float silence[256];
   audio_presentation_position_t position;
   void *device;
   unsigned rate = 0;
   size_t frame_bytes;
   uint64_t epoch;
   device = audio_alsa.init("null", 48000, 64, &rate);
   presentation_require(device != NULL, "real null PCM initialization");
   frame_bytes = 2 * (audio_alsa.use_float(device) ? sizeof(float) : sizeof(int16_t));
   audio_alsa.set_nonblock_state(device, true);
   presentation_script = true;
   presentation_write = 50;
   presentation_require(audio_alsa.write(device, silence, 100 * frame_bytes)
         == (ssize_t)(50 * frame_bytes), "partial acceptance");
   presentation_delay = 20;
   position = presentation_position(device);
   epoch = position.epoch;
   presentation_require(position.valid && !position.drained && !position.failed
         && position.written == 50 && position.played == 30, "actual accepted range less queued frames");
   presentation_delay_error = -EIO;
   position = presentation_position(device);
   presentation_require(!position.valid && !position.drained, "delay query failure must not advance playback");
   presentation_delay_error = 0;
   presentation_delay = -1;
   position = presentation_position(device);
   presentation_require(!position.valid && !position.drained, "negative delay must not prove drain");
   presentation_state = SND_PCM_STATE_XRUN;
   presentation_delay = 0;
   position = presentation_position(device);
   presentation_require(!position.valid && !position.drained, "XRUN must not play a dropped queue");
   presentation_write = -EPIPE;
   presentation_require(audio_alsa.write(device, silence, 100 * frame_bytes) == 0, "XRUN recovery");
   presentation_require(presentation_recoveries == 1, "native recovery call");
   presentation_state = SND_PCM_STATE_RUNNING;
   position = presentation_position(device);
   presentation_require(position.epoch != epoch && position.written == 50
         && !position.valid && !position.drained, "discarded tail invalid in new epoch");
   epoch = position.epoch;
   presentation_write = 25;
   presentation_require(audio_alsa.write(device, silence, 100 * frame_bytes)
         == (ssize_t)(25 * frame_bytes), "new-epoch acceptance");
   presentation_delay = 5;
   position = presentation_position(device);
   presentation_require(position.valid && position.epoch == epoch
         && position.played == 70 && position.written == 75, "new-epoch presentation");
   presentation_delay = 26;
   position = presentation_position(device);
   presentation_require(!position.valid && !position.drained, "unmapped pre-epoch backlog");
   presentation_delay = 0;
   position = presentation_position(device);
   presentation_require(position.valid && position.drained && position.played == 75, "accepted tail drain");
   presentation_write = -EINTR;
   audio_alsa.write(device, silence, frame_bytes);
   position = presentation_position(device);
   presentation_require(position.epoch == epoch, "signal interruption preserves epoch");
   presentation_write = -ESTRPIPE;
   audio_alsa.write(device, silence, frame_bytes);
   presentation_state = SND_PCM_STATE_RUNNING;
   position = presentation_position(device);
   presentation_require(position.epoch != epoch && !position.valid && !position.drained, "suspend invalidates old ranges");
   presentation_state = SND_PCM_STATE_DISCONNECTED;
   position = presentation_position(device);
   presentation_require(position.failed && !position.valid && !position.drained, "device removal");
   presentation_script = false;
   audio_alsa.free(device);
   /* Actual null PCM startup, without scripted state or delay. It may
    * accept writes forever in PREPARED until explicitly started. */
   device = audio_alsa.init("null", 48000, 64, &rate);
   presentation_require(device != NULL, "native null PCM reopen");
   {
      unsigned writes;
      bool running = false;
      for (writes = 0; writes < 64 && !running; writes++)
      {
         ssize_t accepted = audio_alsa.write(device, silence, sizeof(silence));
         presentation_require(accepted > 0, "native null PCM write");
         position = presentation_position(device);
         presentation_require(!position.failed, "native null PCM start");
         running = position.valid;
      }
      presentation_require(running && position.drained
            && position.played == position.written, "native null PCM presentation/drain");
   }
   audio_alsa.free(device);
   puts("ALSA presentation: partial writes, delay failures, XRUN/suspend epochs, tail drain and device removal PASS");
   return EXIT_SUCCESS;
}
