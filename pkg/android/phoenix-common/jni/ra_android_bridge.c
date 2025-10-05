#include <jni.h>
#include <libretro.h>
#include "runloop.h"
#include "verbosity.h"
#include "configuration.h"

// ---------- Cached JNI handles ----------
static JavaVM *g_vm = NULL;
static jclass g_cls_RetroActivityFuture = NULL;       // GlobalRef
static jmethodID g_mid_nativePushContentFps = NULL;   // (F)V

// Small helper: get/attach JNIEnv for the current thread
static JNIEnv* ra_get_env(bool *out_attached)
{
    if (out_attached) *out_attached = false;
    if (!g_vm) return NULL;

    JNIEnv *env = NULL;
    jint r = (*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_6);
    if (r == JNI_OK && env) return env;

    // Not attached -> attach
    if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) != 0)
        return NULL;
    if (out_attached) *out_attached = true;
    return env;
}

// ---------- JNI load/unload ----------
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    g_vm = vm;
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
        return JNI_ERR;

    jclass local = (*env)->FindClass(env, "com/retroarch/browser/retroactivity/RetroActivityFuture");
    if (!local) return JNI_ERR;

    g_cls_RetroActivityFuture = (jclass)(*env)->NewGlobalRef(env, local);
    (*env)->DeleteLocalRef(env, local);
    if (!g_cls_RetroActivityFuture) return JNI_ERR;

    // Cache static push method: public static void nativePushContentFps(float)
    g_mid_nativePushContentFps = (*env)->GetStaticMethodID(env, g_cls_RetroActivityFuture,
                                                           "nativePushContentFps", "(F)V");
    // It's fine if Java side doesn't have it yet; we just won't call it
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) == JNI_OK && env) {
        if (g_cls_RetroActivityFuture) {
            (*env)->DeleteGlobalRef(env, g_cls_RetroActivityFuture);
            g_cls_RetroActivityFuture = NULL;
        }
    }
    g_vm = NULL;
    g_mid_nativePushContentFps = NULL;
}

// ---------- Public C API: push fresh FPS to Java (optional) ----------
void ra_notify_refresh_rate(float fps)
{
    if (fps <= 0.f) return;
    if (!g_vm || !g_cls_RetroActivityFuture || !g_mid_nativePushContentFps) return;

    bool attached = false;
    JNIEnv *env = ra_get_env(&attached);
    if (!env) return;

    (*env)->CallStaticVoidMethod(env, g_cls_RetroActivityFuture,
                                 g_mid_nativePushContentFps, (jfloat)fps);

    if (attached)
        (*g_vm)->DetachCurrentThread(g_vm);
}