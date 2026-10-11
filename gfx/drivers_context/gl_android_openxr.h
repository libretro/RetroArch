#ifndef GL_ANDROID_OPENXR_H
#define GL_ANDROID_OPENXR_H

#include <stdint.h>
#include <boolean.h>
#include <libretro.h>
#include <retro_inline.h>

#ifndef ANDROID
/* The GLES headset context is Android's: elsewhere the shared driver's
 * headset path folds away. */
static INLINE bool gl_android_openxr_active(void) { return false; }
static INLINE bool gl_android_openxr_begin_frame(void) { return false; }
static INLINE uint32_t gl_android_openxr_get_framebuffer(void) { return 0; }
static INLINE bool gl_android_openxr_get_eye_state(struct retro_vr_eye_state out[2]) { (void)out; return false; }
static INLINE bool gl_android_openxr_has_session(void) { return false; }
static INLINE bool gl_android_openxr_is_session_ready(void) { return false; }
static INLINE bool gl_android_openxr_stereo_active(void) { return false; }
#else

/* True while the OpenXR GLES context driver is the live context */
bool gl_android_openxr_active(void);

/* xrWaitFrame / xrBeginFrame / xrLocateViews */
bool gl_android_openxr_begin_frame(void);

uint32_t gl_android_openxr_get_framebuffer(void);

bool gl_android_openxr_sample_tracking(void);
bool gl_android_openxr_get_head_pose(struct retro_vr_head_pose *out);
bool gl_android_openxr_get_eye_state(struct retro_vr_eye_state out[2]);
bool gl_android_openxr_get_eye_size(unsigned *w, unsigned *h);
bool gl_android_openxr_set_reference_space(enum retro_vr_reference_space *space);
uint32_t gl_android_openxr_take_frame_flags(void);

bool gl_android_openxr_has_session(void);

/* has_session, and the session is between xrBeginSession and xrEndSession. */
bool gl_android_openxr_is_session_ready(void);

void gl_android_openxr_set_stereo(bool on);
bool gl_android_openxr_stereo_active(void);
#endif /* ANDROID */

#endif
