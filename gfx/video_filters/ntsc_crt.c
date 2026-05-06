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

static void
crt_demodulate(struct CRT *v, int noise)
{
    /* made static so all this data does not go on the stack */
    static struct {
        int y, i, q;
    } out[AV_LEN + 1], *yiqA, *yiqB;
    int i, j, line, rn;
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

    bpp = crt_bpp4fmt(v->out_format);
    if (bpp == 0) {
        return;
    }
    pitch = v->outw * bpp;

    crt_sincos14(&huesn, &huecs, ((v->hue % 360) + 33) * 8192 / 180);
    huesn >>= 11; /* make 4-bit */
    huecs >>= 11;

    rn = v->rn;
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
    for (i = 0; i < CRT_INPUT_SIZE; i++) {
        int nn = noise;
        rn = (214019 * rn + 140327895);
        /* signal + noise */
        s = v->analog[i] + (((((rn >> 16) & 0xff) - 0x7f) * nn) >> 8);
        if (s >  127) { s =  127; }
        if (s < -127) { s = -127; }
        v->inp[i] = s;
    }
    v->rn = rn;

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
        unsigned pos, ln, scanR;
        int scanL, dx;
        int L, R;
        unsigned char *cL, *cR;
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
        reset_eq(&eqY);
        reset_eq(&eqI);
        reset_eq(&eqQ);

#if (CRT_CC_SAMPLES == 4)
        for (i = L; i < R; i++) {
            out[i].y = eqf(&eqY, sig[i] + bright) << 4;
            out[i].i = eqf(&eqI, sig[i] * wave[(i + 0) & 3] >> 9) >> 3;
            out[i].q = eqf(&eqQ, sig[i] * wave[(i + 3) & 3] >> 9) >> 3;
        }
#else
        for (i = L; i < R; i++) {
            out[i].y = eqf(&eqY, sig[i] + bright) << 4;
            out[i].i = eqf(&eqI, sig[i] * waveI[i % CRT_CC_SAMPLES] >> 9) >> 3;
            out[i].q = eqf(&eqQ, sig[i] * waveQ[i % CRT_CC_SAMPLES] >> 9) >> 3;
        }
#endif

        cL = v->out + (beg * pitch);
        cR = cL + pitch;

        for (pos = scanL; pos < scanR && cL < cR; pos += dx) {
            int y, i, q;
            int r, g, b;
            int aa, bb;

            R = pos & 0xfff;
            L = 0xfff - R;
            s = pos >> 12;

            yiqA = out + s;
            yiqB = out + s + 1;

            /* interpolate between samples if needed */
            y = ((yiqA->y * L) >>  2) + ((yiqB->y * R) >>  2);
            i = ((yiqA->i * L) >> 14) + ((yiqB->i * R) >> 14);
            q = ((yiqA->q * L) >> 14) + ((yiqB->q * R) >> 14);

            /* YIQ to RGB */
            r = (((y + 3879 * i + 2556 * q) >> 12) * v->contrast) >> 8;
            g = (((y - 1126 * i - 2605 * q) >> 12) * v->contrast) >> 8;
            b = (((y - 4530 * i + 7021 * q) >> 12) * v->contrast) >> 8;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            if (v->blend) {
                aa = (r << 16 | g << 8 | b);

                switch (v->out_format) {
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

            switch (v->out_format) {
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

static void
crt_modulate(struct CRT *v, struct NTSC_SETTINGS *s)
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
        return; /* just to be safe */
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
    for (n = 0; n < CRT_CC_VPER; n++) {
        for (x = 0; x < CRT_CC_SAMPLES; x++) {
            v->ccf[n][x] = iccf[x] << 7;
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
        /* expand to 8-bit */
        dst[x * 3 + 0] = ((c >> 11) & 0x1f) << 3;
        dst[x * 3 + 1] = ((c >>  5) & 0x3f) << 2;
        dst[x * 3 + 2] = ((c >>  0) & 0x1f) << 3;
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

static void *ntsc_crt_create(const struct softfilter_config *config,
        unsigned in_fmt, unsigned out_fmt,
        unsigned max_width, unsigned max_height,
        unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
    struct filter_data *filt;
    unsigned char *out_buf;
    (void)out_fmt;
    (void)simd;
    (void)userdata;

    filt = (struct filter_data*)calloc(1, sizeof(*filt));
    if (!filt) return NULL;

    filt->workers = (struct softfilter_thread_data*)
                    calloc(1, sizeof(struct softfilter_thread_data));
    if (!filt->workers) { free(filt); return NULL; }

    /* single-threaded — CRT state is not thread-safe */
    filt->threads = 1;
    filt->in_fmt  = in_fmt;

    /* Allocate CRT output buffer: RGB (3 bpp) */
    out_buf = (unsigned char*)malloc(max_width * max_height * 3);
    if (!out_buf) { free(filt->workers); free(filt); return NULL; }

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
    if (filt->out_buf) free(filt->out_buf);
    free(filt->workers);
    free(filt);
}

/* -----------------------------------------------------------------------
 * The actual work callback — called from RetroArch's worker thread
 * ----------------------------------------------------------------------- */
static void ntsc_crt_work_cb(void *data, void *thread_data)
{
    struct filter_data             *filt = (struct filter_data*)data;
    struct softfilter_thread_data  *thr  = (struct softfilter_thread_data*)thread_data;

    unsigned  width      = thr->width;
    unsigned  height     = thr->height;
    size_t    in_pitch   = thr->in_pitch;
    size_t    out_pitch  = thr->out_pitch;

    /* ---- Convert input to RGB byte-array for crt_modulate ---- */
    unsigned char *rgb_in = (unsigned char*)malloc(width * height * 3);
    if (!rgb_in) return;

    if (thr->colfmt == SOFTFILTER_FMT_XRGB8888) {
        unsigned y;
        for (y = 0; y < height; y++) {
            const uint32_t *row = (const uint32_t*)
                                  ((const uint8_t*)thr->in_data + y * in_pitch);
            xrgb8888_to_rgb(row, rgb_in + y * width * 3, width);
        }
    } else {
        unsigned y;
        for (y = 0; y < height; y++) {
            const uint16_t *row = (const uint16_t*)
                                  ((const uint8_t*)thr->in_data + y * in_pitch);
            rgb565_to_rgb(row, rgb_in + y * width * 3, width);
        }
    }

    /* ---- Resize CRT output buffer if needed ---- */
    if (filt->out_buf_w != width || filt->out_buf_h != height) {
        unsigned char *new_buf = (unsigned char*)realloc(filt->out_buf,
                                                          width * height * 3);
        if (!new_buf) { free(rgb_in); return; }
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

    /* ---- Encode → signal → decode ---- */
    crt_modulate(&filt->crt, &filt->ntsc);
    crt_demodulate(&filt->crt, filt->noise);

    /* ---- Advance field/frame counters ---- */
    filt->field ^= 1;
    if (filt->field == 0) filt->frame++;

    /* ---- Copy RGB output → XRGB8888 destination ---- */
    {
        unsigned y;
        for (y = 0; y < height; y++) {
            uint32_t *dst_row = (uint32_t*)
                                ((uint8_t*)thr->out_data + y * out_pitch);
            const unsigned char *src_row = filt->out_buf + y * width * 3;
            rgb_to_xrgb8888(src_row, dst_row, width);
        }
    }

    free(rgb_in);
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
    struct filter_data            *filt = (struct filter_data*)data;
    struct softfilter_thread_data *thr  = &filt->workers[0];

    thr->out_data  = (uint8_t*)output;
    thr->in_data   = (const uint8_t*)input;
    thr->out_pitch = output_stride;
    thr->in_pitch  = input_stride;
    thr->width     = width;
    thr->height    = height;
    thr->colfmt    = filt->in_fmt;

    packets[0].work        = ntsc_crt_work_cb;
    packets[0].thread_data = thr;
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
