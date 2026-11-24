/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GFX_MUSIC_VISUALIZER_H
#define GFX_MUSIC_VISUALIZER_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#define GFX_MUSIC_VISUALIZER_WIDTH 640
#define GFX_MUSIC_VISUALIZER_HEIGHT 360

typedef struct gfx_music_visualizer gfx_music_visualizer_t;

RETRO_BEGIN_DECLS

/* Create/free/reset only while producer and consumer are quiescent. */
gfx_music_visualizer_t *gfx_music_visualizer_new(void);
void gfx_music_visualizer_free(gfx_music_visualizer_t *visualizer);
void gfx_music_visualizer_reset(gfx_music_visualizer_t *visualizer);
/* Display consumer only: retry a dropped upload into the next writable slot. */
void gfx_music_visualizer_invalidate(gfx_music_visualizer_t *visualizer);
/* One audio producer: borrow stereo PCM for this call, before mixing. */
void gfx_music_visualizer_feed(gfx_music_visualizer_t *visualizer,
      const void *samples, size_t frames, bool floating, unsigned rate);
/* One display consumer: write a lent surface slot, honouring its pitch.
 * False means no new spectrum, or invalid destination; pixels stay untouched. */
bool gfx_music_visualizer_render(gfx_music_visualizer_t *visualizer,
      uint32_t *pixels, size_t pitch, bool rgba);

RETRO_END_DECLS
#endif
