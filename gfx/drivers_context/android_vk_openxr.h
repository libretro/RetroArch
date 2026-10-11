#ifndef android_vk_openxr_H
#define android_vk_openxr_H

#include <stdint.h>
#include <boolean.h>
#include <libretro.h>
#include <retro_inline.h>

#ifndef ANDROID
/* The projection-layer context is Android's: elsewhere the shared
 * driver's path for it folds away. */
static INLINE bool android_vk_openxr_begin_frame(void) { return false; }
static INLINE void android_vk_openxr_end_frame(bool stereo_layer) { (void)stereo_layer; }
static INLINE unsigned android_vk_openxr_get_backbuffer_index(int eye) { (void)eye; return 0; }
static INLINE bool android_vk_openxr_has_session(void) { return false; }
static INLINE bool android_vk_openxr_is_session_ready(void) { return false; }
static INLINE bool android_vk_openxr_get_eye_state(struct retro_vr_eye_state out[2]) { (void)out; return false; }
#else

/* Direct entry points into the XR frame loop, called from vulkan.c */
bool android_vk_openxr_begin_frame(void);

/* Releases both eye swapchain images and calls xrEndFrame with a stereo
 * projection composition layer. */
void android_vk_openxr_end_frame(bool stereo_layer);

bool android_vk_openxr_sample_tracking(void);
bool android_vk_openxr_get_head_pose(struct retro_vr_head_pose *out);
bool android_vk_openxr_get_eye_size(unsigned *w, unsigned *h);
bool android_vk_openxr_set_reference_space(enum retro_vr_reference_space *space);
uint32_t android_vk_openxr_take_frame_flags(void);

/* Index into the combined_images[]/backbuffers[] array (built at init
 * time as [xr_image_idx * 2 + eye]) that this eye's just-acquired image
 * corresponds to for the current frame. Only valid between a successful
 * begin_frame and the matching end_frame. */
unsigned android_vk_openxr_get_backbuffer_index(int eye);

/* True once xrCreateSession + both eye swapchains have been created
 * successfully and the session has not since been torn down. */
bool android_vk_openxr_has_session(void);

/* has_session, and the session is between xrBeginSession and xrEndSession. */
bool android_vk_openxr_is_session_ready(void);

/* True if vk_data is the OpenXR context's gfx_ctx_vulkan_data_t; lets
 * vulkan_common.c route device creation without reaching into this TU's
 * state. */
bool android_vk_openxr_owns_vk_context(const void *vk_data);

/* True while this context driver is current (its XR instance exists). */
bool android_vk_openxr_active(void);

#ifdef XR_VERSION_1_0
XrInstance android_vk_openxr_xr_instance(void);
XrSystemId android_vk_openxr_xr_system_id(void);
#endif

bool android_vk_openxr_get_eye_state(struct retro_vr_eye_state out[2]);

#endif /* ANDROID */

#endif
