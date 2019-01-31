/*
Copyright (c) 2012 Broadcom Europe Ltd
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
 * This file contains all the properties one can set in HDMI
 * Each property has up to two parameters corresponding to list of enums one can set via
 * the set/get property function from TV service (HOST) or HDMI middleware API (Videocore)
 * Each property also has a default value for these parameters. Please refer to 
 * individual properties for details.
 */

#ifndef _VC_HDMI_PROPERTY_H_
#define _VC_HDMI_PROPERTY_H_
#include "vcinclude/common.h"

/**
 * Property type
 */
typedef enum {
   HDMI_PROPERTY_PIXEL_ENCODING   = 0, /**< Set pixel encoding, value of property is HDMI_PIXEL_ENCODING_T, default is RGB full range (unless it is not supported) */
   HDMI_PROPERTY_PIXEL_CLOCK_TYPE = 1, /**< See HDMI_PIXEL_CLOCK_TYPE_T, default is HDMI_PIXEL_CLOCK_TYPE_PAL */
   HDMI_PROPERTY_CONTENT_TYPE     = 2, /**< Set content type flag EDID_ContentType_T */
   HDMI_PROPERTY_FUZZY_MATCH      = 3, /**< disable (0 - default) or enable (1) fuzzy format match */
   HDMI_PROPERTY_3D_STRUCTURE     = 4, /**< Set the 3D structure of the next HDMI power on (only for CEA mode) */

   //More properties to be added here
   HDMI_PROPERTY_MAX                   /**< place holder */
} HDMI_PROPERTY_T;

/**
 * Setting/getting a property
 */
typedef struct {
   HDMI_PROPERTY_T property;
   uint32_t param1;
   uint32_t param2;
   //More fields may be added here if required in future
} HDMI_PROPERTY_PARAM_T;

/**
 * PIXEL_ENCODING: param1: encoding type, param2: n/a
 * Setting encoding type is subjected to support in EDID
 */
typedef enum {
   HDMI_PIXEL_ENCODING_DEFAULT = 0,  /**<Default, note that CEA modes (except VGA) default to RGB limited but DMT modes default to RGB full */
   HDMI_PIXEL_ENCODING_RGB_LIMITED,
   HDMI_PIXEL_ENCODING_RGB_FULL,
   HDMI_PIXEL_ENCODING_YCbCr444_LIMITED,
   HDMI_PIXEL_ENCODING_YCbCr444_FULL,
   /** YCbCr 422 are not used at the moment */
   HDMI_PIXEL_ENCODING_YCbCr422_LIMITED,
   HDMI_PIXEL_ENCODING_YCbCr422_FULL,
   HDMI_PIXEL_ENCODING_MAX /**<place holder */
} HDMI_PIXEL_ENCODING_T;

/**
 * PIXEL_CLOCK_TYPE: param1: pixel clock type, param2: n/a
 * Pixel clock nudge factor (set pixel clock type)
 */
typedef enum {
   HDMI_PIXEL_CLOCK_TYPE_PAL  = 0, /**< Use norminal pixel clock (default) */
   HDMI_PIXEL_CLOCK_TYPE_NTSC = 1, /**< Multiply norminal pixel clock by 1000/1001 to get the alternative frame rate e.g. 59.94Hz rather than 60, not applicable to all formats */
   HDMI_PIXEL_CLOCK_TYPE_MAX       /**< place holder */
} HDMI_PIXEL_CLOCK_TYPE_T;

/**
 * Content type: param1: EDID content type, param2: n/a
 */
/**
 * Content type: the enum is the actual value in AVI infoframe + 1
 * because NODATA and Graphics both have value zero
 */
typedef enum {
   EDID_ContentType_NODATA   = 0x0, /**<Content type none */
   EDID_ContentType_Graphics = 0x1, /**<Graphics, ITC must be set to 1 */
   EDID_ContentType_Photo    = 0x2, /**<Photo */
   EDID_ContentType_Cinema   = 0x3, /**<Cinema */
   EDID_ContentType_Game     = 0x4, /**<Game */
   EDID_ContentType_MAX             /**<place holder */
} EDID_ContentType_T;

/**
 * Fuzzy match: param1 zero (disabled) or non-zero (enabled), param2: n/a
 * If enabled, match format in EDID based on resolutions and formats only
 * Default is zero (disabled, so match on blanking periods and pixel clock)
 * Set to non-zero to enable fuzzy match
 */
typedef enum {
   EDID_FUZZY_MATCH_DISABLED = 0x0,
   EDID_FUZZY_MATCH_ENABLED  = 0x1,
   EDID_FUZZY_MATCH_MAX
} EDID_FuzzyMatch_T;

/**
 * 3D structure: param1 - 3D structure (e.g. top/bottom side by side) (default value is none, i.e. 2D)
 *               param2 - n/a at the moment, may be used in the future
 *
 * 3D structure is auto reset to "2D" every time HDMI is power on. Only affect CEA formats.
 */
/*
 * Matched to the 3d struct bit fields stored internally to represent 3D support in EDID
 */
typedef enum {
   HDMI_3D_FORMAT_NONE = 0,           /**<plain and simple 2D! */
   HDMI_3D_FORMAT_SBS_HALF = (1<<7),  /**<side by side half horizontal */
   HDMI_3D_FORMAT_TB_HALF  = (1<<6),  /**<top and bottom half vertical */
   HDMI_3D_FORMAT_FRAME_PACKING = (1<<8),  /**<frame packed */
   HDMI_3D_FORMAT_FRAME_SEQUENTIAL = (1<<9),  /**<Output left on even frames and right on odd frames (typically 120Hz)*/
   /* More 3D structs, e.g. full frame packing, may be added here */
   HDMI_3D_FORMAT_INVALID = 0xFFFF
} HDMI_3D_FORMAT_T;

#endif //_VC_HDMI_PROPERTY_H_
