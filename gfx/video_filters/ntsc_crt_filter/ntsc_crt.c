/*  RetroArch CPU Filter - NTSC/CRT
 *
 *  Original NTSC/CRT library by EMMIR 2018-2023
 *    https://www.youtube.com/@EMMIR_KC/videos
 *
 *  RetroArch softfilter wrapper 2024
 *
 *  Compile (Linux):
 *    gcc -O2 -shared -fPIC -std=c99 \
 *        -o ntsc_crt.so \
 *        ntsc_crt.c crt_core.c crt_ntsc.c \
 *        -lm
 *
 *  Compile (Windows cross):
 *    x86_64-w64-mingw32-gcc -O2 -shared -std=c99 \
 *        -o ntsc_crt.dll \
 *        ntsc_crt.c crt_core.c crt_ntsc.c \
 *        -lm
 *
 *  Install:
 *    Copy ntsc_crt.so (or .dll) + ntsc_crt.filt to:
 *      <RetroArch>/filters/video/
 */

#include "softfilter.h"
#include "crt_core.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation ntsc_crt_get_implementation
#define softfilter_thread_data        ntsc_crt_softfilter_thread_data
#define filter_data                   ntsc_crt_filter_data
#endif

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
