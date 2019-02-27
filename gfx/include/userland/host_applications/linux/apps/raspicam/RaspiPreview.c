/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiPreview.h"
#include "RaspiCLI.h"

enum
{
   CommandPreview,
   CommandFullScreen,
   CommandOpacity,
   CommandDisablePreview
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandPreview,       "-preview",    "p",  "Preview window settings <'x,y,w,h'>", 1 },
   { CommandFullScreen,    "-fullscreen", "f",  "Fullscreen preview mode", 0 },
   { CommandOpacity,       "-opacity",    "op", "Preview window opacity (0-255)", 1},
   { CommandDisablePreview,"-nopreview",  "n",  "Do not display a preview window", 0},
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

/**
 * Create the preview component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T raspipreview_create(RASPIPREVIEW_PARAMETERS *state)
{
   MMAL_COMPONENT_T *preview = 0;
   MMAL_PORT_T *preview_port = NULL;
   MMAL_STATUS_T status;

   if (!state->wantPreview)
   {
      // No preview required, so create a null sink component to take its place
      status = mmal_component_create("vc.null_sink", &preview);

      if (status != MMAL_SUCCESS)
      {
         vcos_log_error("Unable to create null sink component");
         goto error;
      }
   }
   else
   {
      status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                     &preview);

      if (status != MMAL_SUCCESS)
      {
         vcos_log_error("Unable to create preview component");
         goto error;
      }

      if (!preview->input_num)
      {
         status = MMAL_ENOSYS;
         vcos_log_error("No input ports found on component");
         goto error;
      }

      preview_port = preview->input[0];

      MMAL_DISPLAYREGION_T param;
      param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
      param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

      param.set = MMAL_DISPLAY_SET_LAYER;
      param.layer = PREVIEW_LAYER;

      param.set |= MMAL_DISPLAY_SET_ALPHA;
      param.alpha = state->opacity;

      if (state->wantFullScreenPreview)
      {
         param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
         param.fullscreen = 1;
      }
      else
      {
         param.set |= (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
         param.fullscreen = 0;
         param.dest_rect = state->previewWindow;
      }

      status = mmal_port_parameter_set(preview_port, &param.hdr);

      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         vcos_log_error("unable to set preview port parameters (%u)", status);
         goto error;
      }
   }

   /* Enable component */
   status = mmal_component_enable(preview);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable preview/null sink component (%u)", status);
      goto error;
   }

   state->preview_component = preview;

   return status;

error:

   if (preview)
      mmal_component_destroy(preview);

   return status;
}

/**
 * Destroy the preview component
 *
 * @param state Pointer to state control struct
 *
 */
void raspipreview_destroy(RASPIPREVIEW_PARAMETERS *state)
{
   if (state->preview_component)
   {
      mmal_component_destroy(state->preview_component);
      state->preview_component = NULL;
   }
}

/**
 * Assign set of default parameters to the passed in parameter block
 *
 * @param state Pointer to parameter block
 *
 */
void raspipreview_set_defaults(RASPIPREVIEW_PARAMETERS *state)
{
   state->wantPreview = 1;
   state->wantFullScreenPreview = 1;
   state->opacity = 255;
   state->previewWindow.x = 0;
   state->previewWindow.y = 0;
   state->previewWindow.width = 1024;
   state->previewWindow.height = 768;
   state->preview_component = NULL;
}

/**
 * Dump parameters as human readable to stdout
 *
 * @param state Pointer to parameter block
 *
 */
void raspipreview_dump_parameters(RASPIPREVIEW_PARAMETERS *state)
{
   fprintf(stderr, "Preview %s, Full screen %s\n", state->wantPreview ? "Yes" : "No",
           state->wantFullScreenPreview ? "Yes" : "No");

   fprintf(stderr, "Preview window %d,%d,%d,%d\nOpacity %d\n", state->previewWindow.x,
           state->previewWindow.y, state->previewWindow.width,
           state->previewWindow.height, state->opacity);
};

/**
 * Parse a possible command pair - command and parameter
 * @param arg1 Command
 * @param arg2 Parameter (could be NULL)
 * @return How many parameters were used, 0,1,2
 */
int raspipreview_parse_cmdline(RASPIPREVIEW_PARAMETERS *params, const char *arg1, const char *arg2)
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
   case CommandPreview: // Preview window
   {
      int tmp;

      params->wantPreview = 1;

      tmp = sscanf(arg2, "%d,%d,%d,%d",
                   &params->previewWindow.x, &params->previewWindow.y,
                   &params->previewWindow.width, &params->previewWindow.height);

      // Failed to get any window parameters, so revert to full screen
      if (tmp == 0)
         params->wantFullScreenPreview = 1;
      else
         params->wantFullScreenPreview = 0;

      used = 2;

      break;
   }

   case CommandFullScreen: // Want full screen preview mode (overrides display rect)
      params->wantPreview = 1;
      params->wantFullScreenPreview = 1;

      used = 1;
      break;

   case CommandOpacity: // Define preview window opacity
      if (sscanf(arg2, "%u", &params->opacity) != 1)
         params->opacity = 255;
      else
         used = 2;
      break;

   case CommandDisablePreview: // Turn off preview output
      params->wantPreview = 0;
      used = 1;
      break;
   }

   return used;
}

/**
 * Display help for command line options
 */
void raspipreview_display_help()
{
   fprintf(stdout, "\nPreview parameter commands\n\n");
   raspicli_display_help(cmdline_commands, cmdline_commands_size);
}
