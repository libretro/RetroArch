/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_ENCODE_TEST_TYPES_H
#define MEDIACODEC_ENCODE_TEST_TYPES_H
#include "../../gfx/mediacodec_session/mediacodec_session_test_types.h"
#define AMEDIACODEC_CONFIGURE_FLAG_ENCODE 1
typedef struct AMediaMuxer
{
   int unused;
} AMediaMuxer;
typedef enum OutputFormat
{
   AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4 = 0
} OutputFormat;
#endif
