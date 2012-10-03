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

#include "../console/rarch_console_main_wrap.h"
#include "../console/rarch_console_rom_ext.h"
#include "../console/rarch_console_settings.h"
#include "../general.h"

JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *pvt)
{
   fprintf(stdout, "* JNI_OnLoad called\n" );
   return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnLoad( JavaVM *vm, void *pvt)
{
   fprintf(stdout, "* JNI_OnUnLoad called\n" );
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_load_1game
   (JNIEnv *env, jclass class, jstring j_path, jint j_extract_zip_mode)
{
   jboolean is_copy = false;
   const char * game_path = (*env)->GetStringUTFChars(env, j_path, &is_copy);

   rarch_console_load_game_wrap(game_path, 0, 0);

   (*env)->ReleaseStringUTFChars(env, j_path, game_path);
}

JNIEXPORT jboolean JNICALL Java_com_retroarch_rruntime_run_1frame
   (JNIEnv *env, jclass class)
{
   return rarch_main_iterate();
}

JNIEXPORT void JNICALL Java_com_retroarch_rruntime_startup
   (JNIEnv *env, jclass class, jstring j_config_path)
{
   bool retval = false;
   jboolean is_copy = false;
   const char * config_path = (*env)->GetStringUTFChars(env, j_config_path, &is_copy);
   
   retval = rarch_startup(config_path);

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
   rarch_settings_set_default();
}
