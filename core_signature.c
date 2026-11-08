/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <crypto/pk.h>
#include <lrc_hash.h>
#include <string/stdstring.h>

#include "core_signature.h"
#include "core_signing_keys.h"

#if CORE_SIGNING_KEYS_COUNT > 0
static const uint8_t core_signing_keys[CORE_SIGNING_KEYS_COUNT][CORE_SIG_KEY_LEN] =
{
   CORE_SIGNING_KEYS_LIST
};
#endif

const uint8_t *core_sig_builtin_keys(size_t *num_keys)
{
#if CORE_SIGNING_KEYS_COUNT > 0
   *num_keys = CORE_SIGNING_KEYS_COUNT;
   return core_signing_keys[0];
#else
   *num_keys = 0;
   return NULL;
#endif
}

static uint64_t core_sig_get_u64(const uint8_t *p)
{
   uint64_t v = 0;
   unsigned i;
   for (i = 0; i < 8; i++)
      v = (v << 8) | p[i];
   return v;
}

static void core_sig_sha256(const uint8_t *buf, size_t len, uint8_t *out)
{
   struct sha256_state sha;
   sha256_stream_init(&sha, 0);
   sha256_stream_update(&sha, buf, len);
   sha256_stream_final(&sha, out);
}

void core_sig_key_id(const uint8_t *key, uint8_t *id)
{
   uint8_t digest[CORE_SHA256_LEN];
   core_sig_sha256(key, CORE_SIG_KEY_LEN, digest);
   memcpy(id, digest, CORE_SIG_KEY_ID_LEN);
}

bool core_sig_parse(struct core_sig *sig, const uint8_t *buf, size_t len)
{
   size_t n;
   if (     len < CORE_SIG_FIXED_LEN
         || memcmp(buf, CORE_SIG_MAGIC, sizeof(CORE_SIG_MAGIC) - 1))
      return false;
   n = buf[CORE_SIG_FIXED_LEN - 1];
   /* p256_ecdsa_verify takes no lengths: exactly one r || s, no more */
   if (     n == 0
         || len != CORE_SIG_FIXED_LEN + n + CORE_SIG_RS_LEN
         || memchr(buf + CORE_SIG_FIXED_LEN, '\0', n))
      return false;
   /* The rollback rule reads 0 as unsigned, never too old */
   if (!core_sig_get_u64(buf + 16))
      return false;
   memcpy(sig->key_id, buf + 8, CORE_SIG_KEY_ID_LEN);
   sig->build_time = core_sig_get_u64(buf + 16);
   memcpy(sig->core_sha256, buf + 24, CORE_SHA256_LEN);
   memcpy(sig->name, buf + CORE_SIG_FIXED_LEN, n);
   sig->name[n] = '\0';
   memcpy(sig->r, buf + CORE_SIG_FIXED_LEN + n, 32);
   memcpy(sig->s, buf + CORE_SIG_FIXED_LEN + n + 32, 32);
   core_sig_sha256(buf, CORE_SIG_FIXED_LEN + n, sig->digest);
   return true;
}

enum core_check_result core_sig_verify(const struct core_sig *sig,
      const char *file_name, const uint8_t *core_sha256,
      const uint8_t *keys, size_t num_keys)
{
   uint8_t id[CORE_SIG_KEY_ID_LEN];
   size_t i;
   if (!string_is_equal(sig->name, file_name))
      return CORE_CHECK_MODIFIED;
   for (i = 0; i < num_keys; i++)
   {
      core_sig_key_id(keys + i * CORE_SIG_KEY_LEN, id);
      if (!memcmp(id, sig->key_id, CORE_SIG_KEY_ID_LEN))
         break;
   }
   if (i == num_keys)
      return CORE_CHECK_UNKNOWN_KEY;
   if (     !memcmp(sig->core_sha256, core_sha256, CORE_SHA256_LEN)
         && p256_ecdsa_verify(keys + i * CORE_SIG_KEY_LEN, sig->digest, CORE_SHA256_LEN,
               sig->r, sig->s) == 0)
      return CORE_CHECK_OK;
   return CORE_CHECK_MODIFIED;
}
