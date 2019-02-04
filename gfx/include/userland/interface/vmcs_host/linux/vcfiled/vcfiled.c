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

/**
  * @file
  *
  * Daemon serving files to VideoCore from the host filing system.
  *
  */

#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <syslog.h>
#include <fcntl.h>
#ifdef ANDROID
#include <android/log.h>
#endif

#include "vchiq.h"
#include "interface/vchi/vchi.h"
#include "interface/vcos/vcos.h"
#include "interface/vmcs_host/vc_vchi_filesys.h"
#include "vchost.h"
#include "vcfiled_check.h"

static int log_stderr;                /** Debug output to stderr? */
static int foreground;                /** Don't fork - run in foreground? */
static const char *work_dir = "/";    /** Working directory */
static const char *lock_prefix = "";  /** Lock file prefix */
static int lock_prefix_set = 0;
static const char *progname;

VCHI_INSTANCE_T global_initialise_instance;
VCHI_CONNECTION_T *global_connection;

void vc_host_get_vchi_state(VCHI_INSTANCE_T *initialise_instance, VCHI_CONNECTION_T **connection)
{
   *initialise_instance = global_initialise_instance;
   *connection = global_connection;
}

static void logmsg(int level, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);

   if (log_stderr)
   {
      vfprintf(stderr, fmt, ap);
   }
   else
   {
#ifdef ANDROID
      __android_log_vprint(level, "vcfiled", fmt, ap);
#else
      vsyslog(level | LOG_DAEMON, fmt, ap);
#endif
   }
   va_end(ap);
}

static void usage(const char *progname)
{

   fprintf(stderr,"usage: %s [-debug] [-foreground] [-root dir] [-lock prefix]\n",
           progname);
   fprintf(stderr,"  --debug       - debug to stderr rather than syslog\n");
   fprintf(stderr,"  --foreground  - do not fork, stay in foreground\n");
   fprintf(stderr,"  --root dir    - chdir to this directory first\n");
   fprintf(stderr,"  --lock prefix - prefix to append to VCFILED_LOCKFILE\n");
}

enum { OPT_DEBUG, OPT_FG, OPT_ROOT, OPT_LOCK };

static void parse_args(int argc, char *const *argv)
{
   int finished = 0;
   static struct option long_options[] =
   {
      {"debug", no_argument, &log_stderr, 1},
      {"foreground", no_argument, &foreground, 1},
      {"root", required_argument, NULL, OPT_ROOT},
      {"lock", required_argument, NULL, OPT_LOCK},
      {0, 0, 0, 0},
   };
   while (!finished)
   {
      int option_index = 0;
      int c = getopt_long_only(argc, argv, "", long_options, &option_index);

      switch (c)
      {
      case 0:
         // debug or foreground options, just sets flags directly
         break;
      case OPT_ROOT:
         work_dir = optarg;
         // sanity check directory
         if (chdir(work_dir) < 0)
         {
            fprintf(stderr,"cannot chdir to %s: %s\n", work_dir, strerror(errno));
            exit(-1);
         }
         break;
      case OPT_LOCK:
         lock_prefix = optarg;
         lock_prefix_set = 1;
         break;

      default:
      case '?':
         usage(argv[0]);
         exit(-1);
         break;

      case -1:
         finished = 1;
         break;
      }

   }
}

int main(int argc, char *const *argv)
{
   VCHIQ_INSTANCE_T vchiq_instance;
   int success;
   char lockfile[128];

   progname = argv[0];
   const char *sep = strrchr(progname, '/');
   if (sep)
      progname = sep+1;

   parse_args(argc, argv);

   if (!foreground)
   {
      if ( daemon( 0, 1 ) != 0 )
      {
         fprintf( stderr, "Failed to daemonize: errno = %d", errno );
         return -1;
      }
      log_stderr = 0;
   }
   if ( lock_prefix_set )
   {
      vcos_safe_sprintf( lockfile, sizeof(lockfile), 0, "%s/%s", lock_prefix, VCFILED_LOCKFILE );
   }
   else
   {
      vcos_safe_sprintf( lockfile, sizeof(lockfile), 0, "%s", VCFILED_LOCKFILE );
   }
   success = vcfiled_lock(lockfile, logmsg);
   if (success != 0)
   {
      return -1;
   }

   logmsg( LOG_INFO, "vcfiled started" );

   vcos_init();

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      logmsg(LOG_ERR, "%s: failed to open vchiq instance\n", argv[0]);
      return -1;
   }

   success = vchi_initialise( &global_initialise_instance);
   vcos_assert(success == 0);
   vchiq_instance = (VCHIQ_INSTANCE_T)global_initialise_instance;

   global_connection = vchi_create_connection(single_get_func_table(),
                                              vchi_mphi_message_driver_func_table());

   vchi_connect(&global_connection, 1, global_initialise_instance);
  
   vc_vchi_filesys_init (global_initialise_instance, &global_connection, 1);

   for (;;)
   {
      sleep(INT_MAX);
   }

   vcos_deinit ();

   return 0;
}

