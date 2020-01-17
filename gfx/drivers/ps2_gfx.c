/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../../libretro-common/include/libretro_gskit_ps2.h"

#define GS_TEXT GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00) // turn white GS Screen
#define GS_BLACK GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00) // turn white GS Screen

#define NTSC_WIDTH 640
#define NTSC_HEIGHT 448

typedef struct ps2_video
{
   GSGLOBAL *gsGlobal;
   GSTEXTURE *menuTexture;
   GSTEXTURE *coreTexture;
   bool clearVRAM;
   bool clearVRAM_font; /* I need to create this additional field to be used in the font driver*/
   struct retro_hw_render_interface_gskit_ps2 iface; /* Palette in the cores */

   bool menuVisible;
   bool fullscreen;

   bool vsync;

   int PSM;
   bool force_aspect;
   int menu_filter;
   int core_filter;
} ps2_video_t;

// PRIVATE METHODS
static GSGLOBAL *init_GSGlobal(void)
{
   GSGLOBAL *gsGlobal = gsKit_init_global();

   gsGlobal->Mode = GS_MODE_NTSC;
   gsGlobal->Interlace = GS_INTERLACED;
   gsGlobal->Field = GS_FIELD;
   gsGlobal->Width = NTSC_WIDTH;
   gsGlobal->Height = NTSC_HEIGHT;

   gsGlobal->PSM = GS_PSM_CT16;
   gsGlobal->PSMZ = GS_PSMZ_16;
   gsGlobal->DoubleBuffering = GS_SETTING_OFF;
   gsGlobal->ZBuffering = GS_SETTING_OFF;
   gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

   dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
               D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   /* Initialize the DMAC */
   dmaKit_chan_init(DMA_CHANNEL_GIF);

   gsKit_init_screen(gsGlobal);
   gsKit_mode_switch(gsGlobal, GS_ONESHOT);
   gsKit_clear(gsGlobal, GS_BLACK);

   return gsGlobal;
}

static GSTEXTURE * prepare_new_texture(void)
{
   GSTEXTURE *texture = calloc(1, sizeof(*texture));
   return texture;
}

static void init_ps2_video(ps2_video_t *ps2)
{
   ps2->gsGlobal = init_GSGlobal();
   ps2->menuTexture = prepare_new_texture();
   ps2->coreTexture = prepare_new_texture();

   /* Used for cores that supports palette */
   ps2->iface.interface_type = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2;
   ps2->iface.interface_version = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2_VERSION;
   ps2->iface.coreTexture = ps2->coreTexture;
}

static void deinitTexture(GSTEXTURE *texture)
{
   texture->Mem = NULL;
   texture->Clut = NULL;
}

static bool texture_need_prepare(GSTEXTURE *texture, int width, int height, int PSM)
{
   return texture->Width != width || texture->Height != height || texture->PSM != PSM;
}

static void transfer_texture(GSTEXTURE *texture, const void *frame,
      int width, int height, int PSM, int filter, bool color_correction)
{
   texture->Width = width;
   texture->Height = height;
   texture->PSM = PSM;
   texture->Filter = filter;
   texture->Mem = (void *)frame;
}

static void vram_alloc(GSGLOBAL *gsGlobal, GSTEXTURE *texture)
{
   uint32_t size = gsKit_texture_size(texture->Width, texture->Height, texture->PSM);
   texture->Vram = gsKit_vram_alloc(gsGlobal, size, GSKIT_ALLOC_USERBUFFER);
   if(texture->Vram == GSKIT_ALLOC_ERROR) {
      printf("VRAM Allocation Failed. Will not upload texture.\n");
   }

   if (texture->Clut) {
      /* Right now just supporting 16 x 16 = 256 colours */
      size = gsKit_texture_size(16, 16, texture->ClutPSM);
      texture->VramClut = gsKit_vram_alloc(gsGlobal, size , GSKIT_ALLOC_USERBUFFER);
   }
}

static void prim_texture(GSGLOBAL *gsGlobal, GSTEXTURE *texture, int zPosition, bool force_aspect, struct retro_hw_ps2_insets padding)
{
      float x1, y1, x2, y2;
      float visible_width =  texture->Width - padding.left - padding.right;
      float visible_height =  texture->Height - padding.top - padding.bottom;
   if (force_aspect) {
      float width_proportion = (float)gsGlobal->Width / (float)visible_width;
      float height_proportion = (float)gsGlobal->Height / (float)visible_height;
      float delta = MIN(width_proportion, height_proportion);
      float newWidth = visible_width * delta;
      float newHeight = visible_height * delta;

      x1 = (gsGlobal->Width - newWidth) / 2.0f;
      y1 = (gsGlobal->Height - newHeight) / 2.0f;
      x2 = newWidth + x1;
      y2 = newHeight + y1;

   } else {
      x1 = 0.0f;
      y1 = 0.0f;
      x2 = gsGlobal->Width;
      y2 = gsGlobal->Height;
   }

   gsKit_prim_sprite_texture( gsGlobal, texture,
                              x1, //X1
                              y1,  // Y1
                              padding.left,  // U1
                              padding.top,  // V1
                              x2, // X2
                              y2, // Y2
                              texture->Width - padding.right, // U2
                              texture->Height - padding.bottom, // V2
                              zPosition,
                              GS_TEXT);
}

static void clearVRAMIfNeeded(ps2_video_t *ps2, const void *frame, int width, int height)
{
   if (!ps2->clearVRAM) {
      if(frame && frame != RETRO_HW_FRAME_BUFFER_VALID) {
         bool coreVRAMClear = false;
         coreVRAMClear = texture_need_prepare(ps2->coreTexture, width, height, ps2->PSM);
         ps2->clearVRAM = ps2->clearVRAM || coreVRAMClear;
      }
   }

   if (ps2->clearVRAM) {
      gsKit_vram_clear(ps2->gsGlobal);
      ps2->iface.updatedPalette = true;
      ps2->clearVRAM_font = true; /* we need to upload also palette in the font driver */
   }
}

static void refreshScreen(ps2_video_t *ps2)
{
   if (ps2->vsync) {
      gsKit_sync_flip(ps2->gsGlobal);
   }
   gsKit_queue_exec(ps2->gsGlobal);

   /* Here we are just puting in false the ps2->clearVRAM field
      however, the ps2->clearVRAM_font should be done in the ps2_font driver */
   ps2->clearVRAM = false;
}

static void ps2_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *Texture, bool sendPalette)
{
   gsKit_setup_tbw(Texture);

   if (Texture->PSM == GS_PSM_T8) {
      gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
      if (sendPalette) {
         gsKit_texture_send(Texture->Clut, 16, 16, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
      }

   } else if (Texture->PSM == GS_PSM_T4) {
      gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
      if (sendPalette) {
         gsKit_texture_send(Texture->Clut, 8,  2, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
      }
   } else {
      gsKit_texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE);
   }
}

static void *ps2_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *ps2input = NULL;
   *input_data = NULL;
   (void)video;

   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   if (!ps2)
      return NULL;

   init_ps2_video(ps2);
   if (video->font_enable) {
      font_driver_init_osd(ps2, false, video->is_threaded, FONT_DRIVER_RENDER_PS2);
   }
   ps2->PSM = (video->rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   ps2->fullscreen = video->fullscreen;
   ps2->core_filter = video->smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
   ps2->force_aspect = video->force_aspect;
   ps2->vsync = video->vsync;
   ps2->clearVRAM = true;

   if (input && input_data) {
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
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!width || !height)
      return false;

#if defined(DEBUG)
   if (frame_count%60==0) {
      printf("ps2_gfx_frame %lu\n", frame_count);
   }
#endif

   clearVRAMIfNeeded(ps2, frame, width, height);
   gsKit_clear(ps2->gsGlobal, GS_BLACK);

   if (frame) {
      bool sendPalette = false;
      struct retro_hw_ps2_insets padding = empty_ps2_insets;
      if (frame != RETRO_HW_FRAME_BUFFER_VALID){ /* Checking if the transfer is done in the core */
         /* calculate proper width based in the pitch */
         int bytes_per_pixel = (ps2->PSM == GS_PSM_CT32) ? 4 : 2;
         int real_width = pitch / bytes_per_pixel; 
         transfer_texture(ps2->coreTexture, frame, real_width, height, ps2->PSM, ps2->core_filter, 1);
         padding.right = real_width - width;
      } else {
         sendPalette = ps2->iface.updatedPalette;
         ps2->iface.updatedPalette = false;
         padding = ps2->iface.padding;
         if (ps2->iface.clearTexture) {
            ps2->iface.clearTexture = false;
         }
      }
      if(ps2->clearVRAM) {
         vram_alloc(ps2->gsGlobal, ps2->coreTexture);
      }
      ps2_texture_upload(ps2->gsGlobal, ps2->coreTexture, sendPalette);
      prim_texture(ps2->gsGlobal, ps2->coreTexture, 1, ps2->force_aspect, padding);
   }

   if (ps2->menuVisible) {
      bool texture_empty = !ps2->menuTexture->Width || !ps2->menuTexture->Height;
      if (!texture_empty) {
         if(ps2->clearVRAM) {
            vram_alloc(ps2->gsGlobal, ps2->menuTexture);
         }
         gsKit_texture_upload(ps2->gsGlobal, ps2->menuTexture);
         prim_texture(ps2->gsGlobal, ps2->menuTexture, 2, ps2->fullscreen, empty_ps2_insets);
      }
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
         font_driver_render_msg(ps2, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }

   if(!string_is_empty(msg))
      font_driver_render_msg(ps2, video_info, msg, NULL, NULL);

   refreshScreen(ps2);

   return true;
}

static void ps2_gfx_set_nonblock_state(void *data, bool toggle)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->vsync = !toggle;
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

   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_vram_clear(ps2->gsGlobal);

   font_driver_free_osd();

   deinitTexture(ps2->menuTexture);
   deinitTexture(ps2->coreTexture);

   free(ps2->menuTexture);
   free(ps2->coreTexture);

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

static void ps2_set_filtering(void *data, unsigned index, bool smooth)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   ps2->menu_filter = smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   bool color_correction = false;
   int PSM = (rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   bool texture_changed = texture_need_prepare(ps2->menuTexture, width, height, PSM);

   transfer_texture(ps2->menuTexture, frame, width, height, PSM, ps2->menu_filter, color_correction);
   ps2->clearVRAM = ps2->clearVRAM || texture_changed;
}

static void ps2_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;
   if (ps2->menuVisible != enable) {
      /* If Menu change status, CLEAR VRAM */
      ps2->clearVRAM = true;
      ps2->iface.clearTexture = true;
      ps2->iface.updatedPalette = true;
   }
   ps2->menuVisible = enable;
   ps2->fullscreen = fullscreen;
}

static bool ps2_get_hw_render_interface(void* data,
      const struct retro_hw_render_interface** iface)
{
   ps2_video_t* ps2 = (ps2_video_t*)data;
   ps2->iface.clearTexture = ps2->clearVRAM;
   ps2->iface.updatedPalette = true;
   ps2->iface.padding = empty_ps2_insets;
   *iface = (const struct retro_hw_render_interface*)&ps2->iface;
   return true;
}

static const video_poke_interface_t ps2_poke_interface = {
   NULL,          /* get_flags  */
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
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   ps2_set_texture_frame,
   ps2_set_texture_enable,
   font_driver_render_msg,             /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   ps2_get_hw_render_interface  /* get_hw_render_interface */
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
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  ps2_gfx_get_poke_interface,
};
