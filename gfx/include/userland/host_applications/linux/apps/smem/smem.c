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

// ---- Include Files -------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <user-vcsm.h>
#include "interface/vcos/vcos.h"

// ---- Public Variables ----------------------------------------------------

// ---- Private Constants and Types -----------------------------------------

// Note: Exact match 32 chars.
static char blah_blah[32] = "Testing shared memory interface!";

enum
{
   OPT_ALLOC     = 'a',
   OPT_STATUS    = 's',
   OPT_PID       = 'p',
   OPT_HELP      = 'h',

   // Options from this point onwards don't have any short option equivalents
   OPT_FIRST_LONG_OPT = 0x80,
};

static struct option long_opts[] =
{
   // name                 has_arg              flag     val
   // -------------------  ------------------   ----     ---------------
   { "alloc",              required_argument,   NULL,    OPT_ALLOC },
   { "pid",                required_argument,   NULL,    OPT_PID },
   { "status",             required_argument,   NULL,    OPT_STATUS },
   { "help",               no_argument,         NULL,    OPT_HELP },
   { 0,                    0,                   0,       0 }
};

// Maximum length of option string (3 characters max for each option + NULL)
#define OPTSTRING_LEN  ( sizeof( long_opts ) / sizeof( *long_opts ) * 3 + 1 )

// ---- Private Variables ---------------------------------------------------

static VCOS_LOG_CAT_T smem_log_category;
#define VCOS_LOG_CATEGORY (&smem_log_category)
static VCOS_EVENT_T quit_event;

// ---- Private Functions ---------------------------------------------------

static void show_usage( void )
{
   vcos_log_info( "Usage: smem [OPTION]..." );
   vcos_log_info( "  -a, --alloc=SIZE             Allocates a block of SIZE" );
   vcos_log_info( "  -p, --pid=PID                Use PID for desired action" );
   vcos_log_info( "  -s, --status=TYPE            Queries status of TYPE [for PID]" );
   vcos_log_info( "                all                    all (for current pid)" );
   vcos_log_info( "                vc                     videocore allocations" );
   vcos_log_info( "                map                    host map status" );
   vcos_log_info( "                map <pid>              host map status for pid" );
   vcos_log_info( "                host <pid>             host allocations for pid" );
   vcos_log_info( "  -h, --help                   Print this information" );
}

static void create_optstring( char *optstring )
{
   char *short_opts = optstring;
   struct option *option;

   // Figure out the short options from our options structure
   for ( option = long_opts; option->name != NULL; option++ )
   {
      if (( option->flag == NULL ) && ( option->val < OPT_FIRST_LONG_OPT ))
      {
         *short_opts++ = (char)option->val;

         if ( option->has_arg != no_argument )
         {
            *short_opts++ = ':';
         }

         // Optional arguments require two ':'
         if ( option->has_arg == optional_argument )
         {
            *short_opts++ = ':';
         }
      }
   }
   *short_opts++ = '\0';
}

static int get_status( VCSM_STATUS_T mode, int pid )
{
   switch ( mode )
   {
      case VCSM_STATUS_VC_WALK_ALLOC:
         vcsm_status( VCSM_STATUS_VC_WALK_ALLOC, -1 );
      break;

      case VCSM_STATUS_HOST_WALK_MAP:
         if ( pid != -1 )
         {
            vcsm_status( VCSM_STATUS_HOST_WALK_PID_MAP, pid );
         }
         else
         {
            vcsm_status( VCSM_STATUS_HOST_WALK_MAP, -1 );
         }
      break;

      case VCSM_STATUS_HOST_WALK_PID_ALLOC:
         vcsm_status( VCSM_STATUS_HOST_WALK_PID_ALLOC, pid );
      break;

      case VCSM_STATUS_VC_MAP_ALL:
         vcsm_status( VCSM_STATUS_VC_WALK_ALLOC, -1 );
         vcsm_status( VCSM_STATUS_HOST_WALK_MAP, -1 );
      break;

      default:
      break;
   }

   return 0;
}

static void control_c( int signum )
{
    (void)signum;

    vcos_log_info( "Shutting down..." );

    vcos_event_signal( &quit_event );
}

static int start_monitor( void )
{
   if ( vcos_event_create( &quit_event, "smem" ) != VCOS_SUCCESS )
   {
      vcos_log_info( "Failed to create quit event" );

      return -1;
   }

   // Handle the INT and TERM signals so we can quit
   signal( SIGINT, control_c );
   signal( SIGTERM, control_c );

   return 0;
}

// ---- Public Functions -----------------------------------------------------

// #define DOUBLE_ALLOC
#define RESIZE_ALLOC

int main( int argc, char **argv )
{
   int32_t ret;
   char optstring[OPTSTRING_LEN];
   int  opt;
   int  opt_alloc = 0;
   int  opt_status = 0;
   uint32_t alloc_size = 0;
   int  opt_pid = -1;
   VCSM_STATUS_T status_mode = VCSM_STATUS_NONE;

   void *usr_ptr_1;
   unsigned int usr_hdl_1;
#if defined(DOUBLE_ALLOC) || defined(RESIZE_ALLOC)
   void *usr_ptr_2;
   unsigned int usr_hdl_2;
#endif

   // Initialize VCOS
   vcos_init();

   vcos_log_set_level(&smem_log_category, VCOS_LOG_INFO);
   smem_log_category.flags.want_prefix = 0;
   vcos_log_register( "smem", &smem_log_category );

   // Create the option string that we will be using to parse the arguments
   create_optstring( optstring );

   // Parse the command line arguments
   while (( opt = getopt_long_only( argc, argv, optstring, long_opts,
                                    NULL )) != -1 )
   {
      switch ( opt )
      {
         case 0:
         {
            // getopt_long returns 0 for entries where flag is non-NULL
            break;
         }
         case OPT_ALLOC:
         {
            char *end;
            alloc_size = (uint32_t)strtoul( optarg, &end, 10 );
            if (end == optarg)
            {
               vcos_log_info( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            opt_alloc = 1;
            break;
         }
         case OPT_PID:
         {
            char *end;
            opt_pid = (int)strtol( optarg, &end, 10 );
            if (end == optarg)
            {
               vcos_log_info( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            break;
         }
         case OPT_STATUS:
         {
            char status_str[32];

            /* coverity[secure_coding] String length specified, so can't overflow */
            if ( sscanf( optarg, "%31s", status_str ) != 1 )
            {
               vcos_log_info( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            if ( vcos_strcasecmp( status_str, "all" ) == 0 )
            {
               status_mode = VCSM_STATUS_VC_MAP_ALL;
            }
            else if ( vcos_strcasecmp( status_str, "vc" ) == 0 )
            {
               status_mode = VCSM_STATUS_VC_WALK_ALLOC;
            }
            else if ( vcos_strcasecmp( status_str, "map" ) == 0 )
            {
               status_mode = VCSM_STATUS_HOST_WALK_MAP;
            }
            else if ( vcos_strcasecmp( status_str, "host" ) == 0 )
            {
               status_mode = VCSM_STATUS_HOST_WALK_PID_ALLOC;
            }
            else
            {
               goto err_out;
            }

            opt_status = 1;
            break;
         }
         default:
         {
            vcos_log_info( "Unrecognized option '%d'", opt );
            goto err_usage;
         }
         case '?':
         case OPT_HELP:
         {
            goto err_usage;
         }
      } // end switch
   } // end while

   argc -= optind;
   argv += optind;

   if (( optind == 1 ) || ( argc > 0 ))
   {
      if ( argc > 0 )
      {
         vcos_log_info( "Unrecognized argument -- '%s'", *argv );
      }

      goto err_usage;
   }

   // Start the shared memory support.
   if ( vcsm_init() == -1 )
   {
      vcos_log_info( "Cannot initialize smem device" );
      goto err_out;
   }

   if ( opt_alloc == 1 )
   {
      vcos_log_info( "Allocating 2 times %u-bytes in shared memory", alloc_size );

      usr_hdl_1 = vcsm_malloc( alloc_size,
                               "smem-test-alloc" );

      vcos_log_info( "Allocation 1 result: user %x, vc-hdl %x",
                     usr_hdl_1, vcsm_vc_hdl_from_hdl( usr_hdl_1 ) );

#if defined(DOUBLE_ALLOC) || defined(RESIZE_ALLOC)
      usr_hdl_2 = vcsm_malloc( alloc_size,
                               NULL );
      vcos_log_info( "Allocation 2 result: user %x",
                     usr_hdl_2 );

      usr_ptr_2 = vcsm_lock( usr_hdl_2 );
      vcos_log_info( "Allocation 2 : lock %p",
                     usr_ptr_2 );
      vcos_log_info( "Allocation 2 : unlock %d",
                     vcsm_unlock_hdl( usr_hdl_2 ) );
#endif

      // Do a simple write/read test.
      if ( usr_hdl_1 != 0 )
      {
         usr_ptr_1 = vcsm_lock( usr_hdl_1 );
         vcos_log_info( "Allocation 1 : lock %p",
                        usr_ptr_1 );
         if ( usr_ptr_1 )
         {
            memset ( usr_ptr_1,
                     0,
                     alloc_size );
            memcpy ( usr_ptr_1,
                     blah_blah,
                     32 );
            vcos_log_info( "Allocation 1 contains: \"%s\"",
                           (char *)usr_ptr_1 );

            vcos_log_info( "Allocation 1: vc-hdl %x",
                           vcsm_vc_hdl_from_ptr ( usr_ptr_1 ) );
            vcos_log_info( "Allocation 1: usr-hdl %x",
                           vcsm_usr_handle ( usr_ptr_1 ) );
            vcos_log_info( "Allocation 1 : unlock %d",
                           vcsm_unlock_ptr( usr_ptr_1 ) );
         }

         usr_ptr_1 = vcsm_lock( usr_hdl_1 );
         vcos_log_info( "Allocation 1 (relock) : lock %p",
                        usr_ptr_1 );
         if ( usr_ptr_1 )
         {
            vcos_log_info( "Allocation 1 (relock) : unlock %d",
                           vcsm_unlock_hdl( usr_hdl_1 ) );
         }
      }

#if defined(RESIZE_ALLOC)
      ret = vcsm_resize( usr_hdl_1, 2 * alloc_size );
      vcos_log_info( "Allocation 1 : resize %d", ret );
      if ( ret == 0 )
      {
         usr_ptr_1 = vcsm_lock( usr_hdl_1 );
         vcos_log_info( "Allocation 1 (resize) : lock %p",
                        usr_ptr_1 );
         if ( usr_ptr_1 )
         {
            memset ( usr_ptr_1,
                     0,
                     2 * alloc_size );
            memcpy ( usr_ptr_1,
                     blah_blah,
                     32 );
            vcos_log_info( "Allocation 1 (resized) contains: \"%s\"",
                           (char *)usr_ptr_1 );
            vcos_log_info( "Allocation 1 (resized) : unlock %d",
                           vcsm_unlock_ptr( usr_ptr_1 ) );
         }
      }

      // This checks that the memory can be remapped properly
      // because the Block 1 expanded beyond Block 2 boundary.
      //
      usr_ptr_2 = vcsm_lock( usr_hdl_2 );
      vcos_log_info( "Allocation 2 : lock %p",
                     usr_ptr_2 );
      vcos_log_info( "Allocation 2 : unlock %d",
                     vcsm_unlock_hdl( usr_hdl_2 ) );

      // This checks that we can free a memory block even if it
      // is locked, which could be the case if the application
      // dies.
      //
      usr_ptr_2 = vcsm_lock( usr_hdl_2 );
      vcos_log_info( "Allocation 2 : lock %p",
                     usr_ptr_2 );
      vcsm_free ( usr_hdl_2 );
#endif

#if defined(DOUBLE_ALLOC)
#endif
   }

   if ( opt_status == 1 )
   {
      get_status( status_mode, opt_pid );
   }

   // If we allocated something, wait for the signal to exit to give chance for the
   // user to poke around the allocation test.
   //
   if ( opt_alloc == 1 )
   {
      start_monitor();

      vcos_event_wait( &quit_event );
      vcos_event_delete( &quit_event );
   }

   // Terminate the shared memory support.
   vcsm_exit ();
   goto err_out;

err_usage:
   show_usage();

err_out:
   exit( 1 );
}
