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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "mmalplay.h"

#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_encodings.h"
#include "interface/mmal/util/mmal_util.h"

#define URI_FOR_THREAD_NAME_MAX  16
#define THREAD_PREFIX   "mmal:"
#define VERSION "0.9"

/* Global options */
static MMALPLAY_OPTIONS_T options;
static int unclean_exit;

typedef struct {
   VCOS_MUTEX_T lock;
   VCOS_THREAD_T thread;
   const char *uri;
   MMALPLAY_T *ctx;
   MMALPLAY_OPTIONS_T options;
   MMAL_STATUS_T status;
   char name[sizeof(THREAD_PREFIX) + URI_FOR_THREAD_NAME_MAX];
} FILE_PLAY_INFO_T;

#define FILE_PLAY_MAX   16
static FILE_PLAY_INFO_T play_info[FILE_PLAY_MAX];
static int play_info_count;
static uint32_t sleepy_time;
static unsigned int verbosity;

/* Utility functions used by test program */
void test_signal_handler(int signum);
static void *mmal_playback(void *id);
static int test_parse_cmdline(int argc, const char **argv);

/*****************************************************************************/
int main(int argc, const char **argv)
{
   VCOS_THREAD_ATTR_T attrs;
   int i;

   vcos_init();
   signal(SIGINT, test_signal_handler);

   /* coverity[tainted_data] Ignore unnecessary warning about an attacker
    * being able to pass an arbitrarily long "-vvvvv..." argument */
   if (test_parse_cmdline(argc, argv))
      return -1;

   if (verbosity--)
   {
      static char value[512];
      const char *levels[] = {"warn", "info", "trace"};
      char *env = getenv("VC_LOGLEVEL");
      if (verbosity >= MMAL_COUNTOF(levels)) verbosity = MMAL_COUNTOF(levels) - 1;
      snprintf(value, sizeof(value)-1, "mmalplay:%s,mmal:%s,%s",
               levels[verbosity], levels[verbosity], env ? env : "");
      setenv("VC_LOGLEVEL", value, 1);
   }

   vcos_log_register("mmalplay", VCOS_LOG_CATEGORY);
   LOG_INFO("MMAL Video Playback Test App");

   vcos_thread_attr_init(&attrs);
   for (i = 0; i < play_info_count; i++)
   {
      const char *uri = play_info[i].uri;

      memcpy(play_info[i].name, THREAD_PREFIX, sizeof(THREAD_PREFIX));
      if (strlen(uri) >= URI_FOR_THREAD_NAME_MAX)
         uri += strlen(uri) - URI_FOR_THREAD_NAME_MAX;
      strncat(play_info[i].name, uri, URI_FOR_THREAD_NAME_MAX);

      vcos_mutex_create(&play_info[i].lock, "mmalplay");
      play_info[i].options.render_layer = i;

      if (vcos_thread_create(&play_info[i].thread, play_info[i].name, &attrs, mmal_playback, &play_info[i]) != VCOS_SUCCESS)
      {
         LOG_ERROR("Thread creation failure for URI %s", play_info[i].uri);
         return -2;
      }
   }

   if (sleepy_time != 0)
   {
      sleep(sleepy_time);
      for (i = 0; i < play_info_count; i++)
      {
         vcos_mutex_lock(&play_info[i].lock);
         if (play_info[i].ctx)
            mmalplay_stop(play_info[i].ctx);
         vcos_mutex_unlock(&play_info[i].lock);
      }
   }

   LOG_TRACE("Waiting for threads to terminate");
   for (i = 0; i < play_info_count; i++)
   {
      vcos_thread_join(&play_info[i].thread, NULL);
      LOG_TRACE("Joined thread %d (%i)", i, play_info[i].status);
   }

   LOG_TRACE("Completed");

   /* Check for errors */
   for (i = 0; i < play_info_count; i++)
   {
      if (!play_info[i].status)
         continue;

      LOG_ERROR("Playback of %s failed (%i, %s)", play_info[i].uri,
                play_info[i].status,
                mmal_status_to_string(play_info[i].status));
      fprintf(stderr, "playback of %s failed (%i, %s)\n", play_info[i].uri,
              play_info[i].status,
              mmal_status_to_string(play_info[i].status));
      return play_info[i].status;
   }

   return 0;
}

/*****************************************************************************/
static void *mmal_playback(void *id)
{
   FILE_PLAY_INFO_T *play_info = id;
   MMALPLAY_OPTIONS_T opts;
   MMAL_STATUS_T status;
   MMALPLAY_T *ctx;

   /* Setup the options */
   opts = options;
   opts.output_uri = play_info->options.output_uri;
   opts.render_layer = play_info->options.render_layer;

   vcos_mutex_lock(&play_info->lock);
   ctx = mmalplay_create(play_info->uri, &opts, &status);
   play_info->ctx = ctx;
   vcos_mutex_unlock(&play_info->lock);
   if (!ctx)
      goto end;

   if (!opts.disable_playback)
      status = mmalplay_play(ctx);

   if (unclean_exit)
      goto end;

   vcos_mutex_lock(&play_info->lock);
   if (ctx)
   {
      /* coverity[use] Suppress ATOMICITY warning - ctx might have changed since
       * we initialised it above, which is okay */
      mmalplay_destroy(ctx);
   }
   play_info->ctx = 0;
   vcos_mutex_unlock(&play_info->lock);

 end:
   LOG_TRACE("Thread %s terminating, result %d", play_info->name, status);
   play_info->status = status;
   return NULL;
}

/*****************************************************************************/
void test_signal_handler(int signum)
{
   static MMAL_BOOL_T stopped_already = 0;
   int i;
   MMAL_PARAM_UNUSED(signum);

   if (stopped_already)
   {
      LOG_ERROR("Killing program");
      exit(255);
   }
   stopped_already = 1;

   LOG_ERROR("BYE");
   for (i = 0; i < play_info_count; i++)
   {
      vcos_mutex_lock(&play_info[i].lock);
      if (play_info[i].ctx)
         mmalplay_stop(play_info[i].ctx);
      vcos_mutex_unlock(&play_info[i].lock);
   }
}

/* Parse a string of the form: "fourcc:widthxheight" */
static int get_format(const char *name, uint32_t *fourcc, unsigned int *width, unsigned int *height)
{
   char *delim, fcc[4] = {' ', ' ', ' ', ' '};
   unsigned int value_u1, value_u2;
   size_t size;

   *width = *height = 0;
   *fourcc = MMAL_ENCODING_UNKNOWN;

   /* Fourcc is the first element */
   delim = strchr(name, ':');
   size = delim ? (size_t)(delim - name) : strlen(name);
   memcpy(fcc, name, MMAL_MIN(size, sizeof(fcc)));

   if (size == sizeof("yuv420")-1 && !memcmp(name, "yuv420", size))
      *fourcc = MMAL_ENCODING_I420;
   else if (size == sizeof("yuvuv")-1 && !memcmp(name, "yuvuv", size))
      *fourcc = MMAL_ENCODING_YUVUV128;
   else if (size == sizeof("opaque")-1 && !memcmp(name, "opaque", size))
      *fourcc = MMAL_ENCODING_OPAQUE;
   else if (size > 0 && size <= 4)
      *fourcc = MMAL_FOURCC(fcc[0], fcc[1], fcc[2], fcc[3]);
   else
      return 1;

   if (!delim)
      return 0; /* Nothing more to parse */

   /* Width/height are next */
   /* coverity[secure_coding] Only reading integers, so can't overflow */
   if (sscanf(delim+1, "%ux%u", &value_u1, &value_u2) != 2)
      return 1;

   *width = value_u1;
   *height = value_u2;
   return 0;
}

/*****************************************************************************/
static int test_parse_cmdline(int argc, const char **argv)
{
   unsigned int value_u1 = 0, value_u2 = 0;
   uint32_t color_format;
   float value_f = 0;
   int i, j;

   /* Parse the command line arguments */
   for(i = 1; i < argc; i++)
   {
      if(!argv[i]) continue;

      if(argv[i][0] != '-')
      {
         /* Not an option argument so will be the input URI */
         if (play_info_count >= FILE_PLAY_MAX)
         {
            fprintf(stderr, "Too many URIs!\n");
            goto usage;
         }
         play_info[play_info_count++].uri = argv[i];
         continue;
      }

      /* We are now dealing with command line options */
      switch(argv[i][1])
      {
      case 'V':
         printf("Version: %s\n", VERSION);
         exit(0);
      case 'v':
         for (j = 1; argv[i][j] == 'v'; j++) verbosity++;
         break;
      case 'X':
         unclean_exit = 1;
         break;
      case 'd':
         options.tunnelling = 1;
         break;
      case 's':
         if (!strcmp(argv[i]+1, "step"))
         {
            options.stepping = 1;
            break;
         }
         /* coverity[secure_coding] Only reading numbers, so can't overflow */
         else if (!argv[i][2] && ++i < argc &&
                  sscanf(argv[i], "%f", &value_f) == 1)
         {
            options.seeking = value_f;
            break;
         }
         goto usage;
      case 'n':
         switch (argv[i][2])
         {
            case 'p': options.disable_playback = 1; break;
            case 'v':
               if(argv[i][3] == 'd')
                  options.disable_video_decode = 1;
               else
                  options.disable_video = 1;
               break;
            case 'a': options.disable_audio = 1; break;
            default: break;
         }
         break;
      case 'e':
         if (argv[i][2] != 's')
            goto usage;
         options.enable_scheduling = 1; break;
         break;
      case 't':
         /* coverity[secure_coding] Only reading integers, so can't overflow */
         if (++i >= argc || sscanf(argv[i], "%u", &sleepy_time) != 1)
            goto usage;    /* Time missing / invalid */
         break;
      case 'x':
         /* coverity[secure_coding] Only reading integers, so can't overflow */
         if (++i >= argc || sscanf(argv[i], "%u", &value_u1) != 1)
            goto usage;
         options.output_num = value_u1;
         break;
      case 'f':
         if (i + 1 >= argc || get_format(argv[++i], &color_format, &value_u1, &value_u2))
            goto usage;
         if (argv[i-1][2] == 0)
         {
            options.output_format = color_format;
            options.output_rect.width = value_u1;
            options.output_rect.height = value_u2;
         }
         else if (argv[i-1][2] == 'r')
         {
            options.render_format = color_format;
            options.render_rect.width = value_u1;
            options.render_rect.height = value_u2;
         }
         else
            goto usage;
         break;
      case 'c':
         if (!argv[i][2])
         {
            options.copy_input = 1;
            options.copy_output = 1;
         }
         else if (argv[i][2] == 'i')
            options.copy_input = 1;
         else if (argv[i][2] == 'o')
            options.copy_output = 1;
         break;
      case 'm':
         if (argv[i][2] == 'v')
         {
            if (argv[i][3] == 'r' && i < argc)
               options.component_video_render = argv[++i];
            else if (argv[i][3] == 'd' && i < argc)
               options.component_video_decoder = argv[++i];
            else if (argv[i][3] == 's' && i < argc)
               options.component_splitter = argv[++i];
            else if (argv[i][3] == 'c' && i < argc)
               options.component_video_converter = argv[++i];
            else if (argv[i][3] == 'h' && i < argc)
               options.component_video_scheduler = argv[++i];
            else
               goto usage;
         }
         else if (argv[i][2] == 'a')
         {
            if (argv[i][3] == 'r' && i < argc)
               options.component_audio_render = argv[++i];
            else if (argv[i][3] == 'd' && i < argc)
               options.component_audio_decoder = argv[++i];
            else
               goto usage;
         }
         else if (argv[i][2] == 'c')
         {
            if (argv[i][3] == 'r' && i < argc)
               options.component_container_reader = argv[++i];
            else
               goto usage;
         }
         else
            goto usage;
         break;
      case 'o':
         if (++i >= argc || play_info_count >= FILE_PLAY_MAX)
            goto usage;
         play_info[play_info_count].options.output_uri = argv[i];
         break;

      case 'a':
         if (++i >= argc)
            goto usage;
         options.audio_destination = argv[i];
         break;

      case 'r':
         if (i + 1 >= argc)
            goto usage;
         if (argv[i][2] == 'a')
            options.audio_destination = argv[++i];
         else if (argv[i][2] == 'v')
         {
            /* coverity[secure_coding] Only reading integers, so can't overflow */
            if (sscanf(argv[++i], "%u", &options.video_destination) != 1)
               goto usage;
         }
         else
            goto usage;
         break;

      case 'w':
         options.window = 1;
         break;

      case 'p':
         options.audio_passthrough = 1;
         break;

      case 'h': goto usage;
      default: goto invalid_option;
      }
      continue;
   }

   /* Sanity check that we have at least an input uri */
   if(!play_info_count)
   {
     fprintf(stderr, "missing uri argument\n");
     goto usage;
   }

   return 0;

invalid_option:
   fprintf(stderr, "invalid command line option (%s)\n", argv[i]);

usage:
   {
      const char *program;

      program = strrchr(argv[0], '\\');
      if (program)
         program++;
      if (!program)
      {
         program = strrchr(argv[0], '/');
         if (program)
            program++;
      }
      if (!program)
         program = argv[0];

      fprintf(stderr, "usage: %s [options] uri0 uri1 ... uriN\n", program);
      fprintf(stderr, "options list:\n");
      fprintf(stderr, " -h    : help\n");
      fprintf(stderr, " -V    : print version and exit\n");
      fprintf(stderr, " -v(vv): increase verbosity\n");
      fprintf(stderr, " -np   : disable playback phase\n");
      fprintf(stderr, " -nv   : disable video\n");
      fprintf(stderr, " -nvd  : disable video decode\n");
      fprintf(stderr, " -na   : disable audio\n");
      fprintf(stderr, " -es   : enable scheduling\n");
      fprintf(stderr, " -t <n>: play URI(s) for <n> seconds then stop\n");
      fprintf(stderr, " -f <fourcc:widthxheight> : set format used on output of decoder\n");
      fprintf(stderr, " -fr <fourcc:widthxheight> : set format used for rendering\n");
      fprintf(stderr, " -c    : do full copy of data transferred to videocore\n");
      fprintf(stderr, " -ci   : full copy for input buffers to decoder\n");
      fprintf(stderr, " -co   : full copy for output buffers from decoder\n");
      fprintf(stderr, " -X    : exit without tearing down the mmal pipeline (for testing)\n");
      fprintf(stderr, " -x <n>: use <n> video render modules \n");
      fprintf(stderr, " -d    : use direct port connections (aka tunnelling)\n");
      fprintf(stderr, " -step : stepping (displays 1 frame at a time)\n");
      fprintf(stderr, " -s <f>: seek to <f> seconds into the stream\n");
      fprintf(stderr, " -o <s>: output uri\n");
      fprintf(stderr, " -mcr <s>:  name of the container reader module to use\n");
      fprintf(stderr, " -mvr <s>:  name of the video render module to use\n");
      fprintf(stderr, " -mvd <s>:  name of the video decoder module to use\n");
      fprintf(stderr, " -mvc <s>:  name of the video converter module to use\n");
      fprintf(stderr, " -mvh <s>:  name of the video scheduler to use\n");
      fprintf(stderr, " -mvs <s>:  name of the splitter module to use\n");
      fprintf(stderr, " -mar <s>:  name of the audio render module to use\n");
      fprintf(stderr, " -mad <s>:  name of the audio decoder module to use\n");
      fprintf(stderr, " -ra <s>: set audio destination\n");
      fprintf(stderr, " -rv <n>: set video destination\n");
      fprintf(stderr, " -p    : use audio passthrough\n");
      fprintf(stderr, " -w    : window mode (i.e. not fullscreen)\n");
   }
   return 1;
}
