/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2015-2017 - Andre Leiradella
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdlib.h>

#include <net/net_http.h>
#include <features/features_cpu.h>

#include "net_http_special.h"

int net_http_get(const char **result, size_t *size, const char *url, retro_time_t *timeout)
{
   size_t length;
   uint8_t* data                  = NULL;
   char* res                      = NULL;
   int ret                        = NET_HTTP_GET_OK;
   struct http_t* http            = NULL;
   retro_time_t t0                = cpu_features_get_time_usec();
   struct http_connection_t *conn = net_http_connection_new(url, "GET", NULL);

   *result = NULL;

   /* Error creating the connection descriptor. */
   if (!conn)
      goto error;

   /* Don't bother with timeouts here, it's just a string scan. */
   while (!net_http_connection_iterate(conn)) {}

   /* Error finishing the connection descriptor. */
   if (!net_http_connection_done(conn))
   {
      ret = NET_HTTP_GET_MALFORMED_URL;
      goto error;
   }

   http = net_http_new(conn);

   /* Error connecting to the endpoint. */
   if (!http)
   {
      ret = NET_HTTP_GET_CONNECT_ERROR;
      goto error;
   }

   while (!net_http_update(http, NULL, NULL))
   {
      /* Timeout error. */
      if (timeout && (cpu_features_get_time_usec() - t0) > *timeout)
      {
         ret = NET_HTTP_GET_TIMEOUT;
         goto error;
      }
   }

   data = net_http_data(http, &length, false);

   if (data)
   {
      res = (char*)malloc(length + 1);

      /* Allocation error. */
      if (!res)
         goto error;

      memcpy((void*)res, (void*)data, length);
      free(data);
      res[length] = 0;
      *result = res;
   }
   else
   {
      length = 0;
      *result = NULL;
   }

   if (size)
      *size = length;

error:
   if (http)
      net_http_delete(http);

   if (conn)
      net_http_connection_free(conn);

   if (timeout)
   {
      t0 = cpu_features_get_time_usec() - t0;

      if (t0 < *timeout)
         *timeout -= t0;
      else
         *timeout = 0;
   }

   return ret;
}
