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

#include "../driver.h"
#include "../android/native/jni/jni_macros.h"

typedef struct android_location
{
   jmethodID onLocationInit;
   jmethodID onLocationFree;
   jmethodID onLocationStart;
   jmethodID onLocationStop;
   jmethodID onLocationSetInterval;
   jmethodID onLocationGetLongitude;
   jmethodID onLocationGetLatitude;
   jmethodID onLocationGetHorizontalAccuracy;
   jmethodID onLocationHasChanged;
} androidlocation_t;

static void *android_location_init(void)
{
   JNIEnv *env;
   jclass class;

   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)calloc(1, sizeof(androidlocation_t));
   if (!androidlocation)
      return NULL;

   env = jni_thread_getenv();
   if (!env)
      goto dealloc;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   if (class == NULL)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationInit, class, "onLocationInit", "()V");
   if (!androidlocation->onLocationInit)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationFree, class, "onLocationFree", "()V");
   if (!androidlocation->onLocationFree)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationStart, class, "onLocationStart", "()V");
   if (!androidlocation->onLocationStart)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationStop, class, "onLocationStop", "()V");
   if (!androidlocation->onLocationStop)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationGetLatitude, class, "onLocationGetLatitude", "()D");
   if (!androidlocation->onLocationGetLatitude)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationGetLongitude, class, "onLocationGetLongitude", "()D");
   if (!androidlocation->onLocationGetLongitude)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationGetHorizontalAccuracy, class, "onLocationGetHorizontalAccuracy", "()F");
   if (!androidlocation->onLocationGetHorizontalAccuracy)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationGetLongitude, class, "onLocationSetInterval", "(II)V");
   if (!androidlocation->onLocationSetInterval)
      goto dealloc;

   GET_METHOD_ID(env, androidlocation->onLocationHasChanged, class, "onLocationHasChanged", "()Z");
   if (!androidlocation->onLocationHasChanged)
      goto dealloc;

   CALL_VOID_METHOD(env, android_app->activity->clazz, androidlocation->onLocationInit);

   return androidlocation;
dealloc:
   free(androidlocation);
   return NULL;
}

static void android_location_free(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)data;
   JNIEnv *env = jni_thread_getenv();
   if (!env)
      return;

   CALL_VOID_METHOD(env, android_app->activity->clazz, androidlocation->onLocationFree);

   free(androidlocation);
}

static bool android_location_start(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)data;
   JNIEnv *env = jni_thread_getenv();
   if (!env)
      return false;

   CALL_VOID_METHOD(env, android_app->activity->clazz, androidlocation->onLocationStart);

   return true;
}

static void android_location_stop(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)data;
   JNIEnv *env = jni_thread_getenv();
   if (!env)
      return;

   CALL_VOID_METHOD(env, android_app->activity->clazz, androidlocation->onLocationStop);
}

static bool android_location_get_position(void *data, double *latitude, double *longitude, double *horiz_accuracy,
      double *vert_accuracy)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)data;
   JNIEnv *env = jni_thread_getenv();
   if (!env)
      goto fail;

   jdouble lat, lon, horiz_accu;
   jboolean newLocation;

   CALL_BOOLEAN_METHOD(env, newLocation, android_app->activity->clazz, androidlocation->onLocationHasChanged);

   if (!newLocation)
      goto fail;

   CALL_DOUBLE_METHOD(env, lat,        android_app->activity->clazz, androidlocation->onLocationGetLatitude);
   CALL_DOUBLE_METHOD(env, lon,        android_app->activity->clazz, androidlocation->onLocationGetLongitude);
   CALL_DOUBLE_METHOD(env, horiz_accu, android_app->activity->clazz, androidlocation->onLocationGetHorizontalAccuracy);

   if (lat != 0.0)
      *latitude = lat;
   if (lon != 0.0)
      *longitude = lon;
   if (horiz_accu != 0.0)
      *horiz_accuracy = horiz_accu;

   /* TODO/FIXME - custom implement vertical accuracy since Android location API does not have it? */
   *vert_accuracy = 0.0;

   return true;

fail:
   *latitude  = 0.0;
   *longitude = 0.0;
   *horiz_accuracy  = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

static void android_location_set_interval(void *data, unsigned interval_ms, unsigned interval_distance)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidlocation_t *androidlocation = (androidlocation_t*)data;
   JNIEnv *env = jni_thread_getenv();
   if (!env)
      return;

   CALL_VOID_METHOD_PARAM(env, android_app->activity->clazz, androidlocation->onLocationSetInterval, (int)interval_ms, (int)interval_distance);
}

const location_driver_t location_android = {
   android_location_init,
   android_location_free,
   android_location_start,
   android_location_stop,
   android_location_get_position,
   android_location_set_interval,
   "android",
};
