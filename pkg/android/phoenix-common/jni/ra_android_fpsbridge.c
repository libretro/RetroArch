// If compiled as C++ file, wrap with extern "C"
#include <jni.h>
#include <libretro.h>
#include "runloop.h"
#include "verbosity.h"
#include "configuration.h"

// Static version: Java calls static native method
JNIEXPORT jfloat JNICALL
Java_com_retroarch_browser_retroactivity_RetroActivityFuture_nativeGetContentFps
  (JNIEnv* env, jclass clazz)
{
    float fallback = 60.0f;

    // Prefer content fps from runloop
    runloop_state_t *st = runloop_state_get_ptr();
    if (st) {
        const struct retro_system_av_info *av = runloop_get_system_av_info();
        if (av && av->timing.fps > 0.0) {
            RARCH_LOG("[Android] Content FPS (av_info): %f\n", (float)av->timing.fps);
            return (jfloat)av->timing.fps;
        }
    }

    // Fallback to configured video refresh rate if available
    settings_t *settings = config_get_ptr();
    if (settings && settings->floats.video_refresh_rate > 0.0f) {
        RARCH_LOG("[Android] Using video_refresh_rate as FPS: %f\n",
                  settings->floats.video_refresh_rate);
        return (jfloat)settings->floats.video_refresh_rate;
    }

    RARCH_LOG("[Android] Falling back to default FPS: %f\n", fallback);
    return (jfloat)fallback;
}


