/*
Copyright (c) 2018, Raspberry Pi (Trading) Ltd.
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
 * \file RaspiCommonSettings.c
 *
 * Description
 *
 * Handles general settings applicable to all the camera applications
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>

#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include "RaspiCommonSettings.h"
#include "RaspiCLI.h"
#include "RaspiHelpers.h"
#include "RaspiGPS.h"

enum
{
   CommandHelp,
   CommandWidth,
   CommandHeight,
   CommandOutput,
   CommandVerbose,
   CommandCamSelect,
   CommandSensorMode,
   CommandGpsd,
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandHelp,    "-help",       "?",  "This help information", 0 },
   { CommandWidth,   "-width",      "w",  "Set image width <size>", 1 },
   { CommandHeight,  "-height",     "h",  "Set image height <size>", 1 },
   { CommandOutput,  "-output",     "o",  "Output filename <filename> (to write to stdout, use '-o -'). If not specified, no file is saved", 1 },
   { CommandVerbose, "-verbose",    "v",  "Output verbose information during run", 0 },
   { CommandCamSelect, "-camselect","cs", "Select camera <number>. Default 0", 1 },
   { CommandSensorMode,"-mode",     "md", "Force sensor mode. 0=auto. See docs for other modes available", 1},
   { CommandGpsd,    "-gpsdexif",   "gps","Apply real-time GPS information to output (e.g. EXIF in JPG, annotation in video (requires " LIBGPS_SO_VERSION ")", 0},
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

void raspicommonsettings_set_defaults(RASPICOMMONSETTINGS_PARAMETERS *state)
{
   strncpy(state->camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
   // We dont set width and height since these will be specific to the app being built.
   state->width = 0;
   state->height = 0;
   state->filename = NULL;
   state->verbose = 0;
   state->cameraNum = 0;
   state->sensor_mode = 0;
   state->gps = 0;
};

/**
 * Dump parameters as human readable to stderr
 *
 * @param state Pointer to parameter block
 *
 */
void raspicommonsettings_dump_parameters(RASPICOMMONSETTINGS_PARAMETERS *state)
{
   fprintf(stderr, "Camera Name %s\n", state->camera_name);
   fprintf(stderr, "Width %d, Height %d, filename %s\n", state->width,
           state->height, state->filename);
   fprintf(stderr, "Using camera %d, sensor mode %d\n\n", state->cameraNum, state->sensor_mode);
   fprintf(stderr, "GPS output %s\n\n", state->gps ? "Enabled" : "Disabled");
};

/**
 * Display help for command line options for this module
 */
void raspicommonsettings_display_help()
{
   fprintf(stdout, "\nCommon Settings commands\n\n");
   raspicli_display_help(cmdline_commands, cmdline_commands_size);
}

/**
 * Parse a possible command pair - command and parameter
 * @param arg1 Command
 * @param arg2 Parameter (could be NULL)
 * @return How many parameters were used, 0,1,2
 */
int raspicommonsettings_parse_cmdline(RASPICOMMONSETTINGS_PARAMETERS *state, const char *arg1, const char *arg2, void (*app_help)(char*))
{
   int command_id, used = 0, num_parameters;

   if (!arg1)
      return 0;

   command_id = raspicli_get_command_id(cmdline_commands, cmdline_commands_size, arg1, &num_parameters);

   // If invalid command, or we are missing a parameter, drop out
   if (command_id==-1 || (command_id != -1 && num_parameters > 0 && arg2 == NULL))
      return 0;

   switch (command_id)
   {
   case CommandHelp:
   {
      display_valid_parameters(basename(get_app_name()), app_help);
      exit(0);
      break;
   }
   case CommandWidth: // Width > 0
      if (sscanf(arg2, "%u", &state->width) == 1)
         used = 2;
      break;

   case CommandHeight: // Height > 0
      if (sscanf(arg2, "%u", &state->height) == 1)
         used = 2;
      break;

   case CommandOutput:  // output filename
   {
      int len = strlen(arg2);
      if (len)
      {
         // Ensure that any %<char> is either %% or %d.
         const char *percent = arg2;

         while(*percent && (percent=strchr(percent, '%')) != NULL)
         {
            int digits=0;
            percent++;
            while(isdigit(*percent))
            {
               percent++;
               digits++;
            }
            if(!((*percent == '%' && !digits) || *percent == 'd'))
            {
               used = 0;
               fprintf(stderr, "Filename contains %% characters, but not %%d or %%%% - sorry, will fail\n");
               break;
            }
            percent++;
         }

         state->filename = malloc(len + 10); // leave enough space for any timelapse generated changes to filename
         vcos_assert(state->filename);
         if (state->filename)
            strncpy(state->filename, arg2, len+1);
         used = 2;
      }
      else
         used = 0;
      break;
   }

   case CommandVerbose: // display lots of data during run
      state->verbose = 1;
      used = 1;
      break;

   case CommandCamSelect:  //Select camera input port
   {
      if (sscanf(arg2, "%u", &state->cameraNum) == 1)
         used = 2;
      break;
   }

   case CommandSensorMode:
   {
      if (sscanf(arg2, "%u", &state->sensor_mode) == 1)
         used = 2;
      break;
   }

   case CommandGpsd:
      state->gps = 1;
      used = 1;
      break;
   }

   return used;
}
