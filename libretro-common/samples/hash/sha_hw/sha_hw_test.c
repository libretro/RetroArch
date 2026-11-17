/* The SHA-1 and SHA-256 instruction paths against the scalar rounds
 * and FIPS 180 vectors, on whichever paths this build and CPU have.
 * lrc_hash.c is included whole, so the block functions are called
 * directly, and the instruction set it asks about can be withheld to
 * run the public calls down the scalar path too.
 *
 *    sha_hw_test              every path there is
 *    sha_hw_test --require-hw fails when no instruction path runs:
 *                             the cross lanes, where one must */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>

/* What lrc_hash.c sees of the CPU: all of it, or none of the SHA bits */
static int withhold_sha;
static uint64_t sha_test_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
   if (withhold_sha)
      cpu &= ~(uint64_t)(RETRO_SIMD_SHA1 | RETRO_SIMD_SHA256);
   return cpu;
}
#define cpu_features_get sha_test_cpu_features
#include "../../../hash/lrc_hash.c"
#undef cpu_features_get

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t rng = 0x12345678u;
static uint32_t next_rand(void)
{
   rng ^= rng << 13;
   rng ^= rng >> 17;
   rng ^= rng << 5;
   return rng;
}

static void fill(uint8_t *p, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      p[i] = (uint8_t)next_rand();
}

static void hex(char *s, const uint8_t *d, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      sprintf(s + 2 * i, "%02x", d[i]);
}

/* FIPS 180-2 appendix vectors, and one long enough for many blocks */
static void check_vectors(const char *path)
{
   static const char two_block[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
   uint8_t *million = (uint8_t*)malloc(1000000);
   char s[65];
   uint8_t d[32];
   struct sha256_state st;
   if (!million)
   {
      CHECK(million != NULL);
      return;
   }
   memset(million, 'a', 1000000);
   sha256_hash(s, (const uint8_t*)"abc", 3);
   CHECK(!strcmp(s,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
   sha256_hash(s, (const uint8_t*)two_block, sizeof(two_block) - 1);
   CHECK(!strcmp(s,
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
   sha256_stream_init(&st, 0);
   sha256_stream_update(&st, million, 1000000);
   sha256_stream_final(&st, d);
   hex(s, d, 32);
   CHECK(!strcmp(s,
      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
   SHA1Digest((const uint8_t*)"abc", 3, d);
   hex(s, d, 20);
   CHECK(!strcmp(s, "a9993e364706816aba3e25717850c26c9cd0d89d"));
   SHA1Digest((const uint8_t*)two_block, sizeof(two_block) - 1, d);
   hex(s, d, 20);
   CHECK(!strcmp(s, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"));
   SHA1Digest(million, 1000000, d);
   hex(s, d, 20);
   CHECK(!strcmp(s, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"));
   free(million);
   printf("   vectors: %s\n", path);
}

/* The two paths' digests of the same bytes, for every length around
 * the block and padding boundaries, through the public calls */
static void check_paths_agree(void)
{
   uint8_t buf[700];
   size_t len;
   fill(buf, sizeof(buf));
   for (len = 0; len <= sizeof(buf); len++)
   {
      uint8_t a[32], b[32];
      struct sha256_state st;
      withhold_sha = 0;
      sha256_stream_init(&st, 0);
      sha256_stream_update(&st, buf, len);
      sha256_stream_final(&st, a);
      withhold_sha = 1;
      sha256_stream_init(&st, 0);
      sha256_stream_update(&st, buf, len);
      sha256_stream_final(&st, b);
      CHECK(!memcmp(a, b, 32));
      withhold_sha = 0;
      SHA1Digest(buf, len, a);
      withhold_sha = 1;
      SHA1Digest(buf, len, b);
      CHECK(!memcmp(a, b, 20));
   }
   withhold_sha = 0;
   printf("   lengths 0..%u: both paths agree\n", (unsigned)sizeof(buf));
}

#if defined(SHA_HAVE_ARM_PATH) || defined(SHA_HAVE_X86_PATH)
/* The instruction blocks against the scalar rounds from random states;
 * SHA1DigestMany's two-lane blocks against one lane at a time */
static unsigned check_blocks(void)
{
   unsigned i, ran = 0;
   uint8_t block[128];
   if (cpu_features_get() & RETRO_SIMD_SHA256)
   {
      ran++;
#ifdef SHA_NEED_SCALAR
      for (i = 0; i < 4096; i++)
      {
         struct sha256_state ref;
         uint32_t h[8];
         memset(&ref, 0, sizeof(ref));
         fill((uint8_t*)ref.h, sizeof(ref.h));
         memcpy(h, ref.h, sizeof(h));
         fill(block, 64);
         sha256_block_hw(h, block);
         sha256_block_scalar(&ref, block);
         CHECK(!memcmp(h, ref.h, sizeof(h)));
      }
#endif
   }
   if (cpu_features_get() & RETRO_SIMD_SHA1)
   {
      ran++;
      for (i = 0; i < 4096; i++)
      {
         uint32_t a[5], b[5], c[5], d[5];
         fill((uint8_t*)a, sizeof(a));
         fill((uint8_t*)c, sizeof(c));
         memcpy(b, a, sizeof(b));
         memcpy(d, c, sizeof(d));
         fill(block, 128);
         sha1_hw_blocks2(a, c, block, block + 64, 1);
#ifdef SHA_NEED_SCALAR
         sha1_scalar_blocks(b, block, 1);
         sha1_scalar_blocks(d, block + 64, 1);
#else
         sha1_hw_blocks(b, block, 1);
         sha1_hw_blocks(d, block + 64, 1);
#endif
         CHECK(!memcmp(a, b, sizeof(a)));
         CHECK(!memcmp(c, d, sizeof(c)));
      }
   }
   printf("   instruction blocks: %u of 2 digests on this CPU\n", ran);
   return ran;
}
#endif

int main(int argc, char **argv)
{
   unsigned hw = 0;
   int require_hw = argc > 1 && !strcmp(argv[1], "--require-hw");
   /* Called only where lrc_hash.c dispatches */
   (void)sha_test_cpu_features;
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("sha_hw_test:\n");
#if defined(SHA_HAVE_ARM_PATH) || defined(SHA_HAVE_X86_PATH)
   hw = check_blocks();
#endif
   withhold_sha = 0;
   check_vectors(hw ? "instructions" : "scalar");
#ifdef SHA_NEED_SCALAR
   withhold_sha = 1;
   check_vectors("scalar");
   withhold_sha = 0;
   check_paths_agree();
#endif
   if (require_hw)
      CHECK(hw == 2);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
