#ifndef GL_ANDROID_OPENXR_H
#define GL_ANDROID_OPENXR_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

#include "../video_defines.h"
#include "../video_views.h"

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
static INLINE void gl_android_openxr_recenter(unsigned count) { (void)count; }
static INLINE void gl_android_openxr_set_placement(unsigned placement,
      float distance, float width)
      { (void)placement; (void)distance; (void)width; }
static INLINE void gl_android_openxr_set_pacing(unsigned interval,
      float hz) { (void)interval; (void)hz; }
static INLINE void gl_android_openxr_set_views(const video_views_map_t *map,
      unsigned frame_dims, const video_viewport_t *vp, unsigned rotation,
      unsigned screen_layout, bool swap_eyes)
      { (void)map; (void)frame_dims; (void)vp; (void)rotation;
        (void)screen_layout; (void)swap_eyes; }
static INLINE float gl_android_openxr_refresh(float *rates, unsigned cap,
      unsigned *count) { (void)rates; (void)cap; *count = 0; return 0.0f; }
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
 * take them now; the last ones stay shown. On true, sRGB encoding is
 * off for the drawing until gl_android_openxr_eyes_drawn. */
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
/* gl2, each frame: the Recenter hotkey's count; a new one recenters
 * the headset's screens at its next frame. */
void gl_android_openxr_recenter(unsigned count);
/* gl2, each frame: where the headset keeps the screens (enum
 * video_openxr_placement), and their distance and width. */
void gl_android_openxr_set_placement(unsigned placement, float distance,
      float width);
/* gl2, once a frame: the headset frames each core frame shows for, and
 * the rate to ask the headset for (0: none). */
void gl_android_openxr_set_pacing(unsigned interval, float hz);
/* gl2, once a frame: the frame's views and the size they are in, where
 * the window draws the frame (from its top left) and its quarter turns,
 * and how screens are laid out and eyes swapped. While the menu is
 * closed, an untracked map's screens are cut from the window. */
void gl_android_openxr_set_views(const video_views_map_t *map,
      unsigned frame_dims, const video_viewport_t *vp, unsigned rotation,
      unsigned screen_layout, bool swap_eyes);
/* The headset's measured rate, 0 until known, and up to cap of the
 * rates it can be asked for. */
float gl_android_openxr_refresh(float *rates, unsigned cap,
      unsigned *count);
#endif

#endif
