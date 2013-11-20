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

#ifndef _JNI_MACROS_H
#define _JNI_MACROS_H

#include <jni.h>

struct jni_params
{
   jobject class_obj;
   char class_name[128];
   char method_name[128];
   char method_signature[128];
   char submethod_name[128];
   char submethod_signature[128];
};

struct jni_out_params_char
{
   char *out;
   size_t out_sizeof;
   char in[128];
};

#define JNI_EXCEPTION(env) \
   if ((*env)->ExceptionOccurred(env)) \
   { \
      (*env)->ExceptionDescribe(env); \
      (*env)->ExceptionClear(env); \
   }

#define FIND_CLASS(env, var, classname) \
   var = (*env)->FindClass(env, classname); \
   JNI_EXCEPTION(env)

#define GET_OBJECT_CLASS(env, var, clazz_obj) \
   var = (*env)->GetObjectClass(env, clazz_obj); \
   JNI_EXCEPTION(env)

#define GET_FIELD_ID(env, var, clazz, fieldName, fieldDescriptor) \
   var = (*env)->GetFieldID(env, clazz, fieldName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define GET_STATIC_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   var = (*env)->GetStaticMethodID(env, clazz, methodName, fieldDescriptor); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD(env, var, clazz, methodId) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_STATIC_METHOD_PARAM(env, var, clazz, methodId, ...) \
   var = (*env)->CallStaticObjectMethod(env, clazz, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   var = (*env)->CallObjectMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD(env, clazz_obj, methodId) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#define CALL_VOID_METHOD_PARAM(env, clazz_obj, methodId, ...) \
   (*env)->CallVoidMethod(env, clazz_obj, methodId, __VA_ARGS__); \
   JNI_EXCEPTION(env)

#define CALL_BOOLEAN_METHOD(env, var, clazz_obj, methodId) \
   var = (*env)->CallBooleanMethod(env, clazz_obj, methodId); \
   JNI_EXCEPTION(env)

#endif
