#include <jni.h>
#include <string.h>
#include <libretro.h>   /* struct retro_system_av_info */
#include "runloop.h"    /* runloop_state_get_ptr(), runloop_state_t */
#include "verbosity.h"

JNIEXPORT jfloat JNICALL
Java_com_retroarch_browser_retroactivity_RetroActivityFuture_nativeGetContentFps
  (JNIEnv* env, jclass clazz)
{
    runloop_state_t *st = runloop_state_get_ptr();
    if (!st)
        return 0.0f;

    struct retro_system_av_info av;
    memset(&av, 0, sizeof(av));

    if (st->current_core.retro_get_system_av_info)
        st->current_core.retro_get_system_av_info(&av);
    else
        return 0.0f;

    float fps = (float)av.timing.fps;
    if (fps <= 0.0f)
        return 0.0f;

    /* optional: snap to common display modes so Android will switch */
    if (fps > 59.5f && fps < 60.5f) fps = 60.0f;
    if (fps > 49.5f && fps < 50.5f) fps = 50.0f;
    RARCH_LOG("[Android] Detected FPS from game is %f.\n",fps);
    return fps;
}

