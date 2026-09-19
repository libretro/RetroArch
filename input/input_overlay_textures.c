/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - The RetroArch team
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

/* The overlay pack's textures: the upload of its images, the page
 * lists built from them, and the choice between handing a page to the
 * driver as textures (load_textures) or as pixels (load). Kept apart
 * from input_driver.c so that samples/input/overlay_textures can
 * compile this file as it is against a stub driver. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_OVERLAY

#include "input_overlay.h"
#include "../gfx/gfx_instrument.h"
#include "../gfx/gfx_surface.h"
#include "../gfx/video_driver.h"
#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

void input_overlay_release_textures(input_overlay_t *ol)
{
   size_t i;
   if (!ol)
      return;
   if (ol->surfaces)
   {
      for (i = 0; i < ol->num_images; i++)
         if (ol->images[i] && ol->images[i]->pixels)
            GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
                  -(int)(((size_t)ol->images[i]->width
                        * ol->images[i]->height
                        * sizeof(uint32_t)) >> 10));
      /* The texture goes with its surface; one still in flight frees
       * itself when the video thread is done with it. */
      for (i = 0; i < ol->num_images; i++)
         gfx_surface_free((gfx_surface_t*)ol->surfaces[i]);
      free(ol->surfaces);
      ol->surfaces = NULL;
   }
   for (i = 0; i < ol->size; i++)
      ol->overlays[i].textures = NULL;
   free(ol->page_textures);
   ol->page_textures = NULL;
}

/* Every unique image of the pack becomes one texture, and every page
 * a list of handles into that set, so that switching pages uploads
 * nothing. Only for a driver with load_textures; others keep taking
 * the pixels through load() on each switch. The handles live until
 * input_overlay_release_textures(), which the disable runs while the
 * driver is still there to unload them. Returns false when the pack
 * cannot be uploaded this way, in which case load() is used. */
bool input_overlay_upload_textures(input_overlay_t *ol)
{
   uintptr_t *tex;
   size_t i, j, k, total = ol->num_images;

   if (ol->page_textures || !ol->images || !ol->num_images)
      return ol->page_textures != NULL;
   /* Nothing to upload from: the pixels went once the driver had the
    * textures, and the textures went with the driver. */
   if (!ol->images[0]->pixels)
      return false;

   for (i = 0; i < ol->size; i++)
      total += ol->overlays[i].load_images_size;
   if (!(tex = (uintptr_t*)calloc(total, sizeof(*tex))))
      return false;
   ol->page_textures = tex;
   if (!(ol->surfaces = (void**)calloc(ol->num_images, sizeof(void*))))
   {
      input_overlay_release_textures(ol);
      return false;
   }

   /* Each unique image becomes a surface holding one texture. A still
    * image's pixels are the pack's and stay where they are, so its
    * surface carries no slots of its own and the upload is the same
    * submit-and-own path an animated preview frame takes. An animated
    * one (APNG) gets slots instead: its stream composes each frame
    * into a slot and the texture is updated in place, so the page's
    * handles never change and a frame costs no upload of its own. */
   for (i = 0; i < ol->num_images; i++)
   {
      bool animated    = ol->anim_stream && ol->anim_stream[i];
      /* One slot: a frame is composed here and submitted immediately,
       * and a surface with a submit in flight refuses every slot, so
       * a second one could never be reached - it would be a frame's
       * worth of memory per animated image, for nothing. */
      gfx_surface_t *s = animated
         ? gfx_surface_new(ol->images[i]->width, ol->images[i]->height,
               1, TEXTURE_FILTER_LINEAR, NULL, NULL)
         : gfx_surface_new_static(ol->images[i]->width,
            ol->images[i]->height, TEXTURE_FILTER_LINEAR);
      GFX_INSTR_INC(GFX_INSTR_OVERLAY_UPLOAD);
      GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
            (int)(((size_t)ol->images[i]->width * ol->images[i]->height
                  * sizeof(uint32_t)) >> 10));
      ol->surfaces[i]  = s;
      if (animated && s)
      {
         /* The first frame is already composed in the decoded image:
          * it goes into a slot, and the stream advances from the
          * second on the tick below. */
         memcpy(s->slots[0], ol->images[i]->pixels,
               (size_t)s->width * s->height * sizeof(uint32_t));
         if (gfx_surface_submit(s, 0, ol->images[i]->supports_rgba)
               == GFX_SURFACE_SUBMIT_FAILED)
         {
            input_overlay_release_textures(ol);
            return false;
         }
         ol->anim_next_us[i] = 0;
         continue;
      }
      /* An asset is uploaded once and never again, so a submit that
       * the video thread has not finished with is waited out by the
       * poll below rather than dropped. */
      if (     !s
            || gfx_surface_submit_external(s, ol->images[i]->pixels,
                  ol->images[i]->supports_rgba, NULL, NULL)
               == GFX_SURFACE_SUBMIT_FAILED)
      {
         input_overlay_release_textures(ol);
         return false;
      }
   }
#ifdef HAVE_THREADS
   /* Under threaded video the handles land through the wrapper's
    * completion list; the page needs them now. */
   video_thread_async_poll();
#endif
   for (i = 0; i < ol->num_images; i++)
   {
      gfx_surface_t *s = (gfx_surface_t*)ol->surfaces[i];
      if (!s->handle)
      {
         input_overlay_release_textures(ol);
         return false;
      }
      tex[i] = s->handle;
   }

   /* The loader deduplicated by path, so a page's entry shares its
    * pixels with exactly one unique image. */
   k = ol->num_images;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      o->textures       = &tex[k];
      for (j = 0; j < o->load_images_size; j++, k++)
      {
         size_t u;
         for (u = 0; u < ol->num_images; u++)
         {
            if (ol->images[u]->pixels == o->load_images[j].pixels)
            {
               tex[k] = tex[u];
               break;
            }
         }
         if (u == ol->num_images)
         {
            input_overlay_release_textures(ol);
            return false;
         }
      }
   }
   return true;
}

bool input_overlay_has_source(const input_overlay_t *ol)
{
   /* A pack of hitboxes alone has nothing to lose. */
   if (!ol->num_images || !ol->images)
      return true;
   return ol->page_textures || ol->images[0]->pixels;
}

/* The driver has the pack's textures and nothing reads the pixels
 * again: a pack is anything from a megabyte of button sprites to forty
 * of 4K border, held for the whole session against the one event that
 * would want them - a video reinit, which reloads the overlay from
 * its path anyway (video_driver_init_internal).
 *
 * Only here, after load_textures() has answered true. The page lists
 * are built by matching each page's pixels to a unique image's, so
 * the pixels must outlive input_overlay_upload_textures(); and a
 * driver that declines the textures is shown the pages through
 * load(), which reads them.
 *
 * The sizes stay (image_texture_free() clears them), and the pages'
 * copies of the pointers are cleared with the pixels they pointed at:
 * nothing is left that looks like an image and is not one. The copies
 * in overlay::image and overlay_desc::image are only ever tested, as
 * "this has an image", and are left to say so. */
static void input_overlay_drop_pixels(input_overlay_t *ol)
{
   size_t i, j;

   for (i = 0; i < ol->num_images; i++)
   {
      unsigned width  = ol->images[i]->width;
      unsigned height = ol->images[i]->height;
      if (!ol->images[i]->pixels)
         continue;
      GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
            -(int)(((size_t)width * height * sizeof(uint32_t)) >> 10));
      image_texture_free(ol->images[i]);
      ol->images[i]->width  = width;
      ol->images[i]->height = height;
   }
   for (i = 0; i < ol->size; i++)
      for (j = 0; j < ol->overlays[i].load_images_size; j++)
         ol->overlays[i].load_images[j].pixels = NULL;
}

bool input_overlay_load_page(input_overlay_t *ol)
{
   if (     ol->iface->load_textures
         && !(ol->flags & INPUT_OVERLAY_TEXTURES_DECLINED)
         && input_overlay_upload_textures(ol))
   {
      if (ol->iface->load_textures(ol->iface_data,
               ol->active->textures, ol->active->load_images_size))
      {
         GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE);
         input_overlay_drop_pixels(ol);
         return true;
      }
      /* The wrapper's table answers for any driver; the one beneath
       * it may have no such path. Not held for nothing. */
      input_overlay_release_textures(ol);
      ol->flags |= INPUT_OVERLAY_TEXTURES_DECLINED;
   }
   /* A pack with neither textures nor pixels has nothing to show and
    * must not show what its pages used to point at; it is on its way
    * to being reloaded (input_overlay_init). */
   if (!input_overlay_has_source(ol))
      return false;
   GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE);
   GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE_LOAD);
   if (ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);
   return false;
}

#endif
