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
#include "mmalcam.h"

#include "interface/mmal/mmal_logging.h"

#define VIEWFINDER_LAYER      2
#define DEFAULT_VIDEO_FORMAT  "1280x720:h264";
#define DEFAULT_BIT_RATE      5000000
#define DEFAULT_CAM_NUM       0

struct {
   const char *name;
   MMALCAM_CHANGE_T value;
} mmalcam_change_table[] = {
   { "image_effect", MMALCAM_CHANGE_IMAGE_EFFECT },
   { "rotation", MMALCAM_CHANGE_ROTATION },
   { "zoom", MMALCAM_CHANGE_ZOOM },
   { "focus", MMALCAM_CHANGE_FOCUS },
   { "drc", MMALCAM_CHANGE_DRC },
   { "hdr", MMALCAM_CHANGE_HDR },
   { "contrast", MMALCAM_CHANGE_CONTRAST },
   { "brightness", MMALCAM_CHANGE_BRIGHTNESS },
   { "saturation", MMALCAM_CHANGE_SATURATION },
   { "sharpness", MMALCAM_CHANGE_SHARPNESS },
};

static int stop;
static VCOS_THREAD_T camcorder_thread;
static MMALCAM_BEHAVIOUR_T camcorder_behaviour;
static uint32_t sleepy_time;
static MMAL_BOOL_T stopped_already;

/* Utility functions used by test program */
static void *test_mmal_camcorder(void *id);
static void test_signal_handler(int signum);
static void test_mmalcam_dump_stats(const char *title, MMAL_PARAMETER_STATISTICS_T* stats);
static int test_parse_cmdline(int argc, const char **argv);

/*****************************************************************************/
int main(int argc, const char **argv)
{
   VCOS_THREAD_ATTR_T attrs;
   VCOS_STATUS_T status;
   int result = 0;

   vcos_log_register("mmalcam", VCOS_LOG_CATEGORY);
   printf("MMAL Camera Test App\n");
   signal(SIGINT, test_signal_handler);

   camcorder_behaviour.layer = VIEWFINDER_LAYER;
   camcorder_behaviour.vformat = DEFAULT_VIDEO_FORMAT;
   camcorder_behaviour.zero_copy = 1;
   camcorder_behaviour.bit_rate = DEFAULT_BIT_RATE;
   camcorder_behaviour.focus_test = MMAL_PARAM_FOCUS_MAX;
   camcorder_behaviour.camera_num = DEFAULT_CAM_NUM;

   if(test_parse_cmdline(argc, argv))
   {
      result = -1;
      goto error;
   }

   status = vcos_semaphore_create(&camcorder_behaviour.init_sem, "mmalcam-init", 0);
   vcos_assert(status == VCOS_SUCCESS);

   vcos_thread_attr_init(&attrs);
   if (vcos_thread_create(&camcorder_thread, "mmal camcorder", &attrs, test_mmal_camcorder, &camcorder_behaviour) != VCOS_SUCCESS)
   {
      LOG_ERROR("Thread creation failure");
      result = -2;
      goto error;
   }

   vcos_semaphore_wait(&camcorder_behaviour.init_sem);
   if (camcorder_behaviour.init_result != MMALCAM_INIT_SUCCESS)
   {
      LOG_ERROR("Initialisation failed: %d", camcorder_behaviour.init_result);
      result = (int)camcorder_behaviour.init_result;
      goto error;
   }

   if (sleepy_time != 0)
   {
      sleep(sleepy_time);
      stop = 1;
   }

error:
   LOG_TRACE("Waiting for camcorder thread to terminate");
   vcos_thread_join(&camcorder_thread, NULL);

   test_mmalcam_dump_stats("Render", &camcorder_behaviour.render_stats);
   if (camcorder_behaviour.uri)
      test_mmalcam_dump_stats("Encoder", &camcorder_behaviour.encoder_stats);

   vcos_semaphore_delete(&camcorder_behaviour.init_sem);
   return result;
}

/*****************************************************************************/
static void *test_mmal_camcorder(void *id)
{
   MMALCAM_BEHAVIOUR_T *behaviour = (MMALCAM_BEHAVIOUR_T *)id;
   int value;

   value = test_mmal_start_camcorder(&stop, behaviour);

   LOG_TRACE("Thread terminating, result %d", value);
   return (void *)value;
}

/*****************************************************************************/
static void test_signal_handler(int signum)
{
   (void)signum;

   if (stopped_already)
   {
      LOG_ERROR("Killing program");
      exit(255);
   }
   else
   {
      LOG_ERROR("Stopping normally. CTRL+C again to kill program");
      stop = 1;
      stopped_already = 1;
   }
}

/*****************************************************************************/
static void test_mmalcam_dump_stats(const char *title, MMAL_PARAMETER_STATISTICS_T* stats)
{
   printf("[%s]\n", title);
   printf("buffer_count: %u\n", stats->buffer_count);
   printf("frame_count: %u\n", stats->frame_count);
   printf("frames_skipped: %u\n", stats->frames_skipped);
   printf("frames_discarded: %u\n", stats->frames_discarded);
   printf("eos_seen: %u\n", stats->eos_seen);
   printf("maximum_frame_bytes: %u\n", stats->maximum_frame_bytes);
   printf("total_bytes_hi: %u\n", (uint32_t)(stats->total_bytes >> 32));
   printf("total_bytes_lo: %u\n", (uint32_t)(stats->total_bytes));
   printf("corrupt_macroblocks: %u\n", stats->corrupt_macroblocks);
}

/*****************************************************************************/
static MMAL_BOOL_T test_mmalcam_parse_rect(const char *str, MMAL_RECT_T *rect)
{
   /* coverity[secure_coding] Only reading integers, so can't overflow */
   return sscanf(str, "%d,%d,%d,%d", &rect->x, &rect->y, &rect->width, &rect->height) == 4;
}

/*****************************************************************************/
static int test_parse_cmdline(int argc, const char **argv)
{
   int i;
   int passed_options = 0;

   /* Parse the command line arguments */
   for(i = 1; i < argc; i++)
   {
      if (!argv[i]) continue;

      if (passed_options || argv[i][0] != '-')
      {
         /* Non-option argument */
         continue;
      }

      /* We are now dealing with command line options */
      switch(argv[i][1])
      {
      case '-': passed_options = 1; break;
      case 'h': goto usage;
      case 'o': if (i+1 >= argc) goto invalid_option;
         camcorder_behaviour.uri = argv[++i];
         break;
      case 'v': if (i+1 >= argc) goto invalid_option;
         camcorder_behaviour.vformat = argv[i+1];
         break;
      case 'r': if (i+1 >= argc) goto invalid_option;
         if (!test_mmalcam_parse_rect(argv[i+1], &camcorder_behaviour.display_area)) goto invalid_option;
         i++;
         break;
      case 'c': if (i+2 >= argc) goto invalid_option;
         {
            uint32_t table_index;

            if (sscanf(argv[i+1], "%u", &camcorder_behaviour.seconds_per_change) != 1) goto invalid_option;

            for (table_index = 0; table_index < countof(mmalcam_change_table); table_index++)
               if (strcmp(mmalcam_change_table[table_index].name, argv[i+2]) == 0)
                  break;
            if (table_index >= countof(mmalcam_change_table)) goto invalid_option;

            camcorder_behaviour.change = mmalcam_change_table[table_index].value;
         }
         break;
      case 't': if (i+1 >= argc) goto invalid_option;
         if (sscanf(argv[i+1], "%u", &sleepy_time) != 1) goto invalid_option;
         i++;
         break;
      case 'f': if (i+1 >= argc) goto invalid_option;
         camcorder_behaviour.frame_rate.den = 1;
         if (sscanf(argv[i+1], "%u/%u", &camcorder_behaviour.frame_rate.num, &camcorder_behaviour.frame_rate.den) == 0) goto invalid_option;
         i++;
         break;
      case 'x': camcorder_behaviour.tunneling = 1; break;
      case 'z': camcorder_behaviour.zero_copy = (argv[i][2] != '!'); break;
      case 'O': camcorder_behaviour.opaque = 1; break;
      case 'b': if (i+1 >= argc) goto invalid_option;
         if (sscanf(argv[i+1], "%u", &camcorder_behaviour.bit_rate) == 0) goto invalid_option;
         i++;
         break;
      case 'a': if (i+1 >= argc) goto invalid_option;
         if (sscanf(argv[i+1], "%u", &camcorder_behaviour.focus_test) == 0) goto invalid_option;
         if (camcorder_behaviour.focus_test > MMAL_PARAM_FOCUS_EDOF) goto invalid_option;
         i++;
         break;
      case 'n': if (i+1 >= argc) goto invalid_option;
         if (sscanf(argv[i+1], "%u", &camcorder_behaviour.camera_num) == 0) goto invalid_option;
         i++;
         break;
      default: goto invalid_option;
      }
      continue;
   }

   return 0;

 invalid_option:
   printf("invalid command line option (%s)\n", argv[i]);

 usage:
   {
      const char *program;

      program = strrchr(argv[0], '\\');
      if (program)
         program++;
      else
      {
         program = strrchr(argv[0], '/');
         if (program)
            program++;
         else
            program = argv[0];
      }
      printf("usage: %s [options]\n", program);
      printf("options list:\n");
      printf(" -h          : help\n");
      printf(" -o <file>   : write encoded output to <file>\n");
      printf(" -v <format> : set video resolution and encoding format (defaults to '1280x720:h264')\n");
      printf(" -r <r>      : put viewfinder at position <r>, given as x,y,width,height\n");
      printf(" -c <n> <x>  : change camera parameter every <n> seconds.\n");
      printf("                The parameter changed is defined by <x>, one of\n");
      printf("                image_effect, rotation, zoom, focus, hdr, drc, contrast,\n");
      printf("                brightness, saturation, sharpness\n");
      printf(" -t <n>      : operate camera for <n> seconds\n");
      printf(" -f <n>[/<d>]: set camera frame rate to <n>/<d>, where <d> is 1 if not given\n");
      printf(" -x          : use tunneling\n");
      printf(" -z          : use zero copy buffers (default)\n");
      printf(" -z!         : use full copy buffers\n");
      printf(" -O          : use opaque images\n");
      printf(" -b <n>      : use <n> as the bitrate (bits/s)\n");
      printf(" -a <n>      : Set to focus mode <n> (autofocus will cycle). Use MMAL_PARAM_FOCUS_T values.\n");
      printf(" -n <n>      : Set camera number <n>. Use MMAL_PARAMETER_CAMERA_NUM values.\n");
   }
   return 1;
}
