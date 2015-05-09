/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "video_monitor.h"
#include "../general.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../performance.h"

void video_monitor_adjust_system_rates(void)
{
   float timing_skew;
   global_t *global = global_get_ptr();
   const struct retro_system_timing *info = 
      (const struct retro_system_timing*)&global->system.av_info.timing;
   settings_t *settings = config_get_ptr();

   global->system.force_nonblock = false;

   if (info->fps <= 0.0)
      return;

   timing_skew = fabs(1.0f - info->fps / settings->video.refresh_rate);

   /* We don't want to adjust pitch too much. If we have extreme cases,
    * just don't readjust at all. */
   if (timing_skew <= settings->audio.max_timing_skew)
      return;

   RARCH_LOG("Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
         settings->video.refresh_rate,
         (float)info->fps);

   if (info->fps <= settings->video.refresh_rate)
      return;

   /* We won't be able to do VSync reliably when game FPS > monitor FPS. */
   global->system.force_nonblock = true;
   RARCH_LOG("Game FPS > Monitor FPS. Cannot rely on VSync.\n");
}

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   rarch_main_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);

   settings->video.refresh_rate = hz;
}

/**
 * video_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
void video_monitor_compute_fps_statistics(void)
{
   double avg_fps = 0.0, stddev = 0.0;
   unsigned samples = 0;
   runloop_t *runloop   = rarch_main_get_ptr();
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (runloop->measure_data.frame_time_samples_count <
         2 * MEASURE_FRAME_TIME_SAMPLES_COUNT)
   {
      RARCH_LOG(
            "Does not have enough samples for monitor refresh rate estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("Average monitor Hz: %.6f Hz. (%.3f %% frame time deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points)
{
   unsigned i;
   retro_time_t accum   = 0, avg, accum_var = 0;
   unsigned samples     = 0;
   runloop_t *runloop   = rarch_main_get_ptr();
   settings_t *settings = config_get_ptr();
   
   samples = min(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         runloop->measure_data.frame_time_samples_count);

   if (settings->video.threaded || (samples < 2))
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
      accum += runloop->measure_data.frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)runloop->measure_data.frame_time_samples[i]);
#endif

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = runloop->measure_data.frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation     = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate  = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}

#ifndef TIME_TO_FPS
#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))
#endif

#define FPS_UPDATE_INTERVAL 256

/**
 * video_monitor_get_fps:
 * @buf           : string suitable for Window title
 * @size          : size of buffer.
 * @buf_fps       : string of raw FPS only (optional).
 * @size_fps      : size of raw FPS buffer.
 *
 * Get the amount of frames per seconds.
 *
 * Returns: true if framerate per seconds could be obtained,
 * otherwise false.
 *
 **/

#ifdef _WIN32
#define U64_SIGN "%I64u"
#else
#define U64_SIGN "%llu"
#endif

bool video_monitor_get_fps(char *buf, size_t size,
      char *buf_fps, size_t size_fps)
{
   static float last_fps;
   retro_time_t        new_time;
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   uint64_t frame_count = video_driver_get_frame_count();
   runloop_t *runloop   = rarch_main_get_ptr();
   global_t  *global    = global_get_ptr();

   *buf = '\0';

   new_time = rarch_get_time_usec();

   if (frame_count)
   {
      bool ret = false;
      unsigned write_index = 
         runloop->measure_data.frame_time_samples_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      runloop->measure_data.frame_time_samples[write_index] = 
         new_time - fps_time;
      fps_time = new_time;

      if ((frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);
         curr_time = new_time;

         snprintf(buf, size, "%s || FPS: %6.1f || Frames: " U64_SIGN,
               global->title_buf, last_fps, (unsigned long long)frame_count);
         ret = true;
      }

      if (buf_fps)
         snprintf(buf_fps, size_fps, "FPS: %6.1f || Frames: " U64_SIGN,
               last_fps, (unsigned long long)frame_count);

      return ret;
   }

   curr_time = fps_time = new_time;
   strlcpy(buf, global->title_buf, size);
   if (buf_fps)
      strlcpy(buf_fps, "N/A", size_fps);

   return true;
}
