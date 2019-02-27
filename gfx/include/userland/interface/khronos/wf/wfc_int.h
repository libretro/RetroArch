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

#ifndef WFC_INT_H
#define WFC_INT_H

#include "interface/khronos/include/WF/wfc.h"

//!@file wfc.h
//!@brief Standard OpenWF-C header file. Available from the
//! <a href="http://www.khronos.org/registry/wf/">Khronos OpenWF API Registry</a>.

//==============================================================================

//! Only valid value for attribute list (as of WF-C v1.0)
#define NO_ATTRIBUTES   NULL

//!@name
//! Values for rectangle types
//!@{
#define WFC_RECT_X      0
#define WFC_RECT_Y      1
#define WFC_RECT_WIDTH  2
#define WFC_RECT_HEIGHT 3

#define WFC_RECT_SIZE   4
//!@}

//!@name
//! Values for context background colour
//!@{
#define WFC_BG_CLR_RED     0
#define WFC_BG_CLR_GREEN   1
#define WFC_BG_CLR_BLUE    2
#define WFC_BG_CLR_ALPHA   3

#define WFC_BG_CLR_SIZE    4
//!@}

//==============================================================================

//! Attributes associated with each element
typedef struct
{
   WFCint         dest_rect[WFC_RECT_SIZE];//< The destination region of the element in the context
   WFCfloat       src_rect[WFC_RECT_SIZE];//< The region of the source to be mapped to the destination
   WFCboolean     flip;                   //< If WFC_TRUE, the source is flipped about a horizontal axis through its centre
   WFCRotation    rotation;               //< The rotation of the source, see WFCRotation
   WFCScaleFilter scale_filter;           //< The scaling filter, see WFCScaleFilter
   WFCbitfield    transparency_types;     //< A combination of transparency types, see WFCTransparencyType
   WFCfloat       global_alpha;           //< The global alpha for the element, if WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA is set
   WFCNativeStreamType source_stream;     //< The source stream handle or WFC_INVALID_HANDLE
   WFCNativeStreamType mask_stream;       //< The mask stream handle or WFC_INVALID_HANDLE
} WFC_ELEMENT_ATTRIB_T;

//! Default values for elements
#define WFC_ELEMENT_ATTRIB_DEFAULT \
{ \
   {0, 0, 0, 0}, \
   {0.0, 0.0, 0.0, 0.0}, \
   WFC_FALSE, \
   WFC_ROTATION_0, \
   WFC_SCALE_FILTER_NONE, \
   WFC_TRANSPARENCY_NONE, \
   1.0, \
   WFC_INVALID_HANDLE, \
   WFC_INVALID_HANDLE \
}

//! Attributes associated with a context which are fixed on creation
typedef struct
{
   WFCContextType type;                   //!< The context type, either on- or off-screen
   WFCint         width;                  //!< The width of the context
   WFCint         height;                 //!< The height of the context
} WFC_CONTEXT_STATIC_ATTRIB_T;

//! Default static values for contexts
#define WFC_CONTEXT_STATIC_ATTRIB_DEFAULT \
{ \
   WFC_CONTEXT_TYPE_ON_SCREEN, \
   0, 0, \
}

// Attributes associated with a context that may change
typedef struct
{
   WFCRotation    rotation;               //< The rotation to be applied to the whole context
   WFCfloat       background_clr[WFC_BG_CLR_SIZE]; //< The background colour of the context, in order RGBA, scaled from 0 to 1
} WFC_CONTEXT_DYNAMIC_ATTRIB_T;

//! Default dynamic values for contexts
#define WFC_CONTEXT_DYNAMIC_ATTRIB_DEFAULT \
{ \
   WFC_ROTATION_0, \
   {0.0, 0.0, 0.0, 1.0} \
}

//! Arbitrary maximum number of elements per scene
#define WFC_MAX_ELEMENTS_IN_SCENE 8

//! Arbitrary maximum number of WFC stream ids per client
#define WFC_MAX_STREAMS_PER_CLIENT     128

//! Data for a "scene" (i.e. context and element data associated with a commit).
typedef struct
{
    WFC_CONTEXT_DYNAMIC_ATTRIB_T context;    //!< Dynamic attributes for this scene's context
    uint32_t commit_count;                   //!< Count of the scenes committed for this context
    uint32_t element_count;                  //!< Number of elements to be committed
    WFC_ELEMENT_ATTRIB_T elements[WFC_MAX_ELEMENTS_IN_SCENE];  //!< Attributes of committed elements
} WFC_SCENE_T;

//definitions moved from wfc_server_stream.h so can be included on client

typedef enum
{
   WFC_IMAGE_NO_FLAGS         = 0,
   WFC_IMAGE_FLIP_VERT        = (1 << 0), //< Vertically flip image
   WFC_IMAGE_DISP_NOTHING     = (1 << 1), //< Display nothing on screen
   WFC_IMAGE_CB_ON_NO_CHANGE  = (1 << 2), //< Callback, even if the image is the same
   WFC_IMAGE_CB_ON_COMPOSE    = (1 << 3), //< Callback on composition, instead of when not in use
   WFC_IMAGE_PROTECTION_HDCP  = (1 << 4), //< HDCP required if output to HDMI display
   WFC_IMAGE_FLIP_HORZ        = (1 << 5), //< Horizontally flip image
   WFC_IMAGE_SENTINEL         = 0x7FFFFFFF
} WFC_IMAGE_FLAGS_T;

//! Define the type of generic WFC image
typedef enum
{
   WFC_STREAM_IMAGE_TYPE_OPAQUE = 0,    //< Handle to a multimedia opaque image
   WFC_STREAM_IMAGE_TYPE_RAW,           //< Handle to a raw pixel buffer in shared memory
   WFC_STREAM_IMAGE_TYPE_EGL,           //< Handle to an EGL storage

   WFC_STREAM_IMAGE_TYPE_SENTINEL = 0x7FFFFFFF
} WFC_STREAM_IMAGE_TYPE_T;

//! DRM protection attributes for an image. Currently tis just maps to
//! WFC_IMAGE_PROTECTION_HDCP and DISPMANX_PROTECTION_HDCP but in future
//! could contain other options e.g. to down-sample output.
typedef enum
{
   WFC_PROTECTION_NONE = 0,         //< Image is unprotected
   WFC_PROTECTION_HDCP = (1 << 0),  //< HDCP required if output to HDMI display

   WFC_PROTECTION_SENTINEL = 0x7FFFFFFF
} WFC_PROTECTION_FLAGS_T;

//! Horizontal and vertical flip combinations
typedef enum
{
   WFC_STREAM_IMAGE_FLIP_NONE = 0,     //< No flip
   WFC_STREAM_IMAGE_FLIP_VERT,         //< Vertical flip only
   WFC_STREAM_IMAGE_FLIP_HORZ,         //< Horizontal flip only
   WFC_STREAM_IMAGE_FLIP_BOTH,         //< Horizontal and vertical flip (180 degree rotation)

   WFC_STREAM_IMAGE_FLIP_SENTINEL = 0x7FFFFFFF
} WFC_STREAM_IMAGE_FLIP_T;

//! Describes a generic buffer on the reloctable heap.
typedef struct
{
   uint32_t length;                    //< The size of the structure passed in the message. Used for versioning.
   WFC_STREAM_IMAGE_TYPE_T type;       //< The type of the image buffer e.g. opaque.
   uint32_t handle;                    //< The relocatable heap handle for the buffer
   uint32_t width;                     //< Width of the image in pixels
   uint32_t height;                    //< Height of the image in pixels
   uint32_t format;                    //< The pixel format. Specific to type.
   uint32_t pitch;                     //< The horizontal pitch of the image in bytes
   uint32_t vpitch;                    //< The vertical pitch of the image in rows. Zero implies vpitch == height
   WFC_PROTECTION_FLAGS_T protection;  //< DRM protection
   uint32_t offset;                    //< The starting offset within the heap handle for the buffer
   uint32_t flags;                     //< Type-specific flags associated with the buffer
   WFC_STREAM_IMAGE_FLIP_T flip;       //< Flips to apply to the buffer for display
} WFC_STREAM_IMAGE_T;

//==============================================================================
// VideoCore-specific definitions
//==============================================================================

//!@name
//! VideoCore standard display identifiers (c.f. vc_dispmanx_types.h).
//!@{
#define WFC_ID_MAIN_LCD    0
#define WFC_ID_AUX_LCD     1
#define WFC_ID_HDMI        2
#define WFC_ID_SDTV        3

#define WFC_ID_MAX_SCREENS 3
//!@}

//------------------------------------------------------------------------------
// Non-standard definitions

//!@brief WF-C assumes that images are pre-multiplied prior to blending. However, we
//! need to handle non-pre-multiplied ones.
#define WFC_TRANSPARENCY_SOURCE_VC_NON_PRE_MULT    (1 << 31)

//!@brief Raw pixel formats supported by the server
typedef enum
{
   WFC_PIXEL_FORMAT_NONE,
   WFC_PIXEL_FORMAT_YVU420PLANAR,
   WFC_PIXEL_FORMAT_YUV420PLANAR,
   WFC_PIXEL_FORMAT_FORCE_32BIT   = 0x7FFFFFFF
} WFC_PIXEL_FORMAT_T;

//------------------------------------------------------------------------------
// Non-standard functions

void wfc_set_deferral_stream(WFCDevice dev, WFCContext ctx, WFCNativeStreamType stream);

//==============================================================================

// Callback function types.

/** Called when the buffer of a stream is no longer in use.
 *
 * @param The client stream handle.
 * @param The value passed in when the buffer was set as the front buffer.
 */
typedef void (*WFC_SERVER_STREAM_CALLBACK_T)(WFCNativeStreamType stream, void *cb_data);

//==============================================================================

#endif /* WFC_INT_H */
