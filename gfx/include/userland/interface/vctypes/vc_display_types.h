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

/*=============================================================================
Common image types used by the vc_image library.
=============================================================================*/

#ifndef INTERFACE_VC_DISPLAY_TYPES_H
#define INTERFACE_VC_DISPLAY_TYPES_H

//enums of display input format
typedef enum
{
   VCOS_DISPLAY_INPUT_FORMAT_INVALID = 0,
   VCOS_DISPLAY_INPUT_FORMAT_RGB888,
   VCOS_DISPLAY_INPUT_FORMAT_RGB565
}
VCOS_DISPLAY_INPUT_FORMAT_T;

/** For backward compatibility */
#define DISPLAY_INPUT_FORMAT_INVALID VCOS_DISPLAY_INPUT_FORMAT_INVALID
#define DISPLAY_INPUT_FORMAT_RGB888  VCOS_DISPLAY_INPUT_FORMAT_RGB888
#define DISPLAY_INPUT_FORMAT_RGB565  VCOS_DISPLAY_INPUT_FORMAT_RGB565
typedef VCOS_DISPLAY_INPUT_FORMAT_T DISPLAY_INPUT_FORMAT_T;

// Enum determining how image data for 3D displays has to be supplied
typedef enum
{
   DISPLAY_3D_UNSUPPORTED = 0,   // default
   DISPLAY_3D_INTERLEAVED,       // For autosteroscopic displays
   DISPLAY_3D_SBS_FULL_AUTO,     // Side-By-Side, Full Width (also used by some autostereoscopic displays)
   DISPLAY_3D_SBS_HALF_HORIZ,    // Side-By-Side, Half Width, Horizontal Subsampling (see HDMI spec)
   DISPLAY_3D_TB_HALF,           // Top-bottom 3D
   DISPLAY_3D_FRAME_PACKING,     // Frame Packed 3D
   DISPLAY_3D_FRAME_SEQUENTIAL,  // Output left on even frames and right on odd frames (typically 120Hz)
   DISPLAY_3D_FORMAT_MAX
} DISPLAY_3D_FORMAT_T;

//enums of display types
typedef enum
{
   DISPLAY_INTERFACE_MIN,
   DISPLAY_INTERFACE_SMI,
   DISPLAY_INTERFACE_DPI,
   DISPLAY_INTERFACE_DSI,
   DISPLAY_INTERFACE_LVDS,
   DISPLAY_INTERFACE_MAX

} DISPLAY_INTERFACE_T;

/* display dither setting, used on B0 */
typedef enum {
   DISPLAY_DITHER_NONE   = 0,   /* default if not set */
   DISPLAY_DITHER_RGB666 = 1,
   DISPLAY_DITHER_RGB565 = 2,
   DISPLAY_DITHER_RGB555 = 3,
   DISPLAY_DITHER_MAX
} DISPLAY_DITHER_T;

//info struct
typedef struct
{
   //type
   DISPLAY_INTERFACE_T type;
   //width / height
   uint32_t width;
   uint32_t height;
   //output format
   DISPLAY_INPUT_FORMAT_T input_format;
   //interlaced?
   uint32_t interlaced;
   /* output dither setting (if required) */
   DISPLAY_DITHER_T output_dither;
   /* Pixel frequency */
   uint32_t pixel_freq;
   /* Line rate in lines per second */
   uint32_t line_rate;
   // Format required for image data for 3D displays
   DISPLAY_3D_FORMAT_T format_3d;
   // If display requires PV1 (e.g. DSI1), special config is required in HVS
   uint32_t use_pixelvalve_1;
   // Set for DSI displays which use video mode.
   uint32_t dsi_video_mode;
   // Select HVS channel (usually 0).
   uint32_t hvs_channel;
   // transform required to get the display correctly oriented landscape
   uint32_t transform;
} DISPLAY_INFO_T;

#endif /* __VC_INCLUDE_IMAGE_TYPES_H__ */


