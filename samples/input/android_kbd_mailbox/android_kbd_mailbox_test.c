/* android_kbd_mailbox_test.c -- the Android IME keyboard mailbox
 * from input/drivers/android_input.c, reproduced and beaten on.
 *
 * The protocol: the Android UI thread publishes an immutable
 * snapshot of the whole keyboard line - text plus finished/cancel -
 * into a one-slot atomic mailbox on every input event; publish is
 * exchange-in, freeing whatever snapshot was never taken, and the
 * input thread's poll exchanges it out. The gate (open) is written
 * by the input thread at session start/end and checked by the
 * producer; a snapshot racing past a closing gate sits unread and
 * is drained at end and again at the next start. The claims a
 * regression would break:
 *
 *   1. Snapshots are whole: any snapshot taken carries a consistent
 *      text (checksummed), never a torn mix.
 *   2. Last wins and finished is never lost: superseded texts may
 *      drop (the Java side sends the whole line each time), but the
 *      session's final consumed state equals the last published
 *      snapshot - text, finished and cancel alike.
 *   3. A cancel session ends cancelled: the callback fires with a
 *      NULL line.
 *   4. Session hygiene: a snapshot published as the gate closes is
 *      drained, not delivered into the next session; ASan holds
 *      that every node is freed exactly once.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads; the protocol has no spin anywhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define KBD_BUFFER_SIZE 512
#define N_EVENTS        3000

struct kbd_msg
{
   char text[KBD_BUFFER_SIZE];
   bool finished;
   bool cancel;
};

static retro_atomic_ptr_t g_pending;
static retro_atomic_int_t g_open;
static retro_atomic_int_t g_ui_done;

/* sabotage hook: 0 = correct protocol */
static int g_sabotage_late_write = 0;

static unsigned checksum(const char *b)
{
   unsigned h = 2166136261u;
   for (; *b; b++)
      h = (h ^ (unsigned char)*b) * 16777619u;
   return h;
}

static void kbd_drain_pending(void)
{
   struct kbd_msg *stale = (struct kbd_msg *)
         retro_atomic_exchange_ptr(&g_pending, NULL);
   if (stale)
      free(stale);
}

/* The JNI callback's shape: build a whole snapshot, exchange it in. */
static void ui_publish(const char *text, bool finished, bool cancel)
{
   struct kbd_msg *node;
   struct kbd_msg *stale;

   if (!retro_atomic_load_acquire_int(&g_open))
      return;
   if (!(node = (struct kbd_msg *)malloc(sizeof(*node))))
      abort();
   node->text[0]  = '\0';
   node->finished = finished;
   node->cancel   = cancel;
   if (!cancel && text)
   {
      if (g_sabotage_late_write)
      {
         /* SABOTAGE: publish first, fill after - with a stall so the
          * consumer reliably lands inside the window. */
         size_t half = strlen(text) / 2;
         strlcpy(node->text, text, half + 1);
         stale = (struct kbd_msg *)
               retro_atomic_exchange_ptr(&g_pending, node);
         if (stale)
            free(stale);
         sleep_us(250);
         strlcpy(node->text, text, sizeof(node->text));
         return;
      }
      strlcpy(node->text, text, sizeof(node->text));
   }
   stale = (struct kbd_msg *)
         retro_atomic_exchange_ptr(&g_pending, node);
   if (stale)
      free(stale);
}

struct ui_script { int cancel_session; };

static void ui_thread(void *arg)
{
   struct ui_script *sc = (struct ui_script *)arg;
   char line[KBD_BUFFER_SIZE];
   char msg[KBD_BUFFER_SIZE];
   int  i;

   line[0] = '\0';
   for (i = 0; i < N_EVENTS; i++)
   {
      char ch[2];
      ch[0] = (char)('a' + (i % 26)); ch[1] = '\0';
      if (strlen(line) > 200)
         line[0] = '\0';
      strlcat(line, ch, sizeof(line));
      snprintf(msg, sizeof(msg), "%s#%08x", line, checksum(line));
      ui_publish(msg, false, false);
      if ((i & 31) == 0)
         sleep_us(90);
   }
   if (sc->cancel_session)
      ui_publish(NULL, true, true);
   else
   {
      snprintf(msg, sizeof(msg), "%s#%08x", line, checksum(line));
      ui_publish(msg, true, false);
   }
   retro_atomic_store_release_int(&g_ui_done, 1);
}

/* One session: start, poll until finished, end. Returns the final
 * consumed state through the out-params. */
static int run_session(int cancel_session, char *final_text,
      bool *finished_out, bool *cancel_out, int *torn_out)
{
   sthread_t *ui;
   struct ui_script sc;
   char live[KBD_BUFFER_SIZE];
   int  polls = 0;

   sc.cancel_session = cancel_session;
   live[0]           = '\0';
   *finished_out     = false;
   *cancel_out       = false;
   *torn_out         = 0;

   kbd_drain_pending();
   retro_atomic_store_release_int(&g_open, 1);
   retro_atomic_store_relaxed_int(&g_ui_done, 0);

   ui = sthread_create(ui_thread, &sc);

   for (;;)
   {
      struct kbd_msg *node = (struct kbd_msg *)
            retro_atomic_exchange_ptr(&g_pending, NULL);
      if (node)
      {
         if (!node->cancel)
         {
            const char *hash = strrchr(node->text, '#');
            unsigned want;
            char body[KBD_BUFFER_SIZE];
            if (   !hash || (size_t)(hash - node->text) >= sizeof(body)
                || sscanf(hash + 1, "%x", &want) != 1)
               (*torn_out)++;
            else
            {
               memcpy(body, node->text, (size_t)(hash - node->text));
               body[hash - node->text] = '\0';
               if (checksum(body) != want)
                  (*torn_out)++;
            }
            strlcpy(live, node->text, sizeof(live));
         }
         *finished_out = node->finished;
         *cancel_out   = node->cancel;
         if (node->finished)
         {
            free(node);
            break;
         }
         free(node);
      }
      if (retro_atomic_load_acquire_int(&g_ui_done)
          && !retro_atomic_load_relaxed_ptr(&g_pending))
      {
         /* finished must have been consumed above; reaching here
          * with the UI done and the mailbox empty and no finished
          * seen is claim 2 failing. */
         break;
      }
      polls++;
      sleep_us(120);
   }

   /* android_keyboard_end(): gate first, then drain. */
   retro_atomic_store_release_int(&g_open, 0);
   kbd_drain_pending();
   sthread_join(ui);
   strlcpy(final_text, live, KBD_BUFFER_SIZE);
   return polls;
}

int main(void)
{
   char final_text[KBD_BUFFER_SIZE];
   char expect[KBD_BUFFER_SIZE];
   char line[KBD_BUFFER_SIZE];
   bool finished, cancel;
   int  torn, i, fails = 0;

   retro_atomic_store_relaxed_int(&g_open, 0);

   /* Session 1: normal typing, finished on the last event. */
   run_session(0, final_text, &finished, &cancel, &torn);
   line[0] = '\0';
   for (i = 0; i < N_EVENTS; i++)
   {
      char ch[2];
      ch[0] = (char)('a' + (i % 26)); ch[1] = '\0';
      if (strlen(line) > 200)
         line[0] = '\0';
      strlcat(line, ch, sizeof(line));
   }
   snprintf(expect, sizeof(expect), "%s#%08x", line, checksum(line));
   if (torn)
      { printf("  FAIL: %d torn snapshot(s)\n", torn); fails++; }
   if (!finished || cancel)
      { printf("  FAIL: finished lost (finished=%d cancel=%d)\n",
               finished, cancel); fails++; }
   if (strcmp(final_text, expect))
      { printf("  FAIL: last-wins broken\n    got  %.60s\n    want %.60s\n",
               final_text, expect); fails++; }

   /* Session 2: cancelled. */
   run_session(1, final_text, &finished, &cancel, &torn);
   if (!finished || !cancel)
      { printf("  FAIL: cancel session ended finished=%d cancel=%d\n",
               finished, cancel); fails++; }
   if (torn)
      { printf("  FAIL: %d torn snapshot(s) in cancel session\n", torn);
        fails++; }

   /* Session 3: a snapshot published as the gate closes must not
    * leak into the next session. Publish with the gate open, close
    * without polling, reopen and assert the mailbox starts empty. */
   retro_atomic_store_release_int(&g_open, 1);
   ui_publish("stale#00000000", false, false);
   retro_atomic_store_release_int(&g_open, 0);
   kbd_drain_pending();                    /* end's drain */
   kbd_drain_pending();                    /* next start's drain */
   if (retro_atomic_load_relaxed_ptr(&g_pending))
      { printf("  FAIL: stale snapshot survived session close\n"); fails++; }

   printf("android_kbd_mailbox: %d events x 2 sessions + hygiene\n",
         N_EVENTS);
   if (fails)
   {
      printf("android_kbd_mailbox: FAILED\n");
      return 1;
   }
   printf("android_kbd_mailbox: ok\n");
   return 0;
}
