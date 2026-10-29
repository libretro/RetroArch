/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef NET_RTMP_COMMAND_H
#define NET_RTMP_COMMAND_H
#include <net/net_rtmp_chunk.h>

enum net_rtmp_command
{
   NET_RTMP_CONNECT,
   NET_RTMP_CREATE_STREAM,
   NET_RTMP_PUBLISH,
   NET_RTMP_DELETE_STREAM,
   NET_RTMP_RELEASE_STREAM,
   NET_RTMP_FC_PUBLISH
};
typedef struct net_rtmp_command_config
{
   const char *application, *url, *name;
   uint32_t transaction, stream;
} net_rtmp_command_config_t;
RETRO_BEGIN_DECLS
/* Encodes one AMF0 command body into caller storage; no I/O or allocation.
 * Strings are UTF-8, at most 4096 bytes each, and must not alias output.
 * Failure leaves size zero; partial output must not be transmitted.
 * Connect uses transaction 1; publish/delete use 0; other commands use config.
 * Publish targets the assigned message stream; other commands target stream 0. */
bool net_rtmp_command_encode(enum net_rtmp_command command,
      const net_rtmp_command_config_t *config,
      void *output, size_t capacity, size_t *size);
RETRO_END_DECLS
#endif
