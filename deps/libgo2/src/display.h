#pragma once

/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdint.h>
#include <stdbool.h>


typedef struct go2_display go2_display_t;
typedef struct go2_surface go2_surface_t;
typedef struct go2_frame_buffer go2_frame_buffer_t;
typedef struct go2_presenter go2_presenter_t;

typedef enum go2_rotation
{
    GO2_ROTATION_DEGREES_0 = 0,
    GO2_ROTATION_DEGREES_90,
    GO2_ROTATION_DEGREES_180,
    GO2_ROTATION_DEGREES_270
} go2_rotation_t;

typedef struct go2_context_attributes
{
    int major;
    int minor;
    int red_bits;
    int green_bits;
    int blue_bits;
    int alpha_bits;
    int depth_bits;
    int stencil_bits;
} go2_context_attributes_t;

typedef struct go2_context go2_context_t;


#ifdef __cplusplus
extern "C" {
#endif

go2_display_t* go2_display_create();
void go2_display_destroy(go2_display_t* display);
int go2_display_width_get(go2_display_t* display);
int go2_display_height_get(go2_display_t* display);
void go2_display_present(go2_display_t* display, go2_frame_buffer_t* frame_buffer);
uint32_t go2_display_backlight_get(go2_display_t* display);
void go2_display_backlight_set(go2_display_t* display, uint32_t value);


int go2_drm_format_get_bpp(uint32_t format);


go2_surface_t* go2_surface_create(go2_display_t* display, int width, int height, uint32_t format);
void go2_surface_destroy(go2_surface_t* surface);
int go2_surface_width_get(go2_surface_t* surface);
int go2_surface_height_get(go2_surface_t* surface);
uint32_t go2_surface_format_get(go2_surface_t* surface);
int go2_surface_stride_get(go2_surface_t* surface);
go2_display_t* go2_surface_display_get(go2_surface_t* surface);
int go2_surface_prime_fd(go2_surface_t* surface);
void* go2_surface_map(go2_surface_t* surface);
void go2_surface_unmap(go2_surface_t* surface);
void go2_surface_blit(go2_surface_t* srcSurface, int srcX, int srcY, int srcWidth, int srcHeight,
                      go2_surface_t* dstSurface, int dstX, int dstY, int dstWidth, int dstHeight,
                      go2_rotation_t rotation, int scale_mode);
int go2_surface_save_as_png(go2_surface_t* surface, const char* filename);


go2_frame_buffer_t* go2_frame_buffer_create(go2_surface_t* surface);
void go2_frame_buffer_destroy(go2_frame_buffer_t* frame_buffer);
go2_surface_t* go2_frame_buffer_surface_get(go2_frame_buffer_t* frame_buffer);


go2_presenter_t* go2_presenter_create(go2_display_t* display, uint32_t format, uint32_t background_color, bool managed);
void go2_presenter_destroy(go2_presenter_t* presenter);
void go2_presenter_post(go2_presenter_t* presenter, go2_surface_t* surface, int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, go2_rotation_t rotation, int scale_mode);
go2_display_t* go2_presenter_display_get(go2_presenter_t* presenter);


go2_context_t* go2_context_create(go2_display_t* display, int width, int height, const go2_context_attributes_t* attributes);
void go2_context_destroy(go2_context_t* context);
void* go2_context_egldisplay_get(go2_context_t* context);
void go2_context_make_current(go2_context_t* context);
void go2_context_swap_buffers(go2_context_t* context);
go2_surface_t* go2_context_surface_lock(go2_context_t* context);
void go2_context_surface_unlock(go2_context_t* context, go2_surface_t* surface);

go2_frame_buffer_t* go2_frame_buffer_create(go2_surface_t* surface);

#ifdef __cplusplus
}
#endif
