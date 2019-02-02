/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mmal.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "util/mmal_util_rational.h"
#include "util/mmal_list.h"
#include "mmal_logging.h"


#define CLOCK_PORTS_NUM         5

#define MAX_CLOCK_EVENT_SLOTS   16

#define DEFAULT_FRAME_RATE      30             /* frames per second */
#define DEFAULT_CLOCK_LATENCY   60000          /* microseconds */

#define FILTER_DURATION         2              /* seconds */
#define MAX_FILTER_LENGTH       180            /* samples */

#define MAX_TIME                (~(1LL << 63)) /* microseconds */
#define MIN_TIME                (1LL << 63)    /* microseconds */

#define ABS(a)                  ((a) <  0  ? -(a) : (a))
#define MIN(a,b)                ((a) < (b) ?  (a) : (b))
#define MAX(a,b)                ((a) > (b) ?  (a) : (b))

/** Set to 1 to enable additional stream timing log
 * messages used for debugging the clock algorithm */
#define ENABLE_ADDITIONAL_LOGGING   0
static int clock_additional_logging = ENABLE_ADDITIONAL_LOGGING;

/*****************************************************************************/
typedef int64_t TIME_T;

typedef struct FILTER_T
{
   uint32_t first;              /**< index to the oldest sample */
   uint32_t last;               /**< index to the most recent sample*/
   uint32_t count;              /**< total number of samples in the filter */
   uint32_t length;             /**< maximum number of samples */
   TIME_T sum;                  /**< sum of all samples currently in the filter */
   TIME_T h[MAX_FILTER_LENGTH]; /**< filter history */
} FILTER_T;

/** Frame statistics for a stream */
typedef struct CLOCK_STREAM_T
{
   uint32_t id;                 /**< for debug purposes */

   MMAL_BOOL_T started;         /**< TRUE at least one frame has been received */

   TIME_T pts;                  /**< most recent time-stamp seen */
   TIME_T stc;                  /**< most recent wall-time seen */

   TIME_T mt_off;               /**< offset of the current time stamp from the
                                     arrival time, i.e. PTS - STC */
   TIME_T mt_off_avg;           /**< rolling average of the media time offset */
   TIME_T mt_off_std;           /**< approximate standard deviation of the media
                                     time offset */

   FILTER_T avg_filter;         /**< moving average filter */
   FILTER_T std_filter;         /**< (approximate) standard deviation filter */
} CLOCK_STREAM_T;

/** Clock stream events */
typedef enum CLOCK_STREAM_EVENT_T
{
   CLOCK_STREAM_EVENT_NONE,
   CLOCK_STREAM_EVENT_STARTED,        /**< first data received */
   CLOCK_STREAM_EVENT_DISCONT,        /**< discontinuity detected */
   CLOCK_STREAM_EVENT_FRAME_COMPLETE, /**< complete frame received */
} CLOCK_STREAM_EVENT_T;

/** Clock port event */
typedef struct CLOCK_PORT_EVENT_T
{
   MMAL_LIST_ELEMENT_T link;   /**< must be first */
   MMAL_PORT_T *port;          /**< clock port where the event occurred */
   MMAL_CLOCK_EVENT_T event;   /**< event data */
} CLOCK_PORT_EVENT_T;

/** Clock component context */
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;       /**< current status of the component */

   MMAL_BOOL_T clock_discont;  /**< TRUE -> clock discontinuity detected */

   uint32_t stream_min_id;     /**< id of selected minimum stream (debugging only) */
   uint32_t stream_max_id;     /**< if of selected maximum stream (debugging only) */

   TIME_T mt_off_target;       /**< target clock media time offset */
   TIME_T mt_off_clk;          /**< current clock media time offset */

   TIME_T adj_p;               /**< proportional clock adjustment */
   TIME_T adj_m;               /**< clock adjustment factor (between 1 and 0) */
   TIME_T adj;                 /**< final clock adjustment */

   TIME_T stc_at_update;       /**< the value of the STC the last time the clocks
                                    were updated */

   TIME_T frame_duration;      /**< one frame period (microseconds) */
   MMAL_RATIONAL_T frame_rate; /**< frame rate set by the client */
   uint32_t frame_rate_log2;   /**< frame rate expressed as a power of two */

   MMAL_RATIONAL_T scale;      /**< current clock scale factor */
   MMAL_BOOL_T pending_scale;  /**< TRUE -> scale change is pending */

   MMAL_CLOCK_LATENCY_T           latency;
   MMAL_CLOCK_UPDATE_THRESHOLD_T  update_threshold;
   MMAL_CLOCK_DISCONT_THRESHOLD_T discont_threshold;
   MMAL_CLOCK_REQUEST_THRESHOLD_T request_threshold;

   /** Clock port events */
   struct
   {
      MMAL_LIST_T* queue;      /**< pending events */
      MMAL_LIST_T* free;       /**< available event slots */
      CLOCK_PORT_EVENT_T pool[MAX_CLOCK_EVENT_SLOTS];
   } events;
} MMAL_COMPONENT_MODULE_T;

/** Clock port context */
typedef struct MMAL_PORT_MODULE_T
{
   CLOCK_STREAM_T *stream;     /**< stream associated with this clock port */
} MMAL_PORT_MODULE_T;


/*****************************************************************************/
/** Round x up to the next power of two */
static uint32_t next_pow2(uint32_t x)
{
   x--;
   x = (x >> 1)  | x;
   x = (x >> 2)  | x;
   x = (x >> 4)  | x;
   x = (x >> 8)  | x;
   x = (x >> 16) | x;
   return ++x;
}

/** Given a power of 2 value, return the number of bit shifts */
static uint32_t pow2_shift(uint32_t x)
{
   static const uint32_t BIT_POSITIONS[32] =
   {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
   };

   return BIT_POSITIONS[((x & -x) * 0x077CB531U) >> 27];
}

/** Add 2 values with saturation */
static inline TIME_T saturate_add(TIME_T a, TIME_T b)
{
   TIME_T sum = a + b;
   if (a > 0 && b > 0 && sum < 0)
      sum = MAX_TIME;
   else if (a < 0 && b < 0 && sum > 0)
      sum = MIN_TIME;
   return sum;
}

/*****************************************************************************/
/** Filter reset */
static void filter_init(FILTER_T *filter, uint32_t length)
{
   memset(filter, 0, sizeof(*filter));
   filter->last = length - 1;
   filter->length = length;
};

/** Increment filter index modulo the length */
static inline uint32_t filter_index_wrap(uint32_t index, uint32_t length)
{
   return (++index < length) ? index : 0;
}

/** Remove the oldest sample from the filter */
static void filter_drop(FILTER_T *filter)
{
   if (!filter->count)
      return;

   filter->sum -= filter->h[filter->first];
   filter->first = filter_index_wrap(filter->first, filter->length);
   filter->count--;
}

/** Add a new sample (and drop the oldest when full) */
static void filter_insert(FILTER_T *filter, TIME_T sample)
{
   if (filter->count == filter->length)
      filter_drop(filter);

   filter->last = filter_index_wrap(filter->last, filter->length);
   filter->h[filter->last] = sample;
   filter->sum = saturate_add(filter->sum, sample);
   filter->count++;
}

/*****************************************************************************/
/** Create and initialise a clock stream */
static MMAL_BOOL_T clock_create_stream(CLOCK_STREAM_T **stream, uint32_t id, uint32_t filter_length)
{
   CLOCK_STREAM_T *s = vcos_calloc(1, sizeof(CLOCK_STREAM_T), "clock stream");
   if (!s)
   {
      LOG_ERROR("failed to allocate stream");
      return MMAL_FALSE;
   }

   s->id = id;

   filter_init(&s->avg_filter, filter_length);
   filter_init(&s->std_filter, filter_length);

   *stream = s;
   return MMAL_TRUE;
}

/** Flag this stream as started */
static void clock_start_stream(CLOCK_STREAM_T *stream, TIME_T stc, TIME_T pts)
{
   stream->started = MMAL_TRUE;
   stream->pts = pts;
   stream->stc = stc;
}

/** Reset the internal state of a stream */
static void clock_reset_stream(CLOCK_STREAM_T *stream)
{
   if (!stream)
      return;

   stream->pts = 0;
   stream->stc = 0;
   stream->mt_off = 0;
   stream->mt_off_avg = 0;
   stream->mt_off_std = 0;
   stream->started = MMAL_FALSE;

   filter_init(&stream->avg_filter, stream->avg_filter.length);
   filter_init(&stream->std_filter, stream->std_filter.length);
}

/** Update the internal state of a stream */
static CLOCK_STREAM_EVENT_T clock_update_stream(CLOCK_STREAM_T *stream, TIME_T stc, TIME_T pts,
                                                TIME_T discont_threshold)
{
   CLOCK_STREAM_EVENT_T event = CLOCK_STREAM_EVENT_NONE;
   TIME_T pts_delta, stc_delta;

   if (pts == MMAL_TIME_UNKNOWN)
   {
      LOG_TRACE("ignoring invalid timestamp received at %"PRIi64, stc);
      return CLOCK_STREAM_EVENT_NONE;
   }

   if (!stream->started)
   {
      LOG_TRACE("stream %d started %"PRIi64" %"PRIi64, stream->id, stc, pts);
      clock_start_stream(stream, stc, pts);
      return CLOCK_STREAM_EVENT_STARTED;
   }

   /* XXX: This should really use the buffer flags to determine if a complete
    * frame has been received.  However, not all clients set MMAL buffer flags
    * correctly (if at all). */
   pts_delta = pts - stream->pts;
   stc_delta = stc - stream->stc;

   /* Check for discontinuities. */
   if ((ABS(pts_delta) > discont_threshold) || (ABS(stc_delta) > discont_threshold))
   {
      LOG_ERROR("discontinuity detected on stream %d %"PRIi64" %"PRIi64" %"PRIi64,
                stream->id, pts_delta, stc_delta, discont_threshold);
      return CLOCK_STREAM_EVENT_DISCONT;
   }

   if (pts_delta)
   {
      /* A complete frame has now been received, so update the stream's notion of media time */
      stream->mt_off = stream->pts - stream->stc;

      filter_insert(&stream->avg_filter, stream->mt_off);
      stream->mt_off_avg = stream->avg_filter.sum / stream->avg_filter.count;

      filter_insert(&stream->std_filter, ABS(stream->mt_off - stream->mt_off_avg));
      stream->mt_off_std = stream->std_filter.sum / stream->std_filter.count;

      LOG_TRACE("stream %d %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64,
                stream->id, stream->stc, stream->pts, stream->mt_off_avg, stream->mt_off);

      event = CLOCK_STREAM_EVENT_FRAME_COMPLETE;
   }

   stream->pts = pts;
   stream->stc = stc;

   return event;
}

/*****************************************************************************/
/** Start all enabled clock ports, making sure all use the same thresholds */
static void clock_start_clocks(MMAL_COMPONENT_T *component, TIME_T media_time)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned i;

   for (i = 0; i < component->clock_num; ++i)
   {
      MMAL_PORT_T *port = component->clock[i];
      if (port->is_enabled)
      {
         LOG_TRACE("starting clock %d with time %"PRIi64, port->index, media_time);
         mmal_port_clock_reference_set(port, MMAL_TRUE);
         mmal_port_clock_media_time_set(port, media_time);
         mmal_port_clock_update_threshold_set(port, &module->update_threshold);
         mmal_port_clock_discont_threshold_set(port, &module->discont_threshold);
         mmal_port_clock_request_threshold_set(port, &module->request_threshold);
         mmal_port_clock_active_set(port, MMAL_TRUE);
      }
   }
}

/** Stop (and flush) all enabled clock ports */
static void clock_stop_clocks(MMAL_COMPONENT_T *component)
{
   unsigned i;

   for (i = 0; i < component->clock_num; ++i)
   {
      MMAL_PORT_T *port = component->clock[i];
      if (port->is_enabled)
      {
         LOG_TRACE("stopping clock %d", port->index);
         mmal_port_clock_request_flush(port);
         mmal_port_clock_active_set(port, MMAL_FALSE);
      }
   }
}

/** Reset the internal state of all streams in order to rebase clock
 * adjustment calculations */
static void clock_reset_clocks(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned i;

   for (i = 0; i < component->clock_num; ++i)
      clock_reset_stream(component->clock[i]->priv->module->stream);

   module->clock_discont = MMAL_TRUE;
}

/** Change the media-time for all enabled clock ports */
static void clock_set_media_time(MMAL_COMPONENT_T *component, TIME_T media_time)
{
   unsigned i;

   for (i = 0; i < component->clock_num; ++i)
   {
      MMAL_PORT_T *port = component->clock[i];
      if (port->is_enabled)
         mmal_port_clock_media_time_set(port, media_time);
   }
}

/** Change the scale for all clock ports */
static void clock_set_scale(MMAL_COMPONENT_T *component, MMAL_RATIONAL_T scale)
{
   unsigned i;

   for (i = 0; i < component->clock_num; ++i)
      mmal_port_clock_scale_set(component->clock[i], scale);

   component->priv->module->pending_scale = 0;
}

/** Update the average and standard deviation calculations for all streams
 * (dropping samples where necessary) and return the minimum and maximum
 * streams */
static MMAL_BOOL_T clock_get_mt_off_avg(MMAL_COMPONENT_T *component, TIME_T stc,
                                        CLOCK_STREAM_T **minimum, CLOCK_STREAM_T **maximum)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   TIME_T drop_threshold = 6 * module->frame_duration;
   TIME_T reset_threshold = module->latency.target << 1;
   TIME_T avg_min = MAX_TIME;
   TIME_T avg_max = MIN_TIME;
   TIME_T avg_bias;
   TIME_T stc_delta;
   unsigned i;

   *minimum = 0;
   *maximum = 0;

   for (i = 0; i < component->clock_num; ++i)
   {
      CLOCK_STREAM_T *stream = component->clock[i]->priv->module->stream;
      if (stream)
      {
         stc_delta = stc - stream->stc;

         /* Drop samples from the moving average and standard deviation filters */
         if (stc_delta > reset_threshold)
         {
            filter_init(&stream->avg_filter, stream->avg_filter.length);
            filter_init(&stream->std_filter, stream->std_filter.length);
            LOG_TRACE("reset stream %d filters due to stc_delta %"PRIi64, stream->id, stc_delta);
         }
         else if (stc_delta > drop_threshold)
         {
            filter_drop(&stream->avg_filter);
            filter_drop(&stream->std_filter);
            LOG_TRACE("drop stream %d filter samples due to stc_delta %"PRIi64, stream->id, stc_delta);
         }

         /* No point in continuing if filters are empty */
         if (!stream->avg_filter.count)
            continue;

         /* Calculate new average and standard deviation for the stream */
         stream->mt_off_avg = stream->avg_filter.sum / stream->avg_filter.count;
         stream->mt_off_std = stream->std_filter.sum / stream->std_filter.count;

         /* Select the minimum and maximum average between all active streams */
         avg_bias = (stream->avg_filter.length - stream->avg_filter.count) * ABS(stream->mt_off_avg) / stream->avg_filter.length;
         if ((stream->mt_off_avg + avg_bias) < avg_min)
         {
            avg_min = stream->mt_off_avg;
            *minimum = stream;
            LOG_TRACE("found min on %d mt_off_avg %"PRIi64" mt_off_std %"PRIi64" avg_bias %"PRIi64" count %d",
                      stream->id, stream->mt_off_avg, stream->mt_off_std, avg_bias, stream->avg_filter.count);
         }
         if ((stream->mt_off_avg - avg_bias) > avg_max)
         {
            avg_max = stream->mt_off_avg;
            *maximum = stream;
            LOG_TRACE("found max on %d mt_off_avg %"PRIi64" mt_off_std %"PRIi64" avg_bias %"PRIi64" count %d",
                      stream->id, stream->mt_off_avg, stream->mt_off_std, avg_bias, stream->avg_filter.count);
         }
      }
   }

   return (*minimum) && (*maximum);
}

/** Adjust the media-time of the playback clocks based on current timing statistics */
static void clock_adjust_clocks(MMAL_COMPONENT_T *component, TIME_T stc)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   CLOCK_STREAM_T *stream_min;
   CLOCK_STREAM_T *stream_max;
   TIME_T mt_off_clk;
   TIME_T stc_prev;

   if (!clock_get_mt_off_avg(component, stc, &stream_min, &stream_max))
      return;

   module->stream_min_id = stream_min->id;
   module->stream_max_id = stream_max->id;

   /* Calculate the actual media-time offset seen by the clock */
   mt_off_clk = mmal_port_clock_media_time_get(component->clock[0]) - stc;

   stc_prev = module->stc_at_update;
   module->stc_at_update = stc;

   /* If there has been a discontinuity, restart the clock,
    * else use the clock control loop to apply a clock adjustment */
   if (module->clock_discont)
   {
      module->clock_discont = MMAL_FALSE;

      module->mt_off_clk = stream_min->mt_off_avg - module->latency.target;
      module->mt_off_target = module->mt_off_clk;

      clock_stop_clocks(component);
      clock_start_clocks(component, module->mt_off_clk + stc);
   }
   else
   {
      /* Determine the new clock target */
      TIME_T mt_off_target_max = stream_max->mt_off_avg - module->latency.target;
      TIME_T mt_off_target_min = stream_min->mt_off_avg - module->frame_duration;
      module->mt_off_target = MIN(mt_off_target_max, mt_off_target_min);

      /* Calculate the proportional adjustment, capped by the attack rate
       * set by the client */
      TIME_T stc_delta = (stc > stc_prev) ? (stc - stc_prev) : 0;
      TIME_T adj_p_max = stc_delta * module->latency.attack_rate / module->latency.attack_period;

      module->adj_p = module->mt_off_target - module->mt_off_clk;
      if (module->adj_p < -adj_p_max)
         module->adj_p = -adj_p_max;
      else if (module->adj_p > adj_p_max)
         module->adj_p = adj_p_max;

      /* Calculate the confidence of the adjustment using the approximate
       * standard deviation for the selected stream:
       *
       * adj_m = 1.0 - STD * FPS / 4
       *
       * The adjustment factor is scaled up by 2^20 which is an approximation
       * of 1000000 (microseconds per second) and the frame rate is assumed
       * to be either 32 or 64 which are approximations for 24/25/30 and 60
       * fps to avoid divisions.  This has a lower limit of 0. */
      module->adj_m =
         MAX((1 << 20) - ((stream_min->mt_off_std << module->frame_rate_log2) >> 2), 0);

      /* Modulate the proportional adjustment by the sample confidence
       * and apply the adjustment to the current clock */
      module->adj = (module->adj_p * module->adj_m) >> 20;
      module->adj = (module->adj * (stream_min->avg_filter.count << 8) / stream_min->avg_filter.length) >> 8;
      module->mt_off_clk += module->adj;

      clock_set_media_time(component, module->mt_off_clk + stc);
   }

   /* Any pending clock scale changes can now be applied */
   if (component->priv->module->pending_scale)
      clock_set_scale(component, component->priv->module->scale);
}

/*****************************************************************************/
static void clock_process_stream_event(MMAL_COMPONENT_T *component, CLOCK_STREAM_T *stream,
                                       CLOCK_STREAM_EVENT_T event, TIME_T stc)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   switch (event)
   {
   case CLOCK_STREAM_EVENT_FRAME_COMPLETE:
      clock_adjust_clocks(component, stc);
      if (clock_additional_logging)
      {
         VCOS_ALERT("STRM_%d %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %d %"
                    PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %u %u",
                    stream->id, stream->stc, stream->pts, stream->mt_off_avg, stream->mt_off,
                    stream->mt_off_std, ABS(stream->mt_off - stream->mt_off_avg), stream->avg_filter.count,
                    module->mt_off_clk, module->mt_off_target, module->adj_p, module->adj_m, module->adj,
                    module->stream_min_id, module->stream_max_id);
      }
      break;
   case CLOCK_STREAM_EVENT_DISCONT:
      clock_reset_clocks(component);
      break;
   default:
      /* ignore all other events */
      break;
   }
}

/** Handler for input buffer events */
static void clock_process_input_buffer_info_event(MMAL_COMPONENT_T *component, MMAL_PORT_T *port,
                                                  const MMAL_CLOCK_BUFFER_INFO_T *info)
{
   CLOCK_STREAM_EVENT_T stream_event = CLOCK_STREAM_EVENT_NONE;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   TIME_T stc = (TIME_T)((uint64_t)info->arrival_time);
   TIME_T pts = info->time_stamp;

   LOG_TRACE("port %d %"PRIi64" %"PRIi64, port->index, stc, pts);

   if (!port_module->stream)
   {
      /* First data received for this stream */
      uint32_t filter_length = module->frame_rate.num * FILTER_DURATION /
                               module->frame_rate.den;
      if (!clock_create_stream(&port_module->stream, port->index, filter_length))
         return;
   }

   stream_event = clock_update_stream(port_module->stream, stc, pts, module->discont_threshold.threshold);

   clock_process_stream_event(component, port_module->stream, stream_event, stc);
}

/** Handler for clock scale events */
static void clock_process_scale_event(MMAL_COMPONENT_T *component, MMAL_RATIONAL_T scale)
{
   /* When pausing the clock (i.e. scale = 0.0), apply the scale change
    * immediately.  However, when resuming the clock (i.e. scale = 1.0),
    * the scale change can only be applied the next time new buffer timing
    * information is received.  This ensures that clocks resume with the
    * correct media-time. */
   if (scale.num == 0)
   {
      component->priv->module->scale = scale;
      clock_set_scale(component, scale);
   }
   else
   {
      /* Only support scale == 1.0 */
      if (!mmal_rational_equal(component->priv->module->scale, scale) &&
          (scale.num == scale.den))
      {
         component->priv->module->scale = scale;
         component->priv->module->pending_scale = 1;
         clock_reset_clocks(component);
      }
   }
}

/** Handler for update threshold events */
static void clock_process_update_threshold_event(MMAL_COMPONENT_T *component, const MMAL_CLOCK_UPDATE_THRESHOLD_T *threshold)
{
   unsigned i;

   component->priv->module->update_threshold = *threshold;

   for (i = 0; i < component->clock_num; ++i)
      mmal_port_clock_update_threshold_set(component->clock[i], threshold);
}

/** Handler for discontinuity threshold events */
static void clock_process_discont_threshold_event(MMAL_COMPONENT_T *component, const MMAL_CLOCK_DISCONT_THRESHOLD_T *threshold)
{
   unsigned i;

   component->priv->module->discont_threshold = *threshold;

   for (i = 0; i < component->clock_num; ++i)
      mmal_port_clock_discont_threshold_set(component->clock[i], threshold);
}

/** Handler for request threshold events */
static void clock_process_request_threshold_event(MMAL_COMPONENT_T *component, const MMAL_CLOCK_REQUEST_THRESHOLD_T *threshold)
{
   unsigned i;

   component->priv->module->request_threshold = *threshold;

   for (i = 0; i < component->clock_num; ++i)
      mmal_port_clock_request_threshold_set(component->clock[i], threshold);
}

/** Handler for latency events */
static void clock_process_latency_event(MMAL_COMPONENT_T *component, const MMAL_CLOCK_LATENCY_T *latency)
{
   component->priv->module->latency = *latency;

   clock_reset_clocks(component);
}

/** Add a clock port event to the queue and trigger the action thread */
static MMAL_STATUS_T clock_event_queue(MMAL_COMPONENT_T *component, MMAL_PORT_T *port, const MMAL_CLOCK_EVENT_T *event)
{
   CLOCK_PORT_EVENT_T *slot = (CLOCK_PORT_EVENT_T*)mmal_list_pop_front(component->priv->module->events.free);
   if (!slot)
   {
      LOG_ERROR("no event slots available");
      return MMAL_ENOSPC;
   }

   slot->port = port;
   slot->event = *event;
   mmal_list_push_back(component->priv->module->events.queue, &slot->link);

   return mmal_component_action_trigger(component);
}

/** Get the next clock port event in the queue */
static MMAL_STATUS_T clock_event_dequeue(MMAL_COMPONENT_T *component, CLOCK_PORT_EVENT_T *port_event)
{
   CLOCK_PORT_EVENT_T *slot = (CLOCK_PORT_EVENT_T*)mmal_list_pop_front(component->priv->module->events.queue);
   if (!slot)
      return MMAL_EINVAL;

   port_event->port = slot->port;
   port_event->event = slot->event;
   mmal_list_push_back(component->priv->module->events.free, &slot->link);

   if (port_event->event.buffer)
   {
      port_event->event.buffer->length = 0;
      mmal_port_buffer_header_callback(port_event->port, port_event->event.buffer);
   }

   return MMAL_SUCCESS;
}

/** Event callback from a clock port */
static void clock_event_cb(MMAL_PORT_T *port, const MMAL_CLOCK_EVENT_T *event)
{
   clock_event_queue(port->component, port, event);
}


/*****************************************************************************/
/** Actual processing function */
static MMAL_BOOL_T clock_do_processing(MMAL_COMPONENT_T *component)
{
   CLOCK_PORT_EVENT_T port_event;

   if (clock_event_dequeue(component, &port_event) != MMAL_SUCCESS)
      return MMAL_FALSE; /* No more external events to process */

   /* Process external events (coming from clock ports) */
   switch (port_event.event.id)
   {
   case MMAL_CLOCK_EVENT_SCALE:
      clock_process_scale_event(component, port_event.event.data.scale);
      break;
   case MMAL_CLOCK_EVENT_UPDATE_THRESHOLD:
      clock_process_update_threshold_event(component, &port_event.event.data.update_threshold);
      break;
   case MMAL_CLOCK_EVENT_DISCONT_THRESHOLD:
      clock_process_discont_threshold_event(component, &port_event.event.data.discont_threshold);
      break;
   case MMAL_CLOCK_EVENT_REQUEST_THRESHOLD:
      clock_process_request_threshold_event(component, &port_event.event.data.request_threshold);
      break;
   case MMAL_CLOCK_EVENT_LATENCY:
      clock_process_latency_event(component, &port_event.event.data.latency);
      break;
   case MMAL_CLOCK_EVENT_INPUT_BUFFER_INFO:
      clock_process_input_buffer_info_event(component, port_event.port, &port_event.event.data.buffer);
      break;
   default:
      break;
   }

   return MMAL_TRUE;
}

/** Component action thread */
static void clock_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (clock_do_processing(component));
}


/*****************************************************************************/
/** Set a parameter on the clock component's control port */
static MMAL_STATUS_T clock_control_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status = MMAL_SUCCESS;

   switch (param->id)
   {
      case MMAL_PARAMETER_CLOCK_FRAME_RATE:
      {
         const MMAL_PARAMETER_FRAME_RATE_T *p = (const MMAL_PARAMETER_FRAME_RATE_T *)param;
         module->frame_rate = p->frame_rate;
         /* XXX: take frame_rate.den into account */
         module->frame_rate_log2 = pow2_shift(next_pow2(module->frame_rate.num));
         module->frame_duration = p->frame_rate.den * 1000000 / p->frame_rate.num;
         LOG_TRACE("frame rate %d/%d (%u) duration %"PRIi64,
                   module->frame_rate.num, module->frame_rate.den,
                   module->frame_rate_log2, module->frame_duration);
      }
      break;
      case MMAL_PARAMETER_CLOCK_LATENCY:
      {
         /* Changing the latency setting requires a reset of the clock algorithm, but
          * that can only be safely done from within the component's worker thread.
          * So, queue the new latency setting as a clock event. */
         const MMAL_PARAMETER_CLOCK_LATENCY_T *p = (const MMAL_PARAMETER_CLOCK_LATENCY_T *)param;
         MMAL_CLOCK_EVENT_T event = { MMAL_CLOCK_EVENT_LATENCY, MMAL_CLOCK_EVENT_MAGIC };

         LOG_TRACE("latency target %"PRIi64" attack %"PRIi64"/%"PRIi64,
                   p->value.target, p->value.attack_rate, p->value.attack_period);

         event.data.latency = p->value;
         status = clock_event_queue(port->component, port, &event);
      }
      break;
      default:
         LOG_ERROR("parameter not supported (0x%x)", param->id);
         status = MMAL_ENOSYS;
         break;
   }
   return status;
}

/** Destroy a previously created component */
static MMAL_STATUS_T clock_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int i;

   if (module->events.free)
      mmal_list_destroy(module->events.free);

   if (module->events.queue)
      mmal_list_destroy(module->events.queue);

   if (component->clock_num)
   {
      for (i = 0; i < component->clock_num; ++i)
         vcos_free(component->clock[i]->priv->module->stream);

      mmal_ports_clock_free(component->clock, component->clock_num);
   }

   vcos_free(module);

   return MMAL_SUCCESS;
}

/** Create an instance of a clock component  */
static MMAL_STATUS_T mmal_component_create_clock(const char *name, MMAL_COMPONENT_T *component)
{
   int i;
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));

   component->priv->pf_destroy = clock_component_destroy;

   /* Create the clock ports (clock ports are managed by the framework) */
   component->clock = mmal_ports_clock_alloc(component, CLOCK_PORTS_NUM,
                                             sizeof(MMAL_PORT_MODULE_T), clock_event_cb);
   if (!component->clock)
      goto error;
   component->clock_num = CLOCK_PORTS_NUM;

   component->control->priv->pf_parameter_set = clock_control_parameter_set;

   /* Setup event slots */
   module->events.free = mmal_list_create();
   module->events.queue = mmal_list_create();
   if (!module->events.free || !module->events.queue)
   {
      LOG_ERROR("failed to create list %p %p", module->events.free, module->events.queue);
      goto error;
   }
   for (i = 0; i < MAX_CLOCK_EVENT_SLOTS; ++i)
      mmal_list_push_back(module->events.free, &module->events.pool[i].link);

   component->priv->priority = VCOS_THREAD_PRI_REALTIME;
   status = mmal_component_action_register(component, clock_do_processing_loop);

   module->clock_discont = MMAL_TRUE;
   module->frame_rate.num = DEFAULT_FRAME_RATE;
   module->frame_rate.den = 1;

   module->scale = mmal_port_clock_scale_get(component->clock[0]);

   memset(&module->latency, 0, sizeof(module->latency));
   module->latency.target = DEFAULT_CLOCK_LATENCY;

   mmal_port_clock_update_threshold_get(component->clock[0], &module->update_threshold);
   mmal_port_clock_discont_threshold_get(component->clock[0], &module->discont_threshold);
   mmal_port_clock_request_threshold_get(component->clock[0], &module->request_threshold);

   return status;

 error:
   clock_component_destroy(component);
   return status;
}


/*****************************************************************************/
MMAL_CONSTRUCTOR(mmal_register_component_clock);
void mmal_register_component_clock(void)
{
   mmal_component_supplier_register("clock", mmal_component_create_clock);
}
