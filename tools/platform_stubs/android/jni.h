/* Hermetic compile-only stand-in for the NDK's <jni.h>: the types and
 * the JNINativeInterface slots RetroArch calls, nothing more. */
#ifndef STUB_JNI_H
#define STUB_JNI_H
#include <stdint.h>
#include <stddef.h>
typedef int32_t jint; typedef int64_t jlong; typedef uint8_t jboolean; typedef int8_t jbyte;
typedef uint16_t jchar; typedef int16_t jshort; typedef float jfloat; typedef double jdouble;
typedef jint jsize;
typedef void *jobject; typedef jobject jclass; typedef jobject jstring; typedef jobject jthrowable;
typedef jobject jweak; typedef jobject jarray; typedef jarray jobjectArray; typedef jarray jintArray;
typedef jarray jbyteArray; typedef jarray jfloatArray;
typedef struct _jmethodID *jmethodID; typedef struct _jfieldID *jfieldID;
struct JNINativeInterface;
typedef const struct JNINativeInterface *JNIEnv;
struct JNIInvokeInterface;
typedef const struct JNIInvokeInterface *JavaVM;
struct JNINativeInterface {
   jclass    (*FindClass)(JNIEnv*, const char*);
   jclass    (*GetObjectClass)(JNIEnv*, jobject);
   jmethodID (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);
   jmethodID (*GetStaticMethodID)(JNIEnv*, jclass, const char*, const char*);
   jfieldID  (*GetFieldID)(JNIEnv*, jclass, const char*, const char*);
   jobject   (*CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);
   jboolean  (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
   jint      (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
   jfloat    (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
   jdouble   (*CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);
   void      (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
   jobject   (*CallStaticObjectMethod)(JNIEnv*, jclass, jmethodID, ...);
   jboolean  (*CallStaticBooleanMethod)(JNIEnv*, jclass, jmethodID, ...);
   jobject   (*GetObjectField)(JNIEnv*, jobject, jfieldID);
   jint      (*GetIntField)(JNIEnv*, jobject, jfieldID);
   jstring   (*NewStringUTF)(JNIEnv*, const char*);
   const char *(*GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
   void      (*ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
   jsize     (*GetArrayLength)(JNIEnv*, jarray);
   jobject   (*GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
   jint     *(*GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
   void      (*ReleaseIntArrayElements)(JNIEnv*, jintArray, jint*, jint);
   jobject   (*NewGlobalRef)(JNIEnv*, jobject);
   void      (*DeleteGlobalRef)(JNIEnv*, jobject);
   void      (*DeleteLocalRef)(JNIEnv*, jobject);
   jboolean  (*ExceptionCheck)(JNIEnv*);
   jthrowable (*ExceptionOccurred)(JNIEnv*);
   void      (*ExceptionDescribe)(JNIEnv*);
   void      (*ExceptionClear)(JNIEnv*);
};
struct JNIInvokeInterface {
   jint (*DestroyJavaVM)(JavaVM*);
   jint (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*);
   jint (*DetachCurrentThread)(JavaVM*);
   jint (*GetEnv)(JavaVM*, void**, jint);
};
#define JNI_VERSION_1_6 0x00010006
#define JNI_OK 0
#define JNI_COMMIT 1
#define JNI_ABORT 2
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNIEXPORT
#define JNICALL
#endif
