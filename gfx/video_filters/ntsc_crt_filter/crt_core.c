/*****************************************************************************/
/*
 * NTSC/CRT - integer-only NTSC video signal encoding / decoding emulation
 *
 *   by EMMIR 2018-2023
 *
 *   YouTube: https://www.youtube.com/@EMMIR_KC/videos
 *   Discord: https://discord.com/invite/hdYctSmyQJ
 */
/*****************************************************************************/
#include "crt_core.h"

#include <stdlib.h>
#include <string.h>

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
extern void
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

extern int
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

extern void
crt_resize(struct CRT *v, int w, int h, int f, unsigned char *out)
{
    v->outw = w;
    v->outh = h;
    v->out_format = f;
    v->out = out;
}

extern void
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

extern void
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

extern void
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
#if ((CRT_SYSTEM == CRT_SYSTEM_NTSCVHS) && CRT_VHS_NOISE)
    line = ((rand() % 8) - 4) + 14;
#endif
    for (i = 0; i < CRT_INPUT_SIZE; i++) {
        int nn = noise;
#if ((CRT_SYSTEM == CRT_SYSTEM_NTSCVHS) && CRT_VHS_NOISE)
        rn = rand();
        if (i > (CRT_INPUT_SIZE - CRT_HRES * (16 + ((rand() % 20) - 10))) &&
            i < (CRT_INPUT_SIZE - CRT_HRES * (5 + ((rand() % 8) - 4)))) {
            int ln, sn, cs;

            ln = (i * line) / CRT_HRES;
            crt_sincos14(&sn, &cs, ln * 8192 / 180);
            nn = cs >> 8;
        }
#else
        rn = (214019 * rn + 140327895);
#endif
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
