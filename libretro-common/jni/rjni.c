/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjni.c).
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

#include <stdlib.h>

#include <jni/rjni.h>
#include <rthreads/rthreads.h>

/* The registered VM, and the TLS key holding each thread's cached env.
 * The key lives in a small heap record published with a CAS so that
 * first use from any thread needs no lock and no wait: concurrent
 * creators race, exactly one record is published, losers delete their
 * own. The same pattern publishes every rjni_ref cache. */

static retro_atomic_ptr_t rjni_vm_slot;

#ifdef HAVE_THREAD_STORAGE
static retro_atomic_ptr_t rjni_key_slot;

struct rjni_key_rec
{
   sthread_tls_t key;
};
#endif

/* Per-thread record: the env, and whether this layer attached the
 * thread (only then may the destructor detach it). */
struct rjni_tls_rec
{
   JNIEnv *env;
   int owns_attach;
};

void rjni_set_vm(JavaVM *vm)
{
   retro_atomic_cas_ptr(&rjni_vm_slot, NULL, (void*)vm);
}

JavaVM *rjni_vm(void)
{
   return (JavaVM*)retro_atomic_load_acquire_ptr(&rjni_vm_slot);
}

#ifdef HAVE_THREAD_STORAGE
static void rjni_tls_destruct(void *value)
{
   struct rjni_tls_rec *rec = (struct rjni_tls_rec*)value;
   JavaVM *vm;

   if (!rec)
      return;
   if (rec->owns_attach && (vm = rjni_vm()))
      (*vm)->DetachCurrentThread(vm);
   free(rec);
}

static struct rjni_key_rec *rjni_key(void)
{
   struct rjni_key_rec *rec =
      (struct rjni_key_rec*)retro_atomic_load_acquire_ptr(&rjni_key_slot);

   if (rec)
      return rec;
   if (!(rec = (struct rjni_key_rec*)malloc(sizeof(*rec))))
      return NULL;
   if (!sthread_tls_create_with_dtor(&rec->key, rjni_tls_destruct))
   {
      free(rec);
      return NULL;
   }
   if (retro_atomic_cas_ptr(&rjni_key_slot, NULL, rec))
      return rec;
   sthread_tls_delete(&rec->key);
   free(rec);
   return (struct rjni_key_rec*)retro_atomic_load_acquire_ptr(&rjni_key_slot);
}
#endif

JNIEnv *rjni_env(void)
{
#ifdef HAVE_THREAD_STORAGE
   struct rjni_key_rec *key = rjni_key();
   struct rjni_tls_rec *rec;
#endif
   JNIEnv *env = NULL;
   JavaVM *vm;

#ifdef HAVE_THREAD_STORAGE
   if (key && (rec = (struct rjni_tls_rec*)sthread_tls_get(&key->key)))
      return rec->env;
#endif

   if (!(vm = rjni_vm()))
      return NULL;

   /* GetEnv distinguishes a thread the VM already knows (a Java-created
    * thread calling into native) from one we must attach: detaching the
    * former at exit is not ours to do, the latter we own. */
   switch ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6))
   {
      case JNI_OK:
#ifdef HAVE_THREAD_STORAGE
         if (key && (rec = (struct rjni_tls_rec*)calloc(1, sizeof(*rec))))
         {
            rec->env = env;
            sthread_tls_set(&key->key, rec);
         }
#endif
         return env;
      case JNI_EDETACHED:
         /* Android's AttachCurrentThread takes JNIEnv**, the JDK's takes
          * void**; both fill in the same pointer. */
#ifdef __ANDROID__
         if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
            return NULL;
#else
         if ((*vm)->AttachCurrentThread(vm, (void**)&env, NULL) != JNI_OK)
            return NULL;
#endif
         /* Without a key there is nowhere to remember the attach and no
          * destructor to detach - the env is still usable, so return it
          * rather than fail the call. */
#ifdef HAVE_THREAD_STORAGE
         if (key && (rec = (struct rjni_tls_rec*)calloc(1, sizeof(*rec))))
         {
            rec->env         = env;
            rec->owns_attach = 1;
            sthread_tls_set(&key->key, rec);
         }
#endif
         return env;
      default:
         break;
   }
   return NULL;
}

int rjni_exception_clear(JNIEnv *env)
{
   if (!(*env)->ExceptionCheck(env))
      return 0;
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return 1;
}

/* Publish @global into @cache; on losing the race, release ours and
 * return the winner. */
static jclass rjni_class_publish(rjni_ref *cache, JNIEnv *env, jclass global)
{
   if (retro_atomic_cas_ptr(&cache->p, NULL, (void*)global))
      return global;
   (*env)->DeleteGlobalRef(env, global);
   return (jclass)retro_atomic_load_acquire_ptr(&cache->p);
}

jclass rjni_class(rjni_ref *cache, JNIEnv *env, const char *name)
{
   jclass local;
   jclass global;
   jclass cached = (jclass)retro_atomic_load_acquire_ptr(&cache->p);

   if (cached)
      return cached;
   local = (*env)->FindClass(env, name);
   if (rjni_exception_clear(env) || !local)
      return NULL;
   global = (jclass)(*env)->NewGlobalRef(env, local);
   (*env)->DeleteLocalRef(env, local);
   if (!global)
      return NULL;
   return rjni_class_publish(cache, env, global);
}

jclass rjni_class_from_loader(rjni_ref *cache, JNIEnv *env,
      jobject loader, const char *name)
{
   static rjni_ref loader_class;
   static rjni_ref load_method;
   jclass cls;
   jmethodID load;
   jstring jname;
   jclass local;
   jclass global;
   jclass cached = (jclass)retro_atomic_load_acquire_ptr(&cache->p);

   if (cached)
      return cached;
   if (!(cls = rjni_class(&loader_class, env, "java/lang/ClassLoader")))
      return NULL;
   if (!(load = rjni_method(&load_method, env, cls, "loadClass",
         "(Ljava/lang/String;)Ljava/lang/Class;")))
      return NULL;
   if (!(jname = (*env)->NewStringUTF(env, name)))
      return NULL;
   local = (jclass)(*env)->CallObjectMethod(env, loader, load, jname);
   (*env)->DeleteLocalRef(env, jname);
   if (rjni_exception_clear(env) || !local)
      return NULL;
   global = (jclass)(*env)->NewGlobalRef(env, local);
   (*env)->DeleteLocalRef(env, local);
   if (!global)
      return NULL;
   return rjni_class_publish(cache, env, global);
}

/* Method and field IDs are plain values, valid as long as their class
 * is; losing the publish race costs nothing. */
jmethodID rjni_method(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig)
{
   jmethodID id = (jmethodID)retro_atomic_load_acquire_ptr(&cache->p);

   if (id)
      return id;
   id = (*env)->GetMethodID(env, cls, name, sig);
   if (rjni_exception_clear(env) || !id)
      return NULL;
   retro_atomic_cas_ptr(&cache->p, NULL, (void*)id);
   return id;
}

jmethodID rjni_method_static(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig)
{
   jmethodID id = (jmethodID)retro_atomic_load_acquire_ptr(&cache->p);

   if (id)
      return id;
   id = (*env)->GetStaticMethodID(env, cls, name, sig);
   if (rjni_exception_clear(env) || !id)
      return NULL;
   retro_atomic_cas_ptr(&cache->p, NULL, (void*)id);
   return id;
}

jfieldID rjni_field(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig)
{
   jfieldID id = (jfieldID)retro_atomic_load_acquire_ptr(&cache->p);

   if (id)
      return id;
   id = (*env)->GetFieldID(env, cls, name, sig);
   if (rjni_exception_clear(env) || !id)
      return NULL;
   retro_atomic_cas_ptr(&cache->p, NULL, (void*)id);
   return id;
}

jfieldID rjni_field_static(rjni_ref *cache, JNIEnv *env, jclass cls,
      const char *name, const char *sig)
{
   jfieldID id = (jfieldID)retro_atomic_load_acquire_ptr(&cache->p);

   if (id)
      return id;
   id = (*env)->GetStaticFieldID(env, cls, name, sig);
   if (rjni_exception_clear(env) || !id)
      return NULL;
   retro_atomic_cas_ptr(&cache->p, NULL, (void*)id);
   return id;
}
