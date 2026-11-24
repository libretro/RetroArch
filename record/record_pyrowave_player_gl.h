/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_PLAYER_GL_H
#define RECORD_PYROWAVE_PLAYER_GL_H
#include "record_pyrowave_source.h"
#include "record_pyrowave_gl.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_source_gl record_pyrowave_player_gl_t;
/* Cold construction on the frontend GL context from negotiated metadata.
 * Programs and decoder resources belong to the returned player. */
record_pyrowave_player_gl_t *record_pyrowave_player_gl_new(
      const record_pyrowave_source_config_t *config);
void record_pyrowave_player_gl_video(record_pyrowave_player_gl_t *player,
      record_pyrowave_video_t *video);
/* Context owner. Decode restores renderer bindings; poll never waits.
 * Image lends survive repeated display until their final draw, then present.
 * Drop only an image never used by outstanding display work. */
int record_pyrowave_player_gl_decode(record_pyrowave_player_gl_t *player);
int record_pyrowave_player_gl_discard(record_pyrowave_player_gl_t *player,
      record_gpu_frame_t *frame);
bool record_pyrowave_player_gl_poll(record_pyrowave_player_gl_t *player);
int record_pyrowave_player_gl_peek(record_pyrowave_player_gl_t *player, record_gpu_frame_t *frame);
int record_pyrowave_player_gl_image(record_pyrowave_player_gl_t *player,
      record_pyrowave_gl_image_t *image);
bool record_pyrowave_player_gl_drop(record_pyrowave_player_gl_t *player, unsigned slot);
bool record_pyrowave_player_gl_present(record_pyrowave_player_gl_t *player, unsigned slot);
bool record_pyrowave_player_gl_idle(const record_pyrowave_player_gl_t *player);
/* After I/O quiescence and every image/display loan has retired. */
bool record_pyrowave_player_gl_free(record_pyrowave_player_gl_t *player);
/* Cold recovery: I/O and renderer snapshots must be quiescent. Nonblocking
 * ordered marker retirement, then destruction, on the owning GL context. */
bool record_pyrowave_player_gl_retire(record_pyrowave_player_gl_t *player);
RETRO_END_DECLS
#endif
