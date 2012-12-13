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

#include <string.h>
#include <stddef.h>
#include "jni_utils.h"

void jni_get_char_argv(struct jni_params *params, struct jni_out_params_char *out_params)
{
   JNIEnv *env;
   JavaVM *vm = params->java_vm;

   (*vm)->AttachCurrentThread(vm, &env, 0);

   jclass acl = (*env)->GetObjectClass(env, params->class_obj); //class pointer
   jmethodID giid = (*env)->GetMethodID(env, acl, params->method_name, params->method_signature);
   jobject obj = (*env)->CallObjectMethod(env, params->class_obj, giid); //Got our object

   jclass class_obj = (*env)->GetObjectClass(env, obj); //class pointer of object
   jmethodID gseid = (*env)->GetMethodID(env, class_obj, params->obj_method_name, params->obj_method_signature);

   jstring jsParam1 = (*env)->CallObjectMethod(env, obj, gseid, (*env)->NewStringUTF(env, out_params->in));
   const char *test_argv = (*env)->GetStringUTFChars(env, jsParam1, 0);

   strncpy(out_params->out, test_argv, out_params->out_sizeof);

   (*env)->ReleaseStringUTFChars(env, jsParam1, test_argv);

   (*vm)->DetachCurrentThread(vm);
}
