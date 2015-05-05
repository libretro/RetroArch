/*  RetroArch - A frontend for libretro.
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

#include <retro_miscellaneous.h>
#include <net/net_http.h>
#include <queues/message_queue.h>
#include <string/string_list.h>

#include "../runloop_data.h"
#include "tasks.h"

int cb_core_updater_download(void *data_, size_t len);
int cb_core_updater_list(void *data_, size_t len);


static int rarch_main_data_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int rarch_main_data_http_conn_iterate_transfer_parse(http_handle_t *http)
{
   if (net_http_connection_done(http->connection.handle))
   {
      if (http->connection.handle && http->connection.cb)
         http->connection.cb(http, 0);
   }
   
   net_http_connection_free(http->connection.handle);

   http->connection.handle = NULL;

   return 0;
}

static int rarch_main_data_http_iterate_transfer_parse(http_handle_t *http)
{
   size_t len = 0;
   char *data = (char*)net_http_data(http->handle, &len, false);

   if (data && http->cb)
      http->cb(data, len);

   net_http_delete(http->handle);

   http->handle = NULL;
   msg_queue_clear(http->msg_queue);

   return 0;
}

static int cb_http_conn_default(void *data_, size_t len)
{
   http_handle_t *http = (http_handle_t*)data_;

   if (!http)
      return -1;

   http->handle = net_http_new(http->connection.handle);

   if (!http->handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      return -1;
   }

   http->cb     = NULL;

   if (http->connection.elem1[0] != '\0')
   {
      if (!strcmp(http->connection.elem1, "cb_core_updater_download"))
         http->cb = &cb_core_updater_download;
      if (!strcmp(http->connection.elem1, "cb_core_updater_list"))
         http->cb = &cb_core_updater_list;
   }

   return 0;
}

/**
 * rarch_main_data_http_iterate_poll:
 *
 * Polls HTTP message queue to see if any new URLs 
 * are pending.
 *
 * If handle is freed, will set up a new http handle. 
 * The transfer will be started on the next frame.
 *
 * Returns: 0 when an URL has been pulled and we will
 * begin transferring on the next frame. Returns -1 if
 * no HTTP URL has been pulled. Do nothing in that case.
 **/
static int rarch_main_data_http_iterate_poll(http_handle_t *http)
{
   char elem0[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *url              = msg_queue_pull(http->msg_queue);

   if (!url)
      return -1;

   /* Can only deal with one HTTP transfer at a time for now */
   if (http->handle)
      return -1; 

   str_list                     = string_split(url, "|");

   if (!str_list)
      return -1;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));

   http->connection.handle = net_http_connection_new(elem0);

   if (!http->connection.handle)
      return -1;

   http->connection.cb     = &cb_http_conn_default;

   if (str_list->size > 1)
      strlcpy(http->connection.elem1,
            str_list->elems[1].data,
            sizeof(http->connection.elem1));

   string_list_free(str_list);
   
   return 0;
}

/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_data_http_iterate_transfer(void *data)
{
    http_handle_t *http = (http_handle_t*)data;
    size_t pos = 0, tot = 0;
    int percent = 0;
    if (!net_http_update(http->handle, &pos, &tot))
    {
        if(tot != 0)
            percent=(unsigned long long)pos*100/(unsigned long long)tot;
        else
            percent=0;
        
        if (percent > 0)
        {
            char tmp[PATH_MAX_LENGTH];
            snprintf(tmp, sizeof(tmp), "Download progress: %d%%", percent);
            data_runloop_osd_msg(tmp, sizeof(tmp));
        }
        
        return -1;
    }
    
    return 0;
}

void rarch_main_data_http_iterate(bool is_thread, void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;
   http_handle_t *http = runloop ? &runloop->http : NULL;
   if (!http)
      return;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         rarch_main_data_http_conn_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!rarch_main_data_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         rarch_main_data_http_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_POLL;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(http))
            http->status = HTTP_STATUS_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_POLL:
      default:
         if (rarch_main_data_http_iterate_poll(http) == 0)
            http->status = HTTP_STATUS_CONNECTION_TRANSFER;
         break;
   }
}
