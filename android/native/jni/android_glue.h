/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _ANDROID_NATIVE_APP_GLUE_H
#define _ANDROID_NATIVE_APP_GLUE_H

#include <poll.h>
#include <pthread.h>
#include <sched.h>

#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>

struct android_app
{
   ANativeActivity* activity;
   AConfiguration* config;

   // This is the last instance's saved state, as provided at creation time.
   // It is NULL if there was no state.  You can use this as you need; the
   // memory will remain around until you call android_app_exec_cmd() for
   // APP_CMD_RESUME, at which point it will be freed and savedState set to NULL.
   // These variables should only be changed when processing a APP_CMD_SAVE_STATE,
   // at which point they will be initialized to NULL and you can malloc your
   // state and place the information here.  In that case the memory will be
   // freed for you later.
   void* savedState;
   size_t savedStateSize;

   ALooper* looper;
   AInputQueue* inputQueue;

   ANativeWindow* window;
   ARect contentRect;
   int activityState;
   int destroyRequested;

   pthread_mutex_t mutex;
   pthread_cond_t cond;

   int msgread;
   int msgwrite;

   pthread_t thread;

   int running;
   int stateSaved;
   int destroyed;
   int redrawNeeded;
   AInputQueue* pendingInputQueue;
   ANativeWindow* pendingWindow;
   ARect pendingContentRect;
};

enum {
   LOOPER_ID_MAIN = 1,
   LOOPER_ID_INPUT = 2,
   LOOPER_ID_USER = 3,
};

enum {
   APP_CMD_INPUT_CHANGED,
   /**
    * Command from main thread: a new ANativeWindow is ready for use.  Upon
    * receiving this command, android_app->window will contain the new window
    * surface.
    */
   APP_CMD_INIT_WINDOW,

   /**
    * Command from main thread: the existing ANativeWindow needs to be
    * terminated.  Upon receiving this command, android_app->window still
    * contains the existing window; after calling android_app_exec_cmd
    * it will be set to NULL.
    */
   APP_CMD_TERM_WINDOW,

   /**
    * Command from main thread: the current ANativeWindow has been resized.
    * Please redraw with its new size.
    */
   APP_CMD_WINDOW_RESIZED,

   /**
    * Command from main thread: the system needs that the current ANativeWindow
    * be redrawn.  You should redraw the window before handing this to
    * android_app_exec_cmd() in order to avoid transient drawing glitches.
    */
   APP_CMD_WINDOW_REDRAW_NEEDED,

   /**
    * Command from main thread: the content area of the window has changed,
    * such as from the soft input window being shown or hidden.  You can
    * find the new content rect in android_app::contentRect.
    */
   APP_CMD_CONTENT_RECT_CHANGED,

   /**
    * Command from main thread: the app's activity window has gained
    * input focus.
    */
   APP_CMD_GAINED_FOCUS,

   /**
    * Command from main thread: the app's activity window has lost
    * input focus.
    */
   APP_CMD_LOST_FOCUS,

   /**
    * Command from main thread: the current device configuration has changed.
    */
   APP_CMD_CONFIG_CHANGED,

   /**
    * Command from main thread: the system is running low on memory.
    * Try to reduce your memory use.
    */
   APP_CMD_LOW_MEMORY,

   /**
    * Command from main thread: the app's activity has been started.
    */
   APP_CMD_START,

   /**
    * Command from main thread: the app's activity has been resumed.
    */
   APP_CMD_RESUME,

   /**
    * Command from main thread: the app should generate a new saved state
    * for itself, to restore from later if needed.  If you have saved state,
    * allocate it with malloc and place it in android_app.savedState with
    * the size in android_app.savedStateSize.  The will be freed for you
    * later.
    */
   APP_CMD_SAVE_STATE,

   /**
    * Command from main thread: the app's activity has been paused.
    */
   APP_CMD_PAUSE,

   /**
    * Command from main thread: the app's activity has been stopped.
    */
   APP_CMD_STOP,

   /**
    * Command from main thread: the app's activity is being destroyed,
    * and waiting for the app thread to clean up and exit before proceeding.
    */
   APP_CMD_DESTROY,
};

int8_t android_app_read_cmd(struct android_app* android_app);

extern void engine_app_read_cmd(struct android_app *app);
extern void engine_handle_cmd(struct android_app* android_app, int32_t cmd);
extern void free_saved_state(struct android_app* android_app);

#endif /* _ANDROID_NATIVE_APP_GLUE_H */
