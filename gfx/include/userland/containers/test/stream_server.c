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
#include <ctype.h>

#include "containers/net/net_sockets.h"

#define MAX_BUFFER_LEN  1024
#define MAX_NAME_LEN    256
#define MAX_PORT_LEN    32

int main(int argc, char **argv)
{
   VC_CONTAINER_NET_T *server_sock, *sock;
   vc_container_net_status_t status;
   char buffer[MAX_BUFFER_LEN];
   char name[MAX_NAME_LEN];
   unsigned short port;
   int ii, connections = 1;
   size_t received;

   if (argc < 2)
   {
      printf("Usage:\n%s <port> [<connections>]\n", argv[0]);
      return 1;
   }

   server_sock = vc_container_net_open(NULL, argv[1], VC_CONTAINER_NET_OPEN_FLAG_STREAM, &status);
   if (!server_sock)
   {
      printf("vc_container_net_open failed: %d\n", status);
      return 2;
   }

   if (argc > 2)
   {
      sscanf(argv[2], "%d", &connections);
   }

   status = vc_container_net_listen(server_sock, connections);
   if (status != VC_CONTAINER_NET_SUCCESS)
   {
      printf("vc_container_net_listen failed: %d\n", status);
      vc_container_net_close(server_sock);
      return 3;
   }

   for (ii = 0; ii < connections; ii++)
   {
      status = vc_container_net_accept(server_sock, &sock);
      if (status != VC_CONTAINER_NET_SUCCESS)
      {
         printf("vc_container_net_accept failed: %d\n", status);
         vc_container_net_close(server_sock);
         return 3;
      }

      strcpy(name, "<unknown>");
      vc_container_net_get_client_name(sock, name, sizeof(name));
      vc_container_net_get_client_port(sock, &port);
      printf("Connection from %s:%hu\n", name, port);

      while ((received = vc_container_net_read(sock, buffer, sizeof(buffer) - 1)) != 0)
      {
         char *ptr = buffer;
         size_t jj;

         printf("Rx:");

         /* Flip case and echo data back to client */
         for (jj = 0; jj < received; jj++, ptr++)
         {
            char c = *ptr;

            putchar(c);
            if (isalpha((unsigned char)c))
               *ptr ^= 0x20;  /* Swaps case of ASCII alphabetic characters */
         }

         ptr = buffer;

         printf("Tx:");
         while (received)
         {
            size_t sent;

            sent = vc_container_net_write(sock, ptr, received);
            if (!sent)
            {
               status = vc_container_net_status(sock);
               printf("vc_container_net_write failed: %d\n", status);
               break;
            }

            /* Print out bytes actually sent */
            while (sent--)
            {
               received--;
               putchar(*ptr++);
            }
         }
      }

      status = vc_container_net_status(sock);

      if (status != VC_CONTAINER_NET_SUCCESS && status != VC_CONTAINER_NET_ERROR_CONNECTION_LOST)
         break;

      vc_container_net_close(sock);
   }

   if (status != VC_CONTAINER_NET_SUCCESS)
   {
      printf("vc_container_net_read failed: %d\n", status);
   }

   vc_container_net_close(server_sock);

   return 0;
}
