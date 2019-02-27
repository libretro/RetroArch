/*
Copyright (c) 2016, Joo Aun Saw
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

#ifndef LIBGPS_LOADER_H
#define LIBGPS_LOADER_H

#include "gps.h"

// IMPORTANT: Remember to copy gps.h from gpsd when upgrading libgps version.
#define LIBGPS_SO_VERSION     "libgps.so.22"

/** Structure containing all libgps information
 */
typedef struct
{
   char *server;
   char *port;

   void *libgps_handle;
   int gpsd_connected;
   /* requires libgps (GPSD_API_MAJOR_VERSION >= 5) */
   gps_mask_t current_mask;
   struct gps_data_t gpsdata;
   /* libgps functions */
   int (*gps_read)(struct gps_data_t *);
   bool (*gps_waiting)(const struct gps_data_t *, int);
   int (*gps_open)(const char *, const char *, struct gps_data_t *);
   int (*gps_close)(struct gps_data_t *);
   const char *(*gps_errstr)(const int);
   int (*gps_stream)(struct gps_data_t *, unsigned int, void *);
} gpsd_info;

/* libgps */
void gpsd_init(gpsd_info *gpsd);
int libgps_load(gpsd_info *gpsd);
void libgps_unload(gpsd_info *gpsd);

/* gpsd */
int connect_gpsd(gpsd_info *gpsd);
int disconnect_gpsd(gpsd_info *gpsd);
int wait_gps_time(gpsd_info *gpsd, int timeout_s);
int read_gps_data_once(gpsd_info *gpsd);

/* helper functions */
int deg_to_str(double f, char *buf, int buf_size);

extern const char *LIBGPS_FILE;

#endif /* LIBGPS_LOADER_H */
