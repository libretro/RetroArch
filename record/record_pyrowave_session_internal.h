/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_SESSION_INTERNAL_H
#define RECORD_PYROWAVE_SESSION_INTERNAL_H
#include "record_pyrowave_session.h"
#include <string.h>

/* Typed C89 thunks keep backend SDK objects out of the transport owner. */
#define RECORD_PYROWAVE_SESSION_BINDING(prefix, type, acquire_fn) \
static int prefix##_session_take(void *user, record_gpu_frame_t *frame) \
{ return prefix##_take((type*)user, frame); } \
static int prefix##_session_packet(void *user, unsigned slot, record_pyrowave_packet_t *packet) \
{ return prefix##_packet((type*)user, slot, packet); } \
static bool prefix##_session_release(void *user, unsigned slot) \
{ return prefix##_release((type*)user, slot); } \
static int prefix##_session_acquire(void *user, void **data, size_t *capacity) \
{ return acquire_fn((type*)user, data, capacity); } \
static int prefix##_session_submit(void *user, size_t bytes, uint64_t pts, uint64_t duration) \
{ return prefix##_receive_submit((type*)user, bytes, pts, duration); } \
static bool prefix##_session_cancel(void *user) \
{ return prefix##_receive_cancel((type*)user); } \
static bool prefix##_session_finish(void *user) \
{ return prefix##_finish((type*)user); } \
void prefix##_session_video(type *codec, record_pyrowave_video_t *video) \
{ \
   if (!video) return; \
   memset(video, 0, sizeof(*video)); video->user = codec; \
   video->take = prefix##_session_take; video->packet = prefix##_session_packet; \
   video->release = prefix##_session_release; video->acquire = prefix##_session_acquire; \
   video->submit = prefix##_session_submit; video->cancel = prefix##_session_cancel; \
   video->finish = prefix##_session_finish; \
}
#endif
