/*  RetroArch - A frontend for libretro.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <file/keychain.h>
#include <crypto/crypto.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../configuration.h"
#include "../runloop.h"

/* Setting or entering the keychain passphrase. The key derivation is
 * hundreds of milliseconds on a PC and seconds on a handheld, so the
 * handler runs it a slice at a time: on the task thread it never holds
 * the main thread, and without threads each pump is one short slice.
 * The handler touches only its own state; the keychain itself changes
 * in the callback, on the main thread. */

#define KEYCHAIN_TASK_SLICE 2000

struct keychain_task_state
{
   struct keychain_kdf *kdf;
   uint8_t  psalt[KEYCHAIN_PASS_SALT_SIZE];
   uint8_t  kek[32];
   uint32_t iterations;
   bool     unlock;
   bool     derived;
};

static void task_keychain_handler(retro_task_t *task)
{
   struct keychain_task_state *st = (struct keychain_task_state*)task->state;

   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   if (keychain_kdf_step(st->kdf, KEYCHAIN_TASK_SLICE))
   {
      keychain_kdf_end(st->kdf, st->kek);
      st->kdf     = NULL;
      st->derived = true;
      task_set_progress(task, 100);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   task_set_progress(task, (int8_t)keychain_kdf_progress(st->kdf));
}

static void task_keychain_notify(enum msg_hash_enums id, bool ok)
{
   const char *msg = msg_hash_to_str(id);
   runloop_msg_queue_push(msg, strlen(msg), 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT,
         ok ? MESSAGE_QUEUE_CATEGORY_SUCCESS : MESSAGE_QUEUE_CATEGORY_ERROR);
}

static void task_keychain_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct keychain_task_state *st = (struct keychain_task_state*)task->state;

   if (!st || !st->derived)
      return;
   if (st->unlock)
   {
      if (keychain_unlock_kek(st->kek))
      {
         config_keychain_reapply();
         task_keychain_notify(MSG_KEYCHAIN_UNLOCKED, true);
      }
      else if (keychain_is_locked())
         task_keychain_notify(MSG_KEYCHAIN_PASSPHRASE_WRONG, false);
      else
         task_keychain_notify(MSG_KEYCHAIN_WRITE_FAILED, false);
   }
   else if (keychain_set_passphrase_kek(st->kek, st->psalt, st->iterations))
      task_keychain_notify(MSG_KEYCHAIN_PASSPHRASE_SET, true);
   else
      task_keychain_notify(MSG_KEYCHAIN_WRITE_FAILED, false);
}

static void task_keychain_cleanup(retro_task_t *task)
{
   struct keychain_task_state *st = (struct keychain_task_state*)task->state;
   if (!st)
      return;
   keychain_kdf_end(st->kdf, NULL);
   crypto_memzero(st, sizeof(*st));
   free(st);
   task->state = NULL;
}

bool task_push_keychain_passphrase(const char *passphrase)
{
   struct keychain_task_state *st;
   retro_task_t *task;
   bool          unlock = keychain_is_locked();

   if (!unlock && !keychain_is_ready())
   {
      task_keychain_notify(MSG_KEYCHAIN_WRITE_FAILED, false);
      return false;
   }

   /* Removing the passphrase derives nothing: done here and now. */
   if (!unlock && (!passphrase || !*passphrase))
   {
      bool had = keychain_has_passphrase();
      if (!had)
         return true;
      if (keychain_set_passphrase_kek(NULL, NULL, 0))
      {
         task_keychain_notify(MSG_KEYCHAIN_PASSPHRASE_REMOVED, true);
         return true;
      }
      task_keychain_notify(MSG_KEYCHAIN_WRITE_FAILED, false);
      return false;
   }
   if (!passphrase || !*passphrase)
      return false;

   if (!(st = (struct keychain_task_state*)calloc(1, sizeof(*st))))
      return false;
   st->unlock = unlock;
   if (    !keychain_passphrase_params(unlock, st->psalt, &st->iterations)
         || !(st->kdf = keychain_kdf_begin(passphrase, st->psalt,
               st->iterations))
         || !(task = task_init()))
   {
      keychain_kdf_end(st->kdf, NULL);
      crypto_memzero(st, sizeof(*st));
      free(st);
      task_keychain_notify(MSG_KEYCHAIN_WRITE_FAILED, false);
      return false;
   }

   task->state    = st;
   task->handler  = task_keychain_handler;
   task->callback = task_keychain_callback;
   task->cleanup  = task_keychain_cleanup;
   task->title    = strdup(msg_hash_to_str(unlock
            ? MSG_KEYCHAIN_UNLOCKING : MSG_KEYCHAIN_SETTING_PASSPHRASE));
   task_queue_push(task);
   return true;
}
