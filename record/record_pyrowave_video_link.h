/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_VIDEO_LINK_H
#define RECORD_PYROWAVE_VIDEO_LINK_H
#include "record_pyrowave_session.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_video_link record_pyrowave_video_link_t;
/* Stable I/O-facing endpoint across renderer lifetimes. One I/O reader and
 * one main-thread controller. Backend resources remain borrowed until pause
 * reports idle; the renderer must also retire GPU work before freeing them. */
record_pyrowave_video_link_t *record_pyrowave_video_link_new(void);
void record_pyrowave_video_link_video(record_pyrowave_video_link_t *link,
      record_pyrowave_video_t *video);
void record_pyrowave_video_link_pause(record_pyrowave_video_link_t *link);
bool record_pyrowave_video_link_idle(record_pyrowave_video_link_t *link);
bool record_pyrowave_video_link_bind(record_pyrowave_video_link_t *link,
      const record_pyrowave_video_t *video);
void record_pyrowave_video_link_close(record_pyrowave_video_link_t *link);
/* After the dispatcher is quiescent and no caller can access the endpoint. */
void record_pyrowave_video_link_free(record_pyrowave_video_link_t *link);
RETRO_END_DECLS
#endif
