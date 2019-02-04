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

// Display service command enumeration.

#ifndef VC_DISPSERVICEX_DEFS_H
#define VC_DISPSERVICEX_DEFS_H

#include "interface/vctypes/vc_image_types.h"

#define  HOST_PITCH_ALIGNMENT    4

//Round up to the nearest multiple of 16
#define  PAD16(x) (((x) + (VC_INTERFACE_BLOCK_SIZE-1)) & ~(VC_INTERFACE_BLOCK_SIZE-1))

//The max length for an effect name
#define DISPMANX_MAX_EFFECT_NAME  (28)

// Should really use the VC_IMAGE_TYPE_T, but this one has been extended
// to force it up to 32-bits...
typedef enum {
   // Values initially chosen to match VC_IMAGE_TYPE_T to aid debugging
   // This is now a mandatory constraint
   VC_FORMAT_RGB565    = VC_IMAGE_RGB565,
   VC_FORMAT_YUV420    = VC_IMAGE_YUV420,
   VC_FORMAT_RGB888    = VC_IMAGE_RGB888,
   VC_FORMAT_RGBA32    = VC_IMAGE_RGBA32,
   VC_FORMAT_RGBA565   = VC_IMAGE_RGBA565,
   VC_FORMAT_RGBA16    = VC_IMAGE_RGBA16,
   VC_FORMAT_TF_RGBA32 = VC_IMAGE_TF_RGBA32,
   VC_FORMAT_TF_RGBA16 = VC_IMAGE_TF_RGBA16,
   VC_FORMAT_TF_RGB565 = VC_IMAGE_TF_RGB565,
   VC_FORMAT_BGR888    = VC_IMAGE_BGR888,
   VC_FORMAT_BGR888_NP = VC_IMAGE_BGR888_NP,

   VC_FORMAT_ARGB8888  = VC_IMAGE_ARGB8888,
   VC_FORMAT_XRGB8888  = VC_IMAGE_XRGB8888,

   /* To force 32-bit storage, enabling use in structures over-the-wire */
   VC_FORMAT_RANGE_MAX = 0x7FFFFFFF
} VC_IMAGE_FORMAT_T;

// Transforms.
/* Image transformations. These must match the DISPMAN and Media Player versions */
#define TRANSFORM_HFLIP     (1<<0)
#define TRANSFORM_VFLIP     (1<<1)
#define TRANSFORM_TRANSPOSE (1<<2)

#define VC_DISPMAN_ROT0 VC_IMAGE_ROT0
#define VC_DISPMAN_ROT90 VC_IMAGE_ROT90
#define VC_DISPMAN_ROT180 VC_IMAGE_ROT180
#define VC_DISPMAN_ROT270 VC_IMAGE_ROT270
#define VC_DISPMAN_MIRROR_ROT0 VC_IMAGE_MIRROR_ROT0
#define VC_DISPMAN_MIRROR_ROT90 VC_IMAGE_MIRROR_ROT90
#define VC_DISPMAN_MIRROR_ROT180 VC_IMAGE_MIRROR_ROT180
#define VC_DISPMAN_MIRROR_ROT270 VC_IMAGE_MIRROR_ROT270
#define VC_DISPMAN_TRANSFORM_T VC_IMAGE_TRANSFORM_T

typedef enum {
   VC_RESOURCE_TYPE_HOST,
   VC_RESOURCE_TYPE_VIDEOCORE,
   VC_RESOURCE_TYPE_VIDEOCORE_UNCACHED,
} VC_RESOURCE_TYPE_T;

typedef struct {
   uint8_t  type;            // VC_IMAGE_FORMAT_T
   uint32_t width;           // width in pixels
   uint32_t height;          // height in pixels
   uint32_t pitch;           // pitch of image_data array in *bytes*
   uint32_t size;            // number of *bytes* available in the image_data arry
   uint32_t pointer;         // pointer for image_data - this allows the object to be cast to a VC_IMAGE_T on the VIDEOCORE side
} VC_IMAGE_PARAM_T;

typedef enum {
   VC_DISPMANX_GET_DEVICES = 0,
   VC_DISPMANX_GET_DEVICE_NAME,
   VC_DISPMANX_GET_MODES,
   VC_DISPMANX_GET_MODE_INFO,
   VC_DISPMANX_DISPLAY_QUERY_IMAGE_FORMATS,
   // Resources
   VC_DISPMANX_RESOURCE_CREATE,
   VC_DISPMANX_RESOURCE_WRITE_DATA,
   VC_DISPMANX_RESOURCE_DELETE,
   // Displays
   VC_DISPMANX_DISPLAY_OPEN,
   VC_DISPMANX_DISPLAY_OPEN_MODE,
   VC_DISPMANX_DISPLAY_OPEN_OFFSCREEN,
   VC_DISPMANX_DISPLAY_RECONFIGURE,
   VC_DISPMANX_DISPLAY_SET_DESTINATION,
   VC_DISPMANX_DISPLAY_SET_BACKGROUND,
   VC_DISPMANX_DISPLAY_GET_INFO,
   VC_DISPMANX_DISPLAY_CLOSE,
   // Updates
   VC_DISPMANX_UPDATE_START,
   VC_DISPMANX_ELEMENT_ADD,
   VC_DISPMANX_ELEMENT_CHANGE_SOURCE,
   VC_DISPMANX_ELEMENT_MODIFIED,
   VC_DISPMANX_ELEMENT_REMOVE,
   VC_DISPMANX_UPDATE_SUBMIT,
   VC_DISPMANX_UPDATE_SUBMIT_SYNC,
   // Miscellaneous
   VC_DISPMANX_SNAPSHOT,
   // new features - add to end of list
   VC_CMD_END_OF_LIST
} VC_CMD_CODE_T;

/* The table of functions executed for each command. */

typedef void (*INTERFACE_EXECUTE_FN_T)(int, int);

extern INTERFACE_EXECUTE_FN_T interface_execute_fn[];

#define DISPMANX_MAX_HOST_DEVICES 8
#define DISPMANX_MAX_DEVICE_NAME_LEN 16

//Parameter sets for dispservice commands

typedef struct {
   int32_t response;
   uint32_t ndevices;
   uint32_t dummy[2];
   uint8_t names[DISPMANX_MAX_HOST_DEVICES][DISPMANX_MAX_DEVICE_NAME_LEN];
} DISPMANX_GET_DEVICES_RESP_T;
typedef struct {
   uint32_t device;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMANX_GET_MODES_PARAM_T;
typedef struct {
   uint32_t display;
   uint32_t mode;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMANX_GET_MODE_INFO_PARAM_T;
typedef struct {
   uint32_t type;
   uint32_t width;
   uint32_t height;
   uint32_t dummy[1];   // Pad to multiple of 16 bytes
} DISPMANX_RESOURCE_CREATE_PARAM_T;
typedef struct {
   // This will be needed when we change to vchi.
   int junk; // empty structure not allowed
} DISPMANX_RESOURCE_WRITE_DATA_PARAM_T;
typedef struct {
   uint32_t handle;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMANX_RESOURCE_DELETE_PARAM_T;
typedef struct {
   uint32_t device;
   uint32_t dummy[3];
} DISPMANX_DISPLAY_OPEN_PARAM_T;
typedef struct {
   uint32_t device;
   uint32_t mode;
   uint32_t dummy[2];
} DISPMANX_DISPLAY_OPEN_MODE_PARAM_T;
typedef struct {
   uint32_t dest;
   uint32_t orientation;
   uint32_t dummy[2];
} DISPMANX_DISPLAY_OPEN_OFFSCREEN_PARAM_T;
typedef struct {
   uint32_t display;
   uint32_t dest;
   uint32_t dummy[2];
} DISPMANX_DISPLAY_SET_DESTINATION_PARAM_T;
typedef struct {
   uint32_t display;
   uint32_t update;
   uint32_t colour;
   uint32_t dummy;
} DISPMANX_DISPLAY_SET_BACKGROUND_PARAM_T;
typedef struct {
   uint32_t display;
   uint32_t dummy[3];
} DISPMANX_DISPLAY_GET_INFO_PARAM_T;
typedef struct {
   uint32_t read_response;
   int32_t      width;
   int32_t      height;
   int32_t      aspect_pixwidth;
   int32_t      aspect_pixheight;
   int32_t      fieldrate_num;
   int32_t      fieldrate_denom;
   int32_t      fields_per_frame;
   uint32_t transform;
   uint32_t dummy[3];
} DISPMANX_DISPLAY_GET_INFO_RESP_T;
typedef struct {
   int32_t priority;
   uint32_t dummy[3];
} DISPMANX_UPDATE_START_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t display;
   int32_t layer;
   uint32_t transform;
   uint32_t src_resource;
   uint16_t dest_x;
   uint16_t dest_y;
   uint16_t dest_width;
   uint16_t dest_height;
   uint16_t src_x;
   uint16_t src_y;
   uint16_t src_width;
   uint16_t src_height;
   uint32_t flags;
   uint32_t opacity;
   uint32_t mask_resource;
   // already 16 byte aligned
} DISPMANX_ELEMENT_ADD_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t element;
   uint32_t src_resource;
   uint32_t dummy; // pad to 16 bytes
} DISPMANX_ELEMENT_CHANGE_SOURCE_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t element;
   uint16_t x;
   uint16_t y;
   uint16_t width;
   uint16_t height;
} DISPMANX_ELEMENT_MODIFIED_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t element;
   uint32_t dummy[2];
} DISPMANX_ELEMENT_REMOVE_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t dummy[3];
} DISPMANX_UPDATE_SUBMIT_PARAM_T;
typedef struct {
   uint32_t update;
   uint32_t dummy[3];
} DISPMANX_UPDATE_SUBMIT_SYNC_PARAM_T;
typedef struct {
   uint32_t display;
   uint32_t snapshot_resource;
   uint32_t transform;
   uint32_t dummy[1];
} DISPMANX_DISPLAY_SNAPSHOT_PARAM_T;

// for dispmanx

#define TRANSFORM_HFLIP     (1<<0)
#define TRANSFORM_VFLIP     (1<<1)
#define TRANSFORM_TRANSPOSE (1<<2)


#endif   //VC_DISPSERVICEX_DEFS_H
