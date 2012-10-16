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

#include <EGL/egl.h> /* Requires NDK r5 or newer */
#include <GLES/gl.h>

#include "android-general.h"
#include "../../../general.h"

//forward declarations 
void gfx_ctx_swap_buffers(void);
void gfx_ctx_clear(void);
void gfx_ctx_destroy(void);
bool gfx_ctx_init(void);


JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *pvt)
{
   RARCH_LOG("JNI_OnLoad.\n" );

   return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnLoad( JavaVM *vm, void *pvt)
{
   RARCH_LOG("JNI_OnUnLoad.\n" );
}

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
   if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
   {
      g_android.animating = 1;
      g_android.state.x = AMotionEvent_getX(event, 0);
      g_android.state.y = AMotionEvent_getY(event, 0);
      return 1;
   }
   return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_SAVE_STATE:
         // The system has asked us to save our current state.  Do so.
         g_android.app->savedState = malloc(sizeof(struct saved_state));
	 *((struct saved_state*)g_android.app->savedState) = g_android.state;
	 g_android.app->savedStateSize = sizeof(struct saved_state);
	 break;
      case APP_CMD_INIT_WINDOW:
	 // The window is being shown, get it ready.
	 if (g_android.app->window != NULL)
            g_android.window_inited = true;
	 break;
      case APP_CMD_TERM_WINDOW:
	 // The window is being hidden or closed, clean it up.
	 //gfx_ctx_destroy();
	 break;
      case APP_CMD_GAINED_FOCUS:
	 // When our app gains focus, we start monitoring the accelerometer.
	 if (g_android.accelerometerSensor != NULL)
         {
            ASensorEventQueue_enableSensor(g_android.sensorEventQueue,
               g_android.accelerometerSensor);

	    // We'd like to get 60 events per second (in us).
	    ASensorEventQueue_setEventRate(g_android.sensorEventQueue,
               g_android.accelerometerSensor, (1000L/60)*1000);
	 }
	 break;
      case APP_CMD_LOST_FOCUS:
	 // When our app loses focus, we stop monitoring the accelerometer.
	 // This is to avoid consuming battery while not being used.
	 if (g_android.accelerometerSensor != NULL)
            ASensorEventQueue_disableSensor(g_android.sensorEventQueue,
	       g_android.accelerometerSensor);
	 // Also stop animating.
	 g_android.animating = 0;
	 break;
   }
}

static void android_get_char_argv(char *argv, size_t sizeof_argv, const char * arg_name, struct android_app* state)
{
   JNIEnv *env;
   JavaVM *rarch_vm = state->activity->vm;

   (*rarch_vm)->AttachCurrentThread(rarch_vm, &env, 0);

   jobject me = state->activity->clazz;

   jclass acl = (*env)->GetObjectClass(env, me); //class pointer of NativeActivity
   jmethodID giid = (*env)->GetMethodID(env, acl, "getIntent", "()Landroid/content/Intent;");
   jobject intent = (*env)->CallObjectMethod(env, me, giid); //Got our intent

   jclass icl = (*env)->GetObjectClass(env, intent); //class pointer of Intent
   jmethodID gseid = (*env)->GetMethodID(env, icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

   jstring jsParam1 = (*env)->CallObjectMethod(env, intent, gseid, (*env)->NewStringUTF(env, arg_name));
   const char *test_argv = (*env)->GetStringUTFChars(env, jsParam1, 0);

   strncpy(argv, test_argv, sizeof_argv);

   //When done with it, or when you've made a copy
   (*env)->ReleaseStringUTFChars(env, jsParam1, test_argv);
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

   state->onAppCmd = engine_handle_cmd;
   state->onInputEvent = engine_handle_input;
   g_android.app = state;

   char rom_path[512];
   char libretro_path[512];

   // Get arguments */
   android_get_char_argv(rom_path, sizeof(rom_path), "ROM", state);
   android_get_char_argv(libretro_path, sizeof(libretro_path), "LIBRETRO", state);

   RARCH_LOG("Checking arguments passed...\n");
   RARCH_LOG("ROM Filename: [%s].\n", rom_path);
   RARCH_LOG("Libretro path: [%s].\n", libretro_path);

   /* ugly hack for now - hardcode libretro path to 'allowed' dir */
   snprintf(libretro_path, sizeof(libretro_path), "/data/data/com.retroarch/lib/libretro.so");

   // Prepare to monitor accelerometer
   g_android.sensorManager = ASensorManager_getInstance();
   g_android.accelerometerSensor = ASensorManager_getDefaultSensor(g_android.sensorManager,
      ASENSOR_TYPE_ACCELEROMETER);
   g_android.sensorEventQueue = ASensorManager_createEventQueue(g_android.sensorManager,
      state->looper, LOOPER_ID_USER, NULL, NULL);

   if (state->savedState != NULL) // We are starting with a previous saved state; restore from it.
      g_android.state = *(struct saved_state*)state->savedState;

   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("retroarch");
   argv[argc++] = strdup(rom_path);
   argv[argc++] = strdup("-L");
   argv[argc++] = strdup(libretro_path);
   argv[argc++] = strdup("-v");

   while(!g_android.window_inited)
   {
      // Read all pending events.
      int ident;
      int events;
      struct android_poll_source* source;
      struct android_app* state = g_android.app;

      // If not animating, we will block forever waiting for events.
      // If animating, we loop until all events are read, then continue
      // to draw the next frame of animation.
      while ((ident=ALooper_pollAll(g_android.animating ? 0 : -1, NULL, &events,
				      (void**)&source)) >= 0)
      {
         // Process this event.
         if (source != NULL)
            source->process(state, source);

	 // If a sensor has data, process it now.
	 if (ident == LOOPER_ID_USER && g_android.accelerometerSensor != NULL)
	 {
		 ASensorEvent event;
		 while (ASensorEventQueue_getEvents(g_android.sensorEventQueue, &event, 1) > 0)
			 RARCH_LOG("accelerometer: x=%f y=%f z=%f.\n", event.acceleration.x,
					 event.acceleration.y, event.acceleration.z);
	 }

	 // Check if we are exiting.
	 if (state->destroyRequested != 0)
	    return;
      }

      if (g_android.animating)
      {
         // Done with events; draw next animation frame.
         g_android.state.angle += .01f;

	 if (g_android.state.angle > 1)
            g_android.state.angle = 0;

	 // Drawing is throttled to the screen update rate, so there
	 // is no need to do timing here.
	 //engine_draw_frame();
      }
   }

   RARCH_LOG("Start RetroArch...\n");

   rarch_main(argc, argv);
}
