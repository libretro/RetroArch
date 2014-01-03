/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include "platform_android.h"
#include "../menu/menu_common.h"
#include "../../android/native/jni/jni_macros.h"

#include "../../conf/config_file.h"
#include "../../general.h"
#include "../../file.h"

struct android_app *g_android;
static pthread_key_t thread_key;

//forward decls
static void system_deinit(void *data);
static void system_shutdown(bool unused);
extern void android_app_entry(void *args);

void engine_handle_cmd(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   int8_t cmd;

   if (read(android_app->msgread, &cmd, sizeof(cmd)) != sizeof(cmd))
      cmd = -1;

   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
         slock_lock(android_app->mutex);

         if (android_app->inputQueue)
            AInputQueue_detachLooper(android_app->inputQueue);

         android_app->inputQueue = android_app->pendingInputQueue;

         if (android_app->inputQueue)
         {
            RARCH_LOG("Attaching input queue to looper");
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  NULL);
         }

         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         
         break;

      case APP_CMD_INIT_WINDOW:
         slock_lock(android_app->mutex);
         android_app->window = android_app->pendingWindow;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         if (g_extern.lifecycle_state & (1ULL << RARCH_PAUSE_TOGGLE))
            init_drivers();
         break;

      case APP_CMD_RESUME:
         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_START:
         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_PAUSE:
         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);

         if (!(g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY)))
         {
            RARCH_LOG("Pausing RetroArch.\n");
            g_extern.lifecycle_state |= (1ULL << RARCH_PAUSE_TOGGLE);
         }
         break;

      case APP_CMD_STOP:
         slock_lock(android_app->mutex);
         android_app->activityState = cmd;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_CONFIG_CHANGED:
         break;
      case APP_CMD_TERM_WINDOW:
         slock_lock(android_app->mutex);

         /* The window is being hidden or closed, clean it up. */
         /* terminate display/EGL context here */
         if (g_extern.lifecycle_state & (1ULL << RARCH_PAUSE_TOGGLE))
            uninit_drivers();
         else
            RARCH_WARN("Window is terminated outside PAUSED state.\n");

         android_app->window = NULL;
         scond_broadcast(android_app->cond);
         slock_unlock(android_app->mutex);
         break;

      case APP_CMD_GAINED_FOCUS:
         g_extern.lifecycle_state &= ~(1ULL << RARCH_PAUSE_TOGGLE);

         if ((android_app->sensor_state_mask & (1ULL << RETRO_SENSOR_ACCELEROMETER_ENABLE))
               && android_app->accelerometerSensor == NULL)
            android_input_set_sensor_state(driver.input_data, 0, RETRO_SENSOR_ACCELEROMETER_ENABLE,
                  android_app->accelerometer_event_rate);
         break;
      case APP_CMD_LOST_FOCUS:
         // Avoid draining battery while app is not being used
         if ((android_app->sensor_state_mask & (1ULL << RETRO_SENSOR_ACCELEROMETER_ENABLE))
               && android_app->accelerometerSensor != NULL)
            android_input_set_sensor_state(driver.input_data, 0, RETRO_SENSOR_ACCELEROMETER_DISABLE,
                  android_app->accelerometer_event_rate);
         break;

      case APP_CMD_DESTROY:
         g_extern.lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
         break;
   }
}

static inline void android_app_write_cmd (void *data, int8_t cmd)
{
   struct android_app *android_app = (struct android_app*)data;
   if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
      RARCH_ERR("Failure writing android_app cmd: %s\n", strerror(errno));
}

static void android_app_set_input (void *data, AInputQueue* inputQueue)
{
   struct android_app *android_app = (struct android_app*)data;
   slock_lock(android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);
   while (android_app->inputQueue != android_app->pendingInputQueue)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);
}

static void android_app_set_window (void *data, ANativeWindow* window)
{
   struct android_app *android_app = (struct android_app*)data;
   slock_lock(android_app->mutex);
   if (android_app->pendingWindow != NULL)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window != NULL)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_activity_state (void *data, int8_t cmd)
{
   struct android_app *android_app = (struct android_app*)data;
   slock_lock(android_app->mutex);
   android_app_write_cmd(android_app, cmd);
   while (android_app->activityState != cmd && android_app->activityState != APP_CMD_DEAD)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   if (android_app->activityState == APP_CMD_DEAD)
      RARCH_LOG("RetroArch thread is dead.\n");
}

static void onDestroy(ANativeActivity* activity)
{
   RARCH_LOG("Destroy: %p\n", activity);
   struct android_app* android_app = (struct android_app*)activity->instance;

   sthread_join(android_app->thread);
   RARCH_LOG("Joined with RetroArch thread.\n");

   close(android_app->msgread);
   close(android_app->msgwrite);
   scond_free(android_app->cond);
   slock_free(android_app->mutex);
   free(android_app);
}

static void onStart(ANativeActivity* activity)
{
   RARCH_LOG("Start: %p\n", activity);
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   RARCH_LOG("Resume: %p\n", activity);
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_RESUME);
}

static void onPause(ANativeActivity* activity)
{
   RARCH_LOG("Pause: %p\n", activity);
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   RARCH_LOG("Stop: %p\n", activity);
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_STOP);
}

static void onConfigurationChanged (ANativeActivity *activity)
{
   struct android_app* android_app = (struct android_app*)activity->instance;
   RARCH_LOG("ConfigurationChanged: %p\n", activity);
   android_app_write_cmd(android_app, APP_CMD_CONFIG_CHANGED);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   RARCH_LOG("WindowFocusChanged: %p -- %d\n", activity, focused);
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
{
   RARCH_LOG("NativeWindowCreated: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
{
   RARCH_LOG("NativeWindowDestroyed: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   RARCH_LOG("InputQueueCreated: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
{
   RARCH_LOG("InputQueueDestroyed: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

JNIEnv *jni_thread_getenv(void)
{
   struct android_app* android_app = (struct android_app*)g_android;
   JNIEnv *env;
   int status = (*android_app->activity->vm)->AttachCurrentThread(android_app->activity->vm, &env, 0);
   if (status < 0)
   {
      RARCH_ERR("jni_thread_getenv: Failed to attach current thread.\n");
      return NULL;
   }
   pthread_setspecific(thread_key, (void*)env);

   return env;
}

static void jni_thread_destruct(void *value)
{
   struct android_app* android_app = (struct android_app*)g_android;
   JNIEnv *env = (JNIEnv*)value;
   if (env)
   {
      if (android_app)
         (*android_app->activity->vm)->DetachCurrentThread(android_app->activity->vm);
      pthread_setspecific(thread_key, NULL);
   }
}

// --------------------------------------------------------------------
// Native activity interaction (called from main thread)
// --------------------------------------------------------------------

void ANativeActivity_onCreate(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   (void)savedState;
   (void)savedStateSize;

   RARCH_LOG("Creating Native Activity: %p\n", activity);
   activity->callbacks->onDestroy = onDestroy;
   activity->callbacks->onStart = onStart;
   activity->callbacks->onResume = onResume;
   activity->callbacks->onSaveInstanceState = NULL;
   activity->callbacks->onPause = onPause;
   activity->callbacks->onStop = onStop;
   activity->callbacks->onConfigurationChanged = onConfigurationChanged;
   activity->callbacks->onLowMemory = NULL;
   activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

   // these are set only for the native activity, and are reset when it ends
   ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);

   if (pthread_key_create(&thread_key, jni_thread_destruct))
      RARCH_ERR("Error initializing pthread_key\n");

   struct android_app* android_app = (struct android_app*)malloc(sizeof(struct android_app));
   memset(android_app, 0, sizeof(struct android_app));
   android_app->activity = activity;

   android_app->mutex = slock_new();
   android_app->cond  = scond_new();

   int msgpipe[2];
   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      activity->instance = NULL;
   }
   android_app->msgread = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   android_app->thread = (sthread_t*)sthread_create(android_app_entry, android_app);

   // Wait for thread to start.
   slock_lock(android_app->mutex);
   while (!android_app->running)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   activity->instance = android_app;
}

static bool android_run_events (void *data)
{
   int id = ALooper_pollOnce(-1, NULL, NULL, NULL);

   if (id == LOOPER_ID_MAIN)
      engine_handle_cmd(driver.input_data);

   // Check if we are exiting.
   if (g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY))
      return false;

   return true;
}

static void get_environment_settings(int argc, char *argv[], void *data)
{
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)data;
   jobject obj = NULL;
   jstring jstr = NULL;
   bool valschanged = false;

   if (!android_app)
      return;

   env = jni_thread_getenv();
   if (!env)
      return;

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz, android_app->getIntent);
   RARCH_LOG("Checking arguments passed from intent...\n");

   // ROM
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra, (*env)->NewStringUTF(env, "ROM"));
   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_extern.fullpath, argv, sizeof(g_extern.fullpath));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      RARCH_LOG("ROM Filename: [%s].\n", g_extern.fullpath);
   }

   // Config file
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra, (*env)->NewStringUTF(env, "CONFIGFILE"));
   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_extern.config_path, argv, sizeof(g_extern.config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      RARCH_LOG("Config file: [%s].\n", g_extern.config_path);
   }

   // Current IME
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra, (*env)->NewStringUTF(env, "IME"));
   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(android_app->current_ime, argv, sizeof(android_app->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra, (*env)->NewStringUTF(env, "USED"));
   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      bool used = (strcmp(argv, "false") == 0) ? false : true;
      RARCH_LOG("USED: [%s].\n", used ? "true" : "false");
      (*env)->ReleaseStringUTFChars(env, jstr, argv);
   }


   if (valschanged)
   {
      g_extern.block_config_read = false;
      config_load();
      g_extern.block_config_read = true;
   }

   //LIBRETRO
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra, (*env)->NewStringUTF(env, "LIBRETRO"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_settings.libretro, argv, sizeof(g_settings.libretro));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);
   }

   RARCH_LOG("Libretro path: [%s].\n", g_settings.libretro);

}

static void process_pending_intent(void *data)
{
   RARCH_LOG("process_pending_intent.\n");
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)data;
   jstring jstr = NULL;
   bool valschanged = false;
   bool startgame = false;

   if (!android_app)
      return;

   env = jni_thread_getenv();
   if (!env)
      return;

   // ROM
   jstr = (*env)->CallObjectMethod(env, android_app->activity->clazz, android_app->getPendingIntentFullPath);
   JNI_EXCEPTION(env);
   RARCH_LOG("Checking arguments passed from intent...\n");
   if (android_app->getPendingIntentFullPath && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_extern.fullpath, argv, sizeof(g_extern.fullpath));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      startgame = true;
      RARCH_LOG("ROM Filename: [%s].\n", g_extern.fullpath);
   }

   // Config file
   jstr = (*env)->CallObjectMethod(env, android_app->activity->clazz, android_app->getPendingIntentConfigPath);
   JNI_EXCEPTION(env);
   if (android_app->getPendingIntentConfigPath && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_extern.config_path, argv, sizeof(g_extern.config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      RARCH_LOG("Config file: [%s].\n", g_extern.config_path);
   }

   // Current IME
   jstr = (*env)->CallObjectMethod(env, android_app->activity->clazz, android_app->getPendingIntentIME);
   JNI_EXCEPTION(env);
   if (android_app->getPendingIntentIME && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(android_app->current_ime, argv, sizeof(android_app->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      valschanged = true;
      RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);
   }

   if (valschanged)
   {
      g_extern.block_config_read = false;
      config_load();
      g_extern.block_config_read = true;
   }

   //LIBRETRO
   jstr = (*env)->CallObjectMethod(env, android_app->activity->clazz, android_app->getPendingIntentLibretroPath);
   JNI_EXCEPTION(env);
   if (android_app->getPendingIntentLibretroPath && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(g_settings.libretro, argv, sizeof(g_settings.libretro));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);
   }

   RARCH_LOG("Libretro path: [%s].\n", g_settings.libretro);

   if (startgame)
   {
      RARCH_LOG("Starting new game %s...\n", g_extern.fullpath);
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_PREINIT);
      g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
      load_menu_game_new_core();
   }

   CALL_VOID_METHOD(env, android_app->activity->clazz, android_app->clearPendingIntent);
}

static int process_events(void *data)
{
   jboolean hasPendingIntent;
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)data;

   if (input_key_pressed_func(RARCH_PAUSE_TOGGLE))
         android_run_events(android_app);

   env = jni_thread_getenv();
   if (!env)
      return -1;

   CALL_BOOLEAN_METHOD(env, hasPendingIntent, android_app->activity->clazz, android_app->hasPendingIntent);
   if (hasPendingIntent)
      process_pending_intent(android_app);

   return 0;
}

static void system_init(void *data)
{
   JNIEnv *env;
   jclass class = NULL;
   jobject obj = NULL;
   struct android_app* android_app = (struct android_app*)data;

   ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   slock_lock(android_app->mutex);
   android_app->running = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);

   memset(&g_android, 0, sizeof(g_android));
   g_android = android_app;

   RARCH_LOG("Native Activity started.\n");
   rarch_main_clear_state();
   rarch_init_msg_queue();

   while (!android_app->window)
   {
      if (!android_run_events(android_app))
      {
         system_deinit(android_app);
         system_shutdown(android_app);
      }
   }

   env = jni_thread_getenv();
   if (!env)
      return;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   GET_METHOD_ID(env, android_app->getIntent, class, "getIntent", "()Landroid/content/Intent;");
   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz, android_app->getIntent);
   GET_METHOD_ID(env, android_app->hasPendingIntent, class, "hasPendingIntent", "()Z");
   GET_METHOD_ID(env, android_app->clearPendingIntent, class, "clearPendingIntent", "()V");
   GET_METHOD_ID(env, android_app->getPendingIntentConfigPath, class, "getPendingIntentConfigPath",
         "()Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->getPendingIntentLibretroPath, class, "getPendingIntentLibretroPath",
         "()Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->getPendingIntentFullPath, class, "getPendingIntentFullPath",
         "()Ljava/lang/String;");
   GET_METHOD_ID(env, android_app->getPendingIntentIME, class, "getPendingIntentIME",
         "()Ljava/lang/String;");

   GET_OBJECT_CLASS(env, class, obj);
   GET_METHOD_ID(env, android_app->getStringExtra, class, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
}

static void system_deinit(void *data)
{
   struct android_app* android_app = (struct android_app*)data;

   RARCH_LOG("Deinitializing RetroArch...\n");
   android_app->activityState = APP_CMD_DEAD;

   RARCH_LOG("android_app_destroy!");
   if (android_app->inputQueue != NULL)
      AInputQueue_detachLooper(android_app->inputQueue);
}

static void system_shutdown(bool unused)
{
   (void)unused;
   // exit() here is nasty.
   // pthread_exit(NULL) or return NULL; causes hanging ...
   // Should probably call ANativeActivity_finish(), but it's bugged, it will hang our app.
   exit(0);
}

const frontend_ctx_driver_t frontend_ctx_android = {
   get_environment_settings,     /* get_environment_settings */
   system_init,                  /* init */
   system_deinit,                /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   process_events,               /* process_events */
   NULL,                         /* exec */
   system_shutdown,              /* shutdown */
   "android",
};
