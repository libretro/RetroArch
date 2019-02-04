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

#ifndef VC_DISPSERVICE_DEFS_H
#define VC_DISPSERVICE_DEFS_H

#define  HOST_PITCH_ALIGNMENT    4

//Round up to the nearest multiple of 16
#define  PAD16(x) (((x) + (VC_INTERFACE_BLOCK_SIZE-1)) & ~(VC_INTERFACE_BLOCK_SIZE-1))

//The max length for an effect name
#define DISPMAN_MAX_EFFECT_NAME  (28)

typedef enum {
   // Values initially chosen to match VC_IMAGE_TYPE_T to aid debugging
   // This is now a mandatory constraint
   VC_FORMAT_RGB565    = 1,
   VC_FORMAT_YUV420    = 3,
   VC_FORMAT_RGB888    = 5,
   VC_FORMAT_RGBA32    = 15,
   VC_FORMAT_RGBA565   = 17,
   VC_FORMAT_RGBA16    = 18,
   VC_FORMAT_TF_RGBA32 = 20,
   VC_FORMAT_TF_RGBA16 = 23,
   VC_FORMAT_TF_RGB565 = 25,
   VC_FORMAT_BGR888    = 31,
   VC_FORMAT_BGR888_NP = 32,

   VC_FORMAT_ARGB8888  = 43,
   VC_FORMAT_XRGB8888  = 44,

   /* To force 32-bit storage, enabling use in structures over-the-wire */
   VC_FORMAT_RANGE_MAX = 0x7FFFFFFF
} VC_IMAGE_FORMAT_T;

// Transforms.
/* Image transformations. These must match the DISPMAN and Media Player versions */
#define TRANSFORM_HFLIP     (1<<0)
#define TRANSFORM_VFLIP     (1<<1)
#define TRANSFORM_TRANSPOSE (1<<2)

typedef enum {
   VC_DISPMAN_ROT0           = 0,
   VC_DISPMAN_MIRROR_ROT0    = TRANSFORM_HFLIP,
   VC_DISPMAN_MIRROR_ROT180  = TRANSFORM_VFLIP,
   VC_DISPMAN_ROT180         = TRANSFORM_HFLIP|TRANSFORM_VFLIP,
   VC_DISPMAN_MIRROR_ROT90   = TRANSFORM_TRANSPOSE,
   VC_DISPMAN_ROT270         = TRANSFORM_TRANSPOSE|TRANSFORM_HFLIP,
   VC_DISPMAN_ROT90          = TRANSFORM_TRANSPOSE|TRANSFORM_VFLIP,
   VC_DISPMAN_MIRROR_ROT270  = TRANSFORM_TRANSPOSE|TRANSFORM_HFLIP|TRANSFORM_VFLIP,
} VC_DISPMAN_TRANSFORM_T;

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
   VC_DISPMAN_DISPLAY_SET_DESTINATION = 0,
   VC_DISPMAN_DISPLAY_UPDATE_START,
   VC_DISPMAN_DISPLAY_UPDATE_END,
   VC_DISPMAN_DISPLAY_OBJECT_ADD,
   VC_DISPMAN_DISPLAY_OBJECT_REMOVE,
   VC_DISPMAN_DISPLAY_OBJECT_MODIFY,
   VC_DISPMAN_DISPLAY_LOCK,
   VC_DISPMAN_DISPLAY_UNLOCK,
   VC_DISPMAN_DISPLAY_RESOURCE_CREATE,
   VC_DISPMAN_DISPLAY_RESOURCE_DELETE,
   VC_DISPMAN_DISPLAY_GET_COMPOSITE,
   VC_DISPMAN_DISPLAY_APPLY_EFFECT_INSTANCE,
   VC_DISPMAN_DISPLAY_RECONFIGURE,
   VC_DISPMAN_DISPLAY_CREATE_EFFECTS_INSTANCE,
   VC_DISPMAN_DISPLAY_DELETE_EFFECTS_INSTANCE,
   VC_DISPMAN_DISPLAY_SET_EFFECT,
   VC_DISPMAN_DISPLAY_RESOURCE_SET_ALPHA,
   VC_DISPMAN_DISPLAY_SNAPSHOT,
   VC_DISPMAN_DISPLAY_QUERY_IMAGE_FORMATS,
   VC_DISPMAN_DISPLAY_GET_DISPLAY_DETAILS,
   // new features - add to end of list
   VC_DISPMAN_DISPLAY_RESOURCE_CREATE_FROM_IMAGE,
   VC_CMD_END_OF_LIST
} VC_CMD_CODE_T;

/* The table of functions executed for each command. */

typedef void (*INTERFACE_EXECUTE_FN_T)(int, int);

extern INTERFACE_EXECUTE_FN_T interface_execute_fn[];

//Parameter sets for dispservice commands
typedef struct {
   uint32_t state;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_LOCK_PARAM_T;

typedef struct {
   uint32_t display;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_GET_DISPLAY_DETAILS_PARAM_T;

typedef struct {
   uint32_t display;
   uint32_t resource;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_SET_DEST_PARAM_T;

typedef struct {
   uint32_t display;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_GET_COMPOSITE_PARAM_T;

typedef struct
{
   uint32_t display;
   uint32_t effects_instance;

   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_APPLY_EFFECTS_INSTANCE_PARAM_T;

typedef struct
{
   uint32_t read_response;
   uint32_t effects_instance;
} DISPMAN_CREATE_EFFECTS_INSTANCE_RESPONSE_T;

typedef struct
{
   uint32_t effects_instance;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_DELETE_EFFECTS_INSTANCE_PARAM_T;

typedef struct
{
   uint32_t effects_instance;
   char effect_name[ DISPMAN_MAX_EFFECT_NAME ];
   //no need to pad as long as DISPMAN_MAX_EFFECT_NAME +sizeof(uint32) = 32
} DISPMAN_SET_EFFECT_PARAM_T;

typedef struct {
   uint32_t display;
   uint16_t width;
   uint16_t height;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_RECONFIGURE_PARAM_T;

typedef struct {
   uint32_t display;
   uint32_t transparent_colour;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_SET_TRANSPARENT_COLOUR_PARAM_T;

typedef struct {
   //uint32_t object;
   uint32_t display;
   int32_t layer;
   uint32_t transform;
   uint32_t resource;
   uint16_t dest_x;
   uint16_t dest_y;
   uint16_t dest_width;
   uint16_t dest_height;
   uint16_t src_x;
   uint16_t src_y;
   uint16_t src_width;
   uint16_t src_height;
} DISPMAN_OBJECT_ADD_PARAM_T;

typedef struct {
   uint32_t object;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_OBJECT_REMOVE_PARAM_T;

typedef struct {
   uint32_t object;
   uint16_t src_x;
   uint16_t src_y;
   uint16_t width;
   uint16_t height;
   uint32_t dummy[1];   // Pad to multiple of 16 bytes
} DISPMAN_OBJECT_MODIFY_PARAM_T;

typedef struct
{
   uint32_t *resource;
   VC_IMAGE_PARAM_T image;
   uint8_t  type;   // VC_RESOURCE_TYPE_T
   //Removed padding.  VC_IMAGE_T may change in size, so handle the size in the code that sends and receives the commands
   //uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_RESOURCE_CREATE_PARAM_T;

typedef struct
{
   uint32_t native_image_ptr;
   uint32_t type;   // VC_RESOURCE_TYPE_T
   uint32_t dummy[2];  // Pad to multiple of 16 bytes
} DISPMAN_RESOURCE_CREATE_FROM_IMAGE_PARAM_T;

typedef struct {
   uint32_t resource;
   uint32_t dummy[3];   //Pad to multiple of 16 bytes
} DISPMAN_RESOURCE_DELETE_PARAM_T;

typedef struct {
   uint32_t resource;
   uint32_t alpha;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_RESOURCE_SET_ALPHA_PARAM_T;

typedef struct {
   uint32_t display;
   uint32_t resource;
   uint32_t dummy[2];   //Pad to multiple of 16 bytes
} DISPMAN_DISPLAY_SNAPSHOT_PARAM_T;

#endif   //VC_DISPSERVICE_DEFS_H
