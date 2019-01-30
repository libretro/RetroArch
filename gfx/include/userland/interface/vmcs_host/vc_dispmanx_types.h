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

// Typedefs and enums for the VideoCore III Display Manager

#ifndef _DISPMANX_TYPES_H
#define _DISPMANX_TYPES_H

#include "interface/vctypes/vc_image_types.h"
#include "interface/vctypes/vc_display_types.h"

#define VC_DISPMANX_VERSION   1

/* Opaque handles */
typedef uint32_t DISPMANX_DISPLAY_HANDLE_T;
typedef uint32_t DISPMANX_UPDATE_HANDLE_T;
typedef uint32_t DISPMANX_ELEMENT_HANDLE_T;
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;

typedef uint32_t DISPMANX_PROTECTION_T;

#define DISPMANX_NO_HANDLE 0

#define DISPMANX_PROTECTION_MAX   0x0f
#define DISPMANX_PROTECTION_NONE  0
#define DISPMANX_PROTECTION_HDCP  11   // Derived from the WM DRM levels, 101-300



/* Default display IDs.
   Note: if you overwrite with your own dispmanx_platform_init function, you
   should use IDs you provided during dispmanx_display_attach.
*/
#define DISPMANX_ID_MAIN_LCD  0
#define DISPMANX_ID_AUX_LCD   1
#define DISPMANX_ID_HDMI      2
#define DISPMANX_ID_SDTV      3
#define DISPMANX_ID_FORCE_LCD 4
#define DISPMANX_ID_FORCE_TV  5
#define DISPMANX_ID_FORCE_OTHER 6 /* non-default display */

/* Return codes. Nonzero ones indicate failure. */
typedef enum {
  DISPMANX_SUCCESS      = 0,
  DISPMANX_INVALID      = -1
  /* XXX others TBA */
} DISPMANX_STATUS_T;

typedef enum {
  /* Bottom 2 bits sets the orientation */
  DISPMANX_NO_ROTATE = 0,
  DISPMANX_ROTATE_90 = 1,
  DISPMANX_ROTATE_180 = 2,
  DISPMANX_ROTATE_270 = 3,

  DISPMANX_FLIP_HRIZ = 1 << 16,
  DISPMANX_FLIP_VERT = 1 << 17,

  /* invert left/right images */
  DISPMANX_STEREOSCOPIC_INVERT =  1 << 19,
  /* extra flags for controlling 3d duplication behaviour */
  DISPMANX_STEREOSCOPIC_NONE   =  0 << 20,
  DISPMANX_STEREOSCOPIC_MONO   =  1 << 20,
  DISPMANX_STEREOSCOPIC_SBS    =  2 << 20,
  DISPMANX_STEREOSCOPIC_TB     =  3 << 20,
  DISPMANX_STEREOSCOPIC_MASK   = 15 << 20,

  /* extra flags for controlling snapshot behaviour */
  DISPMANX_SNAPSHOT_NO_YUV = 1 << 24,
  DISPMANX_SNAPSHOT_NO_RGB = 1 << 25,
  DISPMANX_SNAPSHOT_FILL = 1 << 26,
  DISPMANX_SNAPSHOT_SWAP_RED_BLUE = 1 << 27,
  DISPMANX_SNAPSHOT_PACK = 1 << 28
} DISPMANX_TRANSFORM_T;

typedef enum {
  /* Bottom 2 bits sets the alpha mode */
  DISPMANX_FLAGS_ALPHA_FROM_SOURCE = 0,
  DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS = 1,
  DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO = 2,
  DISPMANX_FLAGS_ALPHA_FIXED_EXCEED_0X07 = 3,

  DISPMANX_FLAGS_ALPHA_PREMULT = 1 << 16,
  DISPMANX_FLAGS_ALPHA_MIX = 1 << 17,
  DISPMANX_FLAGS_ALPHA_DISCARD_LOWER_LAYERS = 1 << 18,
} DISPMANX_FLAGS_ALPHA_T;

typedef struct {
  DISPMANX_FLAGS_ALPHA_T flags;
  uint32_t opacity;
  VC_IMAGE_T *mask;
} DISPMANX_ALPHA_T;

typedef struct {
  DISPMANX_FLAGS_ALPHA_T flags;
  uint32_t opacity;
  DISPMANX_RESOURCE_HANDLE_T mask;
} VC_DISPMANX_ALPHA_T;  /* for use with vmcs_host */

typedef enum {
  DISPMANX_FLAGS_CLAMP_NONE = 0,
  DISPMANX_FLAGS_CLAMP_LUMA_TRANSPARENT = 1,
#if __VCCOREVER__ >= 0x04000000
  DISPMANX_FLAGS_CLAMP_TRANSPARENT = 2,
  DISPMANX_FLAGS_CLAMP_REPLACE = 3
#else
  DISPMANX_FLAGS_CLAMP_CHROMA_TRANSPARENT = 2,
  DISPMANX_FLAGS_CLAMP_TRANSPARENT = 3
#endif
} DISPMANX_FLAGS_CLAMP_T;

typedef enum {
  DISPMANX_FLAGS_KEYMASK_OVERRIDE = 1,
  DISPMANX_FLAGS_KEYMASK_SMOOTH = 1 << 1,
  DISPMANX_FLAGS_KEYMASK_CR_INV = 1 << 2,
  DISPMANX_FLAGS_KEYMASK_CB_INV = 1 << 3,
  DISPMANX_FLAGS_KEYMASK_YY_INV = 1 << 4
} DISPMANX_FLAGS_KEYMASK_T;

typedef union {
  struct {
    uint8_t yy_upper;
    uint8_t yy_lower;
    uint8_t cr_upper;
    uint8_t cr_lower;
    uint8_t cb_upper;
    uint8_t cb_lower;
  } yuv;
  struct {
    uint8_t red_upper;
    uint8_t red_lower;
    uint8_t blue_upper;
    uint8_t blue_lower;
    uint8_t green_upper;
    uint8_t green_lower;
  } rgb;
} DISPMANX_CLAMP_KEYS_T;

typedef struct {
  DISPMANX_FLAGS_CLAMP_T mode;
  DISPMANX_FLAGS_KEYMASK_T key_mask;
  DISPMANX_CLAMP_KEYS_T key_value;
  uint32_t replace_value;
} DISPMANX_CLAMP_T;

typedef struct {
  int32_t width;
  int32_t height;
  DISPMANX_TRANSFORM_T transform;
  DISPLAY_INPUT_FORMAT_T input_format;
  uint32_t display_num;
} DISPMANX_MODEINFO_T;

/* Update callback. */
typedef void (*DISPMANX_CALLBACK_FUNC_T)(DISPMANX_UPDATE_HANDLE_T u, void * arg);

/* Progress callback */
typedef void (*DISPMANX_PROGRESS_CALLBACK_FUNC_T)(DISPMANX_UPDATE_HANDLE_T u,
                                                  uint32_t line,
                                                  void * arg);

/* Pluggable display interface */

typedef struct tag_DISPMANX_DISPLAY_FUNCS_T {
   // Get essential HVS configuration to be passed to the HVS driver. Options
   // is any combination of the following flags: HVS_ONESHOT, HVS_FIFOREG,
   // HVS_FIFO32, HVS_AUTOHSTART, HVS_INTLACE; and if HVS_FIFOREG, one of;
   // { HVS_FMT_RGB888, HVS_FMT_RGB565, HVS_FMT_RGB666, HVS_FMT_YUV }.
   int32_t (*get_hvs_config)(void *instance, uint32_t *pchan,
                             uint32_t *poptions, DISPLAY_INFO_T *info,
                             uint32_t *bg_colour, uint32_t *test_mode);
   
   // Get optional HVS configuration for gamma tables, OLED matrix and dither controls.
   // Set these function pointers to NULL if the relevant features are not required.
   int32_t (*get_gamma_params)(void * instance,
                               int32_t gain[3], int32_t offset[3], int32_t gamma[3]);
   int32_t (*get_oled_params)(void * instance, uint32_t * poffsets,
                              uint32_t coeffs[3]);
   int32_t (*get_dither)(void * instance, uint32_t * dither_depth, uint32_t * dither_type);
   
   // Get mode information, which may be returned to the applications as a courtesy.
   // Transform should be set to 0, and {width,height} should be final dimensions.
   int32_t (*get_info)(void * instance, DISPMANX_MODEINFO_T * info);
   
   // Inform driver that the application refcount has become nonzero / zero
   // These callbacks might perhaps be used for backlight and power management.
   int32_t (*open)(void * instance);
   int32_t (*close)(void * instance);
   
   // Display list updated callback. Primarily of use to a "one-shot" display.
   // For convenience of the driver, we pass the register address of the HVS FIFO.
   void (*dlist_updated)(void * instance, volatile uint32_t * fifo_reg);
   
   // End-of-field callback. This may occur in an interrupt context.
   void (*eof_callback)(void * instance);

   // Return screen resolution format
   DISPLAY_INPUT_FORMAT_T (*get_input_format)(void * instance);

   int32_t (*suspend_resume)(void *instance, int up);

   DISPLAY_3D_FORMAT_T (*get_3d_format)(void * instance);
} DISPMANX_DISPLAY_FUNCS_T;

#endif /* ifndef _DISPMANX_TYPES_H */
