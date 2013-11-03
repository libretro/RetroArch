/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../frontend_android.h"
#include "../../android/native/jni/jni_macros.h"

#include "../../conf/config_file.h"
#include "../../general.h"
#include "../../file.h"

struct android_app *g_android;

//forward decls
static void system_deinit(void *data);
static void system_shutdown(bool unused);

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
   char obj_method_name[128], obj_method_signature[128];
   jclass class_ptr = NULL;
   jobject obj = NULL;
   jmethodID giid = NULL;
   jstring ret_char;

   strlcpy(obj_method_name, "getStringExtra", sizeof(obj_method_name));
   strlcpy(obj_method_signature, "(Ljava/lang/String;)Ljava/lang/String;", sizeof(obj_method_signature));

   GET_OBJECT_CLASS(in_params->env, class_ptr, in_params->class_obj);
   GET_METHOD_ID(in_params->env, giid, class_ptr, in_params->method_name, in_params->method_signature);
   CALL_OBJ_METHOD(in_params->env, obj, in_params->class_obj, giid);

   GET_OBJECT_CLASS(in_params->env, class_ptr, obj);
   GET_METHOD_ID(in_params->env, giid, class_ptr, obj_method_name, obj_method_signature);

   CALL_OBJ_METHOD_PARAM(in_params->env, ret_char, obj, giid, (*in_params->env)->NewStringUTF(in_params->env, out_args->in));

   if (giid != NULL && ret_char)
   {
      const char *test_argv = (*in_params->env)->GetStringUTFChars(in_params->env, ret_char, 0);
      strlcpy(out_args->out, test_argv, out_args->out_sizeof);
      (*in_params->env)->ReleaseStringUTFChars(in_params->env, ret_char, test_argv);
   }
}

static void get_environment_settings(int argc, char *argv[], void *data)
{
   struct android_app* android_app = (struct android_app*)data;

   struct jni_params in_params;
   struct jni_out_params_char out_args;

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

   RARCH_LOG("Checking arguments passed ...\n");
   RARCH_LOG("ROM Filename: [%s].\n", g_extern.fullpath);
   RARCH_LOG("Config file: [%s].\n", g_extern.config_path);
   RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);

   config_load();

   // libretro
   out_args.out = g_settings.libretro;
   out_args.out_sizeof = sizeof(g_settings.libretro);
   strlcpy(out_args.in, "LIBRETRO", sizeof(out_args.in));
   jni_get(&in_params, &out_args);

   RARCH_LOG("Checking arguments passed ...\n");
   RARCH_LOG("Libretro path: [%s].\n", g_settings.libretro);

   (*in_params.java_vm)->DetachCurrentThread(in_params.java_vm);
}

static int process_events(void *data)
{
   struct android_app* android_app = (struct android_app*)data;

   if (input_key_pressed_func(RARCH_PAUSE_TOGGLE))
         android_run_events(android_app);

   return 0;
}

static void system_init(void *data)
{
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
