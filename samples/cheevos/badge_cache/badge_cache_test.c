/* badge_cache_test: rcheevos_get_badge_texture() never reads, decodes
 * or uploads on the calling thread, so any thread may call it. On the
 * main thread the first call for a badge on disk posts a decode and
 * returns 0; the decode's completion posts the upload; the first call
 * after the upload lands takes the handle, and the handle is the
 * caller's (a later call starts a fresh load). Off the main thread the
 * call only records the request, and rcheevos_badge_cache_service() on
 * the main thread starts it. A missing file returns 0 and asks for a
 * download exactly when told to, and the download's completion starts
 * the load. A failed load stays failed - it is not retried once a
 * frame - until a reset or a fresh download. A reset unloads ready
 * handles and orphans in-flight ones, also when their slot has been
 * reused meanwhile. The default badge is lent, not given. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <compat/strl.h>

#include "../../../cheevos/cheevos.h"

extern int      st_on_main_thread;
extern char     st_existing[32][64];
extern unsigned st_existing_count;
extern unsigned st_downloads;
extern char     st_last_download[64];
extern unsigned st_uploads;
extern unsigned st_unloads;
extern uintptr_t st_last_unloaded;
extern unsigned st_parked_count;
extern unsigned st_uploads_pending_count;
extern int      st_async_available;
void st_finish_decode(unsigned i, bool ok);
void st_finish_upload(unsigned i, bool ok);

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); failures++; } } while (0)

static void exists(const char *file)
{
   strlcpy(st_existing[st_existing_count++], file, 64);
}

static void exists_png(const char *badge)
{
   char file[64];
   snprintf(file, sizeof(file), "%s.png", badge);
   exists(file);
}

int main(void)
{
   uintptr_t h, h2;

   exists("12345.png");
   exists("12345_lock.png");
   exists("00000.png");
   exists("77777.png");

   /* 1. first ask: nothing synchronous, one decode posted, 0 back */
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h == 0, "first call returned a handle (%lx)", (unsigned long)h);
   CHECK(st_parked_count == 1, "%u decodes posted, want 1", st_parked_count);
   CHECK(st_uploads_pending_count == 0, "upload posted before decode finished");
   CHECK(st_downloads == 0, "download requested for a file that exists");

   /* 2. asking again while loading: still 0, no second decode */
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h == 0 && st_parked_count == 1, "second ask re-posted the decode");

   /* 3. decode lands -> upload posted, still nothing to hand out */
   st_finish_decode(0, true);
   CHECK(st_uploads_pending_count == 1, "decode completion did not post the upload");
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h == 0, "handle handed out before the upload landed");

   /* 4. upload lands -> next ask takes the handle, exactly once */
   st_finish_upload(0, true);
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h == 0x1001, "ready handle not handed out (got %lx)", (unsigned long)h);
   h2 = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h2 == 0 && st_parked_count == 1,
         "second taker got %lx / %u decodes: the handle was not transferred",
         (unsigned long)h2, st_parked_count);
   CHECK(st_unloads == 0, "something unloaded during a clean hand-over");

   /* 5. the locked variant is its own key */
   h2 = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h2 == 0 && st_parked_count == 2, "locked badge shares the unlocked slot");
   st_finish_decode(1, true);
   st_finish_upload(0, true);
   h2 = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h2 == 0x1002, "locked badge handle wrong (%lx)", (unsigned long)h2);
   /* leave the unlocked reload (parked[0]) in flight for step 9 */

   /* 6. missing file: 0, download only when asked */
   h = rcheevos_get_badge_texture("99999", false, false);
   CHECK(h == 0 && st_downloads == 0 && st_parked_count == 1,
         "missing badge without download flag: dl=%u parked=%u",
         st_downloads, st_parked_count);
   h = rcheevos_get_badge_texture("99999", false, true);
   CHECK(h == 0 && st_downloads == 1 && !strcmp(st_last_download, "99999"),
         "missing badge with download flag: dl=%u last=%s",
         st_downloads, st_last_download);

   /* 7. failed decode: stays failed, no decode per ask; a reset
    *    forgets it */
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(st_parked_count == 2, "00000 decode not posted");
   st_finish_decode(1, false);
   h = rcheevos_get_badge_texture("00000", false, false);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0 && st_parked_count == 1, "a failed decode was retried per ask (parked=%u)",
         st_parked_count);

   /* 8. failed upload: same */
   h = rcheevos_get_badge_texture("12345", true, false);
   CHECK(st_parked_count == 2, "locked reload not posted");
   st_finish_decode(1, true);
   st_finish_upload(0, false);
   h = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h == 0 && st_parked_count == 1, "a failed upload was retried per ask");

   /* 9. reset with one load in flight (12345 unlocked, parked[0]) and
    *    a ready handle: ready is unloaded, in-flight is orphaned - its
    *    late delivery is unloaded, never handed out, not even to a
    *    badge that has taken over its slot meanwhile. */
   h = rcheevos_get_badge_texture("77777", false, false);
   CHECK(st_parked_count == 2, "77777 decode not posted");
   st_finish_decode(1, true);
   st_finish_upload(0, true);                 /* 77777 now READY (0x1003) */
   st_unloads = 0;
   rcheevos_badge_cache_reset();
   CHECK(st_unloads == 1 && st_last_unloaded == 0x1003,
         "reset unloaded %u handles (last %lx), want the one ready",
         st_unloads, (unsigned long)st_last_unloaded);
   /* fill every slot, so the orphan's is certainly in use again */
   {
      unsigned i;
      char name[8];
      for (i = 0; i < 16; i++)
      {
         snprintf(name, sizeof(name), "5%04u", i);
         exists_png(name);
         h = rcheevos_get_badge_texture(name, false, false);
      }
      CHECK(st_parked_count == 17, "%u decodes parked, want 17", st_parked_count);
   }
   st_finish_decode(0, true);
   st_finish_upload(0, true);                 /* orphaned delivery: 0x1004 */
   CHECK(st_unloads == 2 && st_last_unloaded == 0x1004,
         "orphaned delivery was not unloaded (unloads=%u last=%lx)",
         st_unloads, (unsigned long)st_last_unloaded);
   {
      unsigned i;
      char name[8];
      for (i = 0; i < 16; i++)
      {
         snprintf(name, sizeof(name), "5%04u", i);
         h = rcheevos_get_badge_texture(name, false, false);
         CHECK(h == 0, "%s was handed the orphan's texture", name);
      }
   }
   rcheevos_badge_cache_reset();
   while (st_parked_count)
      st_finish_decode(0, false);
   /* the reset forgot the failures of 7 and 8 too */
   h = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h == 0 && st_parked_count == 1, "reset did not forget a failed load");
   st_finish_decode(0, true);
   st_finish_upload(0, true);
   h = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h != 0, "reload after reset not handed out");

   /* 10. off the main thread: the request is recorded and nothing
    *     else happens; the main thread's service starts it; the
    *     asking thread takes the handle */
   st_on_main_thread = 0;
   st_downloads      = 0;
   h = rcheevos_get_badge_texture("12345", false, true);
   h = rcheevos_get_badge_texture("88888", false, true);
   rcheevos_badge_cache_service();            /* not the main thread: no-op */
   CHECK(h == 0 && st_parked_count == 0 && st_downloads == 0,
         "off-thread call did work: parked=%u dl=%u", st_parked_count, st_downloads);
   st_on_main_thread = 1;
   rcheevos_badge_cache_service();
   CHECK(st_parked_count == 1 && st_downloads == 1 && !strcmp(st_last_download, "88888"),
         "service: parked=%u dl=%u last=%s", st_parked_count, st_downloads, st_last_download);
   rcheevos_badge_cache_service();
   CHECK(st_parked_count == 1 && st_downloads == 1, "service ran a request twice");
   st_finish_decode(0, true);
   st_finish_upload(0, true);
   st_on_main_thread = 0;
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h != 0, "off-thread asker was not handed the ready handle");
   /* still downloading: asked every frame, requested once */
   h = rcheevos_get_badge_texture("88888", false, true);
   st_on_main_thread = 1;
   rcheevos_badge_cache_service();
   CHECK(h == 0 && st_downloads == 1 && st_parked_count == 0,
         "a badge being fetched was requested again");

   /* 11. the download lands: the load starts from there */
   exists("88888.png");
   rcheevos_update_badge_references("88888");
   CHECK(st_parked_count == 1, "download completion did not start the load");
   st_finish_decode(0, true);
   st_finish_upload(0, true);
   h = rcheevos_get_badge_texture("88888", false, false);
   CHECK(h != 0, "downloaded badge not handed out");
   /* a name nobody waits for is ignored */
   rcheevos_update_badge_references("12345_lock");
   CHECK(st_parked_count == 0, "download of an unwanted badge started a load");

   /* 12. missing without the download flag fails; an asker that does
    *     want the download gets it, once */
   st_downloads = 0;
   h = rcheevos_get_badge_texture("66666", true, false);
   h = rcheevos_get_badge_texture("66666", true, false);
   CHECK(st_downloads == 0, "download without the flag");
   h = rcheevos_get_badge_texture("66666", true, true);
   h = rcheevos_get_badge_texture("66666", true, true);
   CHECK(st_downloads == 1, "%u downloads, want 1", st_downloads);
   exists("66666_lock.png");
   rcheevos_update_badge_references("66666_lock");
   CHECK(st_parked_count == 1, "locked download completion did not start the load");
   st_finish_decode(0, true);
   st_finish_upload(0, true);
   h = rcheevos_get_badge_texture("66666", true, false);
   CHECK(h != 0, "downloaded locked badge not handed out");

   /* 13. the default badge is lent: same handle to every asker, the
    *     slot keeps it, a reset unloads it exactly once */
   h = rcheevos_get_default_badge_texture();
   CHECK(h == 0 && st_parked_count == 1, "default badge decode not posted");
   st_finish_decode(0, true);
   st_finish_upload(0, true);
   h  = rcheevos_get_default_badge_texture();
   h2 = rcheevos_get_default_badge_texture();
   CHECK(h != 0 && h == h2 && st_parked_count == 0,
         "default badge: %lx then %lx", (unsigned long)h, (unsigned long)h2);
   st_unloads = 0;
   rcheevos_badge_cache_reset();
   CHECK(st_unloads == 1 && st_last_unloaded == h, "default badge unloads: %u", st_unloads);

   /* 14. a 0 says whether it is worth sitting out: yes while a file
    *     on disk is loading, no once it is handed over, while it is
    *     being downloaded, and after a failure */
   {
      bool pending = false;
      h = rcheevos_get_badge_texture_ex("12345", false, true, &pending);
      CHECK(h == 0 && pending, "local load not reported pending");
      st_finish_decode(0, true);
      h = rcheevos_get_badge_texture_ex("12345", false, true, &pending);
      CHECK(h == 0 && pending, "upload in flight not reported pending");
      st_finish_upload(0, true);
      h = rcheevos_get_badge_texture_ex("12345", false, true, &pending);
      CHECK(h != 0 && !pending, "handed over, still pending");
      h = rcheevos_get_badge_texture_ex("44444", false, true, &pending);
      CHECK(h == 0 && !pending, "a download reported as a short wait");
      h = rcheevos_get_badge_texture_ex("33333", false, false, &pending);
      CHECK(h == 0 && !pending, "a missing file reported as a short wait");
      st_on_main_thread = 0;
      h = rcheevos_get_badge_texture_ex("12345", true, false, &pending);
      st_on_main_thread = 1;
      CHECK(h == 0 && pending, "off-thread request not reported pending");
      rcheevos_badge_cache_reset();
   }

   /* 15. no async uploader (no wrapper, driver refuses): treated as a
    *     failed load, not a hang */
   st_async_available = 0;
   h = rcheevos_get_badge_texture("00000", false, false);
   st_finish_decode(0, true);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0 && st_parked_count == 0 && st_uploads_pending_count == 0,
         "refused upload left the slot loading");
   st_async_available = 1;

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("badge_cache_test: OK\n");
   return 0;
}
