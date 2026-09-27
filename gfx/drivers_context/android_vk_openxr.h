#ifndef android_vk_openxr_H
#define android_vk_openxr_H

#include <stdint.h>
#include <boolean.h>
#include <libretro.h>

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
bool android_vk_openxr_is_session_ready(void);

bool android_vk_openxr_button(unsigned button);
int16_t android_vk_openxr_axis(unsigned axis);

bool android_vk_openxr_get_eye_state(struct retro_vr_eye_state out[2]);

#endif
