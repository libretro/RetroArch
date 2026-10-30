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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <compat/strl.h>
#include <encodings/utf.h>

#include "openxr_frame.h"

#include "../video_defines.h"
#include "../font_driver.h"
#include "../../msg_hash.h"

#include "../../runloop.h"
#include "../../verbosity.h"

/* The seqlock's copy holds the largest thing it publishes. */
#define OPENXR_SEQ_WORDS \
   ((OPENXR_WORDS(openxr_grabs_t) > OPENXR_WORDS(video_xr_quad_set_t)) \
    ? OPENXR_WORDS(openxr_grabs_t) : OPENXR_WORDS(video_xr_quad_set_t))
typedef char openxr_seq_fits[
      (   OPENXR_WORDS(openxr_tracked_t)    <= OPENXR_SEQ_WORDS
       && OPENXR_WORDS(openxr_screens_t)    <= OPENXR_SEQ_WORDS
       && OPENXR_WORDS(openxr_projection_t) <= OPENXR_SEQ_WORDS
       && OPENXR_WORDS(openxr_aims_t)       <= OPENXR_SEQ_WORDS)
      ? 1 : -1];

/* A latched seqlock: two copies of the words, one writer, any number
 * of readers. The writer stamps odd and stores copy 0, stamps even and
 * stores copy 1; a reader reads the copy the stamp says is not being
 * stored - copy 1 while it is odd, copy 0 while it is even - so a
 * writer stopped part way through holds no reader up. A reader starts
 * over only when the writer has got through a whole copy during its
 * read, which is the writer going on, never waiting for it. */
static void openxr_seq_store(retro_atomic_int_t *words,
      const int *tmp, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      retro_atomic_store_relaxed_int(&words[i], tmp[i]);
}

static void openxr_seq_publish(retro_atomic_int_t *seq,
      retro_atomic_int_t *words, const void *src, size_t len)
{
   int tmp[OPENXR_SEQ_WORDS];
   size_t n = (len + sizeof(int) - 1) / sizeof(int);
   int s    = retro_atomic_load_relaxed_int(seq);

   tmp[n - 1] = 0;
   memcpy(tmp, src, len);
   retro_atomic_store_release_int(seq, (int)((unsigned)s + 1u));
   retro_atomic_thread_fence_release();
   openxr_seq_store(words, tmp, n);
   retro_atomic_store_release_int(seq, (int)((unsigned)s + 2u));
   retro_atomic_thread_fence_release();
   openxr_seq_store(words + n, tmp, n);
}

static void openxr_seq_read(retro_atomic_int_t *seq,
      retro_atomic_int_t *words, void *dst, size_t len)
{
   int tmp[OPENXR_SEQ_WORDS];
   size_t n = (len + sizeof(int) - 1) / sizeof(int);
   for (;;)
   {
      size_t i;
      int s1                         = retro_atomic_load_acquire_int(seq);
      const retro_atomic_int_t *copy = words + ((s1 & 1) ? n : 0);
      for (i = 0; i < n; i++)
         tmp[i] = retro_atomic_load_relaxed_int(
               (retro_atomic_int_t*)&copy[i]);
      retro_atomic_thread_fence_acquire();
      if (retro_atomic_load_relaxed_int(seq) == s1)
         break;
   }
   memcpy(dst, tmp, len);
}

static void openxr_frame_publish_tracked(openxr_frame_t *f)
{
   openxr_seq_publish(&f->tracked_seq, f->tracked_words,
         &f->tracked, sizeof(f->tracked));
}

static void openxr_frame_read_tracked(openxr_frame_t *f,
      openxr_tracked_t *out)
{
   openxr_seq_read(&f->tracked_seq, f->tracked_words,
         out, sizeof(*out));
}

void openxr_frame_tick_notify(openxr_frame_t *f)
{
   if (f->tick_ready)
      retro_eventcount_notify(&f->tick);
}

/* Pixels per radian across one eye, from the first located views. */
static void openxr_frame_measure(openxr_frame_t *f, XrTime time)
{
   float fov;
   uint32_t n = 0;
   XrViewLocateInfo li;
   XrViewState vs;
   XrView views[2];

   li.type                  = XR_TYPE_VIEW_LOCATE_INFO;
   li.next                  = NULL;
   li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
   li.displayTime           = time;
   li.space                 = f->session->local_space;
   vs.type = XR_TYPE_VIEW_STATE;
   vs.next = NULL;
   views[0].type            = XR_TYPE_VIEW;
   views[0].next            = NULL;
   views[1].type            = XR_TYPE_VIEW;
   views[1].next            = NULL;
   if (     XR_FAILED(f->session->LocateViews(f->session->session, &li,
                  &vs, 2, &n, views))
         || !n)
      return;
   fov = views[0].fov.angleRight - views[0].fov.angleLeft;
   if (fov < 0.1f || !f->rt->rec_width)
      return;
   f->tracked.px_per_rad = (float)f->rt->rec_width / fov;
   openxr_frame_publish_tracked(f);
   RARCH_LOG("[OpenXR] %u pixels across %.0f degrees per eye.\n",
         (unsigned)f->rt->rec_width, fov * 57.29578f);
}

static void openxr_frame_error(openxr_frame_t *f,
      const char *fn, XrResult res)
{
   if (!f->frame_failed)
   {
      f->frame_failed = true;
      RARCH_ERR("[OpenXR] %s failed (%d).\n", fn, (int)res);
   }
   /* A runtime may report the loss here without an event. */
   if (res == XR_ERROR_SESSION_LOST || res == XR_ERROR_INSTANCE_LOST)
      openxr_session_end(f->session, res == XR_ERROR_INSTANCE_LOST);
}

/* The swapchains slot shows and their size and layer count: its own
 * once released, else those a resize replaced; NULL for none. */
static const XrSwapchain *openxr_frame_chains(openxr_slot_t *slot,
      unsigned *dims, unsigned *layers)
{
   if (retro_atomic_load_acquire_int(&slot->content))
   {
      *dims   = slot->dims;
      *layers = slot->layers;
      return slot->swapchains[0] ? slot->swapchains : NULL;
   }
   *dims   = slot->old_dims;
   *layers = (unsigned)retro_atomic_load_acquire_int(&slot->old_layers);
   return (*layers && slot->old[0]) ? slot->old : NULL;
}

/* Every quad of slot's item at pose and g's size, both eyes of a stereo
 * screen; a fixed slot is no item. */
static void openxr_frame_hold_slot(video_xr_quad_set_t *set, unsigned slot,
      const video_xr_pose_t *pose, const video_xr_grab_t *g)
{
   unsigned i;
   if (set->slots[slot].fixed)
      return;
   for (i = 0; i < set->num_quads; i++)
      if (set->quads[i].slot == slot)
      {
         set->quads[i].pose   = *pose;
         set->quads[i].width  = g->width;
         set->quads[i].height = g->height;
      }
}

/* Edit mode, in the screens' frame: a held item goes where the hand's
 * aim at time puts it, kept while untracked, snapped and hinged as the
 * main thread does; a let-go one stays until quads planned at it show
 * it. */
static void openxr_frame_hold(openxr_frame_t *f, XrTime time,
      video_xr_quad_set_t *set)
{
   unsigned h, s;
   video_xr_pose_t inv;
   unsigned holding = 0;
   XrSpace space = f->screens.view
      ? f->session->view_space : f->session->local_space;
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
   {
      const video_xr_grab_t *g = &f->grabs.let_go[s];
      if (     g->state == VIDEO_XR_GRAB_RELEASED
            && (int)(set->slots[s].edit_seq - g->release_seq) < 0)
         openxr_frame_hold_slot(set, s, &g->last, g);
   }
   for (h = 0; h < 2; h++)
      if (     f->grabs.hand[h].state == VIDEO_XR_GRAB_HELD
            && f->grabs.hand[h].slot < VIDEO_XR_MAX_SLOTS)
         holding |= 1u << f->grabs.hand[h].slot;
   video_xr_pose_invert(&f->screens.pose, &inv);
   f->hinged = 0;
   for (h = 0; h < 2; h++)
   {
      video_xr_pose_t aim;
      const video_xr_grab_t *g = &f->grabs.hand[h];
      openxr_held_t *held      = &f->held[h];
      if (held->state != g->state || held->slot != g->slot)
      {
         held->pose   = g->last;
         held->slot   = g->slot;
         held->state  = g->state;
         held->hinged = false;
      }
      if (g->state != VIDEO_XR_GRAB_HELD || g->slot >= VIDEO_XR_MAX_SLOTS)
         continue;
      if (f->aim && time && f->aim(f->extra_user, h, space, time, &aim))
      {
         const video_xr_quad_set_t *targets = f->grabs.snap ? set : NULL;
         video_xr_compose(&inv, &aim, &aim);
         video_xr_grab_hinge(g, &aim, targets, holding,
               &f->grabs.start[h], &held->pose, NULL);
      }
      openxr_frame_hold_slot(set, g->slot, &held->pose, g);
      /* The hinge the main thread marks, nothing of the frame's own:
       * a wobble between its polls never flips the line. */
      held->hinged = video_xr_grab_marked(g, &held->pose, set, holding,
            &held->hinge);
      if (held->hinged)
      {
         video_xr_pose_t at;
         video_xr_vec3_t dir, flat;
         video_xr_hinge_t *k = &f->hinges[h];
         *k                  = held->hinge;
         video_xr_pose_identity(&at);
         at.position = k->point;
         video_xr_compose(&f->screens.pose, &at, &at);
         k->point = at.position;
         dir      = k->dir;
         flat     = k->flat;
         video_xr_rotate(&f->screens.pose.orientation, &dir, &k->dir);
         video_xr_rotate(&f->screens.pose.orientation, &flat, &k->flat);
         f->hinged |= 1u << h;
      }
   }
}

static bool openxr_frame_head(openxr_frame_t *f, XrTime time,
      video_xr_pose_t *head);

/* This frame's quads, holds applied, where the screens are, into
 * f->shown; layers of those whose slot shows an image. Runtimes draw
 * only a quad's front, so with the head behind one its back, the laser
 * image's grey square, takes its layer, turned to the head. No slot
 * changes under it: slots change under the graphics lock, or on this
 * thread where the host has none. */
static unsigned openxr_frame_layers(openxr_frame_t *f, XrTime time,
      XrCompositionLayerQuad *layers,
      const XrCompositionLayerBaseHeader **ptrs)
{
   unsigned i;
   video_xr_pose_t head;
   unsigned n                 = 0;
   bool seen                  = false;
   video_xr_quad_set_t *quads = &f->shown;
   openxr_seq_read(&f->quads_seq, f->quads_words,
         quads, sizeof(*quads));
   openxr_frame_hold(f, time, quads);
   f->backs      = 0;
   f->menu_layer = -1;
   /* Follow Head's space is the head's own. Backs are for edit mode,
    * where screens are turned around; outside it nothing changes. */
   if (f->cursor && f->grabs.editing)
   {
      if (f->screens.view)
      {
         video_xr_pose_identity(&head);
         seen = true;
      }
      else
         seen = openxr_frame_head(f, time, &head);
   }
   for (i = 0; i < quads->num_quads; i++)
   {
      unsigned dims, count;
      const XrSwapchain *chains;
      video_xr_pose_t pose;
      bool back;
      video_xr_quad_t *q        = &quads->quads[i];
      openxr_slot_t *slot       = &f->sc->slots[q->slot];
      XrCompositionLayerQuad *l = &layers[n];
      video_xr_compose(&f->screens.pose, &q->pose, &q->pose);
      if (     !(chains = openxr_frame_chains(slot, &dims, &count))
            || q->layer >= count)
         continue;
      back = seen && video_xr_quad_behind(q, &head.position);
      pose = q->pose;
      l->type                        = XR_TYPE_COMPOSITION_LAYER_QUAD;
      l->next                        = NULL;
      l->subImage.imageRect.offset.x = 0;
      l->subImage.imageRect.offset.y = 0;
      l->subImage.imageArrayIndex    = 0;
      /* The UI is drawn over transparent black: premultiplied. */
      l->layerFlags    = (q->kind == VIDEO_XR_QUAD_MENU && !back)
         ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
      l->space         = f->screens.view
         ? f->session->view_space : f->session->local_space;
      l->eyeVisibility = (q->eye == VIDEO_XR_EYE_LEFT)
         ? XR_EYE_VISIBILITY_LEFT
         : ((q->eye == VIDEO_XR_EYE_RIGHT)
               ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH);
      if (back)
      {
         video_xr_turn_back(&pose);
         f->backs                           |= 1u << i;
         l->subImage.swapchain               = f->cursor;
         l->subImage.imageRect.offset.x      =
            VIDEO_XR_BACK_X + VIDEO_XR_BACK_MARGIN;
         l->subImage.imageRect.offset.y      =
            VIDEO_XR_BACK_Y + VIDEO_XR_BACK_MARGIN;
         l->subImage.imageRect.extent.width  = VIDEO_XR_BACK_DIM;
         l->subImage.imageRect.extent.height = VIDEO_XR_BACK_DIM;
      }
      else
      {
         l->subImage.swapchain               = chains[q->layer];
         l->subImage.imageRect.extent.width  = (int32_t)VIDEO_SCALE_W(dims);
         l->subImage.imageRect.extent.height = (int32_t)VIDEO_SCALE_H(dims);
      }
      l->pose.orientation.x = pose.orientation.x;
      l->pose.orientation.y = pose.orientation.y;
      l->pose.orientation.z = pose.orientation.z;
      l->pose.orientation.w = pose.orientation.w;
      l->pose.position.x    = pose.position.x;
      l->pose.position.y    = pose.position.y;
      l->pose.position.z    = pose.position.z;
      l->size.width         = q->width;
      l->size.height        = q->height;
      if (q->kind == VIDEO_XR_QUAD_MENU)
         f->menu_layer = (int)n;
      ptrs[n++]             = (const XrCompositionLayerBaseHeader*)l;
   }
   return n;
}

/* Whether what was drawn in the track space numbered seq shows in t:
 * drawn in it, or in one a recenter's remake carried to it. */
static bool openxr_frame_track_shows(const openxr_track_t *t, int seq)
{
   return t && (   seq == t->seq
                || (   t->carry_to == t->seq
                    && seq >= t->carry_from && seq < t->carry_to));
}

/* The published projection, if its slot has content and it was drawn
 * in the frame's track space or one carried to it. */
static bool openxr_frame_projection(openxr_frame_t *f,
      XrCompositionLayerProjection *layer,
      XrCompositionLayerProjectionView *views)
{
   unsigned eye, w, h;
   unsigned dims             = 0;
   unsigned count            = 0;
   const XrSwapchain *chains = NULL;
   openxr_projection_t p;
   openxr_slot_t *slot       = &f->sc->slots[OPENXR_PROJECTION_SLOT];
   const openxr_track_t *t   = f->track;
   openxr_seq_read(&f->proj_seq, f->proj_words, &p, sizeof(p));
   if (     !p.active
         || !(chains = openxr_frame_chains(slot, &dims, &count))
         || count < 2
         || !openxr_frame_track_shows(t, p.track_seq))
      return false;
   w = MIN(p.width,  VIDEO_SCALE_W(dims));
   h = MIN(p.height, VIDEO_SCALE_H(dims));
   if (!w || !h)
      return false;
   for (eye = 0; eye < 2; eye++)
   {
      views[eye].type                             =
         XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
      views[eye].next                             = NULL;
      views[eye].subImage.imageRect.offset.x      = 0;
      views[eye].subImage.imageRect.offset.y      = 0;
      views[eye].subImage.imageArrayIndex         = 0;
      views[eye].pose                             = p.pose[eye];
      views[eye].fov                              = p.fov[eye];
      views[eye].subImage.swapchain               = chains[eye];
      views[eye].subImage.imageRect.extent.width  = (int32_t)w;
      views[eye].subImage.imageRect.extent.height = (int32_t)h;
   }
   layer->type       = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
   layer->next       = NULL;
   layer->layerFlags = 0;
   layer->space      = t->space;
   layer->viewCount  = 2;
   layer->views      = views;
   return true;
}

/* The head in LOCAL at time, when the runtime finds it. */
static bool openxr_frame_head(openxr_frame_t *f, XrTime time,
      video_xr_pose_t *head)
{
   XrSpaceLocation loc;
   XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
      | XR_SPACE_LOCATION_POSITION_VALID_BIT;

   loc.type = XR_TYPE_SPACE_LOCATION;
   loc.next = NULL;
   if (     XR_FAILED(f->session->LocateSpace(f->session->view_space,
               f->session->local_space, time, &loc))
         || (loc.locationFlags & valid) != valid)
      return false;
   head->orientation.x = loc.pose.orientation.x;
   head->orientation.y = loc.pose.orientation.y;
   head->orientation.z = loc.pose.orientation.z;
   head->orientation.w = loc.pose.orientation.w;
   head->position.x    = loc.pose.position.x;
   head->position.y    = loc.pose.position.y;
   head->position.z    = loc.pose.position.z;
   return true;
}

/* The head's position and heading at time become the anchor. Published
 * before it is counted: a host that reads the count, then the anchor,
 * gets the moved one. */
static void openxr_frame_recenter(openxr_frame_t *f, XrTime time)
{
   video_xr_pose_t head, anchor;
   if (!openxr_frame_head(f, time, &head))
   {
      RARCH_WARN("[OpenXR] Recenter: the headset is not tracked.\n");
      return;
   }
   if (!video_xr_anchor_from_head(&head, &anchor))
      return;
   f->tracked.anchor = anchor;
   openxr_frame_publish_tracked(f);
   retro_atomic_fetch_add_int(&f->recenters, 1);
   RARCH_LOG("[OpenXR] Recentered at %.2f, %.2f, %.2f.\n",
         anchor.position.x, anchor.position.y, anchor.position.z);
}

/* Where Recenter puts the screens: a seated world's origin while
 * tracked content holds one, so a recenter moves both in one frame;
 * else the anchor, by the frame's track space. */
static void openxr_frame_screens_anchor(openxr_frame_t *f,
      video_xr_pose_t *out)
{
   const openxr_track_t *t = f->track;
   /* Lives until the session goes; core_vr_content's sample remakes it. */
   if (!t || !t->local)
   {
      *out = f->tracked.anchor;
      return;
   }
   out->orientation.x = t->origin.orientation.x;
   out->orientation.y = t->origin.orientation.y;
   out->orientation.z = t->origin.orientation.z;
   out->orientation.w = t->origin.orientation.w;
   out->position.x    = t->origin.position.x;
   out->position.y    = t->origin.position.y;
   out->position.z    = t->origin.position.z;
}

/* Where the screens are for the frame shown at time, published for the
 * laser: Fixed in Room where Recenter puts them; Follow Head in VIEW
 * space; Lazy Follow at an anchor that stays until the head turns well
 * away, then glides back in front of it, and that each recenter puts
 * where Recenter puts them. */
static void openxr_frame_place_screens(openxr_frame_t *f, XrTime time)
{
   video_xr_pose_t at;
   bool held;
   int recenters = retro_atomic_load_acquire_int(&f->recenters);
   int placement = retro_atomic_load_acquire_int(&f->placement);
   int grabs     = retro_atomic_load_acquire_int(&f->grabs_seq);
   /* Outside edit mode nothing is published: no copy. */
   if (grabs != f->grabs_read)
   {
      openxr_seq_read(&f->grabs_seq, f->grabs_words, &f->grabs,
            sizeof(f->grabs));
      f->grabs_read = grabs;
   }
   held = f->grabs.hand[0].state == VIDEO_XR_GRAB_HELD
      || f->grabs.hand[1].state == VIDEO_XR_GRAB_HELD;
   openxr_frame_screens_anchor(f, &at);
   f->screens.view = (placement == VIDEO_OPENXR_PLACEMENT_FOLLOW);
   if (placement != VIDEO_OPENXR_PLACEMENT_LAZY)
   {
      f->lazy_valid = false;
      if (f->screens.view)
         video_xr_pose_identity(&f->screens.pose);
      else
         f->screens.pose = at;
   }
   else
   {
      video_xr_pose_t head, target;
      if (!f->lazy_valid)
      {
         f->lazy           = at;
         f->lazy_valid     = true;
         f->lazy_gliding   = false;
         f->lazy_recenters = recenters;
      }
      else if (recenters != f->lazy_recenters)
      {
         /* Held until a seated world is made at the new anchor: its
          * origin is a copy of the anchor. */
         if (!memcmp(&at, &f->tracked.anchor, sizeof(at)))
         {
            f->lazy           = at;
            f->lazy_gliding   = false;
            f->lazy_recenters = recenters;
         }
      }
      /* Edit mode holds the glide while anything is held. */
      else if (   !held
               && openxr_frame_head(f, time, &head)
               && video_xr_anchor_from_head(&head, &target))
         f->lazy_gliding = video_xr_lazy_follow(&f->lazy, &target,
               (f->lazy_time > 0 && time > f->lazy_time)
               ? (double)(time - f->lazy_time) / 1000000000.0 : 0.0,
               f->lazy_gliding);
      f->lazy_time    = time;
      f->screens.pose = f->lazy;
   }
   openxr_seq_publish(&f->screens_seq, f->screens_words,
         &f->screens, sizeof(f->screens));
}

/* Every interval headset frames, a tick for the core. */
static void openxr_frame_tick(openxr_frame_t *f)
{
   unsigned interval = (unsigned)retro_atomic_load_acquire_int(
         &f->interval);
   if (interval != f->tick_interval)
   {
      f->tick_interval = interval;
      f->tick_count    = 0;
   }
   if (!interval || ++f->tick_count < interval)
      return;
   f->tick_count = 0;
   retro_atomic_fetch_add_int(&f->tick_seq, 1);
   openxr_frame_tick_notify(f);
}

/* Performance Boost on: CPU boost, GPU sustained high. Off: both at
 * sustained high, the level a runtime holds when never asked. */
static void openxr_frame_ask_perf(openxr_frame_t *f)
{
   XrResult cpu = XR_SUCCESS, gpu = XR_SUCCESS;
   retro_time_t now;
   int want = video_xr_perf_boost() ? 1 : 0;
   bool changed = want != f->asked_perf;
   if (     !f->rt->perf_settings
         || !f->rt->PerfSettingsSetPerformanceLevelEXT)
      return;
   if (!want && f->asked_perf < 0)
   {
      f->asked_perf = 0;
      f->perf_applied = 3;
      return;
   }
   if (changed)
   {
      f->perf_applied = 0;
      f->perf_retry_usec = 0;
   }
   if (f->perf_applied == 3)
      return;
   now = cpu_features_get_time_usec();
   if (now < f->perf_retry_usec)
      return;
   f->asked_perf = want;
   if (!(f->perf_applied & 1))
   {
      cpu = f->rt->PerfSettingsSetPerformanceLevelEXT(f->session->session,
         XR_PERF_SETTINGS_DOMAIN_CPU_EXT, want
         ? XR_PERF_SETTINGS_LEVEL_BOOST_EXT
         : XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
      if (XR_SUCCEEDED(cpu))
         f->perf_applied |= 1;
   }
   if (!(f->perf_applied & 2))
   {
      gpu = f->rt->PerfSettingsSetPerformanceLevelEXT(f->session->session,
         XR_PERF_SETTINGS_DOMAIN_GPU_EXT,
         XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
      if (XR_SUCCEEDED(gpu))
         f->perf_applied |= 2;
   }
   f->perf_retry_usec = now + 1000000;
   if (f->perf_applied == 3)
      RARCH_LOG("[OpenXR] Performance: %s.\n", want
            ? "CPU boost, GPU sustained high"
            : "CPU and GPU sustained high");
   else if (changed)
      RARCH_WARN("[OpenXR] Performance levels refused (CPU %d, GPU %d).\n",
            (int)cpu, (int)gpu);
}

/* The rate the drawing thread wants, retained after runtime acceptance. */
static void openxr_frame_ask_rate(openxr_frame_t *f)
{
   XrResult res;
   retro_time_t now;
   float hz;
   bool changed;
   int bits = retro_atomic_load_acquire_int(&f->want_rate);
   memcpy(&hz, &bits, sizeof(hz));
   if (!f->rt->refresh_ext || !f->rt->RequestDisplayRefreshRateFB)
      return;
   changed = hz != f->requested_rate;
   if (changed)
   {
      f->requested_rate = hz;
      f->rate_retry_usec = 0;
   }
   if (hz <= 0.0f || hz == f->asked_rate)
      return;
   now = cpu_features_get_time_usec();
   if (now < f->rate_retry_usec)
      return;
   res           = f->rt->RequestDisplayRefreshRateFB(f->session->session, hz);
   if (XR_SUCCEEDED(res))
      f->asked_rate = hz;
   f->rate_retry_usec = now + 1000000;
   if (changed || XR_SUCCEEDED(res))
      RARCH_LOG("[OpenXR] Asked the headset for %.2f Hz (%d).\n", hz,
            (int)res);
}

/* xrWaitFrame and what every headset frame keeps, then xrBeginFrame
 * under the graphics lock, which keep_lock leaves held for the end. */
static bool openxr_frame_wait_begin(openxr_frame_t *f,
      openxr_frame_begun_t *b, bool keep_lock)
{
   XrResult res;
   int session_state;
   XrFrameWaitInfo wait_info;
   XrFrameState state;
   XrFrameBeginInfo begin_info;

   memset(b, 0, sizeof(*b));
   wait_info.type = XR_TYPE_FRAME_WAIT_INFO;
   wait_info.next = NULL;
   state.type = XR_TYPE_FRAME_STATE;
   state.next = NULL;
   if (XR_FAILED(res = f->WaitFrame(f->session->session, &wait_info, &state)))
   {
      openxr_frame_error(f, "xrWaitFrame", res);
      return false;
   }
   b->waited       = true;
   b->display_time = state.predictedDisplayTime;
   f->track        = openxr_session_take_track(f->session);
   b->period       = state.predictedDisplayPeriod;
   session_state = retro_atomic_load_acquire_int(&f->session->state);
   /* A headset that is not worn reports periods that are not its own. */
   if (    (   session_state == XR_SESSION_STATE_VISIBLE
            || session_state == XR_SESSION_STATE_FOCUSED)
         && video_xr_period_add(&f->period,
            (int64_t)state.predictedDisplayPeriod))
   {
      retro_atomic_store_release_int(&f->period_ns,
            (int)f->period.published);
      RARCH_LOG("[OpenXR] The headset runs at %.2f Hz.\n",
            1000000000.0 / (double)f->period.published);
   }
   openxr_frame_ask_rate(f);
   openxr_frame_ask_perf(f);
   f->tracked.predicted_time = state.predictedDisplayTime;
   /* The world moves at the next core frame, so the screens wait too. */
   if (!f->repeating && retro_atomic_load_acquire_int(&f->recenter))
   {
      retro_atomic_store_release_int(&f->recenter, 0);
      openxr_frame_recenter(f, state.predictedDisplayTime);
   }
   openxr_frame_publish_tracked(f);
   if (f->tracked.px_per_rad <= 0.0f)
      openxr_frame_measure(f, state.predictedDisplayTime);

   if (keep_lock && f->thread_hooks.fill)
      f->thread_hooks.fill(f->thread_hooks.user, state.predictedDisplayTime);
   /* The core is woken after the host takes what it drew: a frame it
    * finished first would be shown a headset frame early. An in-line
    * host's frames are the core's own: no ticks. */
   if (!f->inline_host)
      openxr_frame_tick(f);

   begin_info.type = XR_TYPE_FRAME_BEGIN_INFO;
   begin_info.next = NULL;
   openxr_session_lock(f->session);
   if (XR_FAILED(res = f->BeginFrame(f->session->session, &begin_info)))
   {
      openxr_session_unlock(f->session);
      openxr_frame_error(f, "xrBeginFrame", res);
      return false;
   }
   if (!keep_lock)
      openxr_session_unlock(f->session);
   session_state = retro_atomic_load_acquire_int(&f->session->state);
   /* Layers only while the headset shows the session. */
   b->render = state.shouldRender
      && (   session_state == XR_SESSION_STATE_VISIBLE
          || session_state == XR_SESSION_STATE_FOCUSED);
   b->begun  = true;
   return true;
}

/* At most the runtime's layer count: the last screens go first, so the
 * menu and the host's layers over it stay. The quads are ptrs[first]
 * up to ptrs[end], as openxr_frame_layers made them; returns the count
 * kept. */
static uint32_t openxr_frame_fit(const openxr_frame_t *f,
      const XrCompositionLayerBaseHeader **ptrs, uint32_t count,
      uint32_t first, uint32_t end)
{
   while (count > f->rt->max_layers && end > first)
   {
      if ((int)(--end - first) == f->menu_layer)
         continue;
      memmove(&ptrs[end], &ptrs[end + 1],
            (count - end - 1) * sizeof(*ptrs));
      count--;
   }
   return MIN(count, f->rt->max_layers);
}

/* The layers and xrEndFrame; locked: the begin kept the lock. */
static void openxr_frame_submit(openxr_frame_t *f,
      const openxr_frame_begun_t *b, bool locked)
{
   XrResult res;
   XrFrameEndInfo end_info;
   XrCompositionLayerProjection proj;
   XrCompositionLayerProjectionView proj_views[2];
   XrCompositionLayerQuad layers[VIDEO_XR_MAX_QUADS];
   const XrCompositionLayerBaseHeader *ptrs[1 + VIDEO_XR_MAX_QUADS
      + OPENXR_MAX_EXTRA_LAYERS];

   end_info.type                 = XR_TYPE_FRAME_END_INFO;
   end_info.next                 = NULL;
   end_info.layerCount           = 0;
   end_info.displayTime          = b->display_time;
   end_info.environmentBlendMode = f->rt->blend_mode;
   if (!locked)
      openxr_session_lock(f->session);
   openxr_frame_place_screens(f, b->display_time);
   if (b->render)
   {
      uint32_t first, end;
      if (openxr_frame_projection(f, &proj, proj_views))
         ptrs[end_info.layerCount++] =
            (const XrCompositionLayerBaseHeader*)&proj;
      first               = end_info.layerCount;
      end                 = first + openxr_frame_layers(f, b->display_time,
            layers, ptrs + first);
      end_info.layerCount = end;
      if (f->extra_layers)
      {
         unsigned i, n;
         unsigned under = 0;
         unsigned cap   = (f->rt->max_layers > end)
            ? (unsigned)(f->rt->max_layers - end) : 0;
         if (cap < OPENXR_KEPT_EXTRA_LAYERS)
            cap = OPENXR_KEPT_EXTRA_LAYERS;
         if (cap > OPENXR_MAX_EXTRA_LAYERS)
            cap = OPENXR_MAX_EXTRA_LAYERS;
         n = f->extra_layers(f->extra_user, b->display_time, &f->shown,
               f->screens.view
               ? f->session->view_space : f->session->local_space,
               ptrs + end, cap, &under);
         end_info.layerCount += n;
         /* The first under go before the quads. */
         for (i = 0; i < under && i < n; i++, first++, end++)
         {
            const XrCompositionLayerBaseHeader *l = ptrs[end];
            memmove(&ptrs[first + 1], &ptrs[first],
                  (end - first) * sizeof(*ptrs));
            ptrs[first] = l;
         }
      }
      end_info.layerCount = openxr_frame_fit(f, ptrs,
            end_info.layerCount, first, end);
   }
   end_info.layers = end_info.layerCount ? ptrs : NULL;
   res = f->EndFrame(f->session->session, &end_info);
   openxr_swapchains_drop_old(f->sc);
   openxr_session_unlock(f->session);
   if (XR_FAILED(res))
      openxr_frame_error(f, "xrEndFrame", res);
   else
      f->frame_failed = false;
}

/* One headset frame on the XR thread. xrWaitFrame paces this thread at
 * the headset's rate, and the core too while the headset paces it.
 * False when it could not wait for one. */
static bool openxr_frame_run(openxr_frame_t *f)
{
   openxr_frame_begun_t b;
   if (openxr_frame_wait_begin(f, &b, true))
      openxr_frame_submit(f, &b, true);
   return b.waited;
}

/* Whose xrPollEvent it is: the XR thread's, or, while it is parked,
 * the drawing thread's, which takes POLLING for each poll it makes. */
#define OPENXR_POLLER_XR      0
#define OPENXR_POLLER_HOST    1
#define OPENXR_POLLER_POLLING 2
#define OPENXR_EVENTS_HELD    8

/* What the drawing thread fetched while this thread was parked. */
static void openxr_frame_take_events(openxr_frame_t *f)
{
   openxr_event_t e;
   while (retro_spsc_read(&f->events, &e, sizeof(e)) == sizeof(e))
      openxr_session_apply(f->session, &e);
}

/* With nothing to do until the runtime has news, nor a way to wait on
 * it: parked until the drawing thread fetches an event, or a stop, and
 * the polling its own again. */
static void openxr_frame_park(openxr_frame_t *f)
{
   retro_atomic_store_release_int(&f->poller, OPENXR_POLLER_HOST);
   for (;;)
   {
      int key;
      int who = retro_atomic_load_acquire_int(&f->poller);
      if (who == OPENXR_POLLER_XR)
         return;
      if (     who == OPENXR_POLLER_HOST
            && retro_atomic_load_acquire_int(&f->quit)
            && retro_atomic_cas_int(&f->poller, OPENXR_POLLER_HOST,
               OPENXR_POLLER_XR))
         return;
      key = retro_eventcount_prepare_wait(&f->idle);
      who = retro_atomic_load_acquire_int(&f->poller);
      if (     who == OPENXR_POLLER_XR
            || (   who == OPENXR_POLLER_HOST
                && retro_atomic_load_acquire_int(&f->quit)))
         retro_eventcount_cancel_wait(&f->idle);
      else
         retro_eventcount_commit_wait(&f->idle, key);
   }
}

void openxr_frame_host_poll(openxr_frame_t *f)
{
   openxr_event_t e;
   bool fetched = false;
   if (!retro_atomic_cas_int(&f->poller, OPENXR_POLLER_HOST,
            OPENXR_POLLER_POLLING))
      return;
   while (   retro_spsc_write_avail(&f->events) >= sizeof(e)
          && openxr_session_next_event(f->session, &e))
   {
      retro_spsc_write(&f->events, &e, sizeof(e));
      fetched = true;
      if (e.kind == OPENXR_EVENT_INSTANCE_LOST)
         break;
   }
   retro_atomic_store_release_int(&f->poller,
         fetched ? OPENXR_POLLER_XR : OPENXR_POLLER_HOST);
   /* against a stop between the poll and this: see openxr_frame_stop */
   retro_atomic_thread_fence_seq_cst();
   if (fetched || retro_atomic_load_acquire_int(&f->quit))
      retro_eventcount_notify(&f->idle);
}

static void openxr_frame_thread(void *data)
{
   openxr_frame_t *f = (openxr_frame_t*)data;
   if (f->thread_hooks.start)
   {
      bool started = f->thread_hooks.start(f->thread_hooks.user);
      retro_atomic_store_release_int(&f->thread_started, started ? 1 : -1);
      openxr_frame_tick_notify(f);
      if (!started)
         goto end;
   }
   while (!retro_atomic_load_acquire_int(&f->quit))
   {
      openxr_frame_take_events(f);
      openxr_session_poll(f->session);
      if (!openxr_session_running(f->session) || !openxr_frame_run(f))
         openxr_frame_park(f);
   }
end:
   if (f->thread_hooks.stop)
      f->thread_hooks.stop(f->thread_hooks.user);
}

/* The rates XR_FB_display_refresh_rate lists for this session. */
static void openxr_frame_list_rates(openxr_frame_t *f)
{
   char s[256];
   uint32_t i;
   size_t len   = 0;
   uint32_t n   = 0;
   f->num_rates = 0;
   if (     !f->rt->refresh_ext
         || XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
               f->session->session, 0, &n, NULL))
         || !n)
      return;
   if (n > VIDEO_HEADSET_MAX_RATES)
   {
      /* The runtime takes no less room than it lists. */
      uint32_t all = n;
      float *tmp   = (float*)malloc(all * sizeof(*tmp));
      if (!tmp)
         return;
      if (XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
               f->session->session, all, &all, tmp)))
      {
         free(tmp);
         return;
      }
      n = (all < VIDEO_HEADSET_MAX_RATES) ? all : VIDEO_HEADSET_MAX_RATES;
      memcpy(f->rates, tmp, n * sizeof(*tmp));
      free(tmp);
      if (all > n)
         RARCH_WARN("[OpenXR] The headset offers %u rates; %u dropped.\n",
               (unsigned)all, (unsigned)(all - n));
   }
   else if (XR_FAILED(f->rt->EnumerateDisplayRefreshRatesFB(
            f->session->session, n, &n, f->rates)))
      return;
   f->num_rates = n;
   s[0]         = '\0';
   for (i = 0; i < n && len < sizeof(s); i++)
      len += snprintf(s + len, sizeof(s) - len, "%s%.2f",
            i ? ", " : "", f->rates[i]);
   RARCH_LOG("[OpenXR] The headset offers %s Hz.\n", s);
}

bool openxr_frame_load(openxr_frame_t *f, const openxr_runtime_t *rt,
      openxr_session_t *session, openxr_swapchains_t *sc)
{
   f->rt      = rt;
   f->session = session;
   f->sc      = sc;
   if (!f->tick_ready)
   {
      if (!retro_eventcount_init(&f->tick))
         return false;
      f->tick_ready = true;
   }
   if (!f->idle_ready)
   {
      if (!retro_eventcount_init(&f->idle))
         return false;
      if (!retro_spsc_init(&f->events,
               OPENXR_EVENTS_HELD * sizeof(openxr_event_t)))
      {
         retro_eventcount_free(&f->idle);
         return false;
      }
      f->idle_ready = true;
   }
   return OPENXR_FN(rt, f, WaitFrame)
       && OPENXR_FN(rt, f, BeginFrame)
       && OPENXR_FN(rt, f, EndFrame);
}

void openxr_frame_deinit(openxr_frame_t *f)
{
   openxr_frame_stop(f);
   if (f->inline_host)
      openxr_frame_stop_inline(f);
   if (f->tick_ready)
      retro_eventcount_free(&f->tick);
   f->tick_ready = false;
   if (f->idle_ready)
   {
      retro_spsc_free(&f->events);
      retro_eventcount_free(&f->idle);
   }
   f->idle_ready = false;
}

/* A new session's frame loop holds nothing, whatever the input does:
 * its own copy is cleared and what is published now is passed over,
 * the main thread staying the one writer of the grabs. */
static const openxr_grabs_t openxr_frame_no_grabs;
static const openxr_aims_t openxr_frame_no_aims;

void openxr_frame_session_created(openxr_frame_t *f)
{
   video_xr_pose_identity(&f->tracked.anchor);
   video_xr_pose_identity(&f->screens.pose);
   f->screens.view = 0;
   f->lazy_valid   = false;
   f->track        = NULL;
   openxr_frame_publish_tracked(f);
   openxr_seq_publish(&f->screens_seq, f->screens_words,
         &f->screens, sizeof(f->screens));
   f->grabs      = openxr_frame_no_grabs;
   f->grabs_read = retro_atomic_load_acquire_int(&f->grabs_seq);
   openxr_seq_publish(&f->aims_seq, f->aims_words,
         &openxr_frame_no_aims, sizeof(openxr_frame_no_aims));
   retro_atomic_store_release_int(&f->recenter, 0);
   openxr_frame_list_rates(f);
}

static openxr_input_hooks_t openxr_frame_input_hooks;

void openxr_frame_set_input_hooks(const openxr_input_hooks_t *hooks)
{
   if (hooks)
      openxr_frame_input_hooks = *hooks;
   else
      memset(&openxr_frame_input_hooks, 0,
            sizeof(openxr_frame_input_hooks));
}

void openxr_frame_input_attach(openxr_frame_t *f)
{
   openxr_input_handles_t h;
   const openxr_input_hooks_t *in = &openxr_frame_input_hooks;
   if (f->input_attached || !in->session_created)
      return;
   memset(&h, 0, sizeof(h));
   h.frame            = f;
   h.instance         = f->rt->instance;
   h.session          = f->session->session;
   h.get_proc         = f->rt->GetInstanceProcAddr;
   h.frame_controller = f->rt->frame_controller;
   h.palm_pose        = f->rt->palm_pose;
   h.hand_tracking    = f->rt->hand_tracking;
   h.hand_data_source = f->rt->hand_data_source;
   f->extra_layers    = in->frame_layers;
   f->locate_hands    = in->locate_hands;
   f->aim             = in->aim;
   f->extra_user      = in->user;
   f->input_attached  = true;
   in->session_created(in->user, &h);
}

void openxr_frame_input_detach(openxr_frame_t *f)
{
   const openxr_input_hooks_t *in = &openxr_frame_input_hooks;
   if (!f->input_attached)
      return;
   f->input_attached = false;
   f->extra_layers   = NULL;
   f->locate_hands   = NULL;
   f->aim            = NULL;
   f->extra_user     = NULL;
   if (in->session_destroying)
      in->session_destroying(in->user);
}

bool openxr_frame_locate_hands(openxr_frame_t *f, XrSpace base, XrTime time,
      struct retro_vr_controller_poses *out)
{
   if (!f->locate_hands)
   {
      memset(out, 0, sizeof(*out));
      return false;
   }
   return f->locate_hands(f->extra_user, base, time, out);
}

XrSwapchain openxr_frame_cursor(const openxr_frame_t *f)
{
   return f->cursor;
}

unsigned openxr_frame_cursor_w(const openxr_frame_t *f)
{
   /* Not said: 4096, as openxr_runtime's max_dim. */
   unsigned max = f->rt->max_width ? f->rt->max_width : 4096;
   if (max >= VIDEO_XR_CURSOR_W)
      return VIDEO_XR_CURSOR_W;
   return (max >= VIDEO_XR_CURSOR_TAGGED_W)
      ? VIDEO_XR_CURSOR_TAGGED_W : VIDEO_XR_CURSOR_UNTAGGED_W;
}

const char *openxr_frame_cursor_parts(const openxr_frame_t *f)
{
   unsigned w = openxr_frame_cursor_w(f);
   if (w == VIDEO_XR_CURSOR_W)
      return "cut outlines and tags";
   return (w == VIDEO_XR_CURSOR_TAGGED_W) ? "tags" : "no tags";
}

bool openxr_frame_start(openxr_frame_t *f)
{
   if (f->inline_host || f->thread)
      return false;
   video_xr_period_init(&f->period);
   retro_atomic_store_release_int(&f->period_ns, 0);
   f->tick_count    = 0;
   f->tick_interval = 0;
   f->asked_rate    = 0.0f;
   f->requested_rate = 0.0f;
   f->rate_retry_usec = 0;
   f->asked_perf    = -1;
   f->perf_applied  = 0;
   f->perf_retry_usec = 0;
   f->frame_failed  = false;
   retro_atomic_store_release_int(&f->thread_started, 0);
   retro_atomic_store_release_int(&f->quit, 0);
   retro_atomic_store_release_int(&f->poller, OPENXR_POLLER_XR);
   retro_spsc_clear(&f->events);
   openxr_session_set_alive(f->session, true);
   if (!(f->thread = sthread_create(openxr_frame_thread, f)))
   {
      openxr_session_set_alive(f->session, false);
      return false;
   }
   if (f->thread_hooks.start)
   {
      while (!retro_atomic_load_acquire_int(&f->thread_started))
      {
         int key = retro_eventcount_prepare_wait(&f->tick);
         if (retro_atomic_load_acquire_int(&f->thread_started))
         {
            retro_eventcount_cancel_wait(&f->tick);
            break;
         }
         retro_eventcount_commit_wait(&f->tick, key);
      }
      if (retro_atomic_load_acquire_int(&f->thread_started) < 0)
      {
         openxr_frame_stop(f);
         return false;
      }
   }
   return true;
}

void openxr_frame_stop(openxr_frame_t *f)
{
   if (!f->thread)
      return;
   retro_atomic_store_release_int(&f->quit, 1);
   /* A parked thread is woken; one the drawing thread's poll holds is
    * woken by that poll (openxr_frame_host_poll). */
   retro_atomic_thread_fence_seq_cst();
   retro_eventcount_notify(&f->idle);
   sthread_join(f->thread);
   f->thread = NULL;
   /* Nothing shows the session's frames now: a driver that presents on
    * (a staged content load keeps it up) draws none for it. */
   openxr_session_set_alive(f->session, false);
}

bool openxr_frame_start_inline(openxr_frame_t *f)
{
   if (f->thread)
      return false;
   video_xr_period_init(&f->period);
   retro_atomic_store_release_int(&f->period_ns, 0);
   f->asked_rate     = 0.0f;
   f->requested_rate = 0.0f;
   f->rate_retry_usec = 0;
   f->asked_perf     = -1;
   f->perf_applied   = 0;
   f->perf_retry_usec = 0;
   f->frame_failed   = false;
   f->pace_anchor_ns = 0;
   f->inline_host    = true;
   openxr_session_set_alive(f->session, true);
   return true;
}

void openxr_frame_stop_inline(openxr_frame_t *f)
{
   f->inline_host = false;
   if (f->session)
      openxr_session_set_alive(f->session, false);
}

bool openxr_frame_begin(openxr_frame_t *f, openxr_frame_begun_t *b)
{
   memset(b, 0, sizeof(*b));
   if (!f->inline_host)
      return false;
   openxr_session_poll(f->session);
   if (!openxr_session_running(f->session))
      return false;
   return openxr_frame_wait_begin(f, b, false);
}

void openxr_frame_end(openxr_frame_t *f, const openxr_frame_begun_t *b)
{
   retro_time_t sleep_us;
   int64_t period;
   if (!f->inline_host)
      return;
   if (b->begun)
   {
      openxr_frame_submit(f, b, false);
      return;
   }
   /* No headset frame: its last period on the clock, or the XR
    * thread's 10 ms while the session is down. */
   period = (int64_t)retro_atomic_load_acquire_int(&f->period_ns);
   if (period <= 0)
      period = 10000000;
   sleep_us = runloop_pace_schedule(&f->pace_anchor_ns, period,
         cpu_features_get_time_usec());
   if (sleep_us > 0)
      retro_sleep_us((unsigned)sleep_us);
}

void openxr_frame_repeat(openxr_frame_t *f, unsigned n)
{
   unsigned i;
   f->repeating = true;
   for (i = 0; i < n; i++)
   {
      openxr_frame_begun_t b;
      openxr_frame_begin(f, &b);
      openxr_frame_end(f, &b);
   }
   f->repeating = false;
}

/* The runtime's own recenter moves LOCAL, which the anchor is in: the
 * screens go back straight ahead, and a hotkey request with them. */
void openxr_frame_local_changed(openxr_frame_t *f)
{
   retro_atomic_store_release_int(&f->recenter, 0);
   video_xr_pose_identity(&f->tracked.anchor);
   openxr_frame_publish_tracked(f);
   retro_atomic_fetch_add_int(&f->recenters, 1);
   RARCH_LOG("[OpenXR] Recentered by the runtime.\n");
}

void openxr_frame_clear_quads(openxr_frame_t *f)
{
   video_xr_quad_set_t none;
   memset(&none, 0, sizeof(none));
   openxr_seq_publish(&f->quads_seq, f->quads_words, &none, sizeof(none));
}

XrTime openxr_frame_predicted_time(openxr_frame_t *f)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   return t.predicted_time;
}

float openxr_frame_pixels_per_radian(openxr_frame_t *f)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   return t.px_per_rad;
}

float openxr_frame_refresh_rate(openxr_frame_t *f)
{
   int ns = retro_atomic_load_acquire_int(&f->period_ns);
   return (ns > 0) ? (float)(1000000000.0 / (double)ns) : 0.0f;
}

void openxr_frame_set_pacing(openxr_frame_t *f, unsigned interval)
{
   retro_atomic_store_release_int(&f->interval, (int)interval);
   if (!interval)
      f->pace_mode = 0;
}

/* The next tick after the last one seen, or false after timeout_ns. */
static bool openxr_frame_wait_tick(openxr_frame_t *f, int64_t timeout_ns)
{
   int seq;
   retro_time_t deadline = cpu_features_get_time_usec()
      + (retro_time_t)(timeout_ns / 1000);
   for (;;)
   {
      int key;
      retro_time_t left;
      seq = retro_atomic_load_acquire_int(&f->tick_seq);
      if (seq != f->tick_seen || !openxr_session_visible(f->session))
         break;
      left = deadline - cpu_features_get_time_usec();
      if (left <= 0)
         break;
      key = retro_eventcount_prepare_wait(&f->tick);
      seq = retro_atomic_load_acquire_int(&f->tick_seq);
      if (seq != f->tick_seen || !openxr_session_visible(f->session))
      {
         retro_eventcount_cancel_wait(&f->tick);
         break;
      }
      if (!retro_eventcount_commit_wait_timeout(&f->tick, key, left))
      {
         seq = retro_atomic_load_acquire_int(&f->tick_seq);
         break;
      }
   }
   if (seq == f->tick_seen)
      return false;
   f->tick_seen = seq;
   return true;
}

void openxr_frame_pace_skip(openxr_frame_t *f)
{
   if (!f->tick_ready)
      return;
   f->tick_seen = retro_atomic_load_acquire_int(&f->tick_seq);
}

void openxr_frame_pace_wait(openxr_frame_t *f)
{
   int64_t period = (int64_t)retro_atomic_load_acquire_int(&f->period_ns)
      * retro_atomic_load_acquire_int(&f->interval);
   /* No XR-thread ticks to wait on, and pace_anchor_ns is the idle
    * clock's. */
   if (f->inline_host || period <= 0 || !f->tick_ready)
      return;
   if (openxr_session_visible(f->session))
   {
      if (f->pace_mode != 1)
         RARCH_LOG("[OpenXR] Pacing on the headset's frames.\n");
      f->pace_mode = 1;
      if (openxr_frame_wait_tick(f, period * 2))
         f->tick_late = false;
      else if (openxr_session_visible(f->session) && !f->tick_late)
      {
         f->tick_late = true;
         RARCH_WARN("[OpenXR] No headset frame for two intervals; the core carries on.\n");
      }
      f->pace_anchor_ns = (int64_t)cpu_features_get_time_usec() * 1000;
   }
   else
   {
      /* No ticks to wait on: keep the core's rate on the clock, from
       * the last tick on. */
      retro_time_t sleep_us;
      if (f->pace_mode != 2)
         RARCH_LOG("[OpenXR] Pacing on the clock while the headset does not show the session.\n");
      f->pace_mode = 2;
      openxr_frame_pace_skip(f);
      sleep_us     = runloop_pace_schedule(&f->pace_anchor_ns, period,
            cpu_features_get_time_usec());
      if (sleep_us > 0)
         retro_sleep_us((unsigned)sleep_us);
   }
}

unsigned openxr_frame_refresh_rates(const openxr_frame_t *f,
      float *rates, unsigned cap)
{
   unsigned i;
   for (i = 0; i < f->num_rates && i < cap; i++)
      rates[i] = f->rates[i];
   return i;
}

void openxr_frame_request_rate(openxr_frame_t *f, float hz)
{
   int bits;
   memcpy(&bits, &hz, sizeof(bits));
   retro_atomic_store_release_int(&f->want_rate, bits);
}

void openxr_frame_publish(openxr_frame_t *f, const video_xr_quad_set_t *set)
{
   openxr_seq_publish(&f->quads_seq, f->quads_words, set, sizeof(*set));
}

void openxr_frame_publish_grabs(openxr_frame_t *f, const openxr_grabs_t *g)
{
   openxr_seq_publish(&f->grabs_seq, f->grabs_words, g, sizeof(*g));
}

void openxr_frame_publish_aims(openxr_frame_t *f, const openxr_aims_t *a)
{
   openxr_seq_publish(&f->aims_seq, f->aims_words, a, sizeof(*a));
}

void openxr_frame_publish_projection(openxr_frame_t *f,
      const openxr_projection_t *p)
{
   openxr_seq_publish(&f->proj_seq, f->proj_words, p, sizeof(*p));
}

void openxr_frame_clear_projection(openxr_frame_t *f)
{
   openxr_projection_t none;
   memset(&none, 0, sizeof(none));
   openxr_seq_publish(&f->proj_seq, f->proj_words, &none, sizeof(none));
}

void openxr_frame_get_anchor(openxr_frame_t *f, video_xr_pose_t *anchor)
{
   openxr_tracked_t t;
   openxr_frame_read_tracked(f, &t);
   *anchor = t.anchor;
}

void openxr_frame_get_quads(openxr_frame_t *f, video_xr_quad_set_t *out)
{
   openxr_seq_read(&f->quads_seq, f->quads_words, out, sizeof(*out));
}

void openxr_frame_set_placement(openxr_frame_t *f, unsigned placement)
{
   retro_atomic_store_release_int(&f->placement, (int)placement);
}

void openxr_frame_get_shown_quads(openxr_frame_t *f,
      video_xr_quad_set_t *out, XrSpace *space)
{
   unsigned i;
   openxr_screens_t at;
   openxr_frame_get_quads(f, out);
   openxr_seq_read(&f->screens_seq, f->screens_words, &at, sizeof(at));
   for (i = 0; i < out->num_quads; i++)
      video_xr_compose(&at.pose, &out->quads[i].pose,
            &out->quads[i].pose);
   *space = at.view ? f->session->view_space : f->session->local_space;
}

void openxr_frame_get_screens(openxr_frame_t *f, video_xr_pose_t *pose,
      XrSpace *space)
{
   openxr_screens_t at;
   openxr_seq_read(&f->screens_seq, f->screens_words, &at, sizeof(at));
   *pose  = at.pose;
   *space = at.view ? f->session->view_space : f->session->local_space;
}

void openxr_frame_get_aims(openxr_frame_t *f, openxr_aims_t *out)
{
   openxr_seq_read(&f->aims_seq, f->aims_words, out, sizeof(*out));
}

void openxr_frame_request_recenter(openxr_frame_t *f)
{
   retro_atomic_store_release_int(&f->recenter, 1);
}

int openxr_frame_recenters(openxr_frame_t *f)
{
   return retro_atomic_load_acquire_int(&f->recenters);
}

XrDuration openxr_frame_period(openxr_frame_t *f)
{
   return (XrDuration)retro_atomic_load_acquire_int(&f->period_ns);
}

/* A glyph's coverage from the font's atlas into a tag cell's cov at x,
 * y, kept VIDEO_XR_TAG_INSET inside the cell. */
static void openxr_frame_tag_glyph(const struct font_atlas *atlas,
      const struct font_glyph *g, int x, int y, uint8_t *cov)
{
   unsigned gx, gy;
   unsigned w  = VIDEO_SCALE_W(g->dims);
   unsigned h  = VIDEO_SCALE_H(g->dims);
   unsigned ax = VIDEO_SCALE_W(g->atlas_pos);
   unsigned ay = VIDEO_SCALE_H(g->atlas_pos);
   if (!atlas || !atlas->buffer || atlas->format != FONT_ATLAS_FORMAT_A8)
      return;
   for (gy = 0; gy < h && ay + gy < atlas->height; gy++)
      for (gx = 0; gx < w && ax + gx < atlas->width; gx++)
      {
         uint8_t a;
         int cx = x + (int)gx;
         int cy = y + (int)gy;
         if (     cx < VIDEO_XR_TAG_INSET || cy < VIDEO_XR_TAG_INSET
               || cx >= VIDEO_XR_TAG_COLS - VIDEO_XR_TAG_INSET
               || cy >= VIDEO_XR_TAG_ROWS - VIDEO_XR_TAG_INSET)
            continue;
         a = atlas->buffer[(ay + gy) * atlas->width + ax + gx];
         if (a > cov[cy * VIDEO_XR_TAG_COLS + cx])
            cov[cy * VIDEO_XR_TAG_COLS + cx] = a;
      }
}

/* A line of font's, size high, in whole rows above and below its
 * baseline: the font's own, or, where it gives none, a 1.25 em line
 * as a common font's. */
static void openxr_frame_tag_metrics(const font_renderer_driver_t *drv,
      void *font, unsigned size, int *asc, int *desc)
{
   struct font_line_metrics *m = NULL;
   *asc                        = (int)size;
   *desc                       = (int)size / 4;
   if (drv->get_line_metrics)
      drv->get_line_metrics(font, &m);
   if (m)
   {
      *asc  = (int)ceil(m->ascender);
      *desc = (int)ceil(m->descender);
   }
}

/* text in one line, centred in a tag cell's coverage cov, the line
 * (asc rows over its baseline, desc under) in the cell's text rows; a
 * line too tall loses the top of its ascender, never a descender. */
static void openxr_frame_tag_line(const font_renderer_driver_t *drv,
      void *font, int asc, int desc, const char *text, uint8_t *cov)
{
   const char *msg                   = text;
   size_t msg_len                    = strlen(text);
   void *font_data                   = font;
   const struct font_glyph *(*get_glyph)(void*, uint32_t) =
      drv->get_glyph;
   const struct font_glyph *glyph_q  = drv->get_glyph(font, '?');
   int line_x                        = 0;
   int spare                         = VIDEO_XR_TAG_TEXT_ROWS - asc - desc;
   int base                          = VIDEO_XR_TAG_ROWS
      - VIDEO_XR_TAG_INSET - desc - (spare > 0 ? spare / 2 : 0);
#define FONT_LAYOUT_ALIGNED 1
#define FONT_LAYOUT_LINE(line, width, count, bytes) \
   (void)(line); \
   line_x = ((int)VIDEO_XR_TAG_COLS - (int)(width)) / 2;
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   openxr_frame_tag_glyph(drv->get_atlas(font), (glyph), \
         line_x + (pen_x) + (glyph)->draw_offset_x, \
         base - (pen_y) + (glyph)->draw_offset_y, cov);
#include "../font_layout.h"
}

/* Tag tag's words: the laser's, a screen's by number, the menu's. */
static void openxr_frame_tag_text(unsigned tag, char *s, size_t len)
{
   if (!tag)
      strlcpy(s, msg_hash_to_str(MSG_OPENXR_EDIT_TAG_LASER), len);
   else if (tag == VIDEO_XR_MENU_SLOT + 1)
      strlcpy(s, msg_hash_to_str(MSG_OPENXR_EDIT_TAG_MENU), len);
   else
      snprintf(s, len, msg_hash_to_str(MSG_OPENXR_EDIT_TAG_SCREEN), tag);
}

/* Edit mode's tags into px, in the default font. */
static void openxr_frame_cursor_tags(uint8_t *px, bool bottom_up)
{
   unsigned i;
   int asc, desc;
   uint8_t *cov                      = NULL;
   void *font                        = NULL;
   const font_renderer_driver_t *drv = NULL;
   unsigned size                     = VIDEO_XR_TAG_FONT;
   if (!font_renderer_create_default(&drv, &font, NULL, size,
            FONT_ATLAS_FORMAT_A8))
      return;
   openxr_frame_tag_metrics(drv, font, size, &asc, &desc);
   /* A font whose line is taller than the rows is drawn smaller, so no
    * descender is cut. */
   if (asc + desc > VIDEO_XR_TAG_TEXT_ROWS)
   {
      if (drv->free)
         drv->free(font);
      font = NULL;
      size = VIDEO_XR_TAG_FONT * VIDEO_XR_TAG_TEXT_ROWS / (asc + desc);
      if (!size || !font_renderer_create_default(&drv, &font, NULL, size,
               FONT_ATLAS_FORMAT_A8))
         return;
      openxr_frame_tag_metrics(drv, font, size, &asc, &desc);
   }
   if ((cov = (uint8_t*)malloc(VIDEO_XR_TAG_COLS * VIDEO_XR_TAG_ROWS)))
   {
      for (i = 0; i < VIDEO_XR_TAGS; i++)
      {
         char text[64];
         openxr_frame_tag_text(i, text, sizeof(text));
         memset(cov, 0, VIDEO_XR_TAG_COLS * VIDEO_XR_TAG_ROWS);
         openxr_frame_tag_line(drv, font, asc, desc, text, cov);
         video_xr_tag_pixels(px, i, cov, bottom_up);
      }
      free(cov);
   }
   if (drv->free)
      drv->free(font);
}

void openxr_frame_cursor_pixels(uint8_t *px, unsigned w, bool bottom_up)
{
   unsigned i;
   video_xr_cursor_pixels(px);
   if (w == VIDEO_XR_CURSOR_W)
      video_xr_cut_pixels(px, bottom_up);
   if (w >= VIDEO_XR_CURSOR_TAGGED_W)
      openxr_frame_cursor_tags(px, bottom_up);
   if (w < VIDEO_XR_CURSOR_W)
      for (i = 1; i < VIDEO_XR_CURSOR_H; i++)
         memmove(px + (size_t)i * w * 4,
               px + (size_t)i * VIDEO_XR_CURSOR_W * 4, (size_t)w * 4);
}
