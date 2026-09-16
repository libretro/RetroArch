/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_android_native_activity_h
#define STUB_android_native_activity_h
#include <jni.h>
#include <stdint.h>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_window.h>
typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;
typedef struct ANativeActivity { struct ANativeActivityCallbacks *callbacks; JavaVM *vm; JNIEnv *env; jobject clazz; const char *internalDataPath; const char *externalDataPath; int32_t sdkVersion; void *instance; AAssetManager *assetManager; const char *obbPath; } ANativeActivity;
typedef struct ANativeActivityCallbacks { void (*onStart)(ANativeActivity*); void (*onResume)(ANativeActivity*); void *(*onSaveInstanceState)(ANativeActivity*, size_t*); void (*onPause)(ANativeActivity*); void (*onStop)(ANativeActivity*); void (*onDestroy)(ANativeActivity*); void (*onWindowFocusChanged)(ANativeActivity*, int); void (*onNativeWindowCreated)(ANativeActivity*, ANativeWindow*); void (*onNativeWindowResized)(ANativeActivity*, ANativeWindow*); void (*onNativeWindowRedrawNeeded)(ANativeActivity*, ANativeWindow*); void (*onNativeWindowDestroyed)(ANativeActivity*, ANativeWindow*); void (*onInputQueueCreated)(ANativeActivity*, AInputQueue*); void (*onInputQueueDestroyed)(ANativeActivity*, AInputQueue*); void (*onContentRectChanged)(ANativeActivity*, const void*); void (*onConfigurationChanged)(ANativeActivity*); void (*onLowMemory)(ANativeActivity*); } ANativeActivityCallbacks;
void ANativeActivity_finish(ANativeActivity*); void ANativeActivity_setWindowFlags(ANativeActivity*, uint32_t add, uint32_t remove); void ANativeActivity_showSoftInput(ANativeActivity*, uint32_t); void ANativeActivity_hideSoftInput(ANativeActivity*, uint32_t);
int32_t AInputQueue_getEvent(AInputQueue*, AInputEvent**); int32_t AInputQueue_preDispatchEvent(AInputQueue*, AInputEvent*); void AInputQueue_finishEvent(AInputQueue*, AInputEvent*, int handled); void AInputQueue_attachLooper(AInputQueue*, ALooper*, int ident, ALooper_callbackFunc, void*); void AInputQueue_detachLooper(AInputQueue*);
int32_t ANativeWindow_getWidth(ANativeWindow*); int32_t ANativeWindow_getHeight(ANativeWindow*);
#endif
