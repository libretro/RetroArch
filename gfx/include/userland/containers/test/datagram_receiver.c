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
#include <string.h>
#include <stdio.h>

#include "containers/net/net_sockets.h"

int main(int argc, char **argv)
{
   VC_CONTAINER_NET_T *sock;
   vc_container_net_status_t status;
   char *buffer;
   size_t buffer_size;
   size_t received;

   if (argc < 2)
   {
      printf("Usage:\n%s <port>\n", argv[0]);
      return 1;
   }

   sock = vc_container_net_open(NULL, argv[1], 0, &status);
   if (!sock)
   {
      printf("vc_container_net_open failed: %d\n", status);
      return 2;
   }

   buffer_size = vc_container_net_maximum_datagram_size(sock);
   buffer = (char *)malloc(buffer_size);
   if (!buffer)
   {
      vc_container_net_close(sock);
      printf("Failure allocating buffer\n");
      return 3;
   }

   while ((received = vc_container_net_read(sock, buffer, buffer_size)) != 0)
   {
      char *ptr = buffer;

      while (received--)
         putchar(*ptr++);
   }

   if (vc_container_net_status(sock) != VC_CONTAINER_NET_SUCCESS)
   {
      printf("vc_container_net_read failed: %d\n", vc_container_net_status(sock));
   }

   vc_container_net_close(sock);

   return 0;
}
