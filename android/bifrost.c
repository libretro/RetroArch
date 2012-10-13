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

/* RetroArch Bifrost: 
 * A burning rainbow bridge that reaches between Java (the world) 
 * and C/C++, the realm of the gods */

#include <stdio.h>
#include <jni.h>
#include "../boolean.h"

#include "com_retroarch_rruntime.h"

#include "../console/rarch_console.h"
#include "../../console/rarch_console_config.h"
#include "../console/rarch_console_main_wrap.h"
#include "../console/rarch_console_rom_ext.h"
#include "../console/rarch_console_settings.h"
#include "../console/rarch_console_input.h"
#include "../console/rarch_console_video.h"
#include "../general.h"
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#include "../file.h"

JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *pvt)
{
   RARCH_LOG("JNI_OnLoad.\n" );

   rarch_main_clear_state();

   config_set_defaults();
   input_null.init();

   RARCH_LOG("JNI_OnLoad #1.\n" );

   const input_driver_t *input = &input_null;

   bool find_libretro_file = false;

   snprintf(default_paths.config_file, sizeof(default_paths.config_file), "/mnt/extsd/retroarch.cfg");

   RARCH_LOG("JNI_OnLoad #1.1.\n" );

   rarch_init_msg_queue();

   RARCH_LOG("JNI_OnLoad #1.2.\n" );

   rarch_settings_set_default();

   RARCH_LOG("JNI_OnLoad #1.3.\n" );
   //rarch_input_set_controls_default(input);
   rarch_config_load(default_paths.config_file, find_libretro_file);

   RARCH_LOG("JNI_OnLoad #1.4.\n" );
   init_libretro_sym();

   RARCH_LOG("JNI_OnLoad #1.5.\n" );


   input_null.post_init();
   RARCH_LOG("JNI_OnLoad #1.6.\n" );

   //video_gl.start();

   RARCH_LOG("JNI_OnLoad #1.7.\n" );

   //driver.video = &video_gl;

   RARCH_LOG("JNI_OnLoad #1.8.\n" );

   RARCH_LOG("Reached end of JNI_OnLoad.\n" );

   return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnLoad( JavaVM *vm, void *pvt)
{
   RARCH_LOG("JNI_OnUnLoad.\n" );
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_load_1game
   (JNIEnv *env, jclass class, jstring j_path, jint j_extract_zip_mode)
{
   jboolean is_copy = false;
   const char * game_path = (*env)->GetStringUTFChars(env, j_path, &is_copy);
   RARCH_LOG("rruntime_load_game: %s.\n", game_path);

   rarch_console_load_game_wrap(game_path, 0, 0);

   (*env)->ReleaseStringUTFChars(env, j_path, game_path);
}

static int counter = 0;

JNIEXPORT jboolean JNICALL Java_com_retroarch_rruntime_run_1frame
   (JNIEnv *env, jclass class)
{
   counter++;
   RARCH_LOG("counter: %d.\n", counter);
   return rarch_main_iterate();
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_startup
   (JNIEnv *env, jclass class, jstring j_config_path)
{
   bool retval = false;
   jboolean is_copy = false;
   const char * config_path = (*env)->GetStringUTFChars(env, j_config_path, &is_copy);

   RARCH_LOG("rruntime_startup (config file: %s).\n", config_path);
   
   retval = rarch_startup(config_path);

   rarch_init_msg_queue();

   (*env)->ReleaseStringUTFChars(env, j_config_path, config_path);
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_deinit
   (JNIEnv *env, jclass class)
{
   rarch_main_deinit();
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_load_1state
   (JNIEnv *env, jclass class)
{
   rarch_load_state();
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_save_1state
   (JNIEnv *env, jclass class)
{
   rarch_save_state();
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_settings_1change
   (JNIEnv *env, jclass class, jint j_setting)
{
   unsigned setting = j_setting;
   rarch_settings_change(setting);
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_settings_1set_1defaults
   (JNIEnv *env, jclass class)
{
   RARCH_LOG("* rruntime_settings_set_defaults.\n" );
   rarch_settings_set_default();
}

void gfx_ctx_set_window(JNIEnv *jenv,jobject obj, jobject surface);
void gfx_ctx_free_window(JNIEnv *jenv,jobject obj, jobject surface);

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_set_window
   (JNIEnv *env, jclass class, jobject obj, jobject surface)
{
   gfx_ctx_set_window(env, obj, surface);
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_free_window
   (JNIEnv *env, jclass class, jobject obj, jobject surface)
{
   gfx_ctx_free_window(env, obj, surface);
}
