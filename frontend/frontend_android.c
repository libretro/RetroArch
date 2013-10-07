/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include "frontend_android.h"
#include "../android/native/jni/jni_macros.h"
#include "../general.h"
#include "../performance.h"
#include "../driver.h"
#include "menu/rmenu.h"

#include "../config.def.h"

struct android_app *g_android;

static void print_cur_config (void *data)
{
   struct android_app *android_app = (struct android_app*)data;
   char lang[2], country[2];
   AConfiguration_getLanguage(android_app->config, lang);
   AConfiguration_getCountry(android_app->config, country);

   RARCH_LOG("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
         "keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
         "modetype=%d modenight=%d\n",
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
}

static bool android_run_events (void *data)
{
   int id = ALooper_pollOnce(-1, NULL, NULL, NULL);

   if (id == LOOPER_ID_MAIN)
      engine_handle_cmd();

   // Check if we are exiting.
   if (g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY))
      return false;

   return true;
}

static void jni_get (void *data_in, void *data_out)
{
   struct jni_params *in_params = (struct jni_params*)data_in;
   struct jni_out_params_char *out_args = (struct jni_out_params_char*)data_out;
   char obj_method_name[128];
   char obj_method_signature[128];
   jclass class_ptr = NULL;
   jobject obj = NULL;
   jmethodID giid = NULL;
   jstring ret_char;

   snprintf(obj_method_name, sizeof(obj_method_name), "getStringExtra");
   snprintf(obj_method_signature, sizeof(obj_method_signature), "(Ljava/lang/String;)Ljava/lang/String;");

   GET_OBJECT_CLASS(in_params->env, class_ptr, in_params->class_obj);
   GET_METHOD_ID(in_params->env, giid, class_ptr, in_params->method_name, in_params->method_signature);
   CALL_OBJ_METHOD(in_params->env, obj, in_params->class_obj, giid);

   GET_OBJECT_CLASS(in_params->env, class_ptr, obj);
   GET_METHOD_ID(in_params->env, giid, class_ptr, obj_method_name, obj_method_signature);

   CALL_OBJ_METHOD_PARAM(in_params->env, ret_char, obj, giid, (*in_params->env)->NewStringUTF(in_params->env, out_args->in));

   if (giid != NULL && ret_char)
   {
      const char *test_argv = (*in_params->env)->GetStringUTFChars(in_params->env, ret_char, 0);
      strncpy(out_args->out, test_argv, out_args->out_sizeof);
      (*in_params->env)->ReleaseStringUTFChars(in_params->env, ret_char, test_argv);
   }
}

static bool android_app_start_main(struct android_app *android_app)
{
   struct jni_params in_params;
   struct jni_out_params_char out_args;
   bool ret = false;

   in_params.java_vm = android_app->activity->vm;
   in_params.class_obj = android_app->activity->clazz;

   strlcpy(in_params.method_name, "getIntent", sizeof(in_params.method_name));
   strlcpy(in_params.method_signature, "()Landroid/content/Intent;", sizeof(in_params.method_signature));

   (*in_params.java_vm)->AttachCurrentThread(in_params.java_vm, &in_params.env, 0);

   // ROM
   out_args.out = g_extern.fullpath;
   out_args.out_sizeof = sizeof(g_extern.fullpath);
   strlcpy(out_args.in, "ROM", sizeof(out_args.in));
   jni_get(&in_params, &out_args);

   // libretro
   out_args.out = g_settings.libretro;
   out_args.out_sizeof = sizeof(g_settings.libretro);
   strlcpy(out_args.in, "LIBRETRO", sizeof(out_args.in));
   jni_get(&in_params, &out_args);

   // Config file
   out_args.out = g_extern.config_path;
   out_args.out_sizeof = sizeof(g_extern.config_path);
   strlcpy(out_args.in, "CONFIGFILE", sizeof(out_args.in));
   jni_get(&in_params, &out_args);

   // Current IME
   out_args.out = android_app->current_ime;
   out_args.out_sizeof = sizeof(android_app->current_ime);
   strlcpy(out_args.in, "IME", sizeof(out_args.in));
   jni_get(&in_params, &out_args);

   (*in_params.java_vm)->DetachCurrentThread(in_params.java_vm);

   RARCH_LOG("Checking arguments passed ...\n");
   RARCH_LOG("ROM Filename: [%s].\n", g_extern.fullpath);
   RARCH_LOG("Libretro path: [%s].\n", g_settings.libretro);
   RARCH_LOG("Config file: [%s].\n", g_extern.config_path);
   RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);

   config_load();

   menu_init();

   ret = load_menu_game();
   if (ret)
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);

   return ret;
}

static void *android_app_entry(void *data)
{
   struct android_app* android_app = (struct android_app*)data;

   android_app->config = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

   print_cur_config(android_app);

   ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   pthread_mutex_lock(&android_app->mutex);
   android_app->running = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);

   memset(&g_android, 0, sizeof(g_android));
   g_android = android_app;

   RARCH_LOG("Native Activity started.\n");
   rarch_main_clear_state();

   while (!android_app->window)
   {
      if (!android_run_events(android_app))
         goto exit;
   }

   rarch_init_msg_queue();

   if (!android_app_start_main(android_app))
   {
      g_settings.input.overlay[0] = 0;
      // threaded video doesn't work right for just displaying one frame
      g_settings.video.threaded = false;
      init_drivers();
      driver.video_poke->set_aspect_ratio(driver.video_data, ASPECT_RATIO_SQUARE);
      rarch_render_cached_frame();
      sleep(2);
      goto exit;
   }

   if (!g_extern.libretro_dummy)
      menu_rom_history_push_current();

   for (;;)
   {
      if (g_extern.system.shutdown)
         break;
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
      {
         load_menu_game_prepare();

         // If ROM load fails, we exit RetroArch. On console it might make more sense to go back to menu though ...
         if (load_menu_game())
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         else
         {
#ifdef RARCH_CONSOLE
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
#else
            return NULL;
#endif
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
         driver.input->poll(NULL);

         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

         // Main loop
         while (rarch_main_iterate());

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_PREINIT);

         // Menu should always run with vsync on
         video_set_nonblock_state_func(false);

         if (driver.audio_data)
            audio_stop_func();

         while((input_key_pressed_func(RARCH_PAUSE_TOGGLE)) ?
               android_run_events(android_app) : menu_iterate());

         driver_set_nonblock_state(driver.nonblock_state);

         if (driver.audio_data && !audio_start_func())
         {
            RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      }
      else
         break;
   }

exit:
   android_app->activityState = APP_CMD_DEAD;
   RARCH_LOG("Deinitializing RetroArch...\n");

   menu_free();

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   if (g_extern.main_is_init)
      rarch_main_deinit();

   rarch_deinit_msg_queue();
#ifdef PERF_TEST
   rarch_perf_log();
#endif
   rarch_main_clear_state();

   RARCH_LOG("android_app_destroy!");
   if (android_app->inputQueue != NULL)
      AInputQueue_detachLooper(android_app->inputQueue);
   AConfiguration_delete(android_app->config);

   // exit() here is nasty.
   // pthread_exit(NULL) or return NULL; causes hanging ...
   // Should probably call ANativeActivity_finish(), but it's bugged, it will hang our app.
   exit(0);
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
   pthread_mutex_lock(&android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);
   while (android_app->inputQueue != android_app->pendingInputQueue)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);
   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_window (void *data, ANativeWindow* window)
{
   struct android_app *android_app = (struct android_app*)data;
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

static void android_app_set_activity_state (void *data, int8_t cmd)
{
   struct android_app *android_app = (struct android_app*)data;
   pthread_mutex_lock(&android_app->mutex);
   android_app_write_cmd(android_app, cmd);
   while (android_app->activityState != cmd && android_app->activityState != APP_CMD_DEAD)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);
   pthread_mutex_unlock(&android_app->mutex);

   if (android_app->activityState == APP_CMD_DEAD)
      RARCH_LOG("RetroArch thread is dead.\n");
}

static void onDestroy(ANativeActivity* activity)
{
   RARCH_LOG("Destroy: %p\n", activity);
   struct android_app* android_app = (struct android_app*)activity->instance;

   RARCH_LOG("Joining with RetroArch thread.\n");
   pthread_join(android_app->thread, NULL);
   android_app->thread = 0;
   RARCH_LOG("Joined with RetroArch thread.\n");

   close(android_app->msgread);
   close(android_app->msgwrite);
   pthread_cond_destroy(&android_app->cond);
   pthread_mutex_destroy(&android_app->mutex);
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

   struct android_app* android_app = (struct android_app*)malloc(sizeof(struct android_app));
   memset(android_app, 0, sizeof(struct android_app));
   android_app->activity = activity;

   pthread_mutex_init(&android_app->mutex, NULL);
   pthread_cond_init(&android_app->cond, NULL);

   int msgpipe[2];
   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      activity->instance = NULL;
   }
   else
   {
      android_app->msgread = msgpipe[0];
      android_app->msgwrite = msgpipe[1];

      pthread_create(&android_app->thread, NULL, android_app_entry, android_app);

      // Wait for thread to start.
      pthread_mutex_lock(&android_app->mutex);
      while (!android_app->running)
         pthread_cond_wait(&android_app->cond, &android_app->mutex);
      pthread_mutex_unlock(&android_app->mutex);

      activity->instance = android_app;
   }
}
