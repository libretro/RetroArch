/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_NATIVE_BUFFER_H
#define RECORD_NATIVE_BUFFER_H

#include <string.h>
#include <retro_inline.h>
#include "record_gpu_queue.h"

/* Native capture rings are bounded; invalid legacy settings use four slots. */
static INLINE unsigned recording_native_buffer_min(const char *ident)
{
   return ident && !strcmp(ident, "nvenc") ? 4u : 2u;
}

static INLINE unsigned recording_native_buffer_count(
      const char *ident, unsigned requested)
{
   return requested >= recording_native_buffer_min(ident)
      && requested <= RECORD_GPU_QUEUE_SLOTS ? requested : 4u;
}

#endif
