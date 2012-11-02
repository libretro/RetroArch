/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include <jni.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include "android_general.h"
#include "../../../general.h"
#include "../../../performance.h"

static inline void android_app_write_cmd(struct android_app* android_app, int8_t cmd)
{
   write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd);
}

JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *pvt)
{
   return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnLoad( JavaVM *vm, void *pvt) { }

static void print_cur_config(struct android_app* android_app)
{
   char lang[2], country[2];
   AConfiguration_getLanguage(android_app->config, lang);
   AConfiguration_getCountry(android_app->config, country);

   /*
      LOGV("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
      "keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
      "modetype=%d modenight=%d",
      AConfiguration_getMcc(android_app->config),
      AConfiguration_getMnc(android_app->config),
      lang[0], lang[1], country[0], country[1],
      AConfiguration_getOrientation(android_app->config),
      AConfiguration_getTouchscreen(android_app->config),
      AConfiguration_getDensity(android_app->config),
      AConfiguration_getKeyboard(android_app->config),
      AConfiguration_getNavigation(android_app->config),
      AConfiguration_getKeysHidden(android_app->config),
      AConfiguration_getNavHidden(android_app->config),
      AConfiguration_getSdkVersion(android_app->config),
      AConfiguration_getScreenSize(android_app->config),
      AConfiguration_getScreenLong(android_app->config),
      AConfiguration_getUiModeType(android_app->config),
      AConfiguration_getUiModeNight(android_app->config));
      */
}

/**
 * Process the next main command.
 */
void engine_handle_cmd(struct android_app* android_app, int32_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
         RARCH_LOG("engine_handle_cmd: APP_CMD_INPUT_CHANGED.\n");
         pthread_mutex_lock(&android_app->mutex);
         if (android_app->inputQueue != NULL)
            AInputQueue_detachLooper(android_app->inputQueue);

         android_app->inputQueue = android_app->pendingInputQueue;
         if (android_app->inputQueue != NULL)
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  &android_app->inputPollSource);
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_SAVE_STATE:
         RARCH_LOG("engine_handle_cmd: APP_CMD_SAVE_STATE.\n");
         // The system has asked us to save our current state.  Do so.
         break;
      case APP_CMD_INIT_WINDOW:
         RARCH_LOG("engine_handle_cmd: APP_CMD_INIT_WINDOW.\n");
         // The window is being shown, get it ready.
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = android_app->pendingWindow;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);

         if (android_app->window != NULL)
            g_android.window_inited = true;
         break;
      case APP_CMD_START:
         RARCH_LOG("engine_handle_cmd: APP_CMD_START.\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_RESUME:
         RARCH_LOG("engine_handle_cmd: APP_CMD_RESUME.\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_STOP:
         RARCH_LOG("engine_handle_cmd: APP_CMD_STOP.\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_PAUSE:
         RARCH_LOG("engine_handle_cmd: APP_CMD_PAUSE.\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         g_android.init_quit = true;
         break;
      case APP_CMD_TERM_WINDOW:
         RARCH_LOG("engine_handle_cmd: APP_CMD_TERM_WINDOW.\n");
         // The window is being hidden or closed, clean it up.
         pthread_cond_broadcast(&android_app->cond);

         pthread_mutex_lock(&android_app->mutex);
         android_app->window = NULL;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_GAINED_FOCUS:
         RARCH_LOG("engine_handle_cmd: APP_CMD_GAINED_FOCUS.\n");
         // When our app gains focus, we start monitoring the accelerometer.
         break;
      case APP_CMD_LOST_FOCUS:
         RARCH_LOG("engine_handle_cmd: APP_CMD_LOST_FOCUS.\n");
         /*
            if (!g_android.window_inited)
            {
            }
            */
         break;
      case APP_CMD_CONFIG_CHANGED:
         RARCH_LOG("engine_handle_cmd: APP_CMD_CONFIG_CHANGED.\n");
         AConfiguration_fromAssetManager(android_app->config,
               android_app->activity->assetManager);
         print_cur_config(android_app);
         break;
      case APP_CMD_DESTROY:
         RARCH_LOG("engine_handle_cmd: APP_CMD_DESTROY.\n");
         android_app->destroyRequested = 1;
         break;
   }
}

static void android_get_char_argv(char *argv, size_t sizeof_argv, const char *arg_name)
{
   JNIEnv *env;
   JavaVM *rarch_vm = g_android.app->activity->vm;

   (*rarch_vm)->AttachCurrentThread(rarch_vm, &env, 0);

   jobject me = g_android.app->activity->clazz;

   jclass acl = (*env)->GetObjectClass(env, me); //class pointer of NativeActivity
   jmethodID giid = (*env)->GetMethodID(env, acl, "getIntent", "()Landroid/content/Intent;");
   jobject intent = (*env)->CallObjectMethod(env, me, giid); //Got our intent

   jclass icl = (*env)->GetObjectClass(env, intent); //class pointer of Intent
   jmethodID gseid = (*env)->GetMethodID(env, icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

   jstring jsParam1 = (*env)->CallObjectMethod(env, intent, gseid, (*env)->NewStringUTF(env, arg_name));
   const char *test_argv = (*env)->GetStringUTFChars(env, jsParam1, 0);

   strncpy(argv, test_argv, sizeof_argv);

   (*env)->ReleaseStringUTFChars(env, jsParam1, test_argv);

   (*rarch_vm)->DetachCurrentThread(rarch_vm);
}

#define MAX_ARGS 32

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state)
{
   // Make sure glue isn't stripped.
   app_dummy();

   rarch_main_clear_state();

   RARCH_LOG("Native Activity started.\n");

   g_android.app = state;

   struct android_app* android_app = g_android.app;

   char rom_path[512];
   char libretro_path[512];

   // Get arguments */
   android_get_char_argv(rom_path, sizeof(rom_path), "ROM");
   android_get_char_argv(libretro_path, sizeof(libretro_path), "LIBRETRO");

   RARCH_LOG("Checking arguments passed...\n");
   RARCH_LOG("ROM Filename: [%s].\n", rom_path);
   RARCH_LOG("Libretro path: [%s].\n", libretro_path);

   /* ugly hack for now - hardcode libretro path to 'allowed' dir */
   snprintf(libretro_path, sizeof(libretro_path), "/data/data/com.retroarch/lib/libretro.so");

   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("retroarch");
   argv[argc++] = strdup(rom_path);
   argv[argc++] = strdup("-L");
   argv[argc++] = strdup(libretro_path);
   argv[argc++] = strdup("-v");

   g_extern.verbose = true;

   while(!g_android.window_inited)
   {
      // Read all pending events.
      int id;

      // Block forever waiting for events.
      while ((id = ALooper_pollOnce(0, NULL, 0, NULL)) >= 0)
      {
         // Process this event.
         if (id)
         {
            int8_t cmd;

            if (!read(android_app->msgread, &cmd, sizeof(cmd)) == sizeof(cmd))
               cmd = -1;

            engine_handle_cmd(android_app, cmd);
         }

         // Check if we are exiting.
         if (android_app->destroyRequested != 0)
            return;
      }
   }

   RARCH_LOG("Starting RetroArch...\n");

   rarch_main(argc, argv);
}


void app_dummy()
{
}

static void* android_app_entry(void* param)
{
   struct android_app* android_app = (struct android_app*)param;

   /* Init */
   android_app->config = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

   print_cur_config(android_app);

   android_app->cmdPollSource = LOOPER_ID_MAIN;
   android_app->inputPollSource = LOOPER_ID_INPUT;

   ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL,
         &android_app->cmdPollSource);
   android_app->looper = looper;

   pthread_mutex_lock(&android_app->mutex);
   android_app->running = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);

   /* Main function */
   android_main(android_app);

   /* Destroy */
   pthread_mutex_lock(&android_app->mutex);

   if (android_app->inputQueue != NULL)
      AInputQueue_detachLooper(android_app->inputQueue);

   AConfiguration_delete(android_app->config);
   android_app->destroyed = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);
   // Can't touch android_app object after this.

   return NULL;
}

// --------------------------------------------------------------------
// Native activity interaction (called from main thread)
// --------------------------------------------------------------------

static struct android_app* android_app_create(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   struct android_app* android_app = (struct android_app*)malloc(sizeof(struct android_app));
   memset(android_app, 0, sizeof(struct android_app));
   android_app->activity = activity;

   pthread_mutex_init(&android_app->mutex, NULL);
   pthread_cond_init(&android_app->cond, NULL);

   int msgpipe[2];
   if (pipe(msgpipe))
      return NULL;

   android_app->msgread = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   pthread_attr_t attr; 
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_create(&android_app->thread, &attr, android_app_entry, android_app);

   // Wait for thread to start.
   pthread_mutex_lock(&android_app->mutex);
   while (!android_app->running)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);

   return android_app;
}


static void android_app_set_input(struct android_app* android_app,
      AInputQueue* inputQueue)
{
   pthread_mutex_lock(&android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);

   while (android_app->inputQueue != android_app->pendingInputQueue)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_window(struct android_app* android_app, ANativeWindow* window)
{
   pthread_mutex_lock(&android_app->mutex);

   if (android_app->pendingWindow != NULL)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window != NULL)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_activity_state(struct android_app* android_app, int8_t cmd)
{
   pthread_mutex_lock(&android_app->mutex);

   android_app_write_cmd(android_app, cmd);

   while (android_app->activityState != cmd)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_free(struct android_app* android_app)
{
   pthread_mutex_lock(&android_app->mutex);
   android_app_write_cmd(android_app, APP_CMD_DESTROY);

   while (!android_app->destroyed)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);

   close(android_app->msgread);
   close(android_app->msgwrite);
   pthread_cond_destroy(&android_app->cond);
   pthread_mutex_destroy(&android_app->mutex);
   free(android_app);
}

static void onDestroy(ANativeActivity* activity)
{
   android_app_free((struct android_app*)activity->instance);
}

static void onStart(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_RESUME);
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen)
{
   return NULL;
}

static void onPause(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   android_app_set_activity_state((struct android_app*)activity->instance, APP_CMD_STOP);
}

static void onConfigurationChanged(ANativeActivity* activity)
{
   struct android_app* android_app = (struct android_app*)activity->instance;
   android_app_write_cmd(android_app, APP_CMD_CONFIG_CHANGED);
}

static void onLowMemory(ANativeActivity* activity)
{
   struct android_app* android_app = (struct android_app*)activity->instance;
   android_app_write_cmd(android_app, APP_CMD_LOW_MEMORY);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
{
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
{
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
{
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState,
      size_t savedStateSize)
{
   activity->callbacks->onDestroy = onDestroy;
   activity->callbacks->onStart = onStart;
   activity->callbacks->onResume = onResume;
   activity->callbacks->onPause = onPause;
   activity->callbacks->onStop = onStop;
   activity->callbacks->onConfigurationChanged = onConfigurationChanged;
   activity->callbacks->onLowMemory = onLowMemory;
   activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

   activity->instance = android_app_create(activity, savedState, savedStateSize);
}
