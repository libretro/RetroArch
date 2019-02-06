/*
Copyright (c) 2018, Raspberry Pi (Trading) Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <errno.h>
#include <math.h>

#include "RaspiGPS.h"

typedef struct
{
   pthread_mutex_t gps_cache_mutex;
   gpsd_info gpsd;
   struct gps_data_t gpsdata_cache;
   time_t last_valid_time;
   pthread_t gps_reader_thread;
   int terminated;
   int gps_reader_thread_ok;
} GPS_READER_DATA;

static GPS_READER_DATA gps_reader_data;

#define GPS_CACHE_EXPIRY      5 // in seconds

struct gps_data_t *raspi_gps_lock()
{
   pthread_mutex_lock(&gps_reader_data.gps_cache_mutex);
   return &gps_reader_data.gpsdata_cache;
}

void raspi_gps_unlock()
{
   pthread_mutex_unlock(&gps_reader_data.gps_cache_mutex);
}

static void *gps_reader_process(void *gps_reader_data_ptr)
{
   while (!gps_reader_data.terminated)
   {
      int ret = 0;
      int gps_valid = 0;

      gps_reader_data.gpsd.gpsdata.set = 0;
      gps_reader_data.gpsd.gpsdata.fix.mode = 0;
      if (connect_gpsd(&gps_reader_data.gpsd) < 0 ||
            (ret = read_gps_data_once(&gps_reader_data.gpsd)) < 0 )
         break;

      if (ret > 0 && gps_reader_data.gpsd.gpsdata.online)
      {
         if (gps_reader_data.gpsd.gpsdata.fix.mode >= MODE_2D)
         {
            // we have GPS fix, copy fresh data to cache
            gps_valid = 1;
            time(&gps_reader_data.last_valid_time);
            pthread_mutex_lock(&gps_reader_data.gps_cache_mutex);
            memcpy(&gps_reader_data.gpsdata_cache, &gps_reader_data.gpsd.gpsdata,
                   sizeof(struct gps_data_t));
            pthread_mutex_unlock(&gps_reader_data.gps_cache_mutex);
         }
      }
      if (!gps_valid)
      {
         time_t now;
         time(&now);
         if (now - gps_reader_data.last_valid_time > GPS_CACHE_EXPIRY)
         {
            // our cache is stale, clear it
            pthread_mutex_lock(&gps_reader_data.gps_cache_mutex);
            gps_reader_data.gpsdata_cache.online = gps_reader_data.gpsd.gpsdata.online;
            gps_reader_data.gpsdata_cache.set = 0;
            gps_reader_data.gpsdata_cache.fix.mode = 0;
            pthread_mutex_unlock(&gps_reader_data.gps_cache_mutex);
         }
         // we lost GPS fix, copy GPS time to cache if available
         if (gps_reader_data.gpsd.gpsdata.set & TIME_SET)
         {
            pthread_mutex_lock(&gps_reader_data.gps_cache_mutex);
            gps_reader_data.gpsdata_cache.set |= TIME_SET;
            gps_reader_data.gpsdata_cache.fix.time = gps_reader_data.gpsd.gpsdata.fix.time;
            pthread_mutex_unlock(&gps_reader_data.gps_cache_mutex);
         }
      }
   }
   return NULL;
}

void raspi_gps_shutdown(int verbose)
{
   gps_reader_data.terminated = 1;

   if (gps_reader_data.gps_reader_thread_ok)
   {
      if (verbose)
         fprintf(stderr, "Waiting for GPS reader thread to terminate\n");

      pthread_join(gps_reader_data.gps_reader_thread, NULL);
   }
   if (verbose && gps_reader_data.gpsd.gpsd_connected)
      fprintf(stderr, "Closing gpsd connection\n\n");

   disconnect_gpsd(&gps_reader_data.gpsd);

   libgps_unload(&gps_reader_data.gpsd);

   pthread_mutex_destroy(&gps_reader_data.gps_cache_mutex);
}

int raspi_gps_setup(int verbose)
{
   memset(&gps_reader_data, 0, sizeof(gps_reader_data));

   pthread_mutex_init(&gps_reader_data.gps_cache_mutex, NULL);

   gpsd_init(&gps_reader_data.gpsd);
   if (libgps_load(&gps_reader_data.gpsd))
   {
      pthread_mutex_destroy(&gps_reader_data.gps_cache_mutex);
      fprintf(stderr, "Unable to load the libGPS library");
      return -1;
   }
   if (verbose)
      fprintf(stderr, "Connecting to gpsd @ %s:%s\n",
              gps_reader_data.gpsd.server, gps_reader_data.gpsd.port);

   if (connect_gpsd(&gps_reader_data.gpsd))
   {
      fprintf(stderr, "no gpsd running or network error: %d, %s\n",
              errno, gps_reader_data.gpsd.gps_errstr(errno));

      libgps_unload(&gps_reader_data.gpsd);

      pthread_mutex_destroy(&gps_reader_data.gps_cache_mutex);

      return -1;
   }
   if (verbose)
      fprintf(stderr, "Waiting for GPS time\n");

   if (wait_gps_time(&gps_reader_data.gpsd, 2))
   {
      if (verbose)
         fprintf(stderr, "Warning: GPS time not available\n");
   }
   if (verbose)
      fprintf(stderr, "Creating GPS reader thread\n");

   if (pthread_create(&gps_reader_data.gps_reader_thread, NULL,
                      gps_reader_process, &gps_reader_data))
   {
      fprintf(stderr, "Error creating GPS reader thread\n");
      gps_reader_data.terminated = 1;
      raspi_gps_shutdown(verbose);
      return -1;
   }

   gps_reader_data.gps_reader_thread_ok = 1;
   return 0;
}

char *raspi_gps_location_string()
{
   char lat[24] = {"n/a"};
   char lon[24] = {"n/a"};
   char alt[24] = {"Altitude n/a"};
   char speed[24] = {"Speed n/a"};
   char track[24] = {"Track n/a"};
   char datetime[32] = {"Time n/a"};
   char *text;

   struct gps_data_t *gpsdata = raspi_gps_lock();

   if (gpsdata->set & TIME_SET)
   {
      time_t rawtime;
      struct tm *timeinfo;
      rawtime = gpsdata->fix.time;
      timeinfo = localtime(&rawtime);
      strftime(datetime, sizeof(datetime), "%Y:%m:%d %H:%M:%S", timeinfo);
   }

   if (gpsdata->online && gpsdata->fix.mode >= MODE_2D)
   {
      if (gpsdata->set & LATLON_SET)
      {
         if (!isnan(gpsdata->fix.latitude))
            snprintf(lat, sizeof(lat), "%.6lf", gpsdata->fix.latitude);

         if (!isnan(gpsdata->fix.longitude))
            snprintf(lon, sizeof(lon), "%.6lf", gpsdata->fix.longitude);
      }

      if ((gpsdata->set & ALTITUDE_SET) && (gpsdata->fix.mode >= MODE_3D) &&
            (!isnan(gpsdata->fix.altitude)))
      {
         snprintf(alt, sizeof(alt), "Altitude=%.2fm", gpsdata->fix.altitude);
      }

      if ((gpsdata->set & SPEED_SET) && !isnan(gpsdata->fix.speed))
      {
         snprintf(speed, sizeof(speed), "Speed=%.2fkph", gpsdata->fix.speed*MPS_TO_KPH);
      }

      if ((gpsdata->set & TRACK_SET) && !isnan(gpsdata->fix.track))
      {
         snprintf(track, sizeof(track), "Track=%.2f", gpsdata->fix.track);
      }
   }
   raspi_gps_unlock();

   asprintf(&text,  "%s Lat %s Long %s %s %s %s", datetime, lat, lon, alt, speed, track);

   return text;
}
