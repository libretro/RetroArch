/*  RetroArch - A frontend for libretro.
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
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <switch.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#include "../../tasks/tasks_internal.h"

typedef struct
{
      bool blocking;
      bool is_paused;
      uint64_t last_append;
      unsigned latency;
      AudioOutBuffer buffers[5];
      AudioOutBuffer *current_buffer;
} switch_audio_t;

static bool switch_tasks_finder(retro_task_t *task, void *userdata)
{
      return task;
}
task_finder_data_t switch_tasks_finder_data = {switch_tasks_finder, NULL};

#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define FRAMERATE (1000 / 30)
#define SAMPLECOUNT SAMPLERATE / FRAMERATE
#define BYTESPERSAMPLE sizeof(uint16_t)

static uint32_t switch_audio_data_size()
{
      return (SAMPLECOUNT * CHANNELCOUNT * BYTESPERSAMPLE);
}

static size_t switch_audio_buffer_size(void *data)
{
      (void)data;
      return (switch_audio_data_size() + 0xfff) & ~0xfff;
}

static ssize_t switch_audio_write(void *data, const void *buf, size_t size)
{
      size_t to_write = size;
      switch_audio_t *swa = (switch_audio_t *)data;

      if (!swa)
      {
            return -1;
      }

      if (!swa->current_buffer)
      {
            uint32_t num;
            if (R_FAILED(audoutGetReleasedAudioOutBuffer(&swa->current_buffer, &num)))
            {
                  return -1;
            }

            if (num < 1)
            {
                  swa->current_buffer = NULL;

                  if (swa->blocking)
                  {
                        //RARCH_LOG("No buffer, blocking...\n");

                        while (swa->current_buffer == NULL)
                        {
                              num = 0;
                              if (R_FAILED(audoutWaitPlayFinish(&swa->current_buffer, &num, U64_MAX)))
                              {
                                    //if (task_queue_find(&switch_tasks_finder_data))
                                    //{
                                    //      task_queue_check();
                                    //}
                              }
                        }
                  }
                  else
                  {
                        return 0;
                  }
            }

            swa->current_buffer->data_size = 0;
      }

      if (to_write > switch_audio_buffer_size(NULL) - swa->current_buffer->data_size)
            to_write = switch_audio_buffer_size(NULL) - swa->current_buffer->data_size;

      memcpy(((uint8_t *)swa->current_buffer->buffer) + swa->current_buffer->data_size, buf, to_write);
      swa->current_buffer->data_size += to_write;
      swa->current_buffer->buffer_size = switch_audio_buffer_size(NULL);

      if (swa->current_buffer->data_size > (48000 * swa->latency) / 1000)
      {
            Result r = audoutAppendAudioOutBuffer(swa->current_buffer);
            if (R_FAILED(r))
            {
                  return -1;
            }
            swa->current_buffer = NULL;
      }

      swa->last_append = svcGetSystemTick();

      return to_write;
}

static bool switch_audio_stop(void *data)
{
      return true;

      switch_audio_t *swa = (switch_audio_t *)data;
      if (!swa)
            return false;

      if (!swa->is_paused)
      {
            Result rc = audoutStopAudioOut();
            if (R_FAILED(rc))
            {
                  return false;
            }
      }

      swa->is_paused = true;
      return true;
}

static bool switch_audio_start(void *data, bool is_shutdown)
{
      return true;

      switch_audio_t *swa = (switch_audio_t *)data;
      if (!swa)
            return false;

      if (swa->is_paused)
      {
            Result rc = audoutStartAudioOut();
            if (R_FAILED(rc))
            {
                  return false;
            }
      }

      swa->is_paused = false;
      return true;
}

static bool switch_audio_alive(void *data)
{
      switch_audio_t *swa = (switch_audio_t *)data;
      if (!swa)
            return false;
      return !swa->is_paused;
}

static void switch_audio_free(void *data)
{
      switch_audio_t *swa = (switch_audio_t *)data;

      if (swa)
      {
            if (!swa->is_paused)
                  audoutStopAudioOut();

            audoutExit();

            for (int i = 0; i < 5; i++)
                  free(swa->buffers[i].buffer);

            free(swa);
      }
}

static bool switch_audio_use_float(void *data)
{
      (void)data;
      return false; /* force INT16 */
}

static size_t switch_audio_write_avail(void *data)
{
      switch_audio_t *swa = (switch_audio_t *)data;

      if (!swa || !swa->current_buffer)
            return 0;

      return swa->current_buffer->buffer_size;
}

static void switch_audio_set_nonblock_state(void *data, bool state)
{
      switch_audio_t *swa = (switch_audio_t *)data;

      if (swa)
            swa->blocking = !state;
}

static void *switch_audio_init(const char *device,
                               unsigned rate, unsigned latency,
                               unsigned block_frames,
                               unsigned *new_rate)
{
      switch_audio_t *swa = (switch_audio_t *)calloc(1, sizeof(*swa));
      if (!swa)
            return NULL;

      // Init Audio Output
      Result rc = audoutInitialize();
      if (R_FAILED(rc))
      {
            goto cleanExit;
      }

      rc = audoutStartAudioOut();
      if (R_FAILED(rc))
      {
            goto cleanExit;
      }

      // Create Buffers
      for (int i = 0; i < 5; i++)
      {
            swa->buffers[i].next = NULL; // Unused
            swa->buffers[i].buffer = memalign(0x1000, switch_audio_buffer_size(NULL));
            swa->buffers[i].buffer_size = switch_audio_buffer_size(NULL);
            swa->buffers[i].data_size = switch_audio_data_size();
            swa->buffers[i].data_offset = 0;

            memset(swa->buffers[i].buffer, 0, switch_audio_buffer_size(NULL));

            audoutAppendAudioOutBuffer(&swa->buffers[i]);
      }

      // Set audio rate
      *new_rate = audoutGetSampleRate();

      swa->is_paused = false;
      swa->current_buffer = NULL;
      swa->latency = latency;
      swa->last_append = svcGetSystemTick();
      swa->blocking = block_frames;

      printf("[Audio]: Audio initialized\n");

      return swa;

cleanExit:;

      if (swa)
            free(swa);

      printf("[Audio]: Something failed in Audio Init!\n");

      return NULL;
}

audio_driver_t audio_switch = {
    switch_audio_init,
    switch_audio_write,
    switch_audio_stop,
    switch_audio_start,
    switch_audio_alive,
    switch_audio_set_nonblock_state,
    switch_audio_free,
    switch_audio_use_float,
    "switch",
    NULL, /* device_list_new */
    NULL, /* device_list_free */
    switch_audio_write_avail,
    switch_audio_buffer_size, /* buffer_size */
};
