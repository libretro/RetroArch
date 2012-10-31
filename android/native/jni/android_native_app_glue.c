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

#include <jni.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include "android_native_app_glue.h"

int8_t android_app_read_cmd(struct android_app* android_app)
{
   int8_t cmd;

   if (read(android_app->msgread, &cmd, sizeof(cmd)) == sizeof(cmd))
   {
      switch (cmd)
      { 
         case APP_CMD_SAVE_STATE:
            break;
      }
      return cmd;
   }

   return -1;
}

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

void android_app_pre_exec_cmd(struct android_app* android_app, int8_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
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
      case APP_CMD_INIT_WINDOW:
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = android_app->pendingWindow;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_TERM_WINDOW:
         pthread_cond_broadcast(&android_app->cond);
         break;
      case APP_CMD_RESUME:
      case APP_CMD_START:
      case APP_CMD_PAUSE:
      case APP_CMD_STOP:
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_CONFIG_CHANGED:
         AConfiguration_fromAssetManager(android_app->config,
               android_app->activity->assetManager);
         print_cur_config(android_app);
         break;
      case APP_CMD_DESTROY:
         android_app->destroyRequested = 1;
         break;
   }
}

void android_app_post_exec_cmd(struct android_app* android_app, int8_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_TERM_WINDOW:
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = NULL;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;
      case APP_CMD_SAVE_STATE:
         break;
      case APP_CMD_RESUME:
         break;
   }
}

void app_dummy()
{
}

static void android_app_destroy(struct android_app* android_app)
{
   pthread_mutex_lock(&android_app->mutex);

   if (android_app->inputQueue != NULL)
      AInputQueue_detachLooper(android_app->inputQueue);

   AConfiguration_delete(android_app->config);
   android_app->destroyed = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);
   // Can't touch android_app object after this.
}

void process_input(struct android_app* app, struct android_poll_source* source)
{
   AInputEvent* event = NULL;

   if (AInputQueue_getEvent(app->inputQueue, &event) >= 0)
   {
      if (AInputQueue_preDispatchEvent(app->inputQueue, event))
         return;

      int32_t handled = 0;

      if (app->onInputEvent != NULL)
         handled = app->onInputEvent(app, event);

      AInputQueue_finishEvent(app->inputQueue, event, handled);
   }
}

static void process_cmd(struct android_app* app, struct android_poll_source* source)
{
   int8_t cmd = android_app_read_cmd(app);

   android_app_pre_exec_cmd(app, cmd);

   if (app->onAppCmd != NULL)
      app->onAppCmd(app, cmd);

   android_app_post_exec_cmd(app, cmd);
}

static void* android_app_entry(void* param)
{
   struct android_app* android_app = (struct android_app*)param;

   android_app->config = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

   print_cur_config(android_app);

   android_app->cmdPollSource.id = LOOPER_ID_MAIN;
   android_app->cmdPollSource.app = android_app;
   android_app->cmdPollSource.process = process_cmd;
   android_app->inputPollSource.id = LOOPER_ID_INPUT;
   android_app->inputPollSource.app = android_app;
   android_app->inputPollSource.process = process_input;

   ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL,
         &android_app->cmdPollSource);
   android_app->looper = looper;

   pthread_mutex_lock(&android_app->mutex);
   android_app->running = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);

   android_main(android_app);

   android_app_destroy(android_app);
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

static void android_app_write_cmd(struct android_app* android_app, int8_t cmd)
{
   write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd);
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
