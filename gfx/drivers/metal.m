/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_assert.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#import "../../config.h"
#endif

#ifdef HAVE_MENU
#import "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#import "../../menu/widgets/menu_widgets.h"
#endif
#endif

#import "../font_driver.h"

#import "../common/metal_common.h"

#import "../../driver.h"
#import "../../configuration.h"

#import "../../retroarch.h"
#import "../../verbosity.h"

#import "../video_coord_array.h"

/* Temporary workaround for metal not being able to poll flags during init */
static gfx_ctx_driver_t metal_fake_context;
static uint32_t metal_get_flags(void *data);

static bool metal_set_shader(void *data,
                             enum rarch_shader_type type, const char *path);

static void *metal_init(
      const video_info_t *video,
      input_driver_t **input,
      void **input_data)
{
   [apple_platform setViewType:APPLE_VIEW_TYPE_METAL];

   MetalDriver *md = [[MetalDriver alloc] initWithVideo:video input:input inputData:input_data];
   if (md == nil)
   {
      return NULL;
   }

   {
      const char *shader_path;
      enum rarch_shader_type type;

      metal_fake_context.get_flags = metal_get_flags;
      video_context_driver_set(&metal_fake_context);

      shader_path = retroarch_get_shader_preset();
      type = video_shader_parse_type(shader_path);
      metal_set_shader((__bridge void *)md, type, shader_path);
   }

   return (__bridge_retained void *)md;
}

static bool metal_frame(void *data, const void *frame,
                        unsigned frame_width, unsigned frame_height,
                        uint64_t frame_count,
                        unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md renderFrame:frame
                    width:frame_width
                   height:frame_height
               frameCount:frame_count
                    pitch:pitch
                      msg:msg
                     info:video_info];
}

static void metal_set_nonblock_state(void *data, bool non_block)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   md.context.displaySyncEnabled = !non_block;
}

static bool metal_alive(void *data)
{
   return true;
}

static bool metal_has_windowed(void *data)
{
   return true;
}

static bool metal_focus(void *data)
{
   return apple_platform.hasFocus;
}

static bool metal_suppress_screensaver(void *data, bool disable)
{
   RARCH_LOG("[Metal]: suppress screen saver: %s\n", disable ? "YES" : "NO");
   return [apple_platform setDisableDisplaySleep:disable];
}

static bool metal_set_shader(void *data,
                             enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   MetalDriver *md = (__bridge MetalDriver *)data;

   if (!md)
      return false;

   if (type != RARCH_SHADER_SLANG)
   {
      if (!string_is_empty(path) && type != RARCH_SHADER_SLANG)
         RARCH_WARN("[Metal] Only Slang shaders are supported. Falling back to stock.\n");
      path = NULL;
   }

   /* TODO actually return to stock */
   if (string_is_empty(path))
      return true;

   return [md.frameView setShaderFromPath:[NSString stringWithUTF8String:path]];
#else
   return false;
#endif
}

static void metal_free(void *data)
{
   MetalDriver *md = (__bridge_transfer MetalDriver *)data;
   md = nil;
}

static void metal_set_viewport(void *data, unsigned viewport_width,
                               unsigned viewport_height, bool force_full, bool allow_rotate)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md == nil) {
      return;
   }

   [md setViewportWidth:viewport_width height:viewport_height forceFull:force_full allowRotate:allow_rotate];
}

static void metal_set_rotation(void *data, unsigned rotation)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md == nil) {
      return;
   }

   [md setRotation:rotation];
}

static void metal_viewport_info(void *data, struct video_viewport *vp)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   *vp = *md.viewport;
}

static bool metal_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md.frameView readViewport:buffer isIdle:is_idle];
}

static uintptr_t metal_load_texture(void *video_data, void *data,
                                    bool threaded, enum texture_filter_type filter_type)
{
   MetalDriver *md = (__bridge MetalDriver *)video_data;
   struct texture_image *img = (struct texture_image *)data;
   if (!img)
      return 0;

   struct texture_image image = *img;
   Texture *t = [md.context newTexture:image filter:filter_type];
   return (uintptr_t)(__bridge_retained void *)(t);
}

static void metal_unload_texture(void *data, uintptr_t handle)
{
   if (!handle)
      return;
   Texture *t = (__bridge_transfer Texture *)(void *)handle;
   t = nil;
}

static void metal_set_video_mode(void *data,
                                 unsigned width, unsigned height,
                                 bool fullscreen)
{
   RARCH_LOG("[Metal]: set_video_mode res=%dx%d fullscreen=%s\n",
             width, height,
             fullscreen ? "YES" : "NO");
}

static float metal_get_refresh_rate(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   (void)md;

   return 0.0f;
}

static void metal_set_filtering(void *data, unsigned index, bool smooth)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md.frameView setFilteringIndex:index smooth:smooth];
}

static void metal_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   MetalDriver *md = (__bridge MetalDriver *)data;

   md.keepAspect = YES;
   [md setNeedsResize];
}

static void metal_apply_state_changes(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md setNeedsResize];
}

static void metal_set_texture_frame(void *data, const void *frame,
                                    bool rgb32, unsigned width, unsigned height,
                                    float alpha)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   settings_t *settings = config_get_ptr();

   [md.menu updateWidth:width
                 height:height
                 format:rgb32 ? RPixelFormatBGRA8Unorm : RPixelFormatBGRA4Unorm
                 filter:settings->bools.menu_linear_filter ? RTextureFilterLinear : RTextureFilterNearest];
   [md.menu updateFrame:frame];
   md.menu.alpha = alpha;
}

static void metal_set_texture_enable(void *data, bool state, bool full_screen)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   md.menu.enabled = state;
#if 0
   md.menu.fullScreen = full_screen;
#endif
}

static void metal_show_mouse(void *data, bool state)
{
   [apple_platform setCursorVisible:state];
}

static struct video_shader *metal_get_current_shader(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NULL;

   return md.frameView.shader;
}

static uint32_t metal_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static const video_poke_interface_t metal_poke_interface = {
   .get_flags           = metal_get_flags,
   .load_texture        = metal_load_texture,
   .unload_texture      = metal_unload_texture,
   .set_video_mode      = metal_set_video_mode,
   .get_refresh_rate    = metal_get_refresh_rate,
   .set_filtering       = metal_set_filtering,
   .set_aspect_ratio    = metal_set_aspect_ratio,
   .apply_state_changes = metal_apply_state_changes,
   .set_texture_frame   = metal_set_texture_frame,
   .set_texture_enable  = metal_set_texture_enable,
   .set_osd_msg         = font_driver_render_msg,
   .show_mouse          = metal_show_mouse,
   .get_current_shader  = metal_get_current_shader,
};

static void metal_get_poke_interface(void *data,
                                     const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &metal_poke_interface;
}

#ifdef HAVE_OVERLAY

static void metal_overlay_enable(void *data, bool state)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   md.overlay.enabled = state;
}

static bool metal_overlay_load(void *data,
                               const void *images, unsigned num_images)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NO;

   return [md.overlay loadImages:(const struct texture_image *)images count:num_images];
}

static void metal_overlay_tex_geom(void *data, unsigned index,
                                   float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   [md.overlay updateTextureCoordsX:x y:y w:w h:h index:index];
}

static void metal_overlay_vertex_geom(void *data, unsigned index,
                                      float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   [md.overlay updateVertexX:x y:y w:w h:h index:index];
}

static void metal_overlay_full_screen(void *data, bool enable)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   md.overlay.fullscreen = enable;
}

static void metal_overlay_set_alpha(void *data, unsigned index, float mod)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   [md.overlay updateAlpha:mod index:index];
}

static const video_overlay_interface_t metal_overlay_interface = {
   .enable        = metal_overlay_enable,
   .load          = metal_overlay_load,
   .tex_geom      = metal_overlay_tex_geom,
   .vertex_geom   = metal_overlay_vertex_geom,
   .full_screen   = metal_overlay_full_screen,
   .set_alpha     = metal_overlay_set_alpha,
};

static void metal_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &metal_overlay_interface;
}

#endif

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool metal_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_metal = {
   metal_init,
   metal_frame,
   metal_set_nonblock_state,
   metal_alive,
   metal_focus,
   metal_suppress_screensaver,
   metal_has_windowed,
   metal_set_shader,
   metal_free,
   "metal",
   metal_set_viewport,
   metal_set_rotation,
   metal_viewport_info,
   metal_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   metal_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   metal_get_poke_interface,
   NULL, /* metal_wrap_type_to_enum */
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   metal_menu_widgets_enabled
#endif
};
