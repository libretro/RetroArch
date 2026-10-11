#ifndef GL_ANDROID_OPENXR_H
#define GL_ANDROID_OPENXR_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#if !defined(ANDROID) || !defined(HAVE_OPENGLES3)
/* The GLES headset context is Android's, with ES3: elsewhere gl2's
 * headset path folds away. */
static INLINE bool gl_android_openxr_active(void) { return false; }
static INLINE bool gl_android_openxr_begin_frame(void) { return false; }
static INLINE uint32_t gl_android_openxr_get_framebuffer(void) { return 0; }
static INLINE void gl_android_openxr_set_stereo(bool on) { (void)on; }
static INLINE bool gl_android_openxr_eye_targets(uint32_t tex[2],
      unsigned *dims) { (void)tex; (void)dims; return false; }
static INLINE void gl_android_openxr_eyes_drawn(unsigned width,
      unsigned height) { (void)width; (void)height; }
static INLINE void gl_android_openxr_new_frame(void) { }
static INLINE void gl_android_openxr_place_kept(void) { }
static INLINE bool gl_android_openxr_repeat_eyes(void) { return false; }
#else
/* While the headset's GLES context is the live one. */
bool gl_android_openxr_active(void);

/* Polls and begins the headset's frame, once a gl2 frame. */
bool gl_android_openxr_begin_frame(void);

/* The window gl2 draws in, which the headset shows. */
uint32_t gl_android_openxr_get_framebuffer(void);

/* A tracked map is shown: its eyes, not the window, fill the headset. */
void gl_android_openxr_set_stereo(bool on);

/* gl2, in its frame, for a tracked map's eyes: this frame's two eye
 * images as textures, and their size. False when the headset cannot
 * take them now; the last ones stay shown. */
bool gl_android_openxr_eye_targets(uint32_t tex[2], unsigned *dims);
/* gl2 drew each eye width x height from its image's origin. */
void gl_android_openxr_eyes_drawn(unsigned width, unsigned height);

/* gl2, each frame: views kept across a reinit go with a new core frame,
 * which they did not draw, or on a repeat are placed in this session
 * once a frame time locates it. */
void gl_android_openxr_new_frame(void);
void gl_android_openxr_place_kept(void);
/* gl2, on a software repeat: no projection is shown and the views its
 * frame was drawn from hold in this session, so its eyes would be. */
bool gl_android_openxr_repeat_eyes(void);
#endif

#endif
