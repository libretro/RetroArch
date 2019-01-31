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

// Graphics library for VG

#ifndef GRAPHICS_X_PRIVATE_H
#define GRAPHICS_X_PRIVATE_H

#define VCOS_LOG_CATEGORY (&gx_log_cat)

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "VG/openvg.h"
#include "VG/vgu.h"

#include "vgfont.h"
#include "bcm_host.h"

extern VCOS_LOG_CAT_T gx_log_cat;

#define LOG_ERR( fmt, arg... )   vcos_log_error( "%s:%d " fmt, __func__, __LINE__, ##arg)

#define GX_ERROR(format, arg...) if (1) {} else printf( format "\n", ##arg)
#define GX_LOG(format, arg...) if (1) {} else printf( format "\n", ##arg)
#define GX_TRACE(format, arg...) if (1) {} else printf( format "\n", ##arg)

typedef struct
{
   EGL_DISPMANX_WINDOW_T egl_win;
} GX_NATIVE_WINDOW_T;

typedef enum
{
   GX_TOP_BOTTOM,
   GX_BOTTOM_TOP,
} GX_RASTER_ORDER_T;

typedef struct {} GX_PAINT_T;

typedef struct GX_CLIENT_STATE_T GX_CLIENT_STATE_T;
typedef struct {
   EGLDisplay disp;
} GX_DISPLAY_T;

struct GX_DISPLAY_T
{
   EGLDisplay disp;
};

typedef enum
{
   GX_WINDOW, GX_PIXMAP, GX_PBUFFER
} GX_RES_TYPE;

#define RES_MAGIC ('G'<<24|'X'<<16|'R'<<8|'S'<<0)
#define GX_PRIV_FLAG_FLIP (1<<0)

/**
 * Structure encapsulating the per-surface state.
 ***********************************************************/
typedef struct GRAPHICS_RESOURCE_HANDLE_TABLE_T
{
   union
   {
      GX_NATIVE_WINDOW_T native_window;
      VGImage pixmap;
   } u;
   GX_RES_TYPE type;

   uint32_t magic;         /** To work around broken create interface */
   int context_bound;
   const char *last_caller;
   EGLSurface surface;
   EGLContext context;
   EGLConfig config;
   uint32_t screen_id;     /** 0-LCD, etc */
   uint16_t width;
   uint16_t height;
   GRAPHICS_RESOURCE_TYPE_T restype;
   VC_DISPMAN_TRANSFORM_T transform;

   VC_RECT_T dest;         /** destination rectangle in use, for book-keeping */

   VGfloat alpha;
} GRAPHICS_RESOURCE_HANDLE_TABLE_T;

/**
 * Structure used to store an EGL client state. 
 ***********************************************************/
struct GX_CLIENT_STATE_T
{
   EGLSurface read_surface;
   EGLSurface draw_surface;
   EGLContext context;
   EGLenum api;
   GRAPHICS_RESOURCE_HANDLE res;
};

/**
 * \fixme add documentation
 *
 ***********************************************************/ 
void gx_priv_init(void);

/**
 * \fixme add documentation
 *
 ***********************************************************/ 
void gx_priv_destroy(void);

/**
 * \fixme add documentation
 *
 * @param col colour
 *
 * @param rgba OpenVG paint colour
 *
 ***********************************************************/ 
void gx_priv_colour_to_paint(uint32_t col, VGfloat *rgba);

/** 
 * Save current EGL client state.
 *
 * @param state upon return, holds the saved EGL client state.
 *
 * @param res handle to the surface the EGL client state belongs to (may be <code>NULL</code>).
 * 
 */
void gx_priv_save(GX_CLIENT_STATE_T *state, GRAPHICS_RESOURCE_HANDLE res);

/** 
 * Restore current EGL client state.
 *
 * @param state the EGL client state to restore.
 * 
 */
void gx_priv_restore(GX_CLIENT_STATE_T *state);

/** 
 * Create a native window for a surface.
 *
 * @param screen_id \fixme
 * 
 * @param w width of the window
 *
 * @param h height of the window
 *
 * @param type color/raster format of the resource
 *
 * @param win upon successful return, holds a handle to the native window
 *
 * @param cookie \fixme
 *
 * @return VCOS_SUCCESS on success, or error code.
 */
int gx_priv_create_native_window(uint32_t screen_id,
                                 uint32_t w, uint32_t h,
                                 GRAPHICS_RESOURCE_TYPE_T type,
                                 GX_NATIVE_WINDOW_T *win,
                                 void **cookie);

/** 
 * Destroy native window bound to surface.
 *
 * @param res Handle to surface.
 * 
 */
void gx_priv_destroy_native_window(GRAPHICS_RESOURCE_HANDLE_TABLE_T *res);

/** 
 * Initialise font from the given directory.
 *
 * @param font_dir path to font
 * 
 * \fixme only supports Vera.tff at the moment?
 *
 * @return VCOS_SUCCESS on success, or error code.
 */
VCOS_STATUS_T gx_priv_font_init(const char *font_dir);

/**
 * \fixme add documentation
 *
 ***********************************************************/ 
void gx_priv_font_term(void);

/**
 * Fill an area of a surface with a single colour.
 *
 * @param res Handle to surface.
 *
 * @param x x-offset of area to fill
 * 
 * @param y y-offset of area to fill
 *
 * @param width width of area to fill
 *
 * @param height height of area to fill
 *
 * @param fill_colour fill colour
 *
 ***********************************************************/
VCOS_STATUS_T gx_priv_resource_fill(GRAPHICS_RESOURCE_HANDLE res,
                               uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height,
                               uint32_t fill_colour );

/**
 * Render text into a surface
 *
 * @param disp Handle to display.
 *
 * @param res Handle to surface.
 *
 * @param x x-offset
 *
 * @param y y-offset
 *
 * @param width bounding rectangle width
 *
 * @param height bounding rectangle height
 *
 * @param fg_colour foreground color
 *
 * @param bg_colour background color
 *
 * @param text text to render
 *
 * @param text_length length of text
 *
 * @param text_size size of text
 *
 ***********************************************************/
VCOS_STATUS_T gx_priv_render_text( GX_DISPLAY_T *disp,
                                   GRAPHICS_RESOURCE_HANDLE res,
                                   int32_t x,
                                   int32_t y,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t fg_colour,
                                   uint32_t bg_colour,
                                   const char *text,
                                   uint32_t text_length,
                                   uint32_t text_size );

/**
 * Flush a surface.
 *
 * @param res Handle to surface.
 *
 ***********************************************************/ 
void gx_priv_flush(GRAPHICS_RESOURCE_HANDLE res);

/**
 * Called after the EGL/VG initialisation of a window has completed
 * following its creation.
 *
 * @param res ???
 *
 * @param cookie ???
 *
 ***********************************************************/ 
void gx_priv_finish_native_window(GRAPHICS_RESOURCE_HANDLE_TABLE_T *res,
                                  void *cookie);

/**
 * Flush font cache.
 *
 ***********************************************************/ 
void gx_font_cache_flush(void);

/**
 * Read a bitmap (.BMP) image from the given file. 
 *  
 * @param filename filename (must not be <code>NULL</code>).
 *
 * @param width holds the width of the image upon return.
 *
 * @param height holds the height of the image upon return.
 *
 * @param pitch_bytes holds the pitch of the image data (in bytes) upon return.
 *
 * @param restype holds the type of the image upon return.
 *
 * @param vg_format holds the OpenVG image format upon return.
 *
 * @param flags holds flags describing image properties upon return.
 *
 * @param image_data_size holds size of the image data upon return.
 * 
 * @param pimage_data holds the image data buffer upon return (must not be <code>NULL</code>),
 *                    the caller is responsible for releasing the buffer afterwards.
 *
 * @return 0 if success, non-zero otherwise (in which case the output parameters
 *           may be invalid).
 *
 ***********************************************************/ 
int gx_priv_read_bmp(const char *file_name, 
                     uint32_t *width, uint32_t *height, uint32_t *pitch_bytes,
                     GRAPHICS_RESOURCE_TYPE_T *restype,
                     VGImageFormat *vg_format,
                     uint32_t *flags,
                     uint32_t *image_data_size,
                     void **pimage_data);

/**
 * Read a Targa (.TGA) image from the given file. 
 *  
 * @param filename filename (must not be <code>NULL</code>).
 *
 * @param width holds the width of the image upon return.
 *
 * @param height holds the height of the image upon return.
 *
 * @param pitch_bytes holds the pitch of the image data (in bytes) upon return.
 *
 * @param restype holds the type of the image upon return.
 *
 * @param vg_format holds the OpenVG image format upon return.
 *
 * @param flags holds flags describing image properties upon return.
 *
 * @param image_data_size holds size of the image data upon return.
 * 
 * @param pimage_data holds the image data buffer upon return (must not be <code>NULL</code>),
 *                    the caller is responsible for releasing the memory afterwards.
 *
 * @return 0 if success, non-zero otherwise (in which case the output parameters.
 *           may be invalid).
 *
 ***********************************************************/ 
int gx_priv_read_tga(const char *file_name, 
                     uint32_t *width, uint32_t *height, uint32_t *pitch_bytes,
                     GRAPHICS_RESOURCE_TYPE_T *restype,
                     VGImageFormat *vg_format,
                     uint32_t *flags,
                     uint32_t *image_data_size,
                     void **pimage_data);

#endif
