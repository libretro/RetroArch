#include <jni.h>
#include <string.h>
#include <libretro.h>   /* struct retro_system_av_info */
#include "runloop.h"    /* runloop_state_get_ptr(), runloop_state_t */
#include "verbosity.h"
#include "configuration.h"

JNIEXPORT jfloat JNICALL
Java_com_retroarch_browser_retroactivity_RetroActivityFuture_nativeGetContentFps
  (JNIEnv* env, jclass clazz)
{
    runloop_state_t *st = runloop_state_get_ptr();
    settings_t *settings = config_get_ptr();
    if (!settings)
        return 60.0f;


    float fps = (float)settings->floats.video_refresh_rate;
    if (fps <= 0.0f)
        return 60.0f;

    RARCH_LOG("[Android] Detected FPS from game is %f.\n",fps);
    return fps;
}

