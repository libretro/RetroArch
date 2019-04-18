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

/**
 * \file
 *
 * This file describes the public interfaces to the old vc_image library.
 */

#ifndef VC_IMAGE_H
#define VC_IMAGE_H

#if !defined(__VIDEOCORE__) && !defined(__vce__) && !defined(VIDEOCORE_CODE_IN_SIMULATION)
#error This code is only for use on VideoCore. If it is being included
#error on the host side, then you have done something wrong. Defining
#error __VIDEOCORE__ is *not* the right answer!
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* Definition for VC_IMAGE_TYPE_T live in here */
#include "vcinclude/vc_image_types.h"
#include "helpers/vc_image/vc_image_helper.h"

#if !defined __SYMBIAN32__

#if !defined( _MSC_VER) && !defined(__WIN32__)
   #include "vcfw/rtos/rtos.h"
#endif
#include "vcfw/rtos/common/rtos_common_mem.h"

#include "helpers/vc_image/vc_image_metadata.h"

#if defined(__VIDEOCORE3__) && !defined(RGB888)
#ifdef __GNUC__
#warning "RGB888 should be defined on all VideoCore3 platforms, please check your platform makefile."
#else
#  warn "RGB888 should be defined on all VideoCore3 platforms, please check your platform makefile."
#endif
#endif

   /* Types of extra properties that can be passed to vc_image_configure() */

   typedef enum
   {
      VC_IMAGE_PROP_END = 0,
      VC_IMAGE_PROP_NONE = 0x57EAD400,
      VC_IMAGE_PROP_ALIGN,
      VC_IMAGE_PROP_STAGGER,
      VC_IMAGE_PROP_PADDING,
      VC_IMAGE_PROP_PRIORITY,
      VC_IMAGE_PROP_MALLOCFN,
      VC_IMAGE_PROP_CACHEALIAS,
      VC_IMAGE_PROP_CLEARVALUE,

      /* insert only allocation-related properties above this line */

      VC_IMAGE_PROP_REF_IMAGE = 0x57EAD47f,
      VC_IMAGE_PROP_ROTATED,
      VC_IMAGE_PROP_PITCH,
      VC_IMAGE_PROP_DATA,
      VC_IMAGE_PROP_SIZE,
      VC_IMAGE_PROP_PALETTE,
      VC_IMAGE_PROP_MIPMAPS,
      VC_IMAGE_PROP_BAYER_ORDER,
      VC_IMAGE_PROP_BAYER_FORMAT,
      VC_IMAGE_PROP_BAYER_BLOCK_SIZE,
      VC_IMAGE_PROP_CODEC_FOURCC,
      VC_IMAGE_PROP_CODEC_MAXSIZE,
      VC_IMAGE_PROP_CUBEMAP,
      VC_IMAGE_PROP_OPENGL_FORMAT_AND_TYPE,
      VC_IMAGE_PROP_INFO,
      VC_IMAGE_PROP_METADATA,
      VC_IMAGE_PROP_NEW_LIST,
      /* Multi-channel properties */
      VC_IMAGE_PROP_NUM_CHANNELS,
      VC_IMAGE_PROP_IS_TOP_BOTTOM,
      VC_IMAGE_PROP_IS_DECIMATED,
      VC_IMAGE_PROP_IS_PACKED,
      VC_IMAGE_PROP_YUV_COLOURSPACE,
      /* Linked-multichannel properties*/
      VC_IMAGE_PROP_LINKED_MULTICHANN,
      VC_IMAGE_PROP_VPITCH
   } VC_IMAGE_PROPERTY_T;

   /* A property key and value */
   typedef struct
   {
      VC_IMAGE_PROPERTY_T prop;
      unsigned long value;
   } VC_IMAGE_PROPLIST_T;
#define VC_IMAGE_PROPLIST_COUNT(x) (sizeof(x)/sizeof(x[0]))
#define VC_IMAGE_PROPLIST_NULL   ((VC_IMAGE_PROPLIST_T*)0)

   /* Useful bits that can be set, to check validity we'll assume that all other
      bits should be zero and enforce this in vc_image functions to catch people
      who aren't initialising the VC_IMAGE_T structure nicely; update when other
      bits are added */
#define VC_IMAGE_INFO_VALIDBITS    0x9F0F
   /* Define the bits of info used to denote the colourspace */
#define VC_IMAGE_INFO_COLOURSPACE  0x0F

#endif //#if !defined __SYMBIAN32__

#define is_valid_vc_image_hndl(_img, _type) ((_img) != NULL \
                                 && (_img)->image_data == NULL \
                                 && ((_img)->info.info & ~VC_IMAGE_INFO_VALIDBITS) == 0 \
                                 && (_type) > VC_IMAGE_MIN && (_type) < VC_IMAGE_MAX \
                                 && (_img)->type == (_type) \
                                 )

#define is_valid_vc_image_buf(_img, _type) ((_img) != NULL \
                                 && (_img)->image_data != NULL \
                                 && ((_img)->info.info & ~VC_IMAGE_INFO_VALIDBITS) == 0 \
                                 && (_type) > VC_IMAGE_MIN && (_type) < VC_IMAGE_MAX \
                                 && (_img)->type == (_type) \
                                 )

#define is_within_rectangle(x, y, w, h, cont_x, cont_y, cont_w, cont_h) ( \
                              (x) >= (cont_x) \
                              && (y) >= (cont_y) \
                              && ((x) + (w)) <= ((cont_x) + (cont_w)) \
                              && ((y) + (h)) <= ((cont_y) + (cont_h)) \
                              )

#if !defined __SYMBIAN32__

   /**
    * \brief storage for pointers to image headers of the previous and next channel
    *
    *
    * When linked-multichannel structure is being used, this structure stores the pointers
    * necessary to link the current image header in to make represent a multichannel image.
    */
   struct VC_IMAGE_LINKED_MULTICHANN_T {
      VC_IMAGE_T* prev;
      VC_IMAGE_T* next;
   };
   typedef struct VC_IMAGE_LINKED_MULTICHANN_T VC_IMAGE_LINKED_MULTICHANN_T;

   /* Point type for use in polygon drawing. */
   typedef struct vc_image_point_s {
      int x, y;
   } VC_IMAGE_POINT_T;

   /* Useful for rectangular crop regions in a vc_image */
   typedef struct vc_image_rect_s
   {
      unsigned int x, y;
      unsigned int width;
      unsigned int height;
   } VC_IMAGE_RECT_T;

   /* CRC type for the return of CRC results */
   typedef struct VC_IMAGE_CRC_T_ {
      unsigned int y;
      unsigned int uv;
   } VC_IMAGE_CRC_T;

   /* To make stack-based initialisation convenient: */

#define NULL_VC_IMAGE_T {0, 0, 0, 0, 0, 0}

   /* Convenient constants that can be passed more readably to vc_image_text. */

#define VC_IMAGE_COLOUR_TRANSPARENT -1
#define VC_IMAGE_COLOUR_FADE -2

#define CLIP_DEST(x_offset, y_offset, width, height, dest_width, dest_height) \
   if (x_offset<0)                              \
      width-=-x_offset, x_offset=0;             \
   if (y_offset<0)                              \
      height-=-y_offset, y_offset=0;            \
   if (x_offset+width>dest_width)               \
      width-=x_offset+width-dest_width;         \
   if (y_offset+height>dest_height)             \
      height-=y_offset+height-dest_height;      \

   /* Publicly exported functions. */

   /******************************************************************************
   General functions.
   ******************************************************************************/

//routine to return a bitmask (bit referenced by VC_IMAGE_TYPE_T) of the supported image formats built in
//this particular version of the vc_image library
   unsigned int vc_image_get_supported_image_formats( void );

   /******************************************************************************
   Data member access.
   ******************************************************************************/

   /* Initialise all fields of a VC_IMAGE_T to zero. */

   void vc_image_initialise(VC_IMAGE_T *image);

   /* Return specified channel of a multi-channel VC_IMAGE_T as a single-channel VC_IMAGE_T. */

   void vc_image_extract_channel(VC_IMAGE_T *extract, VC_IMAGE_T *image, uint8_t chan_idx);

   /* Fill out all fields of a VC_IMAGE_T in the same way as vc_image_malloc(), but non-allocating. */

#if defined(va_start) /* can't publish this without including <stdarg.h> */
   int vc_image_vconfigure(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, long width, long height, va_list proplist);
   int vc_image_vreconfigure(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, long width, long height, va_list proplist);
#endif
   int vc_image_configure_unwrapped(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, long width, long height, VC_IMAGE_PROPERTY_T prop, ...);
   int vc_image_reconfigure_unwrapped(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, long width, long height, VC_IMAGE_PROPERTY_T prop, ...);
   /* remaining arguments are a VC_IMAGE_PROPERTY_T list terminated with VC_IMAGE_END */

   int vc_image_configure_proplist(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, long width, long height, VC_IMAGE_PROPLIST_T *props, unsigned long n_props);

   void vc_image_lock( VC_IMAGE_BUF_T *dst, const VC_IMAGE_T *src );

   void vc_image_lock_extract( VC_IMAGE_BUF_T *dst, const VC_IMAGE_T *src, uint8_t chan_idx );

   void vc_image_lock_channel(VC_IMAGE_BUF_T *dst, const VC_IMAGE_T *chann);
   void vc_image_lock_channel_perma(VC_IMAGE_BUF_T *dst, const VC_IMAGE_T *chann); //lightweight version of lock channel

   void vc_image_unlock( VC_IMAGE_BUF_T *img );

   void vc_image_lock_perma( VC_IMAGE_BUF_T *dst, const VC_IMAGE_T *src );

   void vc_image_unlock_perma( VC_IMAGE_BUF_T *img );

   /* Mipmap-related functions. Image must be brcm1. */

   /* Take the base image of an image and scale it down into its mipmaps. */
   /* The filter argument type is likely to change, but 0 should always be a
    * reasonable value (even if it becomes a pointer). */
   void vc_image_rebuild_mipmaps(VC_IMAGE_BUF_T *dst, int filter);

   /******************************************************************************
   Image/bitmap manipulation.
   ******************************************************************************/

   /* Fill a region of an image with solid colour. */

   void vc_image_fill(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height, int value);

   /* Blt a region of an image to another. */

   void vc_image_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                     VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Blt a region of an image to another, changing the format on the fly. */

   void vc_image_convert_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                             VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Expose the special cases of above */
   void vc_image_yuv420_to_rgba32_part(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height, VC_IMAGE_BUF_T const *src, int src_x_offset, int src_y_offset, int alpha);

   void vc_image_rgb565_to_rgba32_part(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height, VC_IMAGE_BUF_T const *src, int src_x_offset, int src_y_offset, int alpha);
   /* Blt a region of an image to another with a nominated transparent colour. */

   void vc_image_transparent_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                 VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int transparent_colour);

   /* Blt a region of an image to another but only overwriting pixels of a given colour. */

   void vc_image_overwrite_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                               VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int overwrite_colour);

   /* Blt a region of an image to another only where a mask bitmap has 1s. */

   void vc_image_masked_blt (VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                             VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset,
                             VC_IMAGE_BUF_T *mask, int mask_x_offset, int mask_y_offset,
                             int invert);

   /* Masked fill a region with a solid colour. */

   void vc_image_masked_fill(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height, int value,
                             VC_IMAGE_BUF_T *mask, int mask_x_offset, int mask_y_offset,
                             int invert);

   /* Blt an image into 3 separate buffers, either YUV or RGB
      Will always copy the entire image. In the case of RGB image:
      Y = RGB, U = NULL, V = NULL */

   void vc_image_fetch(VC_IMAGE_BUF_T *src, unsigned char* Y, unsigned char* U, unsigned char* V);

   /* Blt the data in Y U V (or RGB) buffers into a VC_IMAGE structure
      Opposite to vc_image_put above */

   void vc_image_put(VC_IMAGE_BUF_T *src, unsigned char* Y, unsigned char* U, unsigned char* V);

   /* NOT a region of an image. */

   void vc_image_not(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height);

   /* Fade (or brighten) a region of an image. fade is an 8.8 value. */

   void vc_image_fade(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height, int fade);

   /* OR two images together. */

   void vc_image_or(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                    VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* XOR two images together. */

   void vc_image_xor(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                     VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Copy an image in its entirety (works on 1bpp images). */

   void vc_image_copy(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src);

   /* Transpose an image. */

   void vc_image_transpose(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src);
   int vc_image_transpose_ret(VC_IMAGE_T *dest, VC_IMAGE_T *src);

   /* Transpose an image and subsample it by some integer factor */
   void vc_image_transpose_and_subsample(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src, int factor);

   /* Vertically flip an image (turn "upside down"). */

   void vc_image_vflip(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src);

   /* Horizontally flip an image ("mirror"). */

   void vc_image_hflip(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src);

   /* Transpose an image in place. Relies on being able to preserve the pitch. */

   void vc_image_transpose_in_place(VC_IMAGE_BUF_T *dest);

   /* Vertically flip an image (turn "upside down") in place. */

   void vc_image_vflip_in_place(VC_IMAGE_BUF_T *dest);

   /* Horizontally flip an image ("mirror") in place. */

   void vc_image_hflip_in_place(VC_IMAGE_BUF_T *dest);

   /* Add text string to an image. */

   void vc_image_text(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text,
                      int *max_x, int *max_y);

   void vc_image_small_text(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text,
                            int *max_x, int *max_y);

   void vc_image_text_20(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text,
                         int *max_x, int *max_y);

   void vc_image_text_24(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text,
                         int *max_x, int *max_y);

   void vc_image_text_32(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text,
                         int *max_x, int *max_y);

   /* Experimental vector font thingy */

#define VC_IMAGE_ATEXT_FIXED (1<<16) // Add this to "size" if you want fixed-width

   int vc_image_atext_measure(int size, const char * str);

   int vc_image_atext_clip_length(int size, const char * str, int width);

   int vc_image_atext(VC_IMAGE_BUF_T * dest, int x, int y, int fg, int bg, int size, const char * str);

   /* Add optionally rotated text to an image */

   void vc_image_textrotate(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int fg_colour, int bg_colour, const char *text, int rotate);

   /* Line drawing. */

   void vc_image_line(VC_IMAGE_BUF_T *dest, int x_start, int y_start, int x_end, int y_end, int fg_colour);

   /* Unfilled Rect. */

   void vc_image_rect(VC_IMAGE_BUF_T *dest, int x_start, int y_start, int width, int height, int fg_colour);

   /* Add frame . */

   void vc_image_frame(VC_IMAGE_BUF_T *dest, int x_start, int y_start, int width, int height);

   void vc_image_aa_line(VC_IMAGE_BUF_T *dest, int x_start, int y_start, int x_end, int y_end, int thickness, int colour);
   void vc_image_aa_ellipse(VC_IMAGE_BUF_T *dest, int cx, int cy, int a, int b, int thickness, int colour, int filled);
   void vc_image_aa_polygon(VC_IMAGE_BUF_T *dest, VC_IMAGE_POINT_T *p, int n, int thickness, int colour, int filled);

   /* Copy and convert image to 48bpp. WARNING: offsets, width and height must be 16-pixel aligned. */

   void vc_image_convert_to_48bpp(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                  VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Copy and convert image from 16bpp to 24bpp. WARNING: offsets, width and height must be 16-pixel aligned. */

   void vc_image_convert_to_24bpp(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                  VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Convert and overlay image from 16bpp to 24bpp with nominated transparent colour. WARNING: offsets, width and height must be 16-pixel aligned. */

   void vc_image_overlay_to_24bpp(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                  VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int transparent_colour);

   /* Copy and convert image from 24bpp to 16bpp. WARNING: offsets, width and height must be 16-pixel aligned. */

   void vc_image_convert_to_16bpp(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                  VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset);

   /* Copy all, or a portion of src to dest with resize. This function is specialized
      for YUV images (other cases are not yet coded, and code could be rather large
      to link all cases by default). All dimensions should be multiples of 2.
      If smooth_flag nonzero, appropriate smoothing will be applied.                 */

   void vc_image_resize_yuv(VC_IMAGE_BUF_T *dest,
                            int dest_x_offset, int dest_y_offset,
                            int dest_width, int dest_height,
                            VC_IMAGE_BUF_T *src,
                            int src_x_offset, int src_y_offset,
                            int src_width, int src_height,
                            int smooth_flag);

   /* RGB565 resize. Pitch must be 32-byte aligned, dimensions need not be.
      XXX kept YUV and RGB565 version separate to avoid unnecessary linking.
      However if we're going down the DLL route they ought to be combined.   */

   void vc_image_resize_rgb565(VC_IMAGE_BUF_T * dest,
                               int dest_x_offset, int dest_y_offset,
                               int dest_width, int dest_height,
                               VC_IMAGE_BUF_T *src,
                               int src_x_offset, int src_y_offset,
                               int src_width, int src_height,
                               int smooth_flag);

   void vc_image_resize(VC_IMAGE_BUF_T *dest,
                        int dest_x_offset, int dest_y_offset,
                        int dest_width, int dest_height,
                        VC_IMAGE_BUF_T *src,
                        int src_x_offset, int src_y_offset,
                        int src_width, int src_height,
                        int smooth_flag);

   /* Resize YUV in strips. XXX this function has rather bizarre arguments. */

   void vc_image_resize_yuv_strip(VC_IMAGE_BUF_T * dest, VC_IMAGE_BUF_T * src,
                                  int src_x_offset, int src_width,
                                  int pos, int step, int smooth_flag,
                                  void * tmp_buf, int tmp_buf_size);

   /* Blit from a YUV image to an RGB image with optional mirroring followed
   by optional clockwise rotation */
   void vc_image_convert(VC_IMAGE_BUF_T *dest,VC_IMAGE_BUF_T *src,int mirror,int rotate);

   /* Convert bitmap from one type to another, doing vertical flip as part of the conversion */
   void vc_image_convert_vflip(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src);

   /* Convert from YUV to RGB, with optional downsample by 2 in each direction. */
   void vc_image_yuv2rgb(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src, int downsample);

   void vc_image_convert_yuv2rgb(unsigned char *datay, unsigned char *datau
                           , unsigned char *datav, int realwidth, int realheight
                           , unsigned short *buffer, int screenwidth, int screenheight);

   /* Frees up (using free_256bit) the source bytes and vc_image header */
   void vc_image_free(VC_IMAGE_T *img);

   /* Returns an image based on decompressing the given bytes (made by helpers/vc_image/runlength.m) */
   VC_IMAGE_BUF_T *vc_runlength_decode(unsigned char *data);

   /* Returns a 16bit image based on decompressing the given bytes (made by helpers/vc_image/covnertbmp2run.m) */
   VC_IMAGE_BUF_T *vc_run_decode(VC_IMAGE_BUF_T *img,unsigned char *data);

   /* Returns an image based on decompressing the given bytes (made by helpers/vc_image/runlength8.m) */
   VC_IMAGE_BUF_T *vc_runlength_decode8(unsigned char *data);

   /* Returns an image based on decompressing an image (made by helpers/vc_image/enc4by4.m)
   Will place the decompressed data into an existing image if supplied */
   VC_IMAGE_BUF_T *vc_4by4_decode(VC_IMAGE_BUF_T *img,unsigned short *data);

   /* Deblocks a YUV image using a simple averaging scheme on 8 by 8 blocks */
   void vc_image_deblock(VC_IMAGE_BUF_T *img);

   /* Pack the bytes (i.e. remove the padding bytes) in a vc_image. */
   void vc_image_pack(VC_IMAGE_BUF_T *img, int x, int y, int w, int h);

   /* Pack bytes as above, but copy them to another memory block in the process. */
   void vc_image_copy_pack (unsigned char *dest, VC_IMAGE_BUF_T *img, int x, int y, int w, int h);

   /* Unpack bytes to have the correct padding. */
   void vc_image_unpack(VC_IMAGE_BUF_T *img, int w, int h);

   /* Unpack bytes as above, but also copy them to another memory block in the process. */
   void vc_image_copy_unpack(VC_IMAGE_BUF_T *img, int dest_x_off, int dest_y_off, unsigned char *src, int w, int h);

   /* swap red/blue */
   void vc_image_swap_red_blue(VC_IMAGE_BUF_T *img);

#if defined(va_start) /* can't publish this without including <stdarg.h> */
   VC_IMAGE_BUF_T *vc_image_vparmalloc_unwrapped(VC_IMAGE_TYPE_T type, char const *description, long width, long height, va_list proplist);
#endif
   VC_IMAGE_BUF_T *vc_image_parmalloc_unwrapped(VC_IMAGE_TYPE_T type, char const *description, long width, long height, VC_IMAGE_PROPERTY_T prop, ...);

   void vc_image_parfree(VC_IMAGE_BUF_T *p);

   VC_IMAGE_BUF_T *vc_image_malloc( VC_IMAGE_TYPE_T type, long width, long height, int rotate );
   VC_IMAGE_BUF_T *vc_image_prioritymalloc( VC_IMAGE_TYPE_T type, long width, long height, int rotate, int priority, char const *name);
#define vc_image_priorityfree(p) vc_image_free(p)

   VC_IMAGE_T *vc_image_relocatable_alloc(VC_IMAGE_TYPE_T type, const char *description, long width, long height, MEM_FLAG_T flags, VC_IMAGE_PROPERTY_T prop, ...);
   void vc_image_relocatable_free(VC_IMAGE_T *img);

   void vc_image_set_palette( short *colourmap );
   short *vc_image_get_palette( VC_IMAGE_T *src );

   void vc_image_convert_in_place(VC_IMAGE_BUF_T *image, VC_IMAGE_TYPE_T new_type);

   /* Image transformations (flips and 90 degree rotations) */
   VC_IMAGE_TRANSFORM_T vc_image_inverse_transform(VC_IMAGE_TRANSFORM_T transform);

   VC_IMAGE_TRANSFORM_T vc_image_combine_transforms(VC_IMAGE_TRANSFORM_T transform1, VC_IMAGE_TRANSFORM_T transform2);

   void vc_image_transform_point(int *pX, int *pY, int w, int h, VC_IMAGE_TRANSFORM_T transform);

   void vc_image_transform_rect(int *pX, int *pY, int *pW, int *pH, int w, int h, VC_IMAGE_TRANSFORM_T transform);

   void vc_image_transform_dimensions(int *pW, int *pH, VC_IMAGE_TRANSFORM_T transform);

   void vc_image_transform(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src, VC_IMAGE_TRANSFORM_T transform);
   int vc_image_transform_ret(VC_IMAGE_T *dest, VC_IMAGE_T *src, VC_IMAGE_TRANSFORM_T transform);

   void vc_image_transform_in_place(VC_IMAGE_BUF_T *image, VC_IMAGE_TRANSFORM_T transform);

   void vc_image_transform_brcm1s(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src, VC_IMAGE_TRANSFORM_T transform, int hdeci);

   const char *vc_image_transform_string(VC_IMAGE_TRANSFORM_T xfm);

   /* Blt a region of an image to another with a nominated transparent colour and alpha blending.
   Alpha should be between 0 and 256 */
   void vc_image_transparent_alpha_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                       VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int transparent_colour, int alpha);

   /* Blt a region of an image to another with a nominated transparent colour and alpha blending.
   Alpha should be between 0 and 256 */
   void vc_image_transparent_alpha_blt_rotated(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset,
         VC_IMAGE_BUF_T *src, int transparent_colour, int alpha, int scale, int sin_theta, int cos_theta);

   /* Blt a special 4.4 font image generated from a PNG file.
   Only works with a 565 destination image.
   Can choose a colour via red,green,blue arguments.  Each component should be between 0 and 255.
   The alpha value present in the font image is scaled by the alpha argument given to the routine
   Alpha should be between 0 and 256 */
   void vc_image_font_alpha_blt(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int red, int green, int blue, int alpha);

   /* Utilities for image editor */
   extern void im_split_components(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src,
                                      int width, int height, int pitch);

   extern void im_merge_components(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src,
                                      int width, int height, int pitch);

   extern void im_merge_components_adjacent(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src,
            int width, int height, int pitch,
            int badjacent);

   /* Deflicker a YUV image using a simple low pass filter */
   void vc_image_deflicker(VC_IMAGE_BUF_T *img, unsigned char *context);

   /* Blend an Interlaced YUV image using a simple low pass filter which works on colour as well as luma*/
   void vc_image_blend_interlaced(VC_IMAGE_BUF_T *img, unsigned char *context);

   /* Stripe resizing. Resize a 16-high stripe horizontally to have the given width,
      or a 16-wide column to have the given height. Work in place, uses bilinear filtering. */
   void vc_image_resize_stripe_h(VC_IMAGE_BUF_T *image, int d_width, int s_width);
   void vc_image_resize_stripe_v(VC_IMAGE_BUF_T *image, int d_height, int s_height);

   /* Stripe resizing. Resize a horizontal stripe to 16-high, or a vertical stripe to 16 wide.
      Work in place, uses bilinear filtering. */
   void vc_image_decimate_stripe_h(VC_IMAGE_BUF_T *src, int offset, int step);
   void vc_image_decimate_stripe_v(VC_IMAGE_BUF_T *src, int offset, int step);

   extern int vc_image_set_alpha(VC_IMAGE_BUF_T *image,
                                    const int x,
                                    const int y,
                                    const int width,
                                    const int height,
                                    const int alpha );

   /* Make a horizontal alpha gradient mask for used in alpha blending below */
   extern void vc_image_make_alpha_gradient(VC_IMAGE_BUF_T *pimage,
            int start_alpha, int end_alpha);

   /* Alpha blend two images together using an alpha mask */
   extern void vc_image_alphablend(VC_IMAGE_BUF_T *dest, VC_IMAGE_BUF_T *src, VC_IMAGE_BUF_T *mask,
                                      int dest_xoffset, int dest_yoffset,
                                      int src_xoffset, int src_yoffset,
                                      int width, int height);

   typedef struct vc_image_pool_s * VC_IMAGE_POOL_HANDLE_T;
   typedef VC_IMAGE_POOL_HANDLE_T VC_IMAGE_POOL_HANDLE; /* legacy */

   /* Blt a region of an image onto a particular mipmap of a brcm1 image */
   /* XXX do not use this function - the interface is likely to change as I decide
    * what options are available here */
   void vc_image_XXX_mipmap_blt_XXX(VC_IMAGE_BUF_T *dest, int x_offset, int y_offset, int width, int height,
                                    VC_IMAGE_BUF_T *src, int src_x_offset, int src_y_offset, int miplvl, int cubeface, VC_IMAGE_TRANSFORM_T transform);

   /* Function to calculate the crc of the VC_IMAGE */
   VC_IMAGE_CRC_T vc_image_calc_crc_interlaced(VC_IMAGE_BUF_T *img, int cl, int cr, int ct, int cb, int field);

   /* make compatible with old function */
   #define vc_image_calc_crc(img,cl,cr,ct,cb) vc_image_calc_crc_interlaced((img),(cl),(cr),(ct),(cb),-1)

#define vc_image_calc_crc_fast vc_image_calc_crc

#if defined(NDEBUG)
#  define vc_image_debugstr(x) NULL
#else
#  define vc_image_debugstr(x) (x)
#endif

#define vc_image_vparmalloc(type, desc, width, height, proplist) vc_image_vpmalloc(type, vc_image_debugstr(desc), width, height, proplist)
#if __STDC_VERSION__ >= 199901L || _MSC_VER >= 1400
#  define vc_image_parmalloc(type, desc, ...)         vc_image_parmalloc_unwrapped(type, vc_image_debugstr(desc), __VA_ARGS__, VC_IMAGE_PROP_END)
#  define vc_image_configure(...)                     vc_image_configure_unwrapped(__VA_ARGS__, VC_IMAGE_PROP_END)
#  define vc_image_reconfigure(...)                   vc_image_reconfigure_unwrapped(__VA_ARGS__, VC_IMAGE_PROP_END)
#elif !defined(_MSC_VER) && !defined(VCMODS_LCC)
#  define vc_image_parmalloc(type, desc, arglist...)  vc_image_parmalloc_unwrapped(type, vc_image_debugstr(desc), arglist, VC_IMAGE_PROP_END)
#  define vc_image_configure(image, arglist...)       vc_image_configure_unwrapped(image, arglist, VC_IMAGE_PROP_END)
#  define vc_image_reconfigure(image, arglist...)     vc_image_reconfigure_unwrapped(image, arglist, VC_IMAGE_PROP_END)
#else
   /*#  warning "Your compiler does not support variadic macros.  You'll have no vc_image_configure() or vc_image_parmalloc() functions."*/
#endif

// now included in vc_image.c as openmaxil uses it regardless of which formats are built for
extern void vc_subsample(VC_IMAGE_BUF_T *sub,VC_IMAGE_BUF_T *cur);

#ifdef USE_YUV_UV

extern void vc_save_YUV( VC_IMAGE_BUF_T *frame,
                            const char                * const sz_filename);

extern void vc_save_YUV_1file( VC_IMAGE_BUF_T *frame, const char * const sz_filename, int framenum );

extern void vc_load_YUVUV( VC_IMAGE_BUF_T *frame,
                              const char                * const sz_filename,int yuvwidth,int yuvheight);

extern void vc_load_YUV( VC_IMAGE_BUF_T *frame,
                            const char                * const sz_filename,int framenum);

extern void vc_endianreverse32(uint32_t *addr,int count);

/*
** This is my preferred function to load a YUV_UV image.  It works with either a collection of images
** e.g. foo%03u.yuv or one giant concatenated file e.g. foo.yuv.
** It will tile or truncate the output image to what you want which is why there are two heights & widths.
** Return 0 if successful; -1 if the params are unsuitable, -2 if we couldn't open the file.
*/
extern int vc_load_image( const char         * const sz_file_fmt,          /* in     */
                          const unsigned     iim,                          /* in     */
                          const int          pitch,                        /* in     */
                          const int          i_height,                     /* in     */ /* (i)nput image: i.e. raw file */
                          const int          i_width,                      /* in     */
                          const int          o_height,                     /* in     */ /* (o)utput image: i.e. framebuffer */
                          const int          o_width,                      /* in     */
                          const VC_IMAGE_BUF_T * const frame               /* in     */ );

/* Modified version of vc_load_image (for maximum efficiency):
 * - must be supplied with a buffer to read a whole YUV frame into
 * - reads the next sequential image in an already-open file (no fopen/fseek/fclose).
 * - no tiling: output size = input size.
 */
void vc_slurp_image (FILE            * fid,                          /* in     */
                     const int          pitch,                       /* in     */
                     const int          height,                      /* in     */
                     const int          width,                       /* in     */
                     const VC_IMAGE_T * const frame,                 /* in     */
                     unsigned char    * frame_buf,                   /* in     */
                     int                rewind);                     /* in     */

extern int vc_load_image_vowifi( void * infile_fid,
                           const unsigned     iim,                          /* in     */
                           const int          pitch,                        /* in     */
                           const int          i_height,                     /* in     */ /* (i)nput image: i.e. raw file */
                           const int          i_width,                      /* in     */
                           const int          o_height,                     /* in     */ /* (o)utput image: i.e. framebuffer */
                           const int          o_width,                      /* in     */
                           const VC_IMAGE_BUF_T * const frame               /* in     */ ) ;

extern int vc_load_image_vowifi_brcm1d( void * infile_fid,
                           const unsigned     iim,                          /* in     */
                           const int          pitch,                        /* in     */
                           const int          i_height,                     /* in     */ /* (i)nput image: i.e. raw file */
                           const int          i_width,                      /* in     */
                           const int          o_height,                     /* in     */ /* (o)utput image: i.e. framebuffer */
                           const int          o_width,                      /* in     */
                           const VC_IMAGE_BUF_T * const frame,               /* in     */
                           const unsigned char *frame_buf) ;

/*
** Vector routine to difference a pair of images ( d |-|= a ).
** Should probably dcache flush after this if using via uncached alias?
*/
extern
void vc_image_difference( VC_IMAGE_BUF_T         * const d,          /* in/out */
                          const VC_IMAGE_BUF_T   * const a,          /* in     */
                          float                  * const psnr_y,     /*    out */
                          float                  * const psnr_uv,    /*    out */
                          unsigned               * const max_y,      /*    out */
                          unsigned               * const max_uv      /*    out */ );

/*
** The h.263 Annex J deblocker.
** ("dest" & "src" may point to the same storage.)
*/
extern
void vc_image_deblock_h263( VC_IMAGE_BUF_T       * const dest,       /*(in)out */
                            const VC_IMAGE_BUF_T * const src,        /* in     */
                            const int            QUANT               /* in     */ );

/*
 *  Clone the top field of an interlaced image (lines 0,2,4,..) over the bottom field (lines 1,3,5,...)
 *   or vice versa
*/
extern
void vc_image_clone_field(VC_IMAGE_T * image, int clone_top_field);

#endif

   /* RGBA32 colour macros */
#define RGBA_ALPHA_TRANSPARENT 0
#define RGBA_ALPHA_OPAQUE 255

#define RGBA32_TRANSPARENT_COLOUR 0x00000000
#ifdef RGB888
#define RGBA32_APIXEL(r, g, b, a) (((unsigned long)(a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#define RGBA32_SPIXEL(r, g, b, a) ((RGBA_ALPHA_OPAQUE << 24) | ((b) << 16) | ((g) << 8) | (r))
#else
#define RGBA32_APIXEL(r, g, b, a) (((unsigned long)(a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGBA32_SPIXEL(r, g, b, a) (((unsigned long)RGBA_ALPHA_OPAQUE << 24) | ((r) << 16) | ((g) << 8) | (b))
#endif
#define RGBA32_PIXEL(r, g, b, a) ((a) >= 255 ? RGBA32_SPIXEL(r, g, b, a) : RGBA32_APIXEL(r, g, b, a))
#define RGBA32_ALPHA(rgba) ((unsigned long)rgba >> 24)

   /* RGBA16 colour macros */
#define RGBA16_APIXEL(r, g, b, a) (((r) >> 4 << 12) | ((g) >> 4 << 8) | ((b) >> 4 << 4) | ((a) >> 4 << 0))
#define RGBA16_SPIXEL(r, g, b) (((r) >> 4 << 12) | ((g) >> 4 << 8) | ((b) >> 4 << 4) | 0x000F)
#define RGBA16_PIXEL(r, g, b, a) RGBA16_APIXEL(r, g, b, a)

   /* RGBA565 colour macros */
#define RGBA565_TRANSPARENTBITS  0x18DF
#define RGBA565_TRANSPARENTKEY   0xE700
#define RGBA565_ALPHABITS        0x001C

#define RGBA565_TRANSPARENT_COLOUR RGBA565_TRANSPARENTKEY

#define RGBA565_APIXEL(r, g, b, a) (((r) >> 6 << 11) | ((g) >> 6 << 6) | (((a)+16) >> 5 << 2) | ((b) >> 6) | RGBA565_TRANSPARENTKEY)
#define RGBA565_SPIXEL(r, g, b) (((r) >> 3 << 11) | ((g) >> 2 << 5) | ((b) >> 3) | (((r) & (g) & 0xE0) == 0xE0 ? 0x20 : 0x00))
#define RGBA565_PIXEL(r, g, b, a) ((a) >= 0xF0 ? RGBA565_SPIXEL(r, g, b) : RGBA565_APIXEL(r, g, b, a))

#define RGBA565_FROM_RGB565(rgb) ((((rgb) & ~RGBA565_TRANSPARENTBITS) == RGBA565_TRANSPARENTKEY) ? (rgb) ^ 0x0020 : (rgb))
#define RGBA565_TO_RGB565(rgba) ((((rgba) & ~RGBA565_TRANSPARENTBITS) == RGBA565_TRANSPARENTKEY) ? ((rgba) & (RGBA565_TRANSPARENTBITS ^ RGBA565_ALPHABITS)) * 10 : (rgba))
#define RGBA565_ALPHA(rgba) ((((rgba) & ~RGBA565_TRANSPARENTBITS) == RGBA565_TRANSPARENTKEY) ? ((rgba) & RGBA565_ALPHABITS) << 3 : 0x100)

#endif //#if !defined __SYMBIAN32__

#ifdef __cplusplus
}
#endif

#endif
