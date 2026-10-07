/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (crypto.c).
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

#include <string.h>
#include <crypto/crypto.h>
#include <retro_atomic.h>

/* AES-NI + PCLMULQDQ path for x86, chosen at run time. Same gating as
 * the SHA-NI path in lrc_hash.c: GCC / Clang with the target
 * attribute, so the instructions are confined to the functions that
 * carry them and the TU's own baseline is untouched. */
#if (defined(__x86_64__) || defined(__i386__)) && !defined(_MSC_VER) && !defined(AES_NO_X86)
#if defined(__has_attribute) && defined(__has_include)
#if __has_attribute(target) && __has_include(<immintrin.h>) \
   && (!defined(__SCE__) || (defined(__AES__) && defined(__PCLMUL__) && defined(__SSSE3__)))
#define AES_HAVE_X86_PATH 1
#endif
#endif
#endif

#if defined(AES_HAVE_X86_PATH)
#include <immintrin.h>
#include <features/features_cpu.h>
#define AES_TARGET_X86 __attribute__((target("aes,pclmul,ssse3")))
#endif

/* ARMv8 AES + PMULL, same shape as lrc_hash.c's SHA path: used
 * unconditionally when the baseline has the crypto extension, else
 * compiled in under the target attribute and chosen at run time on
 * toolchains whose arm_neon.h declares the intrinsics regardless of
 * the baseline (Clang 16+, GCC 9+). */
#if (defined(__aarch64__) || defined(_M_ARM64)) && !defined(_MSC_VER) && !defined(AES_NO_ARM)
#if defined(__ARM_FEATURE_CRYPTO) || (defined(__ARM_FEATURE_AES) && defined(__ARM_FEATURE_SHA2))
#define AES_HAVE_ARM_PATH 1
#elif (defined(__clang__) && __clang_major__ >= 16) \
   || (!defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 9)
#define AES_HAVE_ARM_PATH    1
#define AES_ARM_NEEDS_TARGET 1
#define AES_ARM_DISPATCH     1
#endif
#endif

#if defined(AES_HAVE_ARM_PATH)
#include <arm_neon.h>
#include <features/features_cpu.h>
#if defined(AES_ARM_NEEDS_TARGET)
#define AES_TARGET_ARM __attribute__((target("+crypto")))
#else
#define AES_TARGET_ARM
#endif
#endif

int crypto_memeq_ct(const void *a, const void *b, size_t len)
{
   const volatile uint8_t *pa = (const volatile uint8_t*)a;
   const volatile uint8_t *pb = (const volatile uint8_t*)b;
   uint8_t acc = 0;
   size_t i;

   for (i = 0; i < len; i++)
      acc |= (uint8_t)(pa[i] ^ pb[i]);

   /* 1 when every octet matched, with no data-dependent branch. */
   return (int)((((unsigned)acc - 1) >> 8) & 1);
}

void crypto_memzero(void *p, size_t len)
{
   volatile uint8_t *vp = (volatile uint8_t*)p;
   while (len--)
      *vp++ = 0;
}

static const uint8_t aes_sbox[256] = {
   0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
   0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
   0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
   0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
   0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
   0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
   0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
   0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
   0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
   0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
   0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
   0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
   0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
   0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
   0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
   0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t aes_rcon[10] = {
   0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

#define XTIME(x) ((uint8_t)(((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b)))

static uint32_t aes_sub_word(uint32_t w)
{
   return ((uint32_t)aes_sbox[(w >> 24) & 0xff] << 24)
        | ((uint32_t)aes_sbox[(w >> 16) & 0xff] << 16)
        | ((uint32_t)aes_sbox[(w >>  8) & 0xff] <<  8)
        |  (uint32_t)aes_sbox[ w        & 0xff];
}

/* Round tables: ft[0][x] = (2s, s, s, 3s) with s = sbox[x], the other
 * three are its byte rotations. The inverse S-box and imc[x] = (14x,
 * 9x, 13x, 11x) serve the inverse cipher, which runs in the standard
 * order on the encryption schedule (InvMixColumns on the state, so no
 * second schedule is kept). All three are constants, generated from
 * aes_sbox above: nothing to build on first use, so nothing for two
 * threads setting up keys at once to race on. */
static const uint32_t aes_ft[4][256] = {
  {
   0xc66363a5UL,0xf87c7c84UL,0xee777799UL,0xf67b7b8dUL,0xfff2f20dUL,0xd66b6bbdUL,
   0xde6f6fb1UL,0x91c5c554UL,0x60303050UL,0x02010103UL,0xce6767a9UL,0x562b2b7dUL,
   0xe7fefe19UL,0xb5d7d762UL,0x4dababe6UL,0xec76769aUL,0x8fcaca45UL,0x1f82829dUL,
   0x89c9c940UL,0xfa7d7d87UL,0xeffafa15UL,0xb25959ebUL,0x8e4747c9UL,0xfbf0f00bUL,
   0x41adadecUL,0xb3d4d467UL,0x5fa2a2fdUL,0x45afafeaUL,0x239c9cbfUL,0x53a4a4f7UL,
   0xe4727296UL,0x9bc0c05bUL,0x75b7b7c2UL,0xe1fdfd1cUL,0x3d9393aeUL,0x4c26266aUL,
   0x6c36365aUL,0x7e3f3f41UL,0xf5f7f702UL,0x83cccc4fUL,0x6834345cUL,0x51a5a5f4UL,
   0xd1e5e534UL,0xf9f1f108UL,0xe2717193UL,0xabd8d873UL,0x62313153UL,0x2a15153fUL,
   0x0804040cUL,0x95c7c752UL,0x46232365UL,0x9dc3c35eUL,0x30181828UL,0x379696a1UL,
   0x0a05050fUL,0x2f9a9ab5UL,0x0e070709UL,0x24121236UL,0x1b80809bUL,0xdfe2e23dUL,
   0xcdebeb26UL,0x4e272769UL,0x7fb2b2cdUL,0xea75759fUL,0x1209091bUL,0x1d83839eUL,
   0x582c2c74UL,0x341a1a2eUL,0x361b1b2dUL,0xdc6e6eb2UL,0xb45a5aeeUL,0x5ba0a0fbUL,
   0xa45252f6UL,0x763b3b4dUL,0xb7d6d661UL,0x7db3b3ceUL,0x5229297bUL,0xdde3e33eUL,
   0x5e2f2f71UL,0x13848497UL,0xa65353f5UL,0xb9d1d168UL,0x00000000UL,0xc1eded2cUL,
   0x40202060UL,0xe3fcfc1fUL,0x79b1b1c8UL,0xb65b5bedUL,0xd46a6abeUL,0x8dcbcb46UL,
   0x67bebed9UL,0x7239394bUL,0x944a4adeUL,0x984c4cd4UL,0xb05858e8UL,0x85cfcf4aUL,
   0xbbd0d06bUL,0xc5efef2aUL,0x4faaaae5UL,0xedfbfb16UL,0x864343c5UL,0x9a4d4dd7UL,
   0x66333355UL,0x11858594UL,0x8a4545cfUL,0xe9f9f910UL,0x04020206UL,0xfe7f7f81UL,
   0xa05050f0UL,0x783c3c44UL,0x259f9fbaUL,0x4ba8a8e3UL,0xa25151f3UL,0x5da3a3feUL,
   0x804040c0UL,0x058f8f8aUL,0x3f9292adUL,0x219d9dbcUL,0x70383848UL,0xf1f5f504UL,
   0x63bcbcdfUL,0x77b6b6c1UL,0xafdada75UL,0x42212163UL,0x20101030UL,0xe5ffff1aUL,
   0xfdf3f30eUL,0xbfd2d26dUL,0x81cdcd4cUL,0x180c0c14UL,0x26131335UL,0xc3ecec2fUL,
   0xbe5f5fe1UL,0x359797a2UL,0x884444ccUL,0x2e171739UL,0x93c4c457UL,0x55a7a7f2UL,
   0xfc7e7e82UL,0x7a3d3d47UL,0xc86464acUL,0xba5d5de7UL,0x3219192bUL,0xe6737395UL,
   0xc06060a0UL,0x19818198UL,0x9e4f4fd1UL,0xa3dcdc7fUL,0x44222266UL,0x542a2a7eUL,
   0x3b9090abUL,0x0b888883UL,0x8c4646caUL,0xc7eeee29UL,0x6bb8b8d3UL,0x2814143cUL,
   0xa7dede79UL,0xbc5e5ee2UL,0x160b0b1dUL,0xaddbdb76UL,0xdbe0e03bUL,0x64323256UL,
   0x743a3a4eUL,0x140a0a1eUL,0x924949dbUL,0x0c06060aUL,0x4824246cUL,0xb85c5ce4UL,
   0x9fc2c25dUL,0xbdd3d36eUL,0x43acacefUL,0xc46262a6UL,0x399191a8UL,0x319595a4UL,
   0xd3e4e437UL,0xf279798bUL,0xd5e7e732UL,0x8bc8c843UL,0x6e373759UL,0xda6d6db7UL,
   0x018d8d8cUL,0xb1d5d564UL,0x9c4e4ed2UL,0x49a9a9e0UL,0xd86c6cb4UL,0xac5656faUL,
   0xf3f4f407UL,0xcfeaea25UL,0xca6565afUL,0xf47a7a8eUL,0x47aeaee9UL,0x10080818UL,
   0x6fbabad5UL,0xf0787888UL,0x4a25256fUL,0x5c2e2e72UL,0x381c1c24UL,0x57a6a6f1UL,
   0x73b4b4c7UL,0x97c6c651UL,0xcbe8e823UL,0xa1dddd7cUL,0xe874749cUL,0x3e1f1f21UL,
   0x964b4bddUL,0x61bdbddcUL,0x0d8b8b86UL,0x0f8a8a85UL,0xe0707090UL,0x7c3e3e42UL,
   0x71b5b5c4UL,0xcc6666aaUL,0x904848d8UL,0x06030305UL,0xf7f6f601UL,0x1c0e0e12UL,
   0xc26161a3UL,0x6a35355fUL,0xae5757f9UL,0x69b9b9d0UL,0x17868691UL,0x99c1c158UL,
   0x3a1d1d27UL,0x279e9eb9UL,0xd9e1e138UL,0xebf8f813UL,0x2b9898b3UL,0x22111133UL,
   0xd26969bbUL,0xa9d9d970UL,0x078e8e89UL,0x339494a7UL,0x2d9b9bb6UL,0x3c1e1e22UL,
   0x15878792UL,0xc9e9e920UL,0x87cece49UL,0xaa5555ffUL,0x50282878UL,0xa5dfdf7aUL,
   0x038c8c8fUL,0x59a1a1f8UL,0x09898980UL,0x1a0d0d17UL,0x65bfbfdaUL,0xd7e6e631UL,
   0x844242c6UL,0xd06868b8UL,0x824141c3UL,0x299999b0UL,0x5a2d2d77UL,0x1e0f0f11UL,
   0x7bb0b0cbUL,0xa85454fcUL,0x6dbbbbd6UL,0x2c16163aUL
  },
  {
   0xa5c66363UL,0x84f87c7cUL,0x99ee7777UL,0x8df67b7bUL,0x0dfff2f2UL,0xbdd66b6bUL,
   0xb1de6f6fUL,0x5491c5c5UL,0x50603030UL,0x03020101UL,0xa9ce6767UL,0x7d562b2bUL,
   0x19e7fefeUL,0x62b5d7d7UL,0xe64dababUL,0x9aec7676UL,0x458fcacaUL,0x9d1f8282UL,
   0x4089c9c9UL,0x87fa7d7dUL,0x15effafaUL,0xebb25959UL,0xc98e4747UL,0x0bfbf0f0UL,
   0xec41adadUL,0x67b3d4d4UL,0xfd5fa2a2UL,0xea45afafUL,0xbf239c9cUL,0xf753a4a4UL,
   0x96e47272UL,0x5b9bc0c0UL,0xc275b7b7UL,0x1ce1fdfdUL,0xae3d9393UL,0x6a4c2626UL,
   0x5a6c3636UL,0x417e3f3fUL,0x02f5f7f7UL,0x4f83ccccUL,0x5c683434UL,0xf451a5a5UL,
   0x34d1e5e5UL,0x08f9f1f1UL,0x93e27171UL,0x73abd8d8UL,0x53623131UL,0x3f2a1515UL,
   0x0c080404UL,0x5295c7c7UL,0x65462323UL,0x5e9dc3c3UL,0x28301818UL,0xa1379696UL,
   0x0f0a0505UL,0xb52f9a9aUL,0x090e0707UL,0x36241212UL,0x9b1b8080UL,0x3ddfe2e2UL,
   0x26cdebebUL,0x694e2727UL,0xcd7fb2b2UL,0x9fea7575UL,0x1b120909UL,0x9e1d8383UL,
   0x74582c2cUL,0x2e341a1aUL,0x2d361b1bUL,0xb2dc6e6eUL,0xeeb45a5aUL,0xfb5ba0a0UL,
   0xf6a45252UL,0x4d763b3bUL,0x61b7d6d6UL,0xce7db3b3UL,0x7b522929UL,0x3edde3e3UL,
   0x715e2f2fUL,0x97138484UL,0xf5a65353UL,0x68b9d1d1UL,0x00000000UL,0x2cc1ededUL,
   0x60402020UL,0x1fe3fcfcUL,0xc879b1b1UL,0xedb65b5bUL,0xbed46a6aUL,0x468dcbcbUL,
   0xd967bebeUL,0x4b723939UL,0xde944a4aUL,0xd4984c4cUL,0xe8b05858UL,0x4a85cfcfUL,
   0x6bbbd0d0UL,0x2ac5efefUL,0xe54faaaaUL,0x16edfbfbUL,0xc5864343UL,0xd79a4d4dUL,
   0x55663333UL,0x94118585UL,0xcf8a4545UL,0x10e9f9f9UL,0x06040202UL,0x81fe7f7fUL,
   0xf0a05050UL,0x44783c3cUL,0xba259f9fUL,0xe34ba8a8UL,0xf3a25151UL,0xfe5da3a3UL,
   0xc0804040UL,0x8a058f8fUL,0xad3f9292UL,0xbc219d9dUL,0x48703838UL,0x04f1f5f5UL,
   0xdf63bcbcUL,0xc177b6b6UL,0x75afdadaUL,0x63422121UL,0x30201010UL,0x1ae5ffffUL,
   0x0efdf3f3UL,0x6dbfd2d2UL,0x4c81cdcdUL,0x14180c0cUL,0x35261313UL,0x2fc3ececUL,
   0xe1be5f5fUL,0xa2359797UL,0xcc884444UL,0x392e1717UL,0x5793c4c4UL,0xf255a7a7UL,
   0x82fc7e7eUL,0x477a3d3dUL,0xacc86464UL,0xe7ba5d5dUL,0x2b321919UL,0x95e67373UL,
   0xa0c06060UL,0x98198181UL,0xd19e4f4fUL,0x7fa3dcdcUL,0x66442222UL,0x7e542a2aUL,
   0xab3b9090UL,0x830b8888UL,0xca8c4646UL,0x29c7eeeeUL,0xd36bb8b8UL,0x3c281414UL,
   0x79a7dedeUL,0xe2bc5e5eUL,0x1d160b0bUL,0x76addbdbUL,0x3bdbe0e0UL,0x56643232UL,
   0x4e743a3aUL,0x1e140a0aUL,0xdb924949UL,0x0a0c0606UL,0x6c482424UL,0xe4b85c5cUL,
   0x5d9fc2c2UL,0x6ebdd3d3UL,0xef43acacUL,0xa6c46262UL,0xa8399191UL,0xa4319595UL,
   0x37d3e4e4UL,0x8bf27979UL,0x32d5e7e7UL,0x438bc8c8UL,0x596e3737UL,0xb7da6d6dUL,
   0x8c018d8dUL,0x64b1d5d5UL,0xd29c4e4eUL,0xe049a9a9UL,0xb4d86c6cUL,0xfaac5656UL,
   0x07f3f4f4UL,0x25cfeaeaUL,0xafca6565UL,0x8ef47a7aUL,0xe947aeaeUL,0x18100808UL,
   0xd56fbabaUL,0x88f07878UL,0x6f4a2525UL,0x725c2e2eUL,0x24381c1cUL,0xf157a6a6UL,
   0xc773b4b4UL,0x5197c6c6UL,0x23cbe8e8UL,0x7ca1ddddUL,0x9ce87474UL,0x213e1f1fUL,
   0xdd964b4bUL,0xdc61bdbdUL,0x860d8b8bUL,0x850f8a8aUL,0x90e07070UL,0x427c3e3eUL,
   0xc471b5b5UL,0xaacc6666UL,0xd8904848UL,0x05060303UL,0x01f7f6f6UL,0x121c0e0eUL,
   0xa3c26161UL,0x5f6a3535UL,0xf9ae5757UL,0xd069b9b9UL,0x91178686UL,0x5899c1c1UL,
   0x273a1d1dUL,0xb9279e9eUL,0x38d9e1e1UL,0x13ebf8f8UL,0xb32b9898UL,0x33221111UL,
   0xbbd26969UL,0x70a9d9d9UL,0x89078e8eUL,0xa7339494UL,0xb62d9b9bUL,0x223c1e1eUL,
   0x92158787UL,0x20c9e9e9UL,0x4987ceceUL,0xffaa5555UL,0x78502828UL,0x7aa5dfdfUL,
   0x8f038c8cUL,0xf859a1a1UL,0x80098989UL,0x171a0d0dUL,0xda65bfbfUL,0x31d7e6e6UL,
   0xc6844242UL,0xb8d06868UL,0xc3824141UL,0xb0299999UL,0x775a2d2dUL,0x111e0f0fUL,
   0xcb7bb0b0UL,0xfca85454UL,0xd66dbbbbUL,0x3a2c1616UL
  },
  {
   0x63a5c663UL,0x7c84f87cUL,0x7799ee77UL,0x7b8df67bUL,0xf20dfff2UL,0x6bbdd66bUL,
   0x6fb1de6fUL,0xc55491c5UL,0x30506030UL,0x01030201UL,0x67a9ce67UL,0x2b7d562bUL,
   0xfe19e7feUL,0xd762b5d7UL,0xabe64dabUL,0x769aec76UL,0xca458fcaUL,0x829d1f82UL,
   0xc94089c9UL,0x7d87fa7dUL,0xfa15effaUL,0x59ebb259UL,0x47c98e47UL,0xf00bfbf0UL,
   0xadec41adUL,0xd467b3d4UL,0xa2fd5fa2UL,0xafea45afUL,0x9cbf239cUL,0xa4f753a4UL,
   0x7296e472UL,0xc05b9bc0UL,0xb7c275b7UL,0xfd1ce1fdUL,0x93ae3d93UL,0x266a4c26UL,
   0x365a6c36UL,0x3f417e3fUL,0xf702f5f7UL,0xcc4f83ccUL,0x345c6834UL,0xa5f451a5UL,
   0xe534d1e5UL,0xf108f9f1UL,0x7193e271UL,0xd873abd8UL,0x31536231UL,0x153f2a15UL,
   0x040c0804UL,0xc75295c7UL,0x23654623UL,0xc35e9dc3UL,0x18283018UL,0x96a13796UL,
   0x050f0a05UL,0x9ab52f9aUL,0x07090e07UL,0x12362412UL,0x809b1b80UL,0xe23ddfe2UL,
   0xeb26cdebUL,0x27694e27UL,0xb2cd7fb2UL,0x759fea75UL,0x091b1209UL,0x839e1d83UL,
   0x2c74582cUL,0x1a2e341aUL,0x1b2d361bUL,0x6eb2dc6eUL,0x5aeeb45aUL,0xa0fb5ba0UL,
   0x52f6a452UL,0x3b4d763bUL,0xd661b7d6UL,0xb3ce7db3UL,0x297b5229UL,0xe33edde3UL,
   0x2f715e2fUL,0x84971384UL,0x53f5a653UL,0xd168b9d1UL,0x00000000UL,0xed2cc1edUL,
   0x20604020UL,0xfc1fe3fcUL,0xb1c879b1UL,0x5bedb65bUL,0x6abed46aUL,0xcb468dcbUL,
   0xbed967beUL,0x394b7239UL,0x4ade944aUL,0x4cd4984cUL,0x58e8b058UL,0xcf4a85cfUL,
   0xd06bbbd0UL,0xef2ac5efUL,0xaae54faaUL,0xfb16edfbUL,0x43c58643UL,0x4dd79a4dUL,
   0x33556633UL,0x85941185UL,0x45cf8a45UL,0xf910e9f9UL,0x02060402UL,0x7f81fe7fUL,
   0x50f0a050UL,0x3c44783cUL,0x9fba259fUL,0xa8e34ba8UL,0x51f3a251UL,0xa3fe5da3UL,
   0x40c08040UL,0x8f8a058fUL,0x92ad3f92UL,0x9dbc219dUL,0x38487038UL,0xf504f1f5UL,
   0xbcdf63bcUL,0xb6c177b6UL,0xda75afdaUL,0x21634221UL,0x10302010UL,0xff1ae5ffUL,
   0xf30efdf3UL,0xd26dbfd2UL,0xcd4c81cdUL,0x0c14180cUL,0x13352613UL,0xec2fc3ecUL,
   0x5fe1be5fUL,0x97a23597UL,0x44cc8844UL,0x17392e17UL,0xc45793c4UL,0xa7f255a7UL,
   0x7e82fc7eUL,0x3d477a3dUL,0x64acc864UL,0x5de7ba5dUL,0x192b3219UL,0x7395e673UL,
   0x60a0c060UL,0x81981981UL,0x4fd19e4fUL,0xdc7fa3dcUL,0x22664422UL,0x2a7e542aUL,
   0x90ab3b90UL,0x88830b88UL,0x46ca8c46UL,0xee29c7eeUL,0xb8d36bb8UL,0x143c2814UL,
   0xde79a7deUL,0x5ee2bc5eUL,0x0b1d160bUL,0xdb76addbUL,0xe03bdbe0UL,0x32566432UL,
   0x3a4e743aUL,0x0a1e140aUL,0x49db9249UL,0x060a0c06UL,0x246c4824UL,0x5ce4b85cUL,
   0xc25d9fc2UL,0xd36ebdd3UL,0xacef43acUL,0x62a6c462UL,0x91a83991UL,0x95a43195UL,
   0xe437d3e4UL,0x798bf279UL,0xe732d5e7UL,0xc8438bc8UL,0x37596e37UL,0x6db7da6dUL,
   0x8d8c018dUL,0xd564b1d5UL,0x4ed29c4eUL,0xa9e049a9UL,0x6cb4d86cUL,0x56faac56UL,
   0xf407f3f4UL,0xea25cfeaUL,0x65afca65UL,0x7a8ef47aUL,0xaee947aeUL,0x08181008UL,
   0xbad56fbaUL,0x7888f078UL,0x256f4a25UL,0x2e725c2eUL,0x1c24381cUL,0xa6f157a6UL,
   0xb4c773b4UL,0xc65197c6UL,0xe823cbe8UL,0xdd7ca1ddUL,0x749ce874UL,0x1f213e1fUL,
   0x4bdd964bUL,0xbddc61bdUL,0x8b860d8bUL,0x8a850f8aUL,0x7090e070UL,0x3e427c3eUL,
   0xb5c471b5UL,0x66aacc66UL,0x48d89048UL,0x03050603UL,0xf601f7f6UL,0x0e121c0eUL,
   0x61a3c261UL,0x355f6a35UL,0x57f9ae57UL,0xb9d069b9UL,0x86911786UL,0xc15899c1UL,
   0x1d273a1dUL,0x9eb9279eUL,0xe138d9e1UL,0xf813ebf8UL,0x98b32b98UL,0x11332211UL,
   0x69bbd269UL,0xd970a9d9UL,0x8e89078eUL,0x94a73394UL,0x9bb62d9bUL,0x1e223c1eUL,
   0x87921587UL,0xe920c9e9UL,0xce4987ceUL,0x55ffaa55UL,0x28785028UL,0xdf7aa5dfUL,
   0x8c8f038cUL,0xa1f859a1UL,0x89800989UL,0x0d171a0dUL,0xbfda65bfUL,0xe631d7e6UL,
   0x42c68442UL,0x68b8d068UL,0x41c38241UL,0x99b02999UL,0x2d775a2dUL,0x0f111e0fUL,
   0xb0cb7bb0UL,0x54fca854UL,0xbbd66dbbUL,0x163a2c16UL
  },
  {
   0x6363a5c6UL,0x7c7c84f8UL,0x777799eeUL,0x7b7b8df6UL,0xf2f20dffUL,0x6b6bbdd6UL,
   0x6f6fb1deUL,0xc5c55491UL,0x30305060UL,0x01010302UL,0x6767a9ceUL,0x2b2b7d56UL,
   0xfefe19e7UL,0xd7d762b5UL,0xababe64dUL,0x76769aecUL,0xcaca458fUL,0x82829d1fUL,
   0xc9c94089UL,0x7d7d87faUL,0xfafa15efUL,0x5959ebb2UL,0x4747c98eUL,0xf0f00bfbUL,
   0xadadec41UL,0xd4d467b3UL,0xa2a2fd5fUL,0xafafea45UL,0x9c9cbf23UL,0xa4a4f753UL,
   0x727296e4UL,0xc0c05b9bUL,0xb7b7c275UL,0xfdfd1ce1UL,0x9393ae3dUL,0x26266a4cUL,
   0x36365a6cUL,0x3f3f417eUL,0xf7f702f5UL,0xcccc4f83UL,0x34345c68UL,0xa5a5f451UL,
   0xe5e534d1UL,0xf1f108f9UL,0x717193e2UL,0xd8d873abUL,0x31315362UL,0x15153f2aUL,
   0x04040c08UL,0xc7c75295UL,0x23236546UL,0xc3c35e9dUL,0x18182830UL,0x9696a137UL,
   0x05050f0aUL,0x9a9ab52fUL,0x0707090eUL,0x12123624UL,0x80809b1bUL,0xe2e23ddfUL,
   0xebeb26cdUL,0x2727694eUL,0xb2b2cd7fUL,0x75759feaUL,0x09091b12UL,0x83839e1dUL,
   0x2c2c7458UL,0x1a1a2e34UL,0x1b1b2d36UL,0x6e6eb2dcUL,0x5a5aeeb4UL,0xa0a0fb5bUL,
   0x5252f6a4UL,0x3b3b4d76UL,0xd6d661b7UL,0xb3b3ce7dUL,0x29297b52UL,0xe3e33eddUL,
   0x2f2f715eUL,0x84849713UL,0x5353f5a6UL,0xd1d168b9UL,0x00000000UL,0xeded2cc1UL,
   0x20206040UL,0xfcfc1fe3UL,0xb1b1c879UL,0x5b5bedb6UL,0x6a6abed4UL,0xcbcb468dUL,
   0xbebed967UL,0x39394b72UL,0x4a4ade94UL,0x4c4cd498UL,0x5858e8b0UL,0xcfcf4a85UL,
   0xd0d06bbbUL,0xefef2ac5UL,0xaaaae54fUL,0xfbfb16edUL,0x4343c586UL,0x4d4dd79aUL,
   0x33335566UL,0x85859411UL,0x4545cf8aUL,0xf9f910e9UL,0x02020604UL,0x7f7f81feUL,
   0x5050f0a0UL,0x3c3c4478UL,0x9f9fba25UL,0xa8a8e34bUL,0x5151f3a2UL,0xa3a3fe5dUL,
   0x4040c080UL,0x8f8f8a05UL,0x9292ad3fUL,0x9d9dbc21UL,0x38384870UL,0xf5f504f1UL,
   0xbcbcdf63UL,0xb6b6c177UL,0xdada75afUL,0x21216342UL,0x10103020UL,0xffff1ae5UL,
   0xf3f30efdUL,0xd2d26dbfUL,0xcdcd4c81UL,0x0c0c1418UL,0x13133526UL,0xecec2fc3UL,
   0x5f5fe1beUL,0x9797a235UL,0x4444cc88UL,0x1717392eUL,0xc4c45793UL,0xa7a7f255UL,
   0x7e7e82fcUL,0x3d3d477aUL,0x6464acc8UL,0x5d5de7baUL,0x19192b32UL,0x737395e6UL,
   0x6060a0c0UL,0x81819819UL,0x4f4fd19eUL,0xdcdc7fa3UL,0x22226644UL,0x2a2a7e54UL,
   0x9090ab3bUL,0x8888830bUL,0x4646ca8cUL,0xeeee29c7UL,0xb8b8d36bUL,0x14143c28UL,
   0xdede79a7UL,0x5e5ee2bcUL,0x0b0b1d16UL,0xdbdb76adUL,0xe0e03bdbUL,0x32325664UL,
   0x3a3a4e74UL,0x0a0a1e14UL,0x4949db92UL,0x06060a0cUL,0x24246c48UL,0x5c5ce4b8UL,
   0xc2c25d9fUL,0xd3d36ebdUL,0xacacef43UL,0x6262a6c4UL,0x9191a839UL,0x9595a431UL,
   0xe4e437d3UL,0x79798bf2UL,0xe7e732d5UL,0xc8c8438bUL,0x3737596eUL,0x6d6db7daUL,
   0x8d8d8c01UL,0xd5d564b1UL,0x4e4ed29cUL,0xa9a9e049UL,0x6c6cb4d8UL,0x5656faacUL,
   0xf4f407f3UL,0xeaea25cfUL,0x6565afcaUL,0x7a7a8ef4UL,0xaeaee947UL,0x08081810UL,
   0xbabad56fUL,0x787888f0UL,0x25256f4aUL,0x2e2e725cUL,0x1c1c2438UL,0xa6a6f157UL,
   0xb4b4c773UL,0xc6c65197UL,0xe8e823cbUL,0xdddd7ca1UL,0x74749ce8UL,0x1f1f213eUL,
   0x4b4bdd96UL,0xbdbddc61UL,0x8b8b860dUL,0x8a8a850fUL,0x707090e0UL,0x3e3e427cUL,
   0xb5b5c471UL,0x6666aaccUL,0x4848d890UL,0x03030506UL,0xf6f601f7UL,0x0e0e121cUL,
   0x6161a3c2UL,0x35355f6aUL,0x5757f9aeUL,0xb9b9d069UL,0x86869117UL,0xc1c15899UL,
   0x1d1d273aUL,0x9e9eb927UL,0xe1e138d9UL,0xf8f813ebUL,0x9898b32bUL,0x11113322UL,
   0x6969bbd2UL,0xd9d970a9UL,0x8e8e8907UL,0x9494a733UL,0x9b9bb62dUL,0x1e1e223cUL,
   0x87879215UL,0xe9e920c9UL,0xcece4987UL,0x5555ffaaUL,0x28287850UL,0xdfdf7aa5UL,
   0x8c8c8f03UL,0xa1a1f859UL,0x89898009UL,0x0d0d171aUL,0xbfbfda65UL,0xe6e631d7UL,
   0x4242c684UL,0x6868b8d0UL,0x4141c382UL,0x9999b029UL,0x2d2d775aUL,0x0f0f111eUL,
   0xb0b0cb7bUL,0x5454fca8UL,0xbbbbd66dUL,0x16163a2cUL
  }
};

static const uint8_t aes_isbox[256] = {
   0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
   0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
   0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
   0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
   0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
   0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
   0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
   0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
   0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
   0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
   0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
   0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
   0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
   0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
   0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
   0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static const uint32_t aes_imc[256] = {
   0x00000000UL,0x0e090d0bUL,0x1c121a16UL,0x121b171dUL,0x3824342cUL,0x362d3927UL,
   0x24362e3aUL,0x2a3f2331UL,0x70486858UL,0x7e416553UL,0x6c5a724eUL,0x62537f45UL,
   0x486c5c74UL,0x4665517fUL,0x547e4662UL,0x5a774b69UL,0xe090d0b0UL,0xee99ddbbUL,
   0xfc82caa6UL,0xf28bc7adUL,0xd8b4e49cUL,0xd6bde997UL,0xc4a6fe8aUL,0xcaaff381UL,
   0x90d8b8e8UL,0x9ed1b5e3UL,0x8ccaa2feUL,0x82c3aff5UL,0xa8fc8cc4UL,0xa6f581cfUL,
   0xb4ee96d2UL,0xbae79bd9UL,0xdb3bbb7bUL,0xd532b670UL,0xc729a16dUL,0xc920ac66UL,
   0xe31f8f57UL,0xed16825cUL,0xff0d9541UL,0xf104984aUL,0xab73d323UL,0xa57ade28UL,
   0xb761c935UL,0xb968c43eUL,0x9357e70fUL,0x9d5eea04UL,0x8f45fd19UL,0x814cf012UL,
   0x3bab6bcbUL,0x35a266c0UL,0x27b971ddUL,0x29b07cd6UL,0x038f5fe7UL,0x0d8652ecUL,
   0x1f9d45f1UL,0x119448faUL,0x4be30393UL,0x45ea0e98UL,0x57f11985UL,0x59f8148eUL,
   0x73c737bfUL,0x7dce3ab4UL,0x6fd52da9UL,0x61dc20a2UL,0xad766df6UL,0xa37f60fdUL,
   0xb16477e0UL,0xbf6d7aebUL,0x955259daUL,0x9b5b54d1UL,0x894043ccUL,0x87494ec7UL,
   0xdd3e05aeUL,0xd33708a5UL,0xc12c1fb8UL,0xcf2512b3UL,0xe51a3182UL,0xeb133c89UL,
   0xf9082b94UL,0xf701269fUL,0x4de6bd46UL,0x43efb04dUL,0x51f4a750UL,0x5ffdaa5bUL,
   0x75c2896aUL,0x7bcb8461UL,0x69d0937cUL,0x67d99e77UL,0x3daed51eUL,0x33a7d815UL,
   0x21bccf08UL,0x2fb5c203UL,0x058ae132UL,0x0b83ec39UL,0x1998fb24UL,0x1791f62fUL,
   0x764dd68dUL,0x7844db86UL,0x6a5fcc9bUL,0x6456c190UL,0x4e69e2a1UL,0x4060efaaUL,
   0x527bf8b7UL,0x5c72f5bcUL,0x0605bed5UL,0x080cb3deUL,0x1a17a4c3UL,0x141ea9c8UL,
   0x3e218af9UL,0x302887f2UL,0x223390efUL,0x2c3a9de4UL,0x96dd063dUL,0x98d40b36UL,
   0x8acf1c2bUL,0x84c61120UL,0xaef93211UL,0xa0f03f1aUL,0xb2eb2807UL,0xbce2250cUL,
   0xe6956e65UL,0xe89c636eUL,0xfa877473UL,0xf48e7978UL,0xdeb15a49UL,0xd0b85742UL,
   0xc2a3405fUL,0xccaa4d54UL,0x41ecdaf7UL,0x4fe5d7fcUL,0x5dfec0e1UL,0x53f7cdeaUL,
   0x79c8eedbUL,0x77c1e3d0UL,0x65daf4cdUL,0x6bd3f9c6UL,0x31a4b2afUL,0x3fadbfa4UL,
   0x2db6a8b9UL,0x23bfa5b2UL,0x09808683UL,0x07898b88UL,0x15929c95UL,0x1b9b919eUL,
   0xa17c0a47UL,0xaf75074cUL,0xbd6e1051UL,0xb3671d5aUL,0x99583e6bUL,0x97513360UL,
   0x854a247dUL,0x8b432976UL,0xd134621fUL,0xdf3d6f14UL,0xcd267809UL,0xc32f7502UL,
   0xe9105633UL,0xe7195b38UL,0xf5024c25UL,0xfb0b412eUL,0x9ad7618cUL,0x94de6c87UL,
   0x86c57b9aUL,0x88cc7691UL,0xa2f355a0UL,0xacfa58abUL,0xbee14fb6UL,0xb0e842bdUL,
   0xea9f09d4UL,0xe49604dfUL,0xf68d13c2UL,0xf8841ec9UL,0xd2bb3df8UL,0xdcb230f3UL,
   0xcea927eeUL,0xc0a02ae5UL,0x7a47b13cUL,0x744ebc37UL,0x6655ab2aUL,0x685ca621UL,
   0x42638510UL,0x4c6a881bUL,0x5e719f06UL,0x5078920dUL,0x0a0fd964UL,0x0406d46fUL,
   0x161dc372UL,0x1814ce79UL,0x322bed48UL,0x3c22e043UL,0x2e39f75eUL,0x2030fa55UL,
   0xec9ab701UL,0xe293ba0aUL,0xf088ad17UL,0xfe81a01cUL,0xd4be832dUL,0xdab78e26UL,
   0xc8ac993bUL,0xc6a59430UL,0x9cd2df59UL,0x92dbd252UL,0x80c0c54fUL,0x8ec9c844UL,
   0xa4f6eb75UL,0xaaffe67eUL,0xb8e4f163UL,0xb6edfc68UL,0x0c0a67b1UL,0x02036abaUL,
   0x10187da7UL,0x1e1170acUL,0x342e539dUL,0x3a275e96UL,0x283c498bUL,0x26354480UL,
   0x7c420fe9UL,0x724b02e2UL,0x605015ffUL,0x6e5918f4UL,0x44663bc5UL,0x4a6f36ceUL,
   0x587421d3UL,0x567d2cd8UL,0x37a10c7aUL,0x39a80171UL,0x2bb3166cUL,0x25ba1b67UL,
   0x0f853856UL,0x018c355dUL,0x13972240UL,0x1d9e2f4bUL,0x47e96422UL,0x49e06929UL,
   0x5bfb7e34UL,0x55f2733fUL,0x7fcd500eUL,0x71c45d05UL,0x63df4a18UL,0x6dd64713UL,
   0xd731dccaUL,0xd938d1c1UL,0xcb23c6dcUL,0xc52acbd7UL,0xef15e8e6UL,0xe11ce5edUL,
   0xf307f2f0UL,0xfd0efffbUL,0xa779b492UL,0xa970b999UL,0xbb6bae84UL,0xb562a38fUL,
   0x9f5d80beUL,0x91548db5UL,0x834f9aa8UL,0x8d4697a3UL
};

#if defined(AES_HAVE_X86_PATH)
/* 0 not checked yet, 1 no, 2 yes. Threads that check at the same time
 * all store the same answer; atomic, so that is not a data race. */
static retro_atomic_int_t aes_x86_ok;

static int aes_x86_available(void)
{
   int v = retro_atomic_load_acquire_int(&aes_x86_ok);
   if (!v)
   {
      uint64_t f = cpu_features_get();
      v          = (f & RETRO_SIMD_AES) && (f & RETRO_SIMD_PCLMUL) ? 2 : 1;
      retro_atomic_store_release_int(&aes_x86_ok, v);
   }
   return v == 2;
}

AES_TARGET_X86
static __m128i aes_x86_bswap32(__m128i v)
{
   const __m128i m = _mm_set_epi8(12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
   return _mm_shuffle_epi8(v, m);
}

AES_TARGET_X86
static __m128i aes_x86_encrypt(const struct aes_ctx *ctx, __m128i b)
{
   unsigned r;
   b = _mm_xor_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)ctx->rk)));
   for (r = 1; r < ctx->rounds; r++)
      b = _mm_aesenc_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
   return _mm_aesenclast_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
}

AES_TARGET_X86
static void aes_x86_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   _mm_storeu_si128((__m128i*)out,
         aes_x86_encrypt(ctx, _mm_loadu_si128((const __m128i*)in)));
}

/* Inverse cipher on the encryption schedule: aesdec wants the round
 * keys through InvMixColumns, applied here per block, as decryption
 * only serves the short Kerberos messages and keeps the context and
 * every other caller unchanged. */
AES_TARGET_X86
static void aes_x86_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   unsigned r = ctx->rounds;
   __m128i b  = _mm_loadu_si128((const __m128i*)in);
   b = _mm_xor_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r))));
   for (r--; r > 0; r--)
      b = _mm_aesdec_si128(b, _mm_aesimc_si128(
               aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r)))));
   b = _mm_aesdeclast_si128(b, aes_x86_bswap32(_mm_loadu_si128((const __m128i*)ctx->rk)));
   _mm_storeu_si128((__m128i*)out, b);
}

/* CTR with eight blocks in flight so the AES unit's latency is hidden,
 * the round keys loaded and byte-swapped once per call rather than once
 * per round. The counter block is kept byte-reversed, where its 32-bit
 * big-endian counter is the low lane: one add steps it (mod 2^32, as
 * GCM's inc32) and one shuffle turns it back into a block. */
AES_TARGET_X86
static void aes_x86_ctr(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   const __m128i rev  = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
   const __m128i one  = _mm_set_epi32(0, 0, 0, 1);
   const __m128i two  = _mm_set_epi32(0, 0, 0, 2);
   const __m128i four = _mm_set_epi32(0, 0, 0, 4);
   const __m128i eight = _mm_set_epi32(0, 0, 0, 8);
   __m128i  ctr = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)counter), rev);
   __m128i  rk[15];
   unsigned r, nr = ctx->rounds;

   for (r = 0; r <= nr; r++)
      rk[r] = aes_x86_bswap32(_mm_loadu_si128((const __m128i*)(ctx->rk + 4 * r)));

   while (len >= 128)
   {
      __m128i c1 = _mm_add_epi32(ctr, one);
      __m128i c2 = _mm_add_epi32(ctr, two);
      __m128i c3 = _mm_add_epi32(c1, two);
      __m128i c4 = _mm_add_epi32(ctr, four);
      __m128i c5 = _mm_add_epi32(c1, four);
      __m128i c6 = _mm_add_epi32(c2, four);
      __m128i c7 = _mm_add_epi32(c3, four);
      __m128i b0 = _mm_xor_si128(_mm_shuffle_epi8(ctr, rev), rk[0]);
      __m128i b1 = _mm_xor_si128(_mm_shuffle_epi8(c1, rev), rk[0]);
      __m128i b2 = _mm_xor_si128(_mm_shuffle_epi8(c2, rev), rk[0]);
      __m128i b3 = _mm_xor_si128(_mm_shuffle_epi8(c3, rev), rk[0]);
      __m128i b4 = _mm_xor_si128(_mm_shuffle_epi8(c4, rev), rk[0]);
      __m128i b5 = _mm_xor_si128(_mm_shuffle_epi8(c5, rev), rk[0]);
      __m128i b6 = _mm_xor_si128(_mm_shuffle_epi8(c6, rev), rk[0]);
      __m128i b7 = _mm_xor_si128(_mm_shuffle_epi8(c7, rev), rk[0]);
      for (r = 1; r < nr; r++)
      {
         __m128i k = rk[r];
         b0 = _mm_aesenc_si128(b0, k); b1 = _mm_aesenc_si128(b1, k);
         b2 = _mm_aesenc_si128(b2, k); b3 = _mm_aesenc_si128(b3, k);
         b4 = _mm_aesenc_si128(b4, k); b5 = _mm_aesenc_si128(b5, k);
         b6 = _mm_aesenc_si128(b6, k); b7 = _mm_aesenc_si128(b7, k);
      }
      _mm_storeu_si128((__m128i*)(out),       _mm_xor_si128(_mm_aesenclast_si128(b0, rk[nr]), _mm_loadu_si128((const __m128i*)(in))));
      _mm_storeu_si128((__m128i*)(out +  16), _mm_xor_si128(_mm_aesenclast_si128(b1, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  16))));
      _mm_storeu_si128((__m128i*)(out +  32), _mm_xor_si128(_mm_aesenclast_si128(b2, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  32))));
      _mm_storeu_si128((__m128i*)(out +  48), _mm_xor_si128(_mm_aesenclast_si128(b3, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  48))));
      _mm_storeu_si128((__m128i*)(out +  64), _mm_xor_si128(_mm_aesenclast_si128(b4, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  64))));
      _mm_storeu_si128((__m128i*)(out +  80), _mm_xor_si128(_mm_aesenclast_si128(b5, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  80))));
      _mm_storeu_si128((__m128i*)(out +  96), _mm_xor_si128(_mm_aesenclast_si128(b6, rk[nr]), _mm_loadu_si128((const __m128i*)(in +  96))));
      _mm_storeu_si128((__m128i*)(out + 112), _mm_xor_si128(_mm_aesenclast_si128(b7, rk[nr]), _mm_loadu_si128((const __m128i*)(in + 112))));
      ctr  = _mm_add_epi32(ctr, eight);
      in  += 128;
      out += 128;
      len -= 128;
   }
   while (len)
   {
      size_t  n = (len < 16) ? len : 16;
      size_t  i;
      uint8_t ks[16];
      __m128i x = _mm_xor_si128(_mm_shuffle_epi8(ctr, rev), rk[0]);
      for (r = 1; r < nr; r++)
         x = _mm_aesenc_si128(x, rk[r]);
      x = _mm_aesenclast_si128(x, rk[nr]);
      if (n == 16)
         _mm_storeu_si128((__m128i*)out, _mm_xor_si128(x,
                  _mm_loadu_si128((const __m128i*)in)));
      else
      {
         _mm_storeu_si128((__m128i*)ks, x);
         for (i = 0; i < n; i++)
            out[i] = in[i] ^ ks[i];
      }
      ctr  = _mm_add_epi32(ctr, one);
      in  += n;
      out += n;
      len -= n;
   }
   _mm_storeu_si128((__m128i*)counter, _mm_shuffle_epi8(ctr, rev));
}

/* The unreduced 256-bit product of a and b, as *lo / *hi. */
AES_TARGET_X86
static void aes_x86_clmul(__m128i a, __m128i b, __m128i *lo, __m128i *hi)
{
   __m128i t3 = _mm_clmulepi64_si128(a, b, 0x00);
   __m128i t4 = _mm_clmulepi64_si128(a, b, 0x10);
   __m128i t5 = _mm_clmulepi64_si128(a, b, 0x01);
   __m128i t6 = _mm_clmulepi64_si128(a, b, 0x11);
   t4  = _mm_xor_si128(t4, t5);
   *lo = _mm_xor_si128(t3, _mm_slli_si128(t4, 8));
   *hi = _mm_xor_si128(t6, _mm_srli_si128(t4, 8));
}

/* The product reduced: one bit left (the reflected order) and modulo
 * x^128 + x^7 + x^2 + x + 1. Both steps are linear, so a sum of
 * products reduces as one. */
AES_TARGET_X86
static __m128i aes_x86_reduce(__m128i t3, __m128i t6)
{
   __m128i t7, t8, t9, t2, t4, t5;

   t7 = _mm_srli_epi32(t3, 31);
   t8 = _mm_srli_epi32(t6, 31);
   t3 = _mm_slli_epi32(t3, 1);
   t6 = _mm_slli_epi32(t6, 1);
   t9 = _mm_srli_si128(t7, 12);
   t8 = _mm_slli_si128(t8, 4);
   t7 = _mm_slli_si128(t7, 4);
   t3 = _mm_or_si128(t3, t7);
   t6 = _mm_or_si128(t6, t8);
   t6 = _mm_or_si128(t6, t9);

   t7 = _mm_slli_epi32(t3, 31);
   t8 = _mm_slli_epi32(t3, 30);
   t9 = _mm_slli_epi32(t3, 25);
   t7 = _mm_xor_si128(t7, t8);
   t7 = _mm_xor_si128(t7, t9);
   t8 = _mm_srli_si128(t7, 4);
   t7 = _mm_slli_si128(t7, 12);
   t3 = _mm_xor_si128(t3, t7);

   t2 = _mm_srli_epi32(t3, 1);
   t4 = _mm_srli_epi32(t3, 2);
   t5 = _mm_srli_epi32(t3, 7);
   t2 = _mm_xor_si128(t2, t4);
   t2 = _mm_xor_si128(t2, t5);
   t2 = _mm_xor_si128(t2, t8);
   t3 = _mm_xor_si128(t3, t2);
   return _mm_xor_si128(t6, t3);
}

/* GHASH multiply with PCLMULQDQ, operands byte-reversed so the
 * reflected GCM bit order becomes a plain polynomial product
 * (Intel's "Algorithm 1"). */
AES_TARGET_X86
static __m128i aes_x86_gfmul(__m128i a, __m128i b)
{
   __m128i lo, hi;
   aes_x86_clmul(a, b, &lo, &hi);
   return aes_x86_reduce(lo, hi);
}

AES_TARGET_X86
static void aes_x86_ghash(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
   const __m128i rev = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
   /* h_hi / h_lo are the big-endian halves of the 16 memory octets;
    * byte-reversing that whole block is what the multiply wants. */
   __m128i h = _mm_shuffle_epi8(_mm_set_epi64x((long long)__builtin_bswap64(ctx->h_lo), (long long)__builtin_bswap64(ctx->h_hi)), rev);
   __m128i y = _mm_shuffle_epi8(_mm_set_epi64x((long long)__builtin_bswap64(*y_lo), (long long)__builtin_bswap64(*y_hi)), rev);
   uint8_t tail[16];

   /* eight blocks per reduction: Y' = (Y+D0)H^8 + D1 H^7 + ... + D7 H,
    * each product by Karatsuba (three multiplies, not four), the eight
    * summed unreduced - low, high and middle terms apart - and folded
    * and reduced once, so the multiplies overlap instead of each
    * waiting on the last */
   if (len >= 128)
   {
      __m128i hp[8], hk[8];
      unsigned j;
      hp[0] = h;
      for (j = 1; j < 8; j++)
         hp[j] = aes_x86_gfmul(hp[j - 1], h);
      for (j = 0; j < 8; j++)
         hk[j] = _mm_xor_si128(hp[j], _mm_srli_si128(hp[j], 8));
      while (len >= 128)
      {
         __m128i lo = _mm_setzero_si128(), hi = lo, mid = lo;
         for (j = 0; j < 8; j++)
         {
            __m128i d = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 16 * j)), rev);
            const __m128i p = hp[7 - j], pk = hk[7 - j];
            if (!j)
               d = _mm_xor_si128(d, y);
            lo  = _mm_xor_si128(lo,  _mm_clmulepi64_si128(d, p, 0x00));
            hi  = _mm_xor_si128(hi,  _mm_clmulepi64_si128(d, p, 0x11));
            mid = _mm_xor_si128(mid, _mm_clmulepi64_si128(
                     _mm_xor_si128(d, _mm_srli_si128(d, 8)), pk, 0x00));
         }
         mid = _mm_xor_si128(mid, _mm_xor_si128(lo, hi));
         lo  = _mm_xor_si128(lo, _mm_slli_si128(mid, 8));
         hi  = _mm_xor_si128(hi, _mm_srli_si128(mid, 8));
         y   = aes_x86_reduce(lo, hi);
         data += 128;
         len  -= 128;
      }
   }
   while (len >= 16)
   {
      __m128i d = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)data), rev);
      y = aes_x86_gfmul(_mm_xor_si128(y, d), h);
      data += 16;
      len  -= 16;
   }
   if (len)
   {
      memset(tail, 0, 16);
      memcpy(tail, data, len);
      y = aes_x86_gfmul(_mm_xor_si128(y, _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)tail), rev)), h);
   }

   {
      uint8_t o[16];
      _mm_storeu_si128((__m128i*)o, _mm_shuffle_epi8(y, rev));
      *y_hi = crypto_load64_be(o);
      *y_lo = crypto_load64_be(o + 8);
   }
}
#endif

#if defined(AES_HAVE_ARM_PATH)
static int aes_arm_available(void)
{
#if defined(AES_ARM_DISPATCH)
   static int ok = -1;
   if (ok < 0)
      ok = (cpu_features_get() & RETRO_SIMD_AES) ? 1 : 0;
   return ok;
#else
   return 1;
#endif
}

/* Round key r as the 16 memory octets: the schedule holds big-endian
 * words, so a little-endian load needs each word's bytes reversed. */
AES_TARGET_ARM
static uint8x16_t aes_arm_rk(const struct aes_ctx *ctx, unsigned r)
{
   return vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32(ctx->rk + 4 * r)));
}

AES_TARGET_ARM
static uint8x16_t aes_arm_encrypt(const struct aes_ctx *ctx, uint8x16_t b)
{
   unsigned r;
   for (r = 0; r + 1 < ctx->rounds; r++)
      b = vaesmcq_u8(vaeseq_u8(b, aes_arm_rk(ctx, r)));
   b = vaeseq_u8(b, aes_arm_rk(ctx, r));
   return veorq_u8(b, aes_arm_rk(ctx, r + 1));
}

AES_TARGET_ARM
static void aes_arm_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   vst1q_u8(out, aes_arm_encrypt(ctx, vld1q_u8(in)));
}

/* Equivalent inverse cipher: AESD fuses the round key add with the
 * inverse shift and substitution, so the InvMixColumns that the
 * standard order puts after the key add moves onto the round key. */
AES_TARGET_ARM
static void aes_arm_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   unsigned r    = ctx->rounds;
   uint8x16_t b  = vaesdq_u8(vld1q_u8(in), aes_arm_rk(ctx, r));
   for (r--; r > 0; r--)
      b = vaesdq_u8(vaesimcq_u8(b), vaesimcq_u8(aes_arm_rk(ctx, r)));
   vst1q_u8(out, veorq_u8(b, aes_arm_rk(ctx, 0)));
}

/* CTR with eight blocks in flight, the round keys loaded once per
 * call. The counter block is kept byte-reversed within each 32-bit
 * lane, where its big-endian counter is lane 3 as a native word: one
 * add steps it (mod 2^32, as GCM's inc32) and one vrev32 turns it back
 * into a block - the other lanes reverse twice to themselves. */
AES_TARGET_ARM
static void aes_arm_ctr(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   const uint32x4_t z     = vdupq_n_u32(0);
   const uint32x4_t one   = vsetq_lane_u32(1, z, 3);
   const uint32x4_t two   = vsetq_lane_u32(2, z, 3);
   const uint32x4_t four  = vsetq_lane_u32(4, z, 3);
   const uint32x4_t eight = vsetq_lane_u32(8, z, 3);
   uint32x4_t cv = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(counter)));
   uint8x16_t rk[15];
   unsigned   r, nr = ctx->rounds;

   for (r = 0; r <= nr; r++)
      rk[r] = aes_arm_rk(ctx, r);

   while (len >= 128)
   {
      uint32x4_t c1 = vaddq_u32(cv, one);
      uint32x4_t c2 = vaddq_u32(cv, two);
      uint32x4_t c3 = vaddq_u32(c1, two);
      uint8x16_t b0 = vrev32q_u8(vreinterpretq_u8_u32(cv));
      uint8x16_t b1 = vrev32q_u8(vreinterpretq_u8_u32(c1));
      uint8x16_t b2 = vrev32q_u8(vreinterpretq_u8_u32(c2));
      uint8x16_t b3 = vrev32q_u8(vreinterpretq_u8_u32(c3));
      uint8x16_t b4 = vrev32q_u8(vreinterpretq_u8_u32(vaddq_u32(cv, four)));
      uint8x16_t b5 = vrev32q_u8(vreinterpretq_u8_u32(vaddq_u32(c1, four)));
      uint8x16_t b6 = vrev32q_u8(vreinterpretq_u8_u32(vaddq_u32(c2, four)));
      uint8x16_t b7 = vrev32q_u8(vreinterpretq_u8_u32(vaddq_u32(c3, four)));
      for (r = 0; r + 1 < nr; r++)
      {
         uint8x16_t k = rk[r];
         b0 = vaesmcq_u8(vaeseq_u8(b0, k)); b1 = vaesmcq_u8(vaeseq_u8(b1, k));
         b2 = vaesmcq_u8(vaeseq_u8(b2, k)); b3 = vaesmcq_u8(vaeseq_u8(b3, k));
         b4 = vaesmcq_u8(vaeseq_u8(b4, k)); b5 = vaesmcq_u8(vaeseq_u8(b5, k));
         b6 = vaesmcq_u8(vaeseq_u8(b6, k)); b7 = vaesmcq_u8(vaeseq_u8(b7, k));
      }
      vst1q_u8(out,       veorq_u8(veorq_u8(vaeseq_u8(b0, rk[nr - 1]), rk[nr]), vld1q_u8(in)));
      vst1q_u8(out +  16, veorq_u8(veorq_u8(vaeseq_u8(b1, rk[nr - 1]), rk[nr]), vld1q_u8(in +  16)));
      vst1q_u8(out +  32, veorq_u8(veorq_u8(vaeseq_u8(b2, rk[nr - 1]), rk[nr]), vld1q_u8(in +  32)));
      vst1q_u8(out +  48, veorq_u8(veorq_u8(vaeseq_u8(b3, rk[nr - 1]), rk[nr]), vld1q_u8(in +  48)));
      vst1q_u8(out +  64, veorq_u8(veorq_u8(vaeseq_u8(b4, rk[nr - 1]), rk[nr]), vld1q_u8(in +  64)));
      vst1q_u8(out +  80, veorq_u8(veorq_u8(vaeseq_u8(b5, rk[nr - 1]), rk[nr]), vld1q_u8(in +  80)));
      vst1q_u8(out +  96, veorq_u8(veorq_u8(vaeseq_u8(b6, rk[nr - 1]), rk[nr]), vld1q_u8(in +  96)));
      vst1q_u8(out + 112, veorq_u8(veorq_u8(vaeseq_u8(b7, rk[nr - 1]), rk[nr]), vld1q_u8(in + 112)));
      cv   = vaddq_u32(cv, eight);
      in  += 128;
      out += 128;
      len -= 128;
   }
   while (len)
   {
      size_t     n = (len < 16) ? len : 16;
      size_t     i;
      uint8_t    tmp[16];
      uint8x16_t x = vrev32q_u8(vreinterpretq_u8_u32(cv));
      for (r = 0; r + 1 < nr; r++)
         x = vaesmcq_u8(vaeseq_u8(x, rk[r]));
      x = veorq_u8(vaeseq_u8(x, rk[nr - 1]), rk[nr]);
      if (n == 16)
         vst1q_u8(out, veorq_u8(x, vld1q_u8(in)));
      else
      {
         vst1q_u8(tmp, x);
         for (i = 0; i < n; i++)
            out[i] = in[i] ^ tmp[i];
      }
      cv   = vaddq_u32(cv, one);
      in  += n;
      out += n;
      len -= n;
   }
   vst1q_u8(counter, vrev32q_u8(vreinterpretq_u8_u32(cv)));
}

/* GHASH multiply on PMULL: the same byte-reversed-operand algorithm
 * as the x86 path, with the SSE lane shifts spelled as vext and the
 * word shifts as vshl / vshr. */
/* The unreduced 256-bit product of a and b, as *lo / *hi. */
AES_TARGET_ARM
static void aes_arm_clmul(uint8x16_t a, uint8x16_t b, uint8x16_t *lo, uint8x16_t *hi)
{
   const uint8x16_t z = vdupq_n_u8(0);
   poly64x2_t pa = vreinterpretq_p64_u8(a);
   poly64x2_t pb = vreinterpretq_p64_u8(b);
   uint8x16_t t3 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 0), (poly64_t)vgetq_lane_p64(pb, 0)));
   uint8x16_t t4 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 0), (poly64_t)vgetq_lane_p64(pb, 1)));
   uint8x16_t t5 = vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pa, 1), (poly64_t)vgetq_lane_p64(pb, 0)));
   uint8x16_t t6 = vreinterpretq_u8_p128(vmull_high_p64(pa, pb));
   t4  = veorq_u8(t4, t5);
   *lo = veorq_u8(t3, vextq_u8(z, t4, 8));    /* slli_si128 8 */
   *hi = veorq_u8(t6, vextq_u8(t4, z, 8));    /* srli_si128 8 */
}

/* The product reduced: one bit left and modulo x^128 + x^7 + x^2 +
 * x + 1, both linear, so a sum of products reduces as one. */
AES_TARGET_ARM
static uint8x16_t aes_arm_reduce(uint8x16_t t3, uint8x16_t t6)
{
   const uint8x16_t z = vdupq_n_u8(0);
   uint8x16_t t7, t8, t9, t2, t4, t5;
   uint32x4_t w;

   w  = vreinterpretq_u32_u8(t3);
   t7 = vreinterpretq_u8_u32(vshrq_n_u32(w, 31));
   t3 = vreinterpretq_u8_u32(vshlq_n_u32(w, 1));
   w  = vreinterpretq_u32_u8(t6);
   t8 = vreinterpretq_u8_u32(vshrq_n_u32(w, 31));
   t6 = vreinterpretq_u8_u32(vshlq_n_u32(w, 1));
   t9 = vextq_u8(t7, z, 12);           /* srli_si128 12 */
   t8 = vextq_u8(z, t8, 12);           /* slli_si128 4 */
   t7 = vextq_u8(z, t7, 12);           /* slli_si128 4 */
   t3 = vorrq_u8(t3, t7);
   t6 = vorrq_u8(t6, t8);
   t6 = vorrq_u8(t6, t9);

   w  = vreinterpretq_u32_u8(t3);
   t7 = vreinterpretq_u8_u32(vshlq_n_u32(w, 31));
   t8 = vreinterpretq_u8_u32(vshlq_n_u32(w, 30));
   t9 = vreinterpretq_u8_u32(vshlq_n_u32(w, 25));
   t7 = veorq_u8(t7, t8);
   t7 = veorq_u8(t7, t9);
   t8 = vextq_u8(t7, z, 4);            /* srli_si128 4 */
   t7 = vextq_u8(z, t7, 4);            /* slli_si128 12 */
   t3 = veorq_u8(t3, t7);

   w  = vreinterpretq_u32_u8(t3);
   t2 = vreinterpretq_u8_u32(vshrq_n_u32(w, 1));
   t4 = vreinterpretq_u8_u32(vshrq_n_u32(w, 2));
   t5 = vreinterpretq_u8_u32(vshrq_n_u32(w, 7));
   t2 = veorq_u8(t2, t4);
   t2 = veorq_u8(t2, t5);
   t2 = veorq_u8(t2, t8);
   t3 = veorq_u8(t3, t2);
   return veorq_u8(t6, t3);
}

AES_TARGET_ARM
static uint8x16_t aes_arm_gfmul(uint8x16_t a, uint8x16_t b)
{
   uint8x16_t lo, hi;
   aes_arm_clmul(a, b, &lo, &hi);
   return aes_arm_reduce(lo, hi);
}

AES_TARGET_ARM
static void aes_arm_ghash(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
   uint8_t    o[16];
   uint8x16_t h, y;

   crypto_store64_be(o,     ctx->h_hi);
   crypto_store64_be(o + 8, ctx->h_lo);
   h = vrev64q_u8(vextq_u8(vld1q_u8(o), vld1q_u8(o), 8));   /* full byte reverse */
   crypto_store64_be(o,     *y_hi);
   crypto_store64_be(o + 8, *y_lo);
   y = vrev64q_u8(vextq_u8(vld1q_u8(o), vld1q_u8(o), 8));

   /* eight blocks per reduction, each product by Karatsuba, the low,
    * high and middle terms summed apart and folded once, as on x86 */
   if (len >= 128)
   {
      const uint8x16_t z = vdupq_n_u8(0);
      uint8x16_t hp[8], hk[8];
      unsigned   j;
      hp[0] = h;
      for (j = 1; j < 8; j++)
         hp[j] = aes_arm_gfmul(hp[j - 1], h);
      for (j = 0; j < 8; j++)
         hk[j] = veorq_u8(hp[j], vextq_u8(hp[j], hp[j], 8));
      while (len >= 128)
      {
         uint8x16_t lo = z, hi = z, mid = z;
         for (j = 0; j < 8; j++)
         {
            uint8x16_t d = vld1q_u8(data + 16 * j);
            uint8x16_t dk;
            poly64x2_t pd, pp, pdk, ppk;
            d = vrev64q_u8(vextq_u8(d, d, 8));
            if (!j)
               d = veorq_u8(d, y);
            dk  = veorq_u8(d, vextq_u8(d, d, 8));
            pd  = vreinterpretq_p64_u8(d);
            pp  = vreinterpretq_p64_u8(hp[7 - j]);
            pdk = vreinterpretq_p64_u8(dk);
            ppk = vreinterpretq_p64_u8(hk[7 - j]);
            lo  = veorq_u8(lo,  vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pd, 0), (poly64_t)vgetq_lane_p64(pp, 0))));
            hi  = veorq_u8(hi,  vreinterpretq_u8_p128(vmull_high_p64(pd, pp)));
            mid = veorq_u8(mid, vreinterpretq_u8_p128(vmull_p64((poly64_t)vgetq_lane_p64(pdk, 0), (poly64_t)vgetq_lane_p64(ppk, 0))));
         }
         mid = veorq_u8(mid, veorq_u8(lo, hi));
         lo  = veorq_u8(lo, vextq_u8(z, mid, 8));    /* slli_si128 8 */
         hi  = veorq_u8(hi, vextq_u8(mid, z, 8));    /* srli_si128 8 */
         y   = aes_arm_reduce(lo, hi);
         data += 128;
         len  -= 128;
      }
   }

   while (len >= 16)
   {
      uint8x16_t d = vld1q_u8(data);
      d = vrev64q_u8(vextq_u8(d, d, 8));
      y = aes_arm_gfmul(veorq_u8(y, d), h);
      data += 16;
      len  -= 16;
   }
   if (len)
   {
      uint8x16_t d;
      memset(o, 0, 16);
      memcpy(o, data, len);
      d = vld1q_u8(o);
      d = vrev64q_u8(vextq_u8(d, d, 8));
      y = aes_arm_gfmul(veorq_u8(y, d), h);
   }

   vst1q_u8(o, vrev64q_u8(vextq_u8(y, y, 8)));
   *y_hi = crypto_load64_be(o);
   *y_lo = crypto_load64_be(o + 8);
}
#endif

int crypto_aes_hw(void)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
      return 1;
#endif
#if defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
      return 1;
#endif
   return 0;
}

int aes_init(struct aes_ctx *ctx, const uint8_t *key, size_t key_len)
{
   unsigned i;
   unsigned nk;
   unsigned total;

   switch (key_len)
   {
      case 16: nk = 4; ctx->rounds = 10; break;
      case 24: nk = 6; ctx->rounds = 12; break;
      case 32: nk = 8; ctx->rounds = 14; break;
      default:
         return -1;
   }

   total = 4 * (ctx->rounds + 1);

   for (i = 0; i < nk; i++)
      ctx->rk[i] = crypto_load32_be(key + 4 * i);

   for (i = nk; i < total; i++)
   {
      uint32_t t = ctx->rk[i - 1];
      if ((i % nk) == 0)
         t = aes_sub_word(crypto_rotl32(t, 8)) ^ ((uint32_t)aes_rcon[i / nk - 1] << 24);
      else if (nk > 6 && (i % nk) == 4)
         t = aes_sub_word(t);
      ctx->rk[i] = ctx->rk[i - nk] ^ t;
   }

   return 0;
}

static void aes_encrypt_block_c(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   const uint32_t *rk = ctx->rk;
   uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
   unsigned r;

   s0 = crypto_load32_be(in)      ^ rk[0];
   s1 = crypto_load32_be(in + 4)  ^ rk[1];
   s2 = crypto_load32_be(in + 8)  ^ rk[2];
   s3 = crypto_load32_be(in + 12) ^ rk[3];
   rk += 4;

#define AES_ROUND(o0, o1, o2, o3, i0, i1, i2, i3)                        \
   o0 = aes_ft[0][i0 >> 24] ^ aes_ft[1][(i1 >> 16) & 0xff]              \
      ^ aes_ft[2][(i2 >> 8) & 0xff] ^ aes_ft[3][i3 & 0xff] ^ rk[0];      \
   o1 = aes_ft[0][i1 >> 24] ^ aes_ft[1][(i2 >> 16) & 0xff]              \
      ^ aes_ft[2][(i3 >> 8) & 0xff] ^ aes_ft[3][i0 & 0xff] ^ rk[1];      \
   o2 = aes_ft[0][i2 >> 24] ^ aes_ft[1][(i3 >> 16) & 0xff]              \
      ^ aes_ft[2][(i0 >> 8) & 0xff] ^ aes_ft[3][i1 & 0xff] ^ rk[2];      \
   o3 = aes_ft[0][i3 >> 24] ^ aes_ft[1][(i0 >> 16) & 0xff]              \
      ^ aes_ft[2][(i1 >> 8) & 0xff] ^ aes_ft[3][i2 & 0xff] ^ rk[3];      \
   rk += 4

   for (r = 1; r < ctx->rounds; r += 2)
   {
      AES_ROUND(t0, t1, t2, t3, s0, s1, s2, s3);
      if (r + 1 == ctx->rounds)
      {
         s0 = t0; s1 = t1; s2 = t2; s3 = t3;
         break;
      }
      AES_ROUND(s0, s1, s2, s3, t0, t1, t2, t3);
   }
#undef AES_ROUND

   /* Last round: SubBytes + ShiftRows only. */
   t0 = ((uint32_t)aes_sbox[s0 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s1 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s2 >> 8) & 0xff] << 8) ^ aes_sbox[s3 & 0xff] ^ rk[0];
   t1 = ((uint32_t)aes_sbox[s1 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s2 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s3 >> 8) & 0xff] << 8) ^ aes_sbox[s0 & 0xff] ^ rk[1];
   t2 = ((uint32_t)aes_sbox[s2 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s3 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s0 >> 8) & 0xff] << 8) ^ aes_sbox[s1 & 0xff] ^ rk[2];
   t3 = ((uint32_t)aes_sbox[s3 >> 24] << 24) ^ ((uint32_t)aes_sbox[(s0 >> 16) & 0xff] << 16)
      ^ ((uint32_t)aes_sbox[(s1 >> 8) & 0xff] << 8) ^ aes_sbox[s2 & 0xff] ^ rk[3];

   crypto_store32_be(out,      t0);
   crypto_store32_be(out + 4,  t1);
   crypto_store32_be(out + 8,  t2);
   crypto_store32_be(out + 12, t3);
}

/* InvMixColumns of one column: each output byte is the matching
 * byte of imc[b] rotated into place. */
static uint32_t aes_imc_col(uint32_t c)
{
   uint32_t t;
   t  = aes_imc[(c >> 24) & 0xff];
   t ^= crypto_rotl32(aes_imc[(c >> 16) & 0xff], 24);
   t ^= crypto_rotl32(aes_imc[(c >>  8) & 0xff], 16);
   t ^= crypto_rotl32(aes_imc[ c        & 0xff],  8);
   return t;
}

/* Inverse cipher, standard order: AddRoundKey with the last round key,
 * then per round InvShiftRows + InvSubBytes, AddRoundKey, and
 * InvMixColumns on the state (skipped for the final round). */
static void aes_decrypt_block_c(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
   const uint32_t *rk = ctx->rk + 4 * ctx->rounds;
   uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
   unsigned r;

   s0 = crypto_load32_be(in)      ^ rk[0];
   s1 = crypto_load32_be(in + 4)  ^ rk[1];
   s2 = crypto_load32_be(in + 8)  ^ rk[2];
   s3 = crypto_load32_be(in + 12) ^ rk[3];

   for (r = ctx->rounds; r > 0; r--)
   {
      rk -= 4;
      /* InvShiftRows moves row i right by i; column j gathers byte
       * row i from column (j - i) mod 4 */
      t0 = ((uint32_t)aes_isbox[(s0 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s3 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s2 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s1        & 0xff];
      t1 = ((uint32_t)aes_isbox[(s1 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s0 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s3 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s2        & 0xff];
      t2 = ((uint32_t)aes_isbox[(s2 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s1 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s0 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s3        & 0xff];
      t3 = ((uint32_t)aes_isbox[(s3 >> 24) & 0xff] << 24)
         | ((uint32_t)aes_isbox[(s2 >> 16) & 0xff] << 16)
         | ((uint32_t)aes_isbox[(s1 >>  8) & 0xff] <<  8)
         |  (uint32_t)aes_isbox[ s0        & 0xff];
      t0 ^= rk[0]; t1 ^= rk[1]; t2 ^= rk[2]; t3 ^= rk[3];
      if (r > 1)
      {
         s0 = aes_imc_col(t0); s1 = aes_imc_col(t1);
         s2 = aes_imc_col(t2); s3 = aes_imc_col(t3);
      }
      else
      {
         s0 = t0; s1 = t1; s2 = t2; s3 = t3;
      }
   }

   crypto_store32_be(out,      s0);
   crypto_store32_be(out + 4,  s1);
   crypto_store32_be(out + 8,  s2);
   crypto_store32_be(out + 12, s3);
}

void aes_decrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_decrypt_block(ctx, in, out);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_decrypt_block(ctx, in, out);
      return;
   }
#endif
   aes_decrypt_block_c(ctx, in, out);
}

void aes_encrypt_block(const struct aes_ctx *ctx,
      const uint8_t *in, uint8_t *out)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_encrypt_block(ctx, in, out);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_encrypt_block(ctx, in, out);
      return;
   }
#endif
   aes_encrypt_block_c(ctx, in, out);
}

static void aes_ctr_inc32(uint8_t *counter)
{
   uint32_t v = crypto_load32_be(counter + 12) + 1;
   crypto_store32_be(counter + 12, v);
}

void aes_ctr_crypt(const struct aes_ctx *ctx, uint8_t *counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   uint8_t ks[16];

#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_ctr(ctx, counter, in, out, len);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_ctr(ctx, counter, in, out, len);
      return;
   }
#endif
   while (len)
   {
      size_t n = (len < 16) ? len : 16;
      size_t i;

      aes_encrypt_block(ctx, counter, ks);
      aes_ctr_inc32(counter);

      for (i = 0; i < n; i++)
         out[i] = in[i] ^ ks[i];

      in  += n;
      out += n;
      len -= n;
   }

   crypto_memzero(ks, sizeof(ks));
}

/* Double in GF(2^128) with the CMAC polynomial. */
static void aes_cmac_dbl(uint8_t *b)
{
   unsigned i;
   uint8_t carry = 0;
   uint8_t msb   = b[0] >> 7;

   for (i = 16; i-- > 0; )
   {
      uint8_t next = b[i] >> 7;
      b[i] = (uint8_t)((b[i] << 1) | carry);
      carry = next;
   }
   b[15] ^= (uint8_t)(0x87 & (0 - msb));
}

void aes_cmac(const struct aes_ctx *ctx,
      const uint8_t *msg, size_t len, uint8_t *mac)
{
   unsigned i;
   size_t  nblocks;
   uint8_t k1[16];
   uint8_t x[16];
   uint8_t last[16];

   memset(x, 0, 16);
   aes_encrypt_block(ctx, x, k1);
   aes_cmac_dbl(k1);

   nblocks = (len + 15) / 16;
   if (nblocks == 0)
      nblocks = 1;

   /* Every block but the last, straight through the chain. */
   while (nblocks > 1)
   {
      for (i = 0; i < 16; i++)
         x[i] ^= msg[i];
      aes_encrypt_block(ctx, x, x);
      msg     += 16;
      len     -= 16;
      nblocks--;
   }

   if (len == 16)
      memcpy(last, msg, 16);
   else
   {
      /* Partial or empty last block: pad 10* and use K2 = dbl(K1). */
      aes_cmac_dbl(k1);
      memset(last, 0, 16);
      memcpy(last, msg, len);
      last[len] = 0x80;
   }

   for (i = 0; i < 16; i++)
      x[i] ^= last[i] ^ k1[i];
   aes_encrypt_block(ctx, x, mac);

   crypto_memzero(k1,   sizeof(k1));
   crypto_memzero(x,    sizeof(x));
   crypto_memzero(last, sizeof(last));
}

/* ---- CCM (RFC 3610) ---------------------------------------------- */

/* CBC-MAC over B_0 || AAD (length-prefixed, padded) || payload
 * (padded), then CTR with A_i counters; the tag is the MAC under
 * counter zero. */
static int aes_ccm_core(const struct aes_ctx *ctx, int encrypt,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *in, size_t len, uint8_t *out,
      uint8_t *tag, size_t tag_len)
{
   uint8_t  x[16];
   uint8_t  ctr[16];
   uint8_t  s0[16];
   size_t   L, i, off;
   size_t   remaining;

   if (nonce_len < 7 || nonce_len > 13 || tag_len < 4 || tag_len > 16 || (tag_len & 1))
      return -1;
   L = 15 - nonce_len;
   /* the length must fit L octets; shifted as 64 bits, since a 32-bit
    * size_t shifted by 32 (L = 4, the 11-octet nonce SMB uses) is
    * undefined and comes back unshifted on MIPS */
   if (L < 8 && ((uint64_t)len >> (8 * L)) != 0)
      return -1;

   /* B_0: flags | nonce | length of the message */
   x[0] = (uint8_t)((aad_len ? 0x40 : 0) | (((tag_len - 2) / 2) << 3) | (L - 1));
   memcpy(x + 1, nonce, nonce_len);
   remaining = len;
   for (i = 0; i < L; i++)
   {
      x[15 - i]  = (uint8_t)remaining;
      remaining >>= 8;
   }
   aes_encrypt_block(ctx, x, x);

   /* AAD, length-prefixed (2 or 6 octets here; > 2^32 is not needed) */
   if (aad_len)
   {
      uint8_t blk[16];
      size_t  n;
      memset(blk, 0, 16);
      if (aad_len < 0xff00)
      {
         blk[0] = (uint8_t)(aad_len >> 8);
         blk[1] = (uint8_t)aad_len;
         off    = 2;
      }
      else
      {
         blk[0] = 0xff; blk[1] = 0xfe;
         crypto_store32_be(blk + 2, (uint32_t)aad_len);
         off = 6;
      }
      n = 16 - off;
      if (n > aad_len)
         n = aad_len;
      memcpy(blk + off, aad, n);
      for (i = 0; i < 16; i++)
         x[i] ^= blk[i];
      aes_encrypt_block(ctx, x, x);
      aad += n;
      aad_len -= n;
      while (aad_len)
      {
         n = aad_len < 16 ? aad_len : 16;
         for (i = 0; i < n; i++)
            x[i] ^= aad[i];
         aes_encrypt_block(ctx, x, x);
         aad += n;
         aad_len -= n;
      }
   }

   /* Counter blocks: flags = L - 1, nonce, counter */
   memset(ctr, 0, 16);
   ctr[0] = (uint8_t)(L - 1);
   memcpy(ctr + 1, nonce, nonce_len);
   aes_encrypt_block(ctx, ctr, s0);           /* S_0, for the tag */

   off = 0;
   while (off < len)
   {
      uint8_t ks[16];
      size_t  n = (len - off < 16) ? len - off : 16;
      size_t  c = off / 16 + 1;
      for (i = 0; i < L; i++)
      {
         ctr[15 - i] = (uint8_t)c;
         c >>= 8;
      }
      aes_encrypt_block(ctx, ctr, ks);
      if (encrypt)
      {
         for (i = 0; i < n; i++)
            x[i] ^= in[off + i];
         aes_encrypt_block(ctx, x, x);
         for (i = 0; i < n; i++)
            out[off + i] = in[off + i] ^ ks[i];
      }
      else
      {
         for (i = 0; i < n; i++)
         {
            out[off + i] = in[off + i] ^ ks[i];
            x[i]        ^= out[off + i];
         }
         aes_encrypt_block(ctx, x, x);
      }
      off += n;
   }

   for (i = 0; i < tag_len; i++)
      tag[i] = x[i] ^ s0[i];
   crypto_memzero(x, sizeof(x));
   crypto_memzero(s0, sizeof(s0));
   return 0;
}

int aes_ccm_encrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len)
{
   return aes_ccm_core(ctx, 1, nonce, nonce_len, aad, aad_len,
         pt, pt_len, ct, tag, tag_len);
}

int aes_ccm_decrypt(const struct aes_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len, uint8_t *pt)
{
   uint8_t calc[16];
   if (aes_ccm_core(ctx, 0, nonce, nonce_len, aad, aad_len,
            ct, ct_len, pt, calc, tag_len) != 0)
      return -1;
   if (!crypto_memeq_ct(calc, tag, tag_len))
   {
      crypto_memzero(pt, ct_len);
      return -1;
   }
   return 0;
}

/* GHASH, scalar: 4-bit tables of H (16 entries, built at init) and a
 * 16-entry reduction table, two lookups per octet. */
static const uint64_t aes_gcm_last4[16] = {
   0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
   0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

static void aes_gcm_mul(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo)
{
   uint8_t  x[16];
   uint64_t zh, zl, rem;
   int      i;
   unsigned lo, hi;

   crypto_store64_be(x,     *y_hi);
   crypto_store64_be(x + 8, *y_lo);

   lo = x[15] & 0x0f;
   zh = ctx->hh[lo];
   zl = ctx->hl[lo];

   for (i = 15; i >= 0; i--)
   {
      lo = x[i] & 0x0f;
      hi = (x[i] >> 4) & 0x0f;

      if (i != 15)
      {
         rem = zl & 0x0f;
         zl  = (zh << 60) | (zl >> 4);
         zh  = (zh >> 4);
         zh ^= aes_gcm_last4[rem] << 48;
         zh ^= ctx->hh[lo];
         zl ^= ctx->hl[lo];
      }
      rem = zl & 0x0f;
      zl  = (zh << 60) | (zl >> 4);
      zh  = (zh >> 4);
      zh ^= aes_gcm_last4[rem] << 48;
      zh ^= ctx->hh[hi];
      zl ^= ctx->hl[hi];
   }

   *y_hi = zh;
   *y_lo = zl;
}

static void aes_gcm_ghash_update(const struct aes_gcm_ctx *ctx,
      uint64_t *y_hi, uint64_t *y_lo, const uint8_t *data, size_t len)
{
#if defined(AES_HAVE_X86_PATH)
   if (aes_x86_available())
   {
      aes_x86_ghash(ctx, y_hi, y_lo, data, len);
      return;
   }
#elif defined(AES_HAVE_ARM_PATH)
   if (aes_arm_available())
   {
      aes_arm_ghash(ctx, y_hi, y_lo, data, len);
      return;
   }
#endif
   while (len)
   {
      uint8_t blk[16];
      size_t  n = (len < 16) ? len : 16;

      if (n < 16)
      {
         memset(blk, 0, 16);
         memcpy(blk, data, n);
         data = blk;
      }

      *y_hi ^= crypto_load64_be(data);
      *y_lo ^= crypto_load64_be(data + 8);
      aes_gcm_mul(ctx, y_hi, y_lo);

      data += n;
      len  -= n;
   }
}

static void aes_gcm_tables(struct aes_gcm_ctx *ctx)
{
   uint64_t vh = ctx->h_hi, vl = ctx->h_lo;
   int i, j;

   ctx->hh[0] = 0; ctx->hl[0] = 0;
   ctx->hh[8] = vh; ctx->hl[8] = vl;

   for (i = 4; i > 0; i >>= 1)
   {
      uint32_t t = (uint32_t)(vl & 1) * 0xe1000000u;
      vl = (vh << 63) | (vl >> 1);
      vh = (vh >> 1) ^ ((uint64_t)t << 32);
      ctx->hl[i] = vl;
      ctx->hh[i] = vh;
   }
   for (i = 2; i <= 8; i *= 2)
   {
      vh = ctx->hh[i];
      vl = ctx->hl[i];
      for (j = 1; j < i; j++)
      {
         ctx->hh[i + j] = vh ^ ctx->hh[j];
         ctx->hl[i + j] = vl ^ ctx->hl[j];
      }
   }
}

int aes_gcm_init(struct aes_gcm_ctx *ctx,
      const uint8_t *key, size_t key_len)
{
   uint8_t h[16];

   if (aes_init(&ctx->aes, key, key_len) != 0)
      return -1;

   memset(h, 0, 16);
   aes_encrypt_block(&ctx->aes, h, h);
   ctx->h_hi = crypto_load64_be(h);
   ctx->h_lo = crypto_load64_be(h + 8);
   aes_gcm_tables(ctx);
   crypto_memzero(h, sizeof(h));
   return 0;
}

static void aes_gcm_j0(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len, uint8_t *j0)
{
   if (nonce_len == 12)
   {
      memcpy(j0, nonce, 12);
      j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
   }
   else
   {
      uint64_t y_hi = 0, y_lo = 0;
      aes_gcm_ghash_update(ctx, &y_hi, &y_lo, nonce, nonce_len);
      y_lo ^= (uint64_t)nonce_len * 8;
      aes_gcm_mul(ctx, &y_hi, &y_lo);
      crypto_store64_be(j0,     y_hi);
      crypto_store64_be(j0 + 8, y_lo);
   }
}

static void aes_gcm_tag(const struct aes_gcm_ctx *ctx, const uint8_t *j0,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len, uint8_t *tag)
{
   unsigned i;
   uint64_t y_hi = 0, y_lo = 0;
   uint8_t  ek[16];

   aes_gcm_ghash_update(ctx, &y_hi, &y_lo, aad, aad_len);
   aes_gcm_ghash_update(ctx, &y_hi, &y_lo, ct, ct_len);
   y_hi ^= (uint64_t)aad_len * 8;
   y_lo ^= (uint64_t)ct_len  * 8;
   aes_gcm_mul(ctx, &y_hi, &y_lo);

   aes_encrypt_block(&ctx->aes, j0, ek);
   crypto_store64_be(tag,     y_hi);
   crypto_store64_be(tag + 8, y_lo);
   for (i = 0; i < 16; i++)
      tag[i] ^= ek[i];
   crypto_memzero(ek, sizeof(ek));
}

int aes_gcm_encrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag)
{
   uint8_t j0[16];
   uint8_t ctr[16];

   aes_gcm_j0(ctx, nonce, nonce_len, j0);
   memcpy(ctr, j0, 16);
   aes_ctr_inc32(ctr);
   aes_ctr_crypt(&ctx->aes, ctr, pt, ct, pt_len);
   aes_gcm_tag(ctx, j0, aad, aad_len, ct, pt_len, tag);
   return 0;
}

int aes_gcm_decrypt(const struct aes_gcm_ctx *ctx,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, uint8_t *pt)
{
   uint8_t j0[16];
   uint8_t ctr[16];
   uint8_t calc[16];
   int ok;

   aes_gcm_j0(ctx, nonce, nonce_len, j0);
   aes_gcm_tag(ctx, j0, aad, aad_len, ct, ct_len, calc);
   ok = crypto_memeq_ct(calc, tag, 16);
   crypto_memzero(calc, sizeof(calc));
   if (!ok)
      return -1;

   memcpy(ctr, j0, 16);
   aes_ctr_inc32(ctr);
   aes_ctr_crypt(&ctx->aes, ctr, ct, pt, ct_len);
   return 0;
}

#define QR(a, b, c, d)                                  \
   a += b; d ^= a; d = crypto_rotl32(d, 16);           \
   c += d; b ^= c; b = crypto_rotl32(b, 12);           \
   a += b; d ^= a; d = crypto_rotl32(d,  8);           \
   c += d; b ^= c; b = crypto_rotl32(b,  7)

static void chacha20_core(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, uint32_t *out)
{
   unsigned i;
   uint32_t s[16];
   uint32_t x[16];

   s[0]  = 0x61707865; s[1]  = 0x3320646e;
   s[2]  = 0x79622d32; s[3]  = 0x6b206574;
   for (i = 0; i < 8; i++)
      s[4 + i] = crypto_load32_le(key + 4 * i);
   s[12] = counter;
   s[13] = crypto_load32_le(nonce);
   s[14] = crypto_load32_le(nonce + 4);
   s[15] = crypto_load32_le(nonce + 8);

   memcpy(x, s, sizeof(x));

   for (i = 0; i < 10; i++)
   {
      QR(x[0], x[4], x[ 8], x[12]);
      QR(x[1], x[5], x[ 9], x[13]);
      QR(x[2], x[6], x[10], x[14]);
      QR(x[3], x[7], x[11], x[15]);
      QR(x[0], x[5], x[10], x[15]);
      QR(x[1], x[6], x[11], x[12]);
      QR(x[2], x[7], x[ 8], x[13]);
      QR(x[3], x[4], x[ 9], x[14]);
   }

   for (i = 0; i < 16; i++)
      out[i] = x[i] + s[i];
}

void chacha20_block(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, uint8_t *out)
{
   unsigned i;
   uint32_t w[16];

   chacha20_core(key, nonce, counter, w);
   for (i = 0; i < 16; i++)
      crypto_store32_le(out + 4 * i, w[i]);
}

#if defined(AES_HAVE_X86_PATH)
static retro_atomic_int_t chacha_x86_ok;

static int chacha_x86_available(void)
{
   int v = retro_atomic_load_acquire_int(&chacha_x86_ok);
   if (!v)
   {
      v = (cpu_features_get() & RETRO_SIMD_SSSE3) ? 2 : 1;
      retro_atomic_store_release_int(&chacha_x86_ok, v);
   }
   return v == 2;
}

#define CHACHA_ROTL(v, n) _mm_or_si128(_mm_slli_epi32(v, n), _mm_srli_epi32(v, 32 - (n)))
#define CHACHA_QR4(a, b, c, d)                                         \
   a = _mm_add_epi32(a, b); d = _mm_shuffle_epi8(_mm_xor_si128(d, a), r16); \
   c = _mm_add_epi32(c, d); b = CHACHA_ROTL(_mm_xor_si128(b, c), 12);       \
   a = _mm_add_epi32(a, b); d = _mm_shuffle_epi8(_mm_xor_si128(d, a), r8);  \
   c = _mm_add_epi32(c, d); b = CHACHA_ROTL(_mm_xor_si128(b, c), 7)

/* Four blocks at once, one state word per vector and one block per
 * lane; the 16- and 8-bit rotations are byte shuffles. @in to @out,
 * whole groups of 256 octets; returns the octets done. */
__attribute__((target("ssse3")))
static size_t chacha20_x86_xor(const uint32_t *s, uint32_t counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   const __m128i r16 = _mm_set_epi8(13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2);
   const __m128i r8  = _mm_set_epi8(14,13,12,15, 10,9,8,11, 6,5,4,7, 2,1,0,3);
   size_t done = 0;

   while (len - done >= 256)
   {
      __m128i x[16], o[16];
      unsigned i, g;
      for (i = 0; i < 16; i++)
         o[i] = _mm_set1_epi32((int)s[i]);
      o[12] = _mm_add_epi32(_mm_set1_epi32((int)counter), _mm_set_epi32(3, 2, 1, 0));
      for (i = 0; i < 16; i++)
         x[i] = o[i];
      for (i = 0; i < 10; i++)
      {
         CHACHA_QR4(x[0], x[4], x[ 8], x[12]);
         CHACHA_QR4(x[1], x[5], x[ 9], x[13]);
         CHACHA_QR4(x[2], x[6], x[10], x[14]);
         CHACHA_QR4(x[3], x[7], x[11], x[15]);
         CHACHA_QR4(x[0], x[5], x[10], x[15]);
         CHACHA_QR4(x[1], x[6], x[11], x[12]);
         CHACHA_QR4(x[2], x[7], x[ 8], x[13]);
         CHACHA_QR4(x[3], x[4], x[ 9], x[14]);
      }
      for (i = 0; i < 16; i++)
         x[i] = _mm_add_epi32(x[i], o[i]);
      /* four words of each of the four blocks per group: transposed
       * back into block order and XORed in place */
      for (g = 0; g < 4; g++)
      {
         __m128i t0 = _mm_unpacklo_epi32(x[4 * g],     x[4 * g + 1]);
         __m128i t1 = _mm_unpacklo_epi32(x[4 * g + 2], x[4 * g + 3]);
         __m128i t2 = _mm_unpackhi_epi32(x[4 * g],     x[4 * g + 1]);
         __m128i t3 = _mm_unpackhi_epi32(x[4 * g + 2], x[4 * g + 3]);
         const uint8_t *ip = in + done + 16 * g;
         uint8_t       *op = out + done + 16 * g;
         _mm_storeu_si128((__m128i*)(op),       _mm_xor_si128(_mm_unpacklo_epi64(t0, t1), _mm_loadu_si128((const __m128i*)(ip))));
         _mm_storeu_si128((__m128i*)(op +  64), _mm_xor_si128(_mm_unpackhi_epi64(t0, t1), _mm_loadu_si128((const __m128i*)(ip +  64))));
         _mm_storeu_si128((__m128i*)(op + 128), _mm_xor_si128(_mm_unpacklo_epi64(t2, t3), _mm_loadu_si128((const __m128i*)(ip + 128))));
         _mm_storeu_si128((__m128i*)(op + 192), _mm_xor_si128(_mm_unpackhi_epi64(t2, t3), _mm_loadu_si128((const __m128i*)(ip + 192))));
      }
      counter += 4;
      done    += 256;
   }
   return done;
}
#undef CHACHA_QR4
#undef CHACHA_ROTL
#endif

/* every AArch64 core, and ARMv7 built for NEON (the Vita, 32-bit
 * Android); older GCC spells the macro __ARM_NEON__ */
#if (defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(_MSC_VER) && !defined(CHACHA_NO_NEON)
#include <arm_neon.h>
#define CHACHA_HAVE_NEON 1
/* rotl by 16 is a halfword swap; the others a shift and a shift-insert */
#define CHACHA_NROT(v, n) vsriq_n_u32(vshlq_n_u32(v, n), v, 32 - (n))
#define CHACHA_NQR(a, b, c, d)                                                \
   a = vaddq_u32(a, b); d = vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(veorq_u32(d, a)))); \
   c = vaddq_u32(c, d); b = CHACHA_NROT(veorq_u32(b, c), 12);                 \
   a = vaddq_u32(a, b); d = CHACHA_NROT(veorq_u32(d, a), 8);                  \
   c = vaddq_u32(c, d); b = CHACHA_NROT(veorq_u32(b, c), 7)

/* Four blocks at once on NEON: one state word per vector and one block
 * per lane, as on SSSE3. Whole groups of 256 octets; returns the octets
 * done. */
static size_t chacha20_neon_xor(const uint32_t *s, uint32_t counter,
      const uint8_t *in, uint8_t *out, size_t len)
{
   static const uint32_t lane[4] = { 0, 1, 2, 3 };
   size_t done = 0;

   while (len - done >= 256)
   {
      uint32x4_t x[16], o[16];
      unsigned   i, g;
      for (i = 0; i < 16; i++)
         o[i] = vdupq_n_u32(s[i]);
      o[12] = vaddq_u32(vdupq_n_u32(counter), vld1q_u32(lane));
      for (i = 0; i < 16; i++)
         x[i] = o[i];
      for (i = 0; i < 10; i++)
      {
         CHACHA_NQR(x[0], x[4], x[ 8], x[12]);
         CHACHA_NQR(x[1], x[5], x[ 9], x[13]);
         CHACHA_NQR(x[2], x[6], x[10], x[14]);
         CHACHA_NQR(x[3], x[7], x[11], x[15]);
         CHACHA_NQR(x[0], x[5], x[10], x[15]);
         CHACHA_NQR(x[1], x[6], x[11], x[12]);
         CHACHA_NQR(x[2], x[7], x[ 8], x[13]);
         CHACHA_NQR(x[3], x[4], x[ 9], x[14]);
      }
      for (i = 0; i < 16; i++)
         x[i] = vaddq_u32(x[i], o[i]);
      /* each group of four words back into block order: a 4x4
       * transpose, then XORed in */
      for (g = 0; g < 4; g++)
      {
         uint32x4x2_t p = vtrnq_u32(x[4 * g],     x[4 * g + 1]);
         uint32x4x2_t q = vtrnq_u32(x[4 * g + 2], x[4 * g + 3]);
         uint32x4_t   r0 = vcombine_u32(vget_low_u32(p.val[0]),  vget_low_u32(q.val[0]));
         uint32x4_t   r1 = vcombine_u32(vget_low_u32(p.val[1]),  vget_low_u32(q.val[1]));
         uint32x4_t   r2 = vcombine_u32(vget_high_u32(p.val[0]), vget_high_u32(q.val[0]));
         uint32x4_t   r3 = vcombine_u32(vget_high_u32(p.val[1]), vget_high_u32(q.val[1]));
         const uint8_t *ip = in + done + 16 * g;
         uint8_t       *op = out + done + 16 * g;
         vst1q_u8(op,       veorq_u8(vreinterpretq_u8_u32(r0), vld1q_u8(ip)));
         vst1q_u8(op +  64, veorq_u8(vreinterpretq_u8_u32(r1), vld1q_u8(ip +  64)));
         vst1q_u8(op + 128, veorq_u8(vreinterpretq_u8_u32(r2), vld1q_u8(ip + 128)));
         vst1q_u8(op + 192, veorq_u8(vreinterpretq_u8_u32(r3), vld1q_u8(ip + 192)));
      }
      counter += 4;
      done    += 256;
   }
   return done;
}
#undef CHACHA_NQR
#undef CHACHA_NROT
#endif

void chacha20_xor(const uint8_t *key, const uint8_t *nonce,
      uint32_t counter, const uint8_t *in, uint8_t *out, size_t len)
{
   uint32_t w[16];
   unsigned i;

#if defined(AES_HAVE_X86_PATH) || defined(CHACHA_HAVE_NEON)
#if defined(AES_HAVE_X86_PATH)
   if (len >= 256 && chacha_x86_available())
#else
   if (len >= 256)
#endif
   {
      uint32_t s[16];
      size_t   done;
      s[0]  = 0x61707865; s[1]  = 0x3320646e;
      s[2]  = 0x79622d32; s[3]  = 0x6b206574;
      for (i = 0; i < 8; i++)
         s[4 + i] = crypto_load32_le(key + 4 * i);
      s[12] = counter;
      s[13] = crypto_load32_le(nonce);
      s[14] = crypto_load32_le(nonce + 4);
      s[15] = crypto_load32_le(nonce + 8);
#if defined(AES_HAVE_X86_PATH)
      done     = chacha20_x86_xor(s, counter, in, out, len);
#else
      done     = chacha20_neon_xor(s, counter, in, out, len);
#endif
      counter += (uint32_t)(done / 64);
      in      += done;
      out     += done;
      len     -= done;
      crypto_memzero(s, sizeof(s));
   }
#endif

   /* Whole blocks a word at a time, no keystream bounce buffer. */
   while (len >= 64)
   {
      chacha20_core(key, nonce, counter, w);
      counter++;
      for (i = 0; i < 16; i++)
         crypto_store32_le(out + 4 * i, crypto_load32_le(in + 4 * i) ^ w[i]);
      in  += 64;
      out += 64;
      len -= 64;
   }
   if (len)
   {
      uint8_t ks[64];
      chacha20_block(key, nonce, counter, ks);
      for (i = 0; i < len; i++)
         out[i] = in[i] ^ ks[i];
      crypto_memzero(ks, sizeof(ks));
   }
   crypto_memzero(w, sizeof(w));
}

#define P1305_MASK 0x3ffffff

void poly1305_init(struct poly1305_ctx *ctx, const uint8_t *key)
{
   uint32_t t0 = crypto_load32_le(key);
   uint32_t t1 = crypto_load32_le(key + 4);
   uint32_t t2 = crypto_load32_le(key + 8);
   uint32_t t3 = crypto_load32_le(key + 12);

   /* r &= 0xffffffc0ffffffc0ffffffc0fffffff, split into 26-bit limbs. */
   ctx->r[0] = t0 & 0x3ffffff; t0 >>= 26; t0 |= t1 << 6;
   ctx->r[1] = t0 & 0x3ffff03; t1 >>= 20; t1 |= t2 << 12;
   ctx->r[2] = t1 & 0x3ffc0ff; t2 >>= 14; t2 |= t3 << 18;
   ctx->r[3] = t2 & 0x3f03fff; t3 >>= 8;
   ctx->r[4] = t3 & 0x00fffff;

   ctx->h[0] = ctx->h[1] = ctx->h[2] = ctx->h[3] = ctx->h[4] = 0;

   ctx->pad[0] = crypto_load32_le(key + 16);
   ctx->pad[1] = crypto_load32_le(key + 20);
   ctx->pad[2] = crypto_load32_le(key + 24);
   ctx->pad[3] = crypto_load32_le(key + 28);

   ctx->leftover = 0;
   ctx->final    = 0;
}

#if defined(__SIZEOF_INT128__) && !defined(POLY1305_NO_128)
/* 64-bit machines: the same arithmetic in 44-bit limbs, 9 multiplies
 * per block instead of 25. The context keeps 26-bit limbs; they are
 * turned into 44-bit ones on the way in and back on the way out, which
 * costs a few operations a call against every block's savings. */
__extension__ typedef unsigned __int128 p1305_u128;
#define P1305_M44 ((uint64_t)0xfffffffffffULL)
#define P1305_M42 ((uint64_t)0x3ffffffffffULL)

static void poly1305_blocks_44(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len)
{
   const uint64_t hibit = ctx->final ? 0 : ((uint64_t)1 << 40);
   uint64_t r0, r1, r2, s1, s2, h0, h1, h2, lo;
   uint32_t c, g[5];
   p1305_u128 up;
   unsigned i;

   /* r: 124 bits, its 26-bit limbs exact */
   lo = (uint64_t)ctx->r[0] | ((uint64_t)ctx->r[1] << 26) | ((uint64_t)ctx->r[2] << 52);
   up = (p1305_u128)(ctx->r[2] >> 12) | ((p1305_u128)ctx->r[3] << 14)
      | ((p1305_u128)ctx->r[4] << 40);
   r0 = lo & P1305_M44;
   r1 = ((lo >> 44) | (uint64_t)(up << 20)) & P1305_M44;
   r2 = (uint64_t)(up >> 24) & P1305_M42;
   s1 = r1 * (5 << 2);
   s2 = r2 * (5 << 2);

   /* h: partly reduced 26-bit limbs, carried first so each fits */
   for (i = 0; i < 5; i++)
      g[i] = ctx->h[i];
   c = g[0] >> 26; g[0] &= P1305_MASK; g[1] += c;
   c = g[1] >> 26; g[1] &= P1305_MASK; g[2] += c;
   c = g[2] >> 26; g[2] &= P1305_MASK; g[3] += c;
   c = g[3] >> 26; g[3] &= P1305_MASK; g[4] += c;
   lo = (uint64_t)g[0] | ((uint64_t)g[1] << 26) | ((uint64_t)(g[2] & 0xfff) << 52);
   up = (p1305_u128)(g[2] >> 12) + ((p1305_u128)g[3] << 14) + ((p1305_u128)g[4] << 40);
   h0 = lo & P1305_M44;
   h1 = ((lo >> 44) | (uint64_t)(up << 20)) & P1305_M44;
   h2 = (uint64_t)(up >> 24);

   while (len >= 16)
   {
      uint64_t t0 = (uint64_t)crypto_load32_le(m)     | ((uint64_t)crypto_load32_le(m + 4)  << 32);
      uint64_t t1 = (uint64_t)crypto_load32_le(m + 8) | ((uint64_t)crypto_load32_le(m + 12) << 32);
      p1305_u128 d0, d1, d2;
      uint64_t cc;

      h0 += t0 & P1305_M44;
      h1 += ((t0 >> 44) | (t1 << 20)) & P1305_M44;
      h2 += ((t1 >> 24) & P1305_M42) | hibit;

      d0 = (p1305_u128)h0 * r0 + (p1305_u128)h1 * s2 + (p1305_u128)h2 * s1;
      d1 = (p1305_u128)h0 * r1 + (p1305_u128)h1 * r0 + (p1305_u128)h2 * s2;
      d2 = (p1305_u128)h0 * r2 + (p1305_u128)h1 * r1 + (p1305_u128)h2 * r0;

      cc = (uint64_t)(d0 >> 44); h0 = (uint64_t)d0 & P1305_M44;
      d1 += cc; cc = (uint64_t)(d1 >> 44); h1 = (uint64_t)d1 & P1305_M44;
      d2 += cc; cc = (uint64_t)(d2 >> 42); h2 = (uint64_t)d2 & P1305_M42;
      h0 += cc * 5; cc = h0 >> 44; h0 &= P1305_M44;
      h1 += cc;

      m   += 16;
      len -= 16;
   }

   /* back to 26-bit limbs: h0 | h1 << 44 | h2 << 88 */
   ctx->h[0] = (uint32_t)(h0 & P1305_MASK);
   ctx->h[1] = (uint32_t)(((h0 >> 26) | (h1 << 18)) & P1305_MASK);
   ctx->h[2] = (uint32_t)((h1 >> 8) & P1305_MASK);
   ctx->h[3] = (uint32_t)(((h1 >> 34) | (h2 << 10)) & P1305_MASK);
   ctx->h[4] = (uint32_t)(h2 >> 16);
}
#endif

static void poly1305_blocks(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len)
{
#if defined(__SIZEOF_INT128__) && !defined(POLY1305_NO_128)
   poly1305_blocks_44(ctx, m, len);
}
#else
   const uint32_t hibit = ctx->final ? 0 : ((uint32_t)1 << 24);
   uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2];
   uint32_t r3 = ctx->r[3], r4 = ctx->r[4];
   uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
   uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
   uint32_t h3 = ctx->h[3], h4 = ctx->h[4];

   while (len >= 16)
   {
      uint64_t d0, d1, d2, d3, d4;
      uint32_t c;
      uint32_t t0 = crypto_load32_le(m);
      uint32_t t1 = crypto_load32_le(m + 4);
      uint32_t t2 = crypto_load32_le(m + 8);
      uint32_t t3 = crypto_load32_le(m + 12);

      /* h += m[i] */
      h0 += t0 & P1305_MASK;
      h1 += ((t0 >> 26) | (t1 <<  6)) & P1305_MASK;
      h2 += ((t1 >> 20) | (t2 << 12)) & P1305_MASK;
      h3 += ((t2 >> 14) | (t3 << 18)) & P1305_MASK;
      h4 += (t3 >> 8) | hibit;

      /* h *= r, mod 2^130 - 5 */
      d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3
         + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
      d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4
         + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
      d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0
         + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
      d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1
         + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
      d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2
         + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

      /* partial reduction */
                    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & P1305_MASK;
      d1 += c;      c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & P1305_MASK;
      d2 += c;      c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & P1305_MASK;
      d3 += c;      c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & P1305_MASK;
      d4 += c;      c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & P1305_MASK;
      h0 += c * 5;  c = h0 >> 26;             h0 &= P1305_MASK;
      h1 += c;

      m   += 16;
      len -= 16;
   }

   ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2;
   ctx->h[3] = h3; ctx->h[4] = h4;
}
#endif

void poly1305_update(struct poly1305_ctx *ctx,
      const uint8_t *m, size_t len)
{
   size_t want;

   if (ctx->leftover)
   {
      want = 16 - ctx->leftover;
      if (want > len)
         want = len;
      memcpy(ctx->buffer + ctx->leftover, m, want);
      len           -= want;
      m             += want;
      ctx->leftover += want;
      if (ctx->leftover < 16)
         return;
      poly1305_blocks(ctx, ctx->buffer, 16);
      ctx->leftover = 0;
   }

   if (len >= 16)
   {
      want = len & ~(size_t)15;
      poly1305_blocks(ctx, m, want);
      m   += want;
      len -= want;
   }

   if (len)
   {
      memcpy(ctx->buffer, m, len);
      ctx->leftover = len;
   }
}

void poly1305_final(struct poly1305_ctx *ctx, uint8_t *tag)
{
   uint32_t h0, h1, h2, h3, h4, c;
   uint32_t g0, g1, g2, g3, g4;
   uint64_t f;
   uint32_t mask;

   if (ctx->leftover)
   {
      size_t i = ctx->leftover;
      ctx->buffer[i++] = 1;
      for (; i < 16; i++)
         ctx->buffer[i] = 0;
      ctx->final = 1;
      poly1305_blocks(ctx, ctx->buffer, 16);
   }

   h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2];
   h3 = ctx->h[3]; h4 = ctx->h[4];

   /* full carry */
                c = h1 >> 26; h1 &= P1305_MASK;
   h2 +=     c; c = h2 >> 26; h2 &= P1305_MASK;
   h3 +=     c; c = h3 >> 26; h3 &= P1305_MASK;
   h4 +=     c; c = h4 >> 26; h4 &= P1305_MASK;
   h0 += c * 5; c = h0 >> 26; h0 &= P1305_MASK;
   h1 +=     c;

   /* g = h + 5 - 2^130; select g when it did not go negative */
   g0 = h0 + 5; c = g0 >> 26; g0 &= P1305_MASK;
   g1 = h1 + c; c = g1 >> 26; g1 &= P1305_MASK;
   g2 = h2 + c; c = g2 >> 26; g2 &= P1305_MASK;
   g3 = h3 + c; c = g3 >> 26; g3 &= P1305_MASK;
   g4 = h4 + c - ((uint32_t)1 << 26);

   mask = (g4 >> 31) - 1;
   g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
   mask = ~mask;
   h0 = (h0 & mask) | g0;
   h1 = (h1 & mask) | g1;
   h2 = (h2 & mask) | g2;
   h3 = (h3 & mask) | g3;
   h4 = (h4 & mask) | g4;

   /* h = h % 2^128 as four 32-bit words */
   h0 = ((h0      ) | (h1 << 26));
   h1 = ((h1 >>  6) | (h2 << 20));
   h2 = ((h2 >> 12) | (h3 << 14));
   h3 = ((h3 >> 18) | (h4 <<  8));

   /* tag = (h + pad) % 2^128 */
   f = (uint64_t)h0 + ctx->pad[0];             h0 = (uint32_t)f;
   f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
   f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
   f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

   crypto_store32_le(tag,      h0);
   crypto_store32_le(tag +  4, h1);
   crypto_store32_le(tag +  8, h2);
   crypto_store32_le(tag + 12, h3);

   crypto_memzero(ctx, sizeof(*ctx));
}

void poly1305_auth(uint8_t *tag, const uint8_t *m, size_t len,
      const uint8_t *key)
{
   struct poly1305_ctx ctx;
   poly1305_init(&ctx, key);
   poly1305_update(&ctx, m, len);
   poly1305_final(&ctx, tag);
}

static const uint8_t aead_zero[16] = { 0 };

/* Poly1305 over pad16(aad) || pad16(ct) || le64(aad_len) || le64(ct_len),
 * keyed with ChaCha20 block 0. */
static void aead_chacha_tag(const uint8_t *key, const uint8_t *nonce,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len, uint8_t *tag)
{
   struct poly1305_ctx p;
   uint8_t otk[64];
   uint8_t lens[16];

   chacha20_block(key, nonce, 0, otk);
   poly1305_init(&p, otk);
   poly1305_update(&p, aad, aad_len);
   if (aad_len & 15)
      poly1305_update(&p, aead_zero, 16 - (aad_len & 15));
   poly1305_update(&p, ct, ct_len);
   if (ct_len & 15)
      poly1305_update(&p, aead_zero, 16 - (ct_len & 15));
   crypto_store64_le(lens,     (uint64_t)aad_len);
   crypto_store64_le(lens + 8, (uint64_t)ct_len);
   poly1305_update(&p, lens, 16);
   poly1305_final(&p, tag);

   crypto_memzero(otk, sizeof(otk));
}

int aead_encrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *pt, size_t pt_len,
      uint8_t *ct, uint8_t *tag, size_t tag_len)
{
   if (key_len != AEAD_KEY_SIZE || nonce_len != AEAD_NONCE_SIZE
         || tag_len != AEAD_TAG_SIZE)
      return -1;

   switch (alg)
   {
      case AEAD_CHACHA20_POLY1305:
         chacha20_xor(key, nonce, 1, pt, ct, pt_len);
         aead_chacha_tag(key, nonce, aad, aad_len, ct, pt_len, tag);
         return 0;
      case AEAD_AES256_GCM:
      {
         struct aes_gcm_ctx g;
         int ret = aes_gcm_init(&g, key, key_len);
         if (ret == 0)
            ret = aes_gcm_encrypt(&g, nonce, nonce_len, aad, aad_len,
                  pt, pt_len, ct, tag);
         crypto_memzero(&g, sizeof(g));
         return ret;
      }
      default:
         break;
   }
   return -1;
}

int aead_decrypt(enum aead_alg alg,
      const uint8_t *key, size_t key_len,
      const uint8_t *nonce, size_t nonce_len,
      const uint8_t *aad, size_t aad_len,
      const uint8_t *ct, size_t ct_len,
      const uint8_t *tag, size_t tag_len,
      uint8_t *pt)
{
   if (key_len != AEAD_KEY_SIZE || nonce_len != AEAD_NONCE_SIZE
         || tag_len != AEAD_TAG_SIZE)
      return -1;

   switch (alg)
   {
      case AEAD_CHACHA20_POLY1305:
      {
         uint8_t calc[16];
         int ok;
         aead_chacha_tag(key, nonce, aad, aad_len, ct, ct_len, calc);
         ok = crypto_memeq_ct(calc, tag, 16);
         crypto_memzero(calc, sizeof(calc));
         if (!ok)
            return -2;
         chacha20_xor(key, nonce, 1, ct, pt, ct_len);
         return 0;
      }
      case AEAD_AES256_GCM:
      {
         struct aes_gcm_ctx g;
         int ret = aes_gcm_init(&g, key, key_len);
         if (ret == 0)
            ret = aes_gcm_decrypt(&g, nonce, nonce_len, aad, aad_len,
                  ct, ct_len, tag, pt) ? -2 : 0;
         crypto_memzero(&g, sizeof(g));
         return ret;
      }
      default:
         break;
   }
   return -1;
}

