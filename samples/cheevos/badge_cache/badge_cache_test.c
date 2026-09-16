/* badge_cache_test: rcheevos_get_badge_texture() never decodes or
 * uploads on the calling thread. The first call for a badge on disk
 * posts a decode and returns 0; the decode's completion posts the
 * upload; the first call after the upload lands takes the handle, and
 * the handle is the caller's (a later call starts a fresh load). A
 * missing file returns 0 and asks for a download exactly when told to.
 * A failed decode or upload is forgotten, not stuck. A reset unloads
 * ready handles and orphans in-flight ones. Off the main thread the
 * answer is 0 with no side effect. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include <compat/strl.h>

#include "../../../cheevos/cheevos.h"

extern int      st_on_main_thread;
extern char     st_existing[8][64];
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

int main(void)
{
   uintptr_t h, h2;

   exists("12345.png");
   exists("12345_lock.png");
   exists("00000.png");

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

   /* 7. failed decode: forgotten; the next ask starts over */
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(st_parked_count == 2, "default badge decode not posted");
   st_finish_decode(1, false);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0 && st_parked_count == 2, "after a failed decode the slot stuck (parked=%u)",
         st_parked_count);
   /* 8. failed upload: same */
   st_finish_decode(1, true);
   st_finish_upload(0, false);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0 && st_parked_count == 2, "after a failed upload the slot stuck");
   st_finish_decode(1, true);
   st_finish_upload(0, true);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0x1003, "default badge handle wrong (%lx)", (unsigned long)h);

   /* 9. reset with one load in flight (12345 unlocked, parked[0]) and
    *    a ready handle: ready is unloaded, in-flight is orphaned - its
    *    late delivery is unloaded, never handed out. */
   st_finish_decode(0, true);
   st_finish_upload(0, true);                 /* 12345 now READY (0x1004) */
   h2 = rcheevos_get_badge_texture("12345", true, false); /* start a locked reload */
   CHECK(st_parked_count == 1, "locked reload not posted");
   st_unloads = 0;
   rcheevos_badge_cache_reset();
   CHECK(st_unloads == 1 && st_last_unloaded == 0x1004,
         "reset unloaded %u handles (last %lx), want the one ready",
         st_unloads, (unsigned long)st_last_unloaded);
   st_finish_decode(0, true);
   st_finish_upload(0, true);                 /* orphaned delivery: 0x1005 */
   CHECK(st_unloads == 2 && st_last_unloaded == 0x1005,
         "orphaned delivery was not unloaded (unloads=%u last=%lx)",
         st_unloads, (unsigned long)st_last_unloaded);
   h2 = rcheevos_get_badge_texture("12345", true, false);
   CHECK(h2 == 0 && st_parked_count == 1, "orphaned handle was handed out, or no reload");
   st_finish_decode(0, true);
   st_finish_upload(0, true);

   /* 10. off the main thread: 0, no side effects */
   st_on_main_thread = 0;
   h = rcheevos_get_badge_texture("12345", false, true);
   CHECK(h == 0 && st_parked_count == 0 && st_downloads == 1,
         "off-thread call had side effects");
   st_on_main_thread = 1;

   /* 11. no async uploader (no wrapper, driver refuses): treated as a
    *     failed load, not a hang */
   st_async_available = 0;
   h = rcheevos_get_badge_texture("00000", false, false);
   st_finish_decode(0, true);
   h = rcheevos_get_badge_texture("00000", false, false);
   CHECK(h == 0 && st_parked_count == 1, "refused upload left the slot stuck");
   st_async_available = 1;

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("badge_cache_test: OK\n");
   return 0;
}
