#ifndef STUB_JNI_H
#define STUB_JNI_H
#include <stdint.h>
typedef int32_t jint; typedef float jfloat; typedef unsigned char jboolean;
typedef int32_t jsize; typedef void* jobject; typedef jobject jclass;
typedef jobject jstring; typedef jobject jarray; typedef jarray jintArray;
typedef struct _jmethodID *jmethodID; typedef struct _jfieldID *jfieldID;
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNI_ABORT 2
struct JNINativeInterface;
typedef const struct JNINativeInterface *JNIEnv;
struct JNINativeInterface {
   void *r0,*r1,*r2,*r3;
   jclass (*GetObjectClass)(JNIEnv*, jobject);
   jmethodID (*GetMethodID)(JNIEnv*, jclass, const char*, const char*);
   jobject (*CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);
   jint (*CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);
   jfloat (*CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);
   jboolean (*CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);
   void (*CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);
   jsize (*GetArrayLength)(JNIEnv*, jarray);
   jint* (*GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
   void (*ReleaseIntArrayElements)(JNIEnv*, jintArray, jint*, jint);
   void (*DeleteLocalRef)(JNIEnv*, jobject);
   jobject (*ExceptionOccurred)(JNIEnv*);
   jboolean (*ExceptionCheck)(JNIEnv*);
   void (*ExceptionClear)(JNIEnv*);
   void (*ExceptionDescribe)(JNIEnv*);
   const char* (*GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
   void (*ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
};
#endif
