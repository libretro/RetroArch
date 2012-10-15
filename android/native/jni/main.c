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

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android_native_app_glue.h>

#include "../../../general.h"

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
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(void)
{
   // initialize OpenGL ES and EGL

   const EGLint attribs[] = 
   {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
    };
   EGLint w, h, dummy, format;
   EGLint numConfigs;
   EGLConfig config;
   EGLSurface surface;
   EGLContext context;

   EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   eglInitialize(display, 0, 0);

   /* Here, the application chooses the configuration it desires. In this
    * sample, we have a very simplified selection process, where we pick
    * the first EGLConfig that matches our criteria */
   eglChooseConfig(display, attribs, &config, 1, &numConfigs);

   /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
    * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
    * As soon as we picked a EGLConfig, we can safely reconfigure the
    * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
   eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

   ANativeWindow_setBuffersGeometry(g_android.app->window, 0, 0, format);

   surface = eglCreateWindowSurface(display, config, g_android.app->window, NULL);
   context = eglCreateContext(display, config, NULL, NULL);

   if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
   {
      RARCH_ERR("Unable to eglMakeCurrent.\n");
      return -1;
   }

   eglQuerySurface(display, surface, EGL_WIDTH, &w);
   eglQuerySurface(display, surface, EGL_HEIGHT, &h);

   g_android.display = display;
   g_android.context = context;
   g_android.surface = surface;
   g_android.width = w;
   g_android.height = h;
   g_android.state.angle = 0;

   // Initialize GL state.
   glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
   glEnable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);

   return 0;
}

/**
 * Just the current frame in the display.
 */
static void engine_draw_frame(void)
{
   if (g_android.display == NULL)
      return;

   // Just fill the screen with a color.
   glClearColor(((float)g_android.state.x)/g_android.width, g_android.state.angle,
      ((float)g_android.state.y)/g_android.height, 1);

   glClear(GL_COLOR_BUFFER_BIT);

   eglSwapBuffers(g_android.display, g_android.surface);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(void)
{
   if (g_android.display != EGL_NO_DISPLAY)
   {
      eglMakeCurrent(g_android.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      if (g_android.context != EGL_NO_CONTEXT)
         eglDestroyContext(g_android.display, g_android.context);
      if (g_android.surface != EGL_NO_SURFACE)
         eglDestroySurface(g_android.display, g_android.surface);

      eglTerminate(g_android.display);
   }
   g_android.animating = 0;
   g_android.display = EGL_NO_DISPLAY;
   g_android.context = EGL_NO_CONTEXT;
   g_android.surface = EGL_NO_SURFACE;
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
         {
            engine_init_display();
	    engine_draw_frame();
	 }
	 break;
      case APP_CMD_TERM_WINDOW:
	 // The window is being hidden or closed, clean it up.
	 engine_term_display();
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
	 engine_draw_frame();
	 break;
   }
}

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

   state->onAppCmd = engine_handle_cmd;
   state->onInputEvent = engine_handle_input;
   g_android.app = state;

   // Prepare to monitor accelerometer
   g_android.sensorManager = ASensorManager_getInstance();
   g_android.accelerometerSensor = ASensorManager_getDefaultSensor(g_android.sensorManager,
      ASENSOR_TYPE_ACCELEROMETER);
   g_android.sensorEventQueue = ASensorManager_createEventQueue(g_android.sensorManager,
      state->looper, LOOPER_ID_USER, NULL, NULL);

   if (state->savedState != NULL) // We are starting with a previous saved state; restore from it.
      g_android.state = *(struct saved_state*)state->savedState;

   // loop waiting for stuff to do.
   RARCH_LOG("Enter Android main loop.\n");

   while (1)
   {
      // Read all pending events.
      int ident;
      int events;
      struct android_poll_source* source;

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
               RARCH_WARN("accelerometer: x=%f y=%f z=%f.\n", event.acceleration.x,
               event.acceleration.y, event.acceleration.z);
	 }

	 // Check if we are exiting.
	 if (state->destroyRequested != 0)
         {
            engine_term_display();
	    return;
	 }
      }

      if (g_android.animating)
      {
         // Done with events; draw next animation frame.
         g_android.state.angle += .01f;

	 if (g_android.state.angle > 1)
            g_android.state.angle = 0;

	 // Drawing is throttled to the screen update rate, so there
	 // is no need to do timing here.
	 engine_draw_frame();
      }
   }
}
