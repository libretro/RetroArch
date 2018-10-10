/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../video_driver.h"

#include "../../driver.h"
#include "../../verbosity.h"

#include <loadcore.h>
#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#define GS_SCREEN_WIDTH 640;
#define GS_SCREEN_HEIGHT 448;

#define GS_BLACK GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00) // turn black GS Screen

typedef struct psp1_menu_frame
{
   void* frame;
   int width;
   int height;
   bool rgb32;

} ps2_menu_frame_t;

typedef struct ps2_video
{
   GSGLOBAL *gsGlobal;
   GSTEXTURE *backgroundTexture;

   ps2_menu_frame_t menu;

} ps2_video_t;

// PRIVATE METHODS
static void initGSGlobal(ps2_video_t *ps2) {
   /* Initilize DMAKit */
   dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   /* Initialize the DMAC */
   dmaKit_chan_init(DMA_CHANNEL_GIF);

   // /* Initilize the GS */
   if(ps2->gsGlobal!=NULL) {
      gsKit_deinit_global(ps2->gsGlobal);
   } 
   ps2->gsGlobal=gsKit_init_global();

   ps2->gsGlobal->DoubleBuffering = GS_SETTING_OFF;    /* Disable double buffering to get rid of the "Out of VRAM" error */
   ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_ON;    /* Enable alpha blending for primitives. */
   ps2->gsGlobal->ZBuffering = GS_SETTING_OFF;
   ps2->gsGlobal->PSM=GS_PSM_CT16;


   ps2->gsGlobal->Interlace = GS_INTERLACED;
   ps2->gsGlobal->Mode = GS_MODE_NTSC;
   ps2->gsGlobal->Field = GS_FIELD;
   ps2->gsGlobal->Width = GS_SCREEN_WIDTH;
   ps2->gsGlobal->Height = GS_SCREEN_HEIGHT;

   gsKit_init_screen(ps2->gsGlobal);    /* Apply settings. */
   gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);
}

static u32 textureSize(GSTEXTURE *texture) {
    return gsKit_texture_size(texture->Width, texture->Height, texture->PSM);
}

static size_t gskitTextureSize(GSTEXTURE *texture) {
   return gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
}

static void *textureEndPTR(GSTEXTURE *texture) {
    return ((void *)((unsigned int)texture->Mem+textureSize(texture)));
}

static void prepareTexture(GSTEXTURE *texture, int delayed) {
   texture->Width = GS_SCREEN_WIDTH;
   texture->Height = GS_SCREEN_HEIGHT;
   texture->PSM = GS_PSM_CT16;
   if (delayed) {
      texture->Delayed = GS_SETTING_ON;
   }
   texture->Filter = GS_FILTER_NEAREST;
   free(texture->Mem);
   texture->Mem = memalign(128, gskitTextureSize(texture));
   gsKit_setup_tbw(texture);
}

static void initBackgroundTexture(ps2_video_t *ps2) {
   ps2->backgroundTexture = malloc(sizeof *ps2->backgroundTexture);
   prepareTexture(ps2->backgroundTexture, 1);
}

static void deinitTexturePTR(void *texture_ptr) {
   if(texture_ptr!=NULL){
      free(texture_ptr);
      texture_ptr=NULL;
   }
}

static void deinitTexture(GSTEXTURE *texture) {
   deinitTexturePTR(texture->Mem);
   deinitTexturePTR(texture->Clut);
}

static void syncGSGlobalChache(GSGLOBAL *gsGlobal) {
     gsKit_queue_exec(gsGlobal);
}

static void syncTextureChache(GSTEXTURE *texture, GSGLOBAL *gsGlobal) {
    SyncDCache(texture->Mem, textureEndPTR(texture));
    gsKit_texture_send_inline(gsGlobal, texture->Mem, texture->Width, texture->Height, texture->Vram, texture->PSM, texture->TBW, GS_CLUT_NONE);
}

static void syncBackgroundChache(ps2_video_t *ps2) {
    // We need to create the VRAM just in this state I dont know why...
//     ps2->backgroundTexture->Vram=gsKit_vram_alloc(ps2->gsGlobal, textureSize(ps2->backgroundTexture) , GSKIT_ALLOC_USERBUFFER);
    syncTextureChache(ps2->backgroundTexture, ps2->gsGlobal);
}







static void *ps2_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   initGSGlobal(ps2);
   initBackgroundTexture(ps2);

   memset(ps2->backgroundTexture->Mem, 255, gskitTextureSize(ps2->backgroundTexture));

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   (void)frame;
   (void)pitch;
   (void)msg;

#ifdef DISPLAY_FPS
   uint32_t diff;
   static uint64_t currentTick,lastTick;
   static int frames;
   static float fps = 0.0;
#endif
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!width || !height)
      return false;

   printf("Size backgroundTexture:%i, size menu:%i\n", gskitTextureSize(ps2->backgroundTexture), ps2->menu.width * ps2->menu.height * (ps2->menu.rgb32 ? 4 : 2));
   uint8_t *src = (uint8_t *)ps2->menu.frame;
   uint8_t *dst = (uint8_t *)ps2->backgroundTexture->Mem;
   int src_stride = ps2->menu.width * (ps2->menu.rgb32 ? 4 : 2);
   int dst_stride = ps2->backgroundTexture->Width * (ps2->menu.rgb32 ? 4 : 2);
   while(src < (uint8_t *)ps2->menu.frame + ps2->menu.height * src_stride)
   {
      memcpy(dst, src, src_stride);
      src += src_stride;
      dst += dst_stride;
   }

   syncBackgroundChache(ps2);
   syncGSGlobalChache(ps2->gsGlobal);

   dmaKit_wait_fast();

   return true;
}

static void ps2_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool ps2_gfx_alive(void *data)
{
   (void)data;
   return true;
}

static bool ps2_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool ps2_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool ps2_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void ps2_gfx_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   deinitTexture(ps2->backgroundTexture);
   gsKit_deinit_global(ps2->gsGlobal);
   
   free(ps2->menu.frame);

   free(data);
}

static bool ps2_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void ps2_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void ps2_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool ps2_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void ps2_set_filtering(void *data, unsigned index, bool smooth)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;
}

static void ps2_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;
}

static void ps2_apply_state_changes(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   (void) rgb32;
   (void) alpha;

#ifdef DEBUG
   /* ps2->menu.frame buffer size is (640 * 448)*2 Bytes */
   retro_assert((width*height) < (480 * 448));
#endif

   printf("ps2_set_texture_frame, width:%i, height:%i, rgb32:%i, alpha:%f\n", width, height, rgb32, alpha);
   if (     !ps2->menu.frame || 
            ps2->menu.width != width || 
            ps2->menu.height != height ||
            ps2->menu.rgb32 != rgb32 ) {
      free(ps2->menu.frame);
      ps2->menu.frame = malloc(width * height * (rgb32 ? 4 : 2));
      ps2->menu.width = width;
      ps2->menu.height = height;
      ps2->menu.rgb32 = rgb32;
   }

   memcpy(ps2->menu.frame, frame, width * height * (rgb32 ? 4 : 2));

}

static void ps2_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void) full_screen;

   ps2_video_t *ps2 = (ps2_video_t*)data;
}

static const video_poke_interface_t ps2_poke_interface = {
   NULL,          /* get_flags  */
   NULL,          /* set_coords */
   NULL,          /* set_mvp */
   NULL,
   NULL,
   NULL,
   NULL, /* get_refresh_rate */
   ps2_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   ps2_set_aspect_ratio,
   ps2_apply_state_changes,
   ps2_set_texture_frame,
   ps2_set_texture_enable,
   NULL,                        /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   NULL                         /* get_hw_render_interface */
};


static void ps2_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &ps2_poke_interface;
}

video_driver_t video_ps2 = {
   ps2_gfx_init,
   ps2_gfx_frame,
   ps2_gfx_set_nonblock_state,
   ps2_gfx_alive,
   ps2_gfx_focus,
   ps2_gfx_suppress_screensaver,
   ps2_gfx_has_windowed,
   ps2_gfx_set_shader,
   ps2_gfx_free,
   "ps2",
   NULL, /* set_viewport */
   ps2_gfx_set_rotation,
   ps2_gfx_viewport_info,
   ps2_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  ps2_gfx_get_poke_interface,
};
