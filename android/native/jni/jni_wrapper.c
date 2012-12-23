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
#include <jni.h>
#include "jni_wrapper.h"
#include "../../../retroarch_logger.h"

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
   (void)reserved;

   /*
   JNIEnv *env;
   jclass loader;

   (*vm)->AttachCurrentThread(vm, &env, 0);

   loader = (*env)->FindClass(env, "org/retroarch/browser/ModuleActivity");

   if (loader == NULL)
   {
      RARCH_ERR("JNI - Can't find LoaderClass.\n");
      goto do_exit;
   }
   */

   RARCH_LOG("JNI - Successfully executed JNI_OnLoad.\n");

   return JNI_VERSION_1_4;

do_exit:
   (*vm)->DetachCurrentThread(vm);
   return -1;
}

void jni_get(void *params, void *out_params, unsigned out_type)
{
   JNIEnv *env;

   if (!params)
      return;

   struct jni_params *in_params = (struct jni_params*)params;

   if (!in_params)
      return;

   JavaVM *vm = in_params->java_vm;
   jobject obj = NULL;
   jmethodID gseid = NULL;

   (*vm)->AttachCurrentThread(vm, &env, 0);

   if (in_params->class_obj)
   {
      jclass acl = (*env)->GetObjectClass(env, in_params->class_obj); //class pointer
      jmethodID giid = (*env)->GetMethodID(env, acl, in_params->method_name, in_params->method_signature);
      obj = (*env)->CallObjectMethod(env, in_params->class_obj, giid); //Got our object
   }

   if (in_params->obj_method_name && obj)
   {
      jclass class_obj = (*env)->GetObjectClass(env, obj); //class pointer of object
      gseid = (*env)->GetMethodID(env, class_obj, in_params->obj_method_name,
            in_params->obj_method_signature);
   }

   switch (out_type)
   {
      case JNI_OUT_CHAR:
         if(gseid != NULL)
         {
            struct jni_out_params_char *out_params_char =  (struct jni_out_params_char*)out_params;

            if (!out_params_char)
               goto do_exit;

            jstring jsParam1 = (*env)->CallObjectMethod(env, obj, 
                  gseid, (*env)->NewStringUTF(env, out_params_char->in));
            const char *test_argv = (*env)->GetStringUTFChars(env, jsParam1, 0);
            strncpy(out_params_char->out, test_argv, out_params_char->out_sizeof);
            (*env)->ReleaseStringUTFChars(env, jsParam1, test_argv);
         }
         break;
      case JNI_OUT_NONE:
      default:
         break;
   }

do_exit:
   (*vm)->DetachCurrentThread(vm);
}
