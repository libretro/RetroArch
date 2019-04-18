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

/* ---- Include Files ---------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "interface/vmcs_host/vc_vchi_gencmd.h"

int main( int argc, char **argv )
{
   int instNum = 0;
   VCHI_INSTANCE_T vchi_instance;
   VCHI_CONNECTION_T *vchi_connection = NULL;

   if ( argc > 1 )
   {
       if (( strcmp( argv[1], "0" ) == 0 ) || ( strcmp( argv[1], "1" ) == 0 ))
       {
           instNum = atoi( argv[1] );
           argv++;
           argc--;
       }
   }

   vcos_init();

    if ( vchi_initialise( &vchi_instance ) != 0)
    {
        printf( "VCHI initialization failed\n" );
        return -1;
    }

    //create a vchi connection
    if ( vchi_connect( NULL, 0, vchi_instance ) != 0)
    {
        printf( "VCHI connection failed\n" );
        return -1;
    }

    vc_vchi_gencmd_init(vchi_instance, &vchi_connection, 1 );

    if (argc > 1)
    {
      int i = 1;
      char buffer[ 1024 ];
      size_t buffer_offset = 0;
      clock_t before=0, after=0;
      double time_diff;
      uint32_t show_time = 0;
      int ret;

      //reset the string
      buffer[0] = '\0';

      //first, strip out a potential leading -t
      if( strcmp( argv[1], "-t" ) == 0 )
      {
         show_time = 1;
         i++;
      }

      for (; i <= argc-1; i++)
      {
         buffer_offset = vcos_safe_strcpy( buffer, argv[i], sizeof(buffer), buffer_offset );
         buffer_offset = vcos_safe_strcpy( buffer, " ", sizeof(buffer), buffer_offset );
      }

      if( show_time )
         before = clock();

      //send the gencmd for the argument
      if (( ret = vc_gencmd_send( "%s", buffer )) != 0 )
      {
          printf( "vc_gencmd_send returned %d\n", ret );
      }

      //get + print out the response!
      if (( ret = vc_gencmd_read_response( buffer, sizeof( buffer ) )) != 0 )
      {
          printf( "vc_gencmd_read_response returned %d\n", ret );
      }

      if( show_time )
         after = clock();

      if( show_time )
      {
         time_diff = ((double) (after - before)) / CLOCKS_PER_SEC;

         printf( "Time took %f seconds (%f msecs) (%f usecs)\n", time_diff, time_diff * 1000, time_diff * 1000000 );
      }

      if ( buffer[0] != '\0' )
      {
          if ( buffer[ strlen( buffer) - 1] == '\n' )
          {
              fputs( buffer, stdout );
          }
          else
          {
              printf("%s\n", buffer );
          }
      }
    }

    vc_gencmd_stop();

    //close the vchi connection
    if ( vchi_disconnect( vchi_instance ) != 0)
    {
        printf( "VCHI disconnect failed\n" );
        return -1;
    }

   return 0;
}
