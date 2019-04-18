/*
Copyright (c) 2018, Raspberry Pi (Trading) Ltd.
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, James Hughes
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

/*
 * \file RaspiStillYUV.c
 * Command line program to capture a still frame and dump uncompressed it to file.
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * Description
 *
 * 2 components are created; camera and preview.
 * Camera component has three ports, preview, video and stills.
 * Preview is connected using standard mmal connections, the stills output
 * is written straight to the file in YUV 420 format via the requisite buffer
 * callback. video port is not used
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 * We use the RaspiPreview code to handle the generic preview
 */

// We use some GNU extensions (basename)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>
#include <unistd.h>
#include <errno.h>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
#include "RaspiCommonSettings.h"
#include "RaspiHelpers.h"
#include "RaspiGPS.h"

#include <semaphore.h>

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

/// Frame advance method
enum
{
   FRAME_NEXT_SINGLE,
   FRAME_NEXT_TIMELAPSE,
   FRAME_NEXT_KEYPRESS,
   FRAME_NEXT_FOREVER,
   FRAME_NEXT_GPIO,
   FRAME_NEXT_SIGNAL,
   FRAME_NEXT_IMMEDIATELY
};

#define CAMERA_SETTLE_TIME       1000

int mmal_status_to_int(MMAL_STATUS_T status);

/** Structure containing all state information for the current run
 */
typedef struct
{
   RASPICOMMONSETTINGS_PARAMETERS common_settings; /// Non-app specific settings
   int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
   char *linkname;                     /// filename of output file
   int timelapse;                      /// Delay between each picture in timelapse mode. If 0, disable timelapse
   MMAL_FOURCC_T encoding;             /// Use a MMAL encoding other than YUV
   int fullResPreview;                 /// If set, the camera preview port runs at capture resolution. Reduces fps.
   int frameNextMethod;                /// Which method to use to advance to next frame
   int burstCaptureMode;               /// Enable burst mode
   int onlyLuma;                       /// Only output the luma / Y plane of the YUV data

   RASPIPREVIEW_PARAMETERS preview_parameters;    /// Preview setup parameters
   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
   MMAL_COMPONENT_T *null_sink_component;    /// Pointer to the camera component
   MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
   MMAL_POOL_T *camera_pool;              /// Pointer to the pool of buffers used by camera stills port
} RASPISTILLYUV_STATE;

/** Struct used to pass information in camera still port userdata to callback
 */
typedef struct
{
   FILE *file_handle;                   /// File handle to write buffer data to.
   VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
   RASPISTILLYUV_STATE *pstate;            /// pointer to our state in case required in callback
} PORT_USERDATA;

/// Comamnd ID's and Structure defining our command line options
enum
{
   CommandTimeout,
   CommandTimelapse,
   CommandUseRGB,
   CommandFullResPreview,
   CommandLink,
   CommandKeypress,
   CommandSignal,
   CommandBurstMode,
   CommandOnlyLuma,
   CommandUseBGR
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandTimeout, "-timeout",    "t",  "Time (in ms) before takes picture and shuts down. If not specified set to 5s", 1 },
   { CommandTimelapse,"-timelapse", "tl", "Timelapse mode. Takes a picture every <t>ms", 1},
   { CommandUseRGB,  "-rgb",        "rgb","Save as RGB data rather than YUV", 0},
   { CommandFullResPreview,"-fullpreview","fp", "Run the preview using the still capture resolution (may reduce preview fps)", 0},
   { CommandLink,    "-latest",     "l",  "Link latest complete image to filename <filename>", 1},
   { CommandKeypress,"-keypress",   "k",  "Wait between captures for a ENTER, X then ENTER to exit", 0},
   { CommandSignal,  "-signal",     "s",  "Wait between captures for a SIGUSR1 from another process", 0},
   { CommandBurstMode, "-burst",    "bm", "Enable 'burst capture mode'", 0},
   { CommandOnlyLuma,  "-luma",     "y",  "Only output the luma / Y of the YUV data'", 0},
   { CommandUseBGR,  "-bgr",        "bgr","Save as BGR data rather than YUV", 0},
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

static struct
{
   char *description;
   int nextFrameMethod;
} next_frame_description[] =
{
   {"Single capture",         FRAME_NEXT_SINGLE},
   {"Capture on timelapse",   FRAME_NEXT_TIMELAPSE},
   {"Capture on keypress",    FRAME_NEXT_KEYPRESS},
   {"Run forever",            FRAME_NEXT_FOREVER},
   {"Capture on GPIO",        FRAME_NEXT_GPIO},
   {"Capture on signal",      FRAME_NEXT_SIGNAL},
};

static int next_frame_description_size = sizeof(next_frame_description) / sizeof(next_frame_description[0]);

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status(RASPISTILLYUV_STATE *state)
{
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   // Default everything to zero
   memset(state, 0, sizeof(RASPISTILLYUV_STATE));

   raspicommonsettings_set_defaults(&state->common_settings);

   // Now set anything non-zero
   state->timeout = -1; // replaced with 5000ms later if unset
   state->timelapse = 0;
   state->linkname = NULL;
   state->fullResPreview = 0;
   state->frameNextMethod = FRAME_NEXT_SINGLE;
   state->burstCaptureMode=0;
   state->onlyLuma = 0;

   // Setup preview window defaults
   raspipreview_set_defaults(&state->preview_parameters);

   // Set up the camera_parameters to default
   raspicamcontrol_set_defaults(&state->camera_parameters);
}

/**
 * Dump image state parameters to stderr. Used for debugging
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void dump_status(RASPISTILLYUV_STATE *state)
{
   int i;
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   raspicommonsettings_dump_parameters(&state->common_settings);

   fprintf(stderr, "Time delay %d, Timelapse %d\n", state->timeout, state->timelapse);
   fprintf(stderr, "Link to latest frame enabled ");
   if (state->linkname)
   {
      fprintf(stderr, " yes, -> %s\n", state->linkname);
   }
   else
   {
      fprintf(stderr, " no\n");
   }
   fprintf(stderr, "Full resolution preview %s\n", state->fullResPreview ? "Yes": "No");

   fprintf(stderr, "Capture method : ");
   for (i=0; i<next_frame_description_size; i++)
   {
      if (state->frameNextMethod == next_frame_description[i].nextFrameMethod)
         fprintf(stderr, "%s", next_frame_description[i].description);
   }
   fprintf(stderr, "\n\n");

   raspipreview_dump_parameters(&state->preview_parameters);
   raspicamcontrol_dump_parameters(&state->camera_parameters);
}

/**
 * Display usage information for the application to stdout
 *
 * @param app_name String to display as the application name
 *
 */
static void application_help_message(char *app_name)
{
   fprintf(stdout, "Runs camera for specific time, and take uncompressed YUV or RGB capture at end if requested\n\n");
   fprintf(stdout, "usage: %s [options]\n\n", app_name);

   fprintf(stdout, "Image parameter commands\n\n");

   raspicli_display_help(cmdline_commands, cmdline_commands_size);

   return;
}

/**
 * Parse the incoming command line and put resulting parameters in to the state
 *
 * @param argc Number of arguments in command line
 * @param argv Array of pointers to strings from command line
 * @param state Pointer to state structure to assign any discovered parameters to
 * @return non-0 if failed for some reason, 0 otherwise
 */
static int parse_cmdline(int argc, const char **argv, RASPISTILLYUV_STATE *state)
{
   // Parse the command line arguments.
   // We are looking for --<something> or -<abbreviation of something>

   int valid = 1; // set 0 if we have a bad parameter
   int i;

   for (i = 1; i < argc && valid; i++)
   {
      int command_id, num_parameters;

      if (!argv[i])
         continue;

      if (argv[i][0] != '-')
      {
         valid = 0;
         continue;
      }

      // Assume parameter is valid until proven otherwise
      valid = 1;

      command_id = raspicli_get_command_id(cmdline_commands, cmdline_commands_size, &argv[i][1], &num_parameters);

      // If we found a command but are missing a parameter, continue (and we will drop out of the loop)
      if (command_id != -1 && num_parameters > 0 && (i + 1 >= argc) )
         continue;

      //  We are now dealing with a command line option
      switch (command_id)
      {
      case CommandLink :
      {
         int len = strlen(argv[i+1]);
         if (len)
         {
            state->linkname = malloc(len + 10);
            vcos_assert(state->linkname);
            if (state->linkname)
               strncpy(state->linkname, argv[i + 1], len+1);
            i++;
         }
         else
            valid = 0;
         break;

      }

      case CommandTimeout: // Time to run viewfinder for before taking picture, in seconds
      {
         if (sscanf(argv[i + 1], "%d", &state->timeout) == 1)
         {
            // Ensure that if previously selected CommandKeypress we don't overwrite it
            if (state->timeout == 0 && state->frameNextMethod == FRAME_NEXT_SINGLE)
               state->frameNextMethod = FRAME_NEXT_FOREVER;

            i++;
         }
         else
            valid = 0;
         break;
      }

      case CommandTimelapse:
         if (sscanf(argv[i + 1], "%u", &state->timelapse) != 1)
            valid = 0;
         else
         {
            if (state->timelapse)
               state->frameNextMethod = FRAME_NEXT_TIMELAPSE;
            else
               state->frameNextMethod = FRAME_NEXT_IMMEDIATELY;

            i++;
         }
         break;

      case CommandUseRGB: // display lots of data during run
         if (state->onlyLuma)
         {
            fprintf(stderr, "--luma and --rgb/--bgr are mutually exclusive\n");
            valid = 0;
         }
         state->encoding = MMAL_ENCODING_RGB24;
         break;

      case CommandFullResPreview:
         state->fullResPreview = 1;
         break;

      case CommandKeypress: // Set keypress between capture mode
         state->frameNextMethod = FRAME_NEXT_KEYPRESS;

         if (state->timeout == -1)
            state->timeout = 0;

         break;

      case CommandSignal:   // Set SIGUSR1 between capture mode
         state->frameNextMethod = FRAME_NEXT_SIGNAL;
         // Reenable the signal
         signal(SIGUSR1, default_signal_handler);

         if (state->timeout == -1)
            state->timeout = 0;

         break;

      case CommandBurstMode:
         state->burstCaptureMode=1;
         break;

      case CommandOnlyLuma:
         if (state->encoding)
         {
            fprintf(stderr, "--luma and --rgb are mutually exclusive\n");
            valid = 0;
         }
         state->onlyLuma = 1;
         break;

      case CommandUseBGR:
         if (state->onlyLuma)
         {
            fprintf(stderr, "--luma and --rgb/--bgr are mutually exclusive\n");
            valid = 0;
         }
         state->encoding = MMAL_ENCODING_BGR24;
         break;

      default:
      {
         // Try parsing for any image specific parameters
         // result indicates how many parameters were used up, 0,1,2
         // but we adjust by -1 as we have used one already
         const char *second_arg = (i + 1 < argc) ? argv[i + 1] : NULL;

         int parms_used = (raspicamcontrol_parse_cmdline(&state->camera_parameters, &argv[i][1], second_arg));

         // Still unused, try common settings
         if (!parms_used)
            parms_used = raspicommonsettings_parse_cmdline(&state->common_settings, &argv[i][1], second_arg, &application_help_message);

         // Still unused, try preview options
         if (!parms_used)
            parms_used = raspipreview_parse_cmdline(&state->preview_parameters, &argv[i][1], second_arg);

         // If no parms were used, this must be a bad parameters
         if (!parms_used)
            valid = 0;
         else
            i += parms_used - 1;

         break;
      }
      }
   }

   if (!valid)
   {
      fprintf(stderr, "Invalid command line option (%s)\n", argv[i-1]);
      return 1;
   }

   return 0;
}

/**
 *  buffer header callback function for camera output port
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   int complete = 0;
   // We pass our file handle and other stuff in via the userdata field.

   PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

   if (pData)
   {
      int bytes_written = 0;
      int bytes_to_write = buffer->length;

      if (pData->pstate->onlyLuma)
         bytes_to_write = vcos_min(buffer->length, port->format->es->video.width * port->format->es->video.height);

      if (bytes_to_write && pData->file_handle)
      {
         mmal_buffer_header_mem_lock(buffer);

         bytes_written = fwrite(buffer->data, 1, bytes_to_write, pData->file_handle);

         mmal_buffer_header_mem_unlock(buffer);
      }

      // We need to check we wrote what we wanted - it's possible we have run out of storage.
      if (buffer->length && bytes_written != bytes_to_write)
      {
         vcos_log_error("Unable to write buffer to file - aborting %d vs %d", bytes_written, bytes_to_write);
         complete = 1;
      }

      // Check end of frame or error
      if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
         complete = 1;
   }
   else
   {
      vcos_log_error("Received a camera still buffer callback with no state");
   }

   // release buffer back to the pool
   mmal_buffer_header_release(buffer);

   // and send one back to the port (if still open)
   if (port->is_enabled)
   {
      MMAL_STATUS_T status;
      MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(pData->pstate->camera_pool->queue);

      // and back to the port from there.
      if (new_buffer)
      {
         status = mmal_port_send_buffer(port, new_buffer);
      }

      if (!new_buffer || status != MMAL_SUCCESS)
         vcos_log_error("Unable to return the buffer to the camera still port");
   }

   if (complete)
   {
      vcos_semaphore_post(&(pData->complete_semaphore));
   }
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return 0 if failed, pointer to component if successful
 *
 */
static MMAL_STATUS_T create_camera_component(RASPISTILLYUV_STATE *state)
{
   MMAL_COMPONENT_T *camera = 0;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;

   /* Create the component */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to create camera component");
      goto error;
   }

   MMAL_PARAMETER_INT32_T camera_num =
   {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};

   status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not select camera : error %d", status);
      goto error;
   }

   if (!camera->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("Camera doesn't have output ports");
      goto error;
   }

   status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings.sensor_mode);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set sensor mode : error %d", status);
      goto error;
   }

   preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
   video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
   still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

   // Enable the camera, and tell it its control callback function
   status = mmal_port_enable(camera->control, default_camera_control_callback);

   if (status != MMAL_SUCCESS )
   {
      vcos_log_error("Unable to enable control port : error %d", status);
      goto error;
   }

   //  set up the camera configuration
   {
      MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
      {
         { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
         .max_stills_w = state->common_settings.width,
         .max_stills_h = state->common_settings.height,
         .stills_yuv422 = 0,
         .one_shot_stills = 1,
         .max_preview_video_w = state->preview_parameters.previewWindow.width,
         .max_preview_video_h = state->preview_parameters.previewWindow.height,
         .num_preview_video_frames = 3,
         .stills_capture_circular_buffer_height = 0,
         .fast_preview_resume = 0,
         .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
      };

      if (state->fullResPreview)
      {
         cam_config.max_preview_video_w = state->common_settings.width;
         cam_config.max_preview_video_h = state->common_settings.height;
      }

      mmal_port_parameter_set(camera->control, &cam_config.hdr);
   }

   raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

   // Now set up the port formats

   format = preview_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 50, 1000 }, {166, 1000}
      };
      mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }
   else if(state->camera_parameters.shutter_speed > 1000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 166, 1000 }, {999, 1000}
      };
      mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }
   if (state->fullResPreview)
   {
      // In this mode we are forcing the preview to be generated from the full capture resolution.
      // This runs at a max of 15fps with the OV5647 sensor.
      format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
      format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
      format->es->video.crop.x = 0;
      format->es->video.crop.y = 0;
      format->es->video.crop.width = state->common_settings.width;
      format->es->video.crop.height = state->common_settings.height;
      format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;
      format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;
   }
   else
   {
      // Use a full FOV 4:3 mode
      format->es->video.width = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.width, 32);
      format->es->video.height = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.height, 16);
      format->es->video.crop.x = 0;
      format->es->video.crop.y = 0;
      format->es->video.crop.width = state->preview_parameters.previewWindow.width;
      format->es->video.crop.height = state->preview_parameters.previewWindow.height;
      format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
      format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
   }

   status = mmal_port_format_commit(preview_port);

   if (status != MMAL_SUCCESS )
   {
      vcos_log_error("camera viewfinder format couldn't be set");
      goto error;
   }

   // Set the same format on the video  port (which we don't use here)
   mmal_format_full_copy(video_port->format, format);
   status = mmal_port_format_commit(video_port);

   if (status != MMAL_SUCCESS )
   {
      vcos_log_error("camera video format couldn't be set");
      goto error;
   }

   // Ensure there are enough buffers to avoid dropping frames
   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   format = still_port->format;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 50, 1000 }, {166, 1000}
      };
      mmal_port_parameter_set(still_port, &fps_range.hdr);
   }
   else if(state->camera_parameters.shutter_speed > 1000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 167, 1000 }, {999, 1000}
      };
      mmal_port_parameter_set(still_port, &fps_range.hdr);
   }
   // Set our stills format on the stills  port
   if (state->encoding)
   {
      format->encoding = state->encoding;
      if (!mmal_util_rgb_order_fixed(still_port))
      {
         if (format->encoding == MMAL_ENCODING_RGB24)
            format->encoding = MMAL_ENCODING_BGR24;
         else if (format->encoding == MMAL_ENCODING_BGR24)
            format->encoding = MMAL_ENCODING_RGB24;
      }
      format->encoding_variant = 0;  //Irrelevant when not in opaque mode
   }
   else
   {
      format->encoding = MMAL_ENCODING_I420;
      format->encoding_variant = MMAL_ENCODING_I420;
   }
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings.width;
   format->es->video.crop.height = state->common_settings.height;
   format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
   format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

   if (still_port->buffer_size < still_port->buffer_size_min)
      still_port->buffer_size = still_port->buffer_size_min;

   still_port->buffer_num = still_port->buffer_num_recommended;

   status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to select zero copy");
      goto error;
   }

   status = mmal_port_format_commit(still_port);

   if (status != MMAL_SUCCESS )
   {
      vcos_log_error("camera still format couldn't be set");
      goto error;
   }

   /* Enable component */
   status = mmal_component_enable(camera);

   if (status != MMAL_SUCCESS )
   {
      vcos_log_error("camera component couldn't be enabled");
      goto error;
   }

   /* Create pool of buffer headers for the output port to consume */
   pool = mmal_port_pool_create(still_port, still_port->buffer_num, still_port->buffer_size);

   if (!pool)
   {
      vcos_log_error("Failed to create buffer header pool for camera still port %s", still_port->name);
   }

   state->camera_pool = pool;
   state->camera_component = camera;

   if (state->common_settings.verbose)
      fprintf(stderr, "Camera component done\n");

   return status;

error:

   if (camera)
      mmal_component_destroy(camera);

   return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPISTILLYUV_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
}

/**
 * Allocates and generates a filename based on the
 * user-supplied pattern and the frame number.
 * On successful return, finalName and tempName point to malloc()ed strings
 * which must be freed externally.  (On failure, returns nulls that
 * don't need free()ing.)
 *
 * @param finalName pointer receives an
 * @param pattern sprintf pattern with %d to be replaced by frame
 * @param frame for timelapse, the frame number
 * @return Returns a MMAL_STATUS_T giving result of operation
*/

MMAL_STATUS_T create_filenames(char** finalName, char** tempName, char * pattern, int frame)
{
   *finalName = NULL;
   *tempName = NULL;
   if (0 > asprintf(finalName, pattern, frame) ||
         0 > asprintf(tempName, "%s~", *finalName))
   {
      if (*finalName != NULL)
      {
         free(*finalName);
      }
      return MMAL_ENOMEM;    // It may be some other error, but it is not worth getting it right
   }
   return MMAL_SUCCESS;
}

/**
 * Function to wait in various ways (depending on settings) for the next frame
 *
 * @param state Pointer to the state data
 * @param [in][out] frame The last frame number, adjusted to next frame number on output
 * @return !0 if to continue, 0 if reached end of run
 */
static int wait_for_next_frame(RASPISTILLYUV_STATE *state, int *frame)
{
   static int64_t complete_time = -1;
   int keep_running = 1;

   int64_t current_time =  get_microseconds64()/1000;

   if (complete_time == -1)
      complete_time =  current_time + state->timeout;

   // if we have run out of time, flag we need to exit
   // If timeout = 0 then always continue
   if (current_time >= complete_time && state->timeout != 0)
      keep_running = 0;

   switch (state->frameNextMethod)
   {
   case FRAME_NEXT_SINGLE :
      // simple timeout for a single capture
      vcos_sleep(state->timeout);
      return 0;

   case FRAME_NEXT_FOREVER :
   {
      *frame+=1;

      // Have a sleep so we don't hog the CPU.
      vcos_sleep(10000);

      // Run forever so never indicate end of loop
      return 1;
   }

   case FRAME_NEXT_TIMELAPSE :
   {
      static int64_t next_frame_ms = -1;

      // Always need to increment by at least one, may add a skip later
      *frame += 1;

      if (next_frame_ms == -1)
      {
         vcos_sleep(CAMERA_SETTLE_TIME);

         // Update our current time after the sleep
         current_time =  get_microseconds64()/1000;

         // Set our initial 'next frame time'
         next_frame_ms = current_time + state->timelapse;
      }
      else
      {
         int64_t this_delay_ms = next_frame_ms - current_time;

         if (this_delay_ms < 0)
         {
            // We are already past the next exposure time
            if (-this_delay_ms < -state->timelapse/2)
            {
               // Less than a half frame late, take a frame and hope to catch up next time
               next_frame_ms += state->timelapse;
               vcos_log_error("Frame %d is %d ms late", *frame, (int)(-this_delay_ms));
            }
            else
            {
               int nskip = 1 + (-this_delay_ms)/state->timelapse;
               vcos_log_error("Skipping frame %d to restart at frame %d", *frame, *frame+nskip);
               *frame += nskip;
               this_delay_ms += nskip * state->timelapse;
               vcos_sleep(this_delay_ms);
               next_frame_ms += (nskip + 1) * state->timelapse;
            }
         }
         else
         {
            vcos_sleep(this_delay_ms);
            next_frame_ms += state->timelapse;
         }
      }

      return keep_running;
   }

   case FRAME_NEXT_KEYPRESS :
   {
      int ch;

      if (state->common_settings.verbose)
         fprintf(stderr, "Press Enter to capture, X then ENTER to exit\n");

      ch = getchar();
      *frame+=1;
      if (ch == 'x' || ch == 'X')
         return 0;
      else
      {
         return keep_running;
      }
   }

   case FRAME_NEXT_IMMEDIATELY :
   {
      // Not waiting, just go to next frame.
      // Actually, we do need a slight delay here otherwise exposure goes
      // badly wrong since we never allow it frames to work it out
      // This could probably be tuned down.
      // First frame has a much longer delay to ensure we get exposure to a steady state
      if (*frame == 0)
         vcos_sleep(CAMERA_SETTLE_TIME);
      else
         vcos_sleep(30);

      *frame+=1;

      return keep_running;
   }

   case FRAME_NEXT_GPIO :
   {
      // Intended for GPIO firing of a capture
      return 0;
   }

   case FRAME_NEXT_SIGNAL :
   {
      // Need to wait for a SIGUSR1 signal
      sigset_t waitset;
      int sig;
      int result = 0;

      sigemptyset( &waitset );
      sigaddset( &waitset, SIGUSR1 );

      // We are multi threaded because we use mmal, so need to use the pthread
      // variant of procmask to block SIGUSR1 so we can wait on it.
      pthread_sigmask( SIG_BLOCK, &waitset, NULL );

      if (state->common_settings.verbose)
      {
         fprintf(stderr, "Waiting for SIGUSR1 to initiate capture\n");
      }

      result = sigwait( &waitset, &sig );

      if (state->common_settings.verbose)
      {
         if( result == 0)
         {
            fprintf(stderr, "Received SIGUSR1\n");
         }
         else
         {
            fprintf(stderr, "Bad signal received - error %d\n", errno);
         }
      }

      *frame+=1;

      return keep_running;
   }
   } // end of switch

   // Should have returned by now, but default to timeout
   return keep_running;
}

static void rename_file(RASPISTILLYUV_STATE *state, FILE *output_file,
                        const char *final_filename, const char *use_filename, int frame)
{
   MMAL_STATUS_T status;

   fclose(output_file);
   vcos_assert(use_filename != NULL && final_filename != NULL);
   if (0 != rename(use_filename, final_filename))
   {
      vcos_log_error("Could not rename temp file to: %s; %s",
                     final_filename,strerror(errno));
   }
   if (state->linkname)
   {
      char *use_link;
      char *final_link;
      status = create_filenames(&final_link, &use_link, state->linkname, frame);

      // Create hard link if possible, symlink otherwise
      if (status != MMAL_SUCCESS
            || (0 != link(final_filename, use_link)
                &&  0 != symlink(final_filename, use_link))
            || 0 != rename(use_link, final_link))
      {
         vcos_log_error("Could not link as filename: %s; %s",
                        state->linkname,strerror(errno));
      }
      if (use_link) free(use_link);
      if (final_link) free(final_link);
   }
}

/**
 * main
 */
int main(int argc, const char **argv)
{
   // Our main data storage vessel..
   RASPISTILLYUV_STATE state;
   int exit_code = EX_OK;

   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_T *camera_preview_port = NULL;
   MMAL_PORT_T *camera_video_port = NULL;
   MMAL_PORT_T *camera_still_port = NULL;
   MMAL_PORT_T *preview_input_port = NULL;
   FILE *output_file = NULL;

   bcm_host_init();

   // Register our application with the logging system
   vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);

   signal(SIGINT, default_signal_handler);

   // Disable USR1 for the moment - may be reenabled if go in to signal capture mode
   signal(SIGUSR1, SIG_IGN);

   set_app_name(argv[0]);

   // Do we have any parameters
   if (argc == 1)
   {
      display_valid_parameters(basename(argv[0]), &application_help_message);
      exit(EX_USAGE);
   }

   default_status(&state);

   // Parse the command line and put options in to our status structure
   if (parse_cmdline(argc, argv, &state))
   {
      status = -1;
      exit(EX_USAGE);
   }

   if (state.timeout == -1)
      state.timeout = 5000;

   if (state.common_settings.gps)
      if (raspi_gps_setup(state.common_settings.verbose))
         state.common_settings.gps = false;

   // Setup for sensor specific parameters
   get_sensor_defaults(state.common_settings.cameraNum, state.common_settings.camera_name,
                       &state.common_settings.width, &state.common_settings.height);

   if (state.common_settings.verbose)
   {
      print_app_details(stderr);
      dump_status(&state);
   }

   // OK, we have a nice set of parameters. Now set up our components
   // We have two components. Camera and Preview
   // Camera is different in stills/video, but preview
   // is the same so handed off to a separate module

   if ((status = create_camera_component(&state)) != MMAL_SUCCESS)
   {
      vcos_log_error("%s: Failed to create camera component", __func__);
      exit_code = EX_SOFTWARE;
   }
   else if ((status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS)
   {
      vcos_log_error("%s: Failed to create preview component", __func__);
      destroy_camera_component(&state);
      exit_code = EX_SOFTWARE;
   }
   else
   {
      PORT_USERDATA callback_data;

      if (state.common_settings.verbose)
         fprintf(stderr, "Starting component connection stage\n");

      camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
      camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
      camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];

      // Note we are lucky that the preview and null sink components use the same input port
      // so we can simple do this without conditionals
      preview_input_port  = state.preview_parameters.preview_component->input[0];

      // Connect camera to preview (which might be a null_sink if no preview required)
      status = connect_ports(camera_preview_port, preview_input_port, &state.preview_connection);

      if (status == MMAL_SUCCESS)
      {
         VCOS_STATUS_T vcos_status;

         // Set up our userdata - this is passed though to the callback where we need the information.
         // Null until we open our filename
         callback_data.file_handle = NULL;
         callback_data.pstate = &state;

         vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);
         vcos_assert(vcos_status == VCOS_SUCCESS);

         camera_still_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;

         if (state.common_settings.verbose)
            fprintf(stderr, "Enabling camera still output port\n");

         // Enable the camera still output port and tell it its callback function
         status = mmal_port_enable(camera_still_port, camera_buffer_callback);

         if (status != MMAL_SUCCESS)
         {
            vcos_log_error("Failed to setup camera output");
            goto error;
         }

         if (state.common_settings.verbose)
            fprintf(stderr, "Starting video preview\n");

         int frame, keep_looping = 1;
         FILE *output_file = NULL;
         char *use_filename = NULL;      // Temporary filename while image being written
         char *final_filename = NULL;    // Name that file gets once writing complete

         frame = 0;

         while (keep_looping)
         {
            keep_looping = wait_for_next_frame(&state, &frame);

            // Open the file
            if (state.common_settings.filename)
            {
               if (state.common_settings.filename[0] == '-')
               {
                  output_file = stdout;

                  // Ensure we don't upset the output stream with diagnostics/info
                  state.common_settings.verbose = 0;
               }
               else
               {
                  vcos_assert(use_filename == NULL && final_filename == NULL);
                  status = create_filenames(&final_filename, &use_filename, state.common_settings.filename, frame);
                  if (status  != MMAL_SUCCESS)
                  {
                     vcos_log_error("Unable to create filenames");
                     goto error;
                  }

                  if (state.common_settings.verbose)
                     fprintf(stderr, "Opening output file %s\n", final_filename);
                  // Technically it is opening the temp~ filename which will be ranamed to the final filename

                  output_file = fopen(use_filename, "wb");

                  if (!output_file)
                  {
                     // Notify user, carry on but discarding encoded output buffers
                     vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, use_filename);
                  }
               }

               callback_data.file_handle = output_file;
            }

            if (output_file)
            {
               int num, q;

               // There is a possibility that shutter needs to be set each loop.
               if (mmal_status_to_int(mmal_port_parameter_set_uint32(state.camera_component->control, MMAL_PARAMETER_SHUTTER_SPEED, state.camera_parameters.shutter_speed) != MMAL_SUCCESS))
                  vcos_log_error("Unable to set shutter speed");

               // Send all the buffers to the camera output port
               num = mmal_queue_length(state.camera_pool->queue);

               for (q=0; q<num; q++)
               {
                  MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.camera_pool->queue);

                  if (!buffer)
                     vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                  if (mmal_port_send_buffer(camera_still_port, buffer)!= MMAL_SUCCESS)
                     vcos_log_error("Unable to send a buffer to camera output port (%d)", q);
               }

               if (state.burstCaptureMode && frame==1)
               {
                  mmal_port_parameter_set_boolean(state.camera_component->control,  MMAL_PARAMETER_CAMERA_BURST_CAPTURE, 1);
               }

               if(state.camera_parameters.enable_annotate)
               {
                  if ((state.camera_parameters.enable_annotate & ANNOTATE_APP_TEXT) && state.common_settings.gps)
                  {
                     char *text = raspi_gps_location_string();
                     raspicamcontrol_set_annotate(state.camera_component, state.camera_parameters.enable_annotate,
                                                  text,
                                                  state.camera_parameters.annotate_text_size,
                                                  state.camera_parameters.annotate_text_colour,
                                                  state.camera_parameters.annotate_bg_colour,
                                                  state.camera_parameters.annotate_justify,
                                                  state.camera_parameters.annotate_x,
                                                  state.camera_parameters.annotate_y
                                                 );
                     free(text);
                  }
                  else
                     raspicamcontrol_set_annotate(state.camera_component, state.camera_parameters.enable_annotate,
                                                  state.camera_parameters.annotate_string,
                                                  state.camera_parameters.annotate_text_size,
                                                  state.camera_parameters.annotate_text_colour,
                                                  state.camera_parameters.annotate_bg_colour,
                                                  state.camera_parameters.annotate_justify,
                                                  state.camera_parameters.annotate_x,
                                                  state.camera_parameters.annotate_y
                                                 );
               }

               if (state.common_settings.verbose)
                  fprintf(stderr, "Starting capture %d\n", frame);

               if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
               {
                  vcos_log_error("%s: Failed to start capture", __func__);
               }
               else
               {
                  // Wait for capture to complete
                  // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                  // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                  vcos_semaphore_wait(&callback_data.complete_semaphore);
                  if (state.common_settings.verbose)
                     fprintf(stderr, "Finished capture %d\n", frame);
               }

               // Ensure we don't die if get callback with no open file
               callback_data.file_handle = NULL;

               if (output_file != stdout)
               {
                  rename_file(&state, output_file, final_filename, use_filename, frame);
               }
               else
               {
                  fflush(output_file);
               }
            }

            if (use_filename)
            {
               free(use_filename);
               use_filename = NULL;
            }
            if (final_filename)
            {
               free(final_filename);
               final_filename = NULL;
            }
         } // end for (frame)

         vcos_semaphore_delete(&callback_data.complete_semaphore);
      }
      else
      {
         mmal_status_to_int(status);
         vcos_log_error("%s: Failed to connect camera to preview", __func__);
      }

error:

      mmal_status_to_int(status);

      if (state.common_settings.verbose)
         fprintf(stderr, "Closing down\n");

      if (output_file)
         fclose(output_file);

      // Disable all our ports that are not handled by connections
      check_disable_port(camera_video_port);

      if (state.preview_connection)
         mmal_connection_destroy(state.preview_connection);

      /* Disable components */
      if (state.preview_parameters.preview_component)
         mmal_component_disable(state.preview_parameters.preview_component);

      if (state.camera_component)
         mmal_component_disable(state.camera_component);

      raspipreview_destroy(&state.preview_parameters);
      destroy_camera_component(&state);

      if (state.common_settings.gps)
         raspi_gps_shutdown(state.common_settings.verbose);

      if (state.common_settings.verbose)
         fprintf(stderr, "Close down completed, all components disconnected, disabled and destroyed\n\n");
   }

   if (status != MMAL_SUCCESS)
      raspicamcontrol_check_configuration(128);

   return exit_code;
}

