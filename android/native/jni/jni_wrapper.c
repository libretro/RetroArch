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

#define JNI_EXCEPTION(env) \
   if ((*env)->ExceptionOccurred(env)) \
   { \
      (*env)->ExceptionDescribe(env); \
      (*env)->ExceptionClear(env); \
   }

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, methodParam) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId, methodParam); \
   JNI_EXCEPTION(env)

void jni_get(void *params, void *out_params, unsigned out_type)
{
   JNIEnv *env;

   if (!params)
      return;

   struct jni_params *in_params = (struct jni_params*)params;

   if (!in_params)
      return;

   JavaVM *vm = in_params->java_vm;
   jclass class_ptr = NULL;
   jobject obj = NULL;
   jmethodID giid = NULL;

   jstring ret_char;
   struct jni_out_params_char *out_params_char = NULL;
   
   (void)ret_char;
   (void)out_params_char;

   (*vm)->AttachCurrentThread(vm, &env, 0);

   if (in_params->class_obj)
   {
      class_ptr = (*env)->GetObjectClass(env, in_params->class_obj); //class pointer
      GET_METHOD_ID(env, giid, class_ptr, in_params->method_name, in_params->method_signature);
      CALL_OBJ_METHOD(env, obj, in_params->class_obj, giid);
   }

   if (in_params->obj_method_name && obj)
   {
      class_ptr = (*env)->GetObjectClass(env, obj); //class pointer of object
      GET_METHOD_ID(env, giid, class_ptr, in_params->obj_method_name, in_params->obj_method_signature);

      switch(out_type)
      {
         case JNI_OUT_CHAR:
            {
               out_params_char =  (struct jni_out_params_char*)out_params;

               if (!out_params_char)
                  goto do_exit;
               CALL_OBJ_METHOD_PARAM(env, ret_char, obj, giid, (*env)->NewStringUTF(env, out_params_char->in));
            }
            break;
         case JNI_OUT_NONE:
         default:
            break;
      }
   }

   switch (out_type)
   {
      case JNI_OUT_CHAR:
         if(giid != NULL && ret_char)
         {
            const char *test_argv = (*env)->GetStringUTFChars(env, ret_char, 0);
            strncpy(out_params_char->out, test_argv, out_params_char->out_sizeof);
            (*env)->ReleaseStringUTFChars(env, ret_char, test_argv);
         }
         break;
      case JNI_OUT_NONE:
      default:
         break;
   }

do_exit:
   (*vm)->DetachCurrentThread(vm);
}
