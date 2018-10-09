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


typedef struct ps2_video
{
   GSGLOBAL *gsGlobal;
   GSTEXTURE *backgroundTexture;
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
   ps2->gsGlobal->Width = 640;
   ps2->gsGlobal->Height = 448;

   gsKit_init_screen(ps2->gsGlobal);    /* Apply settings. */
   gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);
}

static size_t gskitTextureSize(GSTEXTURE *texture) {
   return gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM);
}

static void prepareTexture(GSTEXTURE *texture, int delayed) {
   texture->Width = 640;
   texture->Height = 448;
   texture->PSM = GS_PSM_CT16;
   if (delayed) {
      texture->Delayed = GS_SETTING_ON;
   }
   texture->Filter = GS_FILTER_NEAREST;
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

static void *ps2_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   initGSGlobal(ps2);
   initBackgroundTexture(ps2);

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   (void)data;
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

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

static void ps2_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   (void)iface;
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
