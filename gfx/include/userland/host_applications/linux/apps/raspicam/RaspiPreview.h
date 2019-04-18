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

#ifndef RASPIPREVIEW_H_
#define RASPIPREVIEW_H_

/// Layer that preview window should be displayed on
#define PREVIEW_LAYER      2

// Frames rates of 0 implies variable, but denominator needs to be 1 to prevent div by 0
#define PREVIEW_FRAME_RATE_NUM 0
#define PREVIEW_FRAME_RATE_DEN 1

#define FULL_RES_PREVIEW_FRAME_RATE_NUM 0
#define FULL_RES_PREVIEW_FRAME_RATE_DEN 1

#define FULL_FOV_PREVIEW_16x9_X 1280
#define FULL_FOV_PREVIEW_16x9_Y 720

#define FULL_FOV_PREVIEW_4x3_X 1296
#define FULL_FOV_PREVIEW_4x3_Y 972

#define FULL_FOV_PREVIEW_FRAME_RATE_NUM 0
#define FULL_FOV_PREVIEW_FRAME_RATE_DEN 1

typedef struct
{
   int wantPreview;                       /// Display a preview
   int wantFullScreenPreview;             /// 0 is use previewRect, non-zero to use full screen
   int opacity;                           /// Opacity of window - 0 = transparent, 255 = opaque
   MMAL_RECT_T previewWindow;             /// Destination rectangle for the preview window.
   MMAL_COMPONENT_T *preview_component;   /// Pointer to the created preview display component
} RASPIPREVIEW_PARAMETERS;

MMAL_STATUS_T raspipreview_create(RASPIPREVIEW_PARAMETERS *state);
void raspipreview_destroy(RASPIPREVIEW_PARAMETERS *state);
void raspipreview_set_defaults(RASPIPREVIEW_PARAMETERS *state);
void raspipreview_dump_parameters(RASPIPREVIEW_PARAMETERS *state);
int raspipreview_parse_cmdline(RASPIPREVIEW_PARAMETERS *params, const char *arg1, const char *arg2);
void raspipreview_display_help();

#endif /* RASPIPREVIEW_H_ */
