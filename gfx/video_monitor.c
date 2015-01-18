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
#include "../driver.h"
#include "../general.h"
#include "../retroarch.h"

void video_monitor_adjust_system_rates(void)
{
   float timing_skew;
   const struct retro_system_timing *info = 
      (const struct retro_system_timing*)&g_extern.system.av_info.timing;

   g_extern.system.force_nonblock = false;

   if (info->fps <= 0.0 || info->sample_rate <= 0.0)
      return;

   timing_skew = fabs(1.0f - info->fps / g_settings.video.refresh_rate);

   if (timing_skew > g_settings.audio.max_timing_skew)
   {
      /* We don't want to adjust pitch too much. If we have extreme cases,
       * just don't readjust at all. */
      RARCH_LOG("Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
            g_settings.video.refresh_rate,
            (float)info->fps);

      /* We won't be able to do VSync reliably as game FPS > monitor FPS. */
      if (info->fps > g_settings.video.refresh_rate)
      {
         g_extern.system.force_nonblock = true;
         RARCH_LOG("Game FPS > Monitor FPS. Cannot rely on VSync.\n");
      }

      g_extern.audio_data.in_rate = info->sample_rate;
   }
   else
      g_extern.audio_data.in_rate = info->sample_rate *
         (g_settings.video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         g_extern.audio_data.in_rate);

   if (!driver.video_data)
      return;

   if (g_extern.system.force_nonblock)
      rarch_main_command(RARCH_CMD_VIDEO_SET_NONBLOCKING_STATE);
   else
      driver_set_nonblock_state(driver.nonblock_state);
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
   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_settings.video.refresh_rate = hz;
   driver_adjust_system_rates();

   g_extern.audio_data.orig_src_ratio =
      g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_extern.audio_data.in_rate;
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

   if (g_settings.video.threaded)
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (g_extern.measure_data.frame_time_samples_count <
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
   retro_time_t accum = 0, avg, accum_var = 0;
   unsigned samples   = min(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         g_extern.measure_data.frame_time_samples_count);

   if (!g_settings.fps_monitor_enable || 
         g_settings.video.threaded || (samples < 2))
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
      accum += g_extern.measure_data.frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)g_extern.measure_data.frame_time_samples[i]);
#endif

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = g_extern.measure_data.frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation     = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate  = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}
