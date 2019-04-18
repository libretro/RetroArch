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

#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_io.h"

#include "nb_io.h"

#define MAX_BUFFER_SIZE 2048

int main(int argc, char **argv)
{
   char buffer[MAX_BUFFER_SIZE];
   VC_CONTAINER_IO_T *read_io, *write_io;
   VC_CONTAINER_STATUS_T status;
   size_t received;
   bool ready = true;

   if (argc < 3)
   {
      LOG_INFO(NULL, "Usage:\n%s <read URI> <write URI>\n", argv[0]);
      return 1;
   }

   read_io = vc_container_io_open(argv[1], VC_CONTAINER_IO_MODE_READ, &status);
   if (!read_io)
   {
      LOG_INFO(NULL, "Opening <%s> for read failed: %d\n", argv[1], status);
      return 2;
   }

   write_io = vc_container_io_open(argv[2], VC_CONTAINER_IO_MODE_WRITE, &status);
   if (!write_io)
   {
      vc_container_io_close(read_io);
      LOG_INFO(NULL, "Opening <%s> for write failed: %d\n", argv[2], status);
      return 3;
   }

   nb_set_nonblocking_input(1);

   while (ready)
   {
      size_t total_written = 0;

      received = vc_container_io_read(read_io, buffer, sizeof(buffer));
      while (ready && total_written < received)
      {
         total_written += vc_container_io_write(write_io, buffer + total_written, received - total_written);
         ready &= (write_io->status == VC_CONTAINER_SUCCESS);
      }
      ready &= (read_io->status == VC_CONTAINER_SUCCESS);

      if (nb_char_available())
      {
         char c = nb_get_char();

         switch (c)
         {
         case 'q':
         case 'Q':
         case 0x04:  /* CTRL+D */
         case 0x1A:  /* CTRL+Z */
         case 0x1B:  /* Escape */
            ready = false;
            break;
         default:
            ;/* Do nothing */
         }
      }
   }

   if (read_io->status != VC_CONTAINER_SUCCESS && read_io->status != VC_CONTAINER_ERROR_EOS)
   {
      LOG_INFO(NULL, "Read failed: %d\n", read_io->status);
   }

   if (write_io->status != VC_CONTAINER_SUCCESS)
   {
      LOG_INFO(NULL, "Write failed: %d\n", write_io->status);
   }

   vc_container_io_close(read_io);
   vc_container_io_close(write_io);

   nb_set_nonblocking_input(0);

   return 0;
}
