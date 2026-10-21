/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ANDROID_FRAME_H
#define ANDROID_FRAME_H
#include "android_image_queue.h"
#include "mediacodec_session.h"
#include <formats/video_hw_frame.h>
RETRO_BEGIN_DECLS
typedef struct android_frame_pool android_frame_pool_t;
typedef struct android_frame_view {
 android_image_view_t image;
 mediacodec_output_format_t format;
 video_color_metadata_t color;
 unsigned pixfmt;
} android_frame_view_t;
/* Cold producer setup. One producer; one renderer owner per exported lease.
 * Close/free remain producer-worker operations. Export/claim/finish do not
 * allocate, wait or call native image APIs. */
android_frame_pool_t *android_frame_pool_new(unsigned count);
void android_frame_pool_close(android_frame_pool_t *pool);
bool android_frame_pool_free(android_frame_pool_t *pool);
/* 0 consumes image ticket into a frame; 1 BUSY/-1 refusal take nothing and
 * leave out untouched. Colour is already resolved; native format retained for
 * renderer verification. pixfmt conveys coded bit depth, not private layout. */
int android_frame_export(android_frame_pool_t *pool,const android_frame_view_t *view,video_hw_frame_t *out);
/* Identifies opaque Android handles before a driver interprets image as VkImage.
 * The normal video_frame_import copy preserves this callback and lease key. */
bool android_frame_callback(void (*abandon)(void*,uint64_t,uint64_t));
/* Renderer uses owner/done/done_value/image copied from the exported frame.
 * Claim consumes the pending lease; false takes nothing. A claimed lease must
 * finish with a GPU fd covering ready wait, all reads and ownership release.
 * Use -1 only for an unused image or an already-completed GPU signal. */
bool android_frame_claim(void *owner,uint64_t slot,uint64_t generation,uint64_t image,android_frame_view_t *view);
/* Renderer preparation refusal only, before recording/submitting image use.
 * Restores the caller's original pending lease; takes no fd or image ticket. */
bool android_frame_unclaim(void *owner,uint64_t slot,uint64_t generation);
bool android_frame_finish(void *owner,uint64_t slot,uint64_t generation,int owned_done_fd);
RETRO_END_DECLS
#endif
