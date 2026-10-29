/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2019-2020 - James Leaver
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

#include "com_retroarch_browser_retroactivity_RetroActivityCommon.h"

#include <retro_atomic.h>

#include <string/stdstring.h>
#include "../../frontend/drivers/platform_unix.h"

#include "play_feature_delivery.h"
#include <queues/task_queue.h>
#include <compat/strl.h>

/***************************/
/* Globals (do not fix...) */
/***************************/

/* Java reports core download status asynchronously, from its own
 * thread. The whole status of the transaction is one word, so the
 * callbacks, the claim and the task polling it never lock. */

/* word: progress (0-100), status, ACTIVE, CLAIMING, generation */
#define PFD_PROGRESS_MASK 0x7F
#define PFD_STATUS_SHIFT  7
#define PFD_STATUS_MASK   (0xF << PFD_STATUS_SHIFT)
#define PFD_ACTIVE        (1 << 11)
#define PFD_CLAIMING      (1 << 12)
#define PFD_GEN_ONE       (1 << 13)
#define PFD_GEN_MASK      0x7FFFE000

#define PFD_STATUS(w)     \
   ((enum play_feature_delivery_install_status) \
    (((w) & PFD_STATUS_MASK) >> PFD_STATUS_SHIFT))

typedef struct
{
   retro_atomic_int_t word;
   /* Of the active transaction; written while CLAIMING, read once
    * ACTIVE is seen, and only trusted if the word is unchanged. */
   retro_atomic_int_t name_hash;
   retro_atomic_int_t waiter;
   /* Cached result of play_feature_delivery_enabled_internal():
    * -1 = not yet queried, 0 = disabled, 1 = enabled.  Queried
    * frequently, often in loops, so the fast path is a single
    * acquire load with no lock.  A cold race can at worst issue
    * the (idempotent) Java query twice; both stores write the
    * same value. */
   retro_atomic_int_t enabled_cached;
} play_feature_delivery_state_t;

static play_feature_delivery_state_t play_feature_delivery_state = {
   RETRO_ATOMIC_INT_INITIALIZER(PLAY_FEATURE_DELIVERY_IDLE
         << PFD_STATUS_SHIFT),       /* word */
   RETRO_ATOMIC_INT_INITIALIZER(0),  /* name_hash */
   RETRO_ATOMIC_INT_INITIALIZER(0),  /* waiter */
   RETRO_ATOMIC_INT_INITIALIZER(-1), /* enabled_cached */
};

static play_feature_delivery_state_t* play_feature_delivery_get_state(void)
{
   return &play_feature_delivery_state;
}

/* FNV-1a */
static int play_feature_delivery_name_hash(const char *s)
{
   uint32_t h = 2166136261u;
   while (*s)
   {
      h ^= (unsigned char)*s++;
      h *= 16777619u;
   }
   return (int)(h & 0x7FFFFFFF);
}

/**********************/
/* JNI Native Methods */
/**********************/

/*
 * Class:     com_retroarch_browser_retroactivity_RetroActivityCommon
 * Method:    coreInstallInitiated
 * Signature: (Ljava/lang/String;Z)V
 */
JNIEXPORT void JNICALL Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallInitiated
      (JNIEnv *env, jobject this_obj, jstring core_name, jboolean successful)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();
   const char *core_name_c;
   int hash;

   /* Only update status if an install is active */
   if (!(retro_atomic_load_acquire_int(&state->word) & PFD_ACTIVE))
      return;

   if (!(core_name_c = (*env)->GetStringUTFChars(env, core_name, NULL)))
      return;
   hash = play_feature_delivery_name_hash(core_name_c);
   (*env)->ReleaseStringUTFChars(env, core_name, core_name_c);

   for (;;)
   {
      uint32_t waiter;
      int nw;
      int w = retro_atomic_load_acquire_int(&state->word);

      /* Ensure that status update is for the correct core */
      if (     !(w & PFD_ACTIVE)
            || retro_atomic_load_relaxed_int(&state->name_hash) != hash)
         return;
      waiter = (uint32_t)retro_atomic_load_relaxed_int(&state->waiter);
      nw     = w & ~PFD_STATUS_MASK;
      if (successful)
         nw |= PLAY_FEATURE_DELIVERY_STARTING << PFD_STATUS_SHIFT;
      else
         nw  = (nw & ~PFD_ACTIVE)
            | (PLAY_FEATURE_DELIVERY_FAILED << PFD_STATUS_SHIFT);
      if (retro_atomic_cas_int(&state->word, w, nw))
      {
         task_queue_wake(waiter);
         return;
      }
   }
}

/*
 * Class:     com_retroarch_browser_retroactivity_RetroActivityCommon
 * Method:    coreInstallStatusChanged
 * Signature: ([Ljava/lang/String;IJJ)V
 */
JNIEXPORT void JNICALL Java_com_retroarch_browser_retroactivity_RetroActivityCommon_coreInstallStatusChanged
      (JNIEnv *env, jobject thisObj, jobjectArray core_names, jint status, jlong bytes_downloaded, jlong total_bytes_to_download)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();
   unsigned progress                    = 100;
   enum play_feature_delivery_install_status last_status;
   bool active                          = true;

   /* Note: core_names is a list of cores that
    * are currently installing. We should check
    * that the active core is in this list
    * before updating the status, but if multiple
    * installs are queued then it seems dubious
    * to filter like this - i.e. is it possible
    * for an entry to drop off the queue before
    * the entire transaction is complete? If so,
    * then we may risk 'missing' the final status
    * update...
    * We therefore just monitor the transaction
    * as a whole, and disregard core names... */

   /* Determine download progress */
   if (total_bytes_to_download > 0)
   {
      progress = (unsigned)
            (((float)bytes_downloaded * 100.0f /
                  (float)total_bytes_to_download) + 0.5f);
      if (progress > 100)
         progress = 100;
   }

   /* Check status */
   switch (status)
   {
      case 0: /* INSTALL_STATUS_DOWNLOADING */
         last_status = PLAY_FEATURE_DELIVERY_DOWNLOADING;
         break;
      case 1: /* INSTALL_STATUS_INSTALLING */
         last_status = PLAY_FEATURE_DELIVERY_INSTALLING;
         break;
      case 2: /* INSTALL_STATUS_INSTALLED */
         last_status = PLAY_FEATURE_DELIVERY_INSTALLED;
         active      = false;
         break;
      case 3: /* INSTALL_STATUS_FAILED */
      default:
         last_status = PLAY_FEATURE_DELIVERY_FAILED;
         active      = false;
         break;
   }

   for (;;)
   {
      uint32_t waiter;
      int nw;
      int w = retro_atomic_load_acquire_int(&state->word);

      /* Only update status if an install is active */
      if (!(w & PFD_ACTIVE))
         return;
      waiter = (uint32_t)retro_atomic_load_relaxed_int(&state->waiter);
      nw     = (w & PFD_GEN_MASK)
         | (int)progress
         | ((int)last_status << PFD_STATUS_SHIFT)
         | (active ? PFD_ACTIVE : 0);
      if (retro_atomic_cas_int(&state->word, w, nw))
      {
         task_queue_wake(waiter);
         return;
      }
   }
}

/**********/
/* Status */
/**********/

static bool play_feature_delivery_get_core_name(
      const char *core_file, char *core_name, size_t len)
{
   size_t core_file_len;

   if (!core_file || !*core_file)
      return false;

   core_file_len = strlen(core_file);

   if (len < core_file_len)
      return false;

   /* Ensure that core_file has the correct
    * suffix */
   if (!string_ends_with_size(core_file, "_libretro_android.so",
         core_file_len, STRLEN_CONST("_libretro_android.so")))
      return false;

   /* Copy core_file and remove suffix */
   strlcpy(core_name, core_file, len);
   core_name[core_file_len - STRLEN_CONST("_libretro_android.so")] = '\0';

   return true;
}

/* Returns true if current build utilises
 * play feature delivery
 * > Relies on a Java function call, and is
 *   therefore slow */
static bool play_feature_delivery_enabled_internal(void)
{
   JNIEnv *env             = jni_thread_getenv();
   struct android_app *app = (struct android_app*)g_android;
   bool enabled            = false;

   if (!env ||
       !app ||
       !app->isPlayStoreBuild ||
        app->isMetaQuestBuild)
      return false;

   CALL_BOOLEAN_METHOD(env, enabled, app->activity->clazz,
         app->isPlayStoreBuild);

   return enabled;
}

/* Returns true if current build utilises
 * play feature delivery */
bool play_feature_delivery_enabled(void)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();

   /* Calling Java functions is slow. We need to
    * check Play Store build status frequently,
    * often in loops, so rely on a cached global
    * status flag instead dealing with Java
    * interfaces */
   int cached = retro_atomic_load_acquire_int(&state->enabled_cached);

   if (cached < 0)
   {
      cached = play_feature_delivery_enabled_internal() ? 1 : 0;
      retro_atomic_store_release_int(&state->enabled_cached, cached);
   }

   return (cached == 1);
}

/* Returns a list of cores currently available
 * via play feature delivery.
 * Returns a new string_list on success, or
 * NULL on failure */
struct string_list *play_feature_delivery_available_cores(void)
{
   JNIEnv *env                   = jni_thread_getenv();
   struct android_app *app       = (struct android_app*)g_android;
   struct string_list *core_list = string_list_new();
   union string_list_elem_attr attr;
   jobjectArray available_cores;
   jsize num_cores;
   size_t i;

   attr.i = 0;

   if (!env ||
       !app ||
       !app->getAvailableCores ||
       !core_list)
      goto error;

   /* Get list of available cores */
   CALL_OBJ_METHOD(env, available_cores, app->activity->clazz,
         app->getAvailableCores);
   num_cores = (*env)->GetArrayLength(env, available_cores);

   for (i = 0; i < num_cores; i++)
   {
      /* Extract element of available cores array */
      jstring core_name_jni = (jstring)
            ((*env)->GetObjectArrayElement(env, available_cores, i));
      const char *core_name = NULL;

      /* Convert Java-style string to a proper char array */
      core_name = (*env)->GetStringUTFChars(env, core_name_jni, NULL);

      if (core_name && *core_name)
      {
         char core_file[256];
         /* Generate core file name */
         size_t _len = strlcpy(core_file, core_name, sizeof(core_file));
         strlcpy_lit(core_file       + _len,
               "_libretro_android.so",
               sizeof(core_file) - _len);
         /* Add entry to list */
         if (*core_file)
            string_list_append(core_list, core_file, attr);
      }

      /* Must always 'release' the converted string */
      (*env)->ReleaseStringUTFChars(env, core_name_jni, core_name);
   }

   if (core_list->size < 1)
      goto error;

   return core_list;

error:
   if (core_list)
      string_list_free(core_list);

   return NULL;
}

/* Returns true if specified core is currently
 * installed via play feature delivery */
bool play_feature_delivery_core_installed(const char *core_file)
{
   JNIEnv *env             = jni_thread_getenv();
   struct android_app *app = (struct android_app*)g_android;
   jobjectArray installed_cores;
   jsize num_cores;
   char core_name[256];
   size_t i;

   core_name[0] = '\0';

   if (!env ||
       !app ||
       !app->getInstalledCores)
      return false;

   /* Extract core name */
   if (!play_feature_delivery_get_core_name(
         core_file, core_name, sizeof(core_name)))
      return false;

   /* Get list of installed cores */
   CALL_OBJ_METHOD(env, installed_cores, app->activity->clazz,
         app->getInstalledCores);
   num_cores = (*env)->GetArrayLength(env, installed_cores);

   for (i = 0; i < num_cores; i++)
   {
      /* Extract element of installed cores array */
      jstring installed_core_name_jni = (jstring)
            ((*env)->GetObjectArrayElement(env, installed_cores, i));
      const char *installed_core_name = NULL;

      /* Convert Java-style string to a proper char array */
      installed_core_name = (*env)->GetStringUTFChars(
            env, installed_core_name_jni, NULL);

      /* Check for a match */
      if (  (installed_core_name && *installed_core_name)
          && string_is_equal(core_name, installed_core_name))
      {
         /* Must always 'release' the converted string */
         (*env)->ReleaseStringUTFChars(env,
               installed_core_name_jni, installed_core_name);
         return true;
      }

      /* Must always 'release' the converted string */
      (*env)->ReleaseStringUTFChars(env,
            installed_core_name_jni, installed_core_name);
   }

   return false;
}

/* Fetches last recorded status of the most
 * recently initiated play feature delivery
 * install transaction.
 * 'progress' is an integer from 0-100.
 * Returns true if a transaction is currently
 * in progress. */
bool play_feature_delivery_download_status(
      enum play_feature_delivery_install_status *status,
      unsigned *progress)
{
   int w = retro_atomic_load_acquire_int(
         &play_feature_delivery_get_state()->word);

   if (status)
      *status   = PFD_STATUS(w);

   if (progress)
      *progress = (unsigned)(w & PFD_PROGRESS_MASK);

   return (w & PFD_ACTIVE) != 0;
}

/***********/
/* Control */
/***********/

/* Initialises download of the specified core.
 * Returns false in the event of an error.
 * Download status should be monitored via
 * play_feature_delivery_download_status() */
bool play_feature_delivery_download(const char *core_file, uint32_t waiter)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();
   JNIEnv *env                          = jni_thread_getenv();
   struct android_app *app              = (struct android_app*)g_android;
   bool ret                             = false;
   char core_name[256];
   jstring core_name_jni;

   core_name[0] = '\0';

   if (   !env
       || !app
       || !app->downloadCore)
      return false;

   /* Extract core name */
   if (!play_feature_delivery_get_core_name(
         core_file, core_name, sizeof(core_name)))
      return false;

   /* We only support one download at a time. CLAIMING keeps the
    * callbacks and other claims off until the transaction is set up;
    * Java is asked once it is visible, as it may answer on this
    * thread. */
   for (;;)
   {
      int w = retro_atomic_load_acquire_int(&state->word);
      if (w & (PFD_ACTIVE | PFD_CLAIMING))
         break;
      if (retro_atomic_cas_int(&state->word, w, w | PFD_CLAIMING))
      {
         retro_atomic_store_relaxed_int(&state->name_hash,
               play_feature_delivery_name_hash(core_name));
         retro_atomic_store_relaxed_int(&state->waiter, (int)waiter);
         retro_atomic_store_release_int(&state->word,
               (int)(((unsigned)w + PFD_GEN_ONE) & PFD_GEN_MASK)
               | PFD_ACTIVE
               | (PLAY_FEATURE_DELIVERY_PENDING << PFD_STATUS_SHIFT));
         ret = true;
         break;
      }
   }

   if (ret)
   {
      /* Convert core name to a Java-style string */
      core_name_jni = (*env)->NewStringUTF(env, core_name);

      /* Request download */
      CALL_VOID_METHOD_PARAM(env, app->activity->clazz,
            app->downloadCore, core_name_jni);

      /* Free core_name_jni reference */
      (*env)->DeleteLocalRef(env, core_name_jni);
   }

   return ret;
}

/* Deletes specified core.
 * Returns false in the event of an error. */
bool play_feature_delivery_delete(const char *core_file)
{
   JNIEnv *env             = jni_thread_getenv();
   struct android_app *app = (struct android_app*)g_android;
   char core_name[256];
   jstring core_name_jni;

   core_name[0] = '\0';

   if (   !env
       || !app
       || !app->deleteCore)
      return false;

   /* Extract core name */
   if (!play_feature_delivery_get_core_name(
         core_file, core_name, sizeof(core_name)))
      return false;

   /* Convert to a Java-style string */
   core_name_jni = (*env)->NewStringUTF(env, core_name);

   /* Request core deletion */
   CALL_VOID_METHOD_PARAM(env, app->activity->clazz,
         app->deleteCore, core_name_jni);

   /* Free core_name_jni reference */
   (*env)->DeleteLocalRef(env, core_name_jni);

   return true;
}
