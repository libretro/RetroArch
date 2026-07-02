/* rmp3 - MP3 decoder implementation (minimp3-derived).
 * Declarations live in <formats/rmp3.h>; this file is the implementation. */
#include <formats/rmp3.h>
#include <features/features_cpu.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h> /* For INT_MAX */

#define RMP3_MAX_FREE_FORMAT_FRAME_SIZE  2304    /* more than ISO spec's */
#define RMP3_MAX_FRAME_SYNC_MATCHES      10

#define RMP3_MAX_L3_FRAME_PAYLOAD_BYTES  RMP3_MAX_FREE_FORMAT_FRAME_SIZE /* MUST be >= 320000/8/32000*1152 = 1440 */

#define RMP3_MAX_BITRESERVOIR_BYTES      511
#define RMP3_SHORT_BLOCK_TYPE            2
#define RMP3_STOP_BLOCK_TYPE             3
#define RMP3_MODE_MONO                   3
#define RMP3_MODE_JOINT_STEREO           1
#define RMP3_HDR_SIZE                    4
#define RMP3_HDR_IS_MONO(h)              (((h[3]) & 0xC0) == 0xC0)
#define RMP3_HDR_IS_MS_STEREO(h)         (((h[3]) & 0xE0) == 0x60)
#define RMP3_HDR_IS_FREE_FORMAT(h)       (((h[2]) & 0xF0) == 0)
#define RMP3_HDR_IS_CRC(h)               (!((h[1]) & 1))
#define RMP3_HDR_TEST_PADDING(h)         ((h[2]) & 0x2)
#define RMP3_HDR_TEST_MPEG1(h)           ((h[1]) & 0x8)
#define RMP3_HDR_TEST_NOT_MPEG25(h)      ((h[1]) & 0x10)
#define RMP3_HDR_TEST_I_STEREO(h)        ((h[3]) & 0x10)
#define RMP3_HDR_TEST_MS_STEREO(h)       ((h[3]) & 0x20)
#define RMP3_HDR_GET_STEREO_MODE(h)      (((h[3]) >> 6) & 3)
#define RMP3_HDR_GET_STEREO_MODE_EXT(h)  (((h[3]) >> 4) & 3)
#define RMP3_HDR_GET_LAYER(h)            (((h[1]) >> 1) & 3)
#define RMP3_HDR_GET_BITRATE(h)          ((h[2]) >> 4)
#define RMP3_HDR_GET_SAMPLE_RATE(h)      (((h[2]) >> 2) & 3)
#define RMP3_HDR_GET_MY_SAMPLE_RATE(h)   (RMP3_HDR_GET_SAMPLE_RATE(h) + (((h[1] >> 3) & 1) +  ((h[1] >> 4) & 1))*3)
#define RMP3_HDR_IS_FRAME_576(h)         ((h[1] & 14) == 2)
#define RMP3_HDR_IS_LAYER_1(h)           ((h[1] & 6) == 6)

#define RMP3_BITS_DEQUANTIZER_OUT        -1
#define RMP3_MAX_SCF                     (255 + RMP3_BITS_DEQUANTIZER_OUT*4 - 210)
#define RMP3_MAX_SCFI                    ((RMP3_MAX_SCF + 3) & ~3)

#define RMP3_MIN(a, b)           ((a) > (b) ? (b) : (a))
#define RMP3_MAX(a, b)           ((a) < (b) ? (b) : (a))

#if !defined(RMP3_NO_SIMD)

#if !defined(RMP3_ONLY_SIMD) && (defined(_M_X64) || defined(_M_ARM64) || defined(__x86_64__) || defined(__aarch64__))
/* x64 always have SSE2, arm64 always have neon, no need for generic code */
#define RMP3_ONLY_SIMD
#endif

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__))
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <immintrin.h>
#define RMP3_HAVE_SSE 1
#define RMP3_HAVE_SIMD 1
#define RMP3_VSTORE _mm_storeu_ps
#define RMP3_VLD _mm_loadu_ps
#define RMP3_VSET _mm_set1_ps
#define RMP3_VADD _mm_add_ps
#define RMP3_VSUB _mm_sub_ps
#define RMP3_VMUL _mm_mul_ps
#define RMP3_VMAC(a, x, y) _mm_add_ps(a, _mm_mul_ps(x, y))
#define RMP3_VMSB(a, x, y) _mm_sub_ps(a, _mm_mul_ps(x, y))
#define RMP3_VMUL_S(x, s)  _mm_mul_ps(x, _mm_set1_ps(s))
#define RMP3_VREV(x) _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 1, 2, 3))
typedef __m128 rmp3_f4;
static int rmp3_have_simd(void)
{
#ifdef RMP3_ONLY_SIMD
    return 1;
#else
    /* Detect SSE2 via libretro-common's shared, cached CPU feature query
     * rather than an in-tree cpuid. Memoised here so the SIMD dispatch in
     * the decode loops stays a plain branch. */
    static int g_have_simd; /* 0 = unknown, 1 = no SSE2, 2 = SSE2 */
    if (!g_have_simd)
        g_have_simd = (cpu_features_get() & RETRO_SIMD_SSE2) ? 2 : 1;
    return g_have_simd - 1;
#endif
}
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(__ARM_NEON__) || defined(_M_ARM64)
#include <arm_neon.h>
#define RMP3_HAVE_SIMD 1
#define RMP3_VSTORE vst1q_f32
#define RMP3_VLD vld1q_f32
#define RMP3_VSET vmovq_n_f32
#define RMP3_VADD vaddq_f32
#define RMP3_VSUB vsubq_f32
#define RMP3_VMUL vmulq_f32
#define RMP3_VMAC(a, x, y) vmlaq_f32(a, x, y)
#define RMP3_VMSB(a, x, y) vmlsq_f32(a, x, y)
#define RMP3_VMUL_S(x, s)  vmulq_f32(x, vmovq_n_f32(s))
#define RMP3_VREV(x) vcombine_f32(vget_high_f32(vrev64q_f32(x)), vget_low_f32(vrev64q_f32(x)))
typedef float32x4_t rmp3_f4;
static int rmp3_have_simd(void)
{   /* TODO: detect neon for !RMP3_ONLY_SIMD */
    return 1;
}
#else
#define RMP3_HAVE_SIMD 0
#ifdef RMP3_ONLY_SIMD
#error RMP3_ONLY_SIMD used, but SSE/NEON not enabled
#endif
#endif

#else

#define RMP3_HAVE_SIMD 0

#endif

typedef struct
{
    const uint8_t *buf;
    int pos;
    int limit;
} rmp3_bs;

typedef struct
{
    uint8_t total_bands;
    uint8_t stereo_bands;
    uint8_t bitalloc[64];
    uint8_t scfcod[64];
    float scf[3*64];
} rmp3_L12_scale_info;

typedef struct
{
    uint8_t tab_offset;
    uint8_t code_tab_width;
    uint8_t band_count;
} rmp3_L12_subband_alloc;

typedef struct
{
    const uint8_t *sfbtab;
    uint16_t part_23_length;
    uint16_t big_values;
    uint16_t scalefac_compress;
    uint8_t global_gain;
    uint8_t block_type;
    uint8_t mixed_block_flag;
    uint8_t n_long_sfb;
    uint8_t n_short_sfb;
    uint8_t table_select[3];
    uint8_t region_count[3];
    uint8_t subblock_gain[3];
    uint8_t preflag;
    uint8_t scalefac_scale;
    uint8_t count1_table;
    uint8_t scfsi;
} rmp3_L3_gr_info;

typedef struct
{
    rmp3_bs bs;
    uint8_t maindata[RMP3_MAX_BITRESERVOIR_BYTES + RMP3_MAX_L3_FRAME_PAYLOAD_BYTES];
    rmp3_L3_gr_info gr_info[4];
    float grbuf[2][576];
    float scf[40];
    uint8_t ist_pos[2][39];
    float syn[18 + 15][2*32];
} rmp3dec_scratch;

static void rmp3_bs_init(rmp3_bs *bs, const uint8_t *data, int bytes)
{
    bs->buf   = data;
    bs->pos   = 0;
    bs->limit = bytes*8;
}

static uint32_t rmp3_bs_get_bits(rmp3_bs *bs, int n)
{
    uint32_t next, cache = 0, s = bs->pos & 7;
    int shl = n + s;
    const uint8_t *p = bs->buf + (bs->pos >> 3);
    if ((bs->pos += n) > bs->limit)
        return 0;
    next = *p++ & (255 >> s);
    while ((shl -= 8) > 0)
    {
        cache |= next << shl;
        next = *p++;
    }
    return cache | (next >> -shl);
}

static int rmp3_hdr_valid(const uint8_t *h)
{
    return h[0] == 0xff &&
        ((h[1] & 0xF0) == 0xf0 || (h[1] & 0xFE) == 0xe2) &&
        (RMP3_HDR_GET_LAYER(h) != 0) &&
        (RMP3_HDR_GET_BITRATE(h) != 15) &&
        (RMP3_HDR_GET_SAMPLE_RATE(h) != 3);
}

static int rmp3_hdr_compare(const uint8_t *h1, const uint8_t *h2)
{
    return rmp3_hdr_valid(h2) &&
        ((h1[1] ^ h2[1]) & 0xFE) == 0 &&
        ((h1[2] ^ h2[2]) & 0x0C) == 0 &&
        !(RMP3_HDR_IS_FREE_FORMAT(h1) ^ RMP3_HDR_IS_FREE_FORMAT(h2));
}

static unsigned rmp3_hdr_bitrate_kbps(const uint8_t *h)
{
    static const uint8_t halfrate[2][3][15] = {
        { { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,16,24,28,32,40,48,56,64,72,80,88,96,112,128 } },
        { { 0,16,20,24,28,32,40,48,56,64,80,96,112,128,160 }, { 0,16,24,28,32,40,48,56,64,80,96,112,128,160,192 }, { 0,16,32,48,64,80,96,112,128,144,160,176,192,208,224 } },
    };
    return 2*halfrate[!!RMP3_HDR_TEST_MPEG1(h)][RMP3_HDR_GET_LAYER(h) - 1][RMP3_HDR_GET_BITRATE(h)];
}

static unsigned rmp3_hdr_sample_rate_hz(const uint8_t *h)
{
    static const unsigned g_hz[3] = { 44100, 48000, 32000 };
    return g_hz[RMP3_HDR_GET_SAMPLE_RATE(h)] >> (int)!RMP3_HDR_TEST_MPEG1(h) >> (int)!RMP3_HDR_TEST_NOT_MPEG25(h);
}

static unsigned rmp3_hdr_frame_samples(const uint8_t *h)
{
    return RMP3_HDR_IS_LAYER_1(h) ? 384 : (1152 >> (int)RMP3_HDR_IS_FRAME_576(h));
}

static int rmp3_hdr_frame_bytes(const uint8_t *h, int free_format_size)
{
    int frame_bytes = rmp3_hdr_frame_samples(h)*rmp3_hdr_bitrate_kbps(h)*125/rmp3_hdr_sample_rate_hz(h);
    if (RMP3_HDR_IS_LAYER_1(h))
        frame_bytes &= ~3; /* slot align */
    return frame_bytes ? frame_bytes : free_format_size;
}

static int rmp3_hdr_padding(const uint8_t *h)
{
    return RMP3_HDR_TEST_PADDING(h) ? (RMP3_HDR_IS_LAYER_1(h) ? 4 : 1) : 0;
}

static const rmp3_L12_subband_alloc *rmp3_L12_subband_alloc_table(const uint8_t *hdr, rmp3_L12_scale_info *sci)
{
    const rmp3_L12_subband_alloc *alloc;
    int mode = RMP3_HDR_GET_STEREO_MODE(hdr);
    int nbands, stereo_bands = (mode == RMP3_MODE_MONO) ? 0 : (mode == RMP3_MODE_JOINT_STEREO) ? (RMP3_HDR_GET_STEREO_MODE_EXT(hdr) << 2) + 4 : 32;

    if (RMP3_HDR_IS_LAYER_1(hdr))
    {
        static const rmp3_L12_subband_alloc g_alloc_L1[] = { { 76, 4, 32 } };
        alloc = g_alloc_L1;
        nbands = 32;
    }
    else if (!RMP3_HDR_TEST_MPEG1(hdr))
    {
        static const rmp3_L12_subband_alloc g_alloc_L2M2[] = { { 60, 4, 4 }, { 44, 3, 7 }, { 44, 2, 19 } };
        alloc = g_alloc_L2M2;
        nbands = 30;
    }
    else
    {
        static const rmp3_L12_subband_alloc g_alloc_L2M1[] = { { 0, 4, 3 }, { 16, 4, 8 }, { 32, 3, 12 }, { 40, 2, 7 } };
        int sample_rate_idx = RMP3_HDR_GET_SAMPLE_RATE(hdr);
        unsigned kbps = rmp3_hdr_bitrate_kbps(hdr) >> (int)(mode != RMP3_MODE_MONO);
        if (!kbps) /* free-format */
            kbps = 192;

        alloc = g_alloc_L2M1;
        nbands = 27;
        if (kbps < 56)
        {
            static const rmp3_L12_subband_alloc g_alloc_L2M1_lowrate[] = { { 44, 4, 2 }, { 44, 3, 10 } };
            alloc = g_alloc_L2M1_lowrate;
            nbands = sample_rate_idx == 2 ? 12 : 8;
        }
        else if (kbps >= 96 && sample_rate_idx != 1)
            nbands = 30;
    }

    sci->total_bands = (uint8_t)nbands;
    sci->stereo_bands = (uint8_t)RMP3_MIN(stereo_bands, nbands);

    return alloc;
}

static void rmp3_L12_read_scalefactors(rmp3_bs *bs, uint8_t *pba, uint8_t *scfcod, int bands, float *scf)
{
    static const float g_deq_L12[18*3] = {
#define RMP3_DQ(x) 9.53674316e-07f/x, 7.56931807e-07f/x, 6.00777173e-07f/x
        RMP3_DQ(3),RMP3_DQ(7),RMP3_DQ(15),RMP3_DQ(31),RMP3_DQ(63),RMP3_DQ(127),RMP3_DQ(255),RMP3_DQ(511),RMP3_DQ(1023),RMP3_DQ(2047),RMP3_DQ(4095),RMP3_DQ(8191),RMP3_DQ(16383),RMP3_DQ(32767),RMP3_DQ(65535),RMP3_DQ(3),RMP3_DQ(5),RMP3_DQ(9)
    };
    int i, m;
    for (i = 0; i < bands; i++)
    {
        float s = 0;
        int ba = *pba++;
        int mask = ba ? 4 + ((19 >> scfcod[i]) & 3) : 0;
        for (m = 4; m; m >>= 1)
        {
            if (mask & m)
            {
                int b = rmp3_bs_get_bits(bs, 6);
                s = g_deq_L12[ba*3 - 6 + b % 3]*(1 << 21 >> b/3);
            }
            *scf++ = s;
        }
    }
}

static void rmp3_L12_read_scale_info(const uint8_t *hdr, rmp3_bs *bs, rmp3_L12_scale_info *sci)
{
    static const uint8_t g_bitalloc_code_tab[] = {
        0,17, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,16,
        0,17,18, 3,19,4,5,16,
        0,17,18,16,
        0,17,18,19, 4,5,6, 7,8, 9,10,11,12,13,14,15,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,14,
        0, 2, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16
    };
    const rmp3_L12_subband_alloc *subband_alloc = rmp3_L12_subband_alloc_table(hdr, sci);

    int i, k = 0, ba_bits = 0;
    const uint8_t *ba_code_tab = g_bitalloc_code_tab;

    for (i = 0; i < sci->total_bands; i++)
    {
        uint8_t ba;
        if (i == k)
        {
            k += subband_alloc->band_count;
            ba_bits = subband_alloc->code_tab_width;
            ba_code_tab = g_bitalloc_code_tab + subband_alloc->tab_offset;
            subband_alloc++;
        }
        ba = ba_code_tab[rmp3_bs_get_bits(bs, ba_bits)];
        sci->bitalloc[2*i] = ba;
        if (i < sci->stereo_bands)
            ba = ba_code_tab[rmp3_bs_get_bits(bs, ba_bits)];
        sci->bitalloc[2*i + 1] = sci->stereo_bands ? ba : 0;
    }

    for (i = 0; i < 2*sci->total_bands; i++)
        sci->scfcod[i] = (uint8_t)(sci->bitalloc[i] ? RMP3_HDR_IS_LAYER_1(hdr) ? 2 : rmp3_bs_get_bits(bs, 2) : 6);

    rmp3_L12_read_scalefactors(bs, sci->bitalloc, sci->scfcod, sci->total_bands*2, sci->scf);

    for (i = sci->stereo_bands; i < sci->total_bands; i++)
        sci->bitalloc[2*i + 1] = 0;
}

static int rmp3_L12_dequantize_granule(float *grbuf, rmp3_bs *bs, rmp3_L12_scale_info *sci, int group_size)
{
    int i, j, k, choff = 576;
    for (j = 0; j < 4; j++)
    {
        float *dst = grbuf + group_size*j;
        for (i = 0; i < 2*sci->total_bands; i++)
        {
            int ba = sci->bitalloc[i];
            if (ba != 0)
            {
                if (ba < 17)
                {
                    int half = (1 << (ba - 1)) - 1;
                    for (k = 0; k < group_size; k++)
                    {
                        dst[k] = (float)((int)rmp3_bs_get_bits(bs, ba) - half);
                    }
                } else
                {
                    unsigned mod = (2 << (ba - 17)) + 1;    /* 3, 5, 9 */
                    unsigned code = rmp3_bs_get_bits(bs, mod + 2 - (mod >> 3));  /* 5, 7, 10 */
                    for (k = 0; k < group_size; k++, code /= mod)
                        dst[k] = (float)((int)(code % mod - mod/2));
                }
            }
            dst += choff;
            choff = 18 - choff;
        }
    }
    return group_size*4;
}

static void rmp3_L12_apply_scf_384(rmp3_L12_scale_info *sci,
      const float *scf, float *dst)
{
    int i, k;
    memcpy(dst + 576 + sci->stereo_bands*18, dst + sci->stereo_bands*18, (sci->total_bands - sci->stereo_bands)*18*sizeof(float));
    for (i = 0; i < sci->total_bands; i++, dst += 18, scf += 6)
    {
        for (k = 0; k < 12; k++)
        {
            dst[k + 0]   *= scf[0];
            dst[k + 576] *= scf[3];
        }
    }
}

static int rmp3_L3_read_side_info(rmp3_bs *bs, rmp3_L3_gr_info *gr, const uint8_t *hdr)
{
    static const uint8_t g_scf_long[9][23] = {
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,54,62,70,76,36,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 4,4,4,4,4,4,6,6,8,8,10,12,16,20,24,28,34,42,50,54,76,158,0 },
        { 4,4,4,4,4,4,6,6,6,8,10,12,16,18,22,28,34,40,46,54,54,192,0 },
        { 4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102,26,0 }
    };
    static const uint8_t g_scf_short[9][40] = {
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 8,8,8,8,8,8,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };
    static const uint8_t g_scf_mixed[9][40] = {
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 12,12,12,4,4,4,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 6,6,6,6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };

    unsigned tables, scfsi = 0;
    int main_data_begin, part_23_sum = 0;
    int sr_idx = RMP3_HDR_GET_MY_SAMPLE_RATE(hdr);
    int gr_count = RMP3_HDR_IS_MONO(hdr) ? 1 : 2;

    if (RMP3_HDR_TEST_MPEG1(hdr))
    {
        gr_count *= 2;
        main_data_begin = rmp3_bs_get_bits(bs, 9);
        scfsi = rmp3_bs_get_bits(bs, 7 + gr_count);
    }
    else
        main_data_begin = rmp3_bs_get_bits(bs, 8 + gr_count) >> gr_count;

    do
    {
        if (RMP3_HDR_IS_MONO(hdr))
            scfsi <<= 4;
        gr->part_23_length = (uint16_t)rmp3_bs_get_bits(bs, 12);
        part_23_sum += gr->part_23_length;
        gr->big_values = (uint16_t)rmp3_bs_get_bits(bs,  9);
        if (gr->big_values > 288)
            return -1;
        gr->global_gain = (uint8_t)rmp3_bs_get_bits(bs, 8);
        gr->scalefac_compress = (uint16_t)rmp3_bs_get_bits(bs, RMP3_HDR_TEST_MPEG1(hdr) ? 4 : 9);
        gr->sfbtab = g_scf_long[sr_idx];
        gr->n_long_sfb  = 22;
        gr->n_short_sfb = 0;
        if (rmp3_bs_get_bits(bs, 1))
        {
            gr->block_type = (uint8_t)rmp3_bs_get_bits(bs, 2);
            if (!gr->block_type)
                return -1;
            gr->mixed_block_flag = (uint8_t)rmp3_bs_get_bits(bs, 1);
            gr->region_count[0] = 7;
            gr->region_count[1] = 255;
            if (gr->block_type == RMP3_SHORT_BLOCK_TYPE)
            {
                scfsi &= 0x0F0F;
                if (!gr->mixed_block_flag)
                {
                    gr->region_count[0] = 8;
                    gr->sfbtab = g_scf_short[sr_idx];
                    gr->n_long_sfb = 0;
                    gr->n_short_sfb = 39;
                }
                else
                {
                    gr->sfbtab = g_scf_mixed[sr_idx];
                    gr->n_long_sfb = RMP3_HDR_TEST_MPEG1(hdr) ? 8 : 6;
                    gr->n_short_sfb = 30;
                }
            }
            tables = rmp3_bs_get_bits(bs, 10);
            tables <<= 5;
            gr->subblock_gain[0] = (uint8_t)rmp3_bs_get_bits(bs, 3);
            gr->subblock_gain[1] = (uint8_t)rmp3_bs_get_bits(bs, 3);
            gr->subblock_gain[2] = (uint8_t)rmp3_bs_get_bits(bs, 3);
        }
        else
        {
            gr->block_type = 0;
            gr->mixed_block_flag = 0;
            tables = rmp3_bs_get_bits(bs, 15);
            gr->region_count[0] = (uint8_t)rmp3_bs_get_bits(bs, 4);
            gr->region_count[1] = (uint8_t)rmp3_bs_get_bits(bs, 3);
            gr->region_count[2] = 255;
        }
        gr->table_select[0] = (uint8_t)(tables >> 10);
        gr->table_select[1] = (uint8_t)((tables >> 5) & 31);
        gr->table_select[2] = (uint8_t)((tables) & 31);
        gr->preflag = (uint8_t)(RMP3_HDR_TEST_MPEG1(hdr) ? rmp3_bs_get_bits(bs, 1) : (gr->scalefac_compress >= 500));
        gr->scalefac_scale = (uint8_t)rmp3_bs_get_bits(bs, 1);
        gr->count1_table = (uint8_t)rmp3_bs_get_bits(bs, 1);
        gr->scfsi = (uint8_t)((scfsi >> 12) & 15);
        scfsi <<= 4;
        gr++;
    } while(--gr_count);

    if (part_23_sum + bs->pos > bs->limit + main_data_begin*8)
        return -1;
    return main_data_begin;
}

static void rmp3_L3_read_scalefactors(uint8_t *scf, uint8_t *ist_pos, const uint8_t *scf_size, const uint8_t *scf_count, rmp3_bs *bitbuf, int scfsi)
{
    int i, k;
    for (i = 0; i < 4 && scf_count[i]; i++, scfsi *= 2)
    {
        int cnt = scf_count[i];
        if (scfsi & 8)
            memcpy(scf, ist_pos, cnt);
        else
        {
            int bits = scf_size[i];
            if (!bits)
            {
                memset(scf, 0, cnt);
                memset(ist_pos, 0, cnt);
            }
            else
            {
                int max_scf = (scfsi < 0) ? (1 << bits) - 1 : -1;
                for (k = 0; k < cnt; k++)
                {
                    int s = rmp3_bs_get_bits(bitbuf, bits);
                    ist_pos[k] = (uint8_t)(s == max_scf ? -1 : s);
                    scf[k] = (uint8_t)s;
                }
            }
        }
        ist_pos += cnt;
        scf += cnt;
    }
    scf[0] = scf[1] = scf[2] = 0;
}

static float rmp3_L3_ldexp_q2(float y, int exp_q2)
{
    static const float g_expfrac[4] = { 9.31322575e-10f,7.83145814e-10f,6.58544508e-10f,5.53767716e-10f };
    int e;
    do
    {
        e = RMP3_MIN(30*4, exp_q2);
        y *= g_expfrac[e & 3]*(1 << 30 >> (e >> 2));
    } while ((exp_q2 -= e) > 0);
    return y;
}

static void rmp3_L3_decode_scalefactors(const uint8_t *hdr, uint8_t *ist_pos, rmp3_bs *bs, const rmp3_L3_gr_info *gr, float *scf, int ch)
{
    static const uint8_t g_scf_partitions[3][28] = {
        { 6,5,5, 5,6,5,5,5,6,5, 7,3,11,10,0,0, 7, 7, 7,0, 6, 6,6,3, 8, 8,5,0 },
        { 8,9,6,12,6,9,9,9,6,9,12,6,15,18,0,0, 6,15,12,0, 6,12,9,6, 6,18,9,0 },
        { 9,9,6,12,9,9,9,9,9,9,12,6,18,18,0,0,12,12,12,0,12, 9,9,6,15,12,9,0 }
    };
    const uint8_t *scf_partition = g_scf_partitions[!!gr->n_short_sfb + !gr->n_long_sfb];
    uint8_t scf_size[4], iscf[40];
    int i, scf_shift = gr->scalefac_scale + 1, gain_exp, scfsi = gr->scfsi;
    float gain;

    if (RMP3_HDR_TEST_MPEG1(hdr))
    {
        static const uint8_t g_scfc_decode[16] = { 0,1,2,3, 12,5,6,7, 9,10,11,13, 14,15,18,19 };
        int part = g_scfc_decode[gr->scalefac_compress];
        scf_size[1] = scf_size[0] = (uint8_t)(part >> 2);
        scf_size[3] = scf_size[2] = (uint8_t)(part & 3);
    }
    else
    {
        static const uint8_t g_mod[6*4] = { 5,5,4,4,5,5,4,1,4,3,1,1,5,6,6,1,4,4,4,1,4,3,1,1 };
        int k, modprod, sfc, ist = RMP3_HDR_TEST_I_STEREO(hdr) && ch;
        sfc = gr->scalefac_compress >> ist;
        for (k = ist*3*4; sfc >= 0; sfc -= modprod, k += 4)
        {
            for (modprod = 1, i = 3; i >= 0; i--)
            {
                scf_size[i] = (uint8_t)(sfc / modprod % g_mod[k + i]);
                modprod *= g_mod[k + i];
            }
        }
        scf_partition += k;
        scfsi = -16;
    }
    rmp3_L3_read_scalefactors(iscf, ist_pos, scf_size, scf_partition, bs, scfsi);

    if (gr->n_short_sfb)
    {
        int sh = 3 - scf_shift;
        for (i = 0; i < gr->n_short_sfb; i += 3)
        {
            iscf[gr->n_long_sfb + i + 0] += gr->subblock_gain[0] << sh;
            iscf[gr->n_long_sfb + i + 1] += gr->subblock_gain[1] << sh;
            iscf[gr->n_long_sfb + i + 2] += gr->subblock_gain[2] << sh;
        }
    } else if (gr->preflag)
    {
        static const uint8_t g_preamp[10] = { 1,1,1,1,2,2,3,3,3,2 };
        for (i = 0; i < 10; i++)
            iscf[11 + i] += g_preamp[i];
    }

    gain_exp = gr->global_gain + RMP3_BITS_DEQUANTIZER_OUT*4 - 210 - (RMP3_HDR_IS_MS_STEREO(hdr) ? 2 : 0);
    gain = rmp3_L3_ldexp_q2(1 << (RMP3_MAX_SCFI/4),  RMP3_MAX_SCFI - gain_exp);
    for (i = 0; i < (int)(gr->n_long_sfb + gr->n_short_sfb); i++)
        scf[i] = rmp3_L3_ldexp_q2(gain, iscf[i] << scf_shift);
}

static float rmp3_L3_pow_43(int x)
{
    static const float g_pow43[129] = {
        0,1,2.519842f,4.326749f,6.349604f,8.549880f,10.902724f,13.390518f,16.000000f,18.720754f,21.544347f,24.463781f,27.473142f,30.567351f,33.741992f,36.993181f,40.317474f,43.711787f,47.173345f,50.699631f,54.288352f,57.937408f,61.644865f,65.408941f,69.227979f,73.100443f,77.024898f,81.000000f,85.024491f,89.097188f,93.216975f,97.382800f,101.593667f,105.848633f,110.146801f,114.487321f,118.869381f,123.292209f,127.755065f,132.257246f,136.798076f,141.376907f,145.993119f,150.646117f,155.335327f,160.060199f,164.820202f,169.614826f,174.443577f,179.305980f,184.201575f,189.129918f,194.090580f,199.083145f,204.107210f,209.162385f,214.248292f,219.364564f,224.510845f,229.686789f,234.892058f,240.126328f,245.389280f,250.680604f,256.000000f,261.347174f,266.721841f,272.123723f,277.552547f,283.008049f,288.489971f,293.998060f,299.532071f,305.091761f,310.676898f,316.287249f,321.922592f,327.582707f,333.267377f,338.976394f,344.709550f,350.466646f,356.247482f,362.051866f,367.879608f,373.730522f,379.604427f,385.501143f,391.420496f,397.362314f,403.326427f,409.312672f,415.320884f,421.350905f,427.402579f,433.475750f,439.570269f,445.685987f,451.822757f,457.980436f,464.158883f,470.357960f,476.577530f,482.817459f,489.077615f,495.357868f,501.658090f,507.978156f,514.317941f,520.677324f,527.056184f,533.454404f,539.871867f,546.308458f,552.764065f,559.238575f,565.731879f,572.243870f,578.774440f,585.323483f,591.890898f,598.476581f,605.080431f,611.702349f,618.342238f,625.000000f,631.675540f,638.368763f,645.079578f
    };
    float frac;
    int sign, mult = 256;

    if (x < 129)
    {
        return g_pow43[x];
    }

    if (x < 1024)
    {
        mult = 16;
        x <<= 3;
    }

    sign = 2*x & 64;
    frac = (float)((x & 63) - sign) / ((x & ~63) + sign);
    return g_pow43[(x + sign) >> 6]*(1.f + frac*((4.f/3) + frac*(2.f/9)))*mult;
}

static void rmp3_L3_huffman(float *dst, rmp3_bs *bs, const rmp3_L3_gr_info *gr_info, const float *scf, int layer3gr_limit)
{
    static const float g_pow43_signed[32] = { 0,0,1,-1,2.519842f,-2.519842f,4.326749f,-4.326749f,6.349604f,-6.349604f,8.549880f,-8.549880f,10.902724f,-10.902724f,13.390518f,-13.390518f,16.000000f,-16.000000f,18.720754f,-18.720754f,21.544347f,-21.544347f,24.463781f,-24.463781f,27.473142f,-27.473142f,30.567351f,-30.567351f,33.741992f,-33.741992f,36.993181f,-36.993181f };
    static const int16_t tab0[32] = { 0, };
    static const int16_t tab1[] = { 785,785,785,785,784,784,784,784,513,513,513,513,513,513,513,513,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256 };
    static const int16_t tab2[] = { -255,1313,1298,1282,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,290,288 };
    static const int16_t tab3[] = { -255,1313,1298,1282,769,769,769,769,529,529,529,529,529,529,529,529,528,528,528,528,528,528,528,528,512,512,512,512,512,512,512,512,290,288 };
    static const int16_t tab5[] = { -253,-318,-351,-367,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,819,818,547,547,275,275,275,275,561,560,515,546,289,274,288,258 };
    static const int16_t tab6[] = { -254,-287,1329,1299,1314,1312,1057,1057,1042,1042,1026,1026,784,784,784,784,529,529,529,529,529,529,529,529,769,769,769,769,768,768,768,768,563,560,306,306,291,259 };
    static const int16_t tab7[] = { -252,-413,-477,-542,1298,-575,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-383,-399,1107,1092,1106,1061,849,849,789,789,1104,1091,773,773,1076,1075,341,340,325,309,834,804,577,577,532,532,516,516,832,818,803,816,561,561,531,531,515,546,289,289,288,258 };
    static const int16_t tab8[] = { -252,-429,-493,-559,1057,1057,1042,1042,529,529,529,529,529,529,529,529,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,-382,1077,-415,1106,1061,1104,849,849,789,789,1091,1076,1029,1075,834,834,597,581,340,340,339,324,804,833,532,532,832,772,818,803,817,787,816,771,290,290,290,290,288,258 };
    static const int16_t tab9[] = { -253,-349,-414,-447,-463,1329,1299,-479,1314,1312,1057,1057,1042,1042,1026,1026,785,785,785,785,784,784,784,784,769,769,769,769,768,768,768,768,-319,851,821,-335,836,850,805,849,341,340,325,336,533,533,579,579,564,564,773,832,578,548,563,516,321,276,306,291,304,259 };
    static const int16_t tab10[] = { -251,-572,-733,-830,-863,-879,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,1396,1351,1381,1366,1395,1335,1380,-559,1334,1138,1138,1063,1063,1350,1392,1031,1031,1062,1062,1364,1363,1120,1120,1333,1348,881,881,881,881,375,374,359,373,343,358,341,325,791,791,1123,1122,-703,1105,1045,-719,865,865,790,790,774,774,1104,1029,338,293,323,308,-799,-815,833,788,772,818,803,816,322,292,307,320,561,531,515,546,289,274,288,258 };
    static const int16_t tab11[] = { -251,-525,-605,-685,-765,-831,-846,1298,1057,1057,1312,1282,785,785,785,785,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,1399,1398,1383,1367,1382,1396,1351,-511,1381,1366,1139,1139,1079,1079,1124,1124,1364,1349,1363,1333,882,882,882,882,807,807,807,807,1094,1094,1136,1136,373,341,535,535,881,775,867,822,774,-591,324,338,-671,849,550,550,866,864,609,609,293,336,534,534,789,835,773,-751,834,804,308,307,833,788,832,772,562,562,547,547,305,275,560,515,290,290 };
    static const int16_t tab12[] = { -252,-397,-477,-557,-622,-653,-719,-735,-750,1329,1299,1314,1057,1057,1042,1042,1312,1282,1024,1024,785,785,785,785,784,784,784,784,769,769,769,769,-383,1127,1141,1111,1126,1140,1095,1110,869,869,883,883,1079,1109,882,882,375,374,807,868,838,881,791,-463,867,822,368,263,852,837,836,-543,610,610,550,550,352,336,534,534,865,774,851,821,850,805,593,533,579,564,773,832,578,578,548,548,577,577,307,276,306,291,516,560,259,259 };
    static const int16_t tab13[] = { -250,-2107,-2507,-2764,-2909,-2974,-3007,-3023,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-767,-1052,-1213,-1277,-1358,-1405,-1469,-1535,-1550,-1582,-1614,-1647,-1662,-1694,-1726,-1759,-1774,-1807,-1822,-1854,-1886,1565,-1919,-1935,-1951,-1967,1731,1730,1580,1717,-1983,1729,1564,-1999,1548,-2015,-2031,1715,1595,-2047,1714,-2063,1610,-2079,1609,-2095,1323,1323,1457,1457,1307,1307,1712,1547,1641,1700,1699,1594,1685,1625,1442,1442,1322,1322,-780,-973,-910,1279,1278,1277,1262,1276,1261,1275,1215,1260,1229,-959,974,974,989,989,-943,735,478,478,495,463,506,414,-1039,1003,958,1017,927,942,987,957,431,476,1272,1167,1228,-1183,1256,-1199,895,895,941,941,1242,1227,1212,1135,1014,1014,490,489,503,487,910,1013,985,925,863,894,970,955,1012,847,-1343,831,755,755,984,909,428,366,754,559,-1391,752,486,457,924,997,698,698,983,893,740,740,908,877,739,739,667,667,953,938,497,287,271,271,683,606,590,712,726,574,302,302,738,736,481,286,526,725,605,711,636,724,696,651,589,681,666,710,364,467,573,695,466,466,301,465,379,379,709,604,665,679,316,316,634,633,436,436,464,269,424,394,452,332,438,363,347,408,393,448,331,422,362,407,392,421,346,406,391,376,375,359,1441,1306,-2367,1290,-2383,1337,-2399,-2415,1426,1321,-2431,1411,1336,-2447,-2463,-2479,1169,1169,1049,1049,1424,1289,1412,1352,1319,-2495,1154,1154,1064,1064,1153,1153,416,390,360,404,403,389,344,374,373,343,358,372,327,357,342,311,356,326,1395,1394,1137,1137,1047,1047,1365,1392,1287,1379,1334,1364,1349,1378,1318,1363,792,792,792,792,1152,1152,1032,1032,1121,1121,1046,1046,1120,1120,1030,1030,-2895,1106,1061,1104,849,849,789,789,1091,1076,1029,1090,1060,1075,833,833,309,324,532,532,832,772,818,803,561,561,531,560,515,546,289,274,288,258 };
    static const int16_t tab15[] = { -250,-1179,-1579,-1836,-1996,-2124,-2253,-2333,-2413,-2477,-2542,-2574,-2607,-2622,-2655,1314,1313,1298,1312,1282,785,785,785,785,1040,1040,1025,1025,768,768,768,768,-766,-798,-830,-862,-895,-911,-927,-943,-959,-975,-991,-1007,-1023,-1039,-1055,-1070,1724,1647,-1103,-1119,1631,1767,1662,1738,1708,1723,-1135,1780,1615,1779,1599,1677,1646,1778,1583,-1151,1777,1567,1737,1692,1765,1722,1707,1630,1751,1661,1764,1614,1736,1676,1763,1750,1645,1598,1721,1691,1762,1706,1582,1761,1566,-1167,1749,1629,767,766,751,765,494,494,735,764,719,749,734,763,447,447,748,718,477,506,431,491,446,476,461,505,415,430,475,445,504,399,460,489,414,503,383,474,429,459,502,502,746,752,488,398,501,473,413,472,486,271,480,270,-1439,-1455,1357,-1471,-1487,-1503,1341,1325,-1519,1489,1463,1403,1309,-1535,1372,1448,1418,1476,1356,1462,1387,-1551,1475,1340,1447,1402,1386,-1567,1068,1068,1474,1461,455,380,468,440,395,425,410,454,364,467,466,464,453,269,409,448,268,432,1371,1473,1432,1417,1308,1460,1355,1446,1459,1431,1083,1083,1401,1416,1458,1445,1067,1067,1370,1457,1051,1051,1291,1430,1385,1444,1354,1415,1400,1443,1082,1082,1173,1113,1186,1066,1185,1050,-1967,1158,1128,1172,1097,1171,1081,-1983,1157,1112,416,266,375,400,1170,1142,1127,1065,793,793,1169,1033,1156,1096,1141,1111,1155,1080,1126,1140,898,898,808,808,897,897,792,792,1095,1152,1032,1125,1110,1139,1079,1124,882,807,838,881,853,791,-2319,867,368,263,822,852,837,866,806,865,-2399,851,352,262,534,534,821,836,594,594,549,549,593,593,533,533,848,773,579,579,564,578,548,563,276,276,577,576,306,291,516,560,305,305,275,259 };
    static const int16_t tab16[] = { -251,-892,-2058,-2620,-2828,-2957,-3023,-3039,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,-559,1530,-575,-591,1528,1527,1407,1526,1391,1023,1023,1023,1023,1525,1375,1268,1268,1103,1103,1087,1087,1039,1039,1523,-604,815,815,815,815,510,495,509,479,508,463,507,447,431,505,415,399,-734,-782,1262,-815,1259,1244,-831,1258,1228,-847,-863,1196,-879,1253,987,987,748,-767,493,493,462,477,414,414,686,669,478,446,461,445,474,429,487,458,412,471,1266,1264,1009,1009,799,799,-1019,-1276,-1452,-1581,-1677,-1757,-1821,-1886,-1933,-1997,1257,1257,1483,1468,1512,1422,1497,1406,1467,1496,1421,1510,1134,1134,1225,1225,1466,1451,1374,1405,1252,1252,1358,1480,1164,1164,1251,1251,1238,1238,1389,1465,-1407,1054,1101,-1423,1207,-1439,830,830,1248,1038,1237,1117,1223,1148,1236,1208,411,426,395,410,379,269,1193,1222,1132,1235,1221,1116,976,976,1192,1162,1177,1220,1131,1191,963,963,-1647,961,780,-1663,558,558,994,993,437,408,393,407,829,978,813,797,947,-1743,721,721,377,392,844,950,828,890,706,706,812,859,796,960,948,843,934,874,571,571,-1919,690,555,689,421,346,539,539,944,779,918,873,932,842,903,888,570,570,931,917,674,674,-2575,1562,-2591,1609,-2607,1654,1322,1322,1441,1441,1696,1546,1683,1593,1669,1624,1426,1426,1321,1321,1639,1680,1425,1425,1305,1305,1545,1668,1608,1623,1667,1592,1638,1666,1320,1320,1652,1607,1409,1409,1304,1304,1288,1288,1664,1637,1395,1395,1335,1335,1622,1636,1394,1394,1319,1319,1606,1621,1392,1392,1137,1137,1137,1137,345,390,360,375,404,373,1047,-2751,-2767,-2783,1062,1121,1046,-2799,1077,-2815,1106,1061,789,789,1105,1104,263,355,310,340,325,354,352,262,339,324,1091,1076,1029,1090,1060,1075,833,833,788,788,1088,1028,818,818,803,803,561,561,531,531,816,771,546,546,289,274,288,258 };
    static const int16_t tab24[] = { -253,-317,-381,-446,-478,-509,1279,1279,-811,-1179,-1451,-1756,-1900,-2028,-2189,-2253,-2333,-2414,-2445,-2511,-2526,1313,1298,-2559,1041,1041,1040,1040,1025,1025,1024,1024,1022,1007,1021,991,1020,975,1019,959,687,687,1018,1017,671,671,655,655,1016,1015,639,639,758,758,623,623,757,607,756,591,755,575,754,559,543,543,1009,783,-575,-621,-685,-749,496,-590,750,749,734,748,974,989,1003,958,988,973,1002,942,987,957,972,1001,926,986,941,971,956,1000,910,985,925,999,894,970,-1071,-1087,-1102,1390,-1135,1436,1509,1451,1374,-1151,1405,1358,1480,1420,-1167,1507,1494,1389,1342,1465,1435,1450,1326,1505,1310,1493,1373,1479,1404,1492,1464,1419,428,443,472,397,736,526,464,464,486,457,442,471,484,482,1357,1449,1434,1478,1388,1491,1341,1490,1325,1489,1463,1403,1309,1477,1372,1448,1418,1433,1476,1356,1462,1387,-1439,1475,1340,1447,1402,1474,1324,1461,1371,1473,269,448,1432,1417,1308,1460,-1711,1459,-1727,1441,1099,1099,1446,1386,1431,1401,-1743,1289,1083,1083,1160,1160,1458,1445,1067,1067,1370,1457,1307,1430,1129,1129,1098,1098,268,432,267,416,266,400,-1887,1144,1187,1082,1173,1113,1186,1066,1050,1158,1128,1143,1172,1097,1171,1081,420,391,1157,1112,1170,1142,1127,1065,1169,1049,1156,1096,1141,1111,1155,1080,1126,1154,1064,1153,1140,1095,1048,-2159,1125,1110,1137,-2175,823,823,1139,1138,807,807,384,264,368,263,868,838,853,791,867,822,852,837,866,806,865,790,-2319,851,821,836,352,262,850,805,849,-2399,533,533,835,820,336,261,578,548,563,577,532,532,832,772,562,562,547,547,305,275,560,515,290,290,288,258 };
    static const uint8_t tab32[] = { 130,162,193,209,44,28,76,140,9,9,9,9,9,9,9,9,190,254,222,238,126,94,157,157,109,61,173,205};
    static const uint8_t tab33[] = { 252,236,220,204,188,172,156,140,124,108,92,76,60,44,28,12 };
    static const int16_t * const tabindex[2*16] = { tab0,tab1,tab2,tab3,tab0,tab5,tab6,tab7,tab8,tab9,tab10,tab11,tab12,tab13,tab0,tab15,tab16,tab16,tab16,tab16,tab16,tab16,tab16,tab16,tab24,tab24,tab24,tab24,tab24,tab24,tab24,tab24 };
    static const uint8_t g_linbits[] =  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,6,8,10,13,4,5,6,7,8,9,11,13 };

#define RMP3_PEEK_BITS(n)    (bs_cache >> (32 - n))
#define RMP3_FLUSH_BITS(n)   { bs_cache <<= (n); bs_sh += (n); }
#define RMP3_CHECK_BITS      while (bs_sh >= 0) { bs_cache |= (uint32_t)*bs_next_ptr++ << bs_sh; bs_sh -= 8; }
#define RMP3_BSPOS           ((bs_next_ptr - bs->buf)*8 - 24 + bs_sh)

    float one = 0.0f;
    int ireg = 0, big_val_cnt = gr_info->big_values;
    const uint8_t *sfb = gr_info->sfbtab;
    const uint8_t *bs_next_ptr = bs->buf + bs->pos/8;
    uint32_t bs_cache = (((bs_next_ptr[0]*256u + bs_next_ptr[1])*256u + bs_next_ptr[2])*256u + bs_next_ptr[3]) << (bs->pos & 7);
    int pairs_to_decode, np, bs_sh = (bs->pos & 7) - 8;
    bs_next_ptr += 4;

    while (big_val_cnt > 0)
    {
        int tab_num = gr_info->table_select[ireg];
        int sfb_cnt = gr_info->region_count[ireg++];
        const short *codebook = tabindex[tab_num];
        int linbits = g_linbits[tab_num];
        do
        {
            np = *sfb++ / 2;
            pairs_to_decode = RMP3_MIN(big_val_cnt, np);
            one = *scf++;
            do
            {
                int j, w = 5;
                int leaf = codebook[RMP3_PEEK_BITS(w)];
                while (leaf < 0)
                {
                    RMP3_FLUSH_BITS(w);
                    w = leaf & 7;
                    leaf = codebook[RMP3_PEEK_BITS(w) - (leaf >> 3)];
                }
                RMP3_FLUSH_BITS(leaf >> 8);

                for (j = 0; j < 2; j++, dst++, leaf >>= 4)
                {
                    int lsb = leaf & 0x0F;
                    if (lsb == 15 && linbits)
                    {
                        lsb += RMP3_PEEK_BITS(linbits);
                        RMP3_FLUSH_BITS(linbits);
                        RMP3_CHECK_BITS;
                        *dst = one*rmp3_L3_pow_43(lsb)*((int32_t)bs_cache < 0 ? -1: 1);
                    } else
                    {
                        *dst = g_pow43_signed[lsb*2 + (bs_cache >> 31)]*one;
                    }
                    RMP3_FLUSH_BITS(lsb ? 1 : 0);
                }
                RMP3_CHECK_BITS;
            } while (--pairs_to_decode);
        } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
    }

    for (np = 1 - big_val_cnt;; dst += 4)
    {
        const uint8_t *codebook_count1 = (gr_info->count1_table) ? tab33 : tab32;
        int leaf = codebook_count1[RMP3_PEEK_BITS(4)];
        if (!(leaf & 8))
            leaf = codebook_count1[(leaf >> 3) + (bs_cache << 4 >> (32 - (leaf & 3)))];
        RMP3_FLUSH_BITS(leaf & 7);
        if (RMP3_BSPOS > layer3gr_limit)
            break;
#define RMP3_RELOAD_SCALEFACTOR  if (!--np) { np = *sfb++/2; if (!np) break; one = *scf++; }
#define RMP3_DEQ_COUNT1(s) if (leaf & (128 >> s)) { dst[s] = ((int32_t)bs_cache < 0) ? -one : one; RMP3_FLUSH_BITS(1) }
        RMP3_RELOAD_SCALEFACTOR;
        RMP3_DEQ_COUNT1(0);
        RMP3_DEQ_COUNT1(1);
        RMP3_RELOAD_SCALEFACTOR;
        RMP3_DEQ_COUNT1(2);
        RMP3_DEQ_COUNT1(3);
        RMP3_CHECK_BITS;
    }

    bs->pos = layer3gr_limit;
}

static void rmp3_L3_midside_stereo(float *left, int n)
{
    int i = 0;
    float *right = left + 576;
#if RMP3_HAVE_SIMD
    if (rmp3_have_simd())
    {
        for (; i < n - 3; i += 4)
        {
            rmp3_f4 vl = RMP3_VLD(left + i);
            rmp3_f4 vr = RMP3_VLD(right + i);
            RMP3_VSTORE(left + i, RMP3_VADD(vl, vr));
            RMP3_VSTORE(right + i, RMP3_VSUB(vl, vr));
        }
#ifdef __GNUC__
        /* Workaround for spurious -Waggressive-loop-optimizations warning from gcc.
         * For more info see: https://github.com/lieff/minimp3/issues/88
         */
        if (__builtin_constant_p(n % 4 == 0) && n % 4 == 0)
            return;
#endif
    }
#endif
    for (; i < n; i++)
    {
        float a = left[i];
        float b = right[i];
        left[i] = a + b;
        right[i] = a - b;
    }
}

static void rmp3_L3_intensity_stereo_band(
      float *left, int n, float kl, float kr)
{
    int i;
    for (i = 0; i < n; i++)
    {
        left[i + 576] = left[i]*kr;
        left[i] = left[i]*kl;
    }
}

static void rmp3_L3_stereo_top_band(
      const float *right, const uint8_t *sfb,
      int nbands, int max_band[3])
{
    int i, k;

    max_band[0] = max_band[1] = max_band[2] = -1;

    for (i = 0; i < nbands; i++)
    {
        for (k = 0; k < sfb[i]; k += 2)
        {
            if (right[k] != 0 || right[k + 1] != 0)
            {
                max_band[i % 3] = i;
                break;
            }
        }
        right += sfb[i];
    }
}

static void rmp3_L3_stereo_process(
      float *left, const uint8_t *ist_pos, const uint8_t *sfb,
      const uint8_t *hdr, int max_band[3], int mpeg2_sh)
{
    static const float g_pan[7*2] = { 0,1,0.21132487f,0.78867513f,0.36602540f,0.63397460f,0.5f,0.5f,0.63397460f,0.36602540f,0.78867513f,0.21132487f,1,0 };
    unsigned i, max_pos = RMP3_HDR_TEST_MPEG1(hdr) ? 7 : 64;

    for (i = 0; sfb[i]; i++)
    {
        unsigned ipos = ist_pos[i];
        if ((int)i > max_band[i % 3] && ipos < max_pos)
        {
            float kl, kr, s = RMP3_HDR_TEST_MS_STEREO(hdr) ? 1.41421356f : 1;
            if (RMP3_HDR_TEST_MPEG1(hdr))
            {
                kl = g_pan[2*ipos];
                kr = g_pan[2*ipos + 1];
            } else
            {
                kl = 1;
                kr = rmp3_L3_ldexp_q2(1, (ipos + 1) >> 1 << mpeg2_sh);
                if (ipos & 1)
                {
                    kl = kr;
                    kr = 1;
                }
            }
            rmp3_L3_intensity_stereo_band(left, sfb[i], kl*s, kr*s);
        } else if (RMP3_HDR_TEST_MS_STEREO(hdr))
            rmp3_L3_midside_stereo(left, sfb[i]);
        left += sfb[i];
    }
}

static void rmp3_L3_intensity_stereo(
      float *left, uint8_t *ist_pos, const rmp3_L3_gr_info *gr,
      const uint8_t *hdr)
{
    int max_band[3], n_sfb = gr->n_long_sfb + gr->n_short_sfb;
    int i, max_blocks = gr->n_short_sfb ? 3 : 1;

    rmp3_L3_stereo_top_band(left + 576, gr->sfbtab, n_sfb, max_band);
    if (gr->n_long_sfb)
    {
        max_band[0] = max_band[1] = max_band[2] = RMP3_MAX(RMP3_MAX(max_band[0], max_band[1]), max_band[2]);
    }
    for (i = 0; i < max_blocks; i++)
    {
        int default_pos = RMP3_HDR_TEST_MPEG1(hdr) ? 3 : 0;
        int itop = n_sfb - max_blocks + i;
        int prev = itop - max_blocks;
        ist_pos[itop] = (uint8_t)(max_band[i] >= prev ? default_pos : ist_pos[prev]);
    }
    rmp3_L3_stereo_process(left, ist_pos, gr->sfbtab, hdr, max_band, gr[1].scalefac_compress&1);
}

static void rmp3_L3_reorder(float *grbuf, float *scratch, const uint8_t *sfb)
{
    int i, len;
    float *src = grbuf, *dst = scratch;

    for (;0 != (len = *sfb); sfb += 3, src += 2*len)
    {
        for (i = 0; i < len; i++, src++)
        {
            *dst++ = src[0*len];
            *dst++ = src[1*len];
            *dst++ = src[2*len];
        }
    }
    memcpy(grbuf, scratch, (dst - scratch)*sizeof(float));
}

static void rmp3_L3_antialias(float *grbuf, int nbands)
{
    static const float g_aa[2][8] = {
        {0.85749293f,0.88174200f,0.94962865f,0.98331459f,0.99551782f,0.99916056f,0.99989920f,0.99999316f},
        {0.51449576f,0.47173197f,0.31337745f,0.18191320f,0.09457419f,0.04096558f,0.01419856f,0.00369997f}
    };

    for (; nbands > 0; nbands--, grbuf += 18)
    {
        int i = 0;
#if RMP3_HAVE_SIMD
        if (rmp3_have_simd()) for (; i < 8; i += 4)
        {
            rmp3_f4 vu = RMP3_VLD(grbuf + 18 + i);
            rmp3_f4 vd = RMP3_VLD(grbuf + 14 - i);
            rmp3_f4 vc0 = RMP3_VLD(g_aa[0] + i);
            rmp3_f4 vc1 = RMP3_VLD(g_aa[1] + i);
            vd = RMP3_VREV(vd);
            RMP3_VSTORE(grbuf + 18 + i, RMP3_VSUB(RMP3_VMUL(vu, vc0), RMP3_VMUL(vd, vc1)));
            vd = RMP3_VADD(RMP3_VMUL(vu, vc1), RMP3_VMUL(vd, vc0));
            RMP3_VSTORE(grbuf + 14 - i, RMP3_VREV(vd));
        }
#endif
#ifndef RMP3_ONLY_SIMD
        for(; i < 8; i++)
        {
            float u = grbuf[18 + i];
            float d = grbuf[17 - i];
            grbuf[18 + i] = u*g_aa[0][i] - d*g_aa[1][i];
            grbuf[17 - i] = u*g_aa[1][i] + d*g_aa[0][i];
        }
#endif
    }
}

static void rmp3_L3_dct3_9(float *y)
{
    float s0, s1, s2, s3, s4, s5, s6, s7, s8, t0, t2, t4;

    s0 = y[0]; s2 = y[2]; s4 = y[4]; s6 = y[6]; s8 = y[8];
    t0 = s0 + s6*0.5f;
    s0 -= s6;
    t4 = (s4 + s2)*0.93969262f;
    t2 = (s8 + s2)*0.76604444f;
    s6 = (s4 - s8)*0.17364818f;
    s4 += s8 - s2;

    s2 = s0 - s4*0.5f;
    y[4] = s4 + s0;
    s8 = t0 - t2 + s6;
    s0 = t0 - t4 + t2;
    s4 = t0 + t4 - s6;

    s1 = y[1]; s3 = y[3]; s5 = y[5]; s7 = y[7];

    s3 *= 0.86602540f;
    t0 = (s5 + s1)*0.98480775f;
    t4 = (s5 - s7)*0.34202014f;
    t2 = (s1 + s7)*0.64278761f;
    s1 = (s1 - s5 - s7)*0.86602540f;

    s5 = t0 - s3 - t2;
    s7 = t4 - s3 - t0;
    s3 = t4 + s3 - t2;

    y[0] = s4 - s7;
    y[1] = s2 + s1;
    y[2] = s0 - s3;
    y[3] = s8 + s5;
    y[5] = s8 - s5;
    y[6] = s0 + s3;
    y[7] = s2 - s1;
    y[8] = s4 + s7;
}

static void rmp3_L3_imdct36(float *grbuf, float *overlap, const float *window, int nbands)
{
    int i, j;
    static const float g_twid9[18] = {
        0.73727734f,0.79335334f,0.84339145f,0.88701083f,0.92387953f,0.95371695f,0.97629601f,0.99144486f,0.99904822f,0.67559021f,0.60876143f,0.53729961f,0.46174861f,0.38268343f,0.30070580f,0.21643961f,0.13052619f,0.04361938f
    };

    for (j = 0; j < nbands; j++, grbuf += 18, overlap += 9)
    {
        float co[9], si[9];
        co[0] = -grbuf[0];
        si[0] = grbuf[17];
        for (i = 0; i < 4; i++)
        {
            si[8 - 2*i] =   grbuf[4*i + 1] - grbuf[4*i + 2];
            co[1 + 2*i] =   grbuf[4*i + 1] + grbuf[4*i + 2];
            si[7 - 2*i] =   grbuf[4*i + 4] - grbuf[4*i + 3];
            co[2 + 2*i] = -(grbuf[4*i + 3] + grbuf[4*i + 4]);
        }
        rmp3_L3_dct3_9(co);
        rmp3_L3_dct3_9(si);

        si[1] = -si[1];
        si[3] = -si[3];
        si[5] = -si[5];
        si[7] = -si[7];

        i = 0;

#if RMP3_HAVE_SIMD
        if (rmp3_have_simd()) for (; i < 8; i += 4)
        {
            rmp3_f4 vovl = RMP3_VLD(overlap + i);
            rmp3_f4 vc = RMP3_VLD(co + i);
            rmp3_f4 vs = RMP3_VLD(si + i);
            rmp3_f4 vr0 = RMP3_VLD(g_twid9 + i);
            rmp3_f4 vr1 = RMP3_VLD(g_twid9 + 9 + i);
            rmp3_f4 vw0 = RMP3_VLD(window + i);
            rmp3_f4 vw1 = RMP3_VLD(window + 9 + i);
            rmp3_f4 vsum = RMP3_VADD(RMP3_VMUL(vc, vr1), RMP3_VMUL(vs, vr0));
            RMP3_VSTORE(overlap + i, RMP3_VSUB(RMP3_VMUL(vc, vr0), RMP3_VMUL(vs, vr1)));
            RMP3_VSTORE(grbuf + i, RMP3_VSUB(RMP3_VMUL(vovl, vw0), RMP3_VMUL(vsum, vw1)));
            vsum = RMP3_VADD(RMP3_VMUL(vovl, vw1), RMP3_VMUL(vsum, vw0));
            RMP3_VSTORE(grbuf + 14 - i, RMP3_VREV(vsum));
        }
#endif
        for (; i < 9; i++)
        {
            float ovl  = overlap[i];
            float sum  = co[i]*g_twid9[9 + i] + si[i]*g_twid9[0 + i];
            overlap[i] = co[i]*g_twid9[0 + i] - si[i]*g_twid9[9 + i];
            grbuf[i]      = ovl*window[0 + i] - sum*window[9 + i];
            grbuf[17 - i] = ovl*window[9 + i] + sum*window[0 + i];
        }
    }
}

static void rmp3_L3_idct3(float x0, float x1, float x2, float *dst)
{
    float m1 = x1*0.86602540f;
    float a1 = x0 - x2*0.5f;
    dst[1] = x0 + x2;
    dst[0] = a1 + m1;
    dst[2] = a1 - m1;
}

static void rmp3_L3_imdct12(float *x, float *dst, float *overlap)
{
    static const float g_twid3[6] = { 0.79335334f,0.92387953f,0.99144486f, 0.60876143f,0.38268343f,0.13052619f };
    float co[3], si[3];
    int i;

    rmp3_L3_idct3(-x[0], x[6] + x[3], x[12] + x[9], co);
    rmp3_L3_idct3(x[15], x[12] - x[9], x[6] - x[3], si);
    si[1] = -si[1];

    for (i = 0; i < 3; i++)
    {
        float ovl  = overlap[i];
        float sum  = co[i]*g_twid3[3 + i] + si[i]*g_twid3[0 + i];
        overlap[i] = co[i]*g_twid3[0 + i] - si[i]*g_twid3[3 + i];
        dst[i]     = ovl*g_twid3[2 - i] - sum*g_twid3[5 - i];
        dst[5 - i] = ovl*g_twid3[5 - i] + sum*g_twid3[2 - i];
    }
}

static void rmp3_L3_imdct_short(float *grbuf, float *overlap, int nbands)
{
    for (;nbands > 0; nbands--, overlap += 9, grbuf += 18)
    {
        float tmp[18];
        memcpy(tmp, grbuf, sizeof(tmp));
        memcpy(grbuf, overlap, 6*sizeof(float));
        rmp3_L3_imdct12(tmp, grbuf + 6, overlap + 6);
        rmp3_L3_imdct12(tmp + 1, grbuf + 12, overlap + 6);
        rmp3_L3_imdct12(tmp + 2, overlap, overlap + 6);
    }
}

static void rmp3_L3_change_sign(float *grbuf)
{
    int b, i;
    for (b = 0, grbuf += 18; b < 32; b += 2, grbuf += 36)
        for (i = 1; i < 18; i += 2)
            grbuf[i] = -grbuf[i];
}

static void rmp3_L3_imdct_gr(float *grbuf, float *overlap, unsigned block_type, unsigned n_long_bands)
{
    static const float g_mdct_window[2][18] = {
        { 0.99904822f,0.99144486f,0.97629601f,0.95371695f,0.92387953f,0.88701083f,0.84339145f,0.79335334f,0.73727734f,0.04361938f,0.13052619f,0.21643961f,0.30070580f,0.38268343f,0.46174861f,0.53729961f,0.60876143f,0.67559021f },
        { 1,1,1,1,1,1,0.99144486f,0.92387953f,0.79335334f,0,0,0,0,0,0,0.13052619f,0.38268343f,0.60876143f }
    };
    if (n_long_bands)
    {
        rmp3_L3_imdct36(grbuf, overlap, g_mdct_window[0], n_long_bands);
        grbuf += 18*n_long_bands;
        overlap += 9*n_long_bands;
    }
    if (block_type == RMP3_SHORT_BLOCK_TYPE)
        rmp3_L3_imdct_short(grbuf, overlap, 32 - n_long_bands);
    else
        rmp3_L3_imdct36(grbuf, overlap, g_mdct_window[block_type == RMP3_STOP_BLOCK_TYPE], 32 - n_long_bands);
}

static void rmp3_L3_save_reservoir(rmp3dec *h, rmp3dec_scratch *s)
{
    int pos = (s->bs.pos + 7)/8u;
    int remains = s->bs.limit/8u - pos;
    if (remains > RMP3_MAX_BITRESERVOIR_BYTES)
    {
        pos += remains - RMP3_MAX_BITRESERVOIR_BYTES;
        remains = RMP3_MAX_BITRESERVOIR_BYTES;
    }
    if (remains > 0)
        memmove(h->reserv_buf, s->maindata + pos, remains);
    h->reserv = remains;
}

static int rmp3_L3_restore_reservoir(rmp3dec *h, rmp3_bs *bs, rmp3dec_scratch *s, int main_data_begin)
{
    int frame_bytes = (bs->limit - bs->pos)/8;
    int bytes_have = RMP3_MIN(h->reserv, main_data_begin);
    memcpy(s->maindata, h->reserv_buf + RMP3_MAX(0, h->reserv - main_data_begin), RMP3_MIN(h->reserv, main_data_begin));
    memcpy(s->maindata + bytes_have, bs->buf + bs->pos/8, frame_bytes);
    rmp3_bs_init(&s->bs, s->maindata, bytes_have + frame_bytes);
    return h->reserv >= main_data_begin;
}

static void rmp3_L3_decode(rmp3dec *h, rmp3dec_scratch *s, rmp3_L3_gr_info *gr_info, int nch)
{
    int ch;

    for (ch = 0; ch < nch; ch++)
    {
        int layer3gr_limit = s->bs.pos + gr_info[ch].part_23_length;
        rmp3_L3_decode_scalefactors(h->header, s->ist_pos[ch], &s->bs, gr_info + ch, s->scf, ch);
        rmp3_L3_huffman(s->grbuf[ch], &s->bs, gr_info + ch, s->scf, layer3gr_limit);
    }

    if (RMP3_HDR_TEST_I_STEREO(h->header))
        rmp3_L3_intensity_stereo(s->grbuf[0], s->ist_pos[1], gr_info, h->header);
    else if (RMP3_HDR_IS_MS_STEREO(h->header))
        rmp3_L3_midside_stereo(s->grbuf[0], 576);

    for (ch = 0; ch < nch; ch++, gr_info++)
    {
        int aa_bands = 31;
        int n_long_bands = (gr_info->mixed_block_flag ? 2 : 0) << (int)(RMP3_HDR_GET_MY_SAMPLE_RATE(h->header) == 2);

        if (gr_info->n_short_sfb)
        {
            aa_bands = n_long_bands - 1;
            rmp3_L3_reorder(s->grbuf[ch] + n_long_bands*18, s->syn[0], gr_info->sfbtab + gr_info->n_long_sfb);
        }

        rmp3_L3_antialias(s->grbuf[ch], aa_bands);
        rmp3_L3_imdct_gr(s->grbuf[ch], h->mdct_overlap[ch], gr_info->block_type, n_long_bands);
        rmp3_L3_change_sign(s->grbuf[ch]);
    }
}

static void rmp3d_DCT_II(float *grbuf, int n)
{
    static const float g_sec[24] = {
        10.19000816f,0.50060302f,0.50241929f,3.40760851f,0.50547093f,0.52249861f,2.05778098f,0.51544732f,0.56694406f,1.48416460f,0.53104258f,0.64682180f,1.16943991f,0.55310392f,0.78815460f,0.97256821f,0.58293498f,1.06067765f,0.83934963f,0.62250412f,1.72244716f,0.74453628f,0.67480832f,5.10114861f
    };
    int i, k = 0;
#if RMP3_HAVE_SIMD
    if (rmp3_have_simd()) for (; k < n; k += 4)
    {
        rmp3_f4 t[4][8], *x;
        float *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            rmp3_f4 x0 = RMP3_VLD(&y[i*18]);
            rmp3_f4 x1 = RMP3_VLD(&y[(15 - i)*18]);
            rmp3_f4 x2 = RMP3_VLD(&y[(16 + i)*18]);
            rmp3_f4 x3 = RMP3_VLD(&y[(31 - i)*18]);
            rmp3_f4 t0 = RMP3_VADD(x0, x3);
            rmp3_f4 t1 = RMP3_VADD(x1, x2);
            rmp3_f4 t2 = RMP3_VMUL_S(RMP3_VSUB(x1, x2), g_sec[3*i + 0]);
            rmp3_f4 t3 = RMP3_VMUL_S(RMP3_VSUB(x0, x3), g_sec[3*i + 1]);
            x[0] = RMP3_VADD(t0, t1);
            x[8] = RMP3_VMUL_S(RMP3_VSUB(t0, t1), g_sec[3*i + 2]);
            x[16] = RMP3_VADD(t3, t2);
            x[24] = RMP3_VMUL_S(RMP3_VSUB(t3, t2), g_sec[3*i + 2]);
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            rmp3_f4 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = RMP3_VSUB(x0, x7); x0 = RMP3_VADD(x0, x7);
            x7 = RMP3_VSUB(x1, x6); x1 = RMP3_VADD(x1, x6);
            x6 = RMP3_VSUB(x2, x5); x2 = RMP3_VADD(x2, x5);
            x5 = RMP3_VSUB(x3, x4); x3 = RMP3_VADD(x3, x4);
            x4 = RMP3_VSUB(x0, x3); x0 = RMP3_VADD(x0, x3);
            x3 = RMP3_VSUB(x1, x2); x1 = RMP3_VADD(x1, x2);
            x[0] = RMP3_VADD(x0, x1);
            x[4] = RMP3_VMUL_S(RMP3_VSUB(x0, x1), 0.70710677f);
            x5 = RMP3_VADD(x5, x6);
            x6 = RMP3_VMUL_S(RMP3_VADD(x6, x7), 0.70710677f);
            x7 = RMP3_VADD(x7, xt);
            x3 = RMP3_VMUL_S(RMP3_VADD(x3, x4), 0.70710677f);
            x5 = RMP3_VSUB(x5, RMP3_VMUL_S(x7, 0.198912367f)); /* rotate by PI/8 */
            x7 = RMP3_VADD(x7, RMP3_VMUL_S(x5, 0.382683432f));
            x5 = RMP3_VSUB(x5, RMP3_VMUL_S(x7, 0.198912367f));
            x0 = RMP3_VSUB(xt, x6); xt = RMP3_VADD(xt, x6);
            x[1] = RMP3_VMUL_S(RMP3_VADD(xt, x7), 0.50979561f);
            x[2] = RMP3_VMUL_S(RMP3_VADD(x4, x3), 0.54119611f);
            x[3] = RMP3_VMUL_S(RMP3_VSUB(x0, x5), 0.60134488f);
            x[5] = RMP3_VMUL_S(RMP3_VADD(x0, x5), 0.89997619f);
            x[6] = RMP3_VMUL_S(RMP3_VSUB(x4, x3), 1.30656302f);
            x[7] = RMP3_VMUL_S(RMP3_VSUB(xt, x7), 2.56291556f);
        }

        if (k > n - 3)
        {
#if RMP3_HAVE_SSE
#define RMP3_VSAVE2(i, v) _mm_storel_pi((__m64 *)(void*)&y[i*18], v)
#else
#define RMP3_VSAVE2(i, v) vst1_f32((float32_t *)&y[i*18],  vget_low_f32(v))
#endif
            for (i = 0; i < 7; i++, y += 4*18)
            {
                rmp3_f4 s = RMP3_VADD(t[3][i], t[3][i + 1]);
                RMP3_VSAVE2(0, t[0][i]);
                RMP3_VSAVE2(1, RMP3_VADD(t[2][i], s));
                RMP3_VSAVE2(2, RMP3_VADD(t[1][i], t[1][i + 1]));
                RMP3_VSAVE2(3, RMP3_VADD(t[2][1 + i], s));
            }
            RMP3_VSAVE2(0, t[0][7]);
            RMP3_VSAVE2(1, RMP3_VADD(t[2][7], t[3][7]));
            RMP3_VSAVE2(2, t[1][7]);
            RMP3_VSAVE2(3, t[3][7]);
        } else
        {
#define RMP3_VSAVE4(i, v) RMP3_VSTORE(&y[i*18], v)
            for (i = 0; i < 7; i++, y += 4*18)
            {
                rmp3_f4 s = RMP3_VADD(t[3][i], t[3][i + 1]);
                RMP3_VSAVE4(0, t[0][i]);
                RMP3_VSAVE4(1, RMP3_VADD(t[2][i], s));
                RMP3_VSAVE4(2, RMP3_VADD(t[1][i], t[1][i + 1]));
                RMP3_VSAVE4(3, RMP3_VADD(t[2][1 + i], s));
            }
            RMP3_VSAVE4(0, t[0][7]);
            RMP3_VSAVE4(1, RMP3_VADD(t[2][7], t[3][7]));
            RMP3_VSAVE4(2, t[1][7]);
            RMP3_VSAVE4(3, t[3][7]);
        }
    } else
#endif
#ifdef RMP3_ONLY_SIMD
    {}
#else
    for (; k < n; k++)
    {
        float t[4][8], *x, *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            float x0 = y[i*18];
            float x1 = y[(15 - i)*18];
            float x2 = y[(16 + i)*18];
            float x3 = y[(31 - i)*18];
            float t0 = x0 + x3;
            float t1 = x1 + x2;
            float t2 = (x1 - x2)*g_sec[3*i + 0];
            float t3 = (x0 - x3)*g_sec[3*i + 1];
            x[0] = t0 + t1;
            x[8] = (t0 - t1)*g_sec[3*i + 2];
            x[16] = t3 + t2;
            x[24] = (t3 - t2)*g_sec[3*i + 2];
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            float x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = x0 - x7; x0 += x7;
            x7 = x1 - x6; x1 += x6;
            x6 = x2 - x5; x2 += x5;
            x5 = x3 - x4; x3 += x4;
            x4 = x0 - x3; x0 += x3;
            x3 = x1 - x2; x1 += x2;
            x[0] = x0 + x1;
            x[4] = (x0 - x1)*0.70710677f;
            x5 =  x5 + x6;
            x6 = (x6 + x7)*0.70710677f;
            x7 =  x7 + xt;
            x3 = (x3 + x4)*0.70710677f;
            x5 -= x7*0.198912367f;  /* rotate by PI/8 */
            x7 += x5*0.382683432f;
            x5 -= x7*0.198912367f;
            x0 = xt - x6; xt += x6;
            x[1] = (xt + x7)*0.50979561f;
            x[2] = (x4 + x3)*0.54119611f;
            x[3] = (x0 - x5)*0.60134488f;
            x[5] = (x0 + x5)*0.89997619f;
            x[6] = (x4 - x3)*1.30656302f;
            x[7] = (xt - x7)*2.56291556f;

        }
        for (i = 0; i < 7; i++, y += 4*18)
        {
            y[0*18] = t[0][i];
            y[1*18] = t[2][i] + t[3][i] + t[3][i + 1];
            y[2*18] = t[1][i] + t[1][i + 1];
            y[3*18] = t[2][i + 1] + t[3][i] + t[3][i + 1];
        }
        y[0*18] = t[0][7];
        y[1*18] = t[2][7] + t[3][7];
        y[2*18] = t[1][7];
        y[3*18] = t[3][7];
    }
#endif
}

static short rmp3d_scale_pcm(float sample)
{
   int s;
   if (sample >  32767.0) return (short) 32767;
   if (sample < -32768.0) return (short)-32768;
   s = (int)(sample + .5f);
   s -= (s < 0);   /* away from zero, to be compliant */
   if (s >  32767) return (short) 32767;
   if (s < -32768) return (short)-32768;
   return (short)s;
}

static void rmp3d_synth_pair(short *pcm, int nch, const float *z)
{
    float a  = (z[14*64] - z[    0]) * 29;
    a += (z[ 1*64] + z[13*64]) * 213;
    a += (z[12*64] - z[ 2*64]) * 459;
    a += (z[ 3*64] + z[11*64]) * 2037;
    a += (z[10*64] - z[ 4*64]) * 5153;
    a += (z[ 5*64] + z[ 9*64]) * 6574;
    a += (z[ 8*64] - z[ 6*64]) * 37489;
    a +=  z[ 7*64]             * 75038;
    pcm[0] = rmp3d_scale_pcm(a);

    z += 2;
    a  = z[14*64] * 104;
    a += z[12*64] * 1567;
    a += z[10*64] * 9727;
    a += z[ 8*64] * 64019;
    a += z[ 6*64] * -9975;
    a += z[ 4*64] * -45;
    a += z[ 2*64] * 146;
    a += z[ 0*64] * -5;
    pcm[16*nch] = rmp3d_scale_pcm(a);
}

static void rmp3d_synth(float *xl, short *dstl, int nch, float *lins)
{
    int i;
    float *xr = xl + 576*(nch - 1);
    short *dstr = dstl + (nch - 1);

    static const float g_win[] = {
        -1,26,-31,208,218,401,-519,2063,2000,4788,-5517,7134,5959,35640,-39336,74992,
        -1,24,-35,202,222,347,-581,2080,1952,4425,-5879,7640,5288,33791,-41176,74856,
        -1,21,-38,196,225,294,-645,2087,1893,4063,-6237,8092,4561,31947,-43006,74630,
        -1,19,-41,190,227,244,-711,2085,1822,3705,-6589,8492,3776,30112,-44821,74313,
        -1,17,-45,183,228,197,-779,2075,1739,3351,-6935,8840,2935,28289,-46617,73908,
        -1,16,-49,176,228,153,-848,2057,1644,3004,-7271,9139,2037,26482,-48390,73415,
        -2,14,-53,169,227,111,-919,2032,1535,2663,-7597,9389,1082,24694,-50137,72835,
        -2,13,-58,161,224,72,-991,2001,1414,2330,-7910,9592,70,22929,-51853,72169,
        -2,11,-63,154,221,36,-1064,1962,1280,2006,-8209,9750,-998,21189,-53534,71420,
        -2,10,-68,147,215,2,-1137,1919,1131,1692,-8491,9863,-2122,19478,-55178,70590,
        -3,9,-73,139,208,-29,-1210,1870,970,1388,-8755,9935,-3300,17799,-56778,69679,
        -3,8,-79,132,200,-57,-1283,1817,794,1095,-8998,9966,-4533,16155,-58333,68692,
        -4,7,-85,125,189,-83,-1356,1759,605,814,-9219,9959,-5818,14548,-59838,67629,
        -4,7,-91,117,177,-106,-1428,1698,402,545,-9416,9916,-7154,12980,-61289,66494,
        -5,6,-97,111,163,-127,-1498,1634,185,288,-9585,9838,-8540,11455,-62684,65290
    };
    float *zlin = lins + 15*64;
    const float *w = g_win;

    zlin[4*15]     = xl[18*16];
    zlin[4*15 + 1] = xr[18*16];
    zlin[4*15 + 2] = xl[0];
    zlin[4*15 + 3] = xr[0];

    zlin[4*31]     = xl[1 + 18*16];
    zlin[4*31 + 1] = xr[1 + 18*16];
    zlin[4*31 + 2] = xl[1];
    zlin[4*31 + 3] = xr[1];

    rmp3d_synth_pair(dstr, nch, lins + 4*15 + 1);
    rmp3d_synth_pair(dstr + 32*nch, nch, lins + 4*15 + 64 + 1);
    rmp3d_synth_pair(dstl, nch, lins + 4*15);
    rmp3d_synth_pair(dstl + 32*nch, nch, lins + 4*15 + 64);

#if RMP3_HAVE_SIMD
    if (rmp3_have_simd()) for (i = 14; i >= 0; i--)
    {
#define RMP3_VLOAD(k) rmp3_f4 w0 = RMP3_VSET(*w++); rmp3_f4 w1 = RMP3_VSET(*w++); rmp3_f4 vz = RMP3_VLD(&zlin[4*i - 64*k]); rmp3_f4 vy = RMP3_VLD(&zlin[4*i - 64*(15 - k)]);
#define RMP3_V0(k) { RMP3_VLOAD(k) b =               RMP3_VADD(RMP3_VMUL(vz, w1), RMP3_VMUL(vy, w0)) ; a =               RMP3_VSUB(RMP3_VMUL(vz, w0), RMP3_VMUL(vy, w1));  }
#define RMP3_V1(k) { RMP3_VLOAD(k) b = RMP3_VADD(b, RMP3_VADD(RMP3_VMUL(vz, w1), RMP3_VMUL(vy, w0))); a = RMP3_VADD(a, RMP3_VSUB(RMP3_VMUL(vz, w0), RMP3_VMUL(vy, w1))); }
#define RMP3_V2(k) { RMP3_VLOAD(k) b = RMP3_VADD(b, RMP3_VADD(RMP3_VMUL(vz, w1), RMP3_VMUL(vy, w0))); a = RMP3_VADD(a, RMP3_VSUB(RMP3_VMUL(vy, w1), RMP3_VMUL(vz, w0))); }
        rmp3_f4 a, b;
        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*i + 64] = xl[1 + 18*(1 + i)];
        zlin[4*i + 64 + 1] = xr[1 + 18*(1 + i)];
        zlin[4*i - 64 + 2] = xl[18*(1 + i)];
        zlin[4*i - 64 + 3] = xr[18*(1 + i)];

        RMP3_V0(0) RMP3_V2(1) RMP3_V1(2) RMP3_V2(3) RMP3_V1(4) RMP3_V2(5) RMP3_V1(6) RMP3_V2(7)

        {
#if RMP3_HAVE_SSE
            static const rmp3_f4 g_max = { 32767.0f, 32767.0f, 32767.0f, 32767.0f };
            static const rmp3_f4 g_min = { -32768.0f, -32768.0f, -32768.0f, -32768.0f };
            __m128i pcm8 = _mm_packs_epi32(_mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(a, g_max), g_min)),
                                           _mm_cvtps_epi32(_mm_max_ps(_mm_min_ps(b, g_max), g_min)));
            dstr[(15 - i)*nch] = (short)_mm_extract_epi16(pcm8, 1);
            dstr[(17 + i)*nch] = (short)_mm_extract_epi16(pcm8, 5);
            dstl[(15 - i)*nch] = (short)_mm_extract_epi16(pcm8, 0);
            dstl[(17 + i)*nch] = (short)_mm_extract_epi16(pcm8, 4);
            dstr[(47 - i)*nch] = (short)_mm_extract_epi16(pcm8, 3);
            dstr[(49 + i)*nch] = (short)_mm_extract_epi16(pcm8, 7);
            dstl[(47 - i)*nch] = (short)_mm_extract_epi16(pcm8, 2);
            dstl[(49 + i)*nch] = (short)_mm_extract_epi16(pcm8, 6);
#else
            int16x4_t pcma, pcmb;
            a = RMP3_VADD(a, RMP3_VSET(0.5f));
            b = RMP3_VADD(b, RMP3_VSET(0.5f));
            pcma = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(a), vreinterpretq_s32_u32(vcltq_f32(a, RMP3_VSET(0)))));
            pcmb = vqmovn_s32(vqaddq_s32(vcvtq_s32_f32(b), vreinterpretq_s32_u32(vcltq_f32(b, RMP3_VSET(0)))));
            vst1_lane_s16(dstr + (15 - i)*nch, pcma, 1);
            vst1_lane_s16(dstr + (17 + i)*nch, pcmb, 1);
            vst1_lane_s16(dstl + (15 - i)*nch, pcma, 0);
            vst1_lane_s16(dstl + (17 + i)*nch, pcmb, 0);
            vst1_lane_s16(dstr + (47 - i)*nch, pcma, 3);
            vst1_lane_s16(dstr + (49 + i)*nch, pcmb, 3);
            vst1_lane_s16(dstl + (47 - i)*nch, pcma, 2);
            vst1_lane_s16(dstl + (49 + i)*nch, pcmb, 2);
#endif
        }
    } else
#endif
#ifdef RMP3_ONLY_SIMD
    {}
#else
    for (i = 14; i >= 0; i--)
    {
#define RMP3_LOAD(k) float w0 = *w++; float w1 = *w++; float *vz = &zlin[4*i - k*64]; float *vy = &zlin[4*i - (15 - k)*64];
#define RMP3_S0(k) { int j; RMP3_LOAD(k); for (j = 0; j < 4; j++) b[j]  = vz[j]*w1 + vy[j]*w0, a[j]  = vz[j]*w0 - vy[j]*w1; }
#define RMP3_S1(k) { int j; RMP3_LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vz[j]*w0 - vy[j]*w1; }
#define RMP3_S2(k) { int j; RMP3_LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vy[j]*w1 - vz[j]*w0; }
        float a[4], b[4];

        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*(i + 16)]   = xl[1 + 18*(1 + i)];
        zlin[4*(i + 16) + 1] = xr[1 + 18*(1 + i)];
        zlin[4*(i - 16) + 2] = xl[18*(1 + i)];
        zlin[4*(i - 16) + 3] = xr[18*(1 + i)];

        RMP3_S0(0) RMP3_S2(1) RMP3_S1(2) RMP3_S2(3) RMP3_S1(4) RMP3_S2(5) RMP3_S1(6) RMP3_S2(7)

        dstr[(15 - i)*nch] = rmp3d_scale_pcm(a[1]);
        dstr[(17 + i)*nch] = rmp3d_scale_pcm(b[1]);
        dstl[(15 - i)*nch] = rmp3d_scale_pcm(a[0]);
        dstl[(17 + i)*nch] = rmp3d_scale_pcm(b[0]);
        dstr[(47 - i)*nch] = rmp3d_scale_pcm(a[3]);
        dstr[(49 + i)*nch] = rmp3d_scale_pcm(b[3]);
        dstl[(47 - i)*nch] = rmp3d_scale_pcm(a[2]);
        dstl[(49 + i)*nch] = rmp3d_scale_pcm(b[2]);
    }
#endif
}

static void rmp3d_synth_granule(float *qmf_state, float *grbuf, int nbands, int nch, short *pcm, float *lins)
{
    int i;
    for (i = 0; i < nch; i++)
        rmp3d_DCT_II(grbuf + 576*i, nbands);

    memcpy(lins, qmf_state, sizeof(float)*15*64);

    for (i = 0; i < nbands; i += 2)
        rmp3d_synth(grbuf + i, pcm + 32*nch*i, nch, lins + i*64);
#ifndef RMP3_NONSTANDARD_BUT_LOGICAL
    if (nch == 1)
    {
        for (i = 0; i < 15*64; i += 2)
            qmf_state[i] = lins[nbands*64 + i];
    }
    else
#endif
    {
        memcpy(qmf_state, lins + nbands*64, sizeof(float)*15*64);
    }
}

static int rmp3d_match_frame(const uint8_t *hdr, int mp3_bytes, int frame_bytes)
{
    int i, nmatch;
    for (i = 0, nmatch = 0; nmatch < RMP3_MAX_FRAME_SYNC_MATCHES; nmatch++)
    {
        i += rmp3_hdr_frame_bytes(hdr + i, frame_bytes) + rmp3_hdr_padding(hdr + i);
        if (i + RMP3_HDR_SIZE > mp3_bytes)
            return nmatch > 0;
        if (!rmp3_hdr_compare(hdr, hdr + i))
            return 0;
    }
    return 1;
}

static int rmp3d_find_frame(const uint8_t *mp3, int mp3_bytes, int *free_format_bytes, int *ptr_frame_bytes)
{
    int i, k;
    for (i = 0; i < mp3_bytes - RMP3_HDR_SIZE; i++, mp3++)
    {
        if (rmp3_hdr_valid(mp3))
        {
            int frame_bytes = rmp3_hdr_frame_bytes(mp3, *free_format_bytes);
            int frame_and_padding = frame_bytes + rmp3_hdr_padding(mp3);

            for (k = RMP3_HDR_SIZE; !frame_bytes && k < RMP3_MAX_FREE_FORMAT_FRAME_SIZE && i + 2*k < mp3_bytes - RMP3_HDR_SIZE; k++)
            {
                if (rmp3_hdr_compare(mp3, mp3 + k))
                {
                    int fb = k - rmp3_hdr_padding(mp3);
                    int nextfb = fb + rmp3_hdr_padding(mp3 + k);
                    if (i + k + nextfb + RMP3_HDR_SIZE > mp3_bytes || !rmp3_hdr_compare(mp3, mp3 + k + nextfb))
                        continue;
                    frame_and_padding = k;
                    frame_bytes = fb;
                    *free_format_bytes = fb;
                }
            }

            if ((frame_bytes && i + frame_and_padding <= mp3_bytes &&
                rmp3d_match_frame(mp3, mp3_bytes - i, frame_bytes)) ||
                (!i && frame_and_padding == mp3_bytes))
            {
                *ptr_frame_bytes = frame_and_padding;
                return i;
            }
            *free_format_bytes = 0;
        }
    }
    *ptr_frame_bytes = 0;
    return i;
}

void rmp3dec_init(rmp3dec *dec)
{
    dec->header[0] = 0;
}

int rmp3dec_decode_frame(rmp3dec *dec, const unsigned char *mp3, int mp3_bytes, short *pcm, rmp3dec_frame_info *info)
{
    int i = 0, igr, frame_size = 0, success = 1;
    const uint8_t *hdr;
    rmp3_bs bs_frame[1];
    rmp3dec_scratch scratch;

    if (mp3_bytes > 4 && dec->header[0] == 0xff && rmp3_hdr_compare(dec->header, mp3))
    {
        frame_size = rmp3_hdr_frame_bytes(mp3, dec->free_format_bytes) + rmp3_hdr_padding(mp3);
        if (frame_size != mp3_bytes && (frame_size + RMP3_HDR_SIZE > mp3_bytes || !rmp3_hdr_compare(mp3, mp3 + frame_size)))
            frame_size = 0;
    }
    if (!frame_size)
    {
        memset(dec, 0, sizeof(rmp3dec));
        i = rmp3d_find_frame(mp3, mp3_bytes, &dec->free_format_bytes, &frame_size);
        if (!frame_size || i + frame_size > mp3_bytes)
        {
            info->frame_bytes = i;
            return 0;
        }
    }

    hdr = mp3 + i;
    memcpy(dec->header, hdr, RMP3_HDR_SIZE);
    info->frame_bytes = i + frame_size;
    info->channels = RMP3_HDR_IS_MONO(hdr) ? 1 : 2;
    info->hz = rmp3_hdr_sample_rate_hz(hdr);
    info->layer = 4 - RMP3_HDR_GET_LAYER(hdr);
    info->bitrate_kbps = rmp3_hdr_bitrate_kbps(hdr);

    rmp3_bs_init(bs_frame, hdr + RMP3_HDR_SIZE, frame_size - RMP3_HDR_SIZE);
    if (RMP3_HDR_IS_CRC(hdr))
        rmp3_bs_get_bits(bs_frame, 16);

    if (info->layer == 3)
    {
        int main_data_begin = rmp3_L3_read_side_info(bs_frame, scratch.gr_info, hdr);
        if (main_data_begin < 0 || bs_frame->pos > bs_frame->limit)
        {
            rmp3dec_init(dec);
            return 0;
        }
        success = rmp3_L3_restore_reservoir(dec, bs_frame, &scratch, main_data_begin);
        if (success)
        {
            for (igr = 0; igr < (RMP3_HDR_TEST_MPEG1(hdr) ? 2 : 1); igr++, pcm += 576*info->channels)
            {
                memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
                rmp3_L3_decode(dec, &scratch, scratch.gr_info + igr*info->channels, info->channels);
                rmp3d_synth_granule(dec->qmf_state, scratch.grbuf[0], 18, info->channels, pcm, scratch.syn[0]);
            }
        }
        rmp3_L3_save_reservoir(dec, &scratch);
    } else
    {
        rmp3_L12_scale_info sci[1];
        rmp3_L12_read_scale_info(hdr, bs_frame, sci);

        memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
        for (i = 0, igr = 0; igr < 3; igr++)
        {
            if (12 == (i += rmp3_L12_dequantize_granule(scratch.grbuf[0] + i, bs_frame, sci, info->layer | 1)))
            {
                i = 0;
                rmp3_L12_apply_scf_384(sci, sci->scf + igr, scratch.grbuf[0]);
                rmp3d_synth_granule(dec->qmf_state, scratch.grbuf[0], 12, info->channels, pcm, scratch.syn[0]);
                memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
                pcm += 384*info->channels;
            }
            if (bs_frame->pos > bs_frame->limit)
            {
                rmp3dec_init(dec);
                return 0;
            }
        }
    }
    return success*rmp3_hdr_frame_samples(dec->header);
}

/*
 *
 * Main Public API
 *
 */

/* Options. */
#ifndef RMP3_DEFAULT_CHANNELS
#define RMP3_DEFAULT_CHANNELS      2
#endif
#ifndef RMP3_DEFAULT_SAMPLE_RATE
#define RMP3_DEFAULT_SAMPLE_RATE   44100
#endif

/* Standard library stuff. */
#define rmp3_countof(x)  (sizeof(x) / sizeof(x[0]))
#define rmp3_max(x, y)   (((x) > (y)) ? (x) : (y))
#define rmp3_min(x, y)   (((x) < (y)) ? (x) : (y))

#define RMP3_DATA_CHUNK_SIZE  16384    /* The size in bytes of each chunk of data to read from the MP3 stream. minimp3 recommends 16K. */

static INLINE float rmp3_mix_f32(float x, float y, float a)
{
    return x*(1-a) + y*a;
}

static void rmp3_blend_f32(float* pOut, float* pInA, float* pInB, float factor, uint32_t channels)
{
   uint32_t i;
    for (i = 0; i < channels; ++i)
        pOut[i] = rmp3_mix_f32(pInA[i], pInB[i], factor);
}

void rmp3_src_cache_init(rmp3_src* pSRC, rmp3_src_cache* pCache)
{
    pCache->pSRC = pSRC;
    pCache->cachedFrameCount = 0;
    pCache->iNextFrame = 0;
}

uint64_t rmp3_src_cache_read_frames(rmp3_src_cache* pCache, uint64_t frameCount, float* pFramesOut)
{
   uint64_t totalFramesRead = 0;
   uint32_t channels = pCache->pSRC->config.channels;

   while (frameCount > 0)
   {
      uint32_t framesToReadFromClient;
      /* If there's anything in memory go ahead and copy that over first. */
      uint64_t framesRemainingInMemory = pCache->cachedFrameCount - pCache->iNextFrame;
      uint64_t framesToReadFromMemory = frameCount;
      if (framesToReadFromMemory > framesRemainingInMemory)
         framesToReadFromMemory = framesRemainingInMemory;

      memcpy(pFramesOut, pCache->pCachedFrames + pCache->iNextFrame*channels, (uint32_t)(framesToReadFromMemory * channels * sizeof(float)));
      pCache->iNextFrame += (uint32_t)framesToReadFromMemory;

      totalFramesRead += framesToReadFromMemory;
      frameCount -= framesToReadFromMemory;
      if (frameCount == 0)
         break;


      /* At this point there are still more frames to read from the client, so we'll need to reload the cache with fresh data. */
      pFramesOut += framesToReadFromMemory * channels;

      pCache->iNextFrame = 0;
      pCache->cachedFrameCount = 0;

      framesToReadFromClient = rmp3_countof(pCache->pCachedFrames) / pCache->pSRC->config.channels;
      if (framesToReadFromClient > pCache->pSRC->config.cacheSizeInFrames)
         framesToReadFromClient = pCache->pSRC->config.cacheSizeInFrames;

      pCache->cachedFrameCount = (uint32_t)pCache->pSRC->onRead(pCache->pSRC, framesToReadFromClient, pCache->pCachedFrames, pCache->pSRC->pUserData);


      /* Get out of this loop if nothing was able to be retrieved. */
      if (pCache->cachedFrameCount == 0)
         break;
   }

   return totalFramesRead;
}


uint64_t rmp3_src_read_frames_passthrough(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, uint32_t flush);
uint64_t rmp3_src_read_frames_linear(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, uint32_t flush);

uint32_t rmp3_src_init(const rmp3_src_config* pConfig, rmp3_src_read_proc onRead, void* pUserData, rmp3_src* pSRC)
{
    if (pSRC == NULL) return RMP3_FALSE;
    memset((pSRC), 0, sizeof(*(pSRC)));

    if (pConfig == NULL || onRead == NULL) return RMP3_FALSE;
    if (pConfig->channels == 0 || pConfig->channels > 2) return RMP3_FALSE;

    pSRC->config = *pConfig;
    pSRC->onRead = onRead;
    pSRC->pUserData = pUserData;

    if (pSRC->config.cacheSizeInFrames > RMP3_SRC_CACHE_SIZE_IN_FRAMES || pSRC->config.cacheSizeInFrames == 0)
        pSRC->config.cacheSizeInFrames = RMP3_SRC_CACHE_SIZE_IN_FRAMES;

    rmp3_src_cache_init(pSRC, &pSRC->cache);
    return RMP3_TRUE;
}

uint32_t rmp3_src_set_input_sample_rate(rmp3_src* pSRC, uint32_t sampleRateIn)
{
    if (pSRC == NULL) return RMP3_FALSE;

    /* Must have a sample rate of > 0. */
    if (sampleRateIn == 0)
        return RMP3_FALSE;

    pSRC->config.sampleRateIn = sampleRateIn;
    return RMP3_TRUE;
}

uint32_t rmp3_src_set_output_sample_rate(rmp3_src* pSRC, uint32_t sampleRateOut)
{
    if (pSRC == NULL) return RMP3_FALSE;

    /* Must have a sample rate of > 0. */
    if (sampleRateOut == 0)
        return RMP3_FALSE;

    pSRC->config.sampleRateOut = sampleRateOut;
    return RMP3_TRUE;
}

uint64_t rmp3_src_read_frames_ex(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, uint32_t flush)
{
   rmp3_src_algorithm algorithm;
   if (pSRC == NULL || frameCount == 0 || pFramesOut == NULL) return 0;

   algorithm = pSRC->config.algorithm;

   /* Always use passthrough if the sample rates are the same. */
   if (pSRC->config.sampleRateIn == pSRC->config.sampleRateOut)
      algorithm = rmp3_src_algorithm_none;

   /* Could just use a function pointer instead of a switch for this... */
   switch (algorithm)
   {
      case rmp3_src_algorithm_none:   return rmp3_src_read_frames_passthrough(pSRC, frameCount, pFramesOut, flush);
      case rmp3_src_algorithm_linear: return rmp3_src_read_frames_linear(pSRC, frameCount, pFramesOut, flush);
      default: return 0;
   }
}

uint64_t rmp3_src_read_frames(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut)
{
    return rmp3_src_read_frames_ex(pSRC, frameCount, pFramesOut, RMP3_FALSE);
}

uint64_t rmp3_src_read_frames_passthrough(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, uint32_t flush)
{
    return pSRC->onRead(pSRC, frameCount, pFramesOut, pSRC->pUserData);
}

uint64_t rmp3_src_read_frames_linear(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, uint32_t flush)
{
   float factor;
   uint64_t totalFramesRead = 0;

   /* For linear SRC, the bin is only 2 frames: 1 prior, 1 future. */

   /* Load the bin if necessary. */
   if (!pSRC->algo.linear.isPrevFramesLoaded)
   {
      uint64_t framesRead = rmp3_src_cache_read_frames(&pSRC->cache, 1, pSRC->bin);
      if (framesRead == 0)
         return 0;
      pSRC->algo.linear.isPrevFramesLoaded = RMP3_TRUE;
   }
   if (!pSRC->algo.linear.isNextFramesLoaded)
   {
      uint64_t framesRead = rmp3_src_cache_read_frames(&pSRC->cache, 1, pSRC->bin + pSRC->config.channels);
      if (framesRead == 0)
         return 0;
      pSRC->algo.linear.isNextFramesLoaded = RMP3_TRUE;
   }

   factor = (float)pSRC->config.sampleRateIn / pSRC->config.sampleRateOut;

   while (frameCount > 0)
   {
      uint32_t i;
      uint32_t framesToReadFromClient;
      /* The bin is where the previous and next frames are located. */
      float* pPrevFrame = pSRC->bin;
      float* pNextFrame = pSRC->bin + pSRC->config.channels;

      rmp3_blend_f32((float*)pFramesOut, pPrevFrame, pNextFrame, pSRC->algo.linear.alpha, pSRC->config.channels);

      pSRC->algo.linear.alpha += factor;

      /* The new alpha value is how we determine whether or not we need to read fresh frames. */
      framesToReadFromClient = (uint32_t)pSRC->algo.linear.alpha;
      pSRC->algo.linear.alpha = pSRC->algo.linear.alpha - framesToReadFromClient;

      for (i = 0; i < framesToReadFromClient; ++i)
      {
         uint32_t j;
         uint64_t framesRead;
         for (j = 0; j < pSRC->config.channels; ++j)
            pPrevFrame[j] = pNextFrame[j];

         framesRead = rmp3_src_cache_read_frames(&pSRC->cache, 1, pNextFrame);
         if (framesRead == 0)
         {
            uint32_t j;
            for (j = 0; j < pSRC->config.channels; ++j)
               pNextFrame[j] = 0;

            if (pSRC->algo.linear.isNextFramesLoaded)
               pSRC->algo.linear.isNextFramesLoaded = RMP3_FALSE;
            else
            {
               if (flush)
                  pSRC->algo.linear.isPrevFramesLoaded = RMP3_FALSE;
            }

            break;
         }
      }

      pFramesOut  = (uint8_t*)pFramesOut + (1 * pSRC->config.channels * sizeof(float));
      frameCount -= 1;
      totalFramesRead += 1;

      /* If there's no frames available we need to get out of this loop. */
      if (!pSRC->algo.linear.isNextFramesLoaded && (!flush || !pSRC->algo.linear.isPrevFramesLoaded))
         break;
   }

   return totalFramesRead;
}



static uint32_t rmp3_decode_next_frame(rmp3* pMP3)
{
    if (pMP3->atEnd)
        return RMP3_FALSE;

    do
    {
       rmp3dec_frame_info info;
       uint32_t samplesRead;

       /* minimp3 recommends doing data submission in 16K chunks. If we don't have at least 16K bytes available, get more. */
       if (pMP3->dataSize < RMP3_DATA_CHUNK_SIZE)
       {
          size_t bytesRead;

          if (pMP3->dataCapacity < RMP3_DATA_CHUNK_SIZE)
          {
             uint8_t* pNewData = NULL;
             pMP3->dataCapacity    = RMP3_DATA_CHUNK_SIZE;

             pNewData              = (uint8_t*)realloc(pMP3->pData, pMP3->dataCapacity);
             if (pNewData == NULL)
                return RMP3_FALSE; /* Out of memory. */

             pMP3->pData = pNewData;
          }

          bytesRead = pMP3->onRead(pMP3->pUserData, pMP3->pData + pMP3->dataSize, (pMP3->dataCapacity - pMP3->dataSize));
          if (bytesRead == 0)
          {
             pMP3->atEnd = RMP3_TRUE;
             return RMP3_FALSE; /* No data. */
          }

          pMP3->dataSize += bytesRead;
       }

       if (pMP3->dataSize > INT_MAX)
       {
          pMP3->atEnd = RMP3_TRUE;
          return RMP3_FALSE; /* File too big. */
       }

       samplesRead = rmp3dec_decode_frame(&pMP3->decoder, pMP3->pData, (int)pMP3->dataSize, pMP3->frames, &info);    /* <-- Safe size_t -> int conversion thanks to the check above. */
       if (samplesRead != 0)
       {
          size_t i;
          size_t leftoverDataSize = (pMP3->dataSize - (size_t)info.frame_bytes);
          for (i = 0; i < leftoverDataSize; ++i)
             pMP3->pData[i] = pMP3->pData[i + (size_t)info.frame_bytes];

          pMP3->dataSize = leftoverDataSize;
          pMP3->framesConsumed = 0;
          pMP3->framesRemaining = samplesRead;
          pMP3->frameChannels = info.channels;
          pMP3->frameSampleRate = info.hz;
          rmp3_src_set_input_sample_rate(&pMP3->src, pMP3->frameSampleRate);
          break;
       }
       else
       {
          size_t bytesRead;
          /* Need more data. minimp3 recommends doing data submission in 16K chunks. */
          if (pMP3->dataCapacity == pMP3->dataSize)
          {
             uint8_t *pNewData = NULL;
             /* No room. Expand. */
             pMP3->dataCapacity   += RMP3_DATA_CHUNK_SIZE;

             pNewData              = (uint8_t*)realloc(pMP3->pData, pMP3->dataCapacity);
             if (pNewData == NULL)
                return RMP3_FALSE; /* Out of memory. */

             pMP3->pData = pNewData;
          }

          /* Fill in a chunk. */
          bytesRead = pMP3->onRead(pMP3->pUserData, pMP3->pData + pMP3->dataSize, (pMP3->dataCapacity - pMP3->dataSize));
          if (bytesRead == 0)
          {
             pMP3->atEnd = RMP3_TRUE;
             return RMP3_FALSE; /* Error reading more data. */
          }

          pMP3->dataSize += bytesRead;
       }
    } while (RMP3_TRUE);

    return RMP3_TRUE;
}

static uint64_t rmp3_read_src(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, void* pUserData)
{
   float* pFramesOutF;
   uint32_t totalFramesRead = 0;
   rmp3* pMP3 = (rmp3*)pUserData;

   pFramesOutF = (float*)pFramesOut;

   while (frameCount > 0)
   {
      /* Read from the in-memory buffer first. */
      while (pMP3->framesRemaining > 0 && frameCount > 0)
      {
         if (pMP3->frameChannels == 1)
         {
            if (pMP3->channels == 1)
            {
               /* Mono -> Mono. */
               pFramesOutF[0] = pMP3->frames[pMP3->framesConsumed] / 32768.0f;
            } else {
               /* Mono -> Stereo. */
               pFramesOutF[0] = pMP3->frames[pMP3->framesConsumed] / 32768.0f;
               pFramesOutF[1] = pMP3->frames[pMP3->framesConsumed] / 32768.0f;
            }
         } else {
            if (pMP3->channels == 1)
            {
               /* Stereo -> Mono */
               float sample = 0;
               sample += pMP3->frames[(pMP3->framesConsumed*pMP3->frameChannels)+0] / 32768.0f;
               sample += pMP3->frames[(pMP3->framesConsumed*pMP3->frameChannels)+1] / 32768.0f;
               pFramesOutF[0] = sample * 0.5f;
            } else {
               /* Stereo -> Stereo */
               pFramesOutF[0] = pMP3->frames[(pMP3->framesConsumed*pMP3->frameChannels)+0] / 32768.0f;
               pFramesOutF[1] = pMP3->frames[(pMP3->framesConsumed*pMP3->frameChannels)+1] / 32768.0f;
            }
         }

         pMP3->framesConsumed += 1;
         pMP3->framesRemaining -= 1;
         frameCount -= 1;
         totalFramesRead += 1;
         pFramesOutF += pSRC->config.channels;
      }

      if (frameCount == 0)
         break;

      /* At this point we have exhausted our in-memory buffer so we need to re-fill. Note that the sample rate may have changed
       * at this point which means we'll also need to update our sample rate conversion pipeline. */
      if (!rmp3_decode_next_frame(pMP3))
         break;
   }

   return totalFramesRead;
}

uint32_t rmp3_init_internal(rmp3* pMP3, rmp3_read_proc onRead, rmp3_seek_proc onSeek, void* pUserData, const rmp3_config* pConfig)
{
   rmp3_config config;
   rmp3_src_config srcConfig;

   /* This function assumes the output object has already been reset to 0. Do not do that here, otherwise things will break. */
   rmp3dec_init(&pMP3->decoder);

   /* The config can be null in which case we use defaults. */
   if (pConfig != NULL)
      config = *pConfig;
   else
      memset((&config), 0, sizeof(*(&config)));

   pMP3->channels = config.outputChannels;
   if (pMP3->channels == 0)
      pMP3->channels = RMP3_DEFAULT_CHANNELS;

   /* Cannot have more than 2 channels. */
   if (pMP3->channels > 2)
      pMP3->channels = 2;

   pMP3->sampleRate = config.outputSampleRate;
   if (pMP3->sampleRate == 0)
      pMP3->sampleRate = RMP3_DEFAULT_SAMPLE_RATE;

   pMP3->onRead = onRead;
   pMP3->onSeek = onSeek;
   pMP3->pUserData = pUserData;

   /* We need a sample rate converter for converting the sample rate from the MP3 frames to the requested output sample rate. */
   memset((&srcConfig), 0, sizeof(*(&srcConfig)));
   srcConfig.sampleRateIn = RMP3_DEFAULT_SAMPLE_RATE;
   srcConfig.sampleRateOut = pMP3->sampleRate;
   srcConfig.channels = pMP3->channels;
   srcConfig.algorithm = rmp3_src_algorithm_linear;
   if (!rmp3_src_init(&srcConfig, rmp3_read_src, pMP3, &pMP3->src))
      return RMP3_FALSE;

   /* Decode the first frame to confirm that it is indeed a valid MP3 stream. */
   if (!rmp3_decode_next_frame(pMP3))
      return RMP3_FALSE; /* Not a valid MP3 stream. */

   return RMP3_TRUE;
}

uint32_t rmp3_init(rmp3* pMP3, rmp3_read_proc onRead, rmp3_seek_proc onSeek, void* pUserData, const rmp3_config* pConfig)
{
    if (pMP3 == NULL || onRead == NULL)
        return RMP3_FALSE;

    memset(pMP3, 0, sizeof(*pMP3));
    return rmp3_init_internal(pMP3, onRead, onSeek, pUserData, pConfig);
}


static size_t rmp3__on_read_memory(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
   size_t bytesRemaining;
    rmp3* pMP3 = (rmp3*)pUserData;

    bytesRemaining = pMP3->memory.dataSize - pMP3->memory.currentReadPos;
    if (bytesToRead > bytesRemaining)
        bytesToRead = bytesRemaining;

    if (bytesToRead > 0)
    {
        memcpy(pBufferOut, pMP3->memory.pData + pMP3->memory.currentReadPos, bytesToRead);
        pMP3->memory.currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

static uint32_t rmp3__on_seek_memory(void* pUserData, int byteOffset, rmp3_seek_origin origin)
{
    rmp3* pMP3 = (rmp3*)pUserData;

    if (origin == rmp3_seek_origin_current)
    {
        if (byteOffset > 0)
        {
            if (pMP3->memory.currentReadPos + byteOffset > pMP3->memory.dataSize)
                byteOffset = (int)(pMP3->memory.dataSize - pMP3->memory.currentReadPos);  /* Trying to seek too far forward. */
        }
        else
        {
            if (pMP3->memory.currentReadPos < (size_t)-byteOffset)
                byteOffset = -(int)pMP3->memory.currentReadPos;  /* Trying to seek too far backwards. */
        }

        /* This will never underflow thanks to the clamps above. */
        pMP3->memory.currentReadPos += byteOffset;
    } else {
        if ((uint32_t)byteOffset <= pMP3->memory.dataSize)
            pMP3->memory.currentReadPos = byteOffset;
        else
            pMP3->memory.currentReadPos = pMP3->memory.dataSize;  /* Trying to seek too far forward. */
    }

    return RMP3_TRUE;
}

uint32_t rmp3_init_memory(rmp3* pMP3, const void* pData, size_t dataSize, const rmp3_config* pConfig)
{
    if (pMP3 == NULL)
        return RMP3_FALSE;

    memset(pMP3, 0, sizeof(*pMP3));

    if (pData == NULL || dataSize == 0)
        return RMP3_FALSE;

    pMP3->memory.pData = (const uint8_t*)pData;
    pMP3->memory.dataSize = dataSize;
    pMP3->memory.currentReadPos = 0;

    return rmp3_init_internal(pMP3, rmp3__on_read_memory, rmp3__on_seek_memory, pMP3, pConfig);
}

void rmp3_uninit(rmp3* pMP3)
{
    if (pMP3 == NULL) return;
    rmp3_free(pMP3->pData);
}

uint64_t rmp3_read_f32(rmp3* pMP3, uint64_t framesToRead, float* pBufferOut)
{
    uint64_t totalFramesRead = 0;
    if (pMP3 == NULL || pMP3->onRead == NULL) return 0;

    if (pBufferOut == NULL)
    {
       float temp[4096];
       while (framesToRead > 0)
       {
          uint64_t framesJustRead;
          uint64_t framesToReadRightNow = sizeof(temp)/sizeof(temp[0]) / pMP3->channels;
          if (framesToReadRightNow > framesToRead)
             framesToReadRightNow = framesToRead;

          framesJustRead = rmp3_read_f32(pMP3, framesToReadRightNow, temp);
          if (framesJustRead == 0)
             break;

          framesToRead -= framesJustRead;
          totalFramesRead += framesJustRead;
       }
    } else {
        totalFramesRead = rmp3_src_read_frames_ex(&pMP3->src, framesToRead, pBufferOut, RMP3_TRUE);
    }

    return totalFramesRead;
}

uint32_t rmp3_seek_to_frame(rmp3* pMP3, uint64_t frameIndex)
{
   uint64_t framesRead;

   if (pMP3 == NULL || pMP3->onSeek == NULL) return RMP3_FALSE;

   /* Seek to the start of the stream to begin with. */
   if (!pMP3->onSeek(pMP3->pUserData, 0, rmp3_seek_origin_start))
      return RMP3_FALSE;

   /* Clear any cached data. */
   pMP3->framesConsumed = 0;
   pMP3->framesRemaining = 0;
   pMP3->dataSize = 0;
   pMP3->atEnd = RMP3_FALSE;

   /* TODO: Optimize.
    *
    * This is inefficient. We simply read frames from the start of the stream. */
   framesRead = rmp3_read_f32(pMP3, frameIndex, NULL);
   if (framesRead != frameIndex)
      return RMP3_FALSE;

   return RMP3_TRUE;
}

void rmp3_free(void* p)
{
    free(p);
}



/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

/*
    https://github.com/lieff/minimp3
    To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
