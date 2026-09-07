/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __ASIO_RING_H
#define __ASIO_RING_H

#include <stddef.h>
#include <retro_inline.h>

/* The ring in front of the ASIO device, in frames. Arithmetic only, so
 * it can be checked off Windows: samples/audio/asio_ring. */

/* The ring never holds less than this many periods: the callback takes
 * a whole period at once, and the writer's next burst lands whenever
 * it lands. */
#define ASIO_RING_MIN_PERIODS 2

/* Nor, where the period is small, less than this many: main-thread
 * scheduling jitter underran a smaller ring audibly, on periods of a
 * few dozen frames. */
#define ASIO_RING_JITTER_PERIODS 4

/* But the jitter is a span of time, not of periods, so that margin is
 * capped here: four of ASIO4ALL's 64 frames is 5 ms and stays; four
 * of a wrapper's fixed 528 would be 44 ms on a device that needs 22,
 * and it gets the two-period minimum, 22, instead. */
#define ASIO_RING_MAX_MS      6

/* The smallest ring for @period_frames at @sample_rate. */
static INLINE size_t asio_ring_floor_frames(unsigned sample_rate,
      size_t period_frames)
{
   size_t by_min    = period_frames * ASIO_RING_MIN_PERIODS;
   size_t by_jitter = period_frames * ASIO_RING_JITTER_PERIODS;
   size_t by_time   = (size_t)sample_rate * ASIO_RING_MAX_MS / 1000;
   size_t floor_frames = (by_jitter < by_time) ? by_jitter : by_time;
   return (floor_frames > by_min) ? floor_frames : by_min;
}

/* Frames for the ring: the latency setting less the device stage, since
 * rate control holds the ring half full and the device stage adds all
 * of itself, so the two add up to about the setting; never below the
 * floor. */
static INLINE size_t asio_ring_frames_for(unsigned sample_rate,
      unsigned latency_ms, size_t device_frames, size_t period_frames)
{
   size_t latency_frames = (size_t)sample_rate * latency_ms / 1000;
   size_t floor_frames   = asio_ring_floor_frames(sample_rate, period_frames);
   size_t ring_frames    = (latency_frames > device_frames)
         ? latency_frames - device_frames : 0;
   if (ring_frames < floor_frames)
      ring_frames        = floor_frames;
   return ring_frames;
}

#endif
