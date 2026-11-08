/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_SIGNATURE_H__
#define CORE_SIGNATURE_H__

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <lrc_hash.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* <core>.sig, big-endian: magic (8), key id (8), build time (8, never
 * 0), core SHA-256 (32), name length (1), name, then r || s over the
 * SHA-256 of everything before it. */
#define CORE_SIG_MAGIC       "RACSIG01"
#define CORE_SIG_FIXED_LEN   57
#define CORE_SIG_NAME_MAX    255
#define CORE_SIG_RS_LEN      64
#define CORE_SIG_MAX_LEN     (CORE_SIG_FIXED_LEN + CORE_SIG_NAME_MAX + CORE_SIG_RS_LEN)
#define CORE_SIG_KEY_LEN     65
#define CORE_SIG_KEY_ID_LEN  8
#define CORE_SHA256_LEN      32

enum core_check_result
{
   CORE_CHECK_OK = 0,
   CORE_CHECK_UNSIGNED,
   CORE_CHECK_MODIFIED,
   CORE_CHECK_OLDER,
   CORE_CHECK_UNKNOWN_KEY,
   /* Could not be checked, as an unreadable core: never trusted, and
    * nothing is recorded */
   CORE_CHECK_ERROR
};

/* digest is our own SHA-256 of the signed bytes, never the file's */
struct core_sig
{
   uint64_t build_time;
   uint8_t  key_id[CORE_SIG_KEY_ID_LEN];
   uint8_t  core_sha256[CORE_SHA256_LEN];
   uint8_t  digest[CORE_SHA256_LEN];
   uint8_t  r[32];
   uint8_t  s[32];
   char     name[CORE_SIG_NAME_MAX + 1];
};

/* Keys are num_keys 65-byte P-256 points, 0x04 || X || Y, back to back.
 * libretro's, from core_signing_keys.h; NULL when there are none. */
const uint8_t *core_sig_builtin_keys(size_t *num_keys);

/* The first 8 bytes of the SHA-256 of a 65-byte key */
void core_sig_key_id(const uint8_t *key, uint8_t *id);

/* False unless buf is exactly one well-formed .sig */
bool core_sig_parse(struct core_sig *sig, const uint8_t *buf, size_t len);

/* OK, MODIFIED or UNKNOWN_KEY; the build time is the caller's */
enum core_check_result core_sig_verify(const struct core_sig *sig,
      const char *file_name, const uint8_t *core_sha256,
      const uint8_t *keys, size_t num_keys);

RETRO_END_DECLS

#endif
