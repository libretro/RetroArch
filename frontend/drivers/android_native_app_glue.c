/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include "platform_android.h"

static void free_saved_state(struct android_app* android_app)
{
    pthread_mutex_lock(&android_app->mutex);
    if (android_app->savedState != NULL)
    {
        free(android_app->savedState);
        android_app->savedState = NULL;
        android_app->savedStateSize = 0;
    }
    pthread_mutex_unlock(&android_app->mutex);
}


int8_t android_app_read_cmd(struct android_app *android_app)
{
   int8_t cmd;
   if (read(android_app->msgread, &cmd, sizeof(cmd)) == sizeof(cmd))
   {
      switch (cmd)
      {
         case APP_CMD_SAVE_STATE:
            free_saved_state(android_app);
            break;
      }
      return cmd;
   }
   else
   {
      RARCH_ERR("No data on command pipe.\n");
   }
   return 1;
}

static void print_cur_config(struct android_app* android_app)
{
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

void android_app_pre_exec_cmd(struct android_app* android_app, int8_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
         RARCH_LOG("APP_CMD_INPUT_CHANGED\n");
         pthread_mutex_lock(&android_app->mutex);
         if (android_app->inputQueue != NULL)
            AInputQueue_detachLooper(android_app->inputQueue);
         android_app->inputQueue = android_app->pendingInputQueue;
         if (android_app->inputQueue != NULL)
         {
            RARCH_LOG("Attaching input queue to looper");
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  &android_app->inputPollSource);
         }
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_INIT_WINDOW:
         RARCH_LOG("APP_CMD_INIT_WINDOW\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = android_app->pendingWindow;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_TERM_WINDOW:
         RARCH_LOG("APP_CMD_TERM_WINDOW\n");
         pthread_cond_broadcast(&android_app->cond);
         break;

      case APP_CMD_RESUME:
      case APP_CMD_START:
      case APP_CMD_PAUSE:
      case APP_CMD_STOP:
         RARCH_LOG("activityState=%d\n", cmd);
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_CONFIG_CHANGED:
         RARCH_LOG("APP_CMD_CONFIG_CHANGED\n");
         AConfiguration_fromAssetManager(android_app->config,
               android_app->activity->assetManager);
         print_cur_config(android_app);
         break;

      case APP_CMD_DESTROY:
         RARCH_LOG("APP_CMD_DESTROY\n");
         android_app->destroyRequested = 1;
         break;
   }
}

void android_app_post_exec_cmd(struct android_app* android_app, int8_t cmd)
{
   switch (cmd)
   {
      case APP_CMD_TERM_WINDOW:
         RARCH_LOG("APP_CMD_TERM_WINDOW\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = NULL;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_SAVE_STATE:
         RARCH_LOG("APP_CMD_SAVE_STATE\n");
         pthread_mutex_lock(&android_app->mutex);
         android_app->stateSaved = 1;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_RESUME:
         free_saved_state(android_app);
         break;
   }
}

void app_dummy(void)
{

}

static void android_app_destroy(struct android_app* android_app)
{
    RARCH_LOG("android_app_destroy!");
    free_saved_state(android_app);
    pthread_mutex_lock(&android_app->mutex);
    if (android_app->inputQueue != NULL)
        AInputQueue_detachLooper(android_app->inputQueue);
    AConfiguration_delete(android_app->config);
    android_app->destroyed = 1;
    pthread_cond_broadcast(&android_app->cond);
    pthread_mutex_unlock(&android_app->mutex);
    /* Can't touch android_app object after this. */
}

static void process_input(struct android_app* app, struct android_poll_source* source)
{
    AInputEvent* event = NULL;
    int processed = 0;

    while (AInputQueue_getEvent(app->inputQueue, &event) >= 0)
    {
       int32_t handled = 0;
        RARCH_LOG("New input event: type=%d\n", AInputEvent_getType(event));
        if (AInputQueue_preDispatchEvent(app->inputQueue, event))
            continue;
        if (app->onInputEvent != NULL)
           handled = app->onInputEvent(app, event);
        AInputQueue_finishEvent(app->inputQueue, event, handled);
        processed = 1;
    }
    if (processed == 0)
        RARCH_ERR("Failure reading next input event: %s\n", strerror(errno));
}

static void process_cmd(struct android_app* app, struct android_poll_source* source)
{
   int8_t cmd = android_app_read_cmd(app);
   android_app_pre_exec_cmd(app, cmd);
   if (app->onAppCmd != NULL)
      app->onAppCmd(app, cmd);
   android_app_post_exec_cmd(app, cmd);
}

static void *android_app_entry(void *param)
{
   ALooper *looper;
   struct android_app* android_app = (struct android_app*)param;

   android_app->config = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

   print_cur_config(android_app);

   android_app->cmdPollSource.id        = LOOPER_ID_MAIN;
   android_app->cmdPollSource.app       = android_app;
   android_app->cmdPollSource.process   = process_cmd;
   android_app->inputPollSource.id      = LOOPER_ID_INPUT;
   android_app->inputPollSource.app     = android_app;
   android_app->inputPollSource.process = process_input;

   looper = (ALooper*)ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN,
         ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   pthread_mutex_lock(&android_app->mutex);
   android_app->running = 1;
   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);

   android_main(android_app);

   android_app_destroy(android_app);
   return NULL;
}

/* --------------------------------------------------------------------
 * Native activity interaction (called from main thread)
 * --------------------------------------------------------------------
 */

static struct android_app* android_app_create(ANativeActivity* activity,
        void* savedState, size_t savedStateSize)
{
   int msgpipe[2];
   struct android_app* android_app = (struct android_app*)
      calloc(1, sizeof(*android_app));
   android_app->activity = activity;

   pthread_mutex_init(&android_app->mutex, NULL);
   pthread_cond_init(&android_app->cond, NULL);

   if (savedState != NULL)
   {
      android_app->savedState = malloc(savedStateSize);
      android_app->savedStateSize = savedStateSize;
      memcpy(android_app->savedState, savedState, savedStateSize);
   }

   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      activity->instance = NULL;
   }
   android_app->msgread  = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   pthread_attr_t attr; 
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_create(&android_app->thread, &attr, android_app_entry, android_app);

   /* Wait for thread to start. */
   pthread_mutex_lock(&android_app->mutex);
   while (!android_app->running)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);
   pthread_mutex_unlock(&android_app->mutex);

   return android_app;
}

void android_app_write_cmd(struct android_app *android_app, int8_t cmd)
{
   if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
      RARCH_ERR("Failure writing android_app cmd: %s\n", strerror(errno));
}

static void android_app_set_input(void *data, AInputQueue* inputQueue)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app) return;

   pthread_mutex_lock(&android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);

   while (android_app->inputQueue != android_app->pendingInputQueue)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_window(void *data, ANativeWindow* window)
{
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   pthread_mutex_lock(&android_app->mutex);
   if (android_app->pendingWindow)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   pthread_mutex_unlock(&android_app->mutex);
}

static void android_app_set_activity_state(struct android_app *android_app, int8_t cmd)
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
   RARCH_LOG("Destroy: %p\n", activity);
   android_app_free((struct android_app*)activity->instance);
}

static void onStart(ANativeActivity* activity)
{
   RARCH_LOG("Start: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   RARCH_LOG("Resume: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_RESUME);
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen)
{
   struct android_app* android_app = (struct android_app*)activity->instance;
   void* savedState = NULL;

   pthread_mutex_lock(&android_app->mutex);
   android_app->stateSaved = 0;
   android_app_write_cmd(android_app, APP_CMD_SAVE_STATE);
   while (!android_app->stateSaved)
      pthread_cond_wait(&android_app->cond, &android_app->mutex);

   if (android_app->savedState != NULL)
   {
      savedState = android_app->savedState;
      *outLen = android_app->savedStateSize;
      android_app->savedState = NULL;
      android_app->savedStateSize = 0;
   }

   pthread_mutex_unlock(&android_app->mutex);

   return savedState;
}

static void onPause(ANativeActivity* activity)
{
   RARCH_LOG("Pause: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   RARCH_LOG("Stop: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_STOP);
}

static void onConfigurationChanged(ANativeActivity *activity)
{
   struct android_app* android_app = (struct android_app*)
      activity->instance;

   if (!android_app)
      return;

   RARCH_LOG("ConfigurationChanged: %p\n", activity);
   android_app_write_cmd(android_app, APP_CMD_CONFIG_CHANGED);
}

static void onLowMemory(ANativeActivity* activity)
{
    struct android_app* android_app = (struct android_app*)activity->instance;
    android_app_write_cmd(android_app, APP_CMD_LOW_MEMORY);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   RARCH_LOG("WindowFocusChanged: %p -- %d\n", activity, focused);
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowCreated: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowDestroyed: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   RARCH_LOG("InputQueueCreated: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity,
      AInputQueue* queue)
{
   RARCH_LOG("InputQueueDestroyed: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

void ANativeActivity_onCreate(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   RARCH_LOG("Creating Native Activity: %p\n", activity);
   activity->callbacks->onDestroy               = onDestroy;
   activity->callbacks->onStart                 = onStart;
   activity->callbacks->onResume                = onResume;
   activity->callbacks->onSaveInstanceState     = onSaveInstanceState;
   activity->callbacks->onPause                 = onPause;
   activity->callbacks->onStop                  = onStop;
   activity->callbacks->onConfigurationChanged  = onConfigurationChanged;
   activity->callbacks->onLowMemory             = onLowMemory;
   activity->callbacks->onWindowFocusChanged    = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated   = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated     = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed   = onInputQueueDestroyed;

   activity->instance = android_app_create(activity, savedState, savedStateSize);
}
