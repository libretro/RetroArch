/* AES and GHASH on the instructions this build and CPU have (ARMv8
 * AES + PMULL, AES-NI + PCLMULQDQ) against the portable code, over
 * every mode the library has, and FIPS 197 / GCM vectors on both.
 * crypto.c is included whole, so the instruction set it asks about
 * can be withheld and its cached answer reset between runs.
 *
 *    aes_hw_test              every path there is
 *    aes_hw_test --require-hw fails when the instructions don't run:
 *                             the cross lanes, where they must */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>

static int withhold;
static uint64_t aes_test_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
   if (withhold)
      cpu &= ~(uint64_t)(RETRO_SIMD_AES | RETRO_SIMD_PCLMUL);
   return cpu;
}
#define cpu_features_get aes_test_cpu_features
#include "../../../crypto/crypto.c"
#undef cpu_features_get

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* Withheld or not, and the library's cached answer dropped */
static void use_hw(int hw)
{
   withhold = !hw;
#if defined(AES_HAVE_X86_PATH)
   retro_atomic_store_release_int(&aes_x86_ok, 0);
#endif
#if defined(AES_HAVE_ARM_PATH) && defined(AES_ARM_DISPATCH)
   retro_atomic_store_relaxed_int(&aes_arm_ok, -1);
#endif
}

static uint32_t rng = 0x2545F491u;
static void fill(uint8_t *p, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      rng ^= rng << 13;
      rng ^= rng >> 17;
      rng ^= rng << 5;
      p[i] = (uint8_t)rng;
   }
}

static int unhex(uint8_t *out, const char *s)
{
   int n = 0;
   while (s[0] && s[1])
   {
      unsigned v;
      sscanf(s, "%2x", &v);
      out[n++] = (uint8_t)v;
      s += 2;
   }
   return n;
}

static void check_vectors(void)
{
   uint8_t key[32], pt[64], ct[64], want[64], iv[12], tag[16], wtag[16];
   struct aes_ctx a;
   struct aes_gcm_ctx g;
   int n;
   /* FIPS 197 appendix C.1 and C.3 */
   unhex(key, "000102030405060708090a0b0c0d0e0f");
   unhex(pt,  "00112233445566778899aabbccddeeff");
   unhex(want, "69c4e0d86a7b0430d8cdb78070b4c55a");
   CHECK(aes_init(&a, key, 16) == 0);
   aes_encrypt_block(&a, pt, ct);
   CHECK(!memcmp(ct, want, 16));
   aes_decrypt_block(&a, ct, ct);
   CHECK(!memcmp(ct, pt, 16));
   unhex(key, "000102030405060708090a0b0c0d0e0f"
              "101112131415161718191a1b1c1d1e1f");
   unhex(want, "8ea2b7ca516745bfeafc49904b496089");
   CHECK(aes_init(&a, key, 32) == 0);
   aes_encrypt_block(&a, pt, ct);
   CHECK(!memcmp(ct, want, 16));
   /* GCM spec test case 4: AES-128, 60 bytes, 20 of AAD */
   unhex(key, "feffe9928665731c6d6a8f9467308308");
   unhex(iv,  "cafebabefacedbaddecaf888");
   n = unhex(pt, "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
                 "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39");
   unhex(want, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e"
               "21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091");
   unhex(wtag, "5bc94fbc3221a5db94fae95ae7121a47");
   {
      uint8_t aad[20];
      unhex(aad, "feedfacedeadbeeffeedfacedeadbeefabaddad2");
      CHECK(aes_gcm_init(&g, key, 16) == 0);
      CHECK(aes_gcm_encrypt(&g, iv, 12, aad, 20, pt, (size_t)n, ct, tag) == 0);
      CHECK(!memcmp(ct, want, (size_t)n));
      CHECK(!memcmp(tag, wtag, 16));
   }
}

/* One pass over every mode, its outputs appended to out */
static size_t run_modes(uint8_t *out, uint32_t seed)
{
   static const size_t key_lens[3] = { 16, 24, 32 };
   static const size_t lens[] = { 16, 32, 48, 112, 128, 144, 256, 400,
      1024, 4096 };
   size_t o = 0, k, l;
   rng = seed;
   for (k = 0; k < 3; k++)
      for (l = 0; l < sizeof(lens) / sizeof(lens[0]); l++)
      {
         uint8_t key[32], key2[32], iv[16], ctr[16], tw[16], nonce[12];
         uint8_t aad[37], tag[16];
         uint8_t *in  = (uint8_t*)malloc(lens[l]);
         uint8_t *buf = (uint8_t*)malloc(lens[l]);
         size_t n     = lens[l];
         struct aes_ctx a, b;
         struct aes_gcm_ctx g;
         unsigned num = 0;
         if (!in || !buf)
         {
            free(in);
            free(buf);
            return o;
         }
         fill(key, sizeof(key));
         fill(key2, sizeof(key2));
         fill(in, n);
         aes_init(&a, key, key_lens[k]);
         aes_init(&b, key2, key_lens[k]);
         aes_gcm_init(&g, key, key_lens[k]);

         aes_encrypt_block(&a, in, out + o); o += 16;
         aes_decrypt_block(&a, in, out + o); o += 16;
         fill(iv, 16);
         aes_cbc_encrypt(&a, iv, in, out + o, n); o += n;
         fill(iv, 16);
         aes_cbc_decrypt(&a, iv, in, out + o, n); o += n;
         fill(ctr, 16);
         aes_ctr_crypt(&a, ctr, in, out + o, n); o += n;
         fill(ctr, 16);
         aes_ctr128_crypt(&a, ctr, in, out + o, n - 3); o += n - 3;
         fill(iv, 16);
         aes_cfb128_encrypt(&a, iv, &num, in, out + o, n - 5); o += n - 5;
         fill(tw, 16);
         aes_xts_encrypt(&a, &b, tw, in, out + o, n); o += n;
         aes_xts_decrypt(&a, &b, tw, in, out + o, n); o += n;
         aes_cmac(&a, in, n - 1, out + o); o += 16;
         fill(nonce, 12);
         fill(aad, sizeof(aad));
         aes_gcm_encrypt(&g, nonce, 12, aad, sizeof(aad), in, n - 7,
               out + o, tag);
         o += n - 7;
         memcpy(out + o, tag, 16); o += 16;
         CHECK(aes_gcm_decrypt(&g, nonce, 12, aad, sizeof(aad),
               out + o - 16 - (n - 7), n - 7, tag, buf) == 0);
         CHECK(!memcmp(buf, in, n - 7));
         free(in);
         free(buf);
      }
   return o;
}

int main(int argc, char **argv)
{
   int require_hw = argc > 1 && !strcmp(argv[1], "--require-hw");
   int hw;
   size_t cap = 3 * 10 * (16 * 2 + 4096 * 8 + 64), na, nb;
   uint8_t *a = (uint8_t*)malloc(cap);
   uint8_t *b = (uint8_t*)malloc(cap);
   setvbuf(stdout, NULL, _IONBF, 0);
   (void)aes_test_cpu_features;
   printf("aes_hw_test:\n");
   if (!a || !b)
      return 1;
   use_hw(1);
   hw = crypto_aes_hw();
   printf("   instructions: %s\n", hw ? "yes" : "no");
   check_vectors();
   na = run_modes(a, 1u);
   use_hw(0);
   CHECK(!crypto_aes_hw());
   check_vectors();
   nb = run_modes(b, 1u);
   CHECK(na == nb && na <= cap);
   CHECK(!memcmp(a, b, na));
   printf("   %lu bytes over every mode: both paths agree\n",
         (unsigned long)na);
   if (require_hw)
      CHECK(hw);
   free(a);
   free(b);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
