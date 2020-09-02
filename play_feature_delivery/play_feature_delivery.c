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

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_assert.h>
#include <stdlib.h>
#endif

#include <string/stdstring.h>
#include "../frontend/drivers/platform_unix.h"

#include "play_feature_delivery.h"

/***************************/
/* Globals (do not fix...) */
/***************************/

/* Due to the way the JNI interface works,
 * core download status updates happen
 * asynchronously in a manner that we cannot
 * capture using any standard means. We therefore
 * have to implement status monitoring via an
 * ugly hack, involving a mutex-locked global
 * status struct... */

typedef struct
{
#ifdef HAVE_THREADS
   slock_t *enabled_lock;
   slock_t *status_lock;
#endif
   unsigned download_progress;
   enum play_feature_delivery_install_status last_status;
   char last_core_name[256];
   bool enabled;
   bool enabled_set;
   bool active;
} play_feature_delivery_state_t;

static play_feature_delivery_state_t play_feature_delivery_state = {

#ifdef HAVE_THREADS
   NULL,                       /* enabled_lock */
   NULL,                       /* status_lock */
#endif
   0,                          /* download_progress */
   PLAY_FEATURE_DELIVERY_IDLE, /* last_status */
   {'\0'},                     /* last_core_name */
   false,                      /* enabled */
   false,                      /* enabled_set */
   false,                      /* active */
};

static play_feature_delivery_state_t* play_feature_delivery_get_state(void)
{
   return &play_feature_delivery_state;
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

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(state->status_lock);
#endif

   /* Only update status if an install is active */
   if (state->active)
   {
      /* Convert Java-style string to a proper char array */
      const char *core_name_c = (*env)->GetStringUTFChars(
            env, core_name, NULL);

      /* Ensure that status update is for the
       * correct core */
      if (string_is_equal(state->last_core_name, core_name_c))
      {
         if (successful)
            state->last_status = PLAY_FEATURE_DELIVERY_STARTING;
         else
         {
            state->last_status = PLAY_FEATURE_DELIVERY_FAILED;
            state->active      = false;
         }
      }

      /* Must always 'release' the converted string */
      (*env)->ReleaseStringUTFChars(env, core_name, core_name_c);
   }

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(state->status_lock);
#endif
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

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(state->status_lock);
#endif

   /* Only update status if an install is active */
   if (state->active)
   {
      /* Note: core_names is a list of cores that
       * are currently installing. We should check
       * that state->last_core_name is in this list
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
         state->download_progress = (unsigned)
               (((float)bytes_downloaded * 100.0f /
                     (float)total_bytes_to_download) + 0.5f);
         state->download_progress = (state->download_progress > 100) ?
               100 : state->download_progress;
      }
      else
         state->download_progress = 100;

      /* Check status */
      switch (status)
      {
         case 0: /* INSTALL_STATUS_DOWNLOADING */
            state->last_status = PLAY_FEATURE_DELIVERY_DOWNLOADING;
            break;
         case 1: /* INSTALL_STATUS_INSTALLING */
            state->last_status = PLAY_FEATURE_DELIVERY_INSTALLING;
            break;
         case 2: /* INSTALL_STATUS_INSTALLED */
            state->last_status = PLAY_FEATURE_DELIVERY_INSTALLED;
            state->active      = false;
            break;
         case 3: /* INSTALL_STATUS_FAILED */
         default:
            state->last_status = PLAY_FEATURE_DELIVERY_FAILED;
            state->active      = false;
            break;
      }
   }

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(state->status_lock);
#endif
}

/******************/
/* Initialisation */
/******************/

/* Must be called upon program initialisation */
void play_feature_delivery_init(void)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();

   play_feature_delivery_deinit();

#ifdef HAVE_THREADS
   if (!state->enabled_lock)
      state->enabled_lock = slock_new();

   retro_assert(state->enabled_lock);

   if (!state->status_lock)
      state->status_lock = slock_new();

   retro_assert(state->status_lock);
#endif

   /* Note: Would like to cache whether this
    * is a Play Store build here, but
    * play_feature_delivery_init() is called
    * too early in the startup sequence... */
}

/* Must be called upon program termination */
void play_feature_delivery_deinit(void)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();

#ifdef HAVE_THREADS
   if (state->enabled_lock)
   {
      slock_free(state->enabled_lock);
      state->enabled_lock = NULL;
   }

   if (state->status_lock)
   {
      slock_free(state->status_lock);
      state->status_lock = NULL;
   }
#endif
}

/**********/
/* Status */
/**********/

static bool play_feature_delivery_get_core_name(
      const char *core_file, char *core_name, size_t len)
{
   size_t core_file_len;

   if (string_is_empty(core_file))
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
       !app->isPlayStoreBuild)
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
   bool enabled;

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(state->enabled_lock);
#endif

   /* Calling Java functions is slow. We need to
    * check Play Store build status frequently,
    * often in loops, so rely on a cached global
    * status flag instead dealing with Java
    * interfaces */
   if (!state->enabled_set)
   {
      state->enabled     = play_feature_delivery_enabled_internal();
      state->enabled_set = true;
   }

   enabled = state->enabled;

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(state->enabled_lock);
#endif

   return enabled;
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

      if (!string_is_empty(core_name))
      {
         char core_file[256];
         core_file[0] = '\0';

         /* Generate core file name */
         strlcpy(core_file, core_name, sizeof(core_file));
         strlcat(core_file, "_libretro_android.so", sizeof(core_file));

         /* Add entry to list */
         if (!string_is_empty(core_file))
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
      if (!string_is_empty(installed_core_name) &&
          string_is_equal(core_name, installed_core_name))
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
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();
   bool active;

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(state->status_lock);
#endif

   /* Copy status parameters */
   if (status)
      *status   = state->last_status;

   if (progress)
      *progress = state->download_progress;

   active       = state->active;

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(state->status_lock);
#endif

   return active;
}

/***********/
/* Control */
/***********/

/* Initialises download of the specified core.
 * Returns false in the event of an error.
 * Download status should be monitored via
 * play_feature_delivery_download_status() */
bool play_feature_delivery_download(const char *core_file)
{
   play_feature_delivery_state_t* state = play_feature_delivery_get_state();
   JNIEnv *env                          = jni_thread_getenv();
   struct android_app *app              = (struct android_app*)g_android;
   bool success                         = false;
   char core_name[256];
   jstring core_name_jni;

   core_name[0] = '\0';

   if (!env ||
       !app ||
       !app->downloadCore)
      return false;

   /* Extract core name */
   if (!play_feature_delivery_get_core_name(
         core_file, core_name, sizeof(core_name)))
      return false;

   /* Lock mutex */
#ifdef HAVE_THREADS
   slock_lock(state->status_lock);
#endif

   /* We only support one download at a time */
   if (!state->active)
   {
      /* Update status */
      state->download_progress = 0;
      state->last_status       = PLAY_FEATURE_DELIVERY_PENDING;
      state->active            = true;
      strlcpy(state->last_core_name, core_name,
            sizeof(state->last_core_name));

      /* Convert core name to a Java-style string */
      core_name_jni = (*env)->NewStringUTF(env, core_name);

      /* Request download */
      CALL_VOID_METHOD_PARAM(env, app->activity->clazz,
            app->downloadCore, core_name_jni);

      /* Free core_name_jni reference */
      (*env)->DeleteLocalRef(env, core_name_jni);

      success = true;
   }

   /* Unlock mutex */
#ifdef HAVE_THREADS
   slock_unlock(state->status_lock);
#endif

   return success;
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

   if (!env ||
       !app ||
       !app->deleteCore)
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
