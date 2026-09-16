/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_jni_h
#define STUB_jni_h
#include <stdint.h>
typedef struct _JNIEnv JNIEnv; typedef struct _JavaVM JavaVM; typedef void *jobject; typedef jobject jclass; typedef jobject jstring; typedef jobject jobjectArray; typedef int32_t jint; typedef int64_t jlong; typedef uint8_t jboolean; typedef int8_t jbyte; typedef uint16_t jchar; typedef int16_t jshort; typedef float jfloat; typedef double jdouble; typedef jobject jmethodID_s; typedef struct _jmethodID *jmethodID; typedef struct _jfieldID *jfieldID; typedef jobject jbyteArray; typedef jobject jintArray; typedef jobject jfloatArray; typedef jobject jthrowable; typedef jobject jweak;
struct _JNIEnv { const struct JNINativeInterface *functions; };
struct _JavaVM { const struct JNIInvokeInterface *functions; };
struct JNIInvokeInterface { jint (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*); jint (*DetachCurrentThread)(JavaVM*); jint (*GetEnv)(JavaVM*, void**, jint); };
#define JNI_VERSION_1_6 0x00010006
#define JNI_OK 0
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNIEXPORT
#define JNICALL
#endif
