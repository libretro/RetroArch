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
#include "../gfx/gfx_surface_player.h"
#include "../gfx/video_driver.h"
#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

/* Every image of the pack still has what it was decoded to: pixels,
 * or the BCn payload of a DDS. */
static bool input_overlay_has_pixels(const input_overlay_t *ol)
{
   size_t i;
   for (i = 0; i < ol->num_images; i++)
      if (     !ol->images[i]
            || (!ol->images[i]->pixels && !ol->images[i]->compressed))
         return false;
   return true;
}

/* Image @u was decoded from its payload: every copy of it gets the
 * pixels, so a page entry matches its image either way. */
static void input_overlay_share_pixels(input_overlay_t *ol, size_t u)
{
   size_t i, j;
   const struct texture_compressed *tc = ol->images[u]->compressed;
   uint32_t *pixels                    = ol->images[u]->pixels;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      for (j = 0; j < o->load_images_size; j++)
         if (o->load_images[j].compressed == tc)
            o->load_images[j].pixels = pixels;
      if (o->image.compressed == tc)
         o->image.pixels = pixels;
      for (j = 0; j < o->size; j++)
         if (o->descs[j].image.compressed == tc)
            o->descs[j].image.pixels = pixels;
   }
}

/* Decode to pixels the BCn images the driver cannot sample, or with
 * @all every one of them, for load(). False when one will not decode. */
static bool input_overlay_realize(input_overlay_t *ol, bool all)
{
   size_t i;
   for (i = 0; i < ol->num_images; i++)
   {
      struct texture_image *img = ol->images[i];
      if (!img || !img->compressed || img->pixels)
         continue;
      if (     !all
            && gfx_surface_supports_compressed(img->compressed->format))
         continue;
      if (!image_texture_realize_rgba(img))
         return false;
      input_overlay_share_pixels(ol, i);
   }
   return true;
}

/* Forget every page's copy of @pixels: the block is no longer the
 * pack's, and nothing must be left that looks like an image and is
 * not one. */
static void input_overlay_forget_pixels(input_overlay_t *ol,
      const uint32_t *pixels)
{
   size_t i, j;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      for (j = 0; j < o->load_images_size; j++)
         if (o->load_images[j].pixels == pixels)
            o->load_images[j].pixels = NULL;
      if (o->image.pixels == pixels)
         o->image.pixels = NULL;
      for (j = 0; j < o->size; j++)
         if (o->descs[j].image.pixels == pixels)
            o->descs[j].image.pixels = NULL;
   }
}

static void input_overlay_forget_compressed(input_overlay_t *ol,
      const struct texture_compressed *tc)
{
   size_t i, j;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      for (j = 0; j < o->load_images_size; j++)
         if (o->load_images[j].compressed == tc)
            o->load_images[j].compressed = NULL;
      if (o->image.compressed == tc)
         o->image.compressed = NULL;
      for (j = 0; j < o->size; j++)
         if (o->descs[j].image.compressed == tc)
            o->descs[j].image.compressed = NULL;
   }
}

static void input_overlay_compressed_free(void *payload)
{
   struct texture_image img;
   memset(&img, 0, sizeof(img));
   img.compressed = (struct texture_compressed*)payload;
   image_texture_free(&img);
}

void input_overlay_anim_stop(input_overlay_t *ol)
{
   size_t i;
   if (!ol || !ol->anim_player)
      return;
   for (i = 0; i < ol->num_images; i++)
   {
      gfx_surface_player_free((gfx_surface_player_t*)ol->anim_player[i]);
      ol->anim_player[i] = NULL;
   }
}

void input_overlay_release_textures(input_overlay_t *ol)
{
   size_t i;
   if (!ol)
      return;
   /* a playback finds its surface in the array freed below */
   input_overlay_anim_stop(ol);
   if (ol->surfaces)
   {
      for (i = 0; i < ol->num_images; i++)
         if (ol->images[i] && ol->images[i]->pixels)
            GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
                  -(int)(((size_t)ol->images[i]->width
                        * ol->images[i]->height
                        * sizeof(uint32_t)) >> 10));
      /* The texture goes with its surface. A still image's upload
       * that the video thread has not reached yet is going to read
       * the pack's pixels, and the caller may be about to free the
       * pack: those pixels go with the surface instead, which frees
       * them at the completion. The image keeps its size and is left
       * without pixels, as after input_overlay_drop_pixels(). */
      for (i = 0; i < ol->num_images; i++)
      {
         bool atlas       = ol->anim_2frame
            && ol->anim_2frame[i] == INPUT_OVERLAY_2FRAME_ATLAS;
         uint32_t *pixels = atlas ? ol->anim_2frame_pix[i]
            : ol->images[i] ? ol->images[i]->pixels : NULL;
         /* what a compressed upload still out reads */
         struct texture_compressed *tc = ol->surfaces[i]
            ? ((gfx_surface_t*)ol->surfaces[i])->img.compressed : NULL;
         if (tc)
         {
            /* likewise the payload of a compressed one */
            if (gfx_surface_free_adopt_payload(
                     (gfx_surface_t*)ol->surfaces[i], tc,
                     input_overlay_compressed_free))
               input_overlay_forget_compressed(ol, tc);
         }
         else if (gfx_surface_free_adopt((gfx_surface_t*)ol->surfaces[i],
                  pixels))
         {
            if (atlas)
               ol->anim_2frame_pix[i] = NULL;
            else
            {
               input_overlay_forget_pixels(ol, pixels);
               ol->images[i]->pixels = NULL;
            }
         }
         /* asked again at the next upload */
         if (atlas)
            ol->anim_2frame[i] = INPUT_OVERLAY_2FRAME_PAIR;
      }
      free(ol->surfaces);
      ol->surfaces = NULL;
   }
   for (i = 0; i < ol->size; i++)
   {
      ol->overlays[i].textures       = NULL;
      ol->overlays[i].texture_images = NULL;
   }
   free(ol->page_textures);
   ol->page_textures = NULL;
}

/* A frame dropped on the video thread. A two-frame image is marked as
 * showing neither, so the next poll submits the one its press state
 * wants; a looping one sends the frame still in its slot again. */
static void input_overlay_surface_release(void *user, gfx_surface_t *s,
      unsigned slot)
{
   input_overlay_t *ol = (input_overlay_t*)user;
   size_t i;
   (void)slot;
   if (!s->dropped || !ol || !ol->surfaces)
      return;
   for (i = 0; i < ol->num_images; i++)
   {
      if (ol->surfaces[i] != (void*)s)
         continue;
      if (ol->anim_2frame && ol->anim_2frame[i])
      {
         if (ol->anim_2frame_cur)
            ol->anim_2frame_cur[i] = 0xff;
      }
      else if (ol->anim_resend)
         ol->anim_resend[i] = 1;
   }
}

bool input_overlay_anim_submit(input_overlay_t *ol, size_t i)
{
   gfx_surface_t *s = (gfx_surface_t*)ol->surfaces[i];
   enum gfx_surface_submit_result r = gfx_surface_submit(s, 0,
         ol->images[i]->supports_rgba);
   ol->anim_resend[i] = (r == GFX_SURFACE_SUBMIT_DROPPED);
   return r == GFX_SURFACE_SUBMIT_DONE
       || r == GFX_SURFACE_SUBMIT_QUEUED
       || r == GFX_SURFACE_SUBMIT_DROPPED;
}

/* Every unique image of the pack becomes a surface and its upload is
 * submitted. With threaded video the uploads are the video thread's
 * from here; the handles arrive at a later poll. False when one could
 * not be made or submitted: the surfaces made so far stay where they
 * are for the disable to release - releasing here would hand the
 * pixels of any still in flight to their surfaces, and the caller is
 * about to show the page through load(), which reads them. */
static bool input_overlay_submit_textures(input_overlay_t *ol)
{
   size_t i;

   if (!(ol->surfaces = (void**)calloc(ol->num_images, sizeof(void*))))
      return false;

   /* A still image's pixels are the pack's and stay where they are,
    * so its surface carries no slots of its own and the upload is the
    * same submit-and-own path an animated preview frame takes. An
    * animated one (APNG) gets slots instead: its stream composes each
    * frame into a slot and the texture is updated in place, so the
    * page's handles never change and a frame costs no upload of its
    * own. */
   for (i = 0; i < ol->num_images; i++)
   {
      gfx_surface_src_t src;
      gfx_surface_t *s;
      bool animated    = ol->anim_stream && ol->anim_stream[i];
      /* A two-frame image as one texture with both frames stacked, a
       * press then moving its rectangle: where the driver can move it,
       * and the stack is no taller than a console's textures go. */
      unsigned rows    = INPUT_OVERLAY_2FRAME_ROWS(ol->images[i]->height);
      bool atlas       = animated
         && ol->anim_2frame && ol->anim_2frame[i]
         && ol->anim_2frame_pix[i]
         && ol->iface && ol->iface->tex_geom
         && rows <= 1024
         && VIDEO_SCALE_FITS(ol->images[i]->width, rows);
      if (atlas)
      {
         s = gfx_surface_new_static(
               VIDEO_SCALE_PACK(ol->images[i]->width, rows),
               TEXTURE_FILTER_LINEAR);
         GFX_INSTR_INC(GFX_INSTR_OVERLAY_UPLOAD);
         ol->surfaces[i]  = s;
         if (!s)
            return false;
         ol->anim_2frame[i]     = INPUT_OVERLAY_2FRAME_ATLAS;
         ol->anim_2frame_cur[i] = 0;
         src.pixels       = ol->anim_2frame_pix[i];
         src.payload      = NULL;
         src.payload_free = NULL;
         src.pixfmt       = IMAGE_PIXFMT_8888;
         src.rgba         = ol->images[i]->supports_rgba;
         if (gfx_surface_submit_external(s, &src, NULL, NULL)
               == GFX_SURFACE_SUBMIT_FAILED)
            return false;
         continue;
      }
      /* A BCn image the driver samples goes up as it is */
      if (!animated && ol->images[i]->compressed && !ol->images[i]->pixels)
      {
         s = gfx_surface_new_still(TEXTURE_FILTER_LINEAR);
         GFX_INSTR_INC(GFX_INSTR_OVERLAY_UPLOAD);
         ol->surfaces[i]  = s;
         if (     !s
               || gfx_surface_submit_compressed(s, ol->images[i], NULL,
                     NULL) == GFX_SURFACE_SUBMIT_FAILED)
            return false;
         /* the driver refused it and it was decoded */
         if (ol->images[i]->pixels)
         {
            input_overlay_share_pixels(ol, i);
            GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
                  (int)(((size_t)ol->images[i]->width
                        * ol->images[i]->height
                        * sizeof(uint32_t)) >> 10));
         }
         continue;
      }
      /* One slot: a frame is composed here and submitted immediately,
       * and a surface with a submit in flight refuses every slot, so
       * a second one could never be reached - it would be a frame's
       * worth of memory per animated image, for nothing. */
      s = !VIDEO_SCALE_FITS(ol->images[i]->width,
               ol->images[i]->height)
         ? NULL
         : animated
         ? gfx_surface_new(VIDEO_SCALE_PACK(ol->images[i]->width,
               ol->images[i]->height),
               1, IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR,
               input_overlay_surface_release, ol)
         : gfx_surface_new_static(VIDEO_SCALE_PACK(ol->images[i]->width,
            ol->images[i]->height), TEXTURE_FILTER_LINEAR);
      GFX_INSTR_INC(GFX_INSTR_OVERLAY_UPLOAD);
      GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
            (int)(((size_t)ol->images[i]->width * ol->images[i]->height
                  * sizeof(uint32_t)) >> 10));
      ol->surfaces[i]  = s;
      if (!s)
         return false;
      /* Fitted once, here, where the pack keeps its descriptor in step
       * with its pixels: an animation's stream composes 8-bit frames
       * into an 8888 slot, so its first frame is narrowed too. */
      if (animated && ol->images[i]->pix10)
         image_texture_narrow_10bit(ol->images[i]);
      if (!video_driver_texture_fit(ol->images[i]))
         return false;
      if (animated)
      {
         /* The first frame is already composed in the decoded image:
          * it goes into a slot, and the stream advances from the
          * second on the tick below. */
         memcpy(s->slots[0], ol->images[i]->pixels,
               VIDEO_SCALE_AREA(s->dims) * sizeof(uint32_t));
         if (gfx_surface_submit(s, 0, ol->images[i]->supports_rgba)
               == GFX_SURFACE_SUBMIT_FAILED)
            return false;
         if (ol->anim_player)
         {
            /* the clock starts again from the frame shown here */
            gfx_surface_player_free(
                  (gfx_surface_player_t*)ol->anim_player[i]);
            ol->anim_player[i] = NULL;
         }
         if (ol->anim_resend)
            ol->anim_resend[i] = 0;
         /* The new texture shows the first (unpressed) frame, so a
          * two-frame APNG held down across a reupload gets its
          * pressed frame back on the next poll. */
         if (ol->anim_2frame_cur)
            ol->anim_2frame_cur[i] = 0;
         continue;
      }
      src.pixels       = ol->images[i]->pixels;
      src.payload      = NULL;
      src.payload_free = NULL;
      src.pixfmt       = ol->images[i]->pix10
         ? IMAGE_PIXFMT_2101010 : IMAGE_PIXFMT_8888;
      src.rgba         = ol->images[i]->supports_rgba;
      if (gfx_surface_submit_external(s, &src, NULL, NULL)
            == GFX_SURFACE_SUBMIT_FAILED)
         return false;
   }
   return true;
}

/* The pack will not be shown as textures. What was uploaded is not
 * held for nothing - unless some of it is still the video thread's:
 * a release now would send those images' pixels after their surfaces,
 * and the page is about to go through load(), which reads them. Those
 * wait for the disable. */
static void input_overlay_decline_textures(input_overlay_t *ol)
{
   size_t i;
   ol->flags |= INPUT_OVERLAY_TEXTURES_DECLINED;
   if (ol->surfaces)
      for (i = 0; i < ol->num_images; i++)
         if (     ol->surfaces[i]
               && ((gfx_surface_t*)ol->surfaces[i])->inflight)
            return;
   input_overlay_release_textures(ol);
}

enum overlay_textures_state
{
   OVERLAY_TEXTURES_READY = 0,
   OVERLAY_TEXTURES_PENDING,
   OVERLAY_TEXTURES_FAILED
};

/* Where the submitted uploads stand, and once every handle is in, the
 * page lists built from them. */
static enum overlay_textures_state input_overlay_collect_textures(
      input_overlay_t *ol)
{
   uintptr_t *tex;
   uint32_t *images;
   size_t i, j, k, total = ol->num_images;

   for (i = 0; i < ol->num_images; i++)
   {
      gfx_surface_t *s = (gfx_surface_t*)ol->surfaces[i];
      if (!s)
         return OVERLAY_TEXTURES_FAILED;
      if (s->handle)
         continue;
      /* Still the video thread's, or back from it with nothing. */
      return s->inflight
         ? OVERLAY_TEXTURES_PENDING : OVERLAY_TEXTURES_FAILED;
   }

   for (i = 0; i < ol->size; i++)
      total += ol->overlays[i].load_images_size;
   /* the handles, then the unique image behind each */
   if (!(tex = (uintptr_t*)calloc(total,
               sizeof(*tex) + sizeof(uint32_t))))
      return OVERLAY_TEXTURES_FAILED;
   images = (uint32_t*)(tex + total);
   for (i = 0; i < ol->num_images; i++)
      tex[i] = ((gfx_surface_t*)ol->surfaces[i])->handle;

   /* The loader deduplicated by path, so a page's entry shares its
    * pixels with exactly one unique image. */
   k = ol->num_images;
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      for (j = 0; j < o->load_images_size; j++, k++)
      {
         size_t u;
         /* The page's entry is matched to the pack's unique image by
          * the buffer they share. A released buffer is NULL in both,
          * and NULL matches everything, so a page built after the
          * pixels went would take the first image's texture for every
          * entry - the caller declines the pack instead, and the next
          * init reloads it from its path. */
         for (u = 0; u < ol->num_images; u++)
         {
            const struct texture_image *e = &o->load_images[j];
            if (     (e->pixels && ol->images[u]->pixels == e->pixels)
                  || (     !e->pixels && e->compressed
                        && ol->images[u]->compressed == e->compressed))
            {
               tex[k]    = tex[u];
               images[k] = (uint32_t)u;
               break;
            }
         }
         if (u == ol->num_images)
         {
            free(tex);
            return OVERLAY_TEXTURES_FAILED;
         }
      }
   }
   k = ol->num_images;
   for (i = 0; i < ol->size; i++)
   {
      ol->overlays[i].textures       = &tex[k];
      ol->overlays[i].texture_images = &images[k];
      k                             += ol->overlays[i].load_images_size;
   }
   ol->page_textures = tex;
   return OVERLAY_TEXTURES_READY;
}

/* Every unique image of the pack becomes one texture, and every page
 * a list of handles into that set, so that switching pages uploads
 * nothing. Only for a driver with load_textures; others keep taking
 * the pixels through load() on each switch. The handles live until
 * input_overlay_release_textures(), which the disable runs while the
 * driver is still there to unload them.
 *
 * True when the pack has its textures. False when it has not, and the
 * page goes through load(): either for good, with
 * INPUT_OVERLAY_TEXTURES_DECLINED set, or because the uploads are
 * still with the video thread - the poll follows the posts at once,
 * so under threaded video that is the usual answer to the first call.
 * The surfaces are kept, not thrown away to be uploaded again at the
 * next page, and input_overlay_promote_textures() moves the pack over
 * once the handles are in. */
bool input_overlay_upload_textures(input_overlay_t *ol)
{
   if (ol->page_textures)
      return true;
   if (!ol->images || !ol->num_images)
      return false;

   if (!ol->surfaces)
   {
      /* Nothing to upload from: the pixels went once the driver had
       * the textures, and the textures went with the driver. */
      if (!input_overlay_has_pixels(ol))
         return false;
      if (     !input_overlay_realize(ol, false)
            || !input_overlay_submit_textures(ol))
      {
         input_overlay_decline_textures(ol);
         return false;
      }
#ifdef HAVE_THREADS
      /* Under threaded video the handles land through the wrapper's
       * completion list; a thread quick enough has them there now. */
      video_thread_async_poll();
#endif
   }

   switch (input_overlay_collect_textures(ol))
   {
      case OVERLAY_TEXTURES_READY:
         return true;
      case OVERLAY_TEXTURES_PENDING:
         break;
      case OVERLAY_TEXTURES_FAILED:
         input_overlay_decline_textures(ol);
         break;
   }
   return false;
}

bool input_overlay_has_source(const input_overlay_t *ol)
{
   /* A pack of hitboxes alone has nothing to lose. */
   if (!ol->num_images || !ol->images)
      return true;
   /* All of them: a release with uploads in flight takes the pixels
    * of just those images (input_overlay_release_textures). */
   return ol->page_textures || input_overlay_has_pixels(ol);
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
 * The sizes stay (image_texture_free() clears them): they are how
 * anyone asks whether a page or a desc has an image
 * (OVERLAY_HAS_IMAGE), because images[i] is not a private struct but
 * the first overlay::image or overlay_desc::image that named the file,
 * and clearing its pixels here clears them there. Every other copy of
 * the pointer - the pages' load_images, and the image structs of the
 * descs and pages that share the file - is cleared with it, so nothing
 * is left that points at freed memory. */
static void input_overlay_drop_pixels(input_overlay_t *ol)
{
   size_t i, j;

   for (i = 0; i < ol->num_images; i++)
   {
      unsigned width  = ol->images[i]->width;
      unsigned height = ol->images[i]->height;
      /* a stacked pair is in its texture: its frames go too */
      if (     ol->anim_2frame
            && ol->anim_2frame[i] == INPUT_OVERLAY_2FRAME_ATLAS)
      {
         free(ol->anim_2frame_pix[i]);
         ol->anim_2frame_pix[i] = NULL;
      }
      if (!ol->images[i]->pixels && !ol->images[i]->compressed)
         continue;
      if (ol->images[i]->pixels)
         GFX_INSTR_ADD(GFX_INSTR_OVERLAY_PIXEL_KIB,
               -(int)(((size_t)width * height * sizeof(uint32_t)) >> 10));
      image_texture_free(ol->images[i]);
      ol->images[i]->width  = width;
      ol->images[i]->height = height;
   }
   for (i = 0; i < ol->size; i++)
   {
      struct overlay *o = &ol->overlays[i];
      for (j = 0; j < o->load_images_size; j++)
      {
         o->load_images[j].pixels     = NULL;
         o->load_images[j].compressed = NULL;
      }
      o->image.pixels     = NULL;
      o->image.compressed = NULL;
      for (j = 0; j < o->size; j++)
      {
         o->descs[j].image.pixels     = NULL;
         o->descs[j].image.compressed = NULL;
      }
   }
}

/* The rectangle of the stack each two-frame entry of the active page
 * shows: a press moves it, uploading nothing. */
void input_overlay_atlas_frames(input_overlay_t *ol, size_t image)
{
   unsigned j;
   const struct overlay *o = ol ? ol->active : NULL;

   if (     !o || !o->texture_images || !ol->anim_2frame
         || !ol->iface || !ol->iface->tex_geom)
      return;
   for (j = 0; j < o->load_images_size; j++)
   {
      uint32_t u = o->texture_images[j];
      float rows, h;
      if (     (image != (size_t)-1 && u != image)
            || ol->anim_2frame[u] != INPUT_OVERLAY_2FRAME_ATLAS)
         continue;
      h    = (float)ol->images[u]->height;
      rows = (float)INPUT_OVERLAY_2FRAME_ROWS(ol->images[u]->height);
      ol->iface->tex_geom(ol->iface_data, j, 0.0f,
            ol->anim_2frame_cur[u] ? (h + 2.0f) / rows : 0.0f,
            1.0f, h / rows);
   }
}

enum input_overlay_page input_overlay_load_page(input_overlay_t *ol)
{
   /* Whatever the driver ends up with, it is not what it held. */
   input_overlay_alpha_forget(ol);
   if (     ol->iface->load_textures
         && !(ol->flags & INPUT_OVERLAY_TEXTURES_DECLINED)
         && input_overlay_upload_textures(ol))
   {
      if (ol->iface->load_textures(ol->iface_data,
               ol->active->textures, ol->active->load_images_size))
      {
         GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE);
         input_overlay_drop_pixels(ol);
         input_overlay_atlas_frames(ol, (size_t)-1);
         return INPUT_OVERLAY_PAGE_TEXTURES;
      }
      /* The wrapper's table answers for any driver; the one beneath
       * it may have no such path. */
      input_overlay_decline_textures(ol);
   }
   /* A pack with neither textures nor pixels has nothing to show and
    * must not show what its pages used to point at; it is on its way
    * to being reloaded (input_overlay_init). */
   if (!input_overlay_has_source(ol) || !input_overlay_realize(ol, true))
      return INPUT_OVERLAY_PAGE_NONE;
   GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE);
   GFX_INSTR_INC(GFX_INSTR_OVERLAY_PAGE_LOAD);
   /* The driver's own answer counts: a page it could not load is not
    * there, whatever the pack meant to show. */
   if (     !ol->iface->load
         || !ol->iface->load(ol->iface_data, ol->active->load_images,
               ol->active->load_images_size))
      return INPUT_OVERLAY_PAGE_NONE;
   return INPUT_OVERLAY_PAGE_PIXELS;
}

bool input_overlay_promote_textures(input_overlay_t *ol)
{
   /* Nearly every poll: nothing is pending. */
   if (     !ol
         || !ol->surfaces
         || ol->page_textures
         || (ol->flags & INPUT_OVERLAY_TEXTURES_DECLINED)
         || !ol->iface
         || !ol->iface->load_textures)
      return false;
   if (!input_overlay_upload_textures(ol))
      return false;
   input_overlay_alpha_forget(ol);
   if (ol->iface->load_textures(ol->iface_data,
            ol->active->textures, ol->active->load_images_size))
   {
      input_overlay_drop_pixels(ol);
      input_overlay_atlas_frames(ol, (size_t)-1);
      return true;
   }
   input_overlay_decline_textures(ol);
   return false;
}

#endif
