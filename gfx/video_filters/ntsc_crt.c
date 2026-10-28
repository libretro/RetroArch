/*  RetroArch CPU Filter - NTSC/CRT
 *
 *  Original NTSC/CRT library by EMMIR 2018-2023
 *    https://www.youtube.com/@EMMIR_KC/videos
 *    https://discord.com/invite/hdYctSmyQJ
 *
 *  RetroArch softfilter wrapper 2024
 *
 *  This is a consolidated single-file build of:
 *      crt_core.h  crt_core.c
 *      crt_ntsc.h  crt_ntsc.c
 *      ntsc_crt.c
 */

#include "softfilter.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <retro_atomic.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation ntsc_crt_get_implementation
#define softfilter_thread_data        ntsc_crt_softfilter_thread_data
#define filter_data                   ntsc_crt_filter_data
#endif

/*****************************************************************************/
/*****************************************************************************/
/****                                                                     ****/
/****           NTSC/CRT integer-only NTSC video signal                   ****/
/****           encoding / decoding emulation (EMMIR 2018-2023)           ****/
/****                                                                     ****/
/*****************************************************************************/
/*****************************************************************************/

/* library version */
#define CRT_MAJOR 2
#define CRT_MINOR 3
#define CRT_PATCH 2

/* NOTE: this library does not use the alpha channel at all */
#define CRT_PIX_FORMAT_RGB  0  /* 3 bytes per pixel [R,G,B,R,G,B,R,G,B...] */
#define CRT_PIX_FORMAT_BGR  1  /* 3 bytes per pixel [B,G,R,B,G,R,B,G,R...] */
#define CRT_PIX_FORMAT_ARGB 2  /* 4 bytes per pixel [A,R,G,B,A,R,G,B...]   */
#define CRT_PIX_FORMAT_RGBA 3  /* 4 bytes per pixel [R,G,B,A,R,G,B,A...]   */
#define CRT_PIX_FORMAT_ABGR 4  /* 4 bytes per pixel [A,B,G,R,A,B,G,R...]   */
#define CRT_PIX_FORMAT_BGRA 5  /* 4 bytes per pixel [B,G,R,A,B,G,R,A...]   */

/* do bloom emulation (side effect: makes screen have black borders) */
#define CRT_DO_BLOOM    0  /* does not work for NES */
#define CRT_DO_VSYNC    1  /* look for VSYNC */
#define CRT_DO_HSYNC    1  /* look for HSYNC */

/*****************************************************************************/
/******************** NTSC system parameters (from crt_ntsc.h) ***************/
/*****************************************************************************/

/* 0 = vertical  chroma (228 chroma clocks per line) */
/* 1 = checkered chroma (227.5 chroma clocks per line) */
#define CRT_CHROMA_PATTERN 1

/* chroma clocks (subcarrier cycles) per line */
#if (CRT_CHROMA_PATTERN == 1)
#define CRT_CC_LINE 2275
#else
/* this will give the 'rainbow' effect in the famous waterfall scene */
#define CRT_CC_LINE 2280
#endif

/* NOTE, in general, increasing CRT_CB_FREQ reduces blur and bleed */
#define CRT_CB_FREQ     4 /* carrier frequency relative to sample rate */
#define CRT_HRES        (CRT_CC_LINE * CRT_CB_FREQ / 10) /* horizontal res */
#define CRT_VRES        262                       /* vertical resolution */
#define CRT_INPUT_SIZE  (CRT_HRES * CRT_VRES)

#define CRT_TOP         21     /* first line with active video */
#define CRT_BOT         261    /* final line with active video */
#define CRT_LINES       (CRT_BOT - CRT_TOP) /* number of active video lines */

#define CRT_CC_SAMPLES  4 /* samples per chroma period (samples per 360 deg) */
#define CRT_CC_VPER     1 /* vertical period in which the artifacts repeat */

/* search windows, in samples */
#define CRT_HSYNC_WINDOW 8
#define CRT_VSYNC_WINDOW 8

/* accumulated signal threshold required for sync detection.
 * Larger = more stable, until it's so large that it is never reached in which
 *          case the CRT won't be able to sync
 */
#define CRT_HSYNC_THRESH 4
#define CRT_VSYNC_THRESH 94

/*
 *                      FULL HORIZONTAL LINE SIGNAL (~63500 ns)
 * |---------------------------------------------------------------------------|
 *   HBLANK (~10900 ns)                 ACTIVE VIDEO (~52600 ns)
 * |-------------------||------------------------------------------------------|
 *
 *
 *   WITHIN HBLANK PERIOD:
 *
 *   FP (~1500 ns)  SYNC (~4700 ns)  BW (~600 ns)  CB (~2500 ns)  BP (~1600 ns)
 * |--------------||---------------||------------||-------------||-------------|
 *      BLANK            SYNC           BLANK          BLANK          BLANK
 *
 */
#define LINE_BEG         0
#define FP_ns            1500      /* front porch */
#define SYNC_ns          4700      /* sync tip */
#define BW_ns            600       /* breezeway */
#define CB_ns            2500      /* color burst */
#define BP_ns            1600      /* back porch */
#define AV_ns            52600     /* active video */
#define HB_ns            (FP_ns + SYNC_ns + BW_ns + CB_ns + BP_ns) /* h blank */
/* line duration should be ~63500 ns */
#define LINE_ns          (FP_ns + SYNC_ns + BW_ns + CB_ns + BP_ns + AV_ns)

/* convert nanosecond offset to its corresponding point on the sampled line */
#define ns2pos(ns)       ((ns) * CRT_HRES / LINE_ns)
/* starting points for all the different pulses */
#define FP_BEG           ns2pos(0)
#define SYNC_BEG         ns2pos(FP_ns)
#define BW_BEG           ns2pos(FP_ns + SYNC_ns)
#define CB_BEG           ns2pos(FP_ns + SYNC_ns + BW_ns)
#define BP_BEG           ns2pos(FP_ns + SYNC_ns + BW_ns + CB_ns)
#define AV_BEG           ns2pos(HB_ns)
#define AV_LEN           ns2pos(AV_ns)

/* somewhere between 7 and 12 cycles */
#define CB_CYCLES   10

/* frequencies for bandlimiting */
#define L_FREQ           1431818 /* full line */
#define Y_FREQ           420000  /* Luma   (Y) 4.2  MHz of the 14.31818 MHz */
#define I_FREQ           150000  /* Chroma (I) 1.5  MHz of the 14.31818 MHz */
#define Q_FREQ           55000   /* Chroma (Q) 0.55 MHz of the 14.31818 MHz */

/* IRE units (100 = 1.0V, -40 = 0.0V) */
#define WHITE_LEVEL      100
#define BURST_LEVEL      20
#define BLACK_LEVEL      7
#define BLANK_LEVEL      0
#define SYNC_LEVEL      -40

struct NTSC_SETTINGS {
    const unsigned char *data; /* image data */
    int format;     /* pix format (one of the CRT_PIX_FORMATs) */
    int w, h;       /* width and height of image */
    int raw;        /* 0 = scale image to fit monitor, 1 = don't scale */
    int as_color;   /* 0 = monochrome, 1 = full color */
    int field;      /* 0 = even, 1 = odd */
    int frame;      /* 0 = even, 1 = odd */
    int hue;        /* 0-359 */
    int xoffset;    /* x offset in sample space. 0 is minimum value */
    int yoffset;    /* y offset in # of lines. 0 is minimum value */
    /* make sure your NTSC_SETTINGS struct is zeroed out before you do anything */
    int iirs_initialized; /* internal state */
};

/*****************************************************************************/
/********************** CRT struct (from crt_core.h) *************************/
/*****************************************************************************/

/* One active line's decoding parameters, from crt_demodulate_begin. */
struct crt_line {
    int skip;
    int beg, end;
    unsigned pos;
#if (CRT_CC_SAMPLES == 4)
    int wave[CRT_CC_SAMPLES];
#else
    int waveI[CRT_CC_SAMPLES];
    int waveQ[CRT_CC_SAMPLES];
#endif
    int dx, scanL;
    unsigned scanR;
    int L, R;
};

struct crt_yiq {
    int y, i, q;
};

struct CRT {
    signed char analog[CRT_INPUT_SIZE];
    signed char inp[CRT_INPUT_SIZE]; /* CRT input, can be noisy */

    int outw, outh; /* output width/height */
    int out_format; /* output pixel format (one of the CRT_PIX_FORMATs) */
    unsigned char *out; /* output image */

    int hue, brightness, contrast, saturation; /* common monitor settings */
    int black_point, white_point; /* user-adjustable */
    int scanlines; /* leave gaps between lines if necessary */
    int blend; /* blend new field onto previous image */
    unsigned v_fac; /* factor to stretch img vertically onto the output img */

    /* internal data */
    int ccf[CRT_CC_VPER][CRT_CC_SAMPLES]; /* faster color carrier convergence */
    int hsync, vsync; /* keep track of sync over frames */
    int rn; /* seed for the 'random' noise */

    /* this frame's lines, from crt_demodulate_begin */
    struct crt_line lines[CRT_LINES];
    int bright;
    int frame_ok;
};

/*****************************************************************************/
/*************************** FIXED POINT SIN/COS *****************************/
/*****************************************************************************/

#define T14_2PI           16384
#define T14_MASK          (T14_2PI - 1)
#define T14_PI            (T14_2PI / 2)

/*****************************************************************************/
/*****************************************************************************/
/****            crt_core.c - the demodulator implementation              ****/
/*****************************************************************************/
/*****************************************************************************/

/* ensure negative values for x get properly modulo'd */
#define POSMOD(x, n)     (((x) % (n) + (n)) % (n))

static int sigpsin15[18] = { /* significant points on sine wave (15-bit) */
    0x0000,
    0x0c88,0x18f8,0x2528,0x30f8,0x3c50,0x4718,0x5130,0x5a80,
    0x62f0,0x6a68,0x70e0,0x7640,0x7a78,0x7d88,0x7f60,0x8000,
    0x7f60
};

static int
sintabil8(int n)
{
    int f, i, a, b;

    /* looks scary but if you don't change T14_2PI
     * it won't cause out of bounds memory reads
     */
    f = n >> 0 & 0xff;
    i = n >> 8 & 0xff;
    a = sigpsin15[i];
    b = sigpsin15[i + 1];
    return (a + ((b - a) * f >> 8));
}

/* 14-bit interpolated sine/cosine */
static void
crt_sincos14(int *s, int *c, int n)
{
    int h;

    n &= T14_MASK;
    h = n & ((T14_2PI >> 1) - 1);

    if (h > ((T14_2PI >> 2) - 1)) {
        *c = -sintabil8(h - (T14_2PI >> 2));
        *s = sintabil8((T14_2PI >> 1) - h);
    } else {
        *c = sintabil8((T14_2PI >> 2) - h);
        *s = sintabil8(h);
    }
    if (n > ((T14_2PI >> 1) - 1)) {
        *c = -*c;
        *s = -*s;
    }
}

static int
crt_bpp4fmt(int format)
{
    switch (format) {
        case CRT_PIX_FORMAT_RGB:
        case CRT_PIX_FORMAT_BGR:
            return 3;
        case CRT_PIX_FORMAT_ARGB:
        case CRT_PIX_FORMAT_RGBA:
        case CRT_PIX_FORMAT_ABGR:
        case CRT_PIX_FORMAT_BGRA:
            return 4;
        default:
            return 0;
    }
}

/*****************************************************************************/
/********************************* FILTERS ***********************************/
/*****************************************************************************/

/* convolution is much faster but the EQ looks softer, more authentic, and more analog */
#define USE_CONVOLUTION 0
#define USE_7_SAMPLE_KERNEL 1
#define USE_6_SAMPLE_KERNEL 0
#define USE_5_SAMPLE_KERNEL 0

#if (CRT_CC_SAMPLES != 4)
/* the current convolutions do not filter properly at > 4 samples */
#undef USE_CONVOLUTION
#define USE_CONVOLUTION 0
#endif

#if USE_CONVOLUTION

/* NOT 3 band equalizer, faster convolution instead.
 * eq function names preserved to keep code clean
 */
static struct EQF {
    int h[7];
} eqY, eqI, eqQ;

/* params unused to keep the function the same */
static void
init_eq(struct EQF *f,
        int f_lo, int f_hi, int rate,
        int g_lo, int g_mid, int g_hi)
{
    memset(f, 0, sizeof(struct EQF));
}

static void
reset_eq(struct EQF *f)
{
    memset(f->h, 0, sizeof(f->h));
}

static int
eqf(struct EQF *f, int s)
{
    int i;
    int *h = f->h;

    for (i = 6; i > 0; i--) {
        h[i] = h[i - 1];
    }
    h[0] = s;
#if USE_7_SAMPLE_KERNEL
    /* index : 0 1 2 3 4 5 6 */
    /* weight: 1 4 7 8 7 4 1 */
    return (s + h[6] + ((h[1] + h[5]) * 4) + ((h[2] + h[4]) * 7) + (h[3] * 8)) >> 5;
#elif USE_6_SAMPLE_KERNEL
    /* index : 0 1 2 3 4 5 */
    /* weight: 1 3 4 4 3 1 */
    return (s + h[5] + 3 * (h[1] + h[4]) + 4 * (h[2] + h[3])) >> 4;
#elif USE_5_SAMPLE_KERNEL
    /* index : 0 1 2 3 4 */
    /* weight: 1 2 2 2 1 */
    return (s + h[4] + ((h[1] + h[2] + h[3]) << 1)) >> 3;
#else
    /* index : 0 1 2 3 */
    /* weight: 1 1 1 1*/
    return (s + h[3] + h[1] + h[2]) >> 2;
#endif
}

#else

#define HISTLEN     3
#define HISTOLD     (HISTLEN - 1) /* oldest entry */
#define HISTNEW     0             /* newest entry */

#define EQ_P        16 /* if changed, the gains will need to be adjusted */
#define EQ_R        (1 << (EQ_P - 1)) /* rounding */
/* three band equalizer */
static struct EQF {
    int lf, hf; /* fractions */
    int g[3]; /* gains */
    int fL[4];
    int fH[4];
    int h[HISTLEN]; /* history */
} eqY, eqI, eqQ;

/* f_lo - low cutoff frequency
 * f_hi - high cutoff frequency
 * rate - sampling rate
 * g_lo, g_mid, g_hi - gains
 */
static void
init_eq(struct EQF *f,
        int f_lo, int f_hi, int rate,
        int g_lo, int g_mid, int g_hi)
{
    int sn, cs;

    memset(f, 0, sizeof(struct EQF));

    f->g[0] = g_lo;
    f->g[1] = g_mid;
    f->g[2] = g_hi;

    crt_sincos14(&sn, &cs, T14_PI * f_lo / rate);
#if (EQ_P >= 15)
    f->lf = 2 * (sn << (EQ_P - 15));
#else
    f->lf = 2 * (sn >> (15 - EQ_P));
#endif
    crt_sincos14(&sn, &cs, T14_PI * f_hi / rate);
#if (EQ_P >= 15)
    f->hf = 2 * (sn << (EQ_P - 15));
#else
    f->hf = 2 * (sn >> (15 - EQ_P));
#endif
}

static void
reset_eq(struct EQF *f)
{
    memset(f->fL, 0, sizeof(f->fL));
    memset(f->fH, 0, sizeof(f->fH));
    memset(f->h, 0, sizeof(f->h));
}

/* At four samples per chroma period the decoder runs this through
 * EQF_STEP below instead. */
#if (CRT_CC_SAMPLES != 4)
static int
eqf(struct EQF *f, int s)
{
    int i, r[3];

    f->fL[0] += (f->lf * (s - f->fL[0]) + EQ_R) >> EQ_P;
    f->fH[0] += (f->hf * (s - f->fH[0]) + EQ_R) >> EQ_P;

    for (i = 1; i < 4; i++) {
        f->fL[i] += (f->lf * (f->fL[i - 1] - f->fL[i]) + EQ_R) >> EQ_P;
        f->fH[i] += (f->hf * (f->fH[i - 1] - f->fH[i]) + EQ_R) >> EQ_P;
    }

    r[0] = f->fL[3];
    r[1] = f->fH[3] - f->fL[3];
    r[2] = f->h[HISTOLD] - f->fH[3];

    for (i = 0; i < 3; i++) {
        r[i] = (r[i] * f->g[i]) >> EQ_P;
    }

    for (i = HISTOLD; i > 0; i--) {
        f->h[i] = f->h[i - 1];
    }
    f->h[HISTNEW] = s;

    return (r[0] + r[1] + r[2]);
}
#endif

/* eqf() on locals: P names a filter's state (lf, hf, the gains, the
 * four low and high stages and the history), loaded from a struct EQF
 * by EQF_LOCALS. EQF_STEP runs one sample through it exactly as eqf()
 * does and leaves the result in OUT. */
#define EQF_LOCALS(P, F) \
    const int P##lf = (F).lf, P##hf = (F).hf; \
    const int P##g0 = (F).g[0], P##g1 = (F).g[1], P##g2 = (F).g[2]; \
    int P##L0 = (F).fL[0], P##L1 = (F).fL[1], P##L2 = (F).fL[2], P##L3 = (F).fL[3]; \
    int P##H0 = (F).fH[0], P##H1 = (F).fH[1], P##H2 = (F).fH[2], P##H3 = (F).fH[3]; \
    int P##h0 = (F).h[0], P##h1 = (F).h[1], P##h2 = (F).h[2]

#define EQF_STEP(P, S, OUT) \
    do { \
        int s_ = (S); \
        P##L0 += (P##lf * (s_ - P##L0) + EQ_R) >> EQ_P; \
        P##H0 += (P##hf * (s_ - P##H0) + EQ_R) >> EQ_P; \
        P##L1 += (P##lf * (P##L0 - P##L1) + EQ_R) >> EQ_P; \
        P##H1 += (P##hf * (P##H0 - P##H1) + EQ_R) >> EQ_P; \
        P##L2 += (P##lf * (P##L1 - P##L2) + EQ_R) >> EQ_P; \
        P##H2 += (P##hf * (P##H1 - P##H2) + EQ_R) >> EQ_P; \
        P##L3 += (P##lf * (P##L2 - P##L3) + EQ_R) >> EQ_P; \
        P##H3 += (P##hf * (P##H2 - P##H3) + EQ_R) >> EQ_P; \
        (OUT) = ((P##L3 * P##g0) >> EQ_P) \
              + (((P##H3 - P##L3) * P##g1) >> EQ_P) \
              + (((P##h2 - P##H3) * P##g2) >> EQ_P); \
        P##h2 = P##h1; \
        P##h1 = P##h0; \
        P##h0 = s_; \
    } while (0)

#endif

/*****************************************************************************/
/***************************** PUBLIC FUNCTIONS ******************************/
/*****************************************************************************/

static void
crt_resize(struct CRT *v, int w, int h, int f, unsigned char *out)
{
    v->outw = w;
    v->outh = h;
    v->out_format = f;
    v->out = out;
}

static void
crt_reset(struct CRT *v)
{
    v->hue = 0;
    v->saturation = 10;
    v->brightness = 0;
    v->contrast = 180;
    v->black_point = 0;
    v->white_point = 100;
    v->hsync = 0;
    v->vsync = 0;
}

static void
crt_init(struct CRT *v, int w, int h, int f, unsigned char *out)
{
    memset(v, 0, sizeof(struct CRT));
    crt_resize(v, w, h, f, out);
    crt_reset(v);
    v->rn = 194;

    /* kilohertz to line sample conversion */
#define kHz2L(kHz) (CRT_HRES * (kHz * 100) / L_FREQ)

    /* band gains are pre-scaled as 16-bit fixed point
     * if you change the EQ_P define, you'll need to update these gains too
     */
#if (CRT_CC_SAMPLES == 4)
    init_eq(&eqY, kHz2L(1500), kHz2L(3000), CRT_HRES, 65536, 8192, 9175);
    init_eq(&eqI, kHz2L(80),   kHz2L(1150), CRT_HRES, 65536, 65536, 1311);
    init_eq(&eqQ, kHz2L(80),   kHz2L(1000), CRT_HRES, 65536, 65536, 0);
#elif (CRT_CC_SAMPLES == 5)
    init_eq(&eqY, kHz2L(1500), kHz2L(3000), CRT_HRES, 65536, 12192, 7775);
    init_eq(&eqI, kHz2L(80),   kHz2L(1150), CRT_HRES, 65536, 65536, 1311);
    init_eq(&eqQ, kHz2L(80),   kHz2L(1000), CRT_HRES, 65536, 65536, 0);
#else
#error "NTSC-CRT currently only supports 4 or 5 samples per chroma period."
#endif

}

/* SIMD tiers for the passes that run several lines or samples at once
 * (see "Several lines at once" below). */
#define CRT_LANES_MAX 16

#if (CRT_CC_SAMPLES == 4) && !USE_CONVOLUTION && !CRT_DO_BLOOM \
    && (defined(__x86_64__) || defined(__i386__) \
     || defined(_M_X64) || defined(_M_IX86))
#if defined(_MSC_VER) && !defined(__clang__)
#if _MSC_VER >= 1911
#define CRT_HAVE_LANES_AVX2   1
#define CRT_HAVE_LANES_AVX512 1
#define CRT_TARGET_AVX2
#define CRT_TARGET_AVX512
#endif
#elif defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
/* PlayStation clang and clang-cl declare only the intrinsics of ISAs
 * the whole build enables, so a tier is built only where the baseline
 * has it. */
#ifdef __AVX2__
#define CRT_HAVE_LANES_AVX2 1
#define CRT_TARGET_AVX2
#ifdef __AVX512F__
#define CRT_HAVE_LANES_AVX512 1
#define CRT_TARGET_AVX512
#endif
#endif
#elif (defined(__clang__) && __clang_major__ >= 4) \
   || (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5)
#define CRT_HAVE_LANES_AVX2   1
#define CRT_HAVE_LANES_AVX512 1
#define CRT_TARGET_AVX2   __attribute__((target("avx2")))
#define CRT_TARGET_AVX512 __attribute__((target("avx512f")))
#endif
#endif

#if defined(CRT_HAVE_LANES_AVX2) || defined(CRT_HAVE_LANES_AVX512)
#define CRT_HAVE_LANES 1
#include <immintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define CRT_ZEROUPPER() _mm256_zeroupper()
#else
#define CRT_ZEROUPPER()
#endif
#endif

/* The noise generator: rn' = a * rn + c in 32 bits. Applying it k times
 * is again one multiply and add, with A = a^k and C = c * (a^(k-1) +
 * ... + 1), all mod 2^32, so a lane can start k steps ahead and step k
 * at a time and see exactly the numbers the one-at-a-time loop does. */
#define CRT_LCG_A 214019u
#define CRT_LCG_C 140327895u

static void crt_lcg_coef(unsigned k, unsigned *A, unsigned *C)
{
    unsigned ra = 1, rc = 0, ba = CRT_LCG_A, bc = CRT_LCG_C;
    while (k) {
        if (k & 1) {
            rc = ba * rc + bc;
            ra = ba * ra;
        }
        bc = ba * bc + bc;
        ba = ba * ba;
        k >>= 1;
    }
    *A = ra;
    *C = rc;
}

static unsigned crt_lcg_jump(unsigned rn, unsigned k)
{
    unsigned A, C;
    crt_lcg_coef(k, &A, &C);
    return A * rn + C;
}

#ifdef CRT_HAVE_LANES
static size_t crt_noise_lanes(int lanes, const signed char *an,
        signed char *out, size_t n, unsigned rn, int nn);
#endif

/* Noise for samples [first, last) of the signal, into v->inp: sample i
 * gets the generator's (i + 1)th number from v->rn, so ranges can be
 * done in any order, or at once. v->rn moves on in demodulate_begin. */
static void
crt_noise_range(struct CRT *v, size_t first, size_t last, int noise,
        int lanes)
{
    size_t i  = first;
    unsigned rn = crt_lcg_jump((unsigned)v->rn, (unsigned)first);
#ifdef CRT_HAVE_LANES
    /* Most of it in lanes, the rest below from the state they reached */
    if (lanes) {
        size_t done = crt_noise_lanes(lanes, v->analog + first,
                v->inp + first, last - first, rn, noise);
        rn  = crt_lcg_jump(rn, (unsigned)done);
        i  += done;
    }
#else
    (void)lanes;
#endif
    for (; i < last; i++) {
        int s;
        /* The LCG wraps by design; in unsigned it does so defined. */
        rn = CRT_LCG_A * rn + CRT_LCG_C;
        /* signal + noise */
        s = v->analog[i] + ((((int)((rn >> 16) & 0xff)) - 0x7f) * noise >> 8);
        if (s >  127) { s =  127; }
        if (s < -127) { s = -127; }
        v->inp[i] = (signed char)s;
    }
}

/* The first half of demodulation, in line order, once the noise is in
 * (crt_noise_range over the whole signal): vertical sync,
 * then for each active line the horizontal sync search and colour carrier
 * accumulation - which carry from one line to the next - and the
 * parameters the rest of the line's decoding needs, into v->lines. */
static void
crt_demodulate_begin(struct CRT *v, int noise)
{
    int i, j, line;
    signed char *sig;
    int s = 0;
    int field, ratio;
    int *ccr; /* color carrier signal */
    int huesn, huecs;
    int xnudge = -3, ynudge = 3;
    int bright = v->brightness - (BLACK_LEVEL + v->black_point);
    int bpp, pitch;
#if CRT_DO_BLOOM
    int prev_e; /* filtered beam energy per scan line */
    int max_e; /* approx maximum energy in a scan line */
#endif

#if !CRT_DO_BLOOM
    (void)noise;
#endif
    bpp = crt_bpp4fmt(v->out_format);
    v->frame_ok = (bpp != 0);
    if (bpp == 0) {
        return;
    }
    pitch = v->outw * bpp;
    (void)pitch;
    v->bright = bright;

    crt_sincos14(&huesn, &huecs, ((v->hue % 360) + 33) * 8192 / 180);
    huesn >>= 11; /* make 4-bit */
    huecs >>= 11;

#if !CRT_DO_VSYNC
    /* determine field before we add noise,
     * otherwise it's not reliably recoverable
     */
    for (i = -CRT_VSYNC_WINDOW; i < CRT_VSYNC_WINDOW; i++) {
        line = POSMOD(v->vsync + i, CRT_VRES);
        sig = v->analog + line * CRT_HRES;
        s = 0;
        for (j = 0; j < CRT_HRES; j++) {
            s += sig[j];
            if (s <= (CRT_VSYNC_THRESH * SYNC_LEVEL)) {
                goto found_field;
            }
        }
    }
found_field:
    /* if vsync signal was in second half of line, odd field */
    field = (j > (CRT_HRES / 2));
    v->vsync = -3;
#endif
    /* The generator's state after the whole signal's noise */
    v->rn = (int)crt_lcg_jump((unsigned)v->rn, CRT_INPUT_SIZE);

#if CRT_DO_VSYNC
    /* Look for vertical sync.
     *
     * This is done by integrating the signal and
     * seeing if it exceeds a threshold. The threshold of
     * the vertical sync pulse is much higher because the
     * vsync pulse is a lot longer than the hsync pulse.
     * The signal needs to be integrated to lessen
     * the noise in the signal.
     */
    for (i = -CRT_VSYNC_WINDOW; i < CRT_VSYNC_WINDOW; i++) {
        line = POSMOD(v->vsync + i, CRT_VRES);
        sig = v->inp + line * CRT_HRES;
        s = 0;
        for (j = 0; j < CRT_HRES; j++) {
            s += sig[j];
            /* increase the multiplier to make the vsync
             * more stable when there is a lot of noise
             */
            if (s <= (CRT_VSYNC_THRESH * SYNC_LEVEL)) {
                goto vsync_found;
            }
        }
    }
vsync_found:
    v->vsync = line; /* vsync found (or gave up) at this line */
    /* if vsync signal was in second half of line, odd field */
    field = (j > (CRT_HRES / 2));
#endif

#if CRT_DO_BLOOM
    max_e = (128 + (noise / 2)) * AV_LEN;
    prev_e = (16384 / 8);
#endif
    /* ratio of output height to active video lines in the signal */
    ratio = (v->outh << 16) / CRT_LINES;
    ratio = (ratio + 32768) >> 16;

    field = (field * (ratio / 2));

    for (line = CRT_TOP; line < CRT_BOT; line++) {
        struct crt_line *cl;
        unsigned pos, ln, scanR;
        int scanL, dx;
        int L, R;
#if (CRT_CC_SAMPLES == 4)
        int wave[CRT_CC_SAMPLES];
#else
        int waveI[CRT_CC_SAMPLES];
        int waveQ[CRT_CC_SAMPLES];
#endif
        int dci, dcq; /* decoded I, Q */
        int xpos, ypos;
        int beg, end;
        int phasealign;
#if CRT_DO_BLOOM
        int line_w;
#endif

        beg = (line - CRT_TOP + 0) * (v->outh + v->v_fac) / CRT_LINES + field;
        end = (line - CRT_TOP + 1) * (v->outh + v->v_fac) / CRT_LINES + field;

        cl       = &v->lines[line - CRT_TOP];
        cl->skip = 1;
        cl->beg  = beg;
        cl->end  = end;
        if (beg >= v->outh) { continue; }
        if (end > v->outh) { end = v->outh; }

        /* Look for horizontal sync.
         * See comment above regarding vertical sync.
         */
        ln = (POSMOD(line + v->vsync, CRT_VRES)) * CRT_HRES;
        sig = v->inp + ln + v->hsync;
        s = 0;
        for (i = -CRT_HSYNC_WINDOW; i < CRT_HSYNC_WINDOW; i++) {
            s += sig[SYNC_BEG + i];
            if (s <= (CRT_HSYNC_THRESH * SYNC_LEVEL)) {
                break;
            }
        }
#if CRT_DO_HSYNC
        v->hsync = POSMOD(i + v->hsync, CRT_HRES);
#else
        v->hsync = 0;
#endif

        xpos = POSMOD(AV_BEG + v->hsync + xnudge, CRT_HRES);
        ypos = POSMOD(line + v->vsync + ynudge, CRT_VRES);
        pos = xpos + ypos * CRT_HRES;

        ccr = v->ccf[ypos % CRT_CC_VPER];
#if (CRT_CC_SAMPLES == 4)
        sig = v->inp + ln + (v->hsync & ~3); /* faster */
#else
        sig = v->inp + ln + (v->hsync - (v->hsync % CRT_CC_SAMPLES));
#endif
        for (i = CB_BEG; i < CB_BEG + (CB_CYCLES * CRT_CB_FREQ); i++) {
            int p, n;
            p = ccr[i % CRT_CC_SAMPLES] * 127 / 128; /* fraction of the previous */
            n = sig[i];                 /* mixed with the new sample */
            ccr[i % CRT_CC_SAMPLES] = p + n;
        }

        phasealign = POSMOD(v->hsync, CRT_CC_SAMPLES);

#if (CRT_CC_SAMPLES == 4)
        /* amplitude of carrier = saturation, phase difference = hue */
        dci = ccr[(phasealign + 1) & 3] - ccr[(phasealign + 3) & 3];
        dcq = ccr[(phasealign + 2) & 3] - ccr[(phasealign + 0) & 3];

        wave[0] = ((dci * huecs - dcq * huesn) >> 4) * v->saturation;
        wave[1] = ((dcq * huecs + dci * huesn) >> 4) * v->saturation;
        wave[2] = -wave[0];
        wave[3] = -wave[1];
#elif (CRT_CC_SAMPLES == 5)
        {
            int dciA, dciB;
            int dcqA, dcqB;
            int ang = (v->hue % 360);
            int off180 = CRT_CC_SAMPLES / 2;
            int off90 = CRT_CC_SAMPLES / 4;
            int peakA = phasealign + off90;
            int peakB = phasealign + 0;
            dciA = dciB = dcqA = dcqB = 0;
            /* amplitude of carrier = saturation, phase difference = hue */
            dciA = ccr[(peakA) % CRT_CC_SAMPLES];
            /* average */
            dciB = (ccr[(peakA + off180) % CRT_CC_SAMPLES]
                  + ccr[(peakA + off180 + 1) % CRT_CC_SAMPLES]) / 2;
            dcqA = ccr[(peakB + off180) % CRT_CC_SAMPLES];
            dcqB = ccr[(peakB) % CRT_CC_SAMPLES];
            dci = dciA - dciB;
            dcq = dcqA - dcqB;
            /* create wave tables and rotate them by the hue adjustment angle */
            for (i = 0; i < CRT_CC_SAMPLES; i++) {
                int sn, cs;
                crt_sincos14(&sn, &cs, ang * 8192 / 180);
                waveI[i] = ((dci * cs + dcq * sn) >> 15) * v->saturation;
                /* Q is offset by 90 */
                crt_sincos14(&sn, &cs, (ang + 90) * 8192 / 180);
                waveQ[i] = ((dci * cs + dcq * sn) >> 15) * v->saturation;
                ang += (360 / CRT_CC_SAMPLES);
            }
        }
#endif
        sig = v->inp + pos;
#if CRT_DO_BLOOM
        s = 0;
        for (i = 0; i < AV_LEN; i++) {
            s += sig[i]; /* sum up the scan line */
        }
        /* bloom emulation */
        prev_e = (prev_e * 123 / 128) + ((((max_e >> 1) - s) << 10) / max_e);
        line_w = (AV_LEN * 112 / 128) + (prev_e >> 9);

        dx = (line_w << 12) / v->outw;
        scanL = ((AV_LEN / 2) - (line_w >> 1) + 8) << 12;
        scanR = (AV_LEN - 1) << 12;

        L = (scanL >> 12);
        R = (scanR >> 12);
#else
        dx = ((AV_LEN - 1) << 12) / v->outw;
        scanL = 0;
        scanR = (AV_LEN - 1) << 12;
        L = 0;
        R = AV_LEN;
#endif
        cl->skip  = 0;
        cl->beg   = beg;
        cl->end   = end;
        cl->pos   = pos;
#if (CRT_CC_SAMPLES == 4)
        memcpy(cl->wave, wave, sizeof(cl->wave));
#else
        memcpy(cl->waveI, waveI, sizeof(cl->waveI));
        memcpy(cl->waveQ, waveQ, sizeof(cl->waveQ));
#endif
        cl->dx    = dx;
        cl->scanL = scanL;
        cl->scanR = scanR;
        cl->L     = L;
        cl->R     = R;
    }
}

/* The second half, for lines [first, last) of the active lines: the
 * three equalisers and the resampling into the output rows each line
 * owns. Lines are independent here, so bands of them can be decoded at
 * once; 'out' is AV_LEN + 1 entries of scratch per caller. */
/* ---- Several lines at once -------------------------------------------
 * Lines are independent once demodulate_begin has run, and without
 * bloom they all cover the same samples, so up to 16 run in lockstep,
 * one per SIMD lane (AVX2: 8, AVX-512: 16). Each lane does exactly the
 * scalar decoder's integer operations in the same order - multiplies
 * keep the low 32 bits and shifts are arithmetic, as int arithmetic is
 * on every target this builds for - so the output is unchanged.
 *
 * The lanes' signal is transposed into one byte per lane per sample,
 * and the decoded Y, I and Q are left interleaved likewise, sample s of
 * lane k at [s * lanes + k], for crt_render_line to read in place. The
 * buffers are made at create; the tier comes from the SIMD mask
 * RetroArch passes there, so the plugin needs nothing of its own to
 * detect the CPU. */
struct crt_lanes {
    int n;            /* lanes per batch: 0 (scalar only), 8 or 16 */
    signed char *sig; /* [AV_LEN + 1][n] */
    int *y, *i, *q;   /* [AV_LEN + 1][n] each */
};

/* One frame's encoding parameters, from crt_modulate_begin, for
 * crt_modulate_lines: the active area, levels, filter coefficients,
 * carrier phases and each sample's source column (col) and its byte
 * offset in an RGB row (col3). */
struct crt_mod_frame {
    int destw, desth;
    int xo, yo;
    int white, black;
    int cY, cI, cQ;
    int field_offset;
    int modI[4], modQ[4];
    int col[AV_LEN];
    int col3[AV_LEN];
};

/* The encoder's lanes: one source row per lane, converted to Y, I and Q
 * once per source pixel ([w_cap][n] each), and the signal levels the
 * lanes produce, [AV_LEN][n], before they go to their rows. */
struct crt_mod_lanes {
    int n;
    unsigned w_cap;
    int *y, *i, *q;
    int *out;
};

#ifdef CRT_HAVE_LANES

/* One equaliser step on vectors of lanes: EQF_STEP's updates, in its
 * order, with the history shifted after the output is formed. */
#define CRT_EQ_STEP_V(ADD, SUB, MUL, SRAI, S, O) \
    L0 = ADD(L0, SRAI(ADD(MUL(lf, SUB(S, L0)), rnd), EQ_P)); \
    H0 = ADD(H0, SRAI(ADD(MUL(hf, SUB(S, H0)), rnd), EQ_P)); \
    L1 = ADD(L1, SRAI(ADD(MUL(lf, SUB(L0, L1)), rnd), EQ_P)); \
    H1 = ADD(H1, SRAI(ADD(MUL(hf, SUB(H0, H1)), rnd), EQ_P)); \
    L2 = ADD(L2, SRAI(ADD(MUL(lf, SUB(L1, L2)), rnd), EQ_P)); \
    H2 = ADD(H2, SRAI(ADD(MUL(hf, SUB(H1, H2)), rnd), EQ_P)); \
    L3 = ADD(L3, SRAI(ADD(MUL(lf, SUB(L2, L3)), rnd), EQ_P)); \
    H3 = ADD(H3, SRAI(ADD(MUL(hf, SUB(H2, H3)), rnd), EQ_P)); \
    O  = ADD(ADD(SRAI(MUL(L3, g0), EQ_P), \
                 SRAI(MUL(SUB(H3, L3), g1), EQ_P)), \
             SRAI(MUL(SUB(h2, H3), g2), EQ_P)); \
    h2 = h1; \
    h1 = h0; \
    h0 = S

#ifdef CRT_HAVE_LANES_AVX2
#define CRT_A2_ADD(a, b)  _mm256_add_epi32(a, b)
#define CRT_A2_SUB(a, b)  _mm256_sub_epi32(a, b)
#define CRT_A2_MUL(a, b)  _mm256_mullo_epi32(a, b)
#define CRT_A2_SRAI(a, n) _mm256_srai_epi32(a, n)

/* One equaliser (Y when wave is NULL, else I or Q) over samples [L, R)
 * of 8 lanes. */
CRT_TARGET_AVX2
static void crt_eq_pass_avx2(const struct EQF *t, const signed char *sig,
        int L, int R, const int *wave, int bright, int *dst)
{
    const __m256i lf  = _mm256_set1_epi32(t->lf);
    const __m256i hf  = _mm256_set1_epi32(t->hf);
    const __m256i g0  = _mm256_set1_epi32(t->g[0]);
    const __m256i g1  = _mm256_set1_epi32(t->g[1]);
    const __m256i g2  = _mm256_set1_epi32(t->g[2]);
    const __m256i rnd = _mm256_set1_epi32(EQ_R);
    const __m256i vb  = _mm256_set1_epi32(bright);
    __m256i L0 = _mm256_setzero_si256(), L1 = L0, L2 = L0, L3 = L0;
    __m256i H0 = L0, H1 = L0, H2 = L0, H3 = L0;
    __m256i h0 = L0, h1 = L0, h2 = L0;
    int i;
    for (i = L; i < R; i++) {
        __m256i sv = _mm256_cvtepi8_epi32(
                _mm_loadl_epi64((const __m128i*)(sig + (size_t)i * 8)));
        __m256i o;
        if (wave) {
            sv = _mm256_srai_epi32(_mm256_mullo_epi32(sv,
                        _mm256_loadu_si256((const __m256i*)
                            (wave + (i & 3) * 8))), 9);
        } else {
            sv = _mm256_add_epi32(sv, vb);
        }
        CRT_EQ_STEP_V(CRT_A2_ADD, CRT_A2_SUB, CRT_A2_MUL, CRT_A2_SRAI, sv, o);
        o = wave ? _mm256_srai_epi32(o, 3) : _mm256_slli_epi32(o, 4);
        _mm256_storeu_si256((__m256i*)(dst + (size_t)i * 8), o);
    }
    CRT_ZEROUPPER();
}
#endif

#ifdef CRT_HAVE_LANES_AVX512
#define CRT_A5_ADD(a, b)  _mm512_add_epi32(a, b)
#define CRT_A5_SUB(a, b)  _mm512_sub_epi32(a, b)
#define CRT_A5_MUL(a, b)  _mm512_mullo_epi32(a, b)
#define CRT_A5_SRAI(a, n) _mm512_srai_epi32(a, n)

/* The same over 16 lanes. */
CRT_TARGET_AVX512
static void crt_eq_pass_avx512(const struct EQF *t, const signed char *sig,
        int L, int R, const int *wave, int bright, int *dst)
{
    const __m512i lf  = _mm512_set1_epi32(t->lf);
    const __m512i hf  = _mm512_set1_epi32(t->hf);
    const __m512i g0  = _mm512_set1_epi32(t->g[0]);
    const __m512i g1  = _mm512_set1_epi32(t->g[1]);
    const __m512i g2  = _mm512_set1_epi32(t->g[2]);
    const __m512i rnd = _mm512_set1_epi32(EQ_R);
    const __m512i vb  = _mm512_set1_epi32(bright);
    __m512i L0 = _mm512_setzero_si512(), L1 = L0, L2 = L0, L3 = L0;
    __m512i H0 = L0, H1 = L0, H2 = L0, H3 = L0;
    __m512i h0 = L0, h1 = L0, h2 = L0;
    int i;
    for (i = L; i < R; i++) {
        __m512i sv = _mm512_cvtepi8_epi32(
                _mm_loadu_si128((const __m128i*)(sig + (size_t)i * 16)));
        __m512i o;
        if (wave) {
            sv = _mm512_srai_epi32(_mm512_mullo_epi32(sv,
                        _mm512_loadu_si512((const void*)
                            (wave + (i & 3) * 16))), 9);
        } else {
            sv = _mm512_add_epi32(sv, vb);
        }
        CRT_EQ_STEP_V(CRT_A5_ADD, CRT_A5_SUB, CRT_A5_MUL, CRT_A5_SRAI, sv, o);
        o = wave ? _mm512_srai_epi32(o, 3) : _mm512_slli_epi32(o, 4);
        _mm512_storeu_si512((void*)(dst + (size_t)i * 16), o);
    }
    CRT_ZEROUPPER();
}
#endif

/* Decodes the lanes' lines idx[0..n) into lanes->y/i/q. */
static void crt_decode_lanes(struct CRT *v, const int *idx,
        const struct crt_lanes *lanes, int bright)
{
    const int nl = lanes->n;
    const int L  = v->lines[idx[0]].L;
    const int R  = v->lines[idx[0]].R;
    /* Each lane's carrier for I (wave[i & 3]) and Q (wave[(i + 3) & 3])
     * at the four sample phases. */
    int wi[4 * CRT_LANES_MAX], wq[4 * CRT_LANES_MAX];
    int k, m, i;

    for (k = 0; k < nl; k++) {
        const struct crt_line *cl = &v->lines[idx[k]];
        const signed char *src    = v->inp + cl->pos;
        signed char *dst          = lanes->sig + k;
        for (i = L; i < R; i++) {
            dst[(size_t)i * nl] = src[i];
        }
        for (m = 0; m < 4; m++) {
            wi[m * nl + k] = cl->wave[m];
            wq[m * nl + k] = cl->wave[(m + 3) & 3];
        }
    }

#ifdef CRT_HAVE_LANES_AVX512
    if (nl == 16) {
        crt_eq_pass_avx512(&eqY, lanes->sig, L, R, NULL, bright, lanes->y);
        crt_eq_pass_avx512(&eqI, lanes->sig, L, R, wi,   bright, lanes->i);
        crt_eq_pass_avx512(&eqQ, lanes->sig, L, R, wq,   bright, lanes->q);
        return;
    }
#endif
#ifdef CRT_HAVE_LANES_AVX2
    crt_eq_pass_avx2(&eqY, lanes->sig, L, R, NULL, bright, lanes->y);
    crt_eq_pass_avx2(&eqI, lanes->sig, L, R, wi,   bright, lanes->i);
    crt_eq_pass_avx2(&eqQ, lanes->sig, L, R, wq,   bright, lanes->q);
#endif
}
/* ---- The encoder in lanes ----
 * Each line of the modulator runs three one-pole filters along its
 * samples from zero and turns them into a signal level; lines are
 * independent, so they run in lanes as the decoder's do, with the
 * scalar loop's integer operations in its order. */
/* EXP_P, the filters' fixed-point shift, defined with the modulator
 * further down; checked equal there. */
#define CRT_MOD_EXP_P 11
#define CRT_MOD_STEP_V(ADD, SUB, MUL, SRAI, MIN, MAX) \
    hY  = ADD(hY, SRAI(MUL(SUB(fy, hY), vcY), CRT_MOD_EXP_P)); \
    hI  = ADD(hI, SRAI(MUL(SUB(fi, hI), vcI), CRT_MOD_EXP_P)); \
    hQ  = ADD(hQ, SRAI(MUL(SUB(fq, hQ), vcQ), CRT_MOD_EXP_P)); \
    ire = ADD(vblack, SRAI(MUL(ADD(ADD(hY, SRAI(MUL(hI, mi[xoff]), 4)), \
                                   SRAI(MUL(hQ, mq[xoff]), 4)), \
                               vwhite), 10)); \
    ire = MIN(MAX(ire, vlo), vhi)

#ifdef CRT_HAVE_LANES_AVX2
#define CRT_A2_MIN(a, b) _mm256_min_epi32(a, b)
#define CRT_A2_MAX(a, b) _mm256_max_epi32(a, b)

CRT_TARGET_AVX2
static void crt_mod_pass_avx2(const int *ty, const int *ti, const int *tq,
        const int *colc, int destw, int xo, int black, int white,
        int cY, int cI, int cQ, const int *modI, const int *modQ, int *out)
{
    const __m256i vcY = _mm256_set1_epi32(cY);
    const __m256i vcI = _mm256_set1_epi32(cI);
    const __m256i vcQ = _mm256_set1_epi32(cQ);
    const __m256i vblack = _mm256_set1_epi32(black);
    const __m256i vwhite = _mm256_set1_epi32(white);
    const __m256i vlo = _mm256_setzero_si256();
    const __m256i vhi = _mm256_set1_epi32(110);
    __m256i mi[4], mq[4];
    __m256i hY = vlo, hI = vlo, hQ = vlo;
    int x, m;
    for (m = 0; m < 4; m++) {
        mi[m] = _mm256_set1_epi32(modI[m]);
        mq[m] = _mm256_set1_epi32(modQ[m]);
    }
    for (x = 0; x < destw; x++) {
        size_t c   = (size_t)colc[x] * 8;
        __m256i fy = _mm256_loadu_si256((const __m256i*)(ty + c));
        __m256i fi = _mm256_loadu_si256((const __m256i*)(ti + c));
        __m256i fq = _mm256_loadu_si256((const __m256i*)(tq + c));
        int xoff   = (x + xo) & 3;
        __m256i ire;
        CRT_MOD_STEP_V(CRT_A2_ADD, CRT_A2_SUB, CRT_A2_MUL, CRT_A2_SRAI,
                CRT_A2_MIN, CRT_A2_MAX);
        _mm256_storeu_si256((__m256i*)(out + (size_t)x * 8), ire);
    }
    CRT_ZEROUPPER();
}
#endif

#ifdef CRT_HAVE_LANES_AVX512
#define CRT_A5_MIN(a, b) _mm512_min_epi32(a, b)
#define CRT_A5_MAX(a, b) _mm512_max_epi32(a, b)

CRT_TARGET_AVX512
static void crt_mod_pass_avx512(const int *ty, const int *ti, const int *tq,
        const int *colc, int destw, int xo, int black, int white,
        int cY, int cI, int cQ, const int *modI, const int *modQ, int *out)
{
    const __m512i vcY = _mm512_set1_epi32(cY);
    const __m512i vcI = _mm512_set1_epi32(cI);
    const __m512i vcQ = _mm512_set1_epi32(cQ);
    const __m512i vblack = _mm512_set1_epi32(black);
    const __m512i vwhite = _mm512_set1_epi32(white);
    const __m512i vlo = _mm512_setzero_si512();
    const __m512i vhi = _mm512_set1_epi32(110);
    __m512i mi[4], mq[4];
    __m512i hY = vlo, hI = vlo, hQ = vlo;
    int x, m;
    for (m = 0; m < 4; m++) {
        mi[m] = _mm512_set1_epi32(modI[m]);
        mq[m] = _mm512_set1_epi32(modQ[m]);
    }
    for (x = 0; x < destw; x++) {
        size_t c   = (size_t)colc[x] * 16;
        __m512i fy = _mm512_loadu_si512((const void*)(ty + c));
        __m512i fi = _mm512_loadu_si512((const void*)(ti + c));
        __m512i fq = _mm512_loadu_si512((const void*)(tq + c));
        int xoff   = (x + xo) & 3;
        __m512i ire;
        CRT_MOD_STEP_V(CRT_A5_ADD, CRT_A5_SUB, CRT_A5_MUL, CRT_A5_SRAI,
                CRT_A5_MIN, CRT_A5_MAX);
        _mm512_storeu_si512((void*)(out + (size_t)x * 16), ire);
    }
    CRT_ZEROUPPER();
}
#endif

/* Modulates the n lines from y0 into their signal rows: each lane's
 * source row converted once per pixel, the lanes run, and each lane's
 * levels copied to its row. 'colc' is each sample's source column. */
static void crt_modulate_lanes(struct CRT *v, const struct NTSC_SETTINGS *s,
        const struct crt_mod_lanes *ml, int y0, int desth, int destw,
        const int *colc, int xo, int yo, int black, int white,
        int cY, int cI, int cQ, const int *modI, const int *modQ)
{
    const int nl = ml->n;
    const unsigned char *rows[CRT_LANES_MAX];
    int k, x;
    for (k = 0; k < nl; k++) {
        int y            = y0 + k;
        int field_offset = (s->field * s->h + desth) / desth / 2;
        int sy           = (y * s->h) / desth + field_offset;
        if (sy >= s->h) sy = s->h;
        rows[k] = s->data + (size_t)sy * s->w * 3;
    }
    /* Column by column, so each column's lanes are stored together
     * rather than one value per cache line, lane after lane. */
    for (x = 0; x < s->w; x++) {
        int *py = ml->y + (size_t)x * nl;
        int *pi = ml->i + (size_t)x * nl;
        int *pq = ml->q + (size_t)x * nl;
        for (k = 0; k < nl; k++) {
            const unsigned char *pix = rows[k] + (size_t)x * 3;
            int rA = pix[0], gA = pix[1], bA = pix[2];
            py[k] = (19595 * rA + 38470 * gA +  7471 * bA) >> 14;
            pi[k] = (39059 * rA - 18022 * gA - 21103 * bA) >> 14;
            pq[k] = (13894 * rA - 34275 * gA + 20382 * bA) >> 14;
        }
    }
#ifdef CRT_HAVE_LANES_AVX512
    if (nl == 16)
        crt_mod_pass_avx512(ml->y, ml->i, ml->q, colc, destw, xo, black,
                white, cY, cI, cQ, modI, modQ, ml->out);
    else
#endif
    {
#ifdef CRT_HAVE_LANES_AVX2
        crt_mod_pass_avx2(ml->y, ml->i, ml->q, colc, destw, xo, black,
                white, cY, cI, cQ, modI, modQ, ml->out);
#endif
    }
    {
        signed char *dst[CRT_LANES_MAX];
        for (k = 0; k < nl; k++)
            dst[k] = &v->analog[xo + (y0 + k + yo) * CRT_HRES];
        /* Sample by sample, reading each sample's lanes together. */
        for (x = 0; x < destw; x++) {
            const int *src = ml->out + (size_t)x * nl;
            for (k = 0; k < nl; k++)
                dst[k][x] = (signed char)src[k];
        }
    }
}
/* ---- Noise in lanes ----
 * demodulate_begin's noise pass over the whole signal, many samples a
 * step: each lane holds the generator state for its own sample, steps
 * by the lane count with crt_lcg_coef's constants, and does the scalar
 * loop's arithmetic. Several vectors at once hide the multiply's
 * latency. Returns how many samples it wrote (a multiple of the step);
 * the caller finishes the rest from crt_lcg_jump. */
#ifdef CRT_HAVE_LANES_AVX2
CRT_TARGET_AVX2
static size_t crt_noise_avx2(const signed char *an, signed char *out,
        size_t n, unsigned rn, int nn)
{
    unsigned st[32], A, C;
    size_t i;
    int j;
    __m256i v0, v1, v2, v3;
    const __m256i ff  = _mm256_set1_epi32(0xff);
    const __m256i k7f = _mm256_set1_epi32(0x7f);
    const __m256i vnn = _mm256_set1_epi32(nn);
    const __m256i lo  = _mm256_set1_epi32(-127);
    const __m256i hi  = _mm256_set1_epi32(127);
    const __m256i ord = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
    __m256i vA, vC;
    for (j = 0; j < 32; j++) {
        rn    = CRT_LCG_A * rn + CRT_LCG_C;
        st[j] = rn;
    }
    crt_lcg_coef(32, &A, &C);
    vA = _mm256_set1_epi32((int)A);
    vC = _mm256_set1_epi32((int)C);
    v0 = _mm256_loadu_si256((const __m256i*)(st +  0));
    v1 = _mm256_loadu_si256((const __m256i*)(st +  8));
    v2 = _mm256_loadu_si256((const __m256i*)(st + 16));
    v3 = _mm256_loadu_si256((const __m256i*)(st + 24));
#define CRT_NOISE_A2(V, OFF, S) \
    { \
        __m256i t_ = _mm256_sub_epi32(_mm256_and_si256( \
                    _mm256_srli_epi32(V, 16), ff), k7f); \
        t_ = _mm256_srai_epi32(_mm256_mullo_epi32(t_, vnn), 8); \
        S  = _mm256_add_epi32(_mm256_cvtepi8_epi32(_mm_loadl_epi64( \
                    (const __m128i*)(an + i + (OFF)))), t_); \
        S  = _mm256_min_epi32(_mm256_max_epi32(S, lo), hi); \
        V  = _mm256_add_epi32(_mm256_mullo_epi32(V, vA), vC); \
    }
    for (i = 0; i + 32 <= n; i += 32) {
        __m256i s0, s1, s2, s3, b;
        CRT_NOISE_A2(v0,  0, s0)
        CRT_NOISE_A2(v1,  8, s1)
        CRT_NOISE_A2(v2, 16, s2)
        CRT_NOISE_A2(v3, 24, s3)
        /* Narrowing packs per 128-bit lane: 4-sample groups come out
         * as s0 s1 s2 s3 (low halves) then the high halves; the
         * permute puts them back in sample order. In range, so the
         * saturation never acts. */
        b = _mm256_packs_epi16(_mm256_packs_epi32(s0, s1),
                _mm256_packs_epi32(s2, s3));
        _mm256_storeu_si256((__m256i*)(out + i),
                _mm256_permutevar8x32_epi32(b, ord));
    }
#undef CRT_NOISE_A2
    CRT_ZEROUPPER();
    return i;
}
#endif

#ifdef CRT_HAVE_LANES_AVX512
CRT_TARGET_AVX512
static size_t crt_noise_avx512(const signed char *an, signed char *out,
        size_t n, unsigned rn, int nn)
{
    unsigned st[64], A, C;
    size_t i;
    int j;
    __m512i v0, v1, v2, v3;
    const __m512i ff  = _mm512_set1_epi32(0xff);
    const __m512i k7f = _mm512_set1_epi32(0x7f);
    const __m512i vnn = _mm512_set1_epi32(nn);
    const __m512i lo  = _mm512_set1_epi32(-127);
    const __m512i hi  = _mm512_set1_epi32(127);
    __m512i vA, vC;
    for (j = 0; j < 64; j++) {
        rn    = CRT_LCG_A * rn + CRT_LCG_C;
        st[j] = rn;
    }
    crt_lcg_coef(64, &A, &C);
    vA = _mm512_set1_epi32((int)A);
    vC = _mm512_set1_epi32((int)C);
    v0 = _mm512_loadu_si512((const void*)(st +  0));
    v1 = _mm512_loadu_si512((const void*)(st + 16));
    v2 = _mm512_loadu_si512((const void*)(st + 32));
    v3 = _mm512_loadu_si512((const void*)(st + 48));
#define CRT_NOISE_A5(V, OFF) \
    { \
        __m512i t_ = _mm512_sub_epi32(_mm512_and_si512( \
                    _mm512_srli_epi32(V, 16), ff), k7f); \
        __m512i s_; \
        t_ = _mm512_srai_epi32(_mm512_mullo_epi32(t_, vnn), 8); \
        s_ = _mm512_add_epi32(_mm512_cvtepi8_epi32(_mm_loadu_si128( \
                    (const __m128i*)(an + i + (OFF)))), t_); \
        s_ = _mm512_min_epi32(_mm512_max_epi32(s_, lo), hi); \
        _mm_storeu_si128((__m128i*)(out + i + (OFF)), \
                _mm512_cvtepi32_epi8(s_)); \
        V  = _mm512_add_epi32(_mm512_mullo_epi32(V, vA), vC); \
    }
    for (i = 0; i + 64 <= n; i += 64) {
        CRT_NOISE_A5(v0,  0)
        CRT_NOISE_A5(v1, 16)
        CRT_NOISE_A5(v2, 32)
        CRT_NOISE_A5(v3, 48)
    }
#undef CRT_NOISE_A5
    CRT_ZEROUPPER();
    return i;
}
#endif

/* Samples [0, n) of the noise pass in lanes when there are any; returns
 * how many were done. */
static size_t crt_noise_lanes(int lanes, const signed char *an,
        signed char *out, size_t n, unsigned rn, int nn)
{
#ifdef CRT_HAVE_LANES_AVX512
    if (lanes == 16)
        return crt_noise_avx512(an, out, n, rn, nn);
#endif
#ifdef CRT_HAVE_LANES_AVX2
    if (lanes)
        return crt_noise_avx2(an, out, n, rn, nn);
#endif
    (void)lanes; (void)an; (void)out; (void)n; (void)rn; (void)nn;
    return 0;
}
#endif /* CRT_HAVE_LANES */

/* The lane count a filter created with this SIMD mask decodes with. */
static int crt_lanes_for(unsigned simd)
{
#ifdef CRT_HAVE_LANES_AVX512
    if (simd & SOFTFILTER_SIMD_AVX512) {
        return 16;
    }
#endif
#ifdef CRT_HAVE_LANES_AVX2
    if (simd & SOFTFILTER_SIMD_AVX2) {
        return 8;
    }
#endif
    (void)simd;
    return 0;
}

/* One line's output rows from its decoded samples. Sample s's Y, I and
 * Q are py[s * stride], pi[s * stride] and pq[s * stride], so the same
 * code reads the scalar decoder's struct crt_yiq array (stride 3) and
 * the lane decoder's interleaved buffers (stride = lane count). */
static void
crt_render_line(struct CRT *v, const struct crt_line *cl,
        const int *py, const int *pi, const int *pq, size_t stride,
        int bpp, int pitch, int contrast, int blend, int out_format)
{
    unsigned pos;
    unsigned scanR = cl->scanR;
    int scanL      = cl->scanL;
    int dx         = cl->dx;
    int beg        = cl->beg;
    int end        = cl->end;
    int L, R, s;
    unsigned char *cL, *cR;

    cL = v->out + (beg * pitch);
    cR = cL + pitch;

    for (pos = scanL; pos < scanR && cL < cR; pos += dx) {
        int y, i, q;
        int r, g, b;
        int aa, bb;

        R = pos & 0xfff;
        L = 0xfff - R;
        s = pos >> 12;

        {
            size_t a_ = (size_t)s * stride, b_ = a_ + stride;
            /* interpolate between samples if needed */
            y = ((py[a_] * L) >>  2) + ((py[b_] * R) >>  2);
            i = ((pi[a_] * L) >> 14) + ((pi[b_] * R) >> 14);
            q = ((pq[a_] * L) >> 14) + ((pq[b_] * R) >> 14);
        }

        /* YIQ to RGB */
        r = (((y + 3879 * i + 2556 * q) >> 12) * contrast) >> 8;
        g = (((y - 1126 * i - 2605 * q) >> 12) * contrast) >> 8;
        b = (((y - 4530 * i + 7021 * q) >> 12) * contrast) >> 8;

        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;

        if (blend) {
            aa = (r << 16 | g << 8 | b);

            switch (out_format) {
                case CRT_PIX_FORMAT_RGB:
                case CRT_PIX_FORMAT_RGBA:
                    bb = cL[0] << 16 | cL[1] << 8 | cL[2];
                    break;
                case CRT_PIX_FORMAT_BGR:
                case CRT_PIX_FORMAT_BGRA:
                    bb = cL[2] << 16 | cL[1] << 8 | cL[0];
                    break;
                case CRT_PIX_FORMAT_ARGB:
                    bb = cL[1] << 16 | cL[2] << 8 | cL[3];
                    break;
                case CRT_PIX_FORMAT_ABGR:
                    bb = cL[3] << 16 | cL[2] << 8 | cL[1];
                    break;
                default:
                    bb = 0;
                    break;
            }

            /* blend with previous color there */
            bb = (((aa & 0xfefeff) >> 1) + ((bb & 0xfefeff) >> 1));
        } else {
            bb = (r << 16 | g << 8 | b);
        }

        switch (out_format) {
            case CRT_PIX_FORMAT_RGB:
                cL[0] = bb >> 16 & 0xff;
                cL[1] = bb >>  8 & 0xff;
                cL[2] = bb >>  0 & 0xff;
                break;

            case CRT_PIX_FORMAT_RGBA:
                cL[0] = bb >> 16 & 0xff;
                cL[1] = bb >>  8 & 0xff;
                cL[2] = bb >>  0 & 0xff;
                cL[3] = 0xff;
                break;

            case CRT_PIX_FORMAT_BGR:
                cL[0] = bb >>  0 & 0xff;
                cL[1] = bb >>  8 & 0xff;
                cL[2] = bb >> 16 & 0xff;
                break;

            case CRT_PIX_FORMAT_BGRA:
                cL[0] = bb >>  0 & 0xff;
                cL[1] = bb >>  8 & 0xff;
                cL[2] = bb >> 16 & 0xff;
                cL[3] = 0xff;
                break;

            case CRT_PIX_FORMAT_ARGB:
                cL[0] = 0xff;
                cL[1] = bb >> 16 & 0xff;
                cL[2] = bb >>  8 & 0xff;
                cL[3] = bb >>  0 & 0xff;
                break;

            case CRT_PIX_FORMAT_ABGR:
                cL[0] = 0xff;
                cL[1] = bb >>  0 & 0xff;
                cL[2] = bb >>  8 & 0xff;
                cL[3] = bb >> 16 & 0xff;
                break;

            default:
                break;
        }

        cL += bpp;
    }

    /* duplicate extra lines */
    for (s = beg + 1; s < (end - v->scanlines); s++) {
        memcpy(v->out + s * pitch, v->out + (s - 1) * pitch, pitch);
    }
}

static void
crt_demodulate_lines(struct CRT *v, int first, int last, struct crt_yiq *out,
        const struct crt_lanes *lanes)
{
    int line, bpp, pitch;
    int bright = v->bright;
    /* Read once: every pixel is stored through an unsigned char
     * pointer, which may alias these, so they were fetched again for
     * every pixel. */
    const int contrast   = v->contrast;
    const int blend      = v->blend;
    const int out_format = v->out_format;
    signed char *sig;

    if (!v->frame_ok) {
        return;
    }
    bpp   = crt_bpp4fmt(v->out_format);
    pitch = v->outw * bpp;
    line  = CRT_TOP + first;

#ifdef CRT_HAVE_LANES
    /* Whole batches of lines, one per lane, in line order; what is left
     * (fewer lines than lanes) takes the scalar loop below. Lines that
     * are skipped do nothing either way, so the rows are drawn in the
     * same order as one line at a time. */
    if (lanes && lanes->n) {
        const int nl = lanes->n;
        for (;;) {
            int idx[CRT_LANES_MAX] = { 0 };
            int n = 0, l2, k;
            for (l2 = line; l2 < CRT_TOP + last && n < nl; l2++) {
                if (!v->lines[l2 - CRT_TOP].skip) {
                    idx[n++] = l2 - CRT_TOP;
                }
            }
            if (n < nl) {
                break;
            }
            /* Lockstep needs one sample range for all of them; without
             * bloom every line has [0, AV_LEN). */
            for (k = 1; k < nl; k++) {
                if (     v->lines[idx[k]].L != v->lines[idx[0]].L
                      || v->lines[idx[k]].R != v->lines[idx[0]].R) {
                    break;
                }
            }
            if (k < nl) {
                break;
            }
            crt_decode_lanes(v, idx, lanes, bright);
            for (k = 0; k < nl; k++) {
                crt_render_line(v, &v->lines[idx[k]], lanes->y + k,
                        lanes->i + k, lanes->q + k, (size_t)nl, bpp,
                        pitch, contrast, blend, out_format);
            }
            line = l2;
        }
    }
#else
    (void)lanes;
#endif

    for (; line < CRT_TOP + last; line++) {
        const struct crt_line *cl = &v->lines[line - CRT_TOP];
        int L, R, i;
#if (CRT_CC_SAMPLES == 4)
        const int *wave = cl->wave;
#else
        const int *waveI = cl->waveI;
        const int *waveQ = cl->waveQ;
#endif
        struct EQF eqY_l, eqI_l, eqQ_l;

        if (cl->skip) { continue; }
        L     = cl->L;
        R     = cl->R;
        sig   = v->inp + cl->pos;

        /* Each line starts its equalisers afresh from the templates. */
        eqY_l = eqY;
        eqI_l = eqI;
        eqQ_l = eqQ;
        reset_eq(&eqY_l);
        reset_eq(&eqI_l);
        reset_eq(&eqQ_l);

#if (CRT_CC_SAMPLES == 4) && !USE_CONVOLUTION
        {
            /* eqf() for Y, I and Q side by side with their state in
             * locals: through the struct pointers, every store to out[]
             * (ints too) could alias the state, so it was reloaded and
             * stored back on every sample. Same updates in the same
             * order, so the same result. */
            EQF_LOCALS(y_, eqY_l);
            EQF_LOCALS(i_, eqI_l);
            EQF_LOCALS(q_, eqQ_l);
            const int w0 = wave[0], w1 = wave[1], w2 = wave[2], w3 = wave[3];
            for (i = L; i < R; i++) {
                int sv = sig[i], ry, ri, rq, wi, wq;
                switch (i & 3) {
                    case 0:  wi = w0; wq = w3; break;
                    case 1:  wi = w1; wq = w0; break;
                    case 2:  wi = w2; wq = w1; break;
                    default: wi = w3; wq = w2; break;
                }
                EQF_STEP(y_, sv + bright, ry);
                EQF_STEP(i_, sv * wi >> 9, ri);
                EQF_STEP(q_, sv * wq >> 9, rq);
                out[i].y = ry * 16; /* may be negative */
                out[i].i = ri >> 3;
                out[i].q = rq >> 3;
            }
        }
#elif (CRT_CC_SAMPLES == 4)
        for (i = L; i < R; i++) {
            out[i].y = eqf(&eqY_l, sig[i] + bright) * 16; /* may be negative */
            out[i].i = eqf(&eqI_l, sig[i] * wave[(i + 0) & 3] >> 9) >> 3;
            out[i].q = eqf(&eqQ_l, sig[i] * wave[(i + 3) & 3] >> 9) >> 3;
        }
#else
        for (i = L; i < R; i++) {
            out[i].y = eqf(&eqY_l, sig[i] + bright) * 16; /* may be negative */
            out[i].i = eqf(&eqI_l, sig[i] * waveI[i % CRT_CC_SAMPLES] >> 9) >> 3;
            out[i].q = eqf(&eqQ_l, sig[i] * waveQ[i % CRT_CC_SAMPLES] >> 9) >> 3;
        }
#endif

        crt_render_line(v, cl, &out->y, &out->i, &out->q,
                sizeof(*out) / sizeof(int), bpp, pitch, contrast, blend,
                out_format);
    }
}

/*****************************************************************************/
/*****************************************************************************/
/****           crt_ntsc.c - the modulator implementation                 ****/
/*****************************************************************************/
/*****************************************************************************/

#if (CRT_CHROMA_PATTERN == 1)
/* 227.5 subcarrier cycles per line means every other line has reversed phase */
#define CC_PHASE(ln)     (((ln) & 1) ? -1 : 1)
#else
#define CC_PHASE(ln)     (1)
#endif

#define EXP_P         11
#if defined(CRT_MOD_EXP_P) && (CRT_MOD_EXP_P != EXP_P)
#error "the encoder lanes' CRT_MOD_EXP_P must equal EXP_P"
#endif
#define EXP_ONE       (1 << EXP_P)
#define EXP_MASK      (EXP_ONE - 1)
#define EXP_PI        6434
#define EXP_MUL(x, y) (((x) * (y)) >> EXP_P)
#define EXP_DIV(x, y) (((x) << EXP_P) / (y))

static int e11[] = {
    EXP_ONE,
    5567,  /* e   */
    15133, /* e^2 */
    41135, /* e^3 */
    111817 /* e^4 */
};

/* fixed point e^x */
static int
expx(int n)
{
    int neg, idx, res;
    int nxt, acc, del;
    int i;

    if (n == 0) {
        return EXP_ONE;
    }
    neg = n < 0;
    if (neg) {
        n = -n;
    }
    idx = n >> EXP_P;
    res = EXP_ONE;
    for (i = 0; i < idx / 4; i++) {
        res = EXP_MUL(res, e11[4]);
    }
    idx &= 3;
    if (idx > 0) {
        res = EXP_MUL(res, e11[idx]);
    }

    n &= EXP_MASK;
    nxt = EXP_ONE;
    acc = 0;
    del = 1;
    for (i = 1; i < 17; i++) {
        acc += nxt / del;
        nxt = EXP_MUL(nxt, n);
        del *= i;
        if (del > nxt || nxt <= 0 || del <= 0) {
            break;
        }
    }
    res = EXP_MUL(res, acc);

    if (neg) {
        res = EXP_DIV(EXP_ONE, res);
    }
    return res;
}

/*****************************************************************************/
/********************************* FILTERS ***********************************/
/*****************************************************************************/

/* infinite impulse response low pass filter for bandlimiting YIQ */
static struct IIRLP {
    int c;
    int h; /* history */
} iirY, iirI, iirQ;

/* freq  - total bandwidth
 * limit - max frequency
 */
static void
init_iir(struct IIRLP *f, int freq, int limit)
{
    int rate; /* cycles/pixel rate */

    memset(f, 0, sizeof(struct IIRLP));
    rate = (freq << 9) / limit;
    f->c = EXP_ONE - expx(-((EXP_PI << 9) / rate));
}

static void
reset_iir(struct IIRLP *f)
{
    f->h = 0;
}

/* hi-pass for debugging */
#define HIPASS 0

static int
iirf(struct IIRLP *f, int s)
{
    f->h += EXP_MUL(s - f->h, f->c);
#if HIPASS
    return s - f->h;
#else
    return f->h;
#endif
}

/* The part of encoding done once a frame: the sync and burst of every
 * line, the decoder's starting carrier, and the frame's parameters into
 * 'mf' (the active area always). Returns 1 when the video lines are
 * left to crt_modulate_lines, 0 when they are done (or there is
 * nothing to do). */
static int
crt_modulate_begin(struct CRT *v, struct NTSC_SETTINGS *s,
        struct crt_mod_frame *mf)
{
    int x, y, xo, yo;
    int destw = AV_LEN;
    int desth = ((CRT_LINES * 64500) >> 16);
    int iccf[CRT_CC_SAMPLES];
    int ccmodI[CRT_CC_SAMPLES]; /* color phase for mod */
    int ccmodQ[CRT_CC_SAMPLES]; /* color phase for mod */
    int ccburst[CRT_CC_SAMPLES]; /* color phase for burst */
    int sn, cs, n, ph;
    int inv_phase = 0;
    int bpp;

    if (!s->iirs_initialized) {
        init_iir(&iirY, L_FREQ, Y_FREQ);
        init_iir(&iirI, L_FREQ, I_FREQ);
        init_iir(&iirQ, L_FREQ, Q_FREQ);
        s->iirs_initialized = 1;
    }
#if CRT_DO_BLOOM
    if (s->raw) {
        destw = s->w;
        desth = s->h;
        if (destw > ((AV_LEN * 55500) >> 16)) {
            destw = ((AV_LEN * 55500) >> 16);
        }
        if (desth > ((CRT_LINES * 63500) >> 16)) {
            desth = ((CRT_LINES * 63500) >> 16);
        }
    } else {
        destw = (AV_LEN * 55500) >> 16;
        desth = (CRT_LINES * 63500) >> 16;
    }
#else
    if (s->raw) {
        destw = s->w;
        desth = s->h;
        if (destw > AV_LEN) {
            destw = AV_LEN;
        }
        if (desth > ((CRT_LINES * 64500) >> 16)) {
            desth = ((CRT_LINES * 64500) >> 16);
        }
    }
#endif
    if (s->as_color) {
        for (x = 0; x < CRT_CC_SAMPLES; x++) {
            n = s->hue + x * (360 / CRT_CC_SAMPLES);
            crt_sincos14(&sn, &cs, (n + 33) * 8192 / 180);
            ccburst[x] = sn >> 10;
            crt_sincos14(&sn, &cs, n * 8192 / 180);
            ccmodI[x] = sn >> 10;
            crt_sincos14(&sn, &cs, (n - 90) * 8192 / 180);
            ccmodQ[x] = sn >> 10;
        }
    } else {
        memset(ccburst, 0, sizeof(ccburst));
        memset(ccmodI, 0, sizeof(ccmodI));
        memset(ccmodQ, 0, sizeof(ccmodQ));
    }

    bpp = crt_bpp4fmt(s->format);
    if (bpp == 0) {
        mf->desth = 0;
        return 0; /* just to be safe */
    }
    xo = AV_BEG  + s->xoffset + (AV_LEN    - destw) / 2;
    yo = CRT_TOP + s->yoffset + (CRT_LINES - desth) / 2;

    s->field &= 1;
    s->frame &= 1;
    inv_phase = (s->field == s->frame);
    ph = CC_PHASE(inv_phase);

    /* align signal */
    xo = (xo & ~3);

    for (n = 0; n < CRT_VRES; n++) {
        int t; /* time */
        signed char *line = &v->analog[n * CRT_HRES];

        t = LINE_BEG;

        if (n <= 3 || (n >= 7 && n <= 9)) {
            /* equalizing pulses - small blips of sync, mostly blank */
            while (t < (4   * CRT_HRES / 100)) line[t++] = SYNC_LEVEL;
            while (t < (50  * CRT_HRES / 100)) line[t++] = BLANK_LEVEL;
            while (t < (54  * CRT_HRES / 100)) line[t++] = SYNC_LEVEL;
            while (t < (100 * CRT_HRES / 100)) line[t++] = BLANK_LEVEL;
        } else if (n >= 4 && n <= 6) {
            int even[4] = { 46, 50, 96, 100 };
            int odd[4] =  { 4, 50, 96, 100 };
            int *offs = even;
            if (s->field == 1) {
                offs = odd;
            }
            /* vertical sync pulse - small blips of blank, mostly sync */
            while (t < (offs[0] * CRT_HRES / 100)) line[t++] = SYNC_LEVEL;
            while (t < (offs[1] * CRT_HRES / 100)) line[t++] = BLANK_LEVEL;
            while (t < (offs[2] * CRT_HRES / 100)) line[t++] = SYNC_LEVEL;
            while (t < (offs[3] * CRT_HRES / 100)) line[t++] = BLANK_LEVEL;
        } else {
            int cb;

            /* video line */
            while (t < SYNC_BEG) line[t++] = BLANK_LEVEL; /* FP */
            while (t < BW_BEG)   line[t++] = SYNC_LEVEL;  /* SYNC */
            while (t < AV_BEG)   line[t++] = BLANK_LEVEL; /* BW + CB + BP */
            if (n < CRT_TOP) {
                while (t < CRT_HRES) line[t++] = BLANK_LEVEL;
            }

            /* CB_CYCLES of color burst at 3.579545 Mhz */
            for (t = CB_BEG; t < CB_BEG + (CB_CYCLES * CRT_CB_FREQ); t++) {
#if (CRT_CHROMA_PATTERN == 1)
                int off180 = CRT_CC_SAMPLES / 2;
                cb = ccburst[(t + inv_phase * off180) % CRT_CC_SAMPLES];
#else
                cb = ccburst[t % CRT_CC_SAMPLES];
#endif
                line[t] = (BLANK_LEVEL + (cb * BURST_LEVEL)) >> 5;
                iccf[t % CRT_CC_SAMPLES] = line[t];
            }
        }
    }

    /* The colour carrier the decoder starts from, from the burst */
    for (n = 0; n < CRT_CC_VPER; n++) {
        for (x = 0; x < CRT_CC_SAMPLES; x++) {
            v->ccf[n][x] = iccf[x] * 128; /* may be negative */
        }
    }

    mf->destw = destw;
    mf->desth = desth;
    mf->xo    = xo;
    mf->yo    = yo;

    /* RGB input with 4 samples a cycle, the format the softfilter
     * wrapper feeds this, leaves the video lines to
     * crt_modulate_lines; anything else is encoded here. */
    if (s->format == CRT_PIX_FORMAT_RGB && CRT_CC_SAMPLES == 4
            && destw <= AV_LEN) {
        mf->white = WHITE_LEVEL * v->white_point / 100;
        mf->black = BLACK_LEVEL + v->black_point;
        mf->cY    = iirY.c;
        mf->cI    = iirI.c;
        mf->cQ    = iirQ.c;
        mf->field_offset = (s->field * s->h + desth) / desth / 2;
        for (x = 0; x < 4; x++) {
            mf->modI[x] = ph * ccmodI[x & (CRT_CC_SAMPLES - 1)];
            mf->modQ[x] = ph * ccmodQ[x & (CRT_CC_SAMPLES - 1)];
        }
        for (x = 0; x < destw; x++) {
            mf->col[x]  = (x * s->w) / destw;
            mf->col3[x] = mf->col[x] * 3;
        }
        return 1;
    }

    for (y = 0; y < desth; y++) {
        int field_offset;
        int sy;

        field_offset = (s->field * s->h + desth) / desth / 2;
        sy = (y * s->h) / desth;

        sy += field_offset;

        if (sy >= s->h) sy = s->h;

        sy *= s->w;

        reset_iir(&iirY);
        reset_iir(&iirI);
        reset_iir(&iirQ);

        for (x = 0; x < destw; x++) {
            int fy, fi, fq;
            int rA, gA, bA;
            const unsigned char *pix;
            int ire; /* composite signal */
            int xoff;

            pix = s->data + ((((x * s->w) / destw) + sy) * bpp);
            switch (s->format) {
                case CRT_PIX_FORMAT_RGB:
                case CRT_PIX_FORMAT_RGBA:
                    rA = pix[0];
                    gA = pix[1];
                    bA = pix[2];
                    break;
                case CRT_PIX_FORMAT_BGR:
                case CRT_PIX_FORMAT_BGRA:
                    rA = pix[2];
                    gA = pix[1];
                    bA = pix[0];
                    break;
                case CRT_PIX_FORMAT_ARGB:
                    rA = pix[1];
                    gA = pix[2];
                    bA = pix[3];
                    break;
                case CRT_PIX_FORMAT_ABGR:
                    rA = pix[3];
                    gA = pix[2];
                    bA = pix[1];
                    break;
                default:
                    rA = gA = bA = 0;
                    break;
            }

            /* RGB to YIQ */
            fy = (19595 * rA + 38470 * gA +  7471 * bA) >> 14;
            fi = (39059 * rA - 18022 * gA - 21103 * bA) >> 14;
            fq = (13894 * rA - 34275 * gA + 20382 * bA) >> 14;
            ire = BLACK_LEVEL + v->black_point;

            xoff = (x + xo) % CRT_CC_SAMPLES;
            /* bandlimit Y,I,Q */
            fy = iirf(&iirY, fy);
            fi = iirf(&iirI, fi) * ph * ccmodI[xoff] >> 4;
            fq = iirf(&iirQ, fq) * ph * ccmodQ[xoff] >> 4;
            ire += (fy + fi + fq) * (WHITE_LEVEL * v->white_point / 100) >> 10;
            if (ire < 0)   ire = 0;
            if (ire > 110) ire = 110;

            v->analog[(x + xo) + (y + yo) * CRT_HRES] = ire;
        }
    }
    return 0;
}

/* The video lines [first, last) of the frame crt_modulate_begin set up
 * (0 to mf->desth), each into its own row of the signal: whole batches
 * of lines in lanes, the rest one by one. Lines are independent, so
 * bands of them can be encoded at once, each with its own 'ml'. */
static void
crt_modulate_lines(struct CRT *v, const struct NTSC_SETTINGS *s,
        const struct crt_mod_frame *mf, const struct crt_mod_lanes *ml,
        int first, int last)
{
    /* Loop invariants, held where a store to the signal (signed
     * char, which may alias anything) cannot make the compiler fetch
     * them again. The filters' history is reset at every line, so it
     * lives in locals for the line. */
    const int destw = mf->destw, desth = mf->desth;
    const int xo    = mf->xo,    yo    = mf->yo;
    const int white = mf->white, black = mf->black;
    const int cY    = mf->cY, cI = mf->cI, cQ = mf->cQ;
    const int *modI = mf->modI, *modQ = mf->modQ;
    const int *col  = mf->col3;
    int x, y = first;
#ifdef CRT_HAVE_LANES
    if (ml && ml->n && (unsigned)s->w <= ml->w_cap) {
        for (; y + ml->n <= last; y += ml->n)
            crt_modulate_lanes(v, s, ml, y, desth, destw, mf->col,
                    xo, yo, black, white, cY, cI, cQ, modI, modQ);
    }
#else
    (void)ml;
#endif
    for (; y < last; y++) {
        int sy = (y * s->h) / desth + mf->field_offset;
        int hY = 0, hI = 0, hQ = 0;
        const unsigned char *row;
        signed char *dst;
        if (sy >= s->h) sy = s->h;
        row = s->data + (size_t)sy * s->w * 3;
        dst = &v->analog[xo + (y + yo) * CRT_HRES];
        for (x = 0; x < destw; x++) {
            const unsigned char *pix = row + col[x];
            int rA = pix[0], gA = pix[1], bA = pix[2];
            int fy = (19595 * rA + 38470 * gA +  7471 * bA) >> 14;
            int fi = (39059 * rA - 18022 * gA - 21103 * bA) >> 14;
            int fq = (13894 * rA - 34275 * gA + 20382 * bA) >> 14;
            int xoff = (x + xo) & 3;
            int ire;
            hY += EXP_MUL(fy - hY, cY);
            hI += EXP_MUL(fi - hI, cI);
            hQ += EXP_MUL(fq - hQ, cQ);
            ire = black + ((hY + (hI * modI[xoff] >> 4)
                               + (hQ * modQ[xoff] >> 4)) * white >> 10);
            if (ire < 0)   ire = 0;
            if (ire > 110) ire = 110;
            dst[x] = (signed char)ire;
        }
    }
}

/*****************************************************************************/
/*****************************************************************************/
/****            ntsc_crt.c - RetroArch softfilter wrapper                ****/
/*****************************************************************************/
/*****************************************************************************/

/* -----------------------------------------------------------------------
 * Default tuning — these become the starting values shown in the .filt
 * ----------------------------------------------------------------------- */
#define DEFAULT_NOISE       2     /* 0 = clean signal, ~10 = noisy          */
#define DEFAULT_HUE         0     /* 0-359                                   */
#define DEFAULT_BRIGHTNESS  0     /* monitor brightness offset               */
#define DEFAULT_CONTRAST    180   /* monitor contrast                        */
#define DEFAULT_SATURATION  10    /* color saturation                        */
#define DEFAULT_SCANLINES   0     /* 1 = dark gap between scanlines          */
#define DEFAULT_BLEND       1     /* 1 = blend fields (less flicker)         */
#define DEFAULT_AS_COLOR    1     /* 0 = monochrome, 1 = color               */
#define DEFAULT_BLACK_PT    0     /* black point adjustment                  */
#define DEFAULT_WHITE_PT    100   /* white point adjustment                  */

/* -----------------------------------------------------------------------
 * Per-thread data (same pattern as scanline2x)
 * ----------------------------------------------------------------------- */
struct softfilter_thread_data
{
    void        *out_data;
    const void  *in_data;
    size_t       out_pitch;
    size_t       in_pitch;
    unsigned     colfmt;
    unsigned     width;
    unsigned     height;
    int          first;
    int          last;
    /* without lock-free atomics, this worker's band of active lines
     * [line_first, line_last), which it decodes */
    int          line_first;
    int          line_last;
    struct crt_yiq *scratch;
    /* this worker's lane buffers; n is 0 when it decodes line by line */
    struct crt_lanes lanes;
    /* and for encoding; n is 0 when it encodes line by line */
    struct crt_mod_lanes mod_lanes;
};

/* -----------------------------------------------------------------------
 * Main filter state
 * ----------------------------------------------------------------------- */
struct filter_data
{
    unsigned threads;
    struct softfilter_thread_data *workers;
    unsigned in_fmt;

    /* CRT library state */
    struct CRT          crt;
    struct NTSC_SETTINGS ntsc;

    /* packed-RGB input buffer for crt_modulate (cached across frames) */
    unsigned char *in_buf;
    unsigned       in_buf_w;
    unsigned       in_buf_h;

    /* AV_LEN + 1 entries of line scratch per worker */
    struct crt_yiq *scratch;
    /* every worker's lane buffers, one block made at create */
    void *lane_mem;
    /* lanes per step for the passes that need no buffers (noise):
     * 0, 8 or 16, from the SIMD mask given to create */
    int lanes_tier;
    /* every worker's encoder lane buffers, one block made at create */
    void *mod_lane_mem;

    /* This frame, from the packets pass. Its work is a list of tasks,
     * which the workers take in order as each comes free (see
     * ntsc_crt_task): mod_tasks bands of lines to encode and add noise
     * to, then the line-ordered start of decoding, then dec_tasks bands
     * of lines to decode. tasks is 0 when there is nothing to do. */
    struct crt_mod_frame mod;
    int mod_lines;  /* 1 when the bands encode their video lines */
    int mod_tasks;
    int dec_tasks;
    int tasks;
    retro_atomic_int_t next_task;
    retro_atomic_int_t mod_done;   /* encoding tasks finished */
    retro_atomic_int_t begin_done; /* 1 once the decoding start is in */

    /* output buffer (XRGB8888) */
    unsigned char *out_buf;
    unsigned       out_buf_w;
    unsigned       out_buf_h;

    /* user-settable params */
    int noise;
    int hue;
    int scanlines;
    int blend;
    int as_color;

    /* frame/field counter */
    int frame;
    int field;
};

/* -----------------------------------------------------------------------
 * Helpers: XRGB8888 ↔ RGB packing
 * ----------------------------------------------------------------------- */

/* Convert XRGB8888 source row to a packed RGB byte array for crt_modulate */
static void xrgb8888_to_rgb(const uint32_t *src, unsigned char *dst,
                              unsigned w)
{
    unsigned x;
    for (x = 0; x < w; x++) {
        uint32_t c = src[x];
        dst[x * 3 + 0] = (c >> 16) & 0xff; /* R */
        dst[x * 3 + 1] = (c >>  8) & 0xff; /* G */
        dst[x * 3 + 2] = (c >>  0) & 0xff; /* B */
    }
}

static void rgb565_to_rgb(const uint16_t *src, unsigned char *dst,
                           unsigned w)
{
    unsigned x;
    for (x = 0; x < w; x++) {
        uint16_t c = src[x];
        unsigned r = (c >> 11) & 0x1f;
        unsigned g = (c >>  5) & 0x3f;
        unsigned b = (c >>  0) & 0x1f;
        /* Widen to 8 bits with the top bits copied down, so full
         * scale is 255 and greys stay neutral. */
        dst[x * 3 + 0] = (unsigned char)((r << 3) | (r >> 2));
        dst[x * 3 + 1] = (unsigned char)((g << 2) | (g >> 4));
        dst[x * 3 + 2] = (unsigned char)((b << 3) | (b >> 2));
    }
}

/* Convert CRT XRGB output back to RetroArch XRGB8888 scanline */
static void rgb_to_xrgb8888(const unsigned char *src, uint32_t *dst,
                              unsigned w)
{
    unsigned x;
    for (x = 0; x < w; x++) {
        dst[x] = 0xff000000u
               | ((uint32_t)src[x * 3 + 0] << 16)
               | ((uint32_t)src[x * 3 + 1] <<  8)
               | ((uint32_t)src[x * 3 + 2] <<  0);
    }
}

/* -----------------------------------------------------------------------
 * softfilter API
 * ----------------------------------------------------------------------- */

static unsigned ntsc_crt_input_fmts(void)
{
    return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned ntsc_crt_output_fmts(unsigned input_fmts)
{
    /* We always output XRGB8888 (CRT lib works in RGB) */
    (void)input_fmts;
    return SOFTFILTER_FMT_XRGB8888;
}

static unsigned ntsc_crt_threads(void *data)
{
    struct filter_data *filt = (struct filter_data*)data;
    return filt->threads;
}

/* Output is the same size as input — the CRT effect is spatial, not scaling */
static void ntsc_crt_output_size(void *data,
        unsigned *out_width, unsigned *out_height,
        unsigned width, unsigned height)
{
    (void)data;
    *out_width  = width;
    *out_height = height;
}

static void ntsc_crt_destroy(void *data);

static void *ntsc_crt_create(const struct softfilter_config *config,
        unsigned in_fmt, unsigned out_fmt,
        unsigned max_width, unsigned max_height,
        unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
    struct filter_data *filt;
    unsigned char *out_buf;
    (void)out_fmt;
    (void)userdata;

    filt = (struct filter_data*)calloc(1, sizeof(*filt));
    if (!filt) return NULL;

    if (!threads) threads = 1;
    filt->workers = (struct softfilter_thread_data*)
                    calloc(threads, sizeof(struct softfilter_thread_data));
    filt->scratch = (struct crt_yiq*)
                    malloc((size_t)threads * (AV_LEN + 1) * sizeof(struct crt_yiq));
    if (!filt->workers || !filt->scratch) {
        free(filt->scratch); free(filt->workers); free(filt); return NULL;
    }

    filt->threads = threads;
    filt->in_fmt  = in_fmt;

    /* Lane buffers for every worker, when the CPU can decode lines in
     * lanes: the transposed signal and the decoded Y, I and Q, each
     * started on a 64-byte boundary. Without them the workers decode
     * line by line, as before. */
    filt->lanes_tier = crt_lanes_for(simd);
    {
        int    nl   = crt_lanes_for(simd);
        size_t rows = (size_t)AV_LEN + 1;
        size_t sigb = (rows * nl + 63) & ~(size_t)63;
        size_t intb = (rows * nl * sizeof(int) + 63) & ~(size_t)63;
        size_t per  = sigb + 3 * intb;
        /* Each worker's encoder's: three [max_width][nl] source planes
         * and the [AV_LEN][nl] levels. A frame wider than create was told of,
         * which the softfilter contract rules out, is encoded line by
         * line instead. */
        size_t plane = ((size_t)max_width * nl * sizeof(int) + 63)
            & ~(size_t)63;
        size_t lvls  = ((size_t)AV_LEN * nl * sizeof(int) + 63)
            & ~(size_t)63;
        size_t mper  = 3 * plane + lvls;
        if (nl && max_width
                && (filt->mod_lane_mem = malloc(mper * threads + 63))) {
            unsigned char *base = (unsigned char*)
                (((uintptr_t)filt->mod_lane_mem + 63) & ~(uintptr_t)63);
            unsigned w;
            for (w = 0; w < threads; w++) {
                struct crt_mod_lanes *ml = &filt->workers[w].mod_lanes;
                unsigned char *at        = base + mper * w;
                ml->n     = nl;
                ml->w_cap = max_width;
                ml->y     = (int*)(void*)at;
                ml->i     = (int*)(void*)(at + plane);
                ml->q     = (int*)(void*)(at + 2 * plane);
                ml->out   = (int*)(void*)(at + 3 * plane);
            }
        }
        if (nl && (filt->lane_mem = malloc(per * threads + 63))) {
            unsigned char *base = (unsigned char*)
                (((uintptr_t)filt->lane_mem + 63) & ~(uintptr_t)63);
            unsigned w;
            for (w = 0; w < threads; w++) {
                struct crt_lanes *ln = &filt->workers[w].lanes;
                unsigned char *at    = base + per * w;
                ln->n   = nl;
                ln->sig = (signed char*)at;
                ln->y   = (int*)(void*)(at + sigb);
                ln->i   = (int*)(void*)(at + sigb + intb);
                ln->q   = (int*)(void*)(at + sigb + 2 * intb);
            }
        }
    }

    /* Allocate CRT output buffer: RGB (3 bpp) */
    /* Cleared: crt_demodulate does not write every pixel of it, and
     * whatever it leaves is copied to the screen. */
    out_buf = (unsigned char*)calloc(max_width * max_height, 3);
    if (!out_buf) { ntsc_crt_destroy(filt); return NULL; }

    filt->out_buf   = out_buf;
    filt->out_buf_w = max_width;
    filt->out_buf_h = max_height;

    /* Init CRT library */
    crt_init(&filt->crt, max_width, max_height,
             CRT_PIX_FORMAT_RGB, out_buf);

    /* Read tunable params from .filt config (falls back to defaults) */
    if (config) {
        config->get_int(userdata, "noise",      &filt->noise,     DEFAULT_NOISE);
        config->get_int(userdata, "hue",        &filt->hue,       DEFAULT_HUE);
        config->get_int(userdata, "as_color",   &filt->as_color,  DEFAULT_AS_COLOR);
        config->get_int(userdata, "scanlines",  &filt->scanlines, DEFAULT_SCANLINES);
        config->get_int(userdata, "blend",      &filt->blend,     DEFAULT_BLEND);
        config->get_int(userdata, "brightness", &filt->crt.brightness, DEFAULT_BRIGHTNESS);
        config->get_int(userdata, "contrast",   &filt->crt.contrast,   DEFAULT_CONTRAST);
        config->get_int(userdata, "saturation", &filt->crt.saturation, DEFAULT_SATURATION);
        config->get_int(userdata, "black_point",&filt->crt.black_point, DEFAULT_BLACK_PT);
        config->get_int(userdata, "white_point",&filt->crt.white_point, DEFAULT_WHITE_PT);
    } else {
        filt->noise            = DEFAULT_NOISE;
        filt->hue              = DEFAULT_HUE;
        filt->as_color         = DEFAULT_AS_COLOR;
        filt->scanlines        = DEFAULT_SCANLINES;
        filt->blend            = DEFAULT_BLEND;
        filt->crt.brightness   = DEFAULT_BRIGHTNESS;
        filt->crt.contrast     = DEFAULT_CONTRAST;
        filt->crt.saturation   = DEFAULT_SATURATION;
        filt->crt.black_point  = DEFAULT_BLACK_PT;
        filt->crt.white_point  = DEFAULT_WHITE_PT;
    }

    filt->crt.scanlines = filt->scanlines;
    filt->crt.blend     = filt->blend;
    filt->crt.hue       = filt->hue;

    /* NTSC_SETTINGS — zeroed by calloc, just set fields */
    memset(&filt->ntsc, 0, sizeof(filt->ntsc));
    filt->ntsc.as_color = filt->as_color;
    filt->ntsc.hue      = filt->hue;
    filt->ntsc.format   = CRT_PIX_FORMAT_RGB;

    filt->frame = 0;
    filt->field = 0;

    return filt;
}

static void ntsc_crt_destroy(void *data)
{
    struct filter_data *filt = (struct filter_data*)data;
    if (!filt) return;
    if (filt->in_buf)  free(filt->in_buf);
    if (filt->out_buf) free(filt->out_buf);
    free(filt->scratch);
    free(filt->lane_mem);
    free(filt->mod_lane_mem);
    free(filt->workers);
    free(filt);
}

/* -----------------------------------------------------------------------
 * The actual work callback — called from RetroArch's worker thread
 * ----------------------------------------------------------------------- */
/* Once a frame, before the workers: convert the input and do the part
 * of encoding that is done once a frame. Returns 0 when the frame
 * cannot be processed, leaving the workers nothing to do. */
static int ntsc_crt_prepare(struct filter_data *filt, const void *in_data,
        unsigned width, unsigned height, size_t in_pitch, unsigned colfmt)
{

    /* ---- Convert input to RGB byte-array for crt_modulate ---- */
    unsigned char *rgb_in;
    if (filt->in_buf_w != width || filt->in_buf_h != height) {
        unsigned char *new_buf = (unsigned char*)realloc(filt->in_buf,
                                                          width * height * 3);
        if (!new_buf) return 0;
        filt->in_buf   = new_buf;
        filt->in_buf_w = width;
        filt->in_buf_h = height;
    }
    rgb_in = filt->in_buf;

    if (colfmt == SOFTFILTER_FMT_XRGB8888) {
        unsigned y;
        for (y = 0; y < height; y++) {
            const uint32_t *row = (const uint32_t*)
                                  ((const uint8_t*)in_data + y * in_pitch);
            xrgb8888_to_rgb(row, rgb_in + y * width * 3, width);
        }
    } else {
        unsigned y;
        for (y = 0; y < height; y++) {
            const uint16_t *row = (const uint16_t*)
                                  ((const uint8_t*)in_data + y * in_pitch);
            rgb565_to_rgb(row, rgb_in + y * width * 3, width);
        }
    }

    /* ---- Resize CRT output buffer if needed ---- */
    if (filt->out_buf_w != width || filt->out_buf_h != height) {
        unsigned char *new_buf = (unsigned char*)realloc(filt->out_buf,
                                                          width * height * 3);
        if (!new_buf) return 0;
        memset(new_buf, 0, width * height * 3);
        filt->out_buf   = new_buf;
        filt->out_buf_w = width;
        filt->out_buf_h = height;
        crt_resize(&filt->crt, width, height, CRT_PIX_FORMAT_RGB, filt->out_buf);
    }
    filt->crt.out = filt->out_buf;

    /* ---- Set up NTSC_SETTINGS for this frame ---- */
    filt->ntsc.data   = rgb_in;
    filt->ntsc.format = CRT_PIX_FORMAT_RGB;
    filt->ntsc.w      = (int)width;
    filt->ntsc.h      = (int)height;
    filt->ntsc.raw    = 0;   /* scale to fit */
    filt->ntsc.field  = filt->field;
    filt->ntsc.frame  = filt->frame & 1;
    filt->ntsc.hue    = filt->hue;

    /* ---- Encoding's once-a-frame part; the rest is the tasks ---- */
    filt->mod_lines = crt_modulate_begin(&filt->crt, &filt->ntsc, &filt->mod);

    /* ---- Advance field/frame counters ---- */
    filt->field ^= 1;
    if (filt->field == 0) filt->frame++;

    return 1;
}

/* ---- The frame's tasks ----
 * Encoding: task k encodes video lines [16k, 16k + 16) into their
 * signal rows and adds the noise to its rows of the signal; the first
 * and last also take the rows above and below the picture, which hold
 * sync and burst alone. Then one task finds the syncs and each line's
 * colour, in line order (crt_demodulate_begin), once every encoding
 * task is done. Then task j decodes active lines [16j, 16j + 16) and
 * copies the output rows they draw to the destination.
 *
 * Workers take the tasks in order as each comes free, so a task waits
 * only on tasks already taken, which their workers are running: the
 * work cannot stall however many workers there are or whether they run
 * at once, and a single worker (or a caller running the packets one
 * after another) simply does every task itself. Every task writes its
 * own lines and rows, as before, so the output is unchanged. */
#define CRT_TASK_LINES 32 /* a multiple of every lane count */

#if defined(__SSE2__) || defined(_M_X64) \
   || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define CRT_RELAX() _mm_pause()
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#define CRT_RELAX() __asm__ __volatile__("yield")
#else
#define CRT_RELAX()
#endif

/* Encoding task k: its video lines, then the noise on its rows. */
static void ntsc_crt_encode_task(struct filter_data *filt,
        struct softfilter_thread_data *thr, int k)
{
    const struct crt_mod_frame *mf = &filt->mod;
    int first = k * CRT_TASK_LINES;
    int last  = first + CRT_TASK_LINES;
    int r0, r1;
    if (last > mf->desth) last = mf->desth;
    if (filt->mod_lines && first < last)
        crt_modulate_lines(&filt->crt, &filt->ntsc, mf, &thr->mod_lanes,
                first, last);
    r0 = (k == 0)                   ? 0        : mf->yo + first;
    r1 = (k == filt->mod_tasks - 1) ? CRT_VRES : mf->yo + first + CRT_TASK_LINES;
    crt_noise_range(&filt->crt, (size_t)r0 * CRT_HRES,
            (size_t)r1 * CRT_HRES, filt->noise, filt->lanes_tier);
}

/* Where a band of active lines may end at or after line b: a line with
 * no rows of its own (more lines than output rows) still draws one, the
 * next line's first, which that line then draws over or blends with, so
 * the two go in the same band. */
static int ntsc_crt_band_edge(const struct CRT *v, int b)
{
    while (b > 0 && b < CRT_LINES && !v->lines[b - 1].skip
            && v->lines[b - 1].end <= v->lines[b - 1].beg)
        b++;
    return b;
}

/* Decode active lines [lf, ll) and copy the output rows they own: from
 * the first's first row to the next line's, the lines from 0 starting
 * at row 0 and the lines to the bottom ending there, so the bands of
 * lines split the rows between them and every row is copied once. */
static void ntsc_crt_decode(struct filter_data *filt,
        struct softfilter_thread_data *thr, int lf, int ll)
{
    int height = (int)thr->height;
    int rf, rl, y;

    lf = ntsc_crt_band_edge(&filt->crt, lf);
    ll = ntsc_crt_band_edge(&filt->crt, ll);
    if (lf >= ll)
        return;

    crt_demodulate_lines(&filt->crt, lf, ll, thr->scratch, &thr->lanes);

    rf = (lf == 0)         ? 0      : filt->crt.lines[lf].beg;
    rl = (ll == CRT_LINES) ? height : filt->crt.lines[ll].beg;
    if (rf < 0)       rf = 0;
    if (rf > height)  rf = height;
    if (rl < rf)      rl = rf;
    if (rl > height)  rl = height;

    for (y = rf; y < rl; y++) {
        uint32_t *dst_row = (uint32_t*)
                            ((uint8_t*)thr->out_data + y * thr->out_pitch);
        const unsigned char *src_row = filt->out_buf + (size_t)y * thr->width * 3;
        rgb_to_xrgb8888(src_row, dst_row, thr->width);
    }
}

#if defined(RETRO_ATOMIC_LOCK_FREE)
static void ntsc_crt_task(struct filter_data *filt,
        struct softfilter_thread_data *thr, int t)
{
    if (t < filt->mod_tasks) {
        ntsc_crt_encode_task(filt, thr, t);
        retro_atomic_fetch_add_int(&filt->mod_done, 1);
    } else if (t == filt->mod_tasks) {
        while (retro_atomic_load_acquire_int(&filt->mod_done)
                < filt->mod_tasks)
            CRT_RELAX();
        crt_demodulate_begin(&filt->crt, filt->noise);
        retro_atomic_store_release_int(&filt->begin_done, 1);
    } else {
        int lf = (t - filt->mod_tasks - 1) * CRT_TASK_LINES;
        int ll = lf + CRT_TASK_LINES;
        if (ll > CRT_LINES) ll = CRT_LINES;
        while (!retro_atomic_load_acquire_int(&filt->begin_done))
            CRT_RELAX();
        ntsc_crt_decode(filt, thr, lf, ll);
    }
}
#endif

/* A worker: take the frame's tasks until there are none left; without
 * lock-free atomics, decode the band of lines the packets pass gave it
 * (that pass having done the rest). */
static void ntsc_crt_work_cb(void *data, void *thread_data)
{
    struct filter_data             *filt = (struct filter_data*)data;
    struct softfilter_thread_data  *thr  = (struct softfilter_thread_data*)thread_data;

    if (!filt->tasks)
        return;
#if defined(RETRO_ATOMIC_LOCK_FREE)
    for (;;) {
        int t = retro_atomic_fetch_add_int(&filt->next_task, 1);
        if (t >= filt->tasks)
            break;
        ntsc_crt_task(filt, thr, t);
    }
#else
    ntsc_crt_decode(filt, thr, thr->line_first, thr->line_last);
#endif
}

/* -----------------------------------------------------------------------
 * Packet submission (same structure as scanline2x)
 * ----------------------------------------------------------------------- */
static void ntsc_crt_packets(void *data,
        struct softfilter_work_packet *packets,
        void *output, size_t output_stride,
        const void *input, unsigned width, unsigned height,
        size_t input_stride)
{
    unsigned i;
    struct filter_data *filt = (struct filter_data*)data;

    filt->tasks = 0;
    if (ntsc_crt_prepare(filt, input, width, height,
            input_stride, filt->in_fmt)) {
        filt->mod_tasks = (filt->mod.desth + CRT_TASK_LINES - 1)
            / CRT_TASK_LINES;
        if (filt->mod_tasks < 1)
            filt->mod_tasks = 1;
        filt->dec_tasks = (CRT_LINES + CRT_TASK_LINES - 1) / CRT_TASK_LINES;
        filt->tasks     = filt->mod_tasks + 1 + filt->dec_tasks;
    }
#if defined(RETRO_ATOMIC_LOCK_FREE)
    /* The workers are woken after this, which orders these for them */
    retro_atomic_store_relaxed_int(&filt->next_task, 0);
    retro_atomic_store_relaxed_int(&filt->mod_done, 0);
    retro_atomic_store_relaxed_int(&filt->begin_done, 0);
#else
    /* Encoding and the start of decoding here; bands of lines after */
    if (filt->tasks) {
        int k;
        for (k = 0; k < filt->mod_tasks; k++)
            ntsc_crt_encode_task(filt, &filt->workers[0], k);
        crt_demodulate_begin(&filt->crt, filt->noise);
    }
#endif

    for (i = 0; i < filt->threads; i++) {
        struct softfilter_thread_data *thr = &filt->workers[i];

        thr->out_data   = (uint8_t*)output;
        thr->in_data    = (const uint8_t*)input;
        thr->out_pitch  = output_stride;
        thr->in_pitch   = input_stride;
        thr->width      = width;
        thr->height     = height;
        thr->colfmt     = filt->in_fmt;
        thr->line_first = (int)((CRT_LINES * i) / filt->threads);
        thr->line_last  = (int)((CRT_LINES * (i + 1)) / filt->threads);
        thr->scratch    = filt->scratch + (size_t)i * (AV_LEN + 1);

        packets[i].work        = ntsc_crt_work_cb;
        packets[i].thread_data = thr;
    }
}

/* -----------------------------------------------------------------------
 * Implementation descriptor — matches struct softfilter_implementation
 * ----------------------------------------------------------------------- */
static const struct softfilter_implementation ntsc_crt_impl = {
    ntsc_crt_input_fmts,
    ntsc_crt_output_fmts,

    ntsc_crt_create,
    ntsc_crt_destroy,

    ntsc_crt_threads,
    ntsc_crt_output_size,
    ntsc_crt_packets,

    SOFTFILTER_API_VERSION,
    "NTSC/CRT",
    "ntsc_crt",
};

const struct softfilter_implementation *softfilter_get_implementation(
        softfilter_simd_mask_t simd)
{
    (void)simd;
    return &ntsc_crt_impl;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
