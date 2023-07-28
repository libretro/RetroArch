/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#ifndef __WEBHOOKS_RICH_PRESENCE_H
#define __WEBHOOKS_RICH_PRESENCE_H

#include <stdint.h>
#include <stdlib.h>

#include "cheevos_client.h"
#include "rc_api_request.h"

typedef void (*richpresence_async_handler)
(
  struct richpresence_async_io_request *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
);

typedef struct richpresence_async_io_request
{
   rc_api_request_t request;
   richpresence_async_handler handler;
   int id;
   rcheevos_client_callback callback;
   void* callback_data;
   int attempt_count;
   const char* success_message;
   const char* failure_message;
   const char* headers;
   char type;
} richpresence_async_io_request;

void update_presence();

#endif /* __WEBHOOKS_RICH_PRESENCE_H */
