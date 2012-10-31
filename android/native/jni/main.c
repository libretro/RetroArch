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

#include "android-general.h"
#include "../../../general.h"

JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *pvt)
{
   return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnLoad( JavaVM *vm, void *pvt) { }

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_SAVE_STATE:
         RARCH_LOG("engine_handle_cmd: APP_CMD_SAVE_STATE.\n");
         // The system has asked us to save our current state.  Do so.
         break;
      case APP_CMD_INIT_WINDOW:
         RARCH_LOG("engine_handle_cmd: APP_CMD_INIT_WINDOW.\n");
         // The window is being shown, get it ready.
         if (g_android.app->window != NULL)
            g_android.window_inited = true;
         break;
      case APP_CMD_START:
         RARCH_LOG("engine_handle_cmd: APP_CMD_START.\n");
         break;
      case APP_CMD_RESUME:
         RARCH_LOG("engine_handle_cmd: APP_CMD_RESUME.\n");
         break;
      case APP_CMD_STOP:
         RARCH_LOG("engine_handle_cmd: APP_CMD_STOP.\n");
         break;
      case APP_CMD_PAUSE:
         RARCH_LOG("engine_handle_cmd: APP_CMD_PAUSE.\n");
         g_android.init_quit = true;
         break;
      case APP_CMD_TERM_WINDOW:
         RARCH_LOG("engine_handle_cmd: APP_CMD_TERM_WINDOW.\n");
         // The window is being hidden or closed, clean it up.
         break;
      case APP_CMD_GAINED_FOCUS:
         RARCH_LOG("engine_handle_cmd: APP_CMD_GAINED_FOCUS.\n");
         // When our app gains focus, we start monitoring the accelerometer.
         break;
      case APP_CMD_LOST_FOCUS:
         RARCH_LOG("engine_handle_cmd: APP_CMD_LOST_FOCUS.\n");
         if (!g_android.window_inited)
         {
         }
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

   g_android.app->onAppCmd = engine_handle_cmd;

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
      struct android_poll_source* source;

      // Block forever waiting for events.
      while ((ALooper_pollOnce(0, NULL, 0, (void**)&source)) >= 0)
      {
         // Process this event.
         if (source != NULL)
            process_cmd(g_android.app, source);

         // Check if we are exiting.
         if (g_android.app->destroyRequested != 0)
            return;
      }
   }

   RARCH_LOG("Starting RetroArch...\n");

   rarch_main(argc, argv);
   exit(0);
}
