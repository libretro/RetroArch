/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Francisco Javier Trujillo Mata
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

#define GS_WHITE GS_SETREG_RGBAQ(0xFF,0xFF,0xFF,0x00,0x00) // turn white GS Screen

typedef struct ps2_video
{
   GSGLOBAL *gsGlobal;
   GSTEXTURE *menuTexture;
   GSTEXTURE *coreTexture;
   
   bool menuVisible;
   bool full_screen;

   bool rgb32;
} ps2_video_t;

// PRIVATE METHODS
static void initGSGlobal(ps2_video_t *ps2) {
	ps2->gsGlobal = gsKit_init_global()

	ps2->gsGlobal->PSM = GS_PSM_CT16;
	// ps2->gsGlobal->PSMZ = GS_PSMZ_16S;
	ps2->gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	ps2->gsGlobal->ZBuffering = GS_SETTING_OFF;
      ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_ON;    /* Enable alpha blending for primitives. */

	dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		    D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsKit_init_screen(ps2->gsGlobal);

	// gsKit_mode_switch(ps2->gsGlobal, GS_PERSISTENT);
      gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);

	gsKit_clear(ps2->gsGlobal, GS_WHITE);
}

static u32 textureSize(GSTEXTURE *texture) {
    return gsKit_texture_size(texture->Width, texture->Height, texture->PSM);
}

static size_t gskitTextureSize(GSTEXTURE *texture) {
   return gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
}

static void initTexture(GSTEXTURE *texture) {
   texture = malloc(sizeof *texture);
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

static void ConvertColors32(u32 *buffer, u32 dimensions)
{
    u32 i;
    u32 x32;
    for (i = 0; i < dimensions; i++) {

        x32 = buffer[i];
        buffer[i] = ((x32 >> 16) & 0xFF) | ((x32 << 16) & 0xFF0000) | (x32 & 0xFF00FF00);
    }
}


static void ConvertColors16(u16 *buffer, u32 dimensions)
{
    u32 i;
    u16 x16;
    for (i = 0; i < dimensions; i++) {

        x16 = buffer[i];
        buffer[i] = (x16 & 0x8000) | ((x16 << 10) & 0x7C00) | (x16 & 0x3E0) | ((x16 >> 10) & 0x1F);
    }
}

static void transfer_texture(GSTEXTURE *texture, const void *frame, 
      int width, int height, bool rgb32, bool color_correction) {
   
   int PSM = rgb32 ? GS_PSM_CT32 : GS_PSM_CT16;
   size_t size = gsKit_texture_size_ee(width, height, PSM);
   if (  !texture->Mem || 
      texture->Width != width || 
      texture->Height != height ||
      texture->PSM != PSM ) {
      texture->Width = width;
      texture->Height = height;
      texture->PSM = PSM;
      texture->Filter = GS_FILTER_NEAREST;
      free(texture->Mem);
      texture->Mem = memalign(128, size);
}

if (color_correction) {
   int pixels = width * height;
   int *buffer = (int *)frame;
   if (rgb32) {
      ConvertColors32(buffer, pixels);
   } else {
      ConvertColors16(buffer, pixels);
   }
}

   memcpy(texture->Mem, frame, size);
}


static void *ps2_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   void *ps2input = NULL;
   *input_data = NULL;
   (void)video;

   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   if (!ps2)
      return NULL;

   initGSGlobal(ps2);
   ps2->menuTexture = malloc(sizeof *ps2->menuTexture);
   ps2->coreTexture = malloc(sizeof *ps2->coreTexture);

   ps2->rgb32 = video->rgb32;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input = input_ps2.init(settings->arrays.input_joypad_driver);
      *input = ps2input ? &input_ps2 : NULL;
      *input_data  = ps2input;
   }

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
#ifdef DISPLAY_FPS
   uint32_t diff;
   static uint64_t currentTick,lastTick;
   static int frames;
   static float fps = 0.0;
#endif
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!width || !height)
      return false;


   gsKit_vram_clear(ps2->gsGlobal);

   if (frame) {
      transfer_texture(ps2->coreTexture, frame, width, height, ps2->rgb32, 1);
   }

   if (frame)
   {
      ps2->coreTexture->Vram=gsKit_vram_alloc(ps2->gsGlobal, textureSize(ps2->coreTexture) , GSKIT_ALLOC_USERBUFFER);
      if(ps2->coreTexture->Vram == GSKIT_ALLOC_ERROR)
      {
         printf("VRAM Allocation Failed. Will not upload texture.\n");
      }
   }

   if (ps2->menuVisible)
   {
      ps2->menuTexture->Vram=gsKit_vram_alloc(ps2->gsGlobal, textureSize(ps2->menuTexture) , GSKIT_ALLOC_USERBUFFER);
      if(ps2->menuTexture->Vram == GSKIT_ALLOC_ERROR)
      {
        printf("VRAM Allocation Failed. Will not upload texture.\n");
      }
   }

   if (frame)
   {
      gsKit_texture_upload(ps2->gsGlobal, ps2->coreTexture); 
   }

   if (ps2->menuVisible)
   {
      gsKit_texture_upload(ps2->gsGlobal, ps2->menuTexture);
   }

   if (frame)
   {
      gsKit_prim_sprite_texture( ps2->gsGlobal, ps2->coreTexture,
                              0.0f,
                              0.0f,  // Y1
					0.0f,  // U1
					0.0f,  // V1
					ps2->gsGlobal->Width, // X2
					ps2->gsGlobal->Height, // Y2
					ps2->coreTexture->Width, // U2
					ps2->coreTexture->Height, // V2
					2,
					GS_WHITE);
   }

   if (ps2->menuVisible)
   {
      gsKit_prim_sprite_texture( ps2->gsGlobal, ps2->menuTexture,
                              0.0f,
                              0.0f,  // Y1
					0.0f,  // U1
					0.0f,  // V1
					ps2->gsGlobal->Width, // X2
					ps2->gsGlobal->Height, // Y2
					ps2->menuTexture->Width, // U2
					ps2->menuTexture->Height, // V2
					2,
					GS_WHITE);
   }

   gsKit_sync_flip(ps2->gsGlobal);
   gsKit_queue_exec(ps2->gsGlobal);

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

   gsKit_clear(ps2->gsGlobal, GS_WHITE);

   deinitTexture(ps2->menuTexture);
   deinitTexture(ps2->coreTexture);
   gsKit_vram_clear(ps2->gsGlobal);
   gsKit_deinit_global(ps2->gsGlobal);

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

   transfer_texture(ps2->menuTexture, frame, width, height, rgb32, 0);
}

static void ps2_set_texture_enable(void *data, bool enable, bool full_screen)
{
   (void) full_screen;

   ps2_video_t *ps2 = (ps2_video_t*)data;
   ps2->menuVisible = enable;
   ps2->full_screen = full_screen;
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
