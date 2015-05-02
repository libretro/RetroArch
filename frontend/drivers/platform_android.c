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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include "platform_android.h"

#include "../frontend.h"
#include "../../general.h"
#include <retro_inline.h>

static pthread_key_t thread_key;

struct android_app *g_android;
struct android_app_userdata *g_android_userdata;

JNIEnv *jni_thread_getenv(void)
{
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)g_android;
   int status = (*android_app->activity->vm)->
      AttachCurrentThread(android_app->activity->vm, &env, 0);

   if (status < 0)
   {
      RARCH_ERR("jni_thread_getenv: Failed to attach current thread.\n");
      return NULL;
   }
   pthread_setspecific(thread_key, (void*)env);

   return env;
}

static void jni_thread_destruct(void *value)
{
   JNIEnv *env = (JNIEnv*)value;
   struct android_app *android_app = (struct android_app*)g_android;
   RARCH_LOG("jni_thread_destruct()\n");

   if (!env)
      return;

   if (android_app)
      (*android_app->activity->vm)->
         DetachCurrentThread(android_app->activity->vm);
   pthread_setspecific(thread_key, NULL);
}

void android_main(struct android_app *state)
{
   char *argv[1];
   int argc = 0;
   int ret_iterate, ret_poll;

   g_android          = state;
   g_android_userdata = (struct android_app_userdata*)calloc(1, sizeof(*g_android_userdata));

   state->userData     = g_android_userdata;
   state->onAppCmd     = engine_handle_cmd;
   state->onInputEvent = engine_handle_input;

   if (rarch_main(argc, argv, state) != 0)
      goto end;

   do{
      ret_poll    = 0;
      ret_iterate = rarch_main_iterate();
   }while(ret_iterate != -1 && ret_poll != -1);

end:
   main_exit(state);

   if (g_android_userdata)
      free(g_android_userdata);
   g_android_userdata = NULL;

   exit(0);
}

int system_property_get(const char *name, char *value)
{
   FILE *pipe;
   int length = 0;
   char buffer[PATH_MAX_LENGTH];
   char cmd[PATH_MAX_LENGTH];
   char *curpos = NULL;

   snprintf(cmd, sizeof(cmd), "getprop %s", name);

   pipe = popen(cmd, "r");

   if (!pipe)
   {
      RARCH_ERR("Could not create pipe.\n");
      return 0;
   }

   curpos = value;
   
   while (!feof(pipe))
   {
      if (fgets(buffer, 128, pipe) != NULL)
      {
         int curlen = strlen(buffer);

         memcpy(curpos, buffer, curlen);

         curpos    += curlen;
         length    += curlen;
      }
   }

   *curpos = '\0';

   pclose(pipe);

   return length;
}

static void frontend_android_get_name(char *name, size_t sizeof_name)
{
   int len = system_property_get("ro.product.model", name);
   (void)len;
}

static void frontend_android_get_version(int32_t *major, int32_t *minor, int32_t *rel)
{
   char os_version_str[PROP_VALUE_MAX];
   system_property_get("ro.build.version.release", os_version_str);

   *major  = 0;
   *minor  = 0;
   *rel    = 0;

   /* Parse out the OS version numbers from the system properties. */
   if (os_version_str[0])
   {
      /* Try to parse out the version numbers from the string. */
      int num_read = sscanf(os_version_str, "%d.%d.%d", major, minor, rel);

      if (num_read > 0)
      {
         if (num_read < 2)
            *minor = 0;
         if (num_read < 3)
            *rel = 0;
         return;
      }
   }
}

static void frontend_android_get_os(char *name, size_t sizeof_name, int *major, int *minor)
{
   int rel;

   frontend_android_get_version(major, minor, &rel);

   strlcpy(name, "Android", sizeof_name);
}

static void frontend_android_get_version_sdk(int32_t *sdk)
{
  char os_version_str[PROP_VALUE_MAX];
  system_property_get("ro.build.version.sdk", os_version_str);

  *sdk = 0;
  if (os_version_str[0])
  {
    int num_read = sscanf(os_version_str, "%d", sdk);
    (void) num_read;
  }
}

static bool device_is_xperia_play(const char *name)
{
   if (
         !strcmp(name, "R800x") ||
         !strcmp(name, "R800at") ||
         !strcmp(name, "R800i") ||
         !strcmp(name, "R800a") ||
         !strcmp(name, "SO-01D")
         )
      return true;

   return false;
}

static bool device_is_game_console(const char *name)
{
   if (
         !strcmp(name, "OUYA Console") ||
         device_is_xperia_play(name) ||
         !strcmp(name, "GAMEMID_BT") ||
         !strcmp(name, "S7800") ||
         !strcmp(name, "SHIELD")
         )
      return true;

   return false;
}


static void frontend_android_get_environment_settings(int *argc,
      char *argv[], void *data, void *params_data)
{
   int32_t major, minor, rel;
   char device_model[PROP_VALUE_MAX], device_id[PROP_VALUE_MAX];

   JNIEnv *env;
   jobject obj = NULL;
   jstring jstr = NULL;
   struct android_app* android_app = (struct android_app*)data;
   struct android_app_userdata *userdata = 
      (struct android_app_userdata*)g_android_userdata;

   if (!android_app)
      return;

   env = jni_thread_getenv();
   if (!env)
      return;

   struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;
   if (args)
   {
      args->touched    = true;
      args->no_content = false;
      args->verbose    = false;
      args->sram_path  = NULL;
      args->state_path = NULL;
   }
   
   frontend_android_get_version(&major, &minor, &rel);

   RARCH_LOG("Android OS version (major : %d, minor : %d, rel : %d)\n", major, minor, rel);

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         userdata->getIntent);
   RARCH_LOG("Checking arguments passed from intent ...\n");

   /* Config file. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "CONFIGFILE"));

   if (userdata->getStringExtra && jstr)
   {
      static char config_path[PATH_MAX_LENGTH];
      const char *argv = NULL;
      *config_path = '\0';

      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(config_path, argv, sizeof(config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Config file: [%s].\n", config_path);
      if (args && *config_path)
         args->config_path = config_path;
   }

   /* Current IME. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "IME"));

   if (userdata->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      strlcpy(userdata->current_ime, argv,
            sizeof(userdata->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Current IME: [%s].\n", userdata->current_ime);
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "USED"));

   if (userdata->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      bool used = (strcmp(argv, "false") == 0) ? false : true;
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("USED: [%s].\n", used ? "true" : "false");
   }

   /* LIBRETRO. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "LIBRETRO"));

   if (userdata->getStringExtra && jstr)
   {
      static char core_path[PATH_MAX_LENGTH];
      const char *argv = NULL;

      *core_path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);
      if (argv && *argv)
         strlcpy(core_path, argv, sizeof(core_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Libretro path: [%s].\n", core_path);
      if (args && *core_path)
         args->libretro_path = core_path;
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "ROM"));

   if (userdata->getStringExtra && jstr)
   {
      static char path[PATH_MAX_LENGTH];
      const char *argv = NULL;

      *path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*path)
      {
         RARCH_LOG("Auto-start game %s.\n", path);
         if (args && *path)
            args->content_path = path;
      }
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, userdata->getStringExtra,
         (*env)->NewStringUTF(env, "DATADIR"));

   if (userdata->getStringExtra && jstr)
   {
      static char path[PATH_MAX_LENGTH];
      const char *argv = NULL;

      *path = '\0';
      argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*path)
      {
         RARCH_LOG("Data path: [%s].\n", path);
         if (args && *path)
         {
            fill_pathname_join(g_defaults.assets_dir, path,
                  "assets", sizeof(g_defaults.savestate_dir));
            fill_pathname_join(g_defaults.savestate_dir, path,
                  "savestates", sizeof(g_defaults.savestate_dir));
            fill_pathname_join(g_defaults.extraction_dir, path,
                  "tmp", sizeof(g_defaults.extraction_dir));
            fill_pathname_join(g_defaults.sram_dir, path,
                  "savefiles", sizeof(g_defaults.sram_dir));
            fill_pathname_join(g_defaults.system_dir, path,
                  "system", sizeof(g_defaults.system_dir));
            fill_pathname_join(g_defaults.shader_dir, path,
                  "shaders_glsl", sizeof(g_defaults.shader_dir));
            fill_pathname_join(g_defaults.overlay_dir, path,
                  "overlays", sizeof(g_defaults.overlay_dir));
            fill_pathname_join(g_defaults.core_dir, path,
                  "cores", sizeof(g_defaults.core_dir));
            fill_pathname_join(g_defaults.core_info_dir,
                  path, "info", sizeof(g_defaults.core_info_dir));
            fill_pathname_join(g_defaults.autoconfig_dir,
                  path, "autoconfig/android", sizeof(g_defaults.autoconfig_dir));
            fill_pathname_join(g_defaults.audio_filter_dir,
                  path, "audio_filters", sizeof(g_defaults.audio_filter_dir));
            fill_pathname_join(g_defaults.video_filter_dir,
                  path, "video_filters", sizeof(g_defaults.video_filter_dir));
            strlcpy(g_defaults.content_history_dir, 
                  path, sizeof(g_defaults.content_history_dir));
         }
      }
   }

   frontend_android_get_name(device_model, sizeof(device_model));
   system_property_get("ro.product.id", device_id);

   g_defaults.settings.video_threaded_enable = true;

   // Set automatic default values per device
   if (device_is_xperia_play(device_model))
   {
      g_defaults.settings.out_latency = 128;
      g_defaults.settings.video_refresh_rate = 59.19132938771038;
      g_defaults.settings.video_threaded_enable = false;
   }
   else if (!strcmp(device_model, "GAMEMID_BT"))
      g_defaults.settings.out_latency = 160;
   else if (!strcmp(device_model, "SHIELD"))
      g_defaults.settings.video_refresh_rate = 60.0;
   else if (!strcmp(device_model, "JSS15J"))
      g_defaults.settings.video_refresh_rate = 59.65;

   /* FIXME - needs to be refactored */
#if 0
   /* Explicitly disable input overlay by default 
    * for gamepad-like/console devices. */
   if (device_is_game_console(device_model))
      g_defaults.settings.input_overlay_enable = false;
#endif
}

int android_run_events(void *data);

static bool android_input_lookup_name_prekitkat(char *buf,
      int *vendorId, int *productId, size_t size, int id)
{
   RARCH_LOG("Using old lookup");

   jclass class;
   jmethodID method, getName;
   jobject device, name;
   const char *str = NULL;
   JNIEnv *env = (JNIEnv*)jni_thread_getenv();

   if (!env)
      goto error;

   class = NULL;
   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      goto error;

   method = NULL;
   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      goto error;

   device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto error;
   }

   getName = NULL;
   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto error;

   name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto error;
   }

   buf[0] = '\0';

   str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   RARCH_LOG("device name: %s\n", buf);

   return true;
error:
   return false;
}

static bool android_input_lookup_name(char *buf,
      int *vendorId, int *productId, size_t size, int id)
{
   RARCH_LOG("Using new lookup");

   jclass class;
   jmethodID method, getName, getVendorId, getProductId;
   jobject device, name;
   const char *str = NULL;
   JNIEnv *env = (JNIEnv*)jni_thread_getenv();

   if (!env)
      goto error;

   class = NULL;
   FIND_CLASS(env, class, "android/view/InputDevice");
   if (!class)
      goto error;

   method = NULL;
   GET_STATIC_METHOD_ID(env, method, class, "getDevice",
         "(I)Landroid/view/InputDevice;");
   if (!method)
      goto error;

   device = NULL;
   CALL_OBJ_STATIC_METHOD_PARAM(env, device, class, method, (jint)id);
   if (!device)
   {
      RARCH_ERR("Failed to find device for ID: %d\n", id);
      goto error;
   }

   getName = NULL;
   GET_METHOD_ID(env, getName, class, "getName", "()Ljava/lang/String;");
   if (!getName)
      goto error;

   name = NULL;
   CALL_OBJ_METHOD(env, name, device, getName);
   if (!name)
   {
      RARCH_ERR("Failed to find name for device ID: %d\n", id);
      goto error;
   }

   buf[0] = '\0';

   str = (*env)->GetStringUTFChars(env, name, 0);
   if (str)
      strlcpy(buf, str, size);
   (*env)->ReleaseStringUTFChars(env, name, str);

   RARCH_LOG("device name: %s\n", buf);

   getVendorId = NULL;
   GET_METHOD_ID(env, getVendorId, class, "getVendorId", "()I");
   if (!getVendorId)
      goto error;

   CALL_INT_METHOD(env, *vendorId, device, getVendorId);
   if (!*vendorId)
   {
      RARCH_ERR("Failed to find vendor id for device ID: %d\n", id);
      goto error;
   }
   RARCH_LOG("device vendor id: %d\n", *vendorId);

   getProductId = NULL;
   GET_METHOD_ID(env, getProductId, class, "getProductId", "()I");
   if (!getProductId)
      goto error;

   *productId = 0;
   CALL_INT_METHOD(env, *productId, device, getProductId);
   if (!*productId)
   {
      RARCH_ERR("Failed to find product id for device ID: %d\n", id);
      goto error;
   }
   RARCH_LOG("device product id: %d\n", *productId);

   return true;
error:
   return false;
}

static void frontend_android_init(void *data)
{
   int32_t sdk;
   JNIEnv *env;
   jclass class = NULL;
   jobject obj = NULL;
   struct android_app* android_app = (struct android_app*)data;
   struct android_app_userdata *userdata = 
      (struct android_app_userdata*)g_android_userdata;

   if (!android_app)
      return;

   if (pthread_key_create(&thread_key, jni_thread_destruct))
      RARCH_ERR("Error initializing pthread_key\n");

   /* These are set only for the native activity,
    * and are reset when it ends. */
   ANativeActivity_setWindowFlags(android_app->activity,
         AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);

   RARCH_LOG("Waiting for Android Native Window to be initialized ...\n");

   while (!android_app->window)
   {
      if (android_run_events(android_app) == -1)
         return;
   }

   RARCH_LOG("Android Native Window initialized.\n");

   env = jni_thread_getenv();
   if (!env)
      return;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   GET_METHOD_ID(env, userdata->getIntent, class,
         "getIntent", "()Landroid/content/Intent;");
   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         userdata->getIntent);

   GET_OBJECT_CLASS(env, class, obj);
   GET_METHOD_ID(env, userdata->getStringExtra, class,
         "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

   frontend_android_get_version_sdk(&sdk);

   if (sdk >= 19)
      engine_lookup_name = android_input_lookup_name;
   else
      engine_lookup_name = android_input_lookup_name_prekitkat;
}

static int frontend_android_get_rating(void)
{
   char device_model[PROP_VALUE_MAX];
   frontend_android_get_name(device_model, sizeof(device_model));

   RARCH_LOG("ro.product.model: (%s).\n", device_model);

   if (device_is_xperia_play(device_model))
      return 6;
   else if (!strcmp(device_model, "GT-I9505"))
      return 12;
   else if (!strcmp(device_model, "SHIELD"))
      return 13;
   return -1;
}

static enum frontend_architecture frontend_android_get_architecture(void)
{
   char abi[PROP_VALUE_MAX];
   system_property_get("ro.product.cpu.abi", abi);

   if (!strcmp(abi, "armeabi-v7a"))
      return FRONTEND_ARCH_ARM;
   if (!strcmp(abi, "armeabi"))
      return FRONTEND_ARCH_ARM;
   if (!strcmp(abi, "mips"))
      return FRONTEND_ARCH_MIPS;
   if (!strcmp(abi, "x86"))
      return FRONTEND_ARCH_X86;

   return FRONTEND_ARCH_NONE;
}

const frontend_ctx_driver_t frontend_ctx_android = {
   frontend_android_get_environment_settings,
   frontend_android_init,
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   NULL,                         /* shutdown */
   frontend_android_get_name,
   frontend_android_get_os,
   frontend_android_get_rating,
   NULL,                         /* load_content */
   frontend_android_get_architecture,
   NULL,                         /* get_powerstate */
   "android",
};
