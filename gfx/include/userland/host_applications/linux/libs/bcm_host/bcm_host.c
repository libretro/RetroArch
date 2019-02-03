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
#include "interface/vmcs_host/vc_dispmanx.h"
#include "interface/vmcs_host/vc_vchi_gencmd.h"
#include "interface/vmcs_host/vc_vchi_bufman.h"
#include "interface/vmcs_host/vc_tvservice.h"
#include "interface/vmcs_host/vc_cecservice.h"
#include "interface/vchiq_arm/vchiq_if.h"

static VCHI_INSTANCE_T global_initialise_instance;
static VCHI_CONNECTION_T *global_connection;

int32_t graphics_get_display_size( const uint16_t display_number,
                                                    uint32_t *width,
                                                    uint32_t *height)
{
   DISPMANX_DISPLAY_HANDLE_T display_handle = 0;
   DISPMANX_MODEINFO_T mode_info;
   int32_t success = -1;

   // Display must be opened first.
   display_handle = vc_dispmanx_display_open(display_number);

   if (display_handle) {
      success = vc_dispmanx_display_get_info(display_handle, &mode_info);

      if( success >= 0 )
      {
         if( NULL != width )
         {
            *width = mode_info.width;
         }

         if( NULL != height )
         {
            *height = mode_info.height;
         }
      }
      vc_dispmanx_display_close(display_handle);
      display_handle = 0;
   }

   return success;
}

void host_app_message_handler(void)
{
   printf("host_app_message_handler\n");
}

void vc_host_get_vchi_state(VCHI_INSTANCE_T *initialise_instance, VCHI_CONNECTION_T **connection)
{
   if (initialise_instance) *initialise_instance = global_initialise_instance;
   if (connection) *connection = global_connection;
}

void bcm_host_init(void)
{
   VCHIQ_INSTANCE_T vchiq_instance;
   static int initted;
   int success = -1;
   char response[ 128 ];

   if (initted)
	return;
   initted = 1;
   vcos_init();

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      exit(-1);
   }

   vcos_log("vchi_initialise");
   success = vchi_initialise( &global_initialise_instance);
   vcos_assert(success == 0);
   vchiq_instance = (VCHIQ_INSTANCE_T)global_initialise_instance;

   global_connection = vchi_create_connection(single_get_func_table(),
                                              vchi_mphi_message_driver_func_table());

   vcos_log("vchi_connect");
   vchi_connect(&global_connection, 1, global_initialise_instance);

   vc_vchi_gencmd_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_dispmanx_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_tv_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_cec_init (global_initialise_instance, &global_connection, 1);
   //vc_vchi_bufman_init (global_initialise_instance, &global_connection, 1);

   if ( success == 0 )
   {
      success = vc_gencmd( response, sizeof(response), "set_vll_dir /sd/vlls" );
      vcos_assert( success == 0 );
   }
}

void bcm_host_deinit(void)
{
}

// Fix linking problems. These are referenced by libs, but shouldn't be called
void wfc_stream_await_buffer(void * stream)
{
   vcos_assert(0);
}

static unsigned get_dt_ranges(const char *filename, unsigned offset)
{
   unsigned address = ~0;
   FILE *fp = fopen(filename, "rb");
   if (fp)
   {
      unsigned char buf[4];
      fseek(fp, offset, SEEK_SET);
      if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
         address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
      fclose(fp);
   }
   return address;
}

unsigned bcm_host_get_peripheral_address(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
   return address == ~0 ? 0x20000000 : address;
}

unsigned bcm_host_get_peripheral_size(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
   return address == ~0 ? 0x01000000 : address;
}

unsigned bcm_host_get_sdram_address(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/axi/vc_mem/reg", 8);
   return address == ~0 ? 0x40000000 : address;
}

