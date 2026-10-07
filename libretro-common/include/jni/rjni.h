/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjni.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RJNI_H__
#define __LIBRETRO_SDK_RJNI_H__

#include <jni.h>

#include <retro_common_api.h>
#include <retro_atomic.h>

/* Process-wide JNI plumbing shared by every JNI user in the tree: one
 * registered JavaVM, one per-thread JNIEnv cached in destructor-capable
 * TLS so a thread this layer attached is detached again when it exits
 * (an attached thread exiting undetached aborts the process on Android),
 * and once-caches for the class / method / field lookups, which cross
 * into the VM and are the expensive part of a JNI call site. Everything
 * is lock-free: caches are CAS-published, losers discard their own. */

RETRO_BEGIN_DECLS

/* A once-cache slot for a jclass global reference, a jmethodID or a
 * jfieldID. Zero-initialise (static storage) and hand the same slot to
 * every lookup of the same thing. */
typedef struct rjni_ref
{
   retro_atomic_ptr_t p;
} rjni_ref;

/* Register the process JavaVM. First registration wins; later calls
 * with a different VM are ignored. Safe from any thread. */
void rjni_set_vm(JavaVM *vm);

JavaVM *rjni_vm(void);

/* The calling thread's JNIEnv, attaching it to the VM on first use.
 * Returns NULL if no VM is registered or the attach fails. A thread
 * already attached by the VM (a Java-created thread) is recognised and
 * left attached at exit; only threads this function attached are
 * detached by the TLS destructor. */
JNIEnv *rjni_env(void);

/* Nonzero, after describing and clearing it, if an exception was
 * pending. Uses ExceptionCheck, so no local reference is created. */
int rjni_exception_clear(JNIEnv *env);

/* FindClass + NewGlobalRef once per slot; the global reference is held
 * for process lifetime. Returns NULL (exception cleared) on failure,
 * which is not cached, so a later call retries. */
jclass rjni_class(rjni_ref *cache, JNIEnv *env, const char *name);

/* As rjni_class, but resolved through loader.loadClass(name) so
 * application classes resolve from natively-attached threads, where
 * FindClass only sees the system class loader. @name is dot-separated
 * here, as loadClass expects. */
jclass rjni_class_from_loader(rjni_ref *cache, JNIEnv *env,
      jobject loader, const char *name);

jmethodID rjni_method(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig);

jmethodID rjni_method_static(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig);

jfieldID rjni_field(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig);

jfieldID rjni_field_static(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig);

RETRO_END_DECLS

#ifndef __cplusplus
/* Thin call wrappers for C translation units, each a JNI call followed
 * by an exception describe-and-clear. (*env)-> syntax is C-only; C++
 * callers use env->X() directly. */

/* Every macro below is wrapped in do/while(0). Without it the trailing
 * JNI_EXCEPTION escapes any unbraced guard at the call site, so
 *
 *    if (env != NULL)
 *       CALL_BOOLEAN_METHOD(env, ...);
 *
 * expanded to a guarded call followed by an *unguarded* exception check
 * that dereferences env regardless - a null dereference on exactly the
 * path the guard existed to protect. */
#define JNI_EXCEPTION(env) \
   do { \
      if ((*env)->ExceptionCheck(env)) \
      { \
         (*env)->ExceptionDescribe(env); \
         (*env)->ExceptionClear(env); \
      } \
   } while (0)

#define FIND_CLASS(env, var, classname) \
   do { \
      var = (*env)->FindClass(env, classname); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_OBJECT_CLASS(env, var, clazz_obj) \
   do { \
      var = (*env)->GetObjectClass(env, clazz_obj); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_FIELD_ID(env, var, clazz, fieldName, fieldDescriptor) \
   do { \
      var = (*env)->GetFieldID(env, clazz, fieldName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   do { \
      var = (*env)->GetMethodID(env, clazz, methodName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define GET_STATIC_METHOD_ID(env, var, clazz, methodName, fieldDescriptor) \
   do { \
      var = (*env)->GetStaticMethodID(env, clazz, methodName, fieldDescriptor); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallObjectMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_STATIC_METHOD(env, var, clazz, methodId) \
   do { \
      var = (*env)->CallStaticObjectMethod(env, clazz, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_STATIC_METHOD_PARAM(env, var, clazz, methodId, ...) \
   do { \
      var = (*env)->CallStaticObjectMethod(env, clazz, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_OBJ_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallObjectMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_VOID_METHOD(env, clazz_obj, methodId) \
   do { \
      (*env)->CallVoidMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_VOID_METHOD_PARAM(env, clazz_obj, methodId, ...) \
   do { \
      (*env)->CallVoidMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_BOOLEAN_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallBooleanMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_BOOLEAN_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallBooleanMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_DOUBLE_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallDoubleMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_INT_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallIntMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_INT_METHOD_PARAM(env, var, clazz_obj, methodId, ...) \
   do { \
      var = (*env)->CallIntMethod(env, clazz_obj, methodId, __VA_ARGS__); \
      JNI_EXCEPTION(env); \
   } while (0)

#define CALL_FLOAT_METHOD(env, var, clazz_obj, methodId) \
   do { \
      var = (*env)->CallFloatMethod(env, clazz_obj, methodId); \
      JNI_EXCEPTION(env); \
   } while (0)

#endif /* !__cplusplus */

#endif
