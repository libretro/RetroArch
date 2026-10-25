#include <stdlib.h>
#include <string.h>
#include <string/stdstring.h>
#include "gfx_media_subtitle.h"
#include "../tasks/task_subtitle.h"

struct gfx_media_subtitle
{
   task_subtitle_request_t *request;
   rsubtitle_srt_t *track;
   task_subtitle_catalog_t *catalog;
   const void *identity;
   char *path, *selected;
   unsigned pending_cycles;
   enum task_subtitle_source selected_source;
   int selected_track;
   bool off, rebind;
   gfx_media_subtitle_view_t view;
};

static void media_subtitle_reset(gfx_media_subtitle_t *subtitle)
{
   task_subtitle_release(subtitle->request);
   rsubtitle_srt_free(subtitle->track);
   free(subtitle->path);
   free(subtitle->selected);
   task_subtitle_catalog_release(subtitle->catalog);
   subtitle->request = NULL;
   subtitle->track = NULL;
   subtitle->path = NULL;
   subtitle->identity = NULL;
   subtitle->selected = NULL;
   subtitle->selected_source = TASK_SUBTITLE_SIDECAR;
   subtitle->selected_track = -1;
   subtitle->catalog = NULL;
   subtitle->pending_cycles = 0;
   subtitle->off = subtitle->rebind = false;
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
   subtitle->selected = sidecar;
   subtitle->selected_source = TASK_SUBTITLE_SIDECAR;
   subtitle->selected_track = -1;
   return subtitle->request != NULL;
}

void gfx_media_subtitle_next(gfx_media_subtitle_t *subtitle)
{
   if (!subtitle || !subtitle->path)
      return;
   if (task_subtitle_catalog_status(subtitle->catalog) < 0)
   {
      task_subtitle_catalog_release(subtitle->catalog);
      subtitle->catalog = task_subtitle_discover(subtitle->path);
   }
   if (subtitle->catalog && subtitle->pending_cycles < (unsigned)-1)
      subtitle->pending_cycles++;
}

void gfx_media_subtitle_suspend(gfx_media_subtitle_t *subtitle)
{
   if (subtitle)
      subtitle->rebind = true;
}

static void media_subtitle_select(gfx_media_subtitle_t *subtitle)
{
   size_t count, index, next, length;
   const char *path;
   const task_subtitle_entry_t *entry;
   char *selected = NULL;
   if (!subtitle->pending_cycles
         || !task_subtitle_catalog_status(subtitle->catalog))
      return;
   if (task_subtitle_catalog_status(subtitle->catalog) < 0)
   {
      subtitle->pending_cycles = 0;
      return;
   }
   count = task_subtitle_catalog_count(subtitle->catalog);
   index = count;
   if (!subtitle->off && subtitle->selected)
      for (index = 0; index < count; index++)
      {
         entry = task_subtitle_catalog_entry(subtitle->catalog, index);
         if (entry && entry->source == subtitle->selected_source
               && entry->track == subtitle->selected_track
#if defined(_WIN32)
               && string_is_equal_noncase(subtitle->selected, entry->path))
#else
               && !strcmp(subtitle->selected, entry->path))
#endif
            break;
      }
   next = (index + subtitle->pending_cycles % (count + 1)) % (count + 1);
   subtitle->pending_cycles = 0;
   entry = task_subtitle_catalog_entry(subtitle->catalog, next);
   path = entry ? entry->path : NULL;
   if (path)
   {
      length = strlen(path);
      selected = (char*)malloc(length + 1);
      if (!selected)
         return;
      memcpy(selected, path, length + 1);
   }
   task_subtitle_release(subtitle->request);
   rsubtitle_srt_free(subtitle->track);
   free(subtitle->selected);
   subtitle->selected = selected;
   subtitle->off = selected == NULL;
   subtitle->track = NULL;
   subtitle->selected_source = entry ? entry->source : TASK_SUBTITLE_SIDECAR;
   subtitle->selected_track = entry ? entry->track : -1;
   subtitle->request = task_subtitle_load_entry(entry);
}

static void media_subtitle_track_view(gfx_media_subtitle_t *subtitle,
      gfx_media_subtitle_view_t *view)
{
   const char *name = subtitle->selected, *p;
   size_t used = 0;
   size_t limit = sizeof(view->track) - (subtitle->selected_track >= 0 ? 16 : 1);
   view->off = subtitle->off;
   view->discovering = subtitle->catalog
      && !task_subtitle_catalog_status(subtitle->catalog);
   view->track_count = (unsigned)task_subtitle_catalog_count(subtitle->catalog);
   if (!name || (!subtitle->track && !subtitle->request))
      return;
   for (p = name; *p; p++)
      if (*p == '/' || *p == '\\')
         name = p + 1;
   while (*name)
   {
      unsigned lead = (unsigned char)*name;
      size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
      size_t i;
      if (used + length > limit)
         break;
      for (i = 0; i < length; i++)
         if (!name[i])
            return;
      memcpy(view->track + used, name, length);
      used += length;
      name += length;
   }
   if (subtitle->selected_track >= 0)
   {
      char digits[12];
      unsigned number = (unsigned)subtitle->selected_track + 1;
      size_t count = 0;
      do
      {
         digits[count++] = (char)('0' + number % 10);
         number /= 10;
      } while (number && count < sizeof(digits));
      view->track[used++] = ' ';
      view->track[used++] = '[';
      while (count) view->track[used++] = digits[--count];
      view->track[used++] = ']';
      view->track[used] = 0;
   }
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
   if (!enabled || ((!identity || !path || !*path) && !subtitle->rebind))
      media_subtitle_reset(subtitle);
   else if (identity && path && *path)
   {
      if (subtitle->rebind && subtitle->path && !strcmp(subtitle->path, path))
         subtitle->identity = identity;
      subtitle->rebind = false;
      if (subtitle->identity != identity || !subtitle->path
            || strcmp(subtitle->path, path))
      {
         media_subtitle_reset(subtitle);
         media_subtitle_open(subtitle, identity, path);
      }
      media_subtitle_select(subtitle);
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
      media_subtitle_track_view(subtitle, &next);
      if (subtitle->track && position_us >= 0)
         media_subtitle_text(subtitle, &next, position_us);
   }
   changed = memcmp(&next, &subtitle->view, sizeof(next)) != 0;
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
