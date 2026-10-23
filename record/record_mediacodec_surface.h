/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_SURFACE_H
#define RECORD_MEDIACODEC_SURFACE_H
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <boolean.h>
#include <stdint.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_mediacodec_surface record_mediacodec_surface_t;
typedef struct record_mediacodec_surface_ops
{
   PFNEGLQUERYSTRINGPROC query_string;
   PFNEGLGETCONFIGATTRIBPROC config_attrib;
   PFNEGLCREATECONTEXTPROC create_context;
   PFNEGLCREATEWINDOWSURFACEPROC create_surface;
   PFNEGLDESTROYCONTEXTPROC destroy_context;
   PFNEGLDESTROYSURFACEPROC destroy_surface;
   PFNEGLQUERYSURFACEPROC query_surface;
   PFNEGLQUERYAPIPROC query_api;
   PFNEGLGETCURRENTCONTEXTPROC current_context;
   PFNEGLGETCURRENTDISPLAYPROC current_display;
   PFNEGLGETCURRENTSURFACEPROC current_surface;
   PFNEGLMAKECURRENTPROC make_current;
   PFNEGLPRESENTATIONTIMEANDROIDPROC presentation_time;
   PFNEGLSWAPBUFFERSPROC swap;
} record_mediacodec_surface_ops_t;
/* Cold GLES worker only. Borrows the initialized display and encoder window,
 * which must survive free. Creates an ES3 context, optionally in the renderer
 * share group. Requires a matching recordable RGB8 or PQ RGB10 configuration.
 * PQ requires matching encoder color metadata; this does not configure it.
 * Failure may retain a non-NULL output for native cleanup retry. */
bool record_mediacodec_surface_new(const record_mediacodec_surface_ops_t *ops,
      EGLDisplay display, EGLConfig config, EGLContext share,
      EGLNativeWindowType window, unsigned width, unsigned height, bool pq,
      record_mediacodec_surface_t **output);
/* Worker only. Makes the recording context/window current and saves the
 * worker's previous GLES binding. Caller draws without CPU pixel readback.
 * Source leases must survive completion of those GPU commands. */
bool record_mediacodec_surface_begin(record_mediacodec_surface_t *surface);
/* Terminal cleanup scope, including after a failed post or restore. Reuses
 * an active owned scope, otherwise saves/restores the previous binding.
 * Permanently disables posting; no draw or source reuse is authorized. */
bool record_mediacodec_surface_begin_retirement(
      record_mediacodec_surface_t *surface);
/* Posts the drawn buffer with its exact nonnegative source PTS, then restores
 * the previous binding. Strictly increasing microseconds, including zero.
 * Failure is sticky and may follow a successful post: never retry the frame. */
bool record_mediacodec_surface_present(
      record_mediacodec_surface_t *surface, int64_t timestamp_us);
/* Restore without posting; also retries a failed binding restore. This does
 * not retire GPU reads from a canceled draw or a failed post. */
bool record_mediacodec_surface_leave(record_mediacodec_surface_t *surface);
/* Worker quiescent, after GPU source reads retire. Refuses an active scope.
 * A native destruction failure retains the remaining objects for retry. */
bool record_mediacodec_surface_free(record_mediacodec_surface_t *surface);
RETRO_END_DECLS
#endif
