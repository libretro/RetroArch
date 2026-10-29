/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_RTMP_COMMAND_H
#define RECORD_RTMP_COMMAND_H
#include "record_rtmp_chunk.h"

enum record_rtmp_command
{
   RECORD_RTMP_CONNECT,
   RECORD_RTMP_CREATE_STREAM,
   RECORD_RTMP_PUBLISH,
   RECORD_RTMP_DELETE_STREAM,
   RECORD_RTMP_RELEASE_STREAM,
   RECORD_RTMP_FC_PUBLISH
};
typedef struct record_rtmp_command_config
{
   const char *application, *url, *name;
   uint32_t transaction, stream;
} record_rtmp_command_config_t;
RETRO_BEGIN_DECLS
/* Encodes one AMF0 command body into caller storage; no I/O or allocation.
 * Strings are UTF-8, at most 4096 bytes each, and must not alias output.
 * Failure leaves size zero; partial output must not be transmitted.
 * Connect uses transaction 1; publish/delete use 0; other commands use config.
 * Publish targets the assigned message stream; other commands target stream 0. */
bool record_rtmp_command_encode(enum record_rtmp_command command,
      const record_rtmp_command_config_t *config,
      void *output, size_t capacity, size_t *size);
RETRO_END_DECLS
#endif
