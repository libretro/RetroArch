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

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "containers/net/net_sockets.h"

#include "nb_io.h"

#define MAX_BUFFER_LEN  1024
/* Time in milliseconds to yield when nothing is happening */
#define YIELD_PERIOD_MS 33


static vc_container_net_status_t local_net_control(VC_CONTAINER_NET_T *sock,
      vc_container_net_control_t operation, ...)
{
   vc_container_net_status_t result;
   va_list args;

   va_start(args, operation);
   result = vc_container_net_control(sock, operation, args);
   va_end(args);

   return result;
}

int main(int argc, char **argv)
{
   VC_CONTAINER_NET_T *sock;
   vc_container_net_status_t status;
   char send_buffer[MAX_BUFFER_LEN];
   char recv_buffer[MAX_BUFFER_LEN];
   int ready = 1;
   int to_send = 0;
   size_t received;

   if (argc < 3)
   {
      printf("Usage:\n%s <address> <port>\n", argv[0]);
      return 1;
   }

   sock = vc_container_net_open(argv[1], argv[2], VC_CONTAINER_NET_OPEN_FLAG_STREAM, &status);
   if (!sock)
   {
      printf("vc_container_net_open failed: %d\n", status);
      return 2;
   }

   /* Use non-blocking I/O for both network and console */
   local_net_control(sock, VC_CONTAINER_NET_CONTROL_SET_READ_TIMEOUT_MS, YIELD_PERIOD_MS);
   nb_set_nonblocking_input(1);

   while (ready)
   {
      if (nb_char_available())
      {
         char c = nb_get_char();

         if (c == 26)   /* CTRL+Z */
            break;

         send_buffer[to_send++] = c;

         if (c == '\r') /* Translate CR into CRLF */
         {
            c = '\n';
            nb_put_char(c);
            send_buffer[to_send++] = c;
         }

         if (c == '\n' || to_send == sizeof(send_buffer) - 1)  /* Allow for next character needing translation */
         {
            int already_sent = 0, sent;

            /* Send a line at a time */
            while (to_send)
            {
               sent = vc_container_net_write(sock, send_buffer + already_sent, to_send);
               if (!sent)
               {
                  printf("vc_container_net_write failed: %d\n", vc_container_net_status(sock));
                  to_send = 0;
                  ready = 0;
                  break;
               }
               to_send -= sent;
               already_sent += sent;
            }
         }
      }

      /* Read back and print any data from the server */
      received = vc_container_net_read(sock, recv_buffer, sizeof(recv_buffer) - 1);
      if (received)
      {
         char *ptr = recv_buffer;

         while (received--)
            nb_put_char(*ptr++);
      } else {
         vc_container_net_status_t net_status = vc_container_net_status(sock);

         if (net_status != VC_CONTAINER_NET_ERROR_TIMED_OUT && net_status != VC_CONTAINER_NET_SUCCESS)
         {
            printf("vc_container_net_read failed: %d\n", net_status);
            ready = 0;
         }
      }
   }

   nb_set_nonblocking_input(0);

   vc_container_net_close(sock);

   return 0;
}
