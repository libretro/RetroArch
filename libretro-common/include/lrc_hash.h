/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (lrc_hash.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_HASH_H
#define __LIBRETRO_SDK_HASH_H

#include <stdint.h>
#include <stddef.h>

#include <compat/msvc.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <retro_inline.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/**
 * sha256_hash:
 * @s                 : Output.
 * @in                : Input.
 * @len               : Size of @out.
 *
 * Hashes SHA256 and outputs a human readable string.
 **/
/**
 * Streaming SHA-256 / SHA-224.
 *
 * The one-shot sha256_hash() above stays the short way to a hex
 * string; these are for a caller that feeds a digest in pieces, or
 * that has to copy a digest mid-stream. Members are laid out for the
 * implementation to work on, not to be read by a caller.
 */
struct sha256_state
{
   uint64_t len;
   union
   {
      uint8_t  u8[64];
      uint32_t u32[16];
   } in;
   uint32_t w[64];
   uint32_t h[8];
   unsigned inlen;
   unsigned is224;
};

/**
 * sha256_stream_init:
 * @p                 : State to start.
 * @is224             : Non-zero selects SHA-224 rather than SHA-256.
 **/
void sha256_stream_init(struct sha256_state *p, unsigned is224);

void sha256_stream_update(struct sha256_state *p,
      const uint8_t *data, size_t len);

/**
 * sha256_stream_final:
 * @digest            : 32 octets for SHA-256, 28 for SHA-224.
 **/
void sha256_stream_final(struct sha256_state *p, uint8_t *digest);

/**
 * sha256_stream_block:
 *
 * Compresses one 64-octet block straight into the state, leaving the
 * length count alone. For a caller driving the padding itself.
 **/
void sha256_stream_block(struct sha256_state *p, const uint8_t *data);

void sha256_hash(char *s, const uint8_t *in, size_t len);

/**
 * SHA1Digest:
 * @data              : Input.
 * @len               : Size of @data.
 * @digest            : Output.
 *
 * Hashes SHA1
 **/
void SHA1Digest(const uint8_t* data, size_t len, uint8_t digest[20]);

/**
 * Streaming SHA-1, alongside the one-shot SHA1Digest() above.
 */
struct sha1_state
{
   uint32_t      digest[5];
   uint32_t      length_low;
   uint32_t      length_high;
   unsigned char block[64];
   int           block_index;
   int           computed;
   int           corrupted;
};

void sha1_stream_init(struct sha1_state *p);

void sha1_stream_update(struct sha1_state *p,
      const uint8_t *data, size_t len);

/**
 * sha1_stream_final:
 * @digest            : 20 octets.
 *
 * Returns: true on success, false where the stream was marked bad.
 **/
bool sha1_stream_final(struct sha1_state *p, uint8_t *digest);

/**
 * sha1_stream_block:
 *
 * Compresses one 64-octet block straight into the state, leaving the
 * length count alone.
 **/
void sha1_stream_block(struct sha1_state *p, const uint8_t *data);

int sha1_calculate(const char *path, char *result);

uint32_t djb2_calculate(const char *str);

/* One portable MD5 for every platform. This used to alias the CC_MD5_*
 * family from CommonCrypto on Apple, but those have been deprecated
 * since macOS 10.15, and the first code to actually call the aliases
 * (libsmb2's hmac-md5) promptly failed the Apple builds on it. The
 * implementation in utils/md5.c has no platform dependencies at all. */
typedef struct {
	uint32_t lo, hi;
	uint32_t a, b, c, d;
	unsigned char buffer[64];
	uint32_t block[16];
} MD5_CTX;

/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

void MD5_Init(MD5_CTX *ctx);
void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size);
void MD5_Final(unsigned char *result, MD5_CTX *ctx);

RETRO_END_DECLS

#endif
