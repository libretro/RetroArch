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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "applog.h"
#include "vidtex.h"

#ifdef __ANDROID__
#include "launcher.h"
#else
#include "launcher_rpi.h"
#endif

VCOS_LOG_CAT_T app_log_category;

static int launch_vidtex(const void *params, EGLNativeWindowType win)
{
   return vidtex_run((const VIDTEX_PARAMS_T *)params, win);
}

static void usage(const char *argv0)
{
   fprintf(stderr, "Usage: %s [-d duration-ms] [-i] [uri]\n", argv0);
}

int main(int argc, char **argv)
{
   // Parse command line options/arguments.
   VIDTEX_PARAMS_T params = {0};
   int rc;
   int status = 0;

   opterr = 0;

   while ((rc = getopt(argc, argv, "d:iuvy")) != -1)
   {
      switch (rc)
      {
      case 'd':
         params.duration_ms = atoi(optarg);
         break;

      case 'i':
         params.opts |= VIDTEX_OPT_IMG_PER_FRAME;
         break;

      case 'y':
         params.opts |= VIDTEX_OPT_Y_TEXTURE;
         break;

      case 'u':
         params.opts |= VIDTEX_OPT_U_TEXTURE;
         break;

      case 'v':
         params.opts |= VIDTEX_OPT_V_TEXTURE;
         break;

      default:
         usage(argv[0]);
         return 2;
      }
   }

   if (optind < argc - 1)
   {
      usage(argv[0]);
      return 2;
   }

   if (optind < argc)
   {
      strncpy(params.uri, argv[optind], sizeof(params.uri) - 1);
      params.uri[sizeof(params.uri) - 1] = '\0';
   }

   // Init vcos logging.
   vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_INFO);
   vcos_log_register("vidtex", VCOS_LOG_CATEGORY);

   // Run video-on-texture application in a separate thread.
#ifdef __ANDROID__
   status = Launcher::runApp("vidtex", launch_vidtex, &params, sizeof(params));
#else
   status = runApp("vidtex", launch_vidtex, &params, sizeof(params));
#endif
   return (status == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
