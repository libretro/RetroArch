#ifndef STUB_NATIVE_ACTIVITY_H
#define STUB_NATIVE_ACTIVITY_H
#include <jni.h>
#include <android/native_window.h>
typedef struct AInputQueue AInputQueue;
typedef struct AAssetManager AAssetManager;
typedef struct AConfiguration AConfiguration;
typedef struct ALooper ALooper;
typedef struct ANativeActivity {
   void *callbacks; void *vm; JNIEnv *env; jobject clazz;
   const char *internalDataPath, *externalDataPath; int32_t sdkVersion;
   void *instance; AAssetManager *assetManager;
} ANativeActivity;
#endif
