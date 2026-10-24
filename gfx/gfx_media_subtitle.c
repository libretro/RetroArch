#include <stdlib.h>
#include <string.h>
#include "gfx_media_subtitle.h"
#include "../tasks/task_subtitle.h"

struct gfx_media_subtitle
{
   task_subtitle_request_t *request;
   rsubtitle_srt_t *track;
   const void *identity;
   char *path;
   gfx_media_subtitle_view_t view;
};

static void media_subtitle_reset(gfx_media_subtitle_t *subtitle)
{
   task_subtitle_release(subtitle->request);
   rsubtitle_srt_free(subtitle->track);
   free(subtitle->path);
   subtitle->request = NULL;
   subtitle->track = NULL;
   subtitle->path = NULL;
   subtitle->identity = NULL;
}

gfx_media_subtitle_t *gfx_media_subtitle_new(void)
{
   return (gfx_media_subtitle_t*)calloc(1, sizeof(gfx_media_subtitle_t));
}

void gfx_media_subtitle_free(gfx_media_subtitle_t *subtitle)
{
   if (!subtitle)
      return;
   media_subtitle_reset(subtitle);
   free(subtitle);
}

static bool media_subtitle_open(gfx_media_subtitle_t *subtitle,
      const void *identity, const char *path)
{
   size_t length = strlen(path), end = length;
   char *sidecar;
   if (length > 32768)
      return false;
   subtitle->path = (char*)malloc(length + 1);
   if (!subtitle->path)
      return false;
   memcpy(subtitle->path, path, length + 1);
   subtitle->identity = identity;
   while (end && path[end - 1] != '/' && path[end - 1] != '\\')
   {
      if (path[end - 1] == '.')
      {
         end--;
         break;
      }
      end--;
   }
   if (!end || path[end] != '.')
      end = length;
   sidecar = (char*)malloc(end + sizeof(".srt"));
   if (!sidecar)
      return false;
   memcpy(sidecar, path, end);
   memcpy(sidecar + end, ".srt", sizeof(".srt"));
   subtitle->request = task_subtitle_load(sidecar);
   free(sidecar);
   return subtitle->request != NULL;
}

static void media_subtitle_text(gfx_media_subtitle_t *subtitle,
      gfx_media_subtitle_view_t *view, int64_t position_us)
{
   size_t index = rsubtitle_srt_find(subtitle->track, position_us);
   size_t count = rsubtitle_srt_count(subtitle->track), used = 0;
   unsigned cues = 0;
   while (index < count && cues++ < 8)
   {
      const rsubtitle_srt_cue_t *cue = rsubtitle_srt_cue(subtitle->track, index);
      size_t i = 0;
      if (used)
      {
         if (used + 1 >= sizeof(view->text))
         {
            view->truncated = true;
            break;
         }
         view->text[used++] = '\n';
      }
      while (i < cue->length)
      {
         unsigned lead = (unsigned char)cue->text[i];
         size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
         if (used + length >= sizeof(view->text))
         {
            view->truncated = true;
            break;
         }
         memcpy(view->text + used, cue->text + i, length);
         used += length;
         i += length;
      }
      if (view->truncated)
         break;
      index = rsubtitle_srt_find_from(subtitle->track, position_us, index + 1);
   }
   if (index < count)
      view->truncated = true;
   view->text[used] = '\0';
}

bool gfx_media_subtitle_update(gfx_media_subtitle_t *subtitle,
      const void *identity, const char *path, int64_t position_us,
      int64_t delay_us, bool enabled)
{
   gfx_media_subtitle_view_t next;
   bool changed;
   if (!subtitle)
      return false;
   memset(&next, 0, sizeof(next));
   if (!enabled || !identity || !path || !*path)
      media_subtitle_reset(subtitle);
   else
   {
      if (subtitle->identity != identity || !subtitle->path
            || strcmp(subtitle->path, path))
      {
         media_subtitle_reset(subtitle);
         media_subtitle_open(subtitle, identity, path);
      }
      if (subtitle->request && task_subtitle_status(subtitle->request))
      {
         subtitle->track = task_subtitle_take(subtitle->request);
         task_subtitle_release(subtitle->request);
         subtitle->request = NULL;
      }
      if (delay_us > 0 && position_us < delay_us)
         position_us = -1;
      else if (delay_us < 0 && position_us > (int64_t)(((uint64_t)-1) >> 1) + delay_us)
         position_us = (int64_t)(((uint64_t)-1) >> 1);
      else
         position_us -= delay_us;
      if (subtitle->track && position_us >= 0)
         media_subtitle_text(subtitle, &next, position_us);
   }
   changed = next.truncated != subtitle->view.truncated
      || strcmp(next.text, subtitle->view.text) != 0;
   subtitle->view = next;
   return changed;
}

const gfx_media_subtitle_view_t *gfx_media_subtitle_view(
      const gfx_media_subtitle_t *subtitle)
{
   return subtitle ? &subtitle->view : NULL;
}

void gfx_media_subtitle_publish(gfx_media_subtitle_mailbox_t *mailbox,
      const gfx_media_subtitle_view_t *view)
{
   unsigned slot, i;
   if (!mailbox)
      return;
   for (slot = 0; slot < 3; slot++)
      if (retro_atomic_cas_int(&mailbox->states[slot], 0, 1))
         break;
   if (slot == 3)
      return;
   if (view)
      mailbox->views[slot] = *view;
   else
      memset(&mailbox->views[slot], 0, sizeof(mailbox->views[slot]));
   for (i = 0; i < 3; i++)
      if (i != slot)
         retro_atomic_cas_int(&mailbox->states[i], 2, 0);
   retro_atomic_store_release_int(&mailbox->states[slot], 2);
}

bool gfx_media_subtitle_consume(gfx_media_subtitle_mailbox_t *mailbox,
      gfx_media_subtitle_view_t *view)
{
   unsigned i;
   if (!mailbox || !view)
      return false;
   for (i = 0; i < 3; i++)
      if (retro_atomic_cas_int(&mailbox->states[i], 2, 3))
      {
         *view = mailbox->views[i];
         retro_atomic_store_release_int(&mailbox->states[i], 0);
         return true;
      }
   return false;
}
