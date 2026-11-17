/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <crypto/pk.h>

#include "../../../core_signature.h"
#include "../../../file_path_special.h"

/* Linked with core_signature.c built for Android, the build with
 * libretro's keys */

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* The buildbot's mgba_libretro_android.so, arm64-v8a nightly of
 * 2026-11-15: its .sig, signed with key 1, and the core's SHA-256 */
#define BUILDBOT_NAME "mgba_libretro_android.so"
#define BUILDBOT_TIME 1794727272

static const uint8_t buildbot_sig[145] =
{
   0x52, 0x41, 0x43, 0x53, 0x49, 0x47, 0x30, 0x31, 0xd2, 0x89, 0x89, 0xb6,
   0xaf, 0x40, 0x3a, 0x92, 0x00, 0x00, 0x00, 0x00, 0x6a, 0xf9, 0x5d, 0x68,
   0xb5, 0xa6, 0xed, 0x40, 0xed, 0xe7, 0x35, 0xea, 0x7f, 0x1d, 0x56, 0x91,
   0x6f, 0x4c, 0xee, 0xd8, 0xc2, 0xaa, 0xbe, 0x4d, 0x48, 0x1f, 0xe3, 0x38,
   0x3a, 0xe2, 0xb5, 0xe5, 0x48, 0x5b, 0x65, 0x45, 0x18, 0x6d, 0x67, 0x62,
   0x61, 0x5f, 0x6c, 0x69, 0x62, 0x72, 0x65, 0x74, 0x72, 0x6f, 0x5f, 0x61,
   0x6e, 0x64, 0x72, 0x6f, 0x69, 0x64, 0x2e, 0x73, 0x6f, 0xbf, 0xcb, 0x6e,
   0xd6, 0x03, 0xcd, 0x39, 0x74, 0x86, 0xfb, 0x26, 0x61, 0xa4, 0xeb, 0x57,
   0xa5, 0x1d, 0x48, 0xdf, 0x17, 0x21, 0xad, 0x9f, 0x8c, 0xf8, 0x06, 0x38,
   0xd4, 0xb3, 0x81, 0x78, 0x2b, 0x25, 0xac, 0xec, 0x64, 0xcc, 0x5f, 0x2a,
   0x5e, 0x04, 0x66, 0x13, 0xff, 0x5d, 0x7b, 0x11, 0x02, 0x71, 0xa1, 0xb7,
   0x56, 0x4a, 0x7e, 0x38, 0xc2, 0xe8, 0x96, 0x55, 0x18, 0x81, 0x67, 0x6f,
   0xa7
};

static const uint8_t buildbot_sha[CORE_SHA256_LEN] =
{
   0xb5, 0xa6, 0xed, 0x40, 0xed, 0xe7, 0x35, 0xea, 0x7f, 0x1d, 0x56, 0x91,
   0x6f, 0x4c, 0xee, 0xd8, 0xc2, 0xaa, 0xbe, 0x4d, 0x48, 0x1f, 0xe3, 0x38,
   0x3a, 0xe2, 0xb5, 0xe5, 0x48, 0x5b, 0x65, 0x45
};

static const uint8_t key1_id[CORE_SIG_KEY_ID_LEN] =
{
   0xd2, 0x89, 0x89, 0xb6, 0xaf, 0x40, 0x3a, 0x92
};

static const uint8_t key2_id[CORE_SIG_KEY_ID_LEN] =
{
   0x2e, 0x4d, 0xa2, 0x62, 0x81, 0x8f, 0x74, 0x88
};

#ifdef HAVE_CORE_SIGNING_TEST_KEY
static const uint8_t test_key_id[CORE_SIG_KEY_ID_LEN] =
{
   0xb1, 0x8b, 0x86, 0xce, 0x13, 0x89, 0xe4, 0x6d
};
#endif

/* For core_signature_io.c, which is linked in but not used here */
bool fill_pathname_application_data(char *s, size_t len)
{
   return false;
}

static void test_keys(void)
{
   uint8_t id[CORE_SIG_KEY_ID_LEN];
   uint8_t one[32];
   uint8_t shared[32];
   size_t i;
   size_t n         = 99;
   const uint8_t *k = core_sig_builtin_keys(&n);
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   CHECK(n == 3);
#else
   CHECK(n == 2);
#endif
   if (n < 2)
      return;
   core_sig_key_id(k, id);
   CHECK(!memcmp(id, key1_id, sizeof(id)));
   core_sig_key_id(k + CORE_SIG_KEY_LEN, id);
   CHECK(!memcmp(id, key2_id, sizeof(id)));
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   core_sig_key_id(k + (n - 1) * CORE_SIG_KEY_LEN, id);
   CHECK(!memcmp(id, test_key_id, sizeof(id)));
#endif
   /* Each key is a point on the curve: p256_ecdh accepts it */
   memset(one, 0, sizeof(one));
   one[sizeof(one) - 1] = 1;
   for (i = 0; i < 2; i++)
      CHECK(p256_ecdh(one, k + i * CORE_SIG_KEY_LEN, shared) == 0);
}

static void test_buildbot_sig(void)
{
   uint8_t sha[CORE_SHA256_LEN];
   struct core_check c;
   size_t n            = 0;
   const uint8_t *k    = core_sig_builtin_keys(&n);
   core_trust_t *t     = core_trust_new();
   core_trust_set_t *s = core_trust_get(t);

   memset(&c, 0, sizeof(c));
   strlcpy(c.rec.name, BUILDBOT_NAME, sizeof(c.rec.name));
   core_check_finish(s, k, n, buildbot_sha, buildbot_sig,
         sizeof(buildbot_sig), 0, &c);
   CHECK(c.result == CORE_CHECK_OK && c.record);
   CHECK(c.rec.build_time == BUILDBOT_TIME);
   if (n >= 2)
   {
      /* A key 1 signature: unknown to key 2 alone */
      core_check_finish(s, k + CORE_SIG_KEY_LEN, 1, buildbot_sha,
            buildbot_sig, sizeof(buildbot_sig), 0, &c);
      CHECK(c.result == CORE_CHECK_UNKNOWN_KEY && !c.record);
      memcpy(sha, buildbot_sha, sizeof(sha));
      sha[0] ^= 1;
      core_check_finish(s, k, n, sha, buildbot_sig, sizeof(buildbot_sig),
            0, &c);
      CHECK(c.result == CORE_CHECK_MODIFIED && !c.record);
      strlcpy(c.rec.name, BUILDBOT_NAME ".bak", sizeof(c.rec.name));
      core_check_finish(s, k, n, buildbot_sha, buildbot_sig,
            sizeof(buildbot_sig), 0, &c);
      CHECK(c.result == CORE_CHECK_MODIFIED && !c.record);
   }
   core_trust_set_put(s);
   core_trust_free(t);
}

int main(void)
{
   /* FAIL lines survive a crash or a sanitizer exit */
   setvbuf(stdout, NULL, _IONBF, 0);
   test_keys();
   test_buildbot_sig();
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
