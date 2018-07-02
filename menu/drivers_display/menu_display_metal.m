//
//  menu_display_metal.m
//  RetroArch_Metal
//
//  Created by Stuart Carnie on 5/25/18.
//

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"

#include "../../gfx/font_driver.h"
#include "../../gfx/video_driver.h"
#import "../../gfx/common/metal_common.h"

static void *menu_display_metal_get_default_mvp(video_frame_info_t *video_info)
{
   return NULL;
}

static void menu_display_metal_blend_begin(video_frame_info_t *video_info)
{
}

static void menu_display_metal_blend_end(video_frame_info_t *video_info)
{
}

static void menu_display_metal_draw(menu_display_ctx_draw_t *draw,
                                   video_frame_info_t *video_info)
{
}

static void menu_display_metal_draw_pipeline(
                                            menu_display_ctx_draw_t *draw, video_frame_info_t *video_info)
{
}

static void menu_display_metal_viewport(menu_display_ctx_draw_t *draw,
                                       video_frame_info_t *video_info)
{
}

static void menu_display_metal_restore_clear_color(void)
{
}

static void menu_display_metal_clear_color(
                                          menu_display_ctx_clearcolor_t *clearcolor,
                                          video_frame_info_t *video_info)
{
   (void)clearcolor;
}

static bool menu_display_metal_font_init_first(
                                              void **font_handle, void *video_data,
                                              const char *font_path, float font_size,
                                              bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
                                    font_path, font_size, true,
                                    is_threaded,
                                    FONT_DRIVER_RENDER_METAL_API);

   if (*handle)
      return true;

   return false;
}

static const float *menu_display_metal_get_default_vertices(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static const float *menu_display_metal_get_default_tex_coords(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

menu_display_ctx_driver_t menu_display_ctx_metal = {
   menu_display_metal_draw,
   menu_display_metal_draw_pipeline,
   menu_display_metal_viewport,
   menu_display_metal_blend_begin,
   menu_display_metal_blend_end,
   menu_display_metal_restore_clear_color,
   menu_display_metal_clear_color,
   menu_display_metal_get_default_mvp,
   menu_display_metal_get_default_vertices,
   menu_display_metal_get_default_tex_coords,
   menu_display_metal_font_init_first,
   MENU_VIDEO_DRIVER_GENERIC,
   "menu_display_metal",
   false
};

