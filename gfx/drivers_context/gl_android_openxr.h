#ifndef GL_ANDROID_OPENXR_H
#define GL_ANDROID_OPENXR_H

#include <stdint.h>
#include <boolean.h>
#include <libretro.h>

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

bool gl_android_openxr_is_session_ready(void);

void gl_android_openxr_set_stereo(bool on);
bool gl_android_openxr_stereo_active(void);

#endif
