/* audio/drivers/rwebaudio.c a second time, as a JSPI or ASYNC build
 * compiles it: a blocking write with no room awaits the JavaScript
 * half's RWebAudioWaitRoom() rather than the main loop's block.  Its
 * exported names are renamed so it links beside the published build's
 * copy in rwebaudio_test.c, against the same stand-ins. */

#undef  EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
#undef  EMSCRIPTEN_AUDIO_FAKE_BLOCK
#define EMSCRIPTEN_ASYNCIFY
#define EMSCRIPTEN_FULL_ASYNCIFY

#define audio_rwebaudio            audio_rwebaudio_asyncify
#define rwebaudio_recalibrate_time rwebaudio_recalibrate_time_asyncify

#include "../../../audio/drivers/rwebaudio.c"
