/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <lrc_hash.h>
#include <rthreads/rthreads.h>
#include <string/stdstring.h>

#include "../../../core_signature.h"
#include "../../../core_signing_keys.h"
#include "core_signature_fixtures.h"

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint8_t *fx_core;
static uint8_t fx_keys[2 * CORE_SIG_KEY_LEN];

static void fx_make_core(void)
{
   unsigned long x = FX_CORE_SEED;
   size_t i;
   fx_core = (uint8_t*)malloc(FX_CORE_SIZE);
   for (i = 0; i < FX_CORE_SIZE; i++)
   {
      x          = (x * 1103515245UL + 12345UL) & 0xffffffffUL;
      fx_core[i] = (uint8_t)(x >> 24);
   }
   memcpy(fx_keys, fx_key1, CORE_SIG_KEY_LEN);
   memcpy(fx_keys + CORE_SIG_KEY_LEN, fx_key2, CORE_SIG_KEY_LEN);
}

static void sha(const uint8_t *buf, size_t len, uint8_t *out)
{
   struct sha256_state st;
   sha256_stream_init(&st, 0);
   sha256_stream_update(&st, buf, len);
   sha256_stream_final(&st, out);
}

/* A copy of a fixture with one byte flipped; idx past the end appends */
static uint8_t edited[CORE_SIG_MAX_LEN + 2];
static const uint8_t *flip(const uint8_t *sig, size_t len, size_t idx)
{
   memcpy(edited, sig, len);
   if (idx < len)
      edited[idx] ^= 0x01;
   else
      edited[len] = 0;
   return edited;
}

static enum core_check_result verify(const uint8_t *buf, size_t len,
      const char *name, const uint8_t *core_sha, size_t num_keys)
{
   struct core_sig sig;
   if (!core_sig_parse(&sig, buf, len))
      return CORE_CHECK_MODIFIED;
   return core_sig_verify(&sig, name, core_sha, fx_keys, num_keys);
}

static void test_sig(void)
{
   struct core_sig sig;
   uint8_t core_sha[CORE_SHA256_LEN];
   uint8_t digest[CORE_SHA256_LEN];
   uint8_t id[CORE_SIG_KEY_ID_LEN];
   size_t n = sizeof(fx_sig_good);
   size_t count;
   const uint8_t *keys;
   uint8_t *short_sig;
   sha(fx_core, FX_CORE_SIZE, core_sha);

   /* Parse: exact length, magic, name */
   CHECK(core_sig_parse(&sig, fx_sig_good, n));
   CHECK(string_is_equal(sig.name, FX_NAME));
   CHECK(sig.build_time == FX_TIME);
   CHECK(!memcmp(sig.core_sha256, core_sha, CORE_SHA256_LEN));
   sha(fx_sig_good, n - CORE_SIG_RS_LEN, digest);
   CHECK(!memcmp(sig.digest, digest, CORE_SHA256_LEN));
   CHECK(!core_sig_parse(&sig, fx_sig_good, n - 1));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, n), n + 1));
   CHECK(!core_sig_parse(&sig, fx_sig_good, CORE_SIG_FIXED_LEN));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, 0), n));
   CHECK(!core_sig_parse(&sig, flip(fx_sig_good, n, CORE_SIG_FIXED_LEN - 1),
            n));
   memcpy(edited, fx_sig_good, n);
   edited[CORE_SIG_FIXED_LEN] = '\0';
   CHECK(!core_sig_parse(&sig, edited, n));
   /* Build time 0 would never age out under the rollback rule */
   memcpy(edited, fx_sig_good, n);
   memset(edited + 16, 0, 8);
   CHECK(!core_sig_parse(&sig, edited, n));
   edited[23] = 1;
   CHECK(core_sig_parse(&sig, edited, n) && sig.build_time == 1);
   memcpy(edited, fx_sig_good, CORE_SIG_FIXED_LEN);
   edited[CORE_SIG_FIXED_LEN - 1] = 0;
   memcpy(edited + CORE_SIG_FIXED_LEN, fx_sig_good + n - CORE_SIG_RS_LEN,
         CORE_SIG_RS_LEN);
   CHECK(!core_sig_parse(&sig, edited, CORE_SIG_FIXED_LEN + CORE_SIG_RS_LEN));
   /* Shorter than the fixed part, alone in its allocation so a sanitizer
    * sees any read past it */
   short_sig = (uint8_t*)malloc(CORE_SIG_FIXED_LEN - 1);
   memcpy(short_sig, fx_sig_good, CORE_SIG_FIXED_LEN - 1);
   CHECK(!core_sig_parse(&sig, short_sig, CORE_SIG_FIXED_LEN - 1));
   CHECK(!core_sig_parse(&sig, short_sig, sizeof(CORE_SIG_MAGIC) - 1));
   free(short_sig);

   /* Key ids are the SHA-256 of the key, not the key */
   sha(fx_key1, CORE_SIG_KEY_LEN, digest);
   core_sig_key_id(fx_key1, id);
   CHECK(!memcmp(id, digest, CORE_SIG_KEY_ID_LEN));
   CHECK(!memcmp(id, fx_sig_good + 8, CORE_SIG_KEY_ID_LEN));

   /* Verify */
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 2) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 1) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_key2, sizeof(fx_sig_key2), FX_NAME, core_sha, 2)
         == CORE_CHECK_OK);
   CHECK(verify(fx_sig_key2, sizeof(fx_sig_key2), FX_NAME, core_sha, 1)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(fx_sig_good, n, FX_NAME, core_sha, 0)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(fx_sig_older, sizeof(fx_sig_older), FX_NAME, core_sha, 2)
         == CORE_CHECK_OK);
   /* Validly signed for another name, so only the name check refuses */
   CHECK(verify(fx_sig_other_name, sizeof(fx_sig_other_name),
            "other_libretro.so", core_sha, 2) == CORE_CHECK_OK);
   CHECK(verify(fx_sig_other_name, sizeof(fx_sig_other_name),
            FX_NAME, core_sha, 2) == CORE_CHECK_MODIFIED);
   CHECK(verify(fx_sig_good, n, "test_libretro.dll", core_sha, 2)
         == CORE_CHECK_MODIFIED);
   /* One flipped byte: key id, build time, core hash, name, r, s */
   CHECK(verify(flip(fx_sig_good, n, 9), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_UNKNOWN_KEY);
   CHECK(verify(flip(fx_sig_good, n, 23), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, 30), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, CORE_SIG_FIXED_LEN), n, FX_NAME,
            core_sha, 2) == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 40), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 64), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   CHECK(verify(flip(fx_sig_good, n, n - 1), n, FX_NAME, core_sha, 2)
         == CORE_CHECK_MODIFIED);
   /* The core itself */
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   sha(fx_core, FX_CORE_SIZE, digest);
   fx_core[FX_CORE_SIZE / 2] ^= 0x80;
   CHECK(verify(fx_sig_good, n, FX_NAME, digest, 2) == CORE_CHECK_MODIFIED);

   /* What core_signing_keys.h lists */
   keys = core_sig_builtin_keys(&count);
   CHECK(count == CORE_SIGNING_KEYS_COUNT && (keys == NULL) == (count == 0));
}

int main(void)
{
   /* FAIL lines survive a crash or a sanitizer exit */
   setvbuf(stdout, NULL, _IONBF, 0);
   fx_make_core();
   test_sig();
   free(fx_core);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
