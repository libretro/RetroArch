/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdlib.h>
#include <string.h>

#include "openxr_swapchain.h"

#include "../video_defines.h"

#include "../../verbosity.h"

bool openxr_swapchains_load(openxr_swapchains_t *sc,
      const openxr_runtime_t *rt, openxr_session_t *session)
{
   sc->session = session;
   return OPENXR_FN(rt, sc, EnumerateSwapchainFormats)
       && OPENXR_FN(rt, sc, CreateSwapchain)
       && OPENXR_FN(rt, sc, DestroySwapchain)
       && OPENXR_FN(rt, sc, EnumerateSwapchainImages)
       && OPENXR_FN(rt, sc, AcquireSwapchainImage)
       && OPENXR_FN(rt, sc, WaitSwapchainImage)
       && OPENXR_FN(rt, sc, ReleaseSwapchainImage);
}

void openxr_swapchains_list_formats(openxr_swapchains_t *sc)
{
   uint32_t count = 0;
   int64_t *all   = NULL;
   sc->num_formats = 0;
   if (     XR_FAILED(sc->EnumerateSwapchainFormats(sc->session->session,
               0, &count, NULL))
         || !count
         || !(all = (int64_t*)malloc(count * sizeof(*all))))
      return;
   /* The runtime's preferred come first. */
   if (XR_SUCCEEDED(sc->EnumerateSwapchainFormats(sc->session->session,
               count, &count, all)))
   {
      if (count > OPENXR_MAX_FORMATS)
         count = OPENXR_MAX_FORMATS;
      memcpy(sc->formats, all, count * sizeof(*all));
      sc->num_formats = count;
   }
   free(all);
}

bool openxr_swapchains_supports(const openxr_swapchains_t *sc,
      int64_t format)
{
   uint32_t i;
   for (i = 0; i < sc->num_formats; i++)
      if (sc->formats[i] == format)
         return true;
   return false;
}

XrSwapchain openxr_swapchains_create_filled(openxr_swapchains_t *sc,
      const int64_t *formats, unsigned num_formats, unsigned dims,
      openxr_images_cb images, void *images_user,
      openxr_fill_cb fill, void *fill_user,
      const char **step, int *code)
{
   unsigned i;
   uint32_t n        = 0;
   uint32_t index    = 0;
   bool found        = false;
   XrResult res      = XR_SUCCESS;
   XrSwapchain chain = XR_NULL_HANDLE;
   XrSwapchainCreateInfo ci;
   XrSwapchainImageAcquireInfo ai;
   XrSwapchainImageWaitInfo wi;
   XrSwapchainImageReleaseInfo ri;

   *step = NULL;
   *code = 0;
   for (i = 0; i < num_formats && !found; i++)
      if ((found = openxr_swapchains_supports(sc, formats[i])))
         ci.format = formats[i];
   if (!found)
   {
      *step = "no 8-bit RGBA format";
      return XR_NULL_HANDLE;
   }
   ci.type        = XR_TYPE_SWAPCHAIN_CREATE_INFO;
   ci.next        = NULL;
   ci.createFlags = 0;
   ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
   ci.sampleCount = 1;
   ci.width       = VIDEO_SCALE_W(dims);
   ci.height      = VIDEO_SCALE_H(dims);
   ci.faceCount   = 1;
   ci.arraySize   = 1;
   ci.mipCount    = 1;
   ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
   ai.next = NULL;
   wi.type    = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
   wi.next    = NULL;
   wi.timeout = 100000000; /* 100 ms: a fresh swapchain is free */

   openxr_session_lock(sc->session);
   res = sc->CreateSwapchain(sc->session->session, &ci, &chain);
   if (XR_SUCCEEDED(res))
      res = sc->EnumerateSwapchainImages(chain, 0, &n, NULL);
   if (XR_SUCCEEDED(res) && (!n || n > OPENXR_MAX_IMAGES))
      res = XR_ERROR_SIZE_INSUFFICIENT;
   if (XR_SUCCEEDED(res))
      res = images(images_user, chain, 0, &n);
   if (XR_SUCCEEDED(res))
      res = sc->AcquireSwapchainImage(chain, &ai, &index);
   if (XR_SUCCEEDED(res) && index >= n)
      res = XR_ERROR_SIZE_INSUFFICIENT;
   if (XR_SUCCEEDED(res))
      res = sc->WaitSwapchainImage(chain, &wi);
   openxr_session_unlock(sc->session);
   /* XR_TIMEOUT_EXPIRED succeeds too, but leaves no image. */
   if (res != XR_SUCCESS)
   {
      *step = "swapchain";
      *code = (int)res;
      goto fail;
   }
   if (!fill(fill_user, index, step, code))
      goto fail;
   ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
   ri.next = NULL;
   openxr_session_lock(sc->session);
   res = sc->ReleaseSwapchainImage(chain, &ri);
   openxr_session_unlock(sc->session);
   if (XR_FAILED(res))
   {
      *step = "xrReleaseSwapchainImage";
      *code = (int)res;
      goto fail;
   }
   return chain;

fail:
   if (chain)
   {
      openxr_session_lock(sc->session);
      sc->DestroySwapchain(chain);
      openxr_session_unlock(sc->session);
   }
   return XR_NULL_HANDLE;
}

void openxr_slot_destroy(openxr_swapchains_t *sc, unsigned slot)
{
   unsigned l;
   XrSwapchain chains[2];
   openxr_slot_t *s = &sc->slots[slot];
   if (!s->swapchains[0])
      return;
   openxr_session_lock(sc->session);
   memcpy(chains, s->swapchains, sizeof(chains));
   memset(s->swapchains, 0, sizeof(s->swapchains));
   memset(s->acquired, 0, sizeof(s->acquired));
   memset(s->waited, 0, sizeof(s->waited));
   s->dims      = 0;
   s->layers    = 0;
   retro_atomic_store_release_int(&s->content, 0);
   for (l = 0; l < 2; l++)
      if (chains[l])
         sc->DestroySwapchain(chains[l]);
   openxr_session_unlock(sc->session);
}

bool openxr_slot_create(openxr_swapchains_t *sc, unsigned slot,
      int64_t format, bool mutable_format, unsigned dims, unsigned layers,
      openxr_images_cb images, void *images_user, unsigned *num_images)
{
   uint32_t l;
   uint32_t n   = 0;
   XrResult res = XR_SUCCESS;
   XrSwapchainCreateInfo ci;
   XrSwapchain chains[2];
   openxr_slot_t *s = &sc->slots[slot];

   openxr_slot_destroy(sc, slot);
   ci.type        = XR_TYPE_SWAPCHAIN_CREATE_INFO;
   ci.next        = NULL;
   ci.createFlags = 0;
   ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
   /* The chains' final passes write through a view in the window's
    * format. */
   if (mutable_format)
      ci.usageFlags |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
   ci.format      = format;
   ci.sampleCount = 1;
   ci.width       = VIDEO_SCALE_W(dims);
   ci.height      = VIDEO_SCALE_H(dims);
   ci.faceCount   = 1;
   ci.arraySize   = 1;
   ci.mipCount    = 1;
   chains[0] = XR_NULL_HANDLE;
   chains[1] = XR_NULL_HANDLE;

   openxr_session_lock(sc->session);
   for (l = 0; l < layers && XR_SUCCEEDED(res); l++)
   {
      uint32_t count = 0;
      res = sc->CreateSwapchain(sc->session->session, &ci, &chains[l]);
      if (XR_SUCCEEDED(res))
         res = sc->EnumerateSwapchainImages(chains[l], 0, &count, NULL);
      /* The driver keeps one image count per slot. */
      if (     XR_SUCCEEDED(res)
            && (  !count || count > OPENXR_MAX_IMAGES
               || (l && count != n)))
         res = XR_ERROR_SIZE_INSUFFICIENT;
      if (XR_SUCCEEDED(res))
         res = images(images_user, chains[l], l, &count);
      n = count;
   }
   if (XR_SUCCEEDED(res))
   {
      memcpy(s->swapchains, chains, sizeof(chains));
      memset(s->acquired, 0, sizeof(s->acquired));
      memset(s->waited, 0, sizeof(s->waited));
      s->dims      = dims;
      s->layers    = layers;
      retro_atomic_store_release_int(&s->content, 0);
   }
   else
      for (l = 0; l < 2; l++)
         if (chains[l] != XR_NULL_HANDLE)
            sc->DestroySwapchain(chains[l]);
   openxr_session_unlock(sc->session);

   if (XR_FAILED(res))
   {
      RARCH_ERR("[OpenXR] No %ux%u swapchain with %u layer(s) (%d).\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), layers, (int)res);
      return false;
   }
   *num_images = n;
   RARCH_LOG("[OpenXR] Slot %u: %ux%u, %u layer(s), %u images.\n", slot,
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), layers, (unsigned)n);
   return true;
}

bool openxr_slot_acquire(openxr_swapchains_t *sc, unsigned slot,
      unsigned *index)
{
   unsigned l;
   openxr_slot_t *s = &sc->slots[slot];
   if (!s->swapchains[0])
      return false;
   for (l = 0; l < s->layers; l++)
   {
      if (!s->acquired[l])
      {
         XrSwapchainImageAcquireInfo ai;
         ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
         ai.next = NULL;
         if (XR_FAILED(sc->AcquireSwapchainImage(s->swapchains[l], &ai,
                     &s->index[l])))
            return false;
         s->acquired[l] = true;
         s->waited[l]   = false;
      }
      if (!s->waited[l])
      {
         XrSwapchainImageWaitInfo wi;
         wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
         wi.next = NULL;
         /* Never stall the core, nor the XR thread behind the queue lock:
          * an image the compositor still reads is tried again next frame. */
         wi.timeout = 0;
         if (sc->WaitSwapchainImage(s->swapchains[l], &wi) != XR_SUCCESS)
            return false;
         s->waited[l] = true;
      }
      index[l] = s->index[l];
   }
   return true;
}

void openxr_slot_release(openxr_swapchains_t *sc, unsigned slot)
{
   unsigned l;
   bool released = true;
   XrSwapchainImageReleaseInfo ri;
   openxr_slot_t *s = &sc->slots[slot];
   if (!s->swapchains[0])
      return;
   for (l = 0; l < s->layers; l++)
      if (!s->acquired[l] || !s->waited[l])
         return;
   ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
   ri.next = NULL;
   for (l = 0; l < s->layers; l++)
   {
      if (XR_FAILED(sc->ReleaseSwapchainImage(s->swapchains[l], &ri)))
         released = false;
      s->acquired[l] = false;
      s->waited[l]   = false;
   }
   if (released)
      retro_atomic_store_release_int(&s->content, 1);
}

void openxr_slot_forget(openxr_swapchains_t *sc, unsigned slot)
{
   retro_atomic_store_release_int(&sc->slots[slot].content, 0);
}
