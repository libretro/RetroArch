/*
    Copyright (c) 2016 Micha Mettke

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1.  The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software
        in a product, an acknowledgment in the product documentation would be
        appreciated but is not required.
    2.  Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.
    3.  This notice may not be removed or altered from any source distribution.
*/
#include "zahnrad.h"

/* ==============================================================
 *
 *                          INTERNAL
 *
 * =============================================================== */
#define ZR_POOL_DEFAULT_CAPACITY 16
#define ZR_VALUE_PAGE_CAPACITY 32
#define ZR_DEFAULT_COMMAND_BUFFER_SIZE (4*1024)

enum zr_heading {
    ZR_UP,
    ZR_RIGHT,
    ZR_DOWN,
    ZR_LEFT
};

enum zr_draw_list_stroke {
    ZR_STROKE_OPEN = zr_false,
    /* build up path has no connection back to the beginning */
    ZR_STROKE_CLOSED = zr_true
    /* build up path has a connection back to the beginning */
};

struct zr_text_selection {
    int active;
    /* current selection state */
    zr_size begin;
    /* text selection beginning glyph index */
    zr_size end;
    /* text selection ending glyph index */
};

struct zr_edit_box {
    struct zr_buffer buffer;
    /* glyph buffer to add text into */
    int active;
    /* flag indicating if the buffer is currently being modified  */
    zr_size cursor;
    /* current glyph (not byte) cursor position */
    zr_size glyphs;
    /* number of glyphs inside the edit box */
    struct zr_clipboard clip;
    /* copy paste callbacks */
    zr_filter filter;
    /* input filter callback */
    struct zr_text_selection sel;
    /* text selection */
    float scrollbar;
    /* edit field scrollbar */
    int text_inserted;
};

enum zr_internal_window_flags {
    ZR_WINDOW_PRIVATE       = ZR_FLAG(9),
    /* dummy flag which mark the beginning of the private window flag part */
    ZR_WINDOW_ROM           = ZR_FLAG(10),
    /* sets the window into a read only mode and does not allow input changes */
    ZR_WINDOW_HIDDEN        = ZR_FLAG(11),
    /* Hiddes the window and stops any window interaction and drawing can be set
     * by user input or by closing the window */
    ZR_WINDOW_MINIMIZED     = ZR_FLAG(12),
    /* marks the window as minimized */
    ZR_WINDOW_SUB           = ZR_FLAG(13),
    /* Marks the window as subwindow of another window*/
    ZR_WINDOW_GROUP         = ZR_FLAG(14),
    /* Marks the window as window widget group */
    ZR_WINDOW_POPUP         = ZR_FLAG(15),
    /* Marks the window as a popup window */
    ZR_WINDOW_NONBLOCK      = ZR_FLAG(16),
    /* Marks the window as a nonblock popup window */
    ZR_WINDOW_CONTEXTUAL    = ZR_FLAG(17),
    /* Marks the window as a combo box or menu */
    ZR_WINDOW_COMBO         = ZR_FLAG(18),
    /* Marks the window as a combo box */
    ZR_WINDOW_MENU          = ZR_FLAG(19),
    /* Marks the window as a menu */
    ZR_WINDOW_TOOLTIP       = ZR_FLAG(20),
    /* Marks the window as a menu */
    ZR_WINDOW_REMOVE_ROM    = ZR_FLAG(21)
    /* Removes the read only mode at the end of the window */
};

struct zr_popup {
    struct zr_window *win;
    enum zr_internal_window_flags type;
    zr_hash name;
    int active;

    unsigned combo_count;
    unsigned con_count, con_old;
    unsigned active_con;
};

struct zr_edit_state {
    zr_hash name;
    zr_size cursor;
    struct zr_text_selection sel;
    float scrollbar;
    unsigned int seq;
    unsigned int old;
    int active, prev;
};

struct zr_value {
    int active, prev;
    char buffer[ZR_MAX_NUMBER_BUFFER];
    zr_size length;
    zr_size cursor;
    zr_hash name;
    unsigned int seq;
    unsigned int old;
    int state;
};

struct zr_table {
    unsigned int seq;
    zr_hash keys[ZR_VALUE_PAGE_CAPACITY];
    zr_uint values[ZR_VALUE_PAGE_CAPACITY];
    struct zr_table *next, *prev;
};

struct zr_window {
    zr_hash name;
    /* name of this window */
    unsigned int seq;
    /* window lifeline */
    struct zr_rect bounds;
    /* window size and position */
    zr_flags flags;
    /* window flags modifing its behavior */
    struct zr_scroll scrollbar;
    /* scrollbar x- and y-offset */
    struct zr_command_buffer buffer;
    /* command buffer for queuing drawing calls */

    /* frame window state */
    struct zr_panel *layout;

    /* persistent widget state */
    struct zr_value property;
    struct zr_popup popup;
    struct zr_edit_state edit;

    struct zr_table *tables;
    unsigned short table_count;
    unsigned short table_size;

    /* window list */
    struct zr_window *next;
    struct zr_window *prev;
    struct zr_window *parent;
};

union zr_page_data {
    struct zr_table tbl;
    struct zr_window win;
};

struct zr_window_page {
    unsigned size;
    struct zr_window_page *next;
    union zr_page_data win[1];
};

struct zr_pool {
    struct zr_allocator alloc;
    enum zr_allocation_type type;
    unsigned int page_count;
    struct zr_window_page *pages;
    unsigned capacity;
    zr_size size;
    zr_size cap;
};

/* ==============================================================
 *                          MATH
 * =============================================================== */
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
#ifndef CLAMP
#define CLAMP(i,v,x) (MAX(MIN(v,x), i))
#endif

#define ZR_PI 3.141592654f
#define ZR_UTF_INVALID 0xFFFD
#define ZR_MAX_FLOAT_PRECISION 2

#define ZR_UNUSED(x) ((void)(x))
#define ZR_SATURATE(x) (MAX(0, MIN(1.0f, x)))
#define ZR_LEN(a) (sizeof(a)/sizeof(a)[0])
#define ZR_ABS(a) (((a) < 0) ? -(a) : (a))
#define ZR_BETWEEN(x, a, b) ((a) <= (x) && (x) <= (b))
#define ZR_INBOX(px, py, x, y, w, h)\
    (ZR_BETWEEN(px,x,x+w) && ZR_BETWEEN(py,y,y+h))
#define ZR_INTERSECT(x0, y0, w0, h0, x1, y1, w1, h1) \
    (!(((x1 > (x0 + w0)) || ((x1 + w1) < x0) || (y1 > (y0 + h0)) || (y1 + h1) < y0)))
#define ZR_CONTAINS(x, y, w, h, bx, by, bw, bh)\
    (ZR_INBOX(x,y, bx, by, bw, bh) && ZR_INBOX(x+w,y+h, bx, by, bw, bh))

#define zr_vec2_mov(to,from) (to).x = (from).x, (to).y = (from).y
#define zr_vec2_sub(a, b) zr_vec2((a).x - (b).x, (a).y - (b).y)
#define zr_vec2_add(a, b) zr_vec2((a).x + (b).x, (a).y + (b).y)
#define zr_vec2_len_sqr(a) ((a).x*(a).x+(a).y*(a).y)
#define zr_vec2_muls(a, t) zr_vec2((a).x * (t), (a).y * (t))

#define zr_ptr_add(t, p, i) ((t*)((void*)((zr_byte*)(p) + (i))))
#define zr_ptr_add_const(t, p, i) ((const t*)((const void*)((const zr_byte*)(p) + (i))))

static const struct zr_rect zr_null_rect = {-8192.0f, -8192.0f, 16384, 16384};
static const float FLOAT_PRECISION = 0.00000000000001f;

/* ==============================================================
 *                          ALIGNMENT
 * =============================================================== */
/* Pointer to Integer type conversion for pointer alignment */
#if defined(__PTRDIFF_TYPE__) /* This case should work for GCC*/
# define ZR_UINT_TO_PTR(x) ((void*)(__PTRDIFF_TYPE__)(x))
# define ZR_PTR_TO_UINT(x) ((zr_size)(__PTRDIFF_TYPE__)(x))
#elif !defined(__GNUC__) /* works for compilers other than LLVM */
# define ZR_UINT_TO_PTR(x) ((void*)&((char*)0)[x])
# define ZR_PTR_TO_UINT(x) ((zr_size)(((char*)x)-(char*)0))
#elif defined(ZR_USE_FIXED_TYPES) /* used if we have <stdint.h> */
# define ZR_UINT_TO_PTR(x) ((void*)(uintptr_t)(x))
# define ZR_PTR_TO_UINT(x) ((uintptr_t)(x))
#else /* generates warning but works */
# define ZR_UINT_TO_PTR(x) ((void*)(x))
# define ZR_PTR_TO_UINT(x) ((zr_size)(x))
#endif

#define ZR_ALIGN_PTR(x, mask)\
    (ZR_UINT_TO_PTR((ZR_PTR_TO_UINT((zr_byte*)(x) + (mask-1)) & ~(mask-1))))
#define ZR_ALIGN_PTR_BACK(x, mask)\
    (ZR_UINT_TO_PTR((ZR_PTR_TO_UINT((zr_byte*)(x)) & ~(mask-1))))

#ifdef __cplusplus
template<typename T> struct zr_alignof;
template<typename T, int size_diff> struct zr_helper{enum {value = size_diff};};
template<typename T> struct zr_helper<T,0>{enum {value = zr_alignof<T>::value};};
template<typename T> struct zr_alignof{struct Big {T x; char c;}; enum {
    diff = sizeof(Big) - sizeof(T), value = zr_helper<Big, diff>::value};};
#define ZR_ALIGNOF(t) (zr_alignof<t>::value);
#else
#define ZR_ALIGNOF(t) ((char*)(&((struct {char c; t _h;}*)0)->_h) - (char*)0)
#endif

/* make sure correct type size */
typedef int zr__check_size[(sizeof(zr_size) >= sizeof(void*)) ? 1 : -1];
typedef int zr__check_ptr[(sizeof(zr_ptr) == sizeof(void*)) ? 1 : -1];
typedef int zr__check_flags[(sizeof(zr_flags) >= 4) ? 1 : -1];
typedef int zr__check_rune[(sizeof(zr_rune) >= 4) ? 1 : -1];
typedef int zr__check_uint[(sizeof(zr_uint) == 4) ? 1 : -1];
typedef int zr__check_byte[(sizeof(zr_byte) == 1) ? 1 : -1];
/*
 * ==============================================================
 *
 *                          MATH
 *
 * ===============================================================
 */
/*  Since zahnrad is supposed to work on all systems providing floating point
    math without any dependencies I also had to implement my own math functions
    for sqrt, sin and cos. Since the actual highly accurate implementations for
    the standard library functions are quite complex and I do not need high
    precision for my use cases I use approximations.

    Sqrt
    ----
    For square root zahnrad uses the famous fast inverse square root:
    https://en.wikipedia.org/wiki/Fast_inverse_square_root with
    slightly tweaked magic constant. While on todays hardware it is
    probably not faster it is still fast and accurate enough for
    zahnrads use cases.

    Sine/Cosine
    -----------
    All constants inside both function are generated Remez's minimax
    approximations for value range 0...2*PI. The reason why I decided to
    approximate exactly that range is that zahnrad only needs sine and
    cosine to generate circles which only requires that exact range.
    In addition I used Remez instead of Taylor for additional precision:
    www.lolengine.net/blog/2011/12/21/better-function-approximatations.

    The tool I used to generate constants for both sine and cosine
    (it can actually approximate a lot more functions) can be
    found here: www.lolengine.net/wiki/oss/lolremez
*/
static float
zr_inv_sqrt(float number)
{
    float x2;
    const float threehalfs = 1.5f;
    union {zr_uint i; float f;} conv = {0};
    conv.f = number;
    x2 = number * 0.5f;
    conv.i = 0x5f375A84 - (conv.i >> 1);
    conv.f = conv.f * (threehalfs - (x2 * conv.f * conv.f));
    return conv.f;
}

static float
zr_sin(float x)
{
    static const float a0 = +1.91059300966915117e-31f;
    static const float a1 = +1.00086760103908896f;
    static const float a2 = -1.21276126894734565e-2f;
    static const float a3 = -1.38078780785773762e-1f;
    static const float a4 = -2.67353392911981221e-2f;
    static const float a5 = +2.08026600266304389e-2f;
    static const float a6 = -3.03996055049204407e-3f;
    static const float a7 = +1.38235642404333740e-4f;
    return a0 + x*(a1 + x*(a2 + x*(a3 + x*(a4 + x*(a5 + x*(a6 + x*a7))))));
}

static float
zr_cos(float x)
{
    static const float a0 = +1.00238601909309722f;
    static const float a1 = -3.81919947353040024e-2f;
    static const float a2 = -3.94382342128062756e-1f;
    static const float a3 = -1.18134036025221444e-1f;
    static const float a4 = +1.07123798512170878e-1f;
    static const float a5 = -1.86637164165180873e-2f;
    static const float a6 = +9.90140908664079833e-4f;
    static const float a7 = -5.23022132118824778e-14f;
    return a0 + x*(a1 + x*(a2 + x*(a3 + x*(a4 + x*(a5 + x*(a6 + x*a7))))));
}

static zr_uint
zr_round_up_pow2(zr_uint v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

struct zr_rect
zr_get_null_rect(void)
{
    return zr_null_rect;
}

struct zr_rect
zr_rect(float x, float y, float w, float h)
{
    struct zr_rect r;
    r.x = x, r.y = y;
    r.w = w, r.h = h;
    return r;
}

static struct zr_rect
zr_shrink_rect(struct zr_rect r, float amount)
{
    struct zr_rect res;
    r.w = MAX(r.w, 2 * amount);
    r.h = MAX(r.h, 2 * amount);
    res.x = r.x + amount;
    res.y = r.y + amount;
    res.w = r.w - 2 * amount;
    res.h = r.h - 2 * amount;
    return res;
}

static struct zr_rect
zr_pad_rect(struct zr_rect r, struct zr_vec2 pad)
{
    r.w = MAX(r.w, 2 * pad.x);
    r.h = MAX(r.h, 2 * pad.y);
    r.x += pad.x; r.y += pad.y;
    r.w -= 2 * pad.x;
    r.h -= 2 * pad.y;
    return r;
}

struct zr_vec2
zr_vec2(float x, float y)
{
    struct zr_vec2 ret;
    ret.x = x; ret.y = y;
    return ret;
}
/*
 * ==============================================================
 *
 *                          STANDARD
 *
 * ===============================================================
 */
static int zr_str_match_here(const char *regexp, const char *text);
static int zr_str_match_star(int c, const char *regexp, const char *text);

static void*
zr_memcopy(void *dst0, const void *src0, zr_size length)
{
    zr_ptr t;
    typedef int word;
    char *dst = (char*)dst0;
    const char *src = (const char*)src0;
    if (length == 0 || dst == src)
        goto done;

    #define wsize sizeof(word)
    #define wmask (wsize-1)
    #define TLOOP(s) if (t) TLOOP1(s)
    #define TLOOP1(s) do { s; } while (--t)

    if (dst < src) {
        t = (zr_ptr)src; /* only need low bits */
        if ((t | (zr_ptr)dst) & wmask) {
            if ((t ^ (zr_ptr)dst) & wmask || length < wsize)
                t = length;
            else
                t = wsize - (t & wmask);
            length -= t;
            TLOOP1(*dst++ = *src++);
        }
        t = length / wsize;
        TLOOP(*(word*)(void*)dst = *(const word*)(const void*)src;
            src += wsize; dst += wsize);
        t = length & wmask;
        TLOOP(*dst++ = *src++);
    } else {
        src += length;
        dst += length;
        t = (zr_ptr)src;
        if ((t | (zr_ptr)dst) & wmask) {
            if ((t ^ (zr_ptr)dst) & wmask || length <= wsize)
                t = length;
            else
                t &= wmask;
            length -= t;
            TLOOP1(*--dst = *--src);
        }
        t = length / wsize;
        TLOOP(src -= wsize; dst -= wsize;
            *(word*)(void*)dst = *(const word*)(const void*)src);
        t = length & wmask;
        TLOOP(*--dst = *--src);
    }
    #undef wsize
    #undef wmask
    #undef TLOOP
    #undef TLOOP1
done:
    return (dst0);
}

static void
zr_memset(void *ptr, int c0, zr_size size)
{
    #define word unsigned
    #define wsize sizeof(word)
    #define wmask (wsize - 1)
    zr_byte *dst = (zr_byte*)ptr;
    unsigned c = 0;
    zr_size t = 0;

    if ((c = (zr_byte)c0) != 0) {
        c = (c << 8) | c; /* at least 16-bits  */
        if (sizeof(unsigned int) > 2)
            c = (c << 16) | c; /* at least 32-bits*/
    }

    /* to small of a word count */
    dst = (zr_byte*)ptr;
    if (size < 3 * wsize) {
        while (size--) *dst++ = (zr_byte)c0;
        return;
    }

    /* align destination */
    if ((t = ZR_PTR_TO_UINT(dst) & wmask) != 0) {
        t = wsize -t;
        size -= t;
        do {
            *dst++ = (zr_byte)c0;
        } while (--t != 0);
    }

    /* fill word */
    t = size / wsize;
    do {
        *(word*)((void*)dst) = c;
        dst += wsize;
    } while (--t != 0);

    /* fill trailing bytes */
    t = (size & wmask);
    if (t != 0) {
        do {
            *dst++ = (zr_byte)c0;
        } while (--t != 0);
    }

    #undef word
    #undef wsize
    #undef wmask
}

#define zr_zero_struct(s) zr_zero(&s, sizeof(s))

static void
zr_zero(void *ptr, zr_size size)
{
    ZR_ASSERT(ptr);
    zr_memset(ptr, 0, size);
}

static zr_size
zr_strsiz(const char *str)
{
    zr_size siz = 0;
    ZR_ASSERT(str);
    while (str && *str++ != '\0') siz++;
    return siz;
}

static int
zr_strtof(float *number, const char *buffer)
{
    float m;
    float neg = 1.0f;
    const char *p = buffer;
    float floatvalue = 0;

    ZR_ASSERT(number);
    ZR_ASSERT(buffer);
    if (!number || !buffer) return 0;
    *number = 0;

    /* skip whitespace */
    while (*p && *p == ' ') p++;
    if (*p == '-') {
        neg = -1.0f;
        p++;
    }

    while( *p && *p != '.' && *p != 'e' ) {
        floatvalue = floatvalue * 10.0f + (float) (*p - '0');
        p++;
    }

    if ( *p == '.' ) {
        p++;
        for(m = 0.1f; *p && *p != 'e'; p++ ) {
            floatvalue = floatvalue + (float) (*p - '0') * m;
            m *= 0.1f;
        }
    }
    if ( *p == 'e' ) {
        int i, pow, div;
        p++;
        if ( *p == '-' ) {
            div = zr_true;
            p++;
        } else if ( *p == '+' ) {
            div = zr_false;
            p++;
        } else div = zr_false;

        for ( pow = 0; *p; p++ )
            pow = pow * 10 + (int) (*p - '0');

        for ( m = 1.0, i = 0; i < pow; i++ )
            m *= 10.0f;

        if ( div )
            floatvalue /= m;
        else floatvalue *= m;
    }
    *number = floatvalue * neg;
    return 1;
}

static int
zr_str_match_here(const char *regexp, const char *text)
{
    if (regexp[0] == '\0')
        return 1;
    if (regexp[1] == '*')
        return zr_str_match_star(regexp[0], regexp+2, text);
    if (regexp[0] == '$' && regexp[1] == '\0')
        return *text == '\0';
    if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
        return zr_str_match_here(regexp+1, text+1);
    return 0;
}

static int
zr_str_match_star(int c, const char *regexp, const char *text)
{
    do {/* a '* matches zero or more instances */
        if (zr_str_match_here(regexp, text))
            return 1;
    } while (*text != '\0' && (*text++ == c || c == '.'));
    return 0;
}

static int
zr_strfilter(const char *text, const char *regexp)
{
    /*
    c    matches any literal character c
    .    matches any single character
    ^    matches the beginning of the input string
    $    matches the end of the input string
    *    matches zero or more occurrences of the previous character*/
    if (regexp[0] == '^')
        return zr_str_match_here(regexp+1, text);
    do {    /* must look even if string is empty */
        if (zr_str_match_here(regexp, text))
            return 1;
    } while (*text++ != '\0');
    return 0;
}

static float
zr_pow(float x, int n)
{
    /*  check the sign of n */
    float r = 1;
    int plus = n >= 0;
    n = (plus) ? n : -n;
    while (n > 0) {
        if ((n & 1) == 1)
            r *= x;
        n /= 2;
        x *= x;
    }
    return plus ? r : 1.0f / r;
}

static float
zr_floor(float x)
{
    return (float)((int)x - ((x < 0.0) ? 1 : 0));
}

static int
zr_log10(float n)
{
    int neg;
    int ret;
    int exp = 0;

    neg = (n < 0) ? 1 : 0;
    ret = (neg) ? (int)-n : (int)n;
    while ((ret / 10) > 0) {
        ret /= 10;
        exp++;
    }
    if (neg) exp = -exp;
    return exp;
}

static zr_size
zr_ftos(char *s, float n)
{
    int useExp = 0;
    int digit = 0, m = 0, m1 = 0;
    char *c = s;
    int neg = 0;

    if (n == 0.0) {
        s[0] = '0'; s[1] = '\0';
        return 1;
    }

    neg = (n < 0);
    if (neg) n = -n;

    /* calculate magnitude */
    m = zr_log10(n);
    useExp = (m >= 14 || (neg && m >= 9) || m <= -9);
    if (neg) *(c++) = '-';

    /* set up for scientific notation */
    if (useExp) {
        if (m < 0)
           m -= 1;
        n = n / zr_pow(10.0, m);
        m1 = m;
        m = 0;
    }
    if (m < 1.0) {
        m = 0;
    }

    /* convert the number */
    while (n > FLOAT_PRECISION || m >= 0) {
        float weight = zr_pow(10.0, m);
        if (weight > 0) {
            float t = (float)n / weight;
            float tmp = zr_floor(t);
            digit = (int)tmp;
            n -= ((float)digit * weight);
            *(c++) = (char)('0' + (char)digit);
        }
        if (m == 0 && n > 0)
            *(c++) = '.';
        m--;
    }

    if (useExp) {
        /* convert the exponent */
        int i, j;
        *(c++) = 'e';
        if (m1 > 0) {
            *(c++) = '+';
        } else {
            *(c++) = '-';
            m1 = -m1;
        }
        m = 0;
        while (m1 > 0) {
            *(c++) = (char)('0' + (char)(m1 % 10));
            m1 /= 10;
            m++;
        }
        c -= m;
        for (i = 0, j = m-1; i<j; i++, j--) {
            /* swap without temporary */
            c[i] ^= c[j];
            c[j] ^= c[i];
            c[i] ^= c[j];
        }
        c += m;
    }
    *(c) = '\0';
    return (zr_size)(c - s);
}

static zr_hash
zr_murmur_hash(const void * key, int len, zr_hash seed)
{
    /* 32-Bit MurmurHash3: https://code.google.com/p/smhasher/wiki/MurmurHash3*/
    #define ZR_ROTL(x,r) ((x) << (r) | ((x) >> (32 - r)))
    union {const zr_uint *i; const zr_byte *b;} conv = {0};
    const zr_byte *data = (const zr_byte*)key;
    const int nblocks = len/4;
    zr_uint h1 = seed;
    const zr_uint c1 = 0xcc9e2d51;
    const zr_uint c2 = 0x1b873593;
    const zr_byte *tail;
    const zr_uint *blocks;
    zr_uint k1;
    int i;

    /* body */
    if (!key) return 0;
    conv.b = (data + nblocks*4);
    blocks = (const zr_uint*)conv.i;
    for (i = -nblocks; i; ++i) {
        k1 = blocks[i];
        k1 *= c1;
        k1 = ZR_ROTL(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ZR_ROTL(h1,13);
        h1 = h1*5+0xe6546b64;
    }

    /* tail */
    tail = (const zr_byte*)(data + nblocks*4);
    k1 = 0;
    switch (len & 3) {
    case 3: k1 ^= (zr_uint)(tail[2] << 16);
    case 2: k1 ^= (zr_uint)(tail[1] << 8u);
    case 1: k1 ^= tail[0];
            k1 *= c1;
            k1 = ZR_ROTL(k1,15);
            k1 *= c2;
            h1 ^= k1;
    default: break;
    }

    /* finalization */
    h1 ^= (zr_uint)len;
    /* fmix32 */
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    #undef ZR_ROTL
    return h1;
}

/*
 * ==============================================================
 *
 *                          COLOR
 *
 * ===============================================================
 */
struct zr_color
zr_rgba(zr_byte r, zr_byte g, zr_byte b, zr_byte a)
{
    struct zr_color ret;
    ret.r = r; ret.g = g;
    ret.b = b; ret.a = a;
    return ret;
}

struct zr_color
zr_rgb(zr_byte r, zr_byte g, zr_byte b)
{
    struct zr_color ret;
    ret.r = r; ret.g = g;
    ret.b = b; ret.a = 255;
    return ret;
}

struct zr_color
zr_rgba32(zr_uint in)
{
    struct zr_color ret;
    ret.r = (in & 0xFF);
    ret.g = ((in >> 8) & 0xFF);
    ret.b = ((in >> 16) & 0xFF);
    ret.a = (zr_byte)((in >> 24) & 0xFF);
    return ret;
}

struct zr_color
zr_rgba_f(float r, float g, float b, float a)
{
    struct zr_color ret;
    ret.r = (zr_byte)(ZR_SATURATE(r) * 255.0f);
    ret.g = (zr_byte)(ZR_SATURATE(g) * 255.0f);
    ret.b = (zr_byte)(ZR_SATURATE(b) * 255.0f);
    ret.a = (zr_byte)(ZR_SATURATE(a) * 255.0f);
    return ret;
}

struct zr_color
zr_rgb_f(float r, float g, float b)
{
    struct zr_color ret;
    ret.r = (zr_byte)(ZR_SATURATE(r) * 255.0f);
    ret.g = (zr_byte)(ZR_SATURATE(g) * 255.0f);
    ret.b = (zr_byte)(ZR_SATURATE(b) * 255.0f);
    ret.a = 255;
    return ret;
}

struct zr_color
zr_hsv(zr_byte h, zr_byte s, zr_byte v)
{
    return zr_hsva(h, s, v, 255);
}

struct zr_color
zr_hsv_f(float h, float s, float v)
{
    return zr_hsva_f(h, s, v, 1.0f);
}

struct zr_color
zr_hsva(zr_byte h, zr_byte s, zr_byte v, zr_byte a)
{
    float hf = (float)h / 255.0f;
    float sf = (float)s / 255.0f;
    float vf = (float)v / 255.0f;
    float af = (float)a / 255.0f;
    return zr_hsva_f(hf, sf, vf, af);
}

struct zr_color
zr_hsva_f(float h, float s, float v, float a)
{
    struct zr_colorf {float r,g,b;} out = {0,0,0};
    float hh, p, q, t, ff;
    zr_uint i;

    if (s <= 0.0f) {
        out.r = v; out.g = v; out.b = v;
        return zr_rgb_f(out.r, out.g, out.b);
    }

    hh = h;
    if (hh >= 360.0f) hh = 0;
    hh /= 60.0f;
    i = (zr_uint)hh;
    ff = hh - (float)i;
    p = v * (1.0f - s);
    q = v * (1.0f - (s * ff));
    t = v * (1.0f - (s * (1.0f - ff)));

    switch (i) {
    case 0:
        out.r = v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = v;
        out.b = t;
        break;
    case 3:
        out.r = p;
        out.g = q;
        out.b = v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = v;
        break;
    case 5:
    default:
        out.r = v;
        out.g = p;
        out.b = q;
        break;
    }
    return zr_rgba_f(out.r, out.g, out.b, a);
}

zr_uint
zr_color32(struct zr_color in)
{
    zr_uint out = (zr_uint)in.r;
    out |= ((zr_uint)in.g << 8);
    out |= ((zr_uint)in.b << 16);
    out |= ((zr_uint)in.a << 24);
    return out;
}

void
zr_colorf(float *r, float *g, float *b, float *a, struct zr_color in)
{
    static const float s = 1.0f/255.0f;
    *r = (float)in.r * s;
    *g = (float)in.g * s;
    *b = (float)in.b * s;
    *a = (float)in.a * s;
}

void
zr_color_hsv_f(float *out_h, float *out_s, float *out_v, struct zr_color in)
{
    float a;
    zr_color_hsva_f(out_h, out_s, out_v, &a, in);
}

void
zr_color_hsva_f(float *out_h, float *out_s,
    float *out_v, float *out_a, struct zr_color in)
{
    float chroma;
    float K = 0.0f;
    float r,g,b,a;

    zr_colorf(&r,&g,&b,&a, in);
    if (g < b) {
        const float t = g; g = b; b = t;
        K = -1.f;
    }
    if (a < g) {
        const float t = r; r = g; g = t;
        K = -2.f/6.0f - K;
    }
    chroma = r - ((g < b) ? g: b);
    *out_h = ZR_ABS(K + (g - b)/(6.0f * chroma + 1e-20f));
    *out_s = chroma / (r + 1e-20f);
    *out_v = r;
    *out_a = (float)in.a / 255.0f;
}

void
zr_color_hsva(int *out_h, int *out_s, int *out_v,
                int *out_a, struct zr_color in)
{
    float h,s,v,a;
    zr_color_hsva_f(&h, &s, &v, &a, in);
    *out_h = (zr_byte)(h * 255.0f);
    *out_s = (zr_byte)(s * 255.0f);
    *out_v = (zr_byte)(v * 255.0f);
    *out_a = (zr_byte)(a * 255.0f);
}

void
zr_color_hsv(int *out_h, int *out_s, int *out_v, struct zr_color in)
{
    int a;
    zr_color_hsva(out_h, out_s, out_v, &a, in);
}
/*
 * ==============================================================
 *
 *                          IMAGE
 *
 * ===============================================================
 */
zr_handle
zr_handle_ptr(void *ptr)
{
    zr_handle handle = {0};
    handle.ptr = ptr;
    return handle;
}

zr_handle
zr_handle_id(int id)
{
    zr_handle handle;
    handle.id = id;
    return handle;
}

struct zr_image
zr_subimage_ptr(void *ptr, unsigned short w, unsigned short h, struct zr_rect r)
{
    struct zr_image s;
    zr_zero(&s, sizeof(s));
    s.handle.ptr = ptr;
    s.w = w; s.h = h;
    s.region[0] = (unsigned short)r.x;
    s.region[1] = (unsigned short)r.y;
    s.region[2] = (unsigned short)r.w;
    s.region[3] = (unsigned short)r.h;
    return s;
}

struct zr_image
zr_subimage_id(int id, unsigned short w, unsigned short h, struct zr_rect r)
{
    struct zr_image s;
    zr_zero(&s, sizeof(s));
    s.handle.id = id;
    s.w = w; s.h = h;
    s.region[0] = (unsigned short)r.x;
    s.region[1] = (unsigned short)r.y;
    s.region[2] = (unsigned short)r.w;
    s.region[3] = (unsigned short)r.h;
    return s;
}

struct zr_image
zr_image_ptr(void *ptr)
{
    struct zr_image s;
    zr_zero(&s, sizeof(s));
    ZR_ASSERT(ptr);
    s.handle.ptr = ptr;
    s.w = 0; s.h = 0;
    s.region[0] = 0;
    s.region[1] = 0;
    s.region[2] = 0;
    s.region[3] = 0;
    return s;
}

struct zr_image
zr_image_id(int id)
{
    struct zr_image s;
    zr_zero(&s, sizeof(s));
    s.handle.id = id;
    s.w = 0; s.h = 0;
    s.region[0] = 0;
    s.region[1] = 0;
    s.region[2] = 0;
    s.region[3] = 0;
    return s;
}

int
zr_image_is_subimage(const struct zr_image* img)
{
    ZR_ASSERT(img);
    return !(img->w == 0 && img->h == 0);
}

static void
zr_unify(struct zr_rect *clip, const struct zr_rect *a, float x0, float y0,
    float x1, float y1)
{
    ZR_ASSERT(a);
    ZR_ASSERT(clip);
    clip->x = MAX(a->x, x0);
    clip->y = MAX(a->y, y0);
    clip->w = MIN(a->x + a->w, x1) - clip->x;
    clip->h = MIN(a->y + a->h, y1) - clip->y;
    clip->w = MAX(0, clip->w);
    clip->h = MAX(0, clip->h);
}

static void
zr_triangle_from_direction(struct zr_vec2 *result, struct zr_rect r,
    float pad_x, float pad_y, enum zr_heading direction)
{
    float w_half, h_half;
    ZR_ASSERT(result);

    r.w = MAX(2 * pad_x, r.w);
    r.h = MAX(2 * pad_y, r.h);
    r.w = r.w - 2 * pad_x;
    r.h = r.h - 2 * pad_y;

    r.x = r.x + pad_x;
    r.y = r.y + pad_y;

    w_half = r.w / 2.0f;
    h_half = r.h / 2.0f;

    if (direction == ZR_UP) {
        result[0] = zr_vec2(r.x + w_half, r.y);
        result[1] = zr_vec2(r.x + r.w, r.y + r.h);
        result[2] = zr_vec2(r.x, r.y + r.h);
    } else if (direction == ZR_RIGHT) {
        result[0] = zr_vec2(r.x, r.y);
        result[1] = zr_vec2(r.x + r.w, r.y + h_half);
        result[2] = zr_vec2(r.x, r.y + r.h);
    } else if (direction == ZR_DOWN) {
        result[0] = zr_vec2(r.x, r.y);
        result[1] = zr_vec2(r.x + r.w, r.y);
        result[2] = zr_vec2(r.x + w_half, r.y + r.h);
    } else {
        result[0] = zr_vec2(r.x, r.y + h_half);
        result[1] = zr_vec2(r.x + r.w, r.y);
        result[2] = zr_vec2(r.x + r.w, r.y + r.h);
    }
}

static zr_size
zr_string_float_limit(char *string, int prec)
{
    int dot = 0;
    char *c = string;
    while (*c) {
        if (*c == '.') {
            dot = 1;
            c++;
            continue;
        }
        if (dot == (prec+1)) {
            *c = 0;
            break;
        }
        if (dot > 0) dot++;
        c++;
    }
    return (zr_size)(c - string);
}
/* ==============================================================
 *
 *                          UTF-8
 *
 * ===============================================================*/
static const zr_byte zr_utfbyte[ZR_UTF_SIZE+1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const zr_byte zr_utfmask[ZR_UTF_SIZE+1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const zr_uint zr_utfmin[ZR_UTF_SIZE+1] = {0, 0, 0x80, 0x800, 0x10000};
static const zr_uint zr_utfmax[ZR_UTF_SIZE+1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static zr_size
zr_utf_validate(zr_rune *u, zr_size i)
{
    ZR_ASSERT(u);
    if (!u) return 0;
    if (!ZR_BETWEEN(*u, zr_utfmin[i], zr_utfmax[i]) ||
         ZR_BETWEEN(*u, 0xD800, 0xDFFF))
            *u = ZR_UTF_INVALID;
    for (i = 1; *u > zr_utfmax[i]; ++i);
    return i;
}

static zr_rune
zr_utf_decode_byte(char c, zr_size *i)
{
    ZR_ASSERT(i);
    if (!i) return 0;
    for(*i = 0; *i < ZR_LEN(zr_utfmask); ++(*i)) {
        if (((zr_byte)c & zr_utfmask[*i]) == zr_utfbyte[*i])
            return (zr_byte)(c & ~zr_utfmask[*i]);
    }
    return 0;
}

zr_size
zr_utf_decode(const char *c, zr_rune *u, zr_size clen)
{
    zr_size i, j, len, type=0;
    zr_rune udecoded;

    ZR_ASSERT(c);
    ZR_ASSERT(u);

    if (!c || !u) return 0;
    if (!clen) return 0;
    *u = ZR_UTF_INVALID;

    udecoded = zr_utf_decode_byte(c[0], &len);
    if (!ZR_BETWEEN(len, 1, ZR_UTF_SIZE))
        return 1;

    for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
        udecoded = (udecoded << 6) | zr_utf_decode_byte(c[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;
    *u = udecoded;
    zr_utf_validate(u, len);
    return len;
}

static char
zr_utf_encode_byte(zr_rune u, zr_size i)
{
    return (char)((zr_utfbyte[i]) | ((zr_byte)u & ~zr_utfmask[i]));
}

zr_size
zr_utf_encode(zr_rune u, char *c, zr_size clen)
{
    zr_size len, i;
    len = zr_utf_validate(&u, 0);
    if (clen < len || !len || len > ZR_UTF_SIZE)
        return 0;

    for (i = len - 1; i != 0; --i) {
        c[i] = zr_utf_encode_byte(u, 0);
        u >>= 6;
    }
    c[0] = zr_utf_encode_byte(u, len);
    return len;
}

zr_size
zr_utf_len(const char *str, zr_size len)
{
    const char *text;
    zr_size glyphs = 0;
    zr_size text_len;
    zr_size glyph_len;
    zr_size src_len = 0;
    zr_rune unicode;

    ZR_ASSERT(str);
    if (!str || !len) return 0;

    text = str;
    text_len = len;
    glyph_len = zr_utf_decode(text, &unicode, text_len);
    while (glyph_len && src_len < len) {
        glyphs++;
        src_len = src_len + glyph_len;
        glyph_len = zr_utf_decode(text + src_len, &unicode, text_len - src_len);
    }
    return glyphs;
}

static const char*
zr_utf_at(const char *buffer, zr_size length, int index,
    zr_rune *unicode, zr_size *len)
{
    int i = 0;
    zr_size src_len = 0;
    zr_size glyph_len = 0;
    const char *text;
    zr_size text_len;

    ZR_ASSERT(buffer);
    ZR_ASSERT(unicode);
    ZR_ASSERT(len);

    if (!buffer || !unicode || !len) return 0;
    if (index < 0) {
        *unicode = ZR_UTF_INVALID;
        *len = 0;
        return 0;
    }

    text = buffer;
    text_len = length;
    glyph_len = zr_utf_decode(text, unicode, text_len);
    while (glyph_len) {
        if (i == index) {
            *len = glyph_len;
            break;
        }

        i++;
        src_len = src_len + glyph_len;
        glyph_len = zr_utf_decode(text + src_len, unicode, text_len - src_len);
    }
    if (i != index) return 0;
    return buffer + src_len;
}

/*
 * ==============================================================
 *
 *                          USER FONT
 *
 * ===============================================================
 */
static zr_size
zr_user_font_glyph_index_at_pos(const struct zr_user_font *font, const char *text,
    zr_size text_len, float xoff)
{
    zr_rune unicode;
    zr_size glyph_offset = 0;
    zr_size glyph_len = zr_utf_decode(text, &unicode, text_len);
    zr_size text_width = font->width(font->userdata, font->height, text, glyph_len);
    zr_size src_len = glyph_len;

    while (text_len && glyph_len) {
        if (text_width >= xoff)
            return glyph_offset;

        glyph_offset++;
        text_len -= glyph_len;
        glyph_len = zr_utf_decode(text + src_len, &unicode, text_len);
        src_len += glyph_len;
        text_width = font->width(font->userdata, font->height, text, src_len);
    }
    return glyph_offset;
}

static zr_size
zr_use_font_glyph_clamp(const struct zr_user_font *font, const char *text,
    zr_size text_len, float space, zr_size *glyphs, float *text_width)
{
    zr_size glyph_len = 0;
    float last_width = 0;
    zr_rune unicode = 0;
    float width = 0;
    zr_size len = 0;
    zr_size g = 0;

    glyph_len = zr_utf_decode(text, &unicode, text_len);
    while (glyph_len && (width < space) && (len < text_len)) {
        zr_size s;
        len += glyph_len;
        s = font->width(font->userdata, font->height, text, len);

        last_width = width;
        width = (float)s;
        glyph_len = zr_utf_decode(&text[len], &unicode, text_len - len);
        g++;
    }

    *glyphs = g;
    *text_width = last_width;
    return len;
}

static zr_size
zr_user_font_glyphs_fitting_in_space(const struct zr_user_font *font,
    const char *text, zr_size len, float space, zr_size *row_len,
    zr_size *glyphs, float *text_width, int has_newline)
{
    zr_size glyph_len;
    zr_size width = 0;
    zr_rune unicode = 0;
    zr_size text_len = 0;
    zr_size row_advance = 0;

    ZR_ASSERT(glyphs);
    ZR_ASSERT(text_width);
    ZR_ASSERT(row_len);

    *glyphs = 0;
    *row_len = 0;
    *text_width = 0;

    glyph_len = text_len = zr_utf_decode(text, &unicode, len);
    if (!glyph_len) return 0;
    width = font->width(font->userdata, font->height, text, text_len);
    while ((width <= space) && (text_len <= len) && glyph_len) {
        *text_width = width;
        *glyphs+=1;
        *row_len = text_len;
        row_advance += glyph_len;

        if (has_newline && (unicode == '\n' || unicode == '\r')) {
            zr_rune next = 0;
            zr_utf_decode(text+text_len, &next, len - text_len);
            if (unicode == '\r') {
                *row_len-=1;;
            } else if ((unicode == '\n') && (next == '\r')) {
                *row_len-= 2;
            } else {
                *row_len-=1;
            }
            *text_width = font->width(font->userdata, font->height, text, *row_len);
            break;
        }
        glyph_len = zr_utf_decode(text + text_len, &unicode, len - text_len);
        text_len += glyph_len;
        width = font->width(font->userdata, font->height, text, text_len);
    }
    return row_advance;
}

/* ==============================================================
 *
 *                          BUFFER
 *
 * ===============================================================*/
void
zr_buffer_init(struct zr_buffer *b, const struct zr_allocator *a,
    zr_size initial_size)
{
    ZR_ASSERT(b);
    ZR_ASSERT(a);
    ZR_ASSERT(initial_size);
    if (!b || !a || !initial_size) return;

    zr_zero(b, sizeof(*b));
    b->type = ZR_BUFFER_DYNAMIC;
    b->memory.ptr = a->alloc(a->userdata, initial_size);
    b->memory.size = initial_size;
    b->size = initial_size;
    b->grow_factor = 2.0f;
    b->pool = *a;
}

void
zr_buffer_init_fixed(struct zr_buffer *b, void *m, zr_size size)
{
    ZR_ASSERT(b);
    ZR_ASSERT(m);
    ZR_ASSERT(size);
    if (!b || !m || !size) return;

    zr_zero(b, sizeof(*b));
    b->type = ZR_BUFFER_FIXED;
    b->memory.ptr = m;
    b->memory.size = size;
    b->size = size;
}

static void*
zr_buffer_align(void *unaligned, zr_size align, zr_size *alignment,
    enum zr_buffer_allocation_type type)
{
    void *memory = 0;
    if (type == ZR_BUFFER_FRONT) {
        if (align) {
            memory = ZR_ALIGN_PTR(unaligned, align);
            *alignment = (zr_size)((zr_byte*)memory - (zr_byte*)unaligned);
        } else {
            memory = unaligned;
            *alignment = 0;
        }
    } else {
        if (align) {
            memory = ZR_ALIGN_PTR_BACK(unaligned, align);
            *alignment = (zr_size)((zr_byte*)unaligned - (zr_byte*)memory);
        } else {
            memory = unaligned;
            *alignment = 0;
        }
    }
    return memory;
}

static void*
zr_buffer_realloc(struct zr_buffer *b, zr_size capacity, zr_size *size)
{
    void *temp;
    zr_size buffer_size;

    ZR_ASSERT(b);
    ZR_ASSERT(size);
    if (!b || !size || !b->pool.alloc || !b->pool.free)
        return 0;

    buffer_size = b->memory.size;
    temp = b->pool.alloc(b->pool.userdata, capacity);
    ZR_ASSERT(temp);
    if (!temp) return 0;
    zr_memcopy(temp, b->memory.ptr, buffer_size);
    b->pool.free(b->pool.userdata, b->memory.ptr);
    *size = capacity;

    if (b->size == buffer_size) {
        /* no back buffer so just set correct size */
        b->size = capacity;
        return temp;
    } else {
        /* copy back buffer to the end of the new buffer */
        void *dst, *src;
        zr_size back_size;
        back_size = buffer_size - b->size;
        dst = zr_ptr_add(void, temp, capacity - back_size);
        src = zr_ptr_add(void, temp, b->size);
        zr_memcopy(dst, src, back_size);
        b->size = capacity - back_size;
    }
    return temp;
}

static void*
zr_buffer_alloc(struct zr_buffer *b, enum zr_buffer_allocation_type type,
    zr_size size, zr_size align)
{
    int full;
    zr_size cap;
    zr_size alignment;
    void *unaligned;
    void *memory;

    ZR_ASSERT(b);
    ZR_ASSERT(size);
    if (!b || !size) return 0;
    b->needed += size;

    /* calculate total size with needed alignment + size */
    if (type == ZR_BUFFER_FRONT)
        unaligned = zr_ptr_add(void, b->memory.ptr, b->allocated);
    else unaligned = zr_ptr_add(void, b->memory.ptr, b->size - size);
    memory = zr_buffer_align(unaligned, align, &alignment, type);

    /* check if buffer has enough memory*/
    if (type == ZR_BUFFER_FRONT)
        full = ((b->allocated + size + alignment) > b->size);
    else full = ((b->size - (size + alignment)) <= b->allocated);

    if (full) {
        /* buffer is full so allocate bigger buffer if dynamic */
        ZR_ASSERT(b->type == ZR_BUFFER_DYNAMIC);
        ZR_ASSERT(b->pool.alloc && b->pool.free);
        if (b->type != ZR_BUFFER_DYNAMIC || !b->pool.alloc || !b->pool.free)
            return 0;

        cap = (zr_size)((float)b->memory.size * b->grow_factor);
        cap = MAX(cap, zr_round_up_pow2((zr_uint)(b->allocated + size)));
        b->memory.ptr = zr_buffer_realloc(b, cap, &b->memory.size);
        if (!b->memory.ptr) return 0;

        /* align newly allocated pointer */
        if (type == ZR_BUFFER_FRONT)
            unaligned = zr_ptr_add(void, b->memory.ptr, b->allocated);
        else unaligned = zr_ptr_add(void, b->memory.ptr, b->size);
        memory = zr_buffer_align(unaligned, align, &alignment, type);
    }

    if (type == ZR_BUFFER_FRONT)
        b->allocated += size + alignment;
    else b->size -= (size + alignment);
    b->needed += alignment;
    b->calls++;
    return memory;
}

static void
zr_buffer_mark(struct zr_buffer *buffer, enum zr_buffer_allocation_type type)
{
    ZR_ASSERT(buffer);
    if (!buffer) return;
    buffer->marker[type].active = zr_true;
    if (type == ZR_BUFFER_BACK)
        buffer->marker[type].offset = buffer->size;
    else buffer->marker[type].offset = buffer->allocated;
}

static void
zr_buffer_reset(struct zr_buffer *buffer, enum zr_buffer_allocation_type type)
{
    ZR_ASSERT(buffer);
    if (!buffer) return;
    if (type == ZR_BUFFER_BACK) {
        /* reset back buffer either back to marker or empty */
        buffer->needed -= (buffer->memory.size - buffer->marker[type].offset);
        if (buffer->marker[type].active)
            buffer->size = buffer->marker[type].offset;
        else buffer->size = buffer->memory.size;
        buffer->marker[type].active = zr_false;
    } else {
        /* reset front buffer either back to back marker or empty */
        buffer->needed -= (buffer->allocated - buffer->marker[type].offset);
        if (buffer->marker[type].active)
            buffer->allocated = buffer->marker[type].offset;
        else buffer->allocated = 0;
        buffer->marker[type].active = zr_false;
    }
}

static void
zr_buffer_clear(struct zr_buffer *b)
{
    ZR_ASSERT(b);
    if (!b) return;
    b->allocated = 0;
    b->size = b->memory.size;
    b->calls = 0;
    b->needed = 0;
}

void
zr_buffer_free(struct zr_buffer *b)
{
    ZR_ASSERT(b);
    if (!b || !b->memory.ptr) return;
    if (b->type == ZR_BUFFER_FIXED) return;
    if (!b->pool.free) return;
    ZR_ASSERT(b->pool.free);
    b->pool.free(b->pool.userdata, b->memory.ptr);
}

void
zr_buffer_info(struct zr_memory_status *s, struct zr_buffer *b)
{
    ZR_ASSERT(b);
    ZR_ASSERT(s);
    if (!s || !b) return;
    s->allocated = b->allocated;
    s->size =  b->memory.size;
    s->needed = b->needed;
    s->memory = b->memory.ptr;
    s->calls = b->calls;
}

void*
zr_buffer_memory(struct zr_buffer *buffer)
{
    ZR_ASSERT(buffer);
    if (!buffer) return 0;
    return buffer->memory.ptr;
}

zr_size
zr_buffer_total(struct zr_buffer *buffer)
{
    ZR_ASSERT(buffer);
    if (!buffer) return 0;
    return buffer->memory.size;
}

/*
 * ==============================================================
 *
 *                      Command buffer
 *
 * ===============================================================
*/
static void
zr_command_buffer_init(struct zr_command_buffer *cmdbuf,
    struct zr_buffer *buffer, enum zr_command_clipping clip)
{
    ZR_ASSERT(cmdbuf);
    ZR_ASSERT(buffer);
    if (!cmdbuf || !buffer) return;
    cmdbuf->base = buffer;
    cmdbuf->use_clipping = clip;
    cmdbuf->begin = buffer->allocated;
    cmdbuf->end = buffer->allocated;
    cmdbuf->last = buffer->allocated;
}

static void
zr_command_buffer_reset(struct zr_command_buffer *buffer)
{
    ZR_ASSERT(buffer);
    if (!buffer) return;
    buffer->begin = 0;
    buffer->end = 0;
    buffer->last = 0;
    buffer->clip = zr_null_rect;
#if ZR_COMPILE_WITH_COMMAND_USERDATA
    buffer->userdata.ptr = 0;
#endif
}

static void*
zr_command_buffer_push(struct zr_command_buffer* b,
    enum zr_command_type t, zr_size size)
{
    static const zr_size align = ZR_ALIGNOF(struct zr_command);
    struct zr_command *cmd;
    zr_size alignment;
    void *unaligned;
    void *memory;

    ZR_ASSERT(b);
    ZR_ASSERT(b->base);
    if (!b) return 0;

    cmd = (struct zr_command*)zr_buffer_alloc(b->base,ZR_BUFFER_FRONT,size,align);
    if (!cmd) return 0;

    /* make sure the offset to the next command is aligned */
    b->last = (zr_size)((zr_byte*)cmd - (zr_byte*)b->base->memory.ptr);
    unaligned = (zr_byte*)cmd + size;
    memory = ZR_ALIGN_PTR(unaligned, align);
    alignment = (zr_size)((zr_byte*)memory - (zr_byte*)unaligned);

    cmd->type = t;
    cmd->next = b->base->allocated + alignment;
#if ZR_COMPILE_WITH_COMMAND_USERDATA
    cmd->userdata = b->userdata;
#endif
    b->end = cmd->next;
    return cmd;
}

void
zr_draw_scissor(struct zr_command_buffer *b, struct zr_rect r)
{
    struct zr_command_scissor *cmd;
    ZR_ASSERT(b);
    if (!b) return;

    b->clip.x = r.x;
    b->clip.y = r.y;
    b->clip.w = r.w;
    b->clip.h = r.h;
    cmd = (struct zr_command_scissor*)
        zr_command_buffer_push(b, ZR_COMMAND_SCISSOR, sizeof(*cmd));

    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)MAX(0, r.w);
    cmd->h = (unsigned short)MAX(0, r.h);
}

void
zr_draw_line(struct zr_command_buffer *b, float x0, float y0,
    float x1, float y1, struct zr_color c)
{
    struct zr_command_line *cmd;
    ZR_ASSERT(b);
    if (!b) return;
    cmd = (struct zr_command_line*)
        zr_command_buffer_push(b, ZR_COMMAND_LINE, sizeof(*cmd));
    if (!cmd) return;
    cmd->begin.x = (short)x0;
    cmd->begin.y = (short)y0;
    cmd->end.x = (short)x1;
    cmd->end.y = (short)y1;
    cmd->color = c;
}

void
zr_draw_curve(struct zr_command_buffer *b, float ax, float ay,
    float ctrl0x, float ctrl0y, float ctrl1x, float ctrl1y,
    float bx, float by, struct zr_color col)
{
    struct zr_command_curve *cmd;
    ZR_ASSERT(b);
    if (!b) return;

    cmd = (struct zr_command_curve*)
        zr_command_buffer_push(b, ZR_COMMAND_CURVE, sizeof(*cmd));
    if (!cmd) return;
    cmd->begin.x = (short)ax;
    cmd->begin.y = (short)ay;
    cmd->ctrl[0].x = (short)ctrl0x;
    cmd->ctrl[0].y = (short)ctrl0y;
    cmd->ctrl[1].x = (short)ctrl1x;
    cmd->ctrl[1].y = (short)ctrl1y;
    cmd->end.x = (short)bx;
    cmd->end.y = (short)by;
    cmd->color = col;
}

void
zr_draw_rect(struct zr_command_buffer *b, struct zr_rect rect,
    float rounding, struct zr_color c)
{
    struct zr_command_rect *cmd;
    ZR_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct zr_rect *clip = &b->clip;
        if (!ZR_INTERSECT(rect.x, rect.y, rect.w, rect.h,
            clip->x, clip->y, clip->w, clip->h)) return;
    }

    cmd = (struct zr_command_rect*)
        zr_command_buffer_push(b, ZR_COMMAND_RECT, sizeof(*cmd));
    if (!cmd) return;
    cmd->rounding = (unsigned int)rounding;
    cmd->x = (short)rect.x;
    cmd->y = (short)rect.y;
    cmd->w = (unsigned short)MAX(0, rect.w);
    cmd->h = (unsigned short)MAX(0, rect.h);
    cmd->color = c;
}

void
zr_draw_circle(struct zr_command_buffer *b, struct zr_rect r,
    struct zr_color c)
{
    struct zr_command_circle *cmd;
    ZR_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct zr_rect *clip = &b->clip;
        if (!ZR_INTERSECT(r.x, r.y, r.w, r.h, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct zr_command_circle*)
        zr_command_buffer_push(b, ZR_COMMAND_CIRCLE, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)MAX(r.w, 0);
    cmd->h = (unsigned short)MAX(r.h, 0);
    cmd->color = c;
}

void
zr_draw_arc(struct zr_command_buffer *b, float cx,
    float cy, float radius, float a_min, float a_max, struct zr_color c)
{
    struct zr_command_arc *cmd;
    cmd = (struct zr_command_arc*)
        zr_command_buffer_push(b, ZR_COMMAND_ARC, sizeof(*cmd));
    if (!cmd) return;
    cmd->cx = (short)cx;
    cmd->cy = (short)cy;
    cmd->r = (unsigned short)radius;
    cmd->a[0] = a_min;
    cmd->a[1] = a_max;
    cmd->color = c;
}

void
zr_draw_triangle(struct zr_command_buffer *b,float x0,float y0,
    float x1, float y1, float x2, float y2, struct zr_color c)
{
    struct zr_command_triangle *cmd;
    ZR_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct zr_rect *clip = &b->clip;
        if (!ZR_INBOX(x0, y0, clip->x, clip->y, clip->w, clip->h) ||
            !ZR_INBOX(x1, y1, clip->x, clip->y, clip->w, clip->h) ||
            !ZR_INBOX(x2, y2, clip->x, clip->y, clip->w, clip->h))
            return;
    }

    cmd = (struct zr_command_triangle*)
        zr_command_buffer_push(b, ZR_COMMAND_TRIANGLE, sizeof(*cmd));
    if (!cmd) return;
    cmd->a.x = (short)x0;
    cmd->a.y = (short)y0;
    cmd->b.x = (short)x1;
    cmd->b.y = (short)y1;
    cmd->c.x = (short)x2;
    cmd->c.y = (short)y2;
    cmd->color = c;
}

void
zr_draw_image(struct zr_command_buffer *b, struct zr_rect r,
    struct zr_image *img)
{
    struct zr_command_image *cmd;
    ZR_ASSERT(b);
    if (!b) return;
    if (b->use_clipping) {
        const struct zr_rect *c = &b->clip;
        if (!ZR_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    cmd = (struct zr_command_image*)
        zr_command_buffer_push(b, ZR_COMMAND_IMAGE, sizeof(*cmd));
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)MAX(0, r.w);
    cmd->h = (unsigned short)MAX(0, r.h);
    cmd->img = *img;
}

void
zr_draw_text(struct zr_command_buffer *b, struct zr_rect r,
    const char *string, zr_size length, const struct zr_user_font *font,
    struct zr_color bg, struct zr_color fg)
{
    zr_size text_width = 0;
    struct zr_command_text *cmd;
    ZR_ASSERT(b);
    ZR_ASSERT(font);
    if (!b || !string || !length) return;
    if (b->use_clipping) {
        const struct zr_rect *c = &b->clip;
        if (!ZR_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h))
            return;
    }

    /* make sure text fits inside bounds */
    text_width = font->width(font->userdata, font->height, string, length);
    if (text_width > r.w){
        float txt_width = (float)text_width;
        zr_size glyphs = 0;
        length = zr_use_font_glyph_clamp(font, string, length,
                    r.w, &glyphs, &txt_width);
    }

    if (!length) return;
    cmd = (struct zr_command_text*)
        zr_command_buffer_push(b, ZR_COMMAND_TEXT, sizeof(*cmd) + length + 1);
    if (!cmd) return;
    cmd->x = (short)r.x;
    cmd->y = (short)r.y;
    cmd->w = (unsigned short)r.w;
    cmd->h = (unsigned short)r.h;
    cmd->background = bg;
    cmd->foreground = fg;
    cmd->font = font;
    cmd->length = length;
    cmd->height = font->height;
    zr_memcopy(cmd->string, string, length);
    cmd->string[length] = '\0';
}

/* ==============================================================
 *
 *                          CANVAS
 *
 * ===============================================================*/
#if ZR_COMPILE_WITH_VERTEX_BUFFER
static void
zr_canvas_init(struct zr_canvas *list)
{
    zr_size i = 0;
    zr_zero(list, sizeof(*list));
    for (i = 0; i < ZR_LEN(list->circle_vtx); ++i) {
        const float a = ((float)i / (float)ZR_LEN(list->circle_vtx)) * 2 * ZR_PI;
        list->circle_vtx[i].x = (float)zr_cos(a);
        list->circle_vtx[i].y = (float)zr_sin(a);
    }
}

static void
zr_canvas_setup(struct zr_canvas *list, float global_alpha, struct zr_buffer *cmds,
    struct zr_buffer *vertices, struct zr_buffer *elements,
    struct zr_draw_null_texture null,
    enum zr_anti_aliasing line_AA, enum zr_anti_aliasing shape_AA)
{
    list->null = null;
    list->clip_rect = zr_null_rect;
    list->vertices = vertices;
    list->elements = elements;
    list->buffer = cmds;
    list->line_AA = line_AA;
    list->shape_AA = shape_AA;
    list->global_alpha = global_alpha;
}

static void
zr_canvas_clear(struct zr_canvas *list)
{
    ZR_ASSERT(list);
    if (!list) return;
    if (list->buffer)
        zr_buffer_clear(list->buffer);
    if (list->elements)
        zr_buffer_clear(list->vertices);
    if (list->vertices)
        zr_buffer_clear(list->elements);

    list->element_count = 0;
    list->vertex_count = 0;
    list->cmd_offset = 0;
    list->cmd_count = 0;
    list->path_count = 0;
    list->vertices = 0;
    list->elements = 0;
    list->clip_rect = zr_null_rect;
}

static struct zr_vec2*
zr_canvas_alloc_path(struct zr_canvas *list, zr_size count)
{
    struct zr_vec2 *points;
    static const zr_size point_align = ZR_ALIGNOF(struct zr_vec2);
    static const zr_size point_size = sizeof(struct zr_vec2);
    points = (struct zr_vec2*)
        zr_buffer_alloc(list->buffer, ZR_BUFFER_FRONT,
                        point_size * count, point_align);

    if (!points) return 0;
    if (!list->path_offset) {
        void *memory = zr_buffer_memory(list->buffer);
        list->path_offset = (unsigned int)((zr_byte*)points - (zr_byte*)memory);
    }
    list->path_count += (unsigned int)count;
    return points;
}

static struct zr_vec2
zr_canvas_path_last(struct zr_canvas *list)
{
    void *memory;
    struct zr_vec2 *point;
    ZR_ASSERT(list->path_count);
    memory = zr_buffer_memory(list->buffer);
    point = zr_ptr_add(struct zr_vec2, memory, list->path_offset);
    point += (list->path_count-1);
    return *point;
}

static struct zr_draw_command*
zr_canvas_push_command(struct zr_canvas *list, struct zr_rect clip,
    zr_handle texture)
{
    static const zr_size cmd_align = ZR_ALIGNOF(struct zr_draw_command);
    static const zr_size cmd_size = sizeof(struct zr_draw_command);
    struct zr_draw_command *cmd;

    ZR_ASSERT(list);
    cmd = (struct zr_draw_command*)
        zr_buffer_alloc(list->buffer, ZR_BUFFER_BACK, cmd_size, cmd_align);

    if (!cmd) return 0;
    if (!list->cmd_count) {
        zr_byte *memory = (zr_byte*)zr_buffer_memory(list->buffer);
        zr_size total = zr_buffer_total(list->buffer);
        memory = zr_ptr_add(zr_byte, memory, total);
        list->cmd_offset = (zr_size)(memory - (zr_byte*)cmd);
    }

    cmd->elem_count = 0;
    cmd->clip_rect = clip;
    cmd->texture = texture;

    list->cmd_count++;
    list->clip_rect = clip;
    return cmd;
}

static struct zr_draw_command*
zr_canvas_command_last(struct zr_canvas *list)
{
    void *memory;
    zr_size size;
    struct zr_draw_command *cmd;
    ZR_ASSERT(list->cmd_count);

    memory = zr_buffer_memory(list->buffer);
    size = zr_buffer_total(list->buffer);
    cmd = zr_ptr_add(struct zr_draw_command, memory, size - list->cmd_offset);
    return (cmd - (list->cmd_count-1));
}

static void
zr_canvas_add_clip(struct zr_canvas *list, struct zr_rect rect)
{
    ZR_ASSERT(list);
    if (!list) return;
    if (!list->cmd_count) {
        zr_canvas_push_command(list, rect, list->null.texture);
    } else {
        struct zr_draw_command *prev = zr_canvas_command_last(list);
        zr_canvas_push_command(list, rect, prev->texture);
    }
}

static void
zr_canvas_push_image(struct zr_canvas *list, zr_handle texture)
{
    ZR_ASSERT(list);
    if (!list) return;
    if (!list->cmd_count) {
        zr_canvas_push_command(list, zr_null_rect, list->null.texture);
    } else {
        struct zr_draw_command *prev = zr_canvas_command_last(list);
        if (prev->texture.id != texture.id)
            zr_canvas_push_command(list, prev->clip_rect, texture);
    }
}

#if ZR_COMPILE_WITH_COMMAND_USERDATA
static void
zr_canvas_push_userdata(struct zr_canvas *list, zr_handle userdata)
{
    ZR_ASSERT(list);
    if (!list) return;
    if (!list->cmd_count) {
        struct zr_draw_command *prev;
        zr_canvas_push_command(list, zr_null_rect, list->null.texture);
        prev = zr_canvas_command_last(list);
        prev->userdata = userdata;
    } else {
        struct zr_draw_command *prev = zr_canvas_command_last(list);
        if (prev->userdata.ptr != userdata.ptr) {
            zr_canvas_push_command(list, prev->clip_rect, prev->texture);
            prev = zr_canvas_command_last(list);
            prev->userdata = userdata;
        }
    }
}
#endif

static struct zr_draw_vertex*
zr_canvas_alloc_vertices(struct zr_canvas *list, zr_size count)
{
    struct zr_draw_vertex *vtx;
    static const zr_size vtx_align = ZR_ALIGNOF(struct zr_draw_vertex);
    static const zr_size vtx_size = sizeof(struct zr_draw_vertex);
    ZR_ASSERT(list);
    if (!list) return 0;

    vtx = (struct zr_draw_vertex*)
        zr_buffer_alloc(list->vertices, ZR_BUFFER_FRONT, vtx_size*count, vtx_align);
    if (!vtx) return 0;
    list->vertex_count += (unsigned int)count;
    return vtx;
}

static zr_draw_index*
zr_canvas_alloc_elements(struct zr_canvas *list, zr_size count)
{
    zr_draw_index *ids;
    struct zr_draw_command *cmd;
    static const zr_size elem_align = ZR_ALIGNOF(zr_draw_index);
    static const zr_size elem_size = sizeof(zr_draw_index);
    ZR_ASSERT(list);
    if (!list) return 0;

    ids = (zr_draw_index*)
        zr_buffer_alloc(list->elements, ZR_BUFFER_FRONT, elem_size*count, elem_align);
    if (!ids) return 0;
    cmd = zr_canvas_command_last(list);
    list->element_count += (unsigned int)count;
    cmd->elem_count += (unsigned int)count;
    return ids;
}

static struct zr_draw_vertex
zr_draw_vertex(struct zr_vec2 pos, struct zr_vec2 uv, zr_draw_vertex_color col)
{
    struct zr_draw_vertex out;
    out.position = pos;
    out.uv = uv;
    out.col = col;
    return out;
}

static void
zr_canvas_add_poly_line(struct zr_canvas *list, struct zr_vec2 *points,
    const unsigned int points_count, struct zr_color color, int closed,
    float thickness, enum zr_anti_aliasing aliasing)
{
    zr_size count;
    int thick_line;
    zr_draw_vertex_color col;
    ZR_ASSERT(list);
    if (!list || points_count < 2) return;

    color.a *= list->global_alpha;
    col = zr_color32(color);
    count = points_count;
    if (!closed) count = points_count-1;
    thick_line = thickness > 1.0f;

#if ZR_COMPILE_WITH_COMMAND_USERDATA
    zr_canvas_push_userdata(list, list->userdata);
#endif

    if (aliasing == ZR_ANTI_ALIASING_ON) {
        /* ANTI-ALIASED STROKE */
        const float AA_SIZE = 1.0f;
        static const zr_size pnt_align = ZR_ALIGNOF(struct zr_vec2);
        static const zr_size pnt_size = sizeof(struct zr_vec2);
        const zr_draw_vertex_color col_trans = col & 0x00ffffff;

        /* allocate vertices and elements  */
        zr_size i1 = 0;
        zr_size index = list->vertex_count;
        const zr_size idx_count = (thick_line) ?  (count * 18) : (count * 12);
        const zr_size vtx_count = (thick_line) ? (points_count * 4): (points_count *3);
        struct zr_draw_vertex *vtx = zr_canvas_alloc_vertices(list, vtx_count);
        zr_draw_index *ids = zr_canvas_alloc_elements(list, idx_count);

        zr_size size;
        struct zr_vec2 *normals, *temp;
        if (!vtx || !ids) return;

        /* temporary allocate normals + points */
        zr_buffer_mark(list->vertices, ZR_BUFFER_FRONT);
        size = pnt_size * ((thick_line) ? 5 : 3) * points_count;
        normals = (struct zr_vec2*)
            zr_buffer_alloc(list->vertices, ZR_BUFFER_FRONT, size, pnt_align);
        if (!normals) return;
        temp = normals + points_count;

        /* calculate normals */
        for (i1 = 0; i1 < count; ++i1) {
            const zr_size i2 = ((i1 + 1) == points_count) ? 0 : (i1 + 1);
            struct zr_vec2 diff = zr_vec2_sub(points[i2], points[i1]);
            float len;

            /* vec2 inverted lenth  */
            len = zr_vec2_len_sqr(diff);
            if (len != 0.0f)
                len = zr_inv_sqrt(len);
            else len = 1.0f;

            diff = zr_vec2_muls(diff, len);
            normals[i1].x = diff.y;
            normals[i1].y = -diff.x;
        }

        if (!closed)
            normals[points_count-1] = normals[points_count-2];

        if (!thick_line) {
            zr_size idx1, i;
            if (!closed) {
                struct zr_vec2 d;
                temp[0] = zr_vec2_add(points[0], zr_vec2_muls(normals[0], AA_SIZE));
                temp[1] = zr_vec2_sub(points[0], zr_vec2_muls(normals[0], AA_SIZE));
                d = zr_vec2_muls(normals[points_count-1], AA_SIZE);
                temp[(points_count-1) * 2 + 0] = zr_vec2_add(points[points_count-1], d);
                temp[(points_count-1) * 2 + 1] = zr_vec2_sub(points[points_count-1], d);
            }

            /* fill elements */
            idx1 = index;
            for (i1 = 0; i1 < count; i1++) {
                struct zr_vec2 dm;
                float dmr2;
                zr_size i2 = ((i1 + 1) == points_count) ? 0 : (i1 + 1);
                zr_size idx2 = ((i1+1) == points_count) ? index: (idx1 + 3);

                /* average normals */
                dm = zr_vec2_muls(zr_vec2_add(normals[i1], normals[i2]), 0.5f);
                dmr2 = dm.x * dm.x + dm.y* dm.y;
                if (dmr2 > 0.000001f) {
                    float scale = 1.0f/dmr2;
                    scale = MIN(100.0f, scale);
                    dm = zr_vec2_muls(dm, scale);
                }

                dm = zr_vec2_muls(dm, AA_SIZE);
                temp[i2*2+0] = zr_vec2_add(points[i2], dm);
                temp[i2*2+1] = zr_vec2_sub(points[i2], dm);

                ids[0] = (zr_draw_index)(idx2 + 0); ids[1] = (zr_draw_index)(idx1+0);
                ids[2] = (zr_draw_index)(idx1 + 2); ids[3] = (zr_draw_index)(idx1+2);
                ids[4] = (zr_draw_index)(idx2 + 2); ids[5] = (zr_draw_index)(idx2+0);
                ids[6] = (zr_draw_index)(idx2 + 1); ids[7] = (zr_draw_index)(idx1+1);
                ids[8] = (zr_draw_index)(idx1 + 0); ids[9] = (zr_draw_index)(idx1+0);
                ids[10]= (zr_draw_index)(idx2 + 0); ids[11]= (zr_draw_index)(idx2+1);
                ids += 12;
                idx1 = idx2;
            }

            /* fill vertices */
            for (i = 0; i < points_count; ++i) {
                const struct zr_vec2 uv = list->null.uv;
                vtx[0] = zr_draw_vertex(points[i], uv, col);
                vtx[1] = zr_draw_vertex(temp[i*2+0], uv, col_trans);
                vtx[2] = zr_draw_vertex(temp[i*2+1], uv, col_trans);
                vtx += 3;
            }
        } else {
            zr_size idx1, i;
            const float half_inner_thickness = (thickness - AA_SIZE) * 0.5f;
            if (!closed) {
                struct zr_vec2 d1 = zr_vec2_muls(normals[0], half_inner_thickness + AA_SIZE);
                struct zr_vec2 d2 = zr_vec2_muls(normals[0], half_inner_thickness);

                temp[0] = zr_vec2_add(points[0], d1);
                temp[1] = zr_vec2_add(points[0], d2);
                temp[2] = zr_vec2_sub(points[0], d2);
                temp[3] = zr_vec2_sub(points[0], d1);

                d1 = zr_vec2_muls(normals[points_count-1], half_inner_thickness + AA_SIZE);
                d2 = zr_vec2_muls(normals[points_count-1], half_inner_thickness);

                temp[(points_count-1)*4+0] = zr_vec2_add(points[points_count-1], d1);
                temp[(points_count-1)*4+1] = zr_vec2_add(points[points_count-1], d2);
                temp[(points_count-1)*4+2] = zr_vec2_sub(points[points_count-1], d2);
                temp[(points_count-1)*4+3] = zr_vec2_sub(points[points_count-1], d1);
            }

            /* add all elements */
            idx1 = index;
            for (i1 = 0; i1 < count; ++i1) {
                struct zr_vec2 dm_out, dm_in;
                const zr_size i2 = ((i1+1) == points_count) ? 0: (i1 + 1);
                zr_size idx2 = ((i1+1) == points_count) ? index: (idx1 + 4);

                /* average normals */
                struct zr_vec2 dm = zr_vec2_muls(zr_vec2_add(normals[i1], normals[i2]), 0.5f);
                float dmr2 = dm.x * dm.x + dm.y* dm.y;
                if (dmr2 > 0.000001f) {
                    float scale = 1.0f/dmr2;
                    scale = MIN(100.0f, scale);
                    dm = zr_vec2_muls(dm, scale);
                }

                dm_out = zr_vec2_muls(dm, ((half_inner_thickness) + AA_SIZE));
                dm_in = zr_vec2_muls(dm, half_inner_thickness);
                temp[i2*4+0] = zr_vec2_add(points[i2], dm_out);
                temp[i2*4+1] = zr_vec2_add(points[i2], dm_in);
                temp[i2*4+2] = zr_vec2_sub(points[i2], dm_in);
                temp[i2*4+3] = zr_vec2_sub(points[i2], dm_out);

                /* add indexes */
                ids[0] = (zr_draw_index)(idx2 + 1); ids[1] = (zr_draw_index)(idx1+1);
                ids[2] = (zr_draw_index)(idx1 + 2); ids[3] = (zr_draw_index)(idx1+2);
                ids[4] = (zr_draw_index)(idx2 + 2); ids[5] = (zr_draw_index)(idx2+1);
                ids[6] = (zr_draw_index)(idx2 + 1); ids[7] = (zr_draw_index)(idx1+1);
                ids[8] = (zr_draw_index)(idx1 + 0); ids[9] = (zr_draw_index)(idx1+0);
                ids[10]= (zr_draw_index)(idx2 + 0); ids[11] = (zr_draw_index)(idx2+1);
                ids[12]= (zr_draw_index)(idx2 + 2); ids[13] = (zr_draw_index)(idx1+2);
                ids[14]= (zr_draw_index)(idx1 + 3); ids[15] = (zr_draw_index)(idx1+3);
                ids[16]= (zr_draw_index)(idx2 + 3); ids[17] = (zr_draw_index)(idx2+2);
                ids += 18;
                idx1 = idx2;
            }

            /* add vertices */
            for (i = 0; i < points_count; ++i) {
                const struct zr_vec2 uv = list->null.uv;
                vtx[0] = zr_draw_vertex(temp[i*4+0], uv, col_trans);
                vtx[1] = zr_draw_vertex(temp[i*4+1], uv, col);
                vtx[2] = zr_draw_vertex(temp[i*4+2], uv, col);
                vtx[3] = zr_draw_vertex(temp[i*4+3], uv, col_trans);
                vtx += 4;
            }
        }

        /* free temporary normals + points */
        zr_buffer_reset(list->vertices, ZR_BUFFER_FRONT);
    } else {
        /* NON ANTI-ALIASED STROKE */
        zr_size i1 = 0;
        zr_size idx = list->vertex_count;
        const zr_size idx_count = count * 6;
        const zr_size vtx_count = count * 4;
        struct zr_draw_vertex *vtx = zr_canvas_alloc_vertices(list, vtx_count);
        zr_draw_index *ids = zr_canvas_alloc_elements(list, idx_count);
        if (!vtx || !ids) return;

        for (i1 = 0; i1 < count; ++i1) {
            float dx, dy;
            const struct zr_vec2 uv = list->null.uv;
            const zr_size i2 = ((i1+1) == points_count) ? 0 : i1 + 1;
            const struct zr_vec2 p1 = points[i1];
            const struct zr_vec2 p2 = points[i2];
            struct zr_vec2 diff = zr_vec2_sub(p2, p1);
            float len;

            /* vec2 inverted lenth  */
            len = zr_vec2_len_sqr(diff);
            if (len != 0.0f)
                len = zr_inv_sqrt(len);
            else len = 1.0f;
            diff = zr_vec2_muls(diff, len);

            /* add vertices */
            dx = diff.x * (thickness * 0.5f);
            dy = diff.y * (thickness * 0.5f);

            vtx[0] = zr_draw_vertex(zr_vec2(p1.x + dy, p1.y - dx), uv, col);
            vtx[1] = zr_draw_vertex(zr_vec2(p2.x + dy, p2.y - dx), uv, col);
            vtx[2] = zr_draw_vertex(zr_vec2(p2.x - dy, p2.y + dx), uv, col);
            vtx[3] = zr_draw_vertex(zr_vec2(p1.x - dy, p1.y + dx), uv, col);
            vtx += 4;

            ids[0] = (zr_draw_index)(idx+0); ids[1] = (zr_draw_index)(idx+1);
            ids[2] = (zr_draw_index)(idx+2); ids[3] = (zr_draw_index)(idx+0);
            ids[4] = (zr_draw_index)(idx+2); ids[5] = (zr_draw_index)(idx+3);
            ids += 6;
            idx += 4;
        }
    }
}

static void
zr_canvas_add_poly_convex(struct zr_canvas *list, struct zr_vec2 *points,
    const unsigned int points_count, struct zr_color color,
    enum zr_anti_aliasing aliasing)
{
    static const zr_size pnt_align = ZR_ALIGNOF(struct zr_vec2);
    static const zr_size pnt_size = sizeof(struct zr_vec2);
    zr_draw_vertex_color col;
    ZR_ASSERT(list);
    if (!list || points_count < 3) return;

#if ZR_COMPILE_WITH_COMMAND_USERDATA
    zr_canvas_push_userdata(list, list->userdata);
#endif

    color.a *= list->global_alpha;
    col = zr_color32(color);
    if (aliasing == ZR_ANTI_ALIASING_ON) {
        zr_size i = 0;
        zr_size i0 = 0, i1 = 0;

        const float AA_SIZE = 1.0f;
        const zr_draw_vertex_color col_trans = col & 0x00ffffff;
        zr_size index = list->vertex_count;
        const zr_size idx_count = (points_count-2)*3 + points_count*6;
        const zr_size vtx_count = (points_count*2);
        struct zr_draw_vertex *vtx = zr_canvas_alloc_vertices(list, vtx_count);
        zr_draw_index *ids = zr_canvas_alloc_elements(list, idx_count);

        unsigned int vtx_inner_idx = (unsigned int)(index + 0);
        unsigned int vtx_outer_idx = (unsigned int)(index + 1);
        struct zr_vec2 *normals = 0;
        zr_size size = 0;
        if (!vtx || !ids) return;

        /* temporary allocate normals */
        zr_buffer_mark(list->vertices, ZR_BUFFER_FRONT);
        size = pnt_size * points_count;
        normals = (struct zr_vec2*)
            zr_buffer_alloc(list->vertices, ZR_BUFFER_FRONT, size, pnt_align);
        if (!normals) return;

        /* add elements */
        for (i = 2; i < points_count; i++) {
            ids[0] = (zr_draw_index)(vtx_inner_idx);
            ids[1] = (zr_draw_index)(vtx_inner_idx + ((i-1) << 1));
            ids[2] = (zr_draw_index)(vtx_inner_idx + (i << 1));
            ids += 3;
        }

        /* compute normals */
        for (i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++) {
            struct zr_vec2 p0 = points[i0];
            struct zr_vec2 p1 = points[i1];
            struct zr_vec2 diff = zr_vec2_sub(p1, p0);

            /* vec2 inverted lenth  */
            float len = zr_vec2_len_sqr(diff);
            if (len != 0.0f)
                len = zr_inv_sqrt(len);
            else len = 1.0f;
            diff = zr_vec2_muls(diff, len);

            normals[i0].x = diff.y;
            normals[i0].y = -diff.x;
        }

        /* add vertices + indexes */
        for (i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++) {
            const struct zr_vec2 uv = list->null.uv;
            struct zr_vec2 n0 = normals[i0];
            struct zr_vec2 n1 = normals[i1];
            struct zr_vec2 dm = zr_vec2_muls(zr_vec2_add(n0, n1), 0.5f);

            float dmr2 = dm.x*dm.x + dm.y*dm.y;
            if (dmr2 > 0.000001f) {
                float scale = 1.0f / dmr2;
                scale = MIN(scale, 100.0f);
                dm = zr_vec2_muls(dm, scale);
            }
            dm = zr_vec2_muls(dm, AA_SIZE * 0.5f);

            /* add vertices */
            vtx[0] = zr_draw_vertex(zr_vec2_sub(points[i1], dm), uv, col);
            vtx[1] = zr_draw_vertex(zr_vec2_add(points[i1], dm), uv, col_trans);
            vtx += 2;

            /* add indexes */
            ids[0] = (zr_draw_index)(vtx_inner_idx+(i1<<1));
            ids[1] = (zr_draw_index)(vtx_inner_idx+(i0<<1));
            ids[2] = (zr_draw_index)(vtx_outer_idx+(i0<<1));
            ids[3] = (zr_draw_index)(vtx_outer_idx+(i0<<1));
            ids[4] = (zr_draw_index)(vtx_outer_idx+(i1<<1));
            ids[5] = (zr_draw_index)(vtx_inner_idx+(i1<<1));
            ids += 6;
        }
        /* free temporary normals + points */
        zr_buffer_reset(list->vertices, ZR_BUFFER_FRONT);
    } else {
        zr_size i = 0;
        zr_size index = list->vertex_count;
        const zr_size idx_count = (points_count-2)*3;
        const zr_size vtx_count = points_count;
        struct zr_draw_vertex *vtx = zr_canvas_alloc_vertices(list, vtx_count);
        zr_draw_index *ids = zr_canvas_alloc_elements(list, idx_count);
        if (!vtx || !ids) return;
        for (i = 0; i < vtx_count; ++i) {
            vtx[0] = zr_draw_vertex(points[i], list->null.uv, col);
            vtx++;
        }
        for (i = 2; i < points_count; ++i) {
            ids[0] = (zr_draw_index)index;
            ids[1] = (zr_draw_index)(index+ i - 1);
            ids[2] = (zr_draw_index)(index+i);
            ids += 3;
        }
    }
}

static void
zr_canvas_path_clear(struct zr_canvas *list)
{
    ZR_ASSERT(list);
    if (!list) return;
    zr_buffer_reset(list->buffer, ZR_BUFFER_FRONT);
    list->path_count = 0;
    list->path_offset = 0;
}

static void
zr_canvas_path_line_to(struct zr_canvas *list, struct zr_vec2 pos)
{
    struct zr_vec2 *points = 0;
    struct zr_draw_command *cmd = 0;
    ZR_ASSERT(list);
    if (!list) return;
    if (!list->cmd_count)
        zr_canvas_add_clip(list, zr_null_rect);

    cmd = zr_canvas_command_last(list);
    if (cmd && cmd->texture.ptr != list->null.texture.ptr)
        zr_canvas_push_image(list, list->null.texture);

    points = zr_canvas_alloc_path(list, 1);
    if (!points) return;
    points[0] = pos;
}

static void
zr_canvas_path_arc_to_fast(struct zr_canvas *list, struct zr_vec2 center,
    float radius, int a_min, int a_max)
{
    ZR_ASSERT(list);
    if (!list) return;
    if (a_min <= a_max) {
        int a = 0;
        for (a = a_min; a <= a_max; a++) {
            const struct zr_vec2 c = list->circle_vtx[(zr_size)a % ZR_LEN(list->circle_vtx)];
            const float x = center.x + c.x * radius;
            const float y = center.y + c.y * radius;
            zr_canvas_path_line_to(list, zr_vec2(x, y));
        }
    }
}

static void
zr_canvas_path_arc_to(struct zr_canvas *list, struct zr_vec2 center,
    float radius, float a_min, float a_max, unsigned int segments)
{
    unsigned int i = 0;
    ZR_ASSERT(list);
    if (!list) return;
    if (radius == 0.0f) return;
    for (i = 0; i <= segments; ++i) {
        const float a = a_min + ((float)i / ((float)segments) * (a_max - a_min));
        const float x = center.x + (float)zr_cos(a) * radius;
        const float y = center.y + (float)zr_sin(a) * radius;
        zr_canvas_path_line_to(list, zr_vec2(x, y));
    }
}

static void
zr_canvas_path_rect_to(struct zr_canvas *list, struct zr_vec2 a,
    struct zr_vec2 b, float rounding)
{
    float r;
    ZR_ASSERT(list);
    if (!list) return;
    r = rounding;
    r = MIN(r, ((b.x-a.x) < 0) ? -(b.x-a.x): (b.x-a.x));
    r = MIN(r, ((b.y-a.y) < 0) ? -(b.y-a.y): (b.y-a.y));

    if (r == 0.0f) {
        zr_canvas_path_line_to(list, a);
        zr_canvas_path_line_to(list, zr_vec2(b.x,a.y));
        zr_canvas_path_line_to(list, b);
        zr_canvas_path_line_to(list, zr_vec2(a.x,b.y));
    } else {
        zr_canvas_path_arc_to_fast(list, zr_vec2(a.x + r, a.y + r), r, 6, 9);
        zr_canvas_path_arc_to_fast(list, zr_vec2(b.x - r, a.y + r), r, 9, 12);
        zr_canvas_path_arc_to_fast(list, zr_vec2(b.x - r, b.y - r), r, 0, 3);
        zr_canvas_path_arc_to_fast(list, zr_vec2(a.x + r, b.y - r), r, 3, 6);
    }
}

static void
zr_canvas_path_curve_to(struct zr_canvas *list, struct zr_vec2 p2,
    struct zr_vec2 p3, struct zr_vec2 p4, unsigned int num_segments)
{
    unsigned int i_step;
    float t_step;
    struct zr_vec2 p1;

    ZR_ASSERT(list);
    ZR_ASSERT(list->path_count);
    if (!list || !list->path_count) return;
    num_segments = MAX(num_segments, 1);

    p1 = zr_canvas_path_last(list);
    t_step = 1.0f/(float)num_segments;
    for (i_step = 1; i_step <= num_segments; ++i_step) {
        float t = t_step * (float)i_step;
        float u = 1.0f - t;
        float w1 = u*u*u;
        float w2 = 3*u*u*t;
        float w3 = 3*u*t*t;
        float w4 = t * t *t;
        float x = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x;
        float y = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y;
        zr_canvas_path_line_to(list, zr_vec2(x,y));
    }
}

static void
zr_canvas_path_fill(struct zr_canvas *list, struct zr_color color)
{
    struct zr_vec2 *points;
    ZR_ASSERT(list);
    if (!list) return;
    points = (struct zr_vec2*)zr_buffer_memory(list->buffer);
    zr_canvas_add_poly_convex(list, points, list->path_count, color, list->shape_AA);
    zr_canvas_path_clear(list);
}

static void
zr_canvas_path_stroke(struct zr_canvas *list, struct zr_color color,
    int closed, float thickness)
{
    struct zr_vec2 *points;
    ZR_ASSERT(list);
    if (!list) return;
    points = (struct zr_vec2*)zr_buffer_memory(list->buffer);
    zr_canvas_add_poly_line(list, points, list->path_count, color,
        closed, thickness, list->line_AA);
    zr_canvas_path_clear(list);
}

static void
zr_canvas_add_line(struct zr_canvas *list, struct zr_vec2 a,
    struct zr_vec2 b, struct zr_color col, float thickness)
{
    ZR_ASSERT(list);
    if (!list || !col.a) return;
    zr_canvas_path_line_to(list, zr_vec2_add(a, zr_vec2(0.5f, 0.5f)));
    zr_canvas_path_line_to(list, zr_vec2_add(b, zr_vec2(0.5f, 0.5f)));
    zr_canvas_path_stroke(list,  col, ZR_STROKE_OPEN, thickness);
}

static void
zr_canvas_add_rect(struct zr_canvas *list, struct zr_rect rect,
    struct zr_color col, float rounding)
{
    ZR_ASSERT(list);
    if (!list || !col.a) return;
    zr_canvas_path_rect_to(list, zr_vec2(rect.x + 0.5f, rect.y + 0.5f),
        zr_vec2(rect.x + rect.w + 0.5f, rect.y + rect.h + 0.5f), rounding);
    zr_canvas_path_fill(list,  col);
}

static void
zr_canvas_add_triangle(struct zr_canvas *list, struct zr_vec2 a,
    struct zr_vec2 b, struct zr_vec2 c, struct zr_color col)
{
    ZR_ASSERT(list);
    if (!list || !col.a) return;
    zr_canvas_path_line_to(list, a);
    zr_canvas_path_line_to(list, b);
    zr_canvas_path_line_to(list, c);
    zr_canvas_path_fill(list, col);
}

static void
zr_canvas_add_circle(struct zr_canvas *list, struct zr_vec2 center,
    float radius, struct zr_color col, unsigned int segs)
{
    float a_max;
    ZR_ASSERT(list);
    if (!list || !col.a) return;
    a_max = ZR_PI * 2.0f * ((float)segs - 1.0f) / (float)segs;
    zr_canvas_path_arc_to(list, center, radius, 0.0f, a_max, segs);
    zr_canvas_path_fill(list, col);
}

static void
zr_canvas_add_curve(struct zr_canvas *list, struct zr_vec2 p0,
    struct zr_vec2 cp0, struct zr_vec2 cp1, struct zr_vec2 p1,
    struct zr_color col, unsigned int segments, float thickness)
{
    ZR_ASSERT(list);
    if (!list || !col.a) return;
    zr_canvas_path_line_to(list, p0);
    zr_canvas_path_curve_to(list, cp0, cp1, p1, segments);
    zr_canvas_path_stroke(list, col, ZR_STROKE_OPEN, thickness);
}

static void
zr_canvas_push_rect_uv(struct zr_canvas *list, struct zr_vec2 a,
    struct zr_vec2 c, struct zr_vec2 uva, struct zr_vec2 uvc,
    struct zr_color color)
{
    zr_draw_vertex_color col = zr_color32(color);
    struct zr_draw_vertex *vtx;
    struct zr_vec2 uvb, uvd;
    struct zr_vec2 b,d;
    zr_draw_index *idx;
    zr_draw_index index;
    ZR_ASSERT(list);
    if (!list) return;

    uvb = zr_vec2(uvc.x, uva.y);
    uvd = zr_vec2(uva.x, uvc.y);
    b = zr_vec2(c.x, a.y);
    d = zr_vec2(a.x, c.y);

    index = (zr_draw_index)list->vertex_count;
    vtx = zr_canvas_alloc_vertices(list, 4);
    idx = zr_canvas_alloc_elements(list, 6);
    if (!vtx || !idx) return;

    idx[0] = (zr_draw_index)(index+0); idx[1] = (zr_draw_index)(index+1);
    idx[2] = (zr_draw_index)(index+2); idx[3] = (zr_draw_index)(index+0);
    idx[4] = (zr_draw_index)(index+2); idx[5] = (zr_draw_index)(index+3);

    vtx[0] = zr_draw_vertex(a, uva, col);
    vtx[1] = zr_draw_vertex(b, uvb, col);
    vtx[2] = zr_draw_vertex(c, uvc, col);
    vtx[3] = zr_draw_vertex(d, uvd, col);
}

static void
zr_canvas_add_image(struct zr_canvas *list, struct zr_image texture,
    struct zr_rect rect, struct zr_color color)
{
    ZR_ASSERT(list);
    if (!list) return;
    /* push new command with given texture */
    zr_canvas_push_image(list, texture.handle);
    if (zr_image_is_subimage(&texture)) {
        /* add region inside of the texture  */
        struct zr_vec2 uv[2];
        uv[0].x = (float)texture.region[0]/(float)texture.w;
        uv[0].y = (float)texture.region[1]/(float)texture.h;
        uv[1].x = (float)(texture.region[0] + texture.region[2])/(float)texture.w;
        uv[1].y = (float)(texture.region[1] + texture.region[3])/(float)texture.h;
        zr_canvas_push_rect_uv(list, zr_vec2(rect.x, rect.y),
            zr_vec2(rect.x + rect.w, rect.y + rect.h),  uv[0], uv[1], color);
    } else zr_canvas_push_rect_uv(list, zr_vec2(rect.x, rect.y),
            zr_vec2(rect.x + rect.w, rect.y + rect.h),
            zr_vec2(0.0f, 0.0f), zr_vec2(1.0f, 1.0f),color);
}

static void
zr_canvas_add_text(struct zr_canvas *list, const struct zr_user_font *font,
    struct zr_rect rect, const char *text, zr_size len, float font_height,
    struct zr_color fg)
{
    float x;
    zr_size text_len;
    zr_rune unicode, next;
    zr_size glyph_len, next_glyph_len;
    struct zr_user_font_glyph g;

    ZR_ASSERT(list);
    if (!list || !len || !text) return;
    if (rect.x > (list->clip_rect.x + list->clip_rect.w) ||
        rect.y > (list->clip_rect.y + list->clip_rect.h) ||
        rect.x < list->clip_rect.x || rect.y < list->clip_rect.y)
        return;

    /* draw every glyph image */
    zr_canvas_push_image(list, font->texture);
    x = rect.x;
    glyph_len = text_len = zr_utf_decode(text, &unicode, len);
    if (!glyph_len) return;
    while (text_len <= len && glyph_len) {
        float gx, gy, gh, gw;
        float char_width = 0;
        if (unicode == ZR_UTF_INVALID) break;

        /* query currently drawn glyph information */
        next_glyph_len = zr_utf_decode(text + text_len, &next, len - text_len);
        font->query(font->userdata, font_height, &g, unicode,
                    (next == ZR_UTF_INVALID) ? '\0' : next);

        /* calculate and draw glyph drawing rectangle and image */
        gx = x + g.offset.x;
        /*gy = rect.y + (rect.h/2) - (font->height/2) + g.offset.y;*/
        gy = rect.y + g.offset.y;
        gw = g.width; gh = g.height;
        char_width = g.xadvance;
        fg.a *= list->global_alpha;
        zr_canvas_push_rect_uv(list, zr_vec2(gx,gy), zr_vec2(gx + gw, gy+ gh),
            g.uv[0], g.uv[1], fg);

        /* offset next glyph */
        text_len += glyph_len;
        x += char_width;
        glyph_len = next_glyph_len;
        unicode = next;
    }
}

static void
zr_canvas_load(struct zr_canvas *list, struct zr_context *queue,
    float line_thickness, unsigned int curve_segments)
{
    const struct zr_command *cmd;
    ZR_ASSERT(list);
    ZR_ASSERT(list->vertices);
    ZR_ASSERT(list->elements);
    ZR_ASSERT(queue);
    line_thickness = MAX(line_thickness, 1.0f);
    if (!list || !queue || !list->vertices || !list->elements) return;

    zr_foreach(cmd, queue)
    {
#if ZR_COMPILE_WITH_COMMAND_USERDATA
        list->userdata = cmd->userdata;
#endif
        switch (cmd->type) {
        case ZR_COMMAND_NOP: break;
        case ZR_COMMAND_SCISSOR: {
            const struct zr_command_scissor *s = zr_command(scissor, cmd);
            zr_canvas_add_clip(list, zr_rect(s->x, s->y, s->w, s->h));
        } break;
        case ZR_COMMAND_LINE: {
            const struct zr_command_line *l = zr_command(line, cmd);
            zr_canvas_add_line(list, zr_vec2(l->begin.x, l->begin.y),
                zr_vec2(l->end.x, l->end.y), l->color, line_thickness);
        } break;
        case ZR_COMMAND_CURVE: {
            const struct zr_command_curve *q = zr_command(curve, cmd);
            zr_canvas_add_curve(list, zr_vec2(q->begin.x, q->begin.y),
                zr_vec2(q->ctrl[0].x, q->ctrl[0].y), zr_vec2(q->ctrl[1].x,
                q->ctrl[1].y), zr_vec2(q->end.x, q->end.y), q->color,
                curve_segments, line_thickness);
        } break;
        case ZR_COMMAND_RECT: {
            const struct zr_command_rect *r = zr_command(rect, cmd);
            zr_canvas_add_rect(list, zr_rect(r->x, r->y, r->w, r->h),
                r->color, (float)r->rounding);
        } break;
        case ZR_COMMAND_CIRCLE: {
            const struct zr_command_circle *c = zr_command(circle, cmd);
            zr_canvas_add_circle(list, zr_vec2((float)c->x + (float)c->w/2,
                (float)c->y + (float)c->h/2), (float)c->w/2, c->color,
                curve_segments);
        } break;
        case ZR_COMMAND_ARC: {
            const struct zr_command_arc *c = zr_command(arc, cmd);
            zr_canvas_path_line_to(list, zr_vec2(c->cx, c->cy));
            zr_canvas_path_arc_to(list, zr_vec2(c->cx, c->cy), c->r,
                c->a[0], c->a[1], curve_segments);
            zr_canvas_path_fill(list, c->color);
        } break;
        case ZR_COMMAND_TRIANGLE: {
            const struct zr_command_triangle *t = zr_command(triangle, cmd);
            zr_canvas_add_triangle(list, zr_vec2(t->a.x, t->a.y),
                zr_vec2(t->b.x, t->b.y), zr_vec2(t->c.x, t->c.y), t->color);
        } break;
        case ZR_COMMAND_TEXT: {
            const struct zr_command_text *t = zr_command(text, cmd);
            zr_canvas_add_text(list, t->font, zr_rect(t->x, t->y, t->w, t->h),
                t->string, t->length, t->height, t->foreground);
        } break;
        case ZR_COMMAND_IMAGE: {
            const struct zr_command_image *i = zr_command(image, cmd);
            zr_canvas_add_image(list, i->img, zr_rect(i->x, i->y, i->w, i->h),
                zr_rgb(255, 255, 255));
        } break;
        default: break;
        }
    }
}

void
zr_convert(struct zr_context *ctx, struct zr_buffer *cmds,
    struct zr_buffer *vertices, struct zr_buffer *elements,
    const struct zr_convert_config *config)
{
    zr_canvas_setup(&ctx->canvas, config->global_alpha, cmds, vertices, elements,
        config->null, config->line_AA, config->shape_AA);
    zr_canvas_load(&ctx->canvas, ctx, config->line_thickness,
        config->circle_segment_count);
}

const struct zr_draw_command*
zr__draw_begin(const struct zr_context *ctx,
    const struct zr_buffer *buffer)
{
    zr_byte *memory;
    zr_size offset;
    const struct zr_draw_command *cmd;
    ZR_ASSERT(buffer);
    if (!buffer || !buffer->size || !ctx->canvas.cmd_count) return 0;

    memory = (zr_byte*)buffer->memory.ptr;
    offset = buffer->memory.size - ctx->canvas.cmd_offset;
    cmd = zr_ptr_add(const struct zr_draw_command, memory, offset);
    return cmd;
}

const struct zr_draw_command*
zr__draw_next(const struct zr_draw_command *cmd,
    const struct zr_buffer *buffer, const struct zr_context *ctx)
{
    zr_byte *memory;
    zr_size offset, size;
    const struct zr_draw_command *end;
    ZR_ASSERT(buffer);
    ZR_ASSERT(ctx);
    if (!cmd || !buffer || !ctx) return 0;
    memory = (zr_byte*)buffer->memory.ptr;
    size = buffer->memory.size;
    offset = size - ctx->canvas.cmd_offset;
    end = zr_ptr_add(const struct zr_draw_command, memory, offset);
    end -= (ctx->canvas.cmd_count-1);
    if (cmd <= end) return 0;
    return (cmd-1);
}

#endif
/*
 * ==============================================================
 *
 *                          FONT
 *
 * ===============================================================
 */
#ifdef ZR_COMPILE_WITH_FONT

/* this is a bloody mess but 'fixing' both stb libraries would be a pain */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wbad-function-cast"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wbad-function-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4456)
#endif

#if !ZR_DISABLE_STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#endif
#include "stb_rect_pack.h"

#if !ZR_DISABLE_STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#endif
#include "stb_truetype.h"

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#elif _MSC_VER
#pragma warning (pop)
#endif

struct zr_font_bake_data {
    stbtt_fontinfo info;
    stbrp_rect *rects;
    stbtt_pack_range *ranges;
    zr_rune range_count;
};

struct zr_font_baker {
    stbtt_pack_context spc;
    struct zr_font_bake_data *build;
    stbtt_packedchar *packed_chars;
    stbrp_rect *rects;
    stbtt_pack_range *ranges;
};

static const zr_size zr_rect_align = ZR_ALIGNOF(stbrp_rect);
static const zr_size zr_range_align = ZR_ALIGNOF(stbtt_pack_range);
static const zr_size zr_char_align = ZR_ALIGNOF(stbtt_packedchar);
static const zr_size zr_build_align = ZR_ALIGNOF(struct zr_font_bake_data);
static const zr_size zr_baker_align = ZR_ALIGNOF(struct zr_font_baker);

static int
zr_range_count(const zr_rune *range)
{
    const zr_rune *iter = range;
    ZR_ASSERT(range);
    if (!range) return 0;
    while (*(iter++) != 0);
    return (iter == range) ? 0 : (int)((iter - range)/2);
}

static int
zr_range_glyph_count(const zr_rune *range, int count)
{
    int i = 0;
    int total_glyphs = 0;
    for (i = 0; i < count; ++i) {
        int diff;
        zr_rune f = range[(i*2)+0];
        zr_rune t = range[(i*2)+1];
        ZR_ASSERT(t >= f);
        diff = (int)((t - f) + 1);
        total_glyphs += diff;
    }
    return total_glyphs;
}

const zr_rune*
zr_font_default_glyph_ranges(void)
{
    static const zr_rune ranges[] = {0x0020, 0x00FF, 0};
    return ranges;
}

const zr_rune*
zr_font_chinese_glyph_ranges(void)
{
    static const zr_rune ranges[] = {
        0x0020, 0x00FF,
        0x3000, 0x30FF,
        0x31F0, 0x31FF,
        0xFF00, 0xFFEF,
        0x4e00, 0x9FAF,
        0
    };
    return ranges;
}

const zr_rune*
zr_font_cyrillic_glyph_ranges(void)
{
    static const zr_rune ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0
    };
    return ranges;
}

const zr_rune*
zr_font_korean_glyph_ranges(void)
{
    static const zr_rune ranges[] = {
        0x0020, 0x00FF,
        0x3131, 0x3163,
        0xAC00, 0xD79D,
        0
    };
    return ranges;
}

void
zr_font_bake_memory(zr_size *temp, int *glyph_count,
    struct zr_font_config *config, int count)
{
    int i, range_count = 0;
    ZR_ASSERT(config);
    ZR_ASSERT(glyph_count);
    if (!config) {
        *temp = 0;
        *glyph_count = 0;
        return;
    }

    *glyph_count = 0;
    if (!config->range)
        config->range = zr_font_default_glyph_ranges();
    for (i = 0; i < count; ++i) {
        range_count += zr_range_count(config[i].range);
        *glyph_count += zr_range_glyph_count(config[i].range, range_count);
    }

    *temp = (zr_size)*glyph_count * sizeof(stbrp_rect);
    *temp += (zr_size)range_count * sizeof(stbtt_pack_range);
    *temp += (zr_size)*glyph_count * sizeof(stbtt_packedchar);
    *temp += (zr_size)count * sizeof(struct zr_font_bake_data);
    *temp += sizeof(struct zr_font_baker);
    *temp += zr_rect_align + zr_range_align + zr_char_align;
    *temp += zr_build_align + zr_baker_align;
}

static struct zr_font_baker*
zr_font_baker(void *memory, int glyph_count, int count)
{
    struct zr_font_baker *baker;
    if (!memory) return 0;
    /* setup baker inside a memory block  */
    baker = (struct zr_font_baker*)ZR_ALIGN_PTR(memory, zr_baker_align);
    baker->build = (struct zr_font_bake_data*)ZR_ALIGN_PTR((baker + 1), zr_build_align);
    baker->packed_chars = (stbtt_packedchar*)ZR_ALIGN_PTR((baker->build + count), zr_char_align);
    baker->rects = (stbrp_rect*)ZR_ALIGN_PTR((baker->packed_chars + glyph_count), zr_rect_align);
    baker->ranges = (stbtt_pack_range*)ZR_ALIGN_PTR((baker->rects + glyph_count), zr_range_align);
    return baker;
}

int
zr_font_bake_pack(zr_size *image_memory, int *width, int *height,
    struct zr_recti *custom, void *temp, zr_size temp_size,
    const struct zr_font_config *config, int count)
{
    static const zr_size max_height = 1024 * 32;
    struct zr_font_baker* baker;
    int total_glyph_count = 0;
    int total_range_count = 0;
    int i = 0;

    ZR_ASSERT(image_memory);
    ZR_ASSERT(width);
    ZR_ASSERT(height);
    ZR_ASSERT(config);
    ZR_ASSERT(temp);
    ZR_ASSERT(temp_size);
    ZR_ASSERT(count);
    if (!image_memory || !width || !height || !config || !temp ||
        !temp_size || !count) return zr_false;

    for (i = 0; i < count; ++i) {
        total_range_count += zr_range_count(config[i].range);
        total_glyph_count += zr_range_glyph_count(config[i].range, total_range_count);
    }

    /* setup font baker from temporary memory */
    zr_zero(temp, temp_size);
    baker = zr_font_baker(temp, total_glyph_count, count);
    if (!baker) return zr_false;
    for (i = 0; i < count; ++i) {
        const struct zr_font_config *cfg = &config[i];
        if (!stbtt_InitFont(&baker->build[i].info, (const unsigned char*)cfg->ttf_blob, 0))
            return zr_false;
    }

    *height = 0;
    *width = (total_glyph_count > 1000) ? 1024 : 512;
    stbtt_PackBegin(&baker->spc, 0, (int)*width, (int)max_height, 0, 1, 0);
    {
        int input_i = 0;
        int range_n = 0, rect_n = 0, char_n = 0;

        /* pack custom user data first so it will be in the upper left corner*/
        if (custom) {
            stbrp_rect custom_space;
            zr_zero(&custom_space, sizeof(custom_space));
            custom_space.w = (stbrp_coord)((custom->w * 2) + 1);
            custom_space.h = (stbrp_coord)(custom->h + 1);

            stbtt_PackSetOversampling(&baker->spc, 1, 1);
            stbrp_pack_rects((stbrp_context*)baker->spc.pack_info, &custom_space, 1);
            *height = MAX(*height, (int)(custom_space.y + custom_space.h));

            custom->x = (short)custom_space.x;
            custom->y = (short)custom_space.y;
            custom->w = (short)custom_space.w;
            custom->h = (short)custom_space.h;
        }

        /* first font pass: pack all glyphs */
        for (input_i = 0; input_i < count; input_i++) {
            int n = 0;
            const zr_rune *in_range;
            const struct zr_font_config *cfg = &config[input_i];
            struct zr_font_bake_data *tmp = &baker->build[input_i];
            int glyph_count, range_count;

            /* count glyphs + ranges in current font */
            glyph_count = 0; range_count = 0;
            for (in_range = cfg->range; in_range[0] && in_range[1]; in_range += 2) {
                glyph_count += (int)(in_range[1] - in_range[0]) + 1;
                range_count++;
            }

            /* setup ranges  */
            tmp->ranges = baker->ranges + range_n;
            tmp->range_count = (zr_rune)range_count;
            range_n += range_count;
            for (i = 0; i < range_count; ++i) {
                in_range = &cfg->range[i * 2];
                tmp->ranges[i].font_size = cfg->size;
                tmp->ranges[i].first_unicode_codepoint_in_range = (int)in_range[0];
                tmp->ranges[i].num_chars = (int)(in_range[1]- in_range[0]) + 1;
                tmp->ranges[i].chardata_for_range = baker->packed_chars + char_n;
                char_n += tmp->ranges[i].num_chars;
            }

            /* pack */
            tmp->rects = baker->rects + rect_n;
            rect_n += glyph_count;
            stbtt_PackSetOversampling(&baker->spc, cfg->oversample_h, cfg->oversample_v);
            n = stbtt_PackFontRangesGatherRects(&baker->spc, &tmp->info,
                tmp->ranges, (int)tmp->range_count, tmp->rects);
            stbrp_pack_rects((stbrp_context*)baker->spc.pack_info, tmp->rects, (int)n);

            /* texture height */
            for (i = 0; i < n; ++i) {
                if (tmp->rects[i].was_packed)
                    *height = MAX(*height, tmp->rects[i].y + tmp->rects[i].h);
            }
        }
        ZR_ASSERT(rect_n == total_glyph_count);
        ZR_ASSERT(char_n == total_glyph_count);
        ZR_ASSERT(range_n == total_range_count);
    }
    *height = (int)zr_round_up_pow2((zr_uint)*height);
    *image_memory = (zr_size)(*width) * (zr_size)(*height);
    return zr_true;
}

void
zr_font_bake(void *image_memory, int width, int height,
    void *temp, zr_size temp_size, struct zr_font_glyph *glyphs,
    int glyphs_count, const struct zr_font_config *config, int font_count)
{
    int input_i = 0;
    struct zr_font_baker* baker;
    zr_rune glyph_n = 0;

    ZR_ASSERT(image_memory);
    ZR_ASSERT(width);
    ZR_ASSERT(height);
    ZR_ASSERT(config);
    ZR_ASSERT(temp);
    ZR_ASSERT(temp_size);
    ZR_ASSERT(font_count);
    ZR_ASSERT(glyphs_count);
    if (!image_memory || !width || !height || !config || !temp ||
        !temp_size || !font_count || !glyphs || !glyphs_count)
        return;

    /* second font pass: render glyphs */
    baker = (struct zr_font_baker*)ZR_ALIGN_PTR(temp, zr_baker_align);
    zr_zero(image_memory, (zr_size)((zr_size)width * (zr_size)height));
    baker->spc.pixels = (unsigned char*)image_memory;
    baker->spc.height = (int)height;
    for (input_i = 0; input_i < font_count; ++input_i) {
        const struct zr_font_config *cfg = &config[input_i];
        struct zr_font_bake_data *tmp = &baker->build[input_i];
        stbtt_PackSetOversampling(&baker->spc, cfg->oversample_h, cfg->oversample_v);
        stbtt_PackFontRangesRenderIntoRects(&baker->spc, &tmp->info, tmp->ranges,
            (int)tmp->range_count, tmp->rects);
    }
    stbtt_PackEnd(&baker->spc);

    /* third pass: setup font and glyphs */
    for (input_i = 0; input_i < font_count; ++input_i)  {
        zr_size i = 0;
        int char_idx = 0;
        zr_rune glyph_count = 0;
        const struct zr_font_config *cfg = &config[input_i];
        struct zr_font_bake_data *tmp = &baker->build[input_i];
        struct zr_baked_font *dst_font = cfg->font;

        float font_scale = stbtt_ScaleForPixelHeight(&tmp->info, cfg->size);
        int unscaled_ascent, unscaled_descent, unscaled_line_gap;
        stbtt_GetFontVMetrics(&tmp->info, &unscaled_ascent, &unscaled_descent,
                                &unscaled_line_gap);

        /* fill baked font */
        dst_font->ranges = cfg->range;
        dst_font->height = cfg->size;
        dst_font->ascent = ((float)unscaled_ascent * font_scale);
        dst_font->descent = ((float)unscaled_descent * font_scale);
        dst_font->glyph_offset = glyph_n;

        /* fill own baked font glyph array */
        for (i = 0; i < tmp->range_count; ++i) {
            stbtt_pack_range *range = &tmp->ranges[i];
            for (char_idx = 0; char_idx < range->num_chars; char_idx++) {
                zr_rune codepoint = 0;
                float dummy_x = 0, dummy_y = 0;
                stbtt_aligned_quad q;
                struct zr_font_glyph *glyph;

                /* query glyph bounds from stb_truetype */
                const stbtt_packedchar *pc = &range->chardata_for_range[char_idx];
                glyph_count++;
                if (!pc->x0 && !pc->x1 && !pc->y0 && !pc->y1) continue;
                codepoint = (zr_rune)(range->first_unicode_codepoint_in_range + char_idx);
                stbtt_GetPackedQuad(range->chardata_for_range, (int)width,
                    (int)height, char_idx, &dummy_x, &dummy_y, &q, 0);

                /* fill own glyph type with data */
                glyph = &glyphs[dst_font->glyph_offset + (unsigned int)char_idx];
                glyph->codepoint = codepoint;
                glyph->x0 = q.x0; glyph->y0 = q.y0;
                glyph->x1 = q.x1; glyph->y1 = q.y1;
                glyph->y0 += (dst_font->ascent + 0.5f);
                glyph->y1 += (dst_font->ascent + 0.5f);
                glyph->w = glyph->x1 - glyph->x0 + 0.5f;
                glyph->h = glyph->y1 - glyph->y0;

                if (cfg->coord_type == ZR_COORD_PIXEL) {
                    glyph->u0 = q.s0 * (float)width;
                    glyph->v0 = q.t0 * (float)height;
                    glyph->u1 = q.s1 * (float)width;
                    glyph->v1 = q.t1 * (float)height;
                } else {
                    glyph->u0 = q.s0;
                    glyph->v0 = q.t0;
                    glyph->u1 = q.s1;
                    glyph->v1 = q.t1;
                }
                glyph->xadvance = (pc->xadvance + cfg->spacing.x);
                if (cfg->pixel_snap)
                    glyph->xadvance = (float)(int)(glyph->xadvance + 0.5f);
            }
        }
        dst_font->glyph_count = glyph_count;
        glyph_n += dst_font->glyph_count;
    }
}

void
zr_font_bake_custom_data(void *img_memory, int img_width, int img_height,
    struct zr_recti img_dst, const char *texture_data_mask, int tex_width,
    int tex_height, char white, char black)
{
    zr_byte *pixels;
    int y = 0, x = 0, n = 0;
    ZR_ASSERT(img_memory);
    ZR_ASSERT(img_width);
    ZR_ASSERT(img_height);
    ZR_ASSERT(texture_data_mask);
    ZR_UNUSED(tex_height);
    if (!img_memory || !img_width || !img_height || !texture_data_mask)
        return;

    pixels = (zr_byte*)img_memory;
    for (y = 0, n = 0; y < tex_height; ++y) {
        for (x = 0; x < tex_width; ++x, ++n) {
            const int off0 = ((img_dst.x + x) + (img_dst.y + y) * img_width);
            const int off1 = off0 + 1 + tex_width;
            pixels[off0] = (texture_data_mask[n] == white) ? 0xFF : 0x00;
            pixels[off1] = (texture_data_mask[n] == black) ? 0xFF : 0x00;
        }
    }
}

void
zr_font_bake_convert(void *out_memory, int img_width, int img_height,
    const void *in_memory)
{
    int n = 0;
    const zr_byte *src;
    zr_rune *dst;
    ZR_ASSERT(out_memory);
    ZR_ASSERT(in_memory);
    ZR_ASSERT(img_width);
    ZR_ASSERT(img_height);
    if (!out_memory || !in_memory || !img_height || !img_width) return;

    dst = (zr_rune*)out_memory;
    src = (const zr_byte*)in_memory;
    for (n = (int)(img_width * img_height); n > 0; n--)
        *dst++ = ((zr_rune)(*src++) << 24) | 0x00FFFFFF;
}
/* -------------------------------------------------------------
 *
 *                          FONT
 *
 * --------------------------------------------------------------*/
void
zr_font_init(struct zr_font *font, float pixel_height,
    zr_rune fallback_codepoint, struct zr_font_glyph *glyphs,
    const struct zr_baked_font *baked_font, zr_handle atlas)
{
    ZR_ASSERT(font);
    ZR_ASSERT(glyphs);
    ZR_ASSERT(baked_font);
    if (!font || !glyphs || !baked_font)
        return;

    zr_zero(font, sizeof(*font));
    font->ascent = baked_font->ascent;
    font->descent = baked_font->descent;
    font->size = baked_font->height;
    font->scale = (float)pixel_height / (float)font->size;
    font->glyphs = &glyphs[baked_font->glyph_offset];
    font->glyph_count = baked_font->glyph_count;
    font->ranges = baked_font->ranges;
    font->atlas = atlas;
    font->fallback_codepoint = fallback_codepoint;
    font->fallback = zr_font_find_glyph(font, fallback_codepoint);
}

const struct zr_font_glyph*
zr_font_find_glyph(struct zr_font *font, zr_rune unicode)
{
    int i = 0;
    int count;
    int total_glyphs = 0;
    const struct zr_font_glyph *glyph = 0;
    ZR_ASSERT(font);

    glyph = font->fallback;
    count = zr_range_count(font->ranges);
    for (i = 0; i < count; ++i) {
        int diff;
        zr_rune f = font->ranges[(i*2)+0];
        zr_rune t = font->ranges[(i*2)+1];
        diff = (int)((t - f) + 1);
        if (unicode >= f && unicode <= t)
            return &font->glyphs[((zr_rune)total_glyphs + (unicode - f))];
        total_glyphs += diff;
    }
    return glyph;
}

static zr_size
zr_font_text_width(zr_handle handle, float height, const char *text, zr_size len)
{
    zr_rune unicode;
    zr_size text_len  = 0;
    float text_width = 0;
    zr_size glyph_len = 0;
    float scale = 0;

    struct zr_font *font = (struct zr_font*)handle.ptr;
    ZR_ASSERT(font);
    if (!font || !text || !len)
        return 0;

    scale = height/font->size;
    glyph_len = text_len = zr_utf_decode(text, &unicode, len);
    if (!glyph_len) return 0;
    while (text_len <= len && glyph_len) {
        const struct zr_font_glyph *g;
        if (unicode == ZR_UTF_INVALID) break;

        /* query currently drawn glyph information */
        g = zr_font_find_glyph(font, unicode);
        text_width += g->xadvance * scale;

        /* offset next glyph */
        glyph_len = zr_utf_decode(text + text_len, &unicode, len - text_len);
        text_len += glyph_len;
    }
    return (zr_size)text_width;
}

#if ZR_COMPILE_WITH_VERTEX_BUFFER
static void
zr_font_query_font_glyph(zr_handle handle, float height,
    struct zr_user_font_glyph *glyph, zr_rune codepoint, zr_rune next_codepoint)
{
    float scale;
    const struct zr_font_glyph *g;
    struct zr_font *font;
    ZR_ASSERT(glyph);
    ZR_UNUSED(next_codepoint);
    font = (struct zr_font*)handle.ptr;
    ZR_ASSERT(font);
    if (!font || !glyph)
        return;

    scale = height/font->size;
    g = zr_font_find_glyph(font, codepoint);
    glyph->width = (g->x1 - g->x0) * scale;
    glyph->height = (g->y1 - g->y0) * scale;
    glyph->offset = zr_vec2(g->x0 * scale, g->y0 * scale);
    glyph->xadvance = (g->xadvance * scale);
    glyph->uv[0] = zr_vec2(g->u0, g->v0);
    glyph->uv[1] = zr_vec2(g->u1, g->v1);
}
#endif

struct zr_user_font
zr_font_ref(struct zr_font *font)
{
    struct zr_user_font user_font;
    zr_zero(&user_font, sizeof(user_font));
    user_font.height = font->size * font->scale;
    user_font.width = zr_font_text_width;
    user_font.userdata.ptr = font;
#if ZR_COMPILE_WITH_VERTEX_BUFFER
    user_font.query = zr_font_query_font_glyph;
    user_font.texture = font->atlas;
#endif
    return user_font;
}
#endif
/*
 * ==============================================================
 *
 *                          Edit Box
 *
 * ===============================================================
 */
static void
zr_edit_buffer_append(struct zr_buffer *buffer, const char *str, zr_size len)
{
    char *mem;
    ZR_ASSERT(buffer);
    ZR_ASSERT(str);
    if (!buffer || !str || !len) return;
    mem = (char*)zr_buffer_alloc(buffer, ZR_BUFFER_FRONT, len * sizeof(char), 0);
    if (!mem) return;
    zr_memcopy(mem, str, len * sizeof(char));
}

static int
zr_edit_buffer_insert(struct zr_buffer *buffer, zr_size pos,
    const char *str, zr_size len)
{
    void *mem;
    zr_size i;
    char *src, *dst;

    zr_size copylen;
    ZR_ASSERT(buffer);
    if (!buffer || !str || !len || pos > buffer->allocated) return 0;
    if ((buffer->allocated + len >= buffer->memory.size) &&
        (buffer->type == ZR_BUFFER_FIXED)) return 0;

    copylen = buffer->allocated - pos;
    if (!copylen) {
        zr_edit_buffer_append(buffer, str, len);
        return 1;
    }
    mem = zr_buffer_alloc(buffer, ZR_BUFFER_FRONT, len * sizeof(char), 0);
    if (!mem) return 0;

    /* memmove */
    ZR_ASSERT(((int)pos + (int)len + ((int)copylen - 1)) >= 0);
    ZR_ASSERT(((int)pos + ((int)copylen - 1)) >= 0);
    dst = zr_ptr_add(char, buffer->memory.ptr, pos + len + (copylen - 1));
    src = zr_ptr_add(char, buffer->memory.ptr, pos + (copylen-1));
    for (i = 0; i < copylen; ++i) *dst-- = *src--;
    mem = zr_ptr_add(void, buffer->memory.ptr, pos);
    zr_memcopy(mem, str, len * sizeof(char));
    return 1;
}

static void
zr_edit_buffer_remove(struct zr_buffer *buffer, zr_size len)
{
    ZR_ASSERT(buffer);
    if (!buffer || len > buffer->allocated) return;
    ZR_ASSERT(((int)buffer->allocated - (int)len) >= 0);
    buffer->allocated -= len;
}

static void
zr_edit_buffer_del(struct zr_buffer *buffer, zr_size pos, zr_size len)
{
    ZR_ASSERT(buffer);
    if (!buffer || !len || pos > buffer->allocated ||
        pos + len > buffer->allocated) return;

    if (pos + len < buffer->allocated) {
        /* memmove */
        char *dst = zr_ptr_add(char, buffer->memory.ptr, pos);
        char *src = zr_ptr_add(char, buffer->memory.ptr, pos + len);
        zr_memcopy(dst, src, buffer->allocated - (pos + len));
        ZR_ASSERT(((int)buffer->allocated - (int)len) >= 0);
        buffer->allocated -= len;
    } else zr_edit_buffer_remove(buffer, len);
}

static char*
zr_edit_buffer_at_char(struct zr_buffer *buffer, zr_size pos)
{
    ZR_ASSERT(buffer);
    if (!buffer || pos > buffer->allocated) return 0;
    return zr_ptr_add(char, buffer->memory.ptr, pos);
}

static char*
zr_edit_buffer_at(struct zr_buffer *buffer, int pos, zr_rune *unicode,
    zr_size *len)
{
    int i = 0;
    zr_size src_len = 0;
    zr_size glyph_len = 0;
    char *text;
    zr_size text_len;

    ZR_ASSERT(buffer);
    ZR_ASSERT(unicode);
    ZR_ASSERT(len);
    if (!buffer || !unicode || !len) return 0;
    if (pos < 0) {
        *unicode = 0;
        *len = 0;
        return 0;
    }

    text = (char*)buffer->memory.ptr;
    text_len = buffer->allocated;
    glyph_len = zr_utf_decode(text, unicode, text_len);
    while (glyph_len) {
        if (i == pos) {
            *len = glyph_len;
            break;
        }

        i++;
        src_len = src_len + glyph_len;
        glyph_len = zr_utf_decode(text + src_len, unicode, text_len - src_len);
    }
    if (i != pos) return 0;
    return text + src_len;
}

static void
zr_edit_box_init_buffer(struct zr_edit_box *eb, struct zr_buffer *buffer,
    const struct zr_clipboard *clip, zr_filter f)
{
    ZR_ASSERT(eb);
    if (!eb) return;
    zr_zero(eb, sizeof(*eb));
    eb->buffer = *buffer;
    if (clip) eb->clip = *clip;
    if (f) eb->filter = f;
    else eb->filter = zr_filter_default;
    eb->cursor = 0;
    eb->glyphs = 0;
}

static void
zr_edit_box_init(struct zr_edit_box *eb, void *memory, zr_size size,
                const struct zr_clipboard *clip, zr_filter f)
{
    ZR_ASSERT(eb);
    if (!eb) return;
    zr_zero(eb, sizeof(*eb));
    zr_buffer_init_fixed(&eb->buffer, memory, size);
    if (clip) eb->clip = *clip;
    if (f) eb->filter = f;
    else eb->filter = zr_filter_default;
    eb->cursor = 0;
    eb->glyphs = 0;
}

void
zr_edit_box_clear(struct zr_edit_box *box)
{
    ZR_ASSERT(box);
    if (!box) return;
    zr_buffer_clear(&box->buffer);
    box->cursor = box->glyphs = 0;
}

void
zr_edit_box_add(struct zr_edit_box *eb, const char *str, zr_size len)
{
    int res = 0;
    ZR_ASSERT(eb);
    if (!eb || !str || !len) return;

    if (eb->cursor != eb->glyphs) {
        zr_size l = 0;
        zr_rune unicode;
        int cursor = (eb->cursor) ? (int)(eb->cursor) : 0;
        char *sym = zr_edit_buffer_at(&eb->buffer, cursor, &unicode, &l);
        zr_size offset = (zr_size)(sym - (char*)eb->buffer.memory.ptr);
        res = zr_edit_buffer_insert(&eb->buffer, offset, str, len);
    } else res = zr_edit_buffer_insert(&eb->buffer, eb->buffer.allocated, str, len);

    if (res) {
        zr_size l = zr_utf_len(str, len);
        eb->glyphs += l;
        eb->cursor += l;
        eb->text_inserted = 1;
    }
}

static zr_size
zr_edit_box_buffer_input(struct zr_edit_box *box, const struct zr_input *i)
{
    zr_rune unicode;
    zr_size src_len = 0;
    zr_size text_len = 0, glyph_len = 0;
    zr_size glyphs = 0;

    ZR_ASSERT(box);
    ZR_ASSERT(i);
    if (!box || !i) return 0;

    /* add user provided text to buffer until either no input or buffer space left*/
    glyph_len = zr_utf_decode(i->keyboard.text, &unicode, i->keyboard.text_len);
    while (glyph_len && ((text_len+glyph_len) <= i->keyboard.text_len)) {
        /* filter to make sure the value is correct */
        if (box->filter(box, unicode)) {
            zr_edit_box_add(box, &i->keyboard.text[text_len], glyph_len);
            text_len += glyph_len;
            glyphs++;
        }
        src_len = src_len + glyph_len;
        glyph_len = zr_utf_decode(i->keyboard.text + src_len, &unicode,
            i->keyboard.text_len - src_len);
    }
    return glyphs;
}

void
zr_edit_box_remove(struct zr_edit_box *box, enum zr_edit_remove_operation op)
{
    zr_size len;
    char *buf;
    zr_size min, maxi, diff;
    ZR_ASSERT(box);
    if (!box) return;
    if (!box->glyphs) return;

    buf = (char*)box->buffer.memory.ptr;
    min = MIN(box->sel.end, box->sel.begin);
    maxi = MAX(box->sel.end, box->sel.begin);
    diff = MAX(1, maxi - min);

    if (diff && box->cursor != box->glyphs) {
        zr_size off;
        zr_rune unicode;
        char *begin, *end;

        /* calculate text selection byte position and size */
        begin = zr_edit_buffer_at(&box->buffer, (int)min, &unicode, &len);
        end = zr_edit_buffer_at(&box->buffer, (int)maxi, &unicode, &len);
        len = MAX((zr_size)(end - begin), 1);
        off = (zr_size)(begin - buf);

        /* delete text selection */
        if (len > 1) {
            zr_edit_buffer_del(&box->buffer, off, len);
        } else {
            if (op == ZR_DELETE) {
                zr_edit_buffer_del(&box->buffer, off, len);
            } else {
                zr_edit_buffer_del(&box->buffer, off-1, len);
                box->cursor = (box->cursor) ? box->cursor-1: 0;
                box->sel.begin = box->cursor;
                box->sel.end = box->cursor;
            }
        }
        box->glyphs = zr_utf_len(buf, box->buffer.allocated);
    } else if (op == ZR_REMOVE) {
        zr_rune unicode;
        int cursor;
        char *glyph;
        zr_size offset;

        /* remove last glyph */
        cursor = (int)box->cursor - 1;
        glyph = zr_edit_buffer_at(&box->buffer, cursor, &unicode, &len);
        if (!glyph || !len) return;

        offset = (zr_size)(glyph - (char*)box->buffer.memory.ptr);
        zr_edit_buffer_del(&box->buffer, offset, len);
        box->glyphs--;
    }
    if (op == ZR_DELETE) {
        if (box->cursor >= box->glyphs)
            box->cursor = box->glyphs;
        else if (box->cursor > 0)
            box->cursor--;
    }
}

int
zr_edit_box_has_selection(const struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    if (!eb) return 0;
    return (int)(eb->sel.end - eb->sel.begin);
}

const char*
zr_edit_box_get_selection(zr_size *len, struct zr_edit_box *eb)
{
    zr_size l;
    const char *begin;
    zr_rune unicode;
    ZR_ASSERT(eb);
    ZR_ASSERT(len);
    if (!eb || !len || !(eb->sel.end - eb->sel.begin))
        return 0;

    begin =  zr_edit_buffer_at(&eb->buffer, (int)eb->sel.begin, &unicode, &l);
    *len = (eb->sel.end - eb->sel.begin) + 1;
    return begin;
}

char*
zr_edit_box_get(struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    if (!eb) return 0;
    return (char*)eb->buffer.memory.ptr;
}

const char*
zr_edit_box_get_const(const struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    if (!eb) return 0;
    return (char*)eb->buffer.memory.ptr;
}

char
zr_edit_box_at_char(struct zr_edit_box *eb, zr_size pos)
{
    char *c;
    ZR_ASSERT(eb);
    if (!eb || pos >= eb->buffer.allocated) return 0;
    c = zr_edit_buffer_at_char(&eb->buffer, pos);
    return c ? *c : 0;
}

void
zr_edit_box_at(struct zr_edit_box *eb, zr_size pos, zr_glyph g, zr_size *len)
{
    char *sym;
    zr_rune unicode;

    ZR_ASSERT(eb);
    ZR_ASSERT(g);
    ZR_ASSERT(len);

    if (!eb || !len || !g) return;
    if (pos >= eb->glyphs) {
        *len = 0;
        return;
    }
    sym = zr_edit_buffer_at(&eb->buffer, (int)pos, &unicode, len);
    if (!sym) return;
    zr_memcopy(g, sym, *len);
}

void
zr_edit_box_at_cursor(struct zr_edit_box *eb, zr_glyph g, zr_size *len)
{
    const char *text;
    zr_size text_len;
    zr_size glyph_len = 0;

    ZR_ASSERT(eb);
    ZR_ASSERT(g);
    ZR_ASSERT(len);

    if (!eb || !g || !len) return;
    if (!eb->cursor) {
        *len = 0;
        return;
    }

    text = (char*)eb->buffer.memory.ptr;
    text_len = eb->buffer.allocated;
    glyph_len = zr_utf_len(text, text_len);
    zr_edit_box_at(eb, glyph_len, g, len);
}

void
zr_edit_box_set_cursor(struct zr_edit_box *eb, zr_size pos)
{
    ZR_ASSERT(eb);
    ZR_ASSERT(eb->glyphs >= pos);
    if (!eb || pos > eb->glyphs) return;
    eb->cursor = pos;
}

zr_size
zr_edit_box_get_cursor(struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    if (!eb) return 0;
    return eb->cursor;
}

zr_size
zr_edit_box_len_char(struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    return eb->buffer.allocated;
}

zr_size
zr_edit_box_len(struct zr_edit_box *eb)
{
    ZR_ASSERT(eb);
    return eb->glyphs;
}

/* ===============================================================
 *
 *                          TEXT
 *
 * ===============================================================*/
struct zr_text {
    struct zr_vec2 padding;
    struct zr_color background;
    struct zr_color text;
};

static void
zr_widget_text(struct zr_command_buffer *o, struct zr_rect b,
    const char *string, zr_size len, const struct zr_text *t,
    zr_flags a, const struct zr_user_font *f)
{
    struct zr_rect label;
    zr_size text_width;

    ZR_ASSERT(o);
    ZR_ASSERT(t);
    if (!o || !t) return;

    b.h = MAX(b.h, 2 * t->padding.y);
    label.x = 0; label.w = 0;
    label.y = b.y + t->padding.y;
    label.h = b.h - 2 * t->padding.y;

    text_width = f->width(f->userdata, f->height, (const char*)string, len);
    text_width += (zr_size)(2 * t->padding.x);

    /* align in x-axis */
    if (a & ZR_TEXT_LEFT) {
        label.x = b.x + t->padding.x;
        label.w = MAX(0, b.w - 2 * t->padding.x);
    } else if (a & ZR_TEXT_CENTERED) {
        label.w = MAX(1, 2 * t->padding.x + (float)text_width);
        label.x = (b.x + t->padding.x + ((b.w - 2 * t->padding.x) - label.w) / 2);
        label.x = MAX(b.x + t->padding.x, label.x);
        label.w = MIN(b.x + b.w, label.x + label.w);
        if (label.w >= label.x) label.w -= label.x;
    } else if (a & ZR_TEXT_RIGHT) {
        label.x = MAX(b.x + t->padding.x, (b.x + b.w) - (2 * t->padding.x + (float)text_width));
        label.w = (float)text_width + 2 * t->padding.x;
    } else return;

    /* align in y-axis */
    if (a & ZR_TEXT_MIDDLE) {
        label.y = b.y + b.h/2.0f - (float)f->height/2.0f;
        label.h = b.h - (b.h/2.0f + f->height/2.0f);
    } else if (a & ZR_TEXT_BOTTOM) {
        label.y = b.y + b.h - f->height;
        label.h = f->height;
    }
    zr_draw_text(o, label, (const char*)string,
        len, f, t->background, t->text);
}

static void
zr_widget_text_wrap(struct zr_command_buffer *o, struct zr_rect b,
    const char *string, zr_size len, const struct zr_text *t,
    const struct zr_user_font *f)
{
    float width;
    zr_size glyphs = 0;
    zr_size fitting = 0;
    zr_size done = 0;
    struct zr_rect line;
    struct zr_text text;

    ZR_ASSERT(o);
    ZR_ASSERT(t);
    if (!o || !t) return;

    text.padding = zr_vec2(0,0);
    text.background = t->background;
    text.text = t->text;

    b.w = MAX(b.w, 2 * t->padding.x);
    b.h = MAX(b.h, 2 * t->padding.y);
    b.h = b.h - 2 * t->padding.y;

    line.x = b.x + t->padding.x;
    line.y = b.y + t->padding.y;
    line.w = b.w - 2 * t->padding.x;
    line.h = 2 * t->padding.y + f->height;

    fitting = zr_use_font_glyph_clamp(f, string, len, line.w, &glyphs, &width);
    while (done < len) {
        if (!fitting || line.y + line.h >= (b.y + b.h)) break;
        zr_widget_text(o, line, &string[done], fitting, &text, ZR_TEXT_LEFT, f);
        done += fitting;
        line.y += f->height + 2 * t->padding.y;
        fitting = zr_use_font_glyph_clamp(f, &string[done], len - done,
                                            line.w, &glyphs, &width);
    }
}

/* ===============================================================
 *
 *                          BUTTON
 *
 * ===============================================================*/
struct zr_button {
    float border_width;
    float rounding;
    struct zr_vec2 padding;
    struct zr_vec2 touch_pad;
    struct zr_color border;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
};

struct zr_button_text {
    struct zr_button base;
    zr_flags alignment;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
};

struct zr_button_symbol {
    struct zr_button base;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
};

struct zr_button_icon {
    struct zr_button base;
    struct zr_vec2 padding;
};

struct zr_symbol {
    enum zr_symbol_type type;
    struct zr_color background;
    struct zr_color foreground;
    float border_width;
};

static void
zr_draw_symbol(struct zr_command_buffer *out, const struct zr_symbol *sym,
    struct zr_rect content, const struct zr_user_font *font)
{
    switch (sym->type) {
    case ZR_SYMBOL_X:
    case ZR_SYMBOL_UNDERSCORE:
    case ZR_SYMBOL_PLUS:
    case ZR_SYMBOL_MINUS: {
        /* single character text symbol */
        const char *X = (sym->type == ZR_SYMBOL_X) ? "x":
            (sym->type == ZR_SYMBOL_UNDERSCORE) ? "_":
            (sym->type == ZR_SYMBOL_PLUS) ? "+": "-";
        struct zr_text text;
        text.padding = zr_vec2(0,0);
        text.background = sym->background;
        text.text = sym->foreground;
        zr_widget_text(out, content, X, 1, &text, ZR_TEXT_CENTERED, font);
    } break;
    case ZR_SYMBOL_CIRCLE:
    case ZR_SYMBOL_CIRCLE_FILLED:
    case ZR_SYMBOL_RECT:
    case ZR_SYMBOL_RECT_FILLED: {
        /* simple empty/filled shapes */
        if (sym->type == ZR_SYMBOL_RECT || sym->type == ZR_SYMBOL_RECT_FILLED) {
            zr_draw_rect(out, content,  0, sym->foreground);
            if (sym->type == ZR_SYMBOL_RECT_FILLED)
                zr_draw_rect(out, zr_shrink_rect(content,
                    sym->border_width), 0, sym->background);
        } else {
            zr_draw_circle(out, content, sym->foreground);
            if (sym->type == ZR_SYMBOL_CIRCLE_FILLED)
                zr_draw_circle(out, zr_shrink_rect(content, 1),
                    sym->background);
        }
    } break;
    case ZR_SYMBOL_TRIANGLE_UP:
    case ZR_SYMBOL_TRIANGLE_DOWN:
    case ZR_SYMBOL_TRIANGLE_LEFT:
    case ZR_SYMBOL_TRIANGLE_RIGHT: {
        enum zr_heading heading;
        struct zr_vec2 points[3];
        heading = (sym->type == ZR_SYMBOL_TRIANGLE_RIGHT) ? ZR_RIGHT :
            (sym->type == ZR_SYMBOL_TRIANGLE_LEFT) ? ZR_LEFT:
            (sym->type == ZR_SYMBOL_TRIANGLE_UP) ? ZR_UP: ZR_DOWN;
        zr_triangle_from_direction(points, content, 0, 0, heading);
        zr_draw_triangle(out,  points[0].x, points[0].y,
            points[1].x, points[1].y, points[2].x, points[2].y, sym->foreground);
    } break;
    default:
    case ZR_SYMBOL_MAX: break;
    }
}

static int
zr_button_behavior(enum zr_widget_status *state, struct zr_rect r,
    const struct zr_input *i, enum zr_button_behavior behavior)
{
    int ret = 0;
    *state = ZR_INACTIVE;
    if (!i) return 0;
    if (zr_input_is_mouse_hovering_rect(i, r)) {
        *state = ZR_HOVERED;
        if (zr_input_is_mouse_down(i, ZR_BUTTON_LEFT))
            *state = ZR_ACTIVE;
        if (zr_input_has_mouse_click_in_rect(i, ZR_BUTTON_LEFT, r)) {
            ret = (behavior != ZR_BUTTON_DEFAULT) ?
                zr_input_is_mouse_down(i, ZR_BUTTON_LEFT):
                zr_input_is_mouse_released(i, ZR_BUTTON_LEFT);
        }
    }
    return ret;
}

static void
zr_button_draw(struct zr_command_buffer *o, struct zr_rect r,
    const struct zr_button *b, enum zr_widget_status state)
{
    struct zr_color background;
    switch (state) {
    default:
    case ZR_INACTIVE:
        background = b->normal; break;
    case ZR_HOVERED:
        background = b->hover; break;
    case ZR_ACTIVE:
        background = b->active; break;
    }
    zr_draw_rect(o, r, b->rounding, b->border);
    zr_draw_rect(o, zr_shrink_rect(r, b->border_width),
                                    b->rounding, background);
}

static int
zr_do_button(enum zr_widget_status *state,
    struct zr_command_buffer *o, struct zr_rect r,
    const struct zr_button *b, const struct zr_input *i,
    enum zr_button_behavior behavior, struct zr_rect *content)
{
    int ret = zr_false;
    struct zr_vec2 pad;
    struct zr_rect bounds;
    ZR_ASSERT(b);
    if (!o || !b)
        return zr_false;

    /* calculate button content space */
    pad.x = b->padding.x + b->border_width;
    pad.y = b->padding.y + b->border_width;
    *content = zr_pad_rect(r, pad);

    /* execute and draw button */
    bounds.x = r.x - b->touch_pad.x;
    bounds.y = r.y - b->touch_pad.y;
    bounds.w = r.w + 2 * b->touch_pad.x;
    bounds.h = r.h + 2 * b->touch_pad.y;
    ret = zr_button_behavior(state, bounds, i, behavior);
    zr_button_draw(o, r, b, *state);
    return ret;
}

static int
zr_do_button_text(enum zr_widget_status *state,
    struct zr_command_buffer *o, struct zr_rect r,
    const char *string, enum zr_button_behavior behavior,
    const struct zr_button_text *b, const struct zr_input *i,
    const struct zr_user_font *f)
{
    struct zr_text t;
    struct zr_rect content;
    int ret = zr_false;

    ZR_ASSERT(b);
    ZR_ASSERT(o);
    ZR_ASSERT(string);
    ZR_ASSERT(f);
    if (!o || !b || !f)
        return zr_false;

    ret = zr_do_button(state, o, r, &b->base, i, behavior, &content);
    switch (*state) {
    default:
    case ZR_INACTIVE:
        t.background = b->base.normal;
        t.text = b->normal;
        break;
    case ZR_HOVERED:
        t.background = b->base.hover;
        t.text = b->hover;
        break;
    case ZR_ACTIVE:
        t.background = b->base.active;
        t.text = b->active;
        break;
    }
    t.padding = zr_vec2(0,0);
    zr_widget_text(o, content, string, zr_strsiz(string), &t, b->alignment, f);
    return ret;
}

static int
zr_do_button_symbol(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    enum zr_symbol_type symbol, enum zr_button_behavior bh,
    const struct zr_button_symbol *b, const struct zr_input *in,
    const struct zr_user_font *font)
{
    struct zr_symbol sym;
    struct zr_color background;
    struct zr_color color;
    struct zr_rect content;
    int ret;

    ZR_ASSERT(b);
    ZR_ASSERT(out);
    if (!out || !b)
        return zr_false;

    ret = zr_do_button(state, out, r, &b->base, in, bh, &content);
    switch (*state) {
    default:
    case ZR_INACTIVE:
        background = b->base.normal;
        color = b->normal;
        break;
    case ZR_HOVERED:
        background = b->base.hover;
        color = b->hover;
        break;
    case ZR_ACTIVE:
        background = b->base.active;
        color = b->active;
        break;
    }

    sym.type = symbol;
    sym.background = background;
    sym.foreground = color;
    sym.border_width = b->base.border_width;
    zr_draw_symbol(out, &sym, content, font);
    return ret;
}

static int
zr_do_button_image(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    struct zr_image img, enum zr_button_behavior b,
    const struct zr_button_icon *button, const struct zr_input *in)
{
    int pressed;
    struct zr_rect bounds;

    ZR_ASSERT(button);
    ZR_ASSERT(out);
    if (!out || !button)
        return zr_false;

    pressed = zr_do_button(state, out, r, &button->base, in, b, &bounds);
    zr_draw_image(out, bounds, &img);
    return pressed;
}

static int
zr_do_button_text_symbol(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    enum zr_symbol_type symbol, const char *text, zr_flags align,
    enum zr_button_behavior behavior, const struct zr_button_text *b,
    const struct zr_user_font *f, const struct zr_input *i)
{
    int ret;
    struct zr_rect tri = {0,0,0,0};
    struct zr_color background, color;
    struct zr_symbol sym;

    ZR_ASSERT(b);
    ZR_ASSERT(out);
    if (!out || !b)
        return zr_false;

    ret = zr_do_button_text(state, out, r, text, behavior, b, i, f);
    switch (*state) {
    default:
    case ZR_INACTIVE:
        background = b->base.normal;
        color = b->normal;
        break;
    case ZR_HOVERED:
        background = b->base.hover;
        color = b->hover;
        break;
    case ZR_ACTIVE:
        background = b->base.active;
        color = b->active;
        break;
    }

    /* calculate symbol bounds */
    tri.y = r.y + (r.h/2) - f->height/2;
    tri.w = f->height; tri.h = f->height;
    if (align == ZR_TEXT_LEFT) {
        tri.x = (r.x + r.w) - (2 * b->base.padding.x + tri.w);
        tri.x = MAX(tri.x, 0);
    } else tri.x = r.x + 2 * b->base.padding.x;

    sym.type = symbol;
    sym.background = background;
    sym.foreground = color;
    sym.border_width = 1.0f;
    zr_draw_symbol(out, &sym, tri, f);
    return ret;
}

static int
zr_do_button_text_image(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    struct zr_image img, const char* text, zr_flags align,
    enum zr_button_behavior behavior, const struct zr_button_text *b,
    const struct zr_user_font *f, const struct zr_input *i)
{
    int pressed;
    struct zr_rect icon;
    ZR_ASSERT(b);
    ZR_ASSERT(out);
    if (!out || !b)
        return zr_false;

    pressed = zr_do_button_text(state, out, r, text, behavior, b, i, f);
    icon.y = r.y + b->base.padding.y;
    icon.w = icon.h = r.h - 2 * b->base.padding.y;
    if (align == ZR_TEXT_LEFT) {
        icon.x = (r.x + r.w) - (2 * b->base.padding.x + icon.w);
        icon.x = MAX(icon.x, 0);
    } else icon.x = r.x + 2 * b->base.padding.x;
    zr_draw_image(out, icon, &img);
    return pressed;
}

/* ===============================================================
 *
 *                          TOGGLE
 *
 * ===============================================================*/
enum zr_toggle_type {
    ZR_TOGGLE_CHECK,
    ZR_TOGGLE_OPTION
};

struct zr_toggle {
    float rounding;
    struct zr_vec2 touch_pad;
    struct zr_vec2 padding;
    struct zr_color font;
    struct zr_color font_background;
    struct zr_color background;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color cursor;
};

static int
zr_toggle_behavior(const struct zr_input *in, struct zr_rect select,
    enum zr_widget_status *state, int active)
{
    *state = ZR_INACTIVE;
    if (in && zr_input_is_mouse_hovering_rect(in, select))
        *state = ZR_HOVERED;
    if (zr_input_mouse_clicked(in, ZR_BUTTON_LEFT, select)) {
        *state = ZR_ACTIVE;
        active = !active;
    }
    return active;
}

static void
zr_toggle_draw(struct zr_command_buffer *out,
    enum zr_widget_status state,
    const struct zr_toggle *toggle, int active,
    enum zr_toggle_type type, struct zr_rect r,
    const char *string, const struct zr_user_font *font)
{
    float cursor_pad;
    struct zr_color col;
    struct zr_rect select;
    struct zr_rect cursor;

    select.w = MIN(r.h, font->height + toggle->padding.y);
    select.h = select.w;
    select.x = r.x + toggle->padding.x;
    select.y = (r.y + toggle->padding.y + (select.w / 2)) - (font->height / 2);
    cursor_pad = (type == ZR_TOGGLE_OPTION) ?
        (float)(int)(select.w / 4):
        (float)(int)(select.h / 6);

    /* calculate the bounds of the cursor inside the toggle */
    select.h = MAX(select.w, cursor_pad * 2);
    cursor.h = select.h - cursor_pad * 2;
    cursor.w = cursor.h;
    cursor.x = select.x + cursor_pad;
    cursor.y = select.y + cursor_pad;

    if (state == ZR_HOVERED || state == ZR_ACTIVE)
        col = toggle->hover;
    else col = toggle->normal;

    /* draw radiobutton/checkbox background */
    if (type == ZR_TOGGLE_CHECK)
        zr_draw_rect(out, select , toggle->rounding, col);
    else zr_draw_circle(out, select, col);

    /* draw radiobutton/checkbox cursor if active */
    if (active) {
        if (type == ZR_TOGGLE_CHECK)
            zr_draw_rect(out, cursor, toggle->rounding, toggle->cursor);
        else zr_draw_circle(out, cursor, toggle->cursor);
    }

    /* draw toggle text */
    if (string) {
        struct zr_text text;
        struct zr_rect inner;

        /* calculate text bounds */
        inner.x = r.x + select.w + toggle->padding.x * 2;
        inner.y = select.y;
        inner.w = MAX(r.x + r.w, inner.x + toggle->padding.x);
        inner.w -= (inner.x + toggle->padding.x);
        inner.h = select.w;

        /* draw text */
        text.padding.x = 0;
        text.padding.y = 0;
        text.background = toggle->font_background;
        text.text = toggle->font;
        zr_widget_text(out, inner, string, zr_strsiz(string),
                        &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);
    }
}

static void
zr_do_toggle(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    int *active, const char *string, enum zr_toggle_type type,
    const struct zr_toggle *toggle, const struct zr_input *in,
    const struct zr_user_font *font)
{
    struct zr_rect bounds;
    ZR_ASSERT(toggle);
    ZR_ASSERT(out);
    ZR_ASSERT(font);
    if (!out || !toggle || !font || !active)
        return;

    r.w = MAX(r.w, font->height + 2 * toggle->padding.x);
    r.h = MAX(r.h, font->height + 2 * toggle->padding.y);

    bounds.x = r.x - toggle->touch_pad.x;
    bounds.y = r.y - toggle->touch_pad.y;
    bounds.w = r.w + 2 * toggle->touch_pad.x;
    bounds.h = r.h + 2 * toggle->touch_pad.y;

    *active = zr_toggle_behavior(in, bounds, state, *active);
    zr_toggle_draw(out, *state, toggle, *active, type, r, string, font);
}

/* ===============================================================
 *
 *                          SLIDER
 *
 * ===============================================================*/
struct zr_slider {
    struct zr_vec2 padding;
    struct zr_color border;
    struct zr_color bg;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
    float rounding;
};

static float
zr_slider_behavior(enum zr_widget_status *state, struct zr_rect *cursor,
    const struct zr_input *in, const struct zr_slider *s, struct zr_rect slider,
    float slider_min, float slider_max, float slider_value,
    float slider_step, float slider_steps)
{
    int inslider, incursor;
    inslider = in && zr_input_is_mouse_hovering_rect(in, slider);
    incursor = in && zr_input_has_mouse_click_down_in_rect(in, ZR_BUTTON_LEFT,
                                                            slider, zr_true);

    *state = (inslider) ? ZR_HOVERED: ZR_INACTIVE;
    if (in && inslider && incursor)
    {
        const float d = in->mouse.pos.x - (cursor->x + cursor->w / 2.0f);
        const float pxstep = (slider.w - (2 * s->padding.x)) / slider_steps;

        /* only update value if the next slider step is reached */
        *state = ZR_ACTIVE;
        if (ZR_ABS(d) >= pxstep) {
            float ratio = 0;
            const float steps = (float)((int)(ZR_ABS(d) / pxstep));
            slider_value += (d > 0) ? (slider_step*steps) : -(slider_step*steps);
            slider_value = CLAMP(slider_min, slider_value, slider_max);
            ratio = (slider_value - slider_min)/slider_step;
            cursor->x = slider.x + (cursor->w * ratio);
        }
    }
    return slider_value;
}

static void
zr_slider_draw(struct zr_command_buffer *out,
    enum zr_widget_status state, const struct zr_slider *s,
    struct zr_rect bar, struct zr_rect cursor,
    float slider_min, float slider_max, float slider_value)
{
    struct zr_color col;
    struct zr_rect fill;

    switch (state) {
    default:
    case ZR_INACTIVE:
        col = s->normal; break;
    case ZR_HOVERED:
        col = s->hover; break;
    case ZR_ACTIVE:
        col = s->active; break;
    }

    cursor.w = cursor.h;
    cursor.x = (slider_value <= slider_min) ? cursor.x:
        (slider_value >= slider_max) ? ((bar.x + bar.w) - cursor.w) :
        cursor.x - (cursor.w/2);

    fill.x = bar.x;
    fill.y = bar.y;
    fill.w = (cursor.x + (cursor.w/2.0f)) - bar.x;
    fill.h = bar.h;

    zr_draw_rect(out, bar, 0, s->bg);
    zr_draw_rect(out, fill, 0, col);
    zr_draw_circle(out, cursor, col);
}

static float
zr_do_slider(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect slider,
    float min, float val, float max, float step,
    const struct zr_slider *s, const struct zr_input *in)
{
    float slider_range;
    float slider_min, slider_max;
    float slider_value, slider_steps;
    float cursor_offset;
    struct zr_rect cursor;
    struct zr_rect bar;

    ZR_ASSERT(s);
    ZR_ASSERT(out);
    if (!out || !s)
        return 0;

    /* make sure the provided values are correct */
    slider.x = slider.x + s->padding.x;
    slider.y = slider.y + s->padding.y;
    slider.h = MAX(slider.h, 2 * s->padding.y);
    slider.w = MAX(slider.w, 1 + slider.h + 2 * s->padding.x);
    slider.h -= 2 * s->padding.y;
    slider.w -= 2 * s->padding.y;

    slider_max = MAX(min, max);
    slider_min = MIN(min, max);
    slider_value = CLAMP(slider_min, val, slider_max);
    slider_range = slider_max - slider_min;
    slider_steps = slider_range / step;

    /* calculate slider virtual cursor bounds */
    cursor_offset = (slider_value - slider_min) / step;
    cursor.h = slider.h;
    cursor.w = slider.w / (slider_steps + 1);
    cursor.x = slider.x + (cursor.w * cursor_offset);
    cursor.y = slider.y;

    /* calculate slider background bar bounds */
    bar.x = slider.x;
    bar.y = (slider.y + cursor.h/2) - cursor.h/8;
    bar.w = slider.w;
    bar.h = slider.h/4;

    slider_value = zr_slider_behavior(state, &cursor, in, s, slider,
        slider_min, slider_max, slider_value, step, slider_steps);
    zr_slider_draw(out, *state, s, bar, cursor, slider_min,
                    slider_max, slider_value);
    return slider_value;
}

/* ===============================================================
 *
 *                          PROGRESSBAR
 *
 * ===============================================================*/
struct zr_progress {
    struct zr_vec2 padding;
    struct zr_color border;
    struct zr_color background;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
};

static zr_size
zr_progress_behavior(enum zr_widget_status *state, const struct zr_input *in,
    struct zr_rect r, zr_size max, zr_size value, int modifiable)
{
    *state = ZR_INACTIVE;
    if (in && modifiable && zr_input_is_mouse_hovering_rect(in, r)) {
        if (zr_input_is_mouse_down(in, ZR_BUTTON_LEFT)) {
            float ratio = MAX(0, (float)(in->mouse.pos.x - r.x)) / (float)r.w;
            value = (zr_size)MAX(0,((float)max * ratio));
            *state = ZR_ACTIVE;
        } else *state = ZR_HOVERED;
    }
    if (!max) return value;
    value = MIN(value, max);
    return value;
}

static void
zr_progress_draw(struct zr_command_buffer *out, const struct zr_progress *p,
    enum zr_widget_status state, struct zr_rect r, zr_size max, zr_size value)
{
    struct zr_color col;
    float prog_scale;
    switch (state) {
    default:
    case ZR_INACTIVE:
        col = p->normal; break;
    case ZR_HOVERED:
        col = p->hover; break;
    case ZR_ACTIVE:
        col = p->active; break;
    }

    prog_scale = (float)value / (float)max;
    zr_draw_rect(out, r, 0, p->background);
    r.w = (r.w - 2) * prog_scale;
    zr_draw_rect(out, r, 0, col);
}

static zr_size
zr_do_progress(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect r,
    zr_size value, zr_size max, int modifiable,
    const struct zr_progress *prog, const struct zr_input *in)
{
    zr_size prog_value;
    ZR_ASSERT(prog);
    ZR_ASSERT(out);
    if (!out || !prog) return 0;

    r.w = MAX(r.w, 2 * prog->padding.x);
    r.h = MAX(r.h, 2 * prog->padding.y);
    r = zr_pad_rect(r, zr_vec2(prog->padding.x, prog->padding.y));

    prog_value = MIN(value, max);
    prog_value = zr_progress_behavior(state, in, r, max, prog_value, modifiable);
    zr_progress_draw(out, prog, *state, r, max, value);
    return prog_value;
}

/* ===============================================================
 *
 *                          SCROLLBAR
 *
 * ===============================================================*/
struct zr_scrollbar {
    float rounding;
    struct zr_color border;
    struct zr_color background;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
    int has_scrolling;
};

static float
zr_scrollbar_behavior(enum zr_widget_status *state, struct zr_input *in,
    const struct zr_scrollbar *s, struct zr_rect scroll,
    struct zr_rect cursor, float scroll_offset,
    float target, float scroll_step, enum zr_orientation o)
{
    int left_mouse_down, left_mouse_click_in_cursor;
    if (!in) return scroll_offset;

    *state = ZR_INACTIVE;
    left_mouse_down = in->mouse.buttons[ZR_BUTTON_LEFT].down;
    left_mouse_click_in_cursor = zr_input_has_mouse_click_down_in_rect(in,
        ZR_BUTTON_LEFT, cursor, zr_true);
    if (zr_input_is_mouse_hovering_rect(in, cursor))
        *state = ZR_HOVERED;

    if (left_mouse_down && left_mouse_click_in_cursor) {
        /* update cursor by mouse dragging */
        float pixel, delta;
        *state = ZR_ACTIVE;
        if (o == ZR_VERTICAL) {
            pixel = in->mouse.delta.y;
            delta = (pixel / scroll.h) * target;
            scroll_offset = CLAMP(0, scroll_offset + delta, target - scroll.h);
            /* This is probably one of my most distgusting hacks I have ever done.
             * This basically changes the mouse clicked position with the moving
             * cursor. This allows for better scroll behavior but resulted into me
             * having to remove const correctness for input. But in the end I believe
             * it is worth it. */
            in->mouse.buttons[ZR_BUTTON_LEFT].clicked_pos.y += in->mouse.delta.y;
        } else {
            pixel = in->mouse.delta.x;
            delta = (pixel / scroll.w) * target;
            scroll_offset = CLAMP(0, scroll_offset + delta, target - scroll.w);
            in->mouse.buttons[ZR_BUTTON_LEFT].clicked_pos.x += in->mouse.delta.x;
        }
    } else if (s->has_scrolling && ((in->mouse.scroll_delta<0) ||
            (in->mouse.scroll_delta>0))) {
        /* update cursor by mouse scrolling */
        scroll_offset = scroll_offset + scroll_step * (-in->mouse.scroll_delta);
        if (o == ZR_VERTICAL)
            scroll_offset = CLAMP(0, scroll_offset, target - scroll.h);
        else scroll_offset = CLAMP(0, scroll_offset, target - scroll.w);
    }
    return scroll_offset;
}

static void
zr_scrollbar_draw(struct zr_command_buffer *out, const struct zr_scrollbar *s,
    enum zr_widget_status state, struct zr_rect scroll, struct zr_rect cursor)
{
    struct zr_color col;
    switch (state) {
    default:
    case ZR_INACTIVE:
        col = s->normal; break;
    case ZR_HOVERED:
        col = s->hover; break;
    case ZR_ACTIVE:
        col = s->active; break;
    }
    zr_draw_rect(out, zr_shrink_rect(scroll,1), s->rounding, s->border);
    zr_draw_rect(out, scroll, s->rounding, s->background);
    zr_draw_rect(out, cursor, s->rounding, col);
}

static float
zr_do_scrollbarv(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect scroll,
    float offset, float target, float step, const struct zr_scrollbar *s,
    struct zr_input *i)
{
    struct zr_rect cursor;
    float scroll_step;
    float scroll_offset;
    float scroll_off, scroll_ratio;

    ZR_ASSERT(out);
    ZR_ASSERT(s);
    ZR_ASSERT(state);
    if (!out || !s) return 0;

    /* scrollbar background */
    scroll.w = MAX(scroll.w, 1);
    scroll.h = MAX(scroll.h, 2 * scroll.w);
    if (target <= scroll.h) return 0;

    /* calculate scrollbar constants */
    scroll_step = MIN(step, scroll.h);
    scroll_offset = MIN(offset, target - scroll.h);
    scroll_ratio = scroll.h / target;
    scroll_off = scroll_offset / target;

    /* calculate scrollbar cursor bounds */
    cursor.h = (scroll_ratio * scroll.h - 2);
    cursor.y = scroll.y + (scroll_off * scroll.h) + 1;
    cursor.w = scroll.w - 2;
    cursor.x = scroll.x + 1;

    /* draw scrollbar */
    scroll_offset = zr_scrollbar_behavior(state, i, s, scroll, cursor,
        scroll_offset, target, scroll_step, ZR_VERTICAL);
    scroll_off = scroll_offset / target;
    cursor.y = scroll.y + (scroll_off * scroll.h);
    zr_scrollbar_draw(out, s, *state, scroll, cursor);
    return scroll_offset;
}

static float
zr_do_scrollbarh(enum zr_widget_status *state,
    struct zr_command_buffer *out, struct zr_rect scroll,
    float offset, float target, float step, const struct zr_scrollbar *s,
    struct zr_input *i)
{
    struct zr_rect cursor;
    float scroll_step;
    float scroll_offset;
    float scroll_off, scroll_ratio;

    ZR_ASSERT(out);
    ZR_ASSERT(s);
    if (!out || !s) return 0;

    /* scrollbar background */
    scroll.h = MAX(scroll.h, 1);
    scroll.w = MAX(scroll.w, 2 * scroll.h);
    if (target <= scroll.w) return 0;

    /* calculate scrollbar constants */
    scroll_step = MIN(step, scroll.w);
    scroll_offset = MIN(offset, target - scroll.w);
    scroll_ratio = scroll.w / target;
    scroll_off = scroll_offset / target;

    /* calculate scrollbar cursor bounds */
    cursor.w = scroll_ratio * scroll.w - 2;
    cursor.x = scroll.x + (scroll_off * scroll.w) + 1;
    cursor.h = scroll.h - 2;
    cursor.y = scroll.y + 1;

    /* draw scrollbar */
    scroll_offset = zr_scrollbar_behavior(state, i, s, scroll, cursor,
        scroll_offset, target, scroll_step, ZR_HORIZONTAL);
    scroll_off = scroll_offset / target;
    cursor.x = scroll.x + (scroll_off * scroll.w);
    zr_scrollbar_draw(out, s, *state, scroll, cursor);
    return scroll_offset;
}

/* ===============================================================
 *
 *                          EDIT
 *
 * ===============================================================*/
struct zr_edit {
    int modifiable;
    float border_size;
    float rounding;
    float scrollbar_width;
    struct zr_vec2 padding;
    int show_cursor;
    struct zr_color background;
    struct zr_color border;
    struct zr_color cursor;
    struct zr_color text;
    struct zr_scrollbar scroll;
};

static void
zr_edit_box_handle_input(struct zr_edit_box *box, const struct zr_input *in,
                        int has_special)
{
    char *buffer = zr_edit_box_get(box);
    zr_size len = zr_edit_box_len_char(box);
    zr_size min = MIN(box->sel.end, box->sel.begin);
    zr_size maxi = MAX(box->sel.end, box->sel.begin);
    zr_size diff = maxi - min;
    int enter, tab;

    /* text manipulation */
    if (zr_input_is_key_pressed(in,ZR_KEY_DEL))
        zr_edit_box_remove(box, ZR_DELETE);
    else if (zr_input_is_key_pressed(in,ZR_KEY_BACKSPACE))
        zr_edit_box_remove(box, ZR_REMOVE);

    enter = has_special && zr_input_is_key_pressed(in, ZR_KEY_ENTER);
    tab = has_special && zr_input_is_key_pressed(in, ZR_KEY_TAB);
    if (in->keyboard.text_len || enter || tab) {
        if (diff && box->cursor != box->glyphs) {
            /* replace text selection */
            zr_edit_box_remove(box, ZR_DELETE);
            box->cursor = min;
        }
        if (enter) zr_edit_box_add(box, "\n", 1);
        else if (tab) zr_edit_box_add(box, "    ", 4);
        else zr_edit_box_buffer_input(box, in);
        box->sel.begin = box->cursor;
        box->sel.end = box->cursor;
    }

    /* cursor key movement */
    if (zr_input_is_key_pressed(in, ZR_KEY_LEFT)) {
        box->cursor = (zr_size)(MAX(0, (int)box->cursor - 1));
        box->sel.begin = box->cursor;
        box->sel.end = box->cursor;
    }
    if (zr_input_is_key_pressed(in, ZR_KEY_RIGHT) && box->cursor < box->glyphs) {
        box->cursor = MIN((!box->glyphs) ? 0 : box->glyphs, box->cursor + 1);
        box->sel.begin = box->cursor;
        box->sel.end = box->cursor;
    }

    /* copy & cut & paste functionlity */
    if (zr_input_is_key_pressed(in, ZR_KEY_PASTE) && box->clip.paste)
        box->clip.paste(box->clip.userdata, box);
    if ((zr_input_is_key_pressed(in, ZR_KEY_COPY) && box->clip.copy) ||
        (zr_input_is_key_pressed(in, ZR_KEY_CUT) && box->clip.copy)) {
        if (diff && box->cursor != box->glyphs) {
            /* copy or cut text selection */
            zr_size l;
            zr_rune unicode;
            char *begin, *end;
            begin = zr_edit_buffer_at(&box->buffer, (int)min, &unicode, &l);
            end = zr_edit_buffer_at(&box->buffer, (int)maxi, &unicode, &l);
            box->clip.copy(box->clip.userdata, begin, (zr_size)(end - begin));
            if (zr_input_is_key_pressed(in, ZR_KEY_CUT))
                zr_edit_box_remove(box, ZR_DELETE);
        } else {
            /* copy or cut complete buffer */
            box->clip.copy(box->clip.userdata, buffer, len);
            if (zr_input_is_key_pressed(in, ZR_KEY_CUT))
                zr_edit_box_clear(box);
        }
    }
}

static void
zr_widget_edit_field(struct zr_command_buffer *out, struct zr_rect r,
    struct zr_edit_box *box, const struct zr_edit *field,
    const struct zr_input *in, const struct zr_user_font *font)
{
    char *buffer;
    zr_size len;
    struct zr_text text;

    ZR_ASSERT(out);
    ZR_ASSERT(font);
    ZR_ASSERT(field);
    if (!out || !box || !field)
        return;

    r.w = MAX(r.w, 2 * field->padding.x + 2 * field->border_size);
    r.h = MAX(r.h, font->height + (2 * field->padding.y + 2 * field->border_size));

    /* draw editbox background and border */
    zr_draw_rect(out, r, field->rounding, field->border);
    zr_draw_rect(out, zr_shrink_rect(r, field->border_size),
        field->rounding, field->background);

    /* check if the editbox is activated/deactivated */
    if (in && in->mouse.buttons[ZR_BUTTON_LEFT].clicked &&
            in->mouse.buttons[ZR_BUTTON_LEFT].down)
        box->active = ZR_INBOX(in->mouse.pos.x,in->mouse.pos.y,r.x,r.y,r.w,r.h);

    /* input handling */
    if (box->active && in)
        zr_edit_box_handle_input(box, in, 0);

    buffer = zr_edit_box_get(box);
    len = zr_edit_box_len_char(box);
    {
        /* text management */
        struct zr_rect label;
        zr_size cursor_w = font->width(font->userdata,font->height,"X", 1);
        zr_size text_len = len;
        zr_size glyph_off = 0;
        zr_size glyph_cnt = 0;
        zr_size offset = 0;
        float text_width = 0;

        /* calculate text frame */
        label.w = MAX(r.w,  - 2 * field->padding.x - 2 * field->border_size);
        label.w -= 2 * field->padding.x - 2 * field->border_size;
        {
            zr_size frames = 0;
            zr_size glyphs = 0;
            zr_size frame_len = 0;
            zr_size row_len = 0;
            float space = MAX(label.w, (float)cursor_w);
            space -= (float)cursor_w;

            while (text_len) {
                frames++;
                offset += frame_len;
                frame_len = zr_user_font_glyphs_fitting_in_space(font,
                    &buffer[offset], text_len, space, &row_len, &glyphs, &text_width, 0);
                glyph_off += glyphs;
                if (glyph_off > box->cursor || !frame_len) break;
                text_len -= frame_len;
            }

            text_len = frame_len;
            glyph_cnt = glyphs;
            glyph_off = (frames <= 1) ? 0 : (glyph_off - glyphs);
            offset = (frames <= 1) ? 0 : offset;
        }

        /* set cursor by mouse click and handle text selection */
        if (in && field->show_cursor && in->mouse.buttons[ZR_BUTTON_LEFT].down && box->active) {
            const char *visible = &buffer[offset];
            float xoff = in->mouse.pos.x - (r.x + field->padding.x + field->border_size);
            if (ZR_INBOX(in->mouse.pos.x, in->mouse.pos.y, r.x, r.y, r.w, r.h))
            {
                /* text selection in the current text frame */
                zr_size glyph_index;
                zr_size glyph_pos=zr_user_font_glyph_index_at_pos(font,visible,text_len,xoff);
                if (glyph_cnt + glyph_off >= box->glyphs)
                    glyph_index = glyph_off + MIN(glyph_pos, glyph_cnt);
                else glyph_index = glyph_off + MIN(glyph_pos, glyph_cnt-1);

                if (text_len)
                    zr_edit_box_set_cursor(box, glyph_index);
                if (!box->sel.active) {
                    box->sel.active = zr_true;
                    box->sel.begin = glyph_index;
                    box->sel.end = box->sel.begin;
                } else {
                    if (box->sel.begin > glyph_index) {
                        box->sel.end = glyph_index;
                        box->sel.active = zr_true;
                    }
                }
            } else if (!ZR_INBOX(in->mouse.pos.x,in->mouse.pos.y,r.x,r.y,r.w,r.h) &&
                ZR_INBOX(in->mouse.buttons[ZR_BUTTON_LEFT].clicked_pos.x,
                    in->mouse.buttons[ZR_BUTTON_LEFT].clicked_pos.y,r.x,r.y,r.w,r.h)
                && box->cursor != box->glyphs && box->cursor > 0)
            {
                /* text selection out of the current text frame */
                zr_size glyph = ((in->mouse.pos.x > r.x) &&
                    box->cursor+1 < box->glyphs) ?
                    box->cursor+1: box->cursor-1;
                zr_edit_box_set_cursor(box, glyph);
                if (box->sel.active) {
                    box->sel.end = glyph;
                    box->sel.active = zr_true;
                }
            } else box->sel.active = zr_false;
        } else box->sel.active = zr_false;

        /* calculate the text bounds */
        label.x = r.x + field->padding.x + field->border_size;
        label.y = r.y + field->padding.y + field->border_size;
        label.h = r.h - (2 * field->padding.y + 2 * field->border_size);

        text.padding = zr_vec2(0,0);
        text.background = field->background;
        text.text = field->text;
        zr_widget_text(out, label, &buffer[offset], text_len,
            &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

        /* draw selected text */
        if (box->active && field->show_cursor) {
            if (box->cursor == box->glyphs) {
                /* draw the cursor at the end of the string */
                text_width = font->width(font->userdata, font->height,
                                        buffer + offset, text_len);
                zr_draw_rect(out, zr_rect(label.x+(float)text_width,
                        label.y, (float)cursor_w, label.h), 0, field->cursor);
            } else {
                /* draw text selection */
                zr_size l = 0, s;
                zr_rune unicode;
                char *begin, *end;
                zr_size off_begin, off_end;
                zr_size min = MIN(box->sel.end, box->sel.begin);
                zr_size maxi = MAX(box->sel.end, box->sel.begin);
                struct zr_rect clip = out->clip;

                /* calculate selection text range */
                begin = zr_edit_buffer_at(&box->buffer, (int)min, &unicode, &l);
                end = zr_edit_buffer_at(&box->buffer, (int)maxi, &unicode, &l);
                off_begin = (zr_size)(begin - (char*)box->buffer.memory.ptr);
                off_end = (zr_size)(end - (char*)box->buffer.memory.ptr);

                /* calculate selected text width */
                zr_draw_scissor(out, label);
                s = font->width(font->userdata, font->height, buffer + offset, off_begin - offset);
                label.x += (float)s;
                s = font->width(font->userdata, font->height, begin, MAX(l, off_end - off_begin));
                label.w = (float)s;

                /* draw selected text */
                zr_draw_rect(out, label, 0, field->text);
                text.padding = zr_vec2(0,0);
                text.background = field->text;
                text.text = field->background;
                zr_widget_text(out, label, begin, MAX(l, off_end - off_begin),
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);
                zr_draw_scissor(out, clip);
            }
        }
    }
}

static void
zr_widget_edit_box(struct zr_command_buffer *out, struct zr_rect r,
    struct zr_edit_box *box, const struct zr_edit *field,
    struct zr_input *in, const struct zr_user_font *font)
{
    char *buffer;
    zr_size len;
    zr_size visible_rows = 0;
    zr_size total_rows = 0;
    zr_size cursor_w;
    int prev_state;

    float total_width = 0;
    float total_height = 0;
    zr_size row_height = 0;

    ZR_ASSERT(out);
    ZR_ASSERT(font);
    ZR_ASSERT(field);
    if (!out || !box || !field)
        return;

    /* calculate usable field space */
    r.w = MAX(r.w, 2 * field->padding.x + 2 * field->border_size);
    r.h = MAX(r.h, font->height + (2 * field->padding.y + 2 * field->border_size));

    total_width = r.w - (2 * field->padding.x + 2 * field->border_size);
    total_width -= field->scrollbar_width;
    row_height = (zr_size)(font->height + field->padding.y);

    /* draw edit field background and border */
    zr_draw_rect(out, r, field->rounding, field->border);
    zr_draw_rect(out, zr_shrink_rect(r, field->border_size),
        field->rounding, field->background);

    /* check if edit box is big enough to show even a single row */
    visible_rows = (zr_size)(r.h - (2 * field->border_size + 2 * field->padding.y));
    visible_rows = (zr_size)((float)visible_rows / (font->height + field->padding.y));
    if (!visible_rows) return;

    /* check if editbox is activated/deactivated */
    prev_state = box->active;
    if (in && in->mouse.buttons[ZR_BUTTON_LEFT].clicked &&
        in->mouse.buttons[ZR_BUTTON_LEFT].down)
        box->active = ZR_INBOX(in->mouse.pos.x,in->mouse.pos.y,r.x,r.y,r.w,r.h);

    /* text input handling */
    if (box->active && in && field->modifiable)
        zr_edit_box_handle_input(box, in, 1);

    buffer = zr_edit_box_get(box);
    len = zr_edit_box_len_char(box);
    cursor_w = font->width(font->userdata,font->height,(const char*)"X", 1);
    {
        /* calulate total number of needed rows */
        zr_size glyphs = 0;
        zr_size row_off = 0;
        zr_size text_len = len;
        zr_size offset = 0;
        zr_size row_len;

        float space = total_width;
        float text_width = 0;

        while (text_len) {
            total_rows++;
            offset += row_off;
            row_off = zr_user_font_glyphs_fitting_in_space(font,
                &buffer[offset], text_len, space, &row_len, &glyphs, &text_width, 1);
            if (!row_off){
                text_len = 0;
            } else text_len -= row_off;
        }
        total_height = (float)total_rows * (float)row_height;
    }

    if (!box->active || (!prev_state && box->active)) {
        /* make sure edit box points to the end of the buffer if not active */
        if (total_rows > visible_rows)
            box->scrollbar = (float)((total_rows - visible_rows) * row_height);
        if (!prev_state && box->active) {
            box->cursor = zr_utf_len(buffer, len);
            box->sel.begin = box->cursor;
            box->sel.end = box->cursor;
        }
    }

    if ((in && in->keyboard.text_len && total_rows >= visible_rows && box->active) ||
        box->sel.active || (box->text_inserted && total_rows >= visible_rows))
    {
        /* make sure cursor is always in current visible field while writing */
        box->text_inserted = 0;
        if (box->cursor == box->glyphs && !box->sel.active) {
            /* cursor is at end of text and out of visible frame */
            float row_offset = (float)(total_rows - visible_rows);
            box->scrollbar = (font->height + field->padding.x) * row_offset;
        } else {
            /* cursor is inside text and out of visible frame */
            float text_width;
            zr_size cur_row = 0;
            zr_size glyphs = 0;
            zr_size row_off = 0;
            zr_size row_len = 0;
            zr_size text_len = len;
            zr_size offset = 0, glyph_off = 0;
            zr_size cursor = MIN(box->sel.end, box->sel.begin);
            zr_size scroll_offset = (zr_size)(box->scrollbar / (float)row_height);

            /* find cursor row */
            while (text_len) {
                offset += row_off;
                row_off = zr_user_font_glyphs_fitting_in_space(font,
                    &buffer[offset], text_len, total_width, &row_len, &glyphs, &text_width, 1);
                if ((cursor >= glyph_off && cursor < glyph_off + glyphs) || !row_off)
                    break;

                glyph_off += glyphs;
                text_len -= row_off;
                cur_row++;
            }

            if (cur_row >= visible_rows && !box->sel.active) {
                /* set visible frame to include cursor while writing */
                zr_size row_offset = (cur_row + 1) - visible_rows;
                box->scrollbar = (font->height + field->padding.x) * (float)row_offset;
            } else if (box->sel.active && scroll_offset > cur_row) {
                /* set visible frame to include cursor while selecting */
                zr_size row_offset = (scroll_offset > 0) ? scroll_offset-1: scroll_offset;
                box->scrollbar = (font->height + field->padding.x) * (float)row_offset;
            }
        }
    }
    if (box->text_inserted) {
        /* @NOTE: zr_editbox_add handler: ugly but works */
        box->sel.begin = box->cursor;
        box->sel.end = box->cursor;
        box->text_inserted = 0;
    }

    if (in && field->show_cursor && in->mouse.buttons[ZR_BUTTON_LEFT].down && box->active)
    {
        /* TEXT SELECTION */
        const char *visible = buffer;
        float xoff = in->mouse.pos.x - (r.x + field->padding.x + field->border_size);
        float yoff = in->mouse.pos.y - (r.y + field->padding.y + field->border_size);

        int in_space = (xoff >= 0 && xoff < total_width);
        int in_region = (box->sel.active && yoff < 0) ||
            (yoff >= 0 && yoff < total_height);

        if (ZR_INBOX(in->mouse.pos.x, in->mouse.pos.y, r.x, r.y, r.w, r.h) &&
            in_space && in_region)
        {
            zr_size row;
            zr_size glyph_index = 0, glyph_pos = 0;
            zr_size cur_row = 0;
            zr_size glyphs = 0;
            zr_size row_off = box->glyphs;
            zr_size row_len = 0;
            zr_size text_len = len;
            zr_size offset = 0, glyph_off = 0;
            float text_width = 0;

            /* selection beyond the current visible text rows */
            if (yoff < 0 && box->sel.active) {
                int off = ((int)yoff + (int)box->scrollbar - (int)row_height);
                int next_row =  off / (int)row_height;
                row = (next_row < 0) ? 0 : (zr_size)next_row;
            } else row = (zr_size)((yoff + box->scrollbar)/
                    (font->height + field->padding.y));

            /* find selected row */
            if (text_len) {
                while (text_len && cur_row <= row) {
                    row_off = zr_user_font_glyphs_fitting_in_space(font,
                        &buffer[offset], text_len, total_width, &row_len,
                        &glyphs, &text_width, 1);
                    if (!row_off) break;

                    glyph_off += glyphs;
                    text_len -= row_off;
                    visible += row_off;
                    offset += row_off;
                    cur_row++;
                }
                glyph_off -= glyphs;
                visible -= row_off;
            }

            /* find selected glyphs in row */
            if ((text_width + r.x + field->padding.y + field->border_size) > xoff) {
                glyph_pos = zr_user_font_glyph_index_at_pos(font, visible, row_len, xoff);
                if (glyph_pos + glyph_off >= box->glyphs)
                    glyph_index = box->glyphs;
                else glyph_index = glyph_off + MIN(glyph_pos, glyphs-1);

                zr_edit_box_set_cursor(box, glyph_index);
                if (!box->sel.active) {
                    box->sel.active = zr_true;
                    box->sel.begin = glyph_index;
                    box->sel.end = glyph_index;
                } else {
                    if (box->sel.begin > glyph_index) {
                        box->sel.end = glyph_index;
                        box->sel.active = zr_true;
                    }
                }
            }
        } else box->sel.active = zr_false;
    } else box->sel.active = zr_false;

    {
        /* SCROLLBAR */
        struct zr_rect bounds;
        float scroll_target, scroll_offset, scroll_step;
        struct zr_scrollbar scroll = field->scroll;
        enum zr_widget_status state;

        bounds.x = (r.x + r.w) - (field->scrollbar_width + field->border_size);
        bounds.y = r.y + field->border_size + field->padding.y;
        bounds.w = field->scrollbar_width;
        bounds.h = r.h - (2 * field->border_size + 2 * field->padding.y);

        scroll_offset = box->scrollbar;
        scroll_step = total_height * 0.10f;
        scroll_target = total_height;
        scroll.has_scrolling = box->active;
        box->scrollbar = zr_do_scrollbarv(&state, out, bounds, scroll_offset,
                            scroll_target, scroll_step, &scroll, in);
    }
    {
        /* DRAW TEXT */
        zr_size text_len = len;
        zr_size offset = 0;
        zr_size row_off = 0;
        zr_size row_len = 0;
        zr_size glyphs = 0;
        zr_size glyph_off = 0;
        float text_width = 0;
        struct zr_rect scissor;
        struct zr_rect clip;

        struct zr_rect label;
        struct zr_rect old_clip = out->clip;

        /* calculate clipping rect for scrollbar */
        clip = zr_shrink_rect(r, field->border_size);
        clip.x += field->padding.x;
        clip.y += field->padding.y;
        clip.w -= 2 * field->padding.x;
        clip.h -= 2 * field->padding.y;
        zr_unify(&scissor, &out->clip, clip.x, clip.y, clip.x + clip.w, clip.y + clip.h);

        /* calculate row text space */
        zr_draw_scissor(out, scissor);
        label.x = r.x + field->padding.x + field->border_size;
        label.y = (r.y + field->padding.y + field->border_size) - box->scrollbar;
        label.h = font->height + field->padding.y;

        /* draw each text row */
        while (text_len) {
            /* selection bounds */
            struct zr_text text;
            zr_size begin = MIN(box->sel.end, box->sel.begin);
            zr_size end = MAX(box->sel.end, box->sel.begin);

            offset += row_off;
            row_off = zr_user_font_glyphs_fitting_in_space(font,
                &buffer[offset], text_len, total_width, &row_len,
                &glyphs, &text_width, 1);
            label.w = text_width;
            if (!row_off || !row_len) break;

            /* draw either unselected or selected row */
            if (glyph_off <= begin && glyph_off + glyphs > begin &&
                glyph_off + glyphs <= end && box->active)
            {
                /* 1.) first case with selection beginning in current row */
                zr_size l = 0, sel_begin, sel_len;
                zr_size unselected_text_width;
                zr_rune unicode;

                /* calculate selection beginning string position */
                const char *from;
                from = zr_utf_at(&buffer[offset], row_len,
                    (int)(begin - glyph_off), &unicode, &l);
                sel_begin = (zr_size)(from - (char*)box->buffer.memory.ptr);
                sel_begin = sel_begin - offset;
                sel_len = row_len - sel_begin;

                /* draw unselected text part */
                unselected_text_width =
                    font->width(font->userdata, font->height, &buffer[offset],
                                (row_len >= sel_len) ? row_len - sel_len: 0);

                text.padding = zr_vec2(0,0);
                text.background = field->background;
                text.text = field->text;
                zr_widget_text(out, label, &buffer[offset],
                    (row_len >= sel_len) ? row_len - sel_len: 0,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                /* draw selected text part */
                label.x += (float)(unselected_text_width);
                label.w -= (float)(unselected_text_width);
                text.background = field->text;
                text.text = field->background;
                zr_draw_rect(out, label, 0, field->text);
                zr_widget_text(out, label, &buffer[offset+sel_begin],
                    sel_len, &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                label.x -= (float)unselected_text_width;
                label.w += (float)(unselected_text_width);
            } else if (glyph_off > begin && glyph_off + glyphs < end && box->active) {
                /*  2.) selection spanning over current row */
                text.padding = zr_vec2(0,0);
                text.background = field->text;
                text.text = field->background;
                zr_draw_rect(out, label, 0, field->text);
                zr_widget_text(out, label, &buffer[offset], row_len,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);
            } else if (glyph_off > begin && glyph_off + glyphs >= end &&
                    box->active && end >= glyph_off && end <= glyph_off + glyphs) {
                /* 3.) selection ending in current row */
                zr_size l = 0, sel_end, sel_len;
                zr_size selected_text_width;
                zr_rune unicode;

                /* calculate selection beginning string position */
                const char *to = zr_utf_at(&buffer[offset], row_len,
                    (int)(end - glyph_off), &unicode, &l);
                sel_end = (zr_size)(to - (char*)box->buffer.memory.ptr);
                sel_len = (sel_end - offset);
                sel_end = sel_end - offset;

                /* draw selected text part */
                selected_text_width = font->width(font->userdata, font->height,
                    &buffer[offset], sel_len);
                text.padding = zr_vec2(0,0);
                text.background = field->text;
                text.text = field->background;
                zr_draw_rect(out, label, 0, field->text);
                zr_widget_text(out, label, &buffer[offset], sel_len,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                /* draw unselected text part */
                label.x += (float)selected_text_width;
                label.w -= (float)(selected_text_width);
                text.background = field->background;
                text.text = field->text;
                zr_widget_text(out, label, &buffer[offset+sel_end],
                    (row_len >= sel_len) ? row_len - sel_len: 0,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                label.x -= (float)selected_text_width;
                label.w += (float)(selected_text_width);
            }
            else if (glyph_off <= begin && glyph_off + glyphs >= begin &&
                    box->active && glyph_off <= end && glyph_off + glyphs > end)
            {
                /* 4.) selection beginning and ending in current row */
                zr_size l = 0;
                zr_size cur_text_width;
                zr_size sel_begin, sel_end, sel_len;
                zr_rune unicode;
                float label_x = label.x;
                float label_w = label.w;
                float tmp;

                const char *from = zr_utf_at(&buffer[offset], row_len,
                    (int)(begin - glyph_off), &unicode, &l);
                const char *to = zr_utf_at(&buffer[offset], row_len,
                    (int)(end - glyph_off), &unicode, &l);

                /* calculate selection bounds and length */
                sel_begin = (zr_size)(from - (char*)box->buffer.memory.ptr);
                sel_begin = sel_begin - offset;
                sel_end = (zr_size)(to - (char*)box->buffer.memory.ptr);
                sel_end = sel_end - offset;
                sel_len = (sel_end - sel_begin);
                if (!sel_len) {
                    sel_len = zr_utf_decode(&buffer[offset+sel_begin],
                                            &unicode, row_len);
                    sel_end += zr_utf_decode(&buffer[offset+sel_end],
                                            &unicode, row_len);
                }

                /* draw beginning unselected text part */
                cur_text_width = font->width(font->userdata, font->height,
                                            &buffer[offset], sel_begin);
                text.padding = zr_vec2(0,0);
                text.background = field->background;
                text.text = field->text;
                zr_widget_text(out, label, &buffer[offset], sel_begin,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                /* draw selected text part */
                label.x += (float)cur_text_width;
                label.w -= (float)(cur_text_width);
                tmp = label.w;

                text.background = field->text;
                text.text = field->background;
                label.w = font->width(font->userdata, font->height,
                                            &buffer[offset+sel_begin], sel_len);
                zr_draw_rect(out, label, 0, field->text);
                zr_widget_text(out, label, &buffer[offset+sel_begin], sel_len,
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);
                cur_text_width = font->width(font->userdata, font->height,
                                            &buffer[offset+sel_begin], sel_len);
                label.w = tmp;

                /* draw ending unselected text part */
                label.x += (float)cur_text_width;
                label.w -= (float)(cur_text_width);
                text.background = field->background;
                text.text = field->text;
                zr_widget_text(out, label, &buffer[offset+sel_end],
                    row_len - (sel_len + sel_begin),
                    &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);

                label.x = (float)label_x;
                label.w = (float)label_w;
            } else {
                /* 5.) no selection */
                label.w = text_width;
                text.background = field->background;
                text.text = field->text;
                zr_widget_text(out, label, &buffer[offset],
                    row_len, &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, font);
            }

            glyph_off += glyphs;
            text_len -= row_off;
            label.y += font->height + field->padding.y;
        }

        /* draw the cursor at the end of the string */
        if (box->active && field->show_cursor) {
            if (box->cursor == box->glyphs) {
                if (len) label.y -= (font->height + field->padding.y);
                zr_draw_rect(out, zr_rect(label.x+(float)text_width,
                    label.y, (float)cursor_w, label.h), 0, field->cursor);
            }
        }
        zr_draw_scissor(out, old_clip);
    }
}

int zr_filter_default(const struct zr_edit_box *box, zr_rune unicode)
{(void)unicode;ZR_UNUSED(box);return zr_true;}

int
zr_filter_ascii(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if (unicode > 128) return zr_false;
    else return zr_true;
}

int
zr_filter_float(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if ((unicode < '0' || unicode > '9') && unicode != '.' && unicode != '-')
        return zr_false;
    else return zr_true;
}

int
zr_filter_decimal(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if ((unicode < '0' || unicode > '9') && unicode != '-')
        return zr_false;
    else return zr_true;
}

int
zr_filter_hex(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if ((unicode < '0' || unicode > '9') &&
        (unicode < 'a' || unicode > 'f') &&
        (unicode < 'A' || unicode > 'F'))
        return zr_false;
    else return zr_true;
}

int
zr_filter_oct(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if (unicode < '0' || unicode > '7')
        return zr_false;
    else return zr_true;
}

int
zr_filter_binary(const struct zr_edit_box *box, zr_rune unicode)
{
    ZR_UNUSED(box);
    if (unicode != '0' && unicode != '1')
        return zr_false;
    else return zr_true;
}

static zr_size
zr_widget_edit(struct zr_command_buffer *out, struct zr_rect r,
    char *buffer, zr_size len, zr_size max, int *active,
    zr_size *cursor, const struct zr_edit *field, zr_filter filter,
    const struct zr_input *in, const struct zr_user_font *font)
{
    struct zr_edit_box box;
    zr_edit_box_init(&box, buffer, max, 0, filter);

    box.buffer.allocated = len;
    box.active = *active;
    box.glyphs = zr_utf_len(buffer, len);
    if (!cursor) {
        box.cursor = box.glyphs;
    } else{
        box.cursor = MIN(*cursor, box.glyphs);
        box.sel.begin = box.cursor;
        box.sel.end = box.cursor;
    }

    zr_widget_edit_field(out, r, &box, field, in, font);
    *active = box.active;
    if (cursor)
        *cursor = box.cursor;
    return zr_edit_box_len_char(&box);
}
/* ===============================================================
 *
 *                          PROPERTY
 *
 * ===============================================================*/
enum zr_property_state {
    ZR_PROPERTY_DEFAULT,
    ZR_PROPERTY_EDIT,
    ZR_PROPERTY_DRAG
};

struct zr_property {
    float border_size;
    struct zr_vec2 padding;
    struct zr_color border;
    struct zr_color normal;
    struct zr_color hover;
    struct zr_color active;
    struct zr_color text;
    float rounding;
};

static float
zr_drag_behavior(enum zr_widget_status *state, const struct zr_input *in,
    struct zr_rect drag, float min, float val, float max, float inc_per_pixel)
{
    int left_mouse_down = in && in->mouse.buttons[ZR_BUTTON_LEFT].down;
    int left_mouse_click_in_cursor = in &&
        zr_input_has_mouse_click_down_in_rect(in, ZR_BUTTON_LEFT, drag, zr_true);

    *state = ZR_INACTIVE;
    if (zr_input_is_mouse_hovering_rect(in, drag))
        *state = ZR_HOVERED;

    if (left_mouse_down && left_mouse_click_in_cursor) {
        float delta, pixels;
        pixels = in->mouse.delta.x;
        delta = pixels * inc_per_pixel;
        val += delta;
        val = CLAMP(min, val, max);
        *state = ZR_ACTIVE;
    }
    return val;
}

static float
zr_property_behavior(enum zr_widget_status *ws, const struct zr_input *in,
    struct zr_rect property, struct zr_rect left, struct zr_rect right,
    struct zr_rect label, struct zr_rect edit, struct zr_rect empty,
    int *state, float min, float value, float max, float step, float inc_per_pixel)
{
    if (zr_button_behavior(ws, left, in, ZR_BUTTON_DEFAULT))
        value = CLAMP(min, value - step, max);
    if (zr_button_behavior(ws, right, in, ZR_BUTTON_DEFAULT))
        value = CLAMP(min, value + step, max);

    if (*state == ZR_PROPERTY_DEFAULT) {
        if (zr_button_behavior(ws, edit, in, ZR_BUTTON_DEFAULT))
            *state = ZR_PROPERTY_EDIT;
        else if (zr_input_has_mouse_click_in_rect(in, ZR_BUTTON_LEFT, label))
            *state = ZR_PROPERTY_DRAG;
        else if (zr_input_has_mouse_click_in_rect(in, ZR_BUTTON_LEFT, empty))
            *state = ZR_PROPERTY_DRAG;
    }
    if (*state == ZR_PROPERTY_DRAG) {
        value = zr_drag_behavior(ws, in, property, min, value, max, inc_per_pixel);
        if (*ws != ZR_ACTIVE) *state = ZR_PROPERTY_DEFAULT;
    }
    return value;
}

static void
zr_property_draw(struct zr_command_buffer *out,
    struct zr_property *p, struct zr_rect property,
    struct zr_rect left, struct zr_rect right,
    struct zr_rect label, const char *name, zr_size len,
    const struct zr_user_font *f)
{
    struct zr_symbol sym;
    struct zr_text text;
    /* background */
    zr_draw_rect(out, property, p->rounding, p->border);
    zr_draw_rect(out, zr_shrink_rect(property, p->border_size),
                                    p->rounding, p->normal);

    /* buttons */
    sym.type = ZR_SYMBOL_TRIANGLE_LEFT;
    sym.background = p->normal;
    sym.foreground = p->text;
    sym.border_width = 0;

    zr_draw_symbol(out, &sym, left, f);
    sym.type = ZR_SYMBOL_TRIANGLE_RIGHT;
    zr_draw_symbol(out, &sym, right, f);

    /* label */
    text.padding = zr_vec2(0,0);
    text.background = p->normal;
    text.text = p->text;
    zr_widget_text(out, label, name, len, &text, ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, f);
}

static float
zr_do_property(enum zr_widget_status *ws,
    struct zr_command_buffer *out, struct zr_rect property,
    const char *name, float min, float val, float max,
    float step, float inc_per_pixel, char *buffer, zr_size *len,
    int *state, zr_size *cursor, struct zr_property *p, zr_filter filter,
    const struct zr_input *in, const struct zr_user_font *f)
{
    zr_size size;
    char string[ZR_MAX_NUMBER_BUFFER];
    int active, old;
    zr_size num_len, name_len;
    float property_min;
    float property_max;
    float property_value;
    zr_size *length;
    char *dst = 0;

    struct zr_edit field;
    struct zr_rect left;
    struct zr_rect right;
    struct zr_rect label;
    struct zr_rect edit;
    struct zr_rect empty;

    /* make sure the provided values are correct */
    property_max = MAX(min, max);
    property_min = MIN(min, max);
    property_value = CLAMP(property_min, val, property_max);

    /* left decrement button */
    left.h = f->height/2;
    left.w = left.h;
    left.x = property.x + p->border_size + p->padding.x;
    left.y = property.y + p->border_size + property.h/2.0f - left.h/2;

    /* text label */
    name_len = zr_strsiz(name);
    size = f->width(f->userdata, f->height, name, name_len);
    label.x = left.x + left.w + p->padding.x;
    label.w = (float)size + 2 * p->padding.x;
    label.y = property.y + p->border_size;
    label.h = property.h - 2 * p->border_size;

    /* right increment button */
    right.y = left.y;
    right.w = left.w;
    right.h = left.h;
    right.x = property.x + property.w - (right.w + p->padding.x);

    /* edit */
    if (*state == ZR_PROPERTY_EDIT) {
        size = f->width(f->userdata, f->height, buffer, *len);
        length = len;
        dst = buffer;
    } else {
        zr_ftos(string, property_value);
        num_len = zr_string_float_limit(string, ZR_MAX_FLOAT_PRECISION);
        size = f->width(f->userdata, f->height, string, num_len);
        dst = string;
        length = &num_len;
    }

    edit.w =  (float)size + 2 * p->padding.x;
    edit.x = right.x - (edit.w + p->padding.x);
    edit.y = property.y + p->border_size + 1;
    edit.h = property.h - (2 * p->border_size + 2);

    /* empty left space activator */
    empty.w = edit.x - (label.x + label.w);
    empty.x = label.x + label.w;
    empty.y = property.y;
    empty.h = property.h;

    old = (*state == ZR_PROPERTY_EDIT);
    property_value = zr_property_behavior(ws, in, property, left, right, label,
                                edit, empty, state, property_min, property_value,
                                property_max, step, inc_per_pixel);
    zr_property_draw(out, p, property, left, right, label, name, name_len, f);

    /* edit field */
    field.border_size = 0;
    field.scrollbar_width = 0;
    field.rounding = 0;
    field.padding.x = 0;
    field.padding.y = 0;
    field.show_cursor = zr_true;
    field.background = p->normal;
    field.border = p->normal;
    field.cursor = p->text;
    field.text = p->text;

    active = (*state == ZR_PROPERTY_EDIT);
    if (old != ZR_PROPERTY_EDIT && active) {
        /* property has been activated so setup buffer */
        zr_memcopy(buffer, dst, *length);
        *cursor = zr_utf_len(buffer, *length);
        *len = *length;
        length = len;
        dst = buffer;
    }

    *length = zr_widget_edit(out, edit, dst, *length,
        ZR_MAX_NUMBER_BUFFER, &active, cursor, &field, filter,
        (*state == ZR_PROPERTY_EDIT) ? in: 0, f);
    if (active && zr_input_is_key_pressed(in, ZR_KEY_ENTER))
        active = !active;

    if (old && !active) {
        /* property is now not active so convert edit text to value*/
        *state = ZR_PROPERTY_DEFAULT;
        buffer[*len] = '\0';
        zr_string_float_limit(buffer, ZR_MAX_FLOAT_PRECISION);
        zr_strtof(&property_value, buffer);
        property_value = CLAMP(min, property_value, max);
    }
    return property_value;
}
/* ==============================================================
 *
 *                          INPUT
 *
 * ===============================================================*/
void
zr_input_begin(struct zr_context *ctx)
{
    zr_size i;
    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;

    in = &ctx->input;
    for (i = 0; i < ZR_BUTTON_MAX; ++i)
        in->mouse.buttons[i].clicked = 0;
    in->keyboard.text_len = 0;
    in->mouse.scroll_delta = 0;
    zr_vec2_mov(in->mouse.prev, in->mouse.pos);
    for (i = 0; i < ZR_KEY_MAX; i++)
        in->keyboard.keys[i].clicked = 0;
}

void
zr_input_motion(struct zr_context *ctx, int x, int y)
{
    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;

    in = &ctx->input;
    in->mouse.pos.x = (float)x;
    in->mouse.pos.y = (float)y;
}

void
zr_input_key(struct zr_context *ctx, enum zr_keys key, int down)
{
    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    if (in->keyboard.keys[key].down == down) return;

    in->keyboard.keys[key].down = down;
    in->keyboard.keys[key].clicked++;
}

void
zr_input_button(struct zr_context *ctx, enum zr_buttons id, int x, int y, int down)
{
    struct zr_mouse_button *btn;
    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    if (in->mouse.buttons[id].down == down) return;

    btn = &in->mouse.buttons[id];
    btn->clicked_pos.x = (float)x;
    btn->clicked_pos.y = (float)y;
    btn->down = down;
    btn->clicked++;
}

void
zr_input_scroll(struct zr_context *ctx, float y)
{
    ZR_ASSERT(ctx);
    if (!ctx) return;
    ctx->input.mouse.scroll_delta += y;
}

void
zr_input_glyph(struct zr_context *ctx, const zr_glyph glyph)
{
    zr_size len = 0;
    zr_rune unicode;

    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;

    len = zr_utf_decode(glyph, &unicode, ZR_UTF_SIZE);
    if (len && ((in->keyboard.text_len + len) < ZR_INPUT_MAX)) {
        zr_utf_encode(unicode, &in->keyboard.text[in->keyboard.text_len],
            ZR_INPUT_MAX - in->keyboard.text_len);
        in->keyboard.text_len += len;
    }
}

void
zr_input_char(struct zr_context *ctx, char c)
{
    zr_glyph glyph;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    glyph[0] = c;
    zr_input_glyph(ctx, glyph);
}

void
zr_input_unicode(struct zr_context *ctx, zr_rune unicode)
{
    zr_glyph rune;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    zr_utf_encode(unicode, rune, ZR_UTF_SIZE);
    zr_input_glyph(ctx, rune);
}

void
zr_input_end(struct zr_context *ctx)
{
    struct zr_input *in;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    in = &ctx->input;
    in->mouse.delta = zr_vec2_sub(in->mouse.pos, in->mouse.prev);
}

int
zr_input_has_mouse_click_in_rect(const struct zr_input *i, enum zr_buttons id,
    struct zr_rect b)
{
    const struct zr_mouse_button *btn;
    if (!i) return zr_false;
    btn = &i->mouse.buttons[id];
    if (!ZR_INBOX(btn->clicked_pos.x,btn->clicked_pos.y,b.x,b.y,b.w,b.h))
        return zr_false;
    return zr_true;
}

int
zr_input_has_mouse_click_down_in_rect(const struct zr_input *i, enum zr_buttons id,
    struct zr_rect b, int down)
{
    const struct zr_mouse_button *btn;
    if (!i) return zr_false;
    btn = &i->mouse.buttons[id];
    return zr_input_has_mouse_click_in_rect(i, id, b) && (btn->down == down);
}

int
zr_input_is_mouse_click_in_rect(const struct zr_input *i, enum zr_buttons id,
    struct zr_rect b)
{
    const struct zr_mouse_button *btn;
    if (!i) return zr_false;
    btn = &i->mouse.buttons[id];
    return (zr_input_has_mouse_click_down_in_rect(i, id, b, zr_false) &&
            btn->clicked) ? zr_true : zr_false;
}

int
zr_input_any_mouse_click_in_rect(const struct zr_input *in, struct zr_rect b)
{
    int i, down = 0;
    for (i = 0; i < ZR_BUTTON_MAX; ++i)
        down = down || zr_input_is_mouse_click_in_rect(in, (enum zr_buttons)i, b);
    return down;
}

int
zr_input_is_mouse_hovering_rect(const struct zr_input *i, struct zr_rect rect)
{
    if (!i) return zr_false;
    return ZR_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h);
}

int
zr_input_is_mouse_prev_hovering_rect(const struct zr_input *i, struct zr_rect rect)
{
    if (!i) return zr_false;
    return ZR_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h);
}

int
zr_input_mouse_clicked(const struct zr_input *i, enum zr_buttons id, struct zr_rect rect)
{
    if (!i) return zr_false;
    if (!zr_input_is_mouse_hovering_rect(i, rect)) return zr_false;
    return zr_input_is_mouse_click_in_rect(i, id, rect);
}

int
zr_input_is_mouse_down(const struct zr_input *i, enum zr_buttons id)
{
    if (!i) return zr_false;
    return i->mouse.buttons[id].down;
}

int
zr_input_is_mouse_pressed(const struct zr_input *i, enum zr_buttons id)
{
    const struct zr_mouse_button *b;
    if (!i) return zr_false;
    b = &i->mouse.buttons[id];
    if (b->down && b->clicked)
        return zr_true;
    return zr_false;
}

int
zr_input_is_mouse_released(const struct zr_input *i, enum zr_buttons id)
{
    if (!i) return zr_false;
    return (!i->mouse.buttons[id].down && i->mouse.buttons[id].clicked);
}

int
zr_input_is_key_pressed(const struct zr_input *i, enum zr_keys key)
{
    const struct zr_key *k;
    if (!i) return zr_false;
    k = &i->keyboard.keys[key];
    if (k->down && k->clicked)
        return zr_true;
    return zr_false;
}

int
zr_input_is_key_released(const struct zr_input *i, enum zr_keys key)
{
    const struct zr_key *k;
    if (!i) return zr_false;
    k = &i->keyboard.keys[key];
    if (!k->down && k->clicked)
        return zr_true;
    return zr_false;
}

int
zr_input_is_key_down(const struct zr_input *i, enum zr_keys key)
{
    const struct zr_key *k;
    if (!i) return zr_false;
    k = &i->keyboard.keys[key];
    if (k->down) return zr_true;
    return zr_false;
}

/* ==============================================================
 *
 *                          STYLE
 *
 * ===============================================================*/
#define ZR_STYLE_PROPERTY_MAP(PROPERTY)\
    PROPERTY(ITEM_SPACING,      4.0f, 4.0f)\
    PROPERTY(ITEM_PADDING,      4.0f, 4.0f)\
    PROPERTY(TOUCH_PADDING,     0.0f, 0.0f)\
    PROPERTY(PADDING,           8.0f, 10.0f)\
    PROPERTY(SCALER_SIZE,       16.0f, 16.0f)\
    PROPERTY(SCROLLBAR_SIZE,    10.0f, 10.0f)\
    PROPERTY(SIZE,              64.0f, 64.0f)

#define ZR_STYLE_ROUNDING_MAP(ROUNDING)\
    ROUNDING(BUTTON,    4.0f)\
    ROUNDING(SLIDER,    8.0f)\
    ROUNDING(CHECK,     0.0f)\
    ROUNDING(INPUT,     0.0f)\
    ROUNDING(PROPERTY,  10.0f)\
    ROUNDING(CHART,     4.0f)\
    ROUNDING(SCROLLBAR, 3.0f)

#define ZR_STYLE_COLOR_MAP(COLOR)\
    COLOR(TEXT,                     175, 175, 175, 255)\
    COLOR(TEXT_HOVERING,            120, 120, 120, 255)\
    COLOR(TEXT_ACTIVE,              100, 100, 100, 255)\
    COLOR(WINDOW,                   45, 45, 45, 255)\
    COLOR(HEADER,                   40, 40, 40, 255)\
    COLOR(BORDER,                   65, 65, 65, 255)\
    COLOR(BUTTON,                   50, 50, 50, 255)\
    COLOR(BUTTON_HOVER,             35, 35, 35, 255)\
    COLOR(BUTTON_ACTIVE,            40, 40, 40, 255)\
    COLOR(TOGGLE,                   100, 100, 100, 255)\
    COLOR(TOGGLE_HOVER,             120, 120, 120, 255)\
    COLOR(TOGGLE_CURSOR,            45, 45, 45, 255)\
    COLOR(SELECTABLE,               100, 100, 100, 255)\
    COLOR(SELECTABLE_HOVER,         80, 80, 80, 255)\
    COLOR(SELECTABLE_TEXT,          45, 45, 45, 255)\
    COLOR(SLIDER,                   38, 38, 38, 255)\
    COLOR(SLIDER_CURSOR,            100, 100, 100, 255)\
    COLOR(SLIDER_CURSOR_HOVER,      120, 120, 120, 255)\
    COLOR(SLIDER_CURSOR_ACTIVE,     150, 150, 150, 255)\
    COLOR(PROGRESS,                 38, 38, 38, 255)\
    COLOR(PROGRESS_CURSOR,          100, 100, 100, 255)\
    COLOR(PROGRESS_CURSOR_HOVER,    120, 120, 120, 255)\
    COLOR(PROGRESS_CURSOR_ACTIVE,   150, 150, 150, 255)\
    COLOR(PROPERTY,                 38, 38, 38, 255)\
    COLOR(PROPERTY_HOVER,           50, 50, 50, 255)\
    COLOR(PROPERTY_ACTIVE,          60, 60, 60, 255)\
    COLOR(INPUT,                    45, 45, 45, 255)\
    COLOR(INPUT_CURSOR,             100, 100, 100, 255)\
    COLOR(INPUT_TEXT,               135, 135, 135, 255)\
    COLOR(COMBO,                    45, 45, 45, 255)\
    COLOR(HISTO,                    120, 120, 120, 255)\
    COLOR(HISTO_BARS,               45, 45, 45, 255)\
    COLOR(HISTO_HIGHLIGHT,          255, 0, 0, 255)\
    COLOR(PLOT,                     120, 120, 120, 255)\
    COLOR(PLOT_LINES,               45, 45, 45, 255)\
    COLOR(PLOT_HIGHLIGHT,           255, 0, 0, 255)\
    COLOR(SCROLLBAR,                40, 40, 40, 255)\
    COLOR(SCROLLBAR_CURSOR,         100, 100, 100, 255)\
    COLOR(SCROLLBAR_CURSOR_HOVER,   120, 120, 120, 255)\
    COLOR(SCROLLBAR_CURSOR_ACTIVE,  150, 150, 150, 255)\
    COLOR(TABLE_LINES,              100, 100, 100, 255)\
    COLOR(TAB_HEADER,               40, 40, 40, 255)\
    COLOR(SCALER,                   100, 100, 100, 255)

static const char *zr_style_color_names[] = {
    #define COLOR(a,b,c,d,e) #a,
        ZR_STYLE_COLOR_MAP(COLOR)
    #undef COLOR
};
static const char *zr_style_rounding_names[] = {
    #define ROUNDING(a,b) #a,
        ZR_STYLE_ROUNDING_MAP(ROUNDING)
    #undef ROUNDING
};
static const char *zr_style_property_names[] = {
    #define PROPERTY(a,b,c) #a,
        ZR_STYLE_PROPERTY_MAP(PROPERTY)
    #undef PROPERTY
};

const char*
zr_get_color_name(enum zr_style_colors color)
{return zr_style_color_names[color];}

const char*
zr_get_rounding_name(enum zr_style_rounding rounding)
{return zr_style_rounding_names[rounding];}

const char*
zr_get_property_name(enum zr_style_properties property)
{return zr_style_property_names[property];}

static void
zr_style_default_properties(struct zr_style *style)
{
    #define PROPERTY(a,b,c) style->properties[ZR_PROPERTY_##a] = zr_vec2(b, c);
        ZR_STYLE_PROPERTY_MAP(PROPERTY)
    #undef PROPERTY
}

static void
zr_style_default_rounding(struct zr_style *style)
{
    #define ROUNDING(a,b) style->rounding[ZR_ROUNDING_##a] = b;
        ZR_STYLE_ROUNDING_MAP(ROUNDING)
    #undef ROUNDING
}

static void
zr_style_default_color(struct zr_style *style)
{
    #define COLOR(a,b,c,d,e) style->colors[ZR_COLOR_##a] = zr_rgba(b,c,d,e);
        ZR_STYLE_COLOR_MAP(COLOR)
    #undef COLOR
}

void
zr_load_default_style(struct zr_context *ctx, zr_flags flags)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (flags & ZR_DEFAULT_COLOR)
        zr_style_default_color(style);
    if (flags & ZR_DEFAULT_PROPERTIES)
        zr_style_default_properties(style);
    if (flags & ZR_DEFAULT_ROUNDING)
        zr_style_default_rounding(style);

    style->header.align = ZR_HEADER_RIGHT;
    style->header.close_symbol = 'x';
    style->header.minimize_symbol = '-';
    style->header.maximize_symbol = '+';
}

void
zr_set_font(struct zr_context *ctx, const struct zr_user_font *font)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    style->font = *font;
}

struct zr_vec2
zr_get_property(const struct zr_context *ctx, enum zr_style_properties index)
{
    const struct zr_style *style;
    static const struct zr_vec2 zero = {0,0};
    ZR_ASSERT(ctx);
    if (!ctx) return zero;
    style = &ctx->style;
    return style->properties[index];
}

struct zr_color
zr_get_color(const struct zr_context *ctx, enum zr_style_colors index)
{
    const struct zr_style *style;
    static const struct zr_color zero = {0,0,0,0};
    ZR_ASSERT(ctx);
    if (!ctx) return zero;
    style = &ctx->style;
    return style->colors[index];
}

void
zr_push_color(struct zr_context *ctx, enum zr_style_colors index,
    struct zr_color col)
{
    struct zr_style *style;
    struct zr_saved_color *c;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (style->stack.color >= ZR_MAX_COLOR_STACK) return;

    c = &style->stack.colors[style->stack.color++];
    c->value = style->colors[index];
    c->type = index;
    style->colors[index] = col;
}

void
zr_push_property(struct zr_context *ctx, enum zr_style_properties index,
    struct zr_vec2 v)
{
    struct zr_style *style;
    struct zr_saved_property *p;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (style->stack.property >= ZR_MAX_ATTRIB_STACK) return;

    p = &style->stack.properties[style->stack.property++];
    p->value = style->properties[index];
    p->type = index;
    style->properties[index] = v;
}

void
zr_push_font(struct zr_context *ctx, struct zr_user_font font)
{
    struct zr_style *style;
    struct zr_saved_font *f;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (style->stack.font >= ZR_MAX_FONT_STACK) return;

    f = &style->stack.fonts[style->stack.font++];
    f->font_height_begin = style->stack.font_height;
    f->font_height_end = style->stack.font_height;
    f->value = style->font;
    style->font = font;
}

void
zr_push_font_height(struct zr_context *ctx, float font_height)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (style->stack.font >= ZR_MAX_FONT_HEIGHT_STACK) return;

    style->stack.font_heights[style->stack.font_height++] = style->font.height;
    if (style->stack.font)
        style->stack.fonts[style->stack.font-1].font_height_end++;
    style->font.height = font_height;
}

void
zr_pop_color(struct zr_context *ctx)
{
    struct zr_style *style;
    struct zr_saved_color *c;
    ZR_ASSERT(ctx);

    if (!ctx) return;
    style = &ctx->style;
    if (!style->stack.color) return;

    c = &style->stack.colors[--style->stack.color];
    style->colors[c->type] = c->value;
}

void
zr_pop_property(struct zr_context *ctx)
{
    struct zr_style *style;
    struct zr_saved_property *p;

    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (!style->stack.property) return;

    p = &style->stack.properties[--style->stack.property];
    style->properties[p->type] = p->value;
}

void
zr_pop_font(struct zr_context *ctx)
{
    struct zr_style *style;
    struct zr_saved_font *f;

    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (!style->stack.font) return;

    f = &style->stack.fonts[--style->stack.font];
    style->stack.font_height = f->font_height_begin;
    style->font = f->value;
    if (style->stack.font_height)
        style->font.height = style->stack.font_heights[style->stack.font_height-1];
}

void
zr_pop_font_height(struct zr_context *ctx)
{
    struct zr_style *style;
    float font_height;

    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    if (!style->stack.font_height) return;

    font_height = style->stack.font_heights[--style->stack.font_height];
    style->font.height = font_height;
    if (style->stack.font) {
        ZR_ASSERT(style->stack.fonts[style->stack.font-1].font_height_end);
        style->stack.fonts[style->stack.font-1].font_height_end--;
    }
}

void
zr_reset_colors(struct zr_context *ctx)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    while (style->stack.color)
        zr_pop_color(ctx);
}

void
zr_reset_properties(struct zr_context *ctx)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    while (style->stack.property)
        zr_pop_property(ctx);
}

void
zr_reset_font(struct zr_context *ctx)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    while (style->stack.font)
        zr_pop_font(ctx);
}

void
zr_reset_font_height(struct zr_context *ctx)
{
    struct zr_style *style;
    ZR_ASSERT(ctx);
    if (!ctx) return;
    style = &ctx->style;
    while (style->stack.font_height)
        zr_pop_font_height(ctx);
}

void
zr_reset(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    if (!ctx) return;
    zr_reset_colors(ctx);
    zr_reset_properties(ctx);
    zr_reset_font(ctx);
    zr_reset_font_height(ctx);
}

/* ===============================================================
 *
 *                          POOL
 *
 * ===============================================================*/
static void
zr_pool_init(struct zr_pool *pool, struct zr_allocator *alloc,
            unsigned int capacity)
{
    zr_zero(pool, sizeof(*pool));
    pool->alloc = *alloc;
    pool->capacity = capacity;
    pool->pages = 0;
    pool->type = ZR_BUFFER_DYNAMIC;
}

static void
zr_pool_free(struct zr_pool *pool)
{
    struct zr_window_page *next;
    struct zr_window_page *iter = pool->pages;
    if (!pool) return;
    if (pool->type == ZR_BUFFER_FIXED) return;
    while (iter) {
        next = iter->next;
        pool->alloc.free(pool->alloc.userdata, iter);
        iter = next;
    }
    pool->alloc.free(pool->alloc.userdata, pool);
}

static void
zr_pool_init_fixed(struct zr_pool *pool, void *memory, zr_size size)
{
    zr_zero(pool, sizeof(*pool));
    /* make sure pages have correct granularity to at least fit one page into memory */
    if (size < sizeof(struct zr_window_page) + ZR_POOL_DEFAULT_CAPACITY * sizeof(struct zr_window))
        pool->capacity = (unsigned)(size - sizeof(struct zr_window_page)) / sizeof(struct zr_window);
    else pool->capacity = ZR_POOL_DEFAULT_CAPACITY;
    pool->pages = (struct zr_window_page*)memory;
    pool->type = ZR_BUFFER_FIXED;
    pool->size = size;
}

static void*
zr_pool_alloc(struct zr_pool *pool)
{
    if (!pool->pages || pool->pages->size >= pool->capacity) {
        /* allocate new page */
        struct zr_window_page *page;
        if (pool->type == ZR_BUFFER_FIXED) {
            if (!pool->pages) {
                ZR_ASSERT(pool->pages);
                return 0;
            }
            ZR_ASSERT(pool->pages->size < pool->capacity);
            return 0;
        } else {
            zr_size size = sizeof(struct zr_window_page);
            size += ZR_POOL_DEFAULT_CAPACITY * sizeof(union zr_page_data);
            page = (struct zr_window_page*)pool->alloc.alloc(pool->alloc.userdata, size);
            page->size = 0;
            page->next = pool->pages;
            pool->pages = page;
        }
    }
    return &pool->pages->win[pool->pages->size++];
}

/* ===============================================================
 *
 *                          CONTEXT
 *
 * ===============================================================*/
static void* zr_create_window(struct zr_context *ctx);
static void zr_free_window(struct zr_context *ctx, struct zr_window *win);
static void zr_free_table(struct zr_context *ctx, struct zr_table *tbl);
static void zr_remove_table(struct zr_window *win, struct zr_table *tbl);

static void
zr_setup(struct zr_context *ctx, const struct zr_user_font *font)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(font);
    if (!ctx || !font) return;
    zr_zero_struct(*ctx);
    zr_load_default_style(ctx, ZR_DEFAULT_ALL);
    ctx->style.font = *font;
#if ZR_COMPILE_WITH_VERTEX_BUFFER
    zr_canvas_init(&ctx->canvas);
#endif
}

int
zr_init_fixed(struct zr_context *ctx, void *memory, zr_size size,
    const struct zr_user_font *font)
{
    ZR_ASSERT(memory);
    if (!memory) return 0;
    zr_setup(ctx, font);
    zr_buffer_init_fixed(&ctx->memory, memory, size);
    ctx->pool = 0;
    return 1;
}

int
zr_init_custom(struct zr_context *ctx, struct zr_buffer *cmds,
    struct zr_buffer *pool, const struct zr_user_font *font)
{
    ZR_ASSERT(cmds);
    ZR_ASSERT(pool);
    if (!cmds || !pool) return 0;
    zr_setup(ctx, font);
    ctx->memory = *cmds;
    if (pool->type == ZR_BUFFER_FIXED) {
        /* take memory from buffer and alloc fixed pool */
        void *memory = pool->memory.ptr;
        zr_size size = pool->memory.size;
        ctx->pool = memory;
        ZR_ASSERT(size > sizeof(struct zr_pool));
        size -= sizeof(struct zr_pool);
        zr_pool_init_fixed((struct zr_pool*)ctx->pool,
            (void*)((zr_byte*)ctx->pool+sizeof(struct zr_pool)), size);
    } else {
        /* create dynamic pool from buffer allocator */
        struct zr_allocator *alloc = &pool->pool;
        ctx->pool = alloc->alloc(alloc->userdata, sizeof(struct zr_pool));
        zr_pool_init((struct zr_pool*)ctx->pool, alloc, ZR_POOL_DEFAULT_CAPACITY);
    }
    return 1;
}

int
zr_init(struct zr_context *ctx, struct zr_allocator *alloc,
    const struct zr_user_font *font)
{
    ZR_ASSERT(alloc);
    if (!alloc) return 0;
    zr_setup(ctx, font);
    zr_buffer_init(&ctx->memory, alloc, ZR_DEFAULT_COMMAND_BUFFER_SIZE);
    ctx->pool = alloc->alloc(alloc->userdata, sizeof(struct zr_pool));
    zr_pool_init((struct zr_pool*)ctx->pool, alloc, ZR_POOL_DEFAULT_CAPACITY);
    return 1;
}

#if ZR_COMPILE_WITH_COMMAND_USERDATA
void
zr_set_user_data(struct zr_context *ctx, zr_handle handle)
{
    if (!ctx) return;
    ctx->userdata = handle;
    if (ctx->current)
        ctx->current->buffer.userdata = handle;
}
#endif

void
zr_free(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    if (!ctx) return;
    zr_buffer_free(&ctx->memory);
    if (ctx->pool) zr_pool_free((struct zr_pool*)ctx->pool);

    zr_zero(&ctx->input, sizeof(ctx->input));
    zr_zero(&ctx->style, sizeof(ctx->style));
    zr_zero(&ctx->memory, sizeof(ctx->memory));

    ctx->seq = 0;
    ctx->pool = 0;
    ctx->build = 0;
    ctx->begin = 0;
    ctx->end = 0;
    ctx->active = 0;
    ctx->current = 0;
    ctx->freelist = 0;
    ctx->count = 0;
}

void
zr_clear(struct zr_context *ctx)
{
    struct zr_window *iter;
    struct zr_window *next;
    ZR_ASSERT(ctx);

    if (!ctx) return;
    if (ctx->pool)
        zr_buffer_clear(&ctx->memory);
    else zr_buffer_reset(&ctx->memory, ZR_BUFFER_FRONT);

    ctx->build = 0;
    ctx->memory.calls = 0;
#if ZR_COMPILE_WITH_VERTEX_BUFFER
    zr_canvas_clear(&ctx->canvas);
#endif

    /* garbage collector */
    iter = ctx->begin;
    while (iter) {
        /* make sure minimized windows do not get removed */
        if (iter->flags & ZR_WINDOW_MINIMIZED) {
            iter = iter->next;
            continue;
        }

        /* free unused popup windows */
        if (iter->popup.win && iter->popup.win->seq != ctx->seq) {
            zr_free_window(ctx, iter->popup.win);
            iter->popup.win = 0;
        }

        {struct zr_table *n, *it = iter->tables;
        while (it) {
            /* remove unused window state tables */
            n = it->next;
            if (it->seq != ctx->seq) {
                zr_remove_table(iter, it);
                zr_zero(it, sizeof(union zr_page_data));
                zr_free_table(ctx, it);
                if (it == iter->tables)
                    iter->tables = n;
            }
            it = n;
        }}

        /* window itself is not used anymore so free */
        if (iter->seq != ctx->seq) {
            next = iter->next;
            zr_free_window(ctx, iter);
            ctx->count--;
            iter = next;
        } else iter = iter->next;
    }
    ctx->seq++;
}

/* ----------------------------------------------------------------
 *
 *                          BUFFERING
 *
 * ---------------------------------------------------------------*/
static void
zr_start(struct zr_context *ctx, struct zr_window *win)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(win);
    if (!ctx || !win) return;
    win->buffer.begin = ctx->memory.allocated;
    win->buffer.end = win->buffer.begin;
    win->buffer.last = win->buffer.begin;
    win->buffer.clip = zr_null_rect;
}

static void
zr_start_popup(struct zr_context *ctx, struct zr_window *win)
{
    struct zr_popup_buffer *buf;
    struct zr_panel *iter;
    ZR_ASSERT(ctx);
    ZR_ASSERT(win);
    if (!ctx || !win) return;

    /* make sure to use the correct popup buffer*/
    iter = win->layout;
    while (iter->parent)
        iter = iter->parent;

    /* save buffer fill state for popup */
    buf = &iter->popup_buffer;
    buf->begin = win->buffer.end;
    buf->end = win->buffer.end;
    buf->parent = win->buffer.last;
    buf->last = buf->begin;
    buf->active = zr_true;
}

static void
zr_finish_popup(struct zr_context *ctx, struct zr_window *win)
{
    struct zr_popup_buffer *buf;
    struct zr_panel *iter;
    ZR_ASSERT(ctx);
    ZR_ASSERT(win);
    if (!ctx || !win) return;

    /* make sure to use the correct popup buffer*/
    iter = win->layout;
    while (iter->parent)
        iter = iter->parent;

    buf = &iter->popup_buffer;
    buf->last = win->buffer.last;
    buf->end = win->buffer.end;
}

static void
zr_finish(struct zr_context *ctx, struct zr_window *win)
{
    struct zr_popup_buffer *buf;
    struct zr_command *parent_last;
    struct zr_command *sublast;
    struct zr_command *last;
    void *memory;

    ZR_ASSERT(ctx);
    ZR_ASSERT(win);
    if (!ctx || !win) return;
    win->buffer.end = ctx->memory.allocated;
    if (!win->layout->popup_buffer.active) return;

    /* frome here this case is for popup windows */
    buf = &win->layout->popup_buffer;
    memory = ctx->memory.memory.ptr;

    /* redirect the sub-window buffer to the end of the window command buffer */
    parent_last = zr_ptr_add(struct zr_command, memory, buf->parent);
    sublast = zr_ptr_add(struct zr_command, memory, buf->last);
    last = zr_ptr_add(struct zr_command, memory, win->buffer.last);

    parent_last->next = buf->end;
    sublast->next = last->next;
    last->next = buf->begin;
    win->buffer.last = buf->last;
    win->buffer.end = buf->end;
    buf->active = zr_false;
}

static void
zr_build(struct zr_context *ctx)
{
    struct zr_window *iter;
    struct zr_window *next;
    struct zr_command *cmd;
    zr_byte *buffer;

    iter = ctx->begin;
    buffer = (zr_byte*)ctx->memory.memory.ptr;
    while (iter != 0) {
        next = iter->next;
        if (iter->buffer.last != iter->buffer.begin) {
            cmd = zr_ptr_add(struct zr_command, buffer, iter->buffer.last);
            while (next && next->buffer.last == next->buffer.begin)
                next = next->next; /* skip empty command buffers */

            if (next) {
                cmd->next = next->buffer.begin;
            } else cmd->next = ctx->memory.allocated;
        }
        iter = next;
    }
}

const struct zr_command*
zr__begin(struct zr_context *ctx)
{
    struct zr_window *iter;
    zr_byte *buffer;
    ZR_ASSERT(ctx);
    if (!ctx) return 0;
    if (!ctx->count) return 0;

    /* build one command list out of all windows */
    buffer = (zr_byte*)ctx->memory.memory.ptr;
    if (!ctx->build) {
        zr_build(ctx);
        ctx->build = zr_true;
    }

    iter = ctx->begin;
    while (iter && iter->buffer.begin == iter->buffer.end)
        iter = iter->next;
    if (!iter) return 0;
    return zr_ptr_add_const(struct zr_command, buffer, iter->buffer.begin);
}

const struct zr_command*
zr__next(struct zr_context *ctx, const struct zr_command *cmd)
{
    zr_byte *buffer;
    const struct zr_command *next;
    ZR_ASSERT(ctx);
    if (!ctx || !cmd || !ctx->count) return 0;
    if (cmd->next >= ctx->memory.allocated) return 0;
    buffer = (zr_byte*)ctx->memory.memory.ptr;
    next = zr_ptr_add_const(struct zr_command, buffer, cmd->next);
    return next;
}

/* ----------------------------------------------------------------
 *
 *                          TABLES
 *
 * ---------------------------------------------------------------*/
static struct zr_table*
zr_create_table(struct zr_context *ctx)
{void *tbl = (void*)zr_create_window(ctx); return (struct zr_table*)tbl;}

static void
zr_free_table(struct zr_context *ctx, struct zr_table *tbl)
{zr_free_window(ctx, (struct zr_window*)tbl);}

static void
zr_push_table(struct zr_window *win, struct zr_table *tbl)
{
    if (!win->tables) {
        win->tables = tbl;
        tbl->next = 0;
        tbl->prev = 0;
        win->table_count = 1;
        win->table_size = 1;
        return;
    }
    win->tables->prev = tbl;
    tbl->next = win->tables;
    tbl->prev = 0;
    win->tables = tbl;
    win->table_count++;
    win->table_size = 0;
}

static void
zr_remove_table(struct zr_window *win, struct zr_table *tbl)
{
    if (win->tables == tbl)
        win->tables = tbl->next;
    if (tbl->next)
        tbl->next->prev = tbl->prev;
    if (tbl->prev)
        tbl->prev->next = tbl->next;
    tbl->next = 0;
    tbl->prev = 0;
}

static zr_uint*
zr_add_value(struct zr_context *ctx, struct zr_window *win,
            zr_hash name, zr_uint value)
{
    if (!win->tables || win->table_size == ZR_VALUE_PAGE_CAPACITY) {
        struct zr_table *tbl = zr_create_table(ctx);
        zr_push_table(win, tbl);
    }
    win->tables->seq = win->seq;
    win->tables->keys[win->table_size] = name;
    win->tables->values[win->table_size] = value;
    return &win->tables->values[win->table_size++];
}

static zr_uint*
zr_find_value(struct zr_window *win, zr_hash name)
{
    unsigned short size = win->table_size;
    struct zr_table *iter = win->tables;
    while (iter) {
        unsigned short i = 0;
        for (i = 0; i < size; ++i) {
            if (iter->keys[i] == name) {
                iter->seq = win->seq;
                return &iter->values[i];
            }
        }
        size = ZR_VALUE_PAGE_CAPACITY;
        iter = iter->next;
    }
    return 0;
}
/* ----------------------------------------------------------------
 *
 *                          WINDOW
 *
 * ---------------------------------------------------------------*/
static int zr_panel_begin(struct zr_context *ctx, const char *title);
static void zr_panel_end(struct zr_context *ctx);

static void*
zr_create_window(struct zr_context *ctx)
{
    struct zr_window *win = 0;
    if (ctx->freelist) {
        /* unlink window from free list */
        win = ctx->freelist;
        ctx->freelist = win->next;
    } else if (ctx->pool) {
        /* allocate window from memory pool */
        win = (struct zr_window*) zr_pool_alloc((struct zr_pool*)ctx->pool);
        ZR_ASSERT(win);
        if (!win) return 0;
    } else {
        /* allocate new window from the back of the fixed size memory buffer */
        static const zr_size size = sizeof(union zr_page_data);
        static const zr_size align = ZR_ALIGNOF(union zr_page_data);
        win = (struct zr_window*)zr_buffer_alloc(&ctx->memory, ZR_BUFFER_BACK, size, align);
        ZR_ASSERT(win);
        if (!win) return 0;
    }
    zr_zero(win, sizeof(union zr_page_data));
    win->seq = ctx->seq;
    return win;
}

static void
zr_free_window(struct zr_context *ctx, struct zr_window *win)
{
    /* unlink windows from list */
    struct zr_table *n, *it = win->tables;

    if (win == ctx->begin) {
        ctx->begin = win->next;
        if (win->next)
            ctx->begin->prev = 0;
    } else if (win == ctx->end) {
        ctx->end = win->prev;
        if (win->prev)
            ctx->end->next = 0;
    } else {
        if (win->next)
            win->next->prev = win->next;
        if (win->prev)
            win->prev->next = win->prev;
    }
    if (win->popup.win) {
        zr_free_window(ctx, win->popup.win);
        win->popup.win = 0;
    }

    win->next = 0;
    win->prev = 0;

    while (it) {
        /*free window state tables */
        n = it->next;
        if (it->seq != ctx->seq) {
            zr_remove_table(win, it);
            zr_free_table(ctx, it);
            if (it == win->tables)
                win->tables = n;
        }
        it = n;
    }

    /* link windows into freelist */
    if (!ctx->freelist) {
        ctx->freelist = win;
    } else {
        win->next = ctx->freelist;
        ctx->freelist = win;
    }
}

static struct zr_window*
zr_find_window(struct zr_context *ctx, zr_hash hash)
{
    struct zr_window *iter;
    iter = ctx->begin;
    while (iter) {
        if (iter->name == hash)
            return iter;
        iter = iter->next;
    }
    return 0;
}

static void
zr_insert_window(struct zr_context *ctx, struct zr_window *win)
{
    struct zr_window *end;
    ZR_ASSERT(ctx);
    ZR_ASSERT(win);
    if (!win || !ctx) return;

    if (!ctx->begin) {
        win->next = 0;
        win->prev = 0;
        ctx->begin = win;
        ctx->end = win;
        ctx->count = 1;
        return;
    }

    end = ctx->end;
    end->next = win;
    win->prev = ctx->end;
    win->next = 0;
    ctx->end = win;
    ctx->count++;
}

static void
zr_remove_window(struct zr_context *ctx, struct zr_window *win)
{
    if (win->prev)
        win->prev->next = win->next;
    if (win->next)
        win->next->prev = win->prev;
    if (ctx->begin == win)
        ctx->begin = win->next;
    if (ctx->end == win)
        ctx->end = win->prev;

    win->next = 0;
    win->prev = 0;
    ctx->count--;
}

int
zr_begin(struct zr_context *ctx, struct zr_panel *layout,
    const char *title, struct zr_rect bounds, zr_flags flags)
{
    struct zr_window *win;
    zr_hash title_hash;
    int title_len;
    int ret = 0;

    ZR_ASSERT(ctx);
    ZR_ASSERT(title);
    ZR_ASSERT(!ctx->current && "if this triggers you missed a `zr_end` call");
    if (!ctx || ctx->current || !title)
        return 0;

    /* find or create window */
    title_len = (int)zr_strsiz(title);
    title_hash = zr_murmur_hash(title, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) {
        win = (struct zr_window*)zr_create_window(ctx);
        zr_insert_window(ctx, win);
        zr_command_buffer_init(&win->buffer, &ctx->memory, ZR_CLIPPING_ON);
        ZR_ASSERT(win);
        if (!win) return 0;

        win->flags = flags;
        win->bounds = bounds;
        win->name = title_hash;
        win->popup.win = 0;
    } else {
        /* update public window flags */
        win->flags &= ~(zr_flags)(ZR_WINDOW_PRIVATE-1);
        win->flags |= flags;
        win->seq++;
    }
    if (win->flags & ZR_WINDOW_HIDDEN) return 0;

    /* overlapping window */
    if (!(win->flags & ZR_WINDOW_SUB) && !(win->flags & ZR_WINDOW_HIDDEN))
    {
        int inpanel, ishovered;
        const struct zr_window *iter = win;

        /* This is so terrible but neccessary for minimized windows. The difference
         * lies in the size of the window. But it is not possible to get the size
         * without cheating because I do not have the information at this point.
         * Even worse this is wrong since windows could have different window heights.
         * I leave it in for now since I otherwise loose my mind right now. */
        struct zr_vec2 window_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);
        struct zr_vec2 item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
        float h = ctx->style.font.height + 2 * item_padding.y + window_padding.y;

        /* activate window if hovered and no other window is overlapping this window */
        zr_start(ctx, win);
        inpanel = zr_input_mouse_clicked(&ctx->input, ZR_BUTTON_LEFT, win->bounds);
        ishovered = zr_input_is_mouse_hovering_rect(&ctx->input, win->bounds);
        if ((win != ctx->active) && ishovered) {
            iter = win->next;
            while (iter) {
                if (!(iter->flags & ZR_WINDOW_MINIMIZED)) {
                    if (ZR_INTERSECT(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h,
                        iter->bounds.x, iter->bounds.y, iter->bounds.w, iter->bounds.h) &&
                        !(iter->flags & ZR_WINDOW_HIDDEN))
                        break;
                } else {
                    if (ZR_INTERSECT(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h,
                        iter->bounds.x, iter->bounds.y, iter->bounds.w, h) &&
                        !(iter->flags & ZR_WINDOW_HIDDEN))
                        break;
                }
                if (iter->popup.win && iter->popup.active && !(iter->flags & ZR_WINDOW_HIDDEN) &&
                    ZR_INTERSECT(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h,
                    iter->popup.win->bounds.x, iter->popup.win->bounds.y,
                    iter->popup.win->bounds.w, iter->popup.win->bounds.h))
                    break;
                iter = iter->next;
            }
        }

        /* activate window if clicked */
        if (iter && inpanel && (win != ctx->end)) {
            iter = win->next;
            while (iter) {
                /* try to find a panel with higher priorty in the same position */
                if (!(iter->flags & ZR_WINDOW_MINIMIZED)) {
                    if (ZR_INBOX(ctx->input.mouse.prev.x, ctx->input.mouse.prev.y, iter->bounds.x,
                        iter->bounds.y, iter->bounds.w, iter->bounds.h) &&
                        !(iter->flags & ZR_WINDOW_HIDDEN))
                        break;
                } else {
                    if (ZR_INBOX(ctx->input.mouse.prev.x, ctx->input.mouse.prev.y, iter->bounds.x,
                        iter->bounds.y, iter->bounds.w, h) &&
                        !(iter->flags & ZR_WINDOW_HIDDEN))
                        break;
                }
                if (iter->popup.win && iter->popup.active && !(iter->flags & ZR_WINDOW_HIDDEN) &&
                    ZR_INTERSECT(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h,
                    iter->popup.win->bounds.x, iter->popup.win->bounds.y,
                    iter->popup.win->bounds.w, iter->popup.win->bounds.h))
                    break;
                iter = iter->next;
            }
        }

        if (!iter) {
            /* current window is active in that position so transfer to top
             * at the highest priority in stack */
            zr_remove_window(ctx, win);
            zr_insert_window(ctx, win);

            /* NOTE: I am not really sure if activating a window should directly
             * happen on that frame or the following frame. Directly would simplify
             * clicking window closing/minimizing but could cause wrong behavior.
             * For now I activate the window on the next frame to prevent wrong
             * behavior. If not wanted just replace line with:
             * win->flags &= ~(zr_flags)ZR_WINDOW_ROM; */
            win->flags |= ZR_WINDOW_REMOVE_ROM;
            ctx->active = win;
        }
        if (ctx->end != win)
            win->flags |= ZR_WINDOW_ROM;
    }
    win->layout = layout;
    ctx->current = win;
    ret = zr_panel_begin(ctx, title);
    layout->offset = &win->scrollbar;
    return ret;
}

void
zr_end(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    if (!ctx || !ctx->current) return;
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    zr_panel_end(ctx);
    ctx->current = 0;
}

struct zr_rect
zr_window_get_bounds(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return zr_rect(0,0,0,0);
    return ctx->current->bounds;
}
struct zr_vec2
zr_window_get_position(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return zr_vec2(0,0);
    return zr_vec2(ctx->current->bounds.x, ctx->current->bounds.y);
}

struct zr_vec2
zr_window_get_size(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return zr_vec2(0,0);
    return zr_vec2(ctx->current->bounds.w, ctx->current->bounds.h);
}

float
zr_window_get_width(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return 0;
    return ctx->current->bounds.w;
}

float
zr_window_get_height(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return 0;
    return ctx->current->bounds.h;
}

struct zr_rect
zr_window_get_content_region(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return zr_rect(0,0,0,0);
    return ctx->current->layout->clip;
}

struct zr_vec2
zr_window_get_content_region_min(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current) return zr_vec2(0,0);
    return zr_vec2(ctx->current->layout->clip.x, ctx->current->layout->clip.y);
}

struct zr_vec2
zr_window_get_content_region_max(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current) return zr_vec2(0,0);
    return zr_vec2(ctx->current->layout->clip.x + ctx->current->layout->clip.w,
        ctx->current->layout->clip.y + ctx->current->layout->clip.h);
}

struct zr_vec2
zr_window_get_content_region_size(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current) return zr_vec2(0,0);
    return zr_vec2(ctx->current->layout->clip.w, ctx->current->layout->clip.h);
}

struct zr_command_buffer*
zr_window_get_canvas(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current) return 0;
    return &ctx->current->buffer;
}

int
zr_window_has_focus(const struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current) return 0;
    return ctx->current == ctx->active;
}

int
zr_window_is_collapsed(struct zr_context *ctx, const char *name)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx) return 0;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) return 0;
    return win->flags & ZR_WINDOW_MINIMIZED;
}

int
zr_window_is_closed(struct zr_context *ctx, const char *name)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx) return 0;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) return 0;
    return win->flags & ZR_WINDOW_HIDDEN;
}

int
zr_window_is_active(struct zr_context *ctx, const char *name)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx) return 0;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) return 0;
    return win == ctx->active;
}

void
zr_window_set_bounds(struct zr_context *ctx, struct zr_rect bounds)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return;
    ctx->current->bounds = bounds;
}

void
zr_window_set_position(struct zr_context *ctx, struct zr_vec2 pos)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return;
    ctx->current->bounds.x = pos.x;
    ctx->current->bounds.y = pos.y;
}

void
zr_window_set_size(struct zr_context *ctx, struct zr_vec2 size)
{
    ZR_ASSERT(ctx); ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current) return;
    ctx->current->bounds.w = size.x;
    ctx->current->bounds.h = size.y;
}

void
zr_window_collapse(struct zr_context *ctx, const char *name,
                    enum zr_collapse_states c)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx) return;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) return;
    if (c == ZR_MINIMIZED)
        win->flags |= ZR_WINDOW_MINIMIZED;
    else win->flags &= ~(zr_flags)ZR_WINDOW_MINIMIZED;
}

void
zr_window_collapse_if(struct zr_context *ctx, const char *name,
    enum zr_collapse_states c, int cond)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx || !cond) return;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    if (!win) return;

    if (c == ZR_MINIMIZED)
        win->flags |= ZR_WINDOW_HIDDEN;
    else win->flags &= ~(zr_flags)ZR_WINDOW_HIDDEN;
}

void
zr_window_set_focus(struct zr_context *ctx, const char *name)
{
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;
    ZR_ASSERT(ctx);
    if (!ctx) return;

    title_len = (int)zr_strsiz(name);
    title_hash = zr_murmur_hash(name, (int)title_len, ZR_WINDOW_TITLE);
    win = zr_find_window(ctx, title_hash);
    ctx->active = win;
}

/*----------------------------------------------------------------
 *
 *                          PANEL
 *
 * --------------------------------------------------------------*/
struct zr_window_header {
    float x, y, w, h;
    float front, back;
};

static int
zr_header_button(struct zr_context *ctx, struct zr_window_header *header,
    zr_rune symbol, enum zr_style_header_align align)
{
    /* calculate the position of the close icon position and draw it */
    zr_glyph glyph;
    struct zr_rect sym = {0,0,0,0};
    float sym_bw = 0;
    int ret = zr_false;

    const struct zr_style *c;
    struct zr_command_buffer *out;
    struct zr_vec2 item_padding;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    /* cache configuration data */
    win = ctx->current;
    layout = win->layout;
    c = &ctx->style;
    out = &win->buffer;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);

    sym.x = header->front;
    sym.y = header->y;
    {
        /* single unicode rune text icon */
        const char *X = glyph;
        const zr_size len = zr_utf_encode(symbol, glyph, sizeof(glyph));
        const zr_size t = c->font.width(c->font.userdata, c->font.height, X, len);
        const float text_width = (float)t;
        struct zr_text text;

        /* calculate bounds of the icon */
        sym_bw = text_width;
        sym.w = (float)text_width + 2 * item_padding.x;
        sym.h = c->font.height + 2 * item_padding.y;
        if (align == ZR_HEADER_RIGHT)
            sym.x = header->back - sym.w;

        text.padding = zr_vec2(0,0);
        text.background = c->colors[ZR_COLOR_HEADER];
        text.text = c->colors[ZR_COLOR_TEXT];
        zr_widget_text(out, sym, X, len, &text,
            ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, &c->font);
    }

    /* check if the icon has been pressed */
    if (!(layout->flags & ZR_WINDOW_ROM)) {
        struct zr_rect bounds;
        enum zr_widget_status status;
        bounds.x = sym.x; bounds.y = sym.y;
        bounds.w = sym_bw; bounds.h = sym.h;
        ret = zr_button_behavior(&status, bounds, &ctx->input, ZR_BUTTON_DEFAULT);
    }

    /* update the header space */
    if (align == ZR_HEADER_RIGHT)
        header->back -= (sym.w + item_padding.x);
    else header->front += sym.w + item_padding.x;
    return ret;
}

static int
zr_header_toggle(struct zr_context *ctx, struct zr_window_header *header,
    zr_rune active, zr_rune inactive, enum zr_style_header_align align, int state)
{
    int ret = zr_header_button(ctx, header,(state) ? active : inactive, align);
    if (ret) return !state;
    else return state;
}

static int
zr_header_flag(struct zr_context *ctx, struct zr_window_header *header,
    zr_rune inactive, zr_rune active, enum zr_style_header_align align,
    zr_flags flag)
{
    struct zr_window *win = ctx->current;
    struct zr_panel *layout = win->layout;
    zr_flags flags = win->flags;
    int state = (flags & flag) ? zr_true : zr_false;
    int ret = zr_header_toggle(ctx, header, inactive, active, align, state);
    if (ret != ((flags & flag) ? zr_true : zr_false)) {
        /* the state of the toggle icon has been changed  */
        if (!ret) layout->flags &= ~flag;
        else layout->flags |= flag;
        return zr_true;
    }
    return zr_false;
}

static int
zr_panel_begin(struct zr_context *ctx, const char *title)
{
    int header_active = 0;
    float scrollbar_size;
    struct zr_vec2 item_padding;
    struct zr_vec2 item_spacing;
    struct zr_vec2 window_padding;
    struct zr_vec2 scaler_size;

    struct zr_input *in;
    struct zr_window *win;
    const struct zr_style *c;
    struct zr_panel *layout;
    struct zr_command_buffer *out;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    c = &ctx->style;
    in = &ctx->input;
    win = ctx->current;
    layout = win->layout;

    /* cache style data */
    scrollbar_size = zr_get_property(ctx, ZR_PROPERTY_SCROLLBAR_SIZE).x;
    window_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    item_spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    scaler_size = zr_get_property(ctx, ZR_PROPERTY_SCALER_SIZE);

    /* check arguments */
    zr_zero(layout, sizeof(*layout));
    if (win->flags & ZR_WINDOW_HIDDEN)
        return 0;

    /* move panel position if requested */
    layout->header_h = c->font.height + 4 * item_padding.y;
    layout->header_h += window_padding.y;
    if ((win->flags & ZR_WINDOW_MOVABLE) && !(win->flags & ZR_WINDOW_ROM)) {
        int incursor;
        struct zr_rect move;
        move.x = win->bounds.x;
        move.y = win->bounds.y;
        move.w = win->bounds.w;
        move.h = layout->header_h;
        incursor = zr_input_is_mouse_prev_hovering_rect(in, move);
        if (zr_input_is_mouse_down(in, ZR_BUTTON_LEFT) && incursor) {
            win->bounds.x = win->bounds.x + in->mouse.delta.x;
            win->bounds.y = win->bounds.y + in->mouse.delta.y;
        }
    }

#if ZR_COMPILE_WITH_COMMAND_USERDATA
    win->buffer.userdata = ctx->userdata;
#endif

    /* setup window layout */
    out = &win->buffer;
    layout->bounds = win->bounds;
    layout->at_x = win->bounds.x;
    layout->at_y = win->bounds.y;
    layout->width = win->bounds.w;
    layout->height = win->bounds.h;
    layout->row.index = 0;
    layout->row.columns = 0;
    layout->row.height = 0;
    layout->row.ratio = 0;
    layout->row.item_width = 0;
    layout->row.tree_depth = 0;
    layout->max_x = 0;
    layout->flags = win->flags;

    /* calculate window header */
    if (win->flags & ZR_WINDOW_MINIMIZED) {
        layout->header_h = 0;
        layout->row.height = 0;
    } else {
        layout->header_h = 2 * item_spacing.y;
        layout->row.height = layout->header_h + 1;
    }

    /* calculate window footer height */
    if (!(win->flags & ZR_WINDOW_NONBLOCK) &&
        (!(win->flags & ZR_WINDOW_NO_SCROLLBAR) || (win->flags & ZR_WINDOW_SCALABLE)))
        layout->footer_h = scaler_size.y + item_padding.y;
    else layout->footer_h = 0;

    /* calculate the window size */
    if (!(win->flags & ZR_WINDOW_NO_SCROLLBAR))
        layout->width = win->bounds.w - scrollbar_size;
    layout->height = win->bounds.h - (layout->header_h + 2 * item_spacing.y);
    layout->height -= layout->footer_h;

    /* window header */
    header_active = (win->flags & (ZR_WINDOW_CLOSABLE|ZR_WINDOW_MINIMIZABLE));
    header_active = header_active || (win->flags & ZR_WINDOW_TITLE);
    header_active = header_active && !(win->flags & ZR_WINDOW_HIDDEN) && title;

    if (header_active)
    {
        struct zr_rect old_clip = out->clip;
        struct zr_window_header header;

        /* This is a little bit of a performace hack. To make sure the header
         * does not get overdrawn with text you do not have to push a scissor rect.
         * This is possible because the command buffer automatically clips text
         * by using its clipping rectangle. But since the clipping rect gets
         * reused to calculate the window clipping rect the old clipping rect
         * has to be stored and reset afterwards. */
        out->clip.x = header.x = layout->bounds.x + window_padding.x;
        out->clip.y = header.y = layout->bounds.y + item_padding.y;
        out->clip.w = header.w = MAX(layout->bounds.w, 2 * window_padding.x);
        out->clip.h = header.w -= 2 * window_padding.x;

        /* update the header height and first row height */
        layout->header_h = c->font.height + 2 * item_padding.y;
        layout->header_h += window_padding.y;
        layout->row.height += layout->header_h;

        header.h = layout->header_h;
        header.back = header.x + header.w;
        header.front = header.x;

        layout->height = layout->bounds.h - (header.h + 2 * item_spacing.y);
        layout->height -= layout->footer_h;

        /* draw header background */
        if (!(layout->flags & ZR_WINDOW_BORDER)) {
            zr_draw_rect(out, zr_rect(layout->bounds.x, layout->bounds.y,
                layout->bounds.w, layout->header_h), 0, c->colors[ZR_COLOR_HEADER]);
        } else {
            zr_draw_rect(out, zr_rect(layout->bounds.x, layout->bounds.y+1,
                layout->bounds.w, layout->header_h), 0, c->colors[ZR_COLOR_HEADER]);
        }

        /* window header icons */
        if (win->flags & ZR_WINDOW_CLOSABLE)
            zr_header_flag(ctx, &header, c->header.close_symbol,
                c->header.close_symbol, c->header.align, ZR_WINDOW_HIDDEN);
        if (win->flags & ZR_WINDOW_MINIMIZABLE)
            zr_header_flag(ctx, &header, c->header.maximize_symbol,
                c->header.minimize_symbol, c->header.align, ZR_WINDOW_MINIMIZED);

        {
            /* window header title */
            zr_size text_len = zr_strsiz(title);
            struct zr_rect label = {0,0,0,0};

            /* calculate and allocate space from the header */
            zr_size t = c->font.width(c->font.userdata, c->font.height, title, text_len);
            if (c->header.align == ZR_HEADER_LEFT) {
                header.back = header.back - (3 * item_padding.x + (float)t);
                label.x = header.back;
            } else {
                label.x = header.front;
                header.front += 3 * item_padding.x + (float)t;
            }

            {
                /* calculate label bounds and draw text */
                struct zr_text text;
                text.padding = zr_vec2(0,0);
                text.background = c->colors[ZR_COLOR_HEADER];
                text.text = c->colors[ZR_COLOR_TEXT];

                label.y = header.y;
                label.h = c->font.height + 2 * item_padding.y;
                label.w = MAX((float)t + 2 * item_padding.x, 4 * item_padding.x);
                zr_widget_text(out, label,(const char*)title, text_len, &text,
                    ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, &c->font);
            }
        }
        out->clip = old_clip;
    }

    /* fix header height for transistion between minimized and maximized window state */
    if (win->flags & ZR_WINDOW_MINIMIZED && !(layout->flags & ZR_WINDOW_MINIMIZED))
        layout->row.height += 2 * item_spacing.y + 1;

    if (layout->flags & ZR_WINDOW_MINIMIZED) {
        /* draw window background if minimized */
        layout->row.height = 0;
        zr_draw_rect(out, zr_rect(layout->bounds.x, layout->bounds.y,
            layout->bounds.w, layout->row.height), 0, c->colors[ZR_COLOR_WINDOW]);
    } else if (!(layout->flags & ZR_WINDOW_DYNAMIC)) {
        /* draw static window body */
        struct zr_rect body = layout->bounds;
        if (header_active) {
            body.y += layout->header_h;
            body.h -= layout->header_h;
        }
        zr_draw_rect(out, body, 0, c->colors[ZR_COLOR_WINDOW]);
    } else {
        /* draw dynamic window body */
        zr_draw_rect(out, zr_rect(layout->bounds.x, layout->bounds.y,
            layout->bounds.w, layout->row.height + window_padding.y), 0,
            c->colors[ZR_COLOR_WINDOW]);
    }

    /* draw top window border line */
    if (layout->flags & ZR_WINDOW_BORDER) {
        zr_draw_line(out, layout->bounds.x, layout->bounds.y,
            layout->bounds.x + layout->bounds.w, layout->bounds.y,
            c->colors[ZR_COLOR_BORDER]);
    }

    {
        /* calculate and set the window clipping rectangle*/
        struct zr_rect clip;
        if (!(win->flags & ZR_WINDOW_DYNAMIC)) {
            layout->clip.x = win->bounds.x + window_padding.x;
            layout->clip.w = layout->width - 2 * window_padding.x;
        } else {
            layout->clip.x = win->bounds.x;
            layout->clip.w = layout->width;
        }

        layout->clip.h = win->bounds.h - (layout->footer_h + layout->header_h);
        layout->clip.h -= (window_padding.y + item_padding.y);
        layout->clip.y = win->bounds.y;
        if (win->flags & ZR_WINDOW_BORDER) {
            layout->clip.y += 1;
            layout->clip.h -= 2;
        }

        /* combo box and menu do not have header space */
        if (!(win->flags & ZR_WINDOW_COMBO) && !(win->flags & ZR_WINDOW_MENU))
            layout->clip.y += layout->header_h;

        zr_unify(&clip, &win->buffer.clip, layout->clip.x, layout->clip.y,
            layout->clip.x + layout->clip.w, layout->clip.y + layout->clip.h);
        zr_draw_scissor(out, clip);

        win->buffer.clip.x = layout->bounds.x;
        win->buffer.clip.w = layout->width;
        if (!(win->flags & ZR_WINDOW_NO_SCROLLBAR))
            win->buffer.clip.w += scrollbar_size;
    }
    return !(layout->flags & ZR_WINDOW_HIDDEN) && !(layout->flags & ZR_WINDOW_MINIMIZED);
}

static void
zr_panel_end(struct zr_context *ctx)
{
    /* local read only style variables */
    float scrollbar_size;
    struct zr_vec2 item_padding;
    struct zr_vec2 item_spacing;
    struct zr_vec2 window_padding;
    struct zr_vec2 scaler_size;
    struct zr_rect footer = {0,0,0,0};

    /* pointers to subsystems */
    struct zr_input *in;
    struct zr_window *window;
    struct zr_panel *layout;
    struct zr_command_buffer *out;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    window = ctx->current;
    layout = window->layout;
    config = &ctx->style;
    out = &window->buffer;
    in = (layout->flags & ZR_WINDOW_ROM) ? 0 :&ctx->input;
    if (!(layout->flags & ZR_WINDOW_SUB))
        zr_draw_scissor(out, zr_null_rect);

    /* cache configuration data */
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    item_spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    window_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);
    scrollbar_size = zr_get_property(ctx, ZR_PROPERTY_SCROLLBAR_SIZE).x;
    scaler_size = zr_get_property(ctx, ZR_PROPERTY_SCALER_SIZE);

    /* update the current cursor Y-position to point over the last added widget */
    layout->at_y += layout->row.height;

    /* draw footer and fill empty spaces inside a dynamically growing panel */
    if (layout->flags & ZR_WINDOW_DYNAMIC && !(layout->flags & ZR_WINDOW_MINIMIZED)) {
        layout->height = layout->at_y - layout->bounds.y;
        layout->height = MIN(layout->height, layout->bounds.h);
        if ((layout->offset->x == 0) || (layout->flags & ZR_WINDOW_NO_SCROLLBAR)) {
            /* special case for dynamic windows without horizontal scrollbar
             * or hidden scrollbars */
            footer.x = window->bounds.x;
            footer.y = window->bounds.y + layout->height + item_spacing.y;
            footer.w = window->bounds.w + scrollbar_size;
            layout->footer_h = 0;
            footer.h = 0;

            if ((layout->offset->x == 0) && !(layout->flags & ZR_WINDOW_NO_SCROLLBAR)) {
                /* special case for windows like combobox, menu require draw call
                 * to fill the empty scrollbar background */
                struct zr_rect bounds;
                bounds.x = layout->bounds.x + layout->width;
                bounds.y = layout->clip.y;
                bounds.w = scrollbar_size;
                bounds.h = layout->height;
                zr_draw_rect(out, bounds, 0, config->colors[ZR_COLOR_WINDOW]);
            }
        } else {
            /* dynamic window with visible scrollbars and therefore bigger footer */
            footer.x = window->bounds.x;
            footer.w = window->bounds.w + scrollbar_size;
            footer.h = layout->footer_h;
            if ((layout->flags & ZR_WINDOW_COMBO) || (layout->flags & ZR_WINDOW_MENU) ||
                (layout->flags & ZR_WINDOW_CONTEXTUAL))
                footer.y = window->bounds.y + layout->height;
            else footer.y = window->bounds.y + layout->height + layout->footer_h;
            zr_draw_rect(out, footer, 0, config->colors[ZR_COLOR_WINDOW]);

            if (!(layout->flags & ZR_WINDOW_COMBO) && !(layout->flags & ZR_WINDOW_MENU)) {
                /* fill empty scrollbar space */
                struct zr_rect bounds;
                bounds.x = layout->bounds.x;
                bounds.y = window->bounds.y + layout->height;
                bounds.w = layout->bounds.w;
                bounds.h = layout->row.height;
                zr_draw_rect(out, bounds, 0, config->colors[ZR_COLOR_WINDOW]);
            }
        }
    }

    /* scrollbars */
    if (!(layout->flags & ZR_WINDOW_NO_SCROLLBAR) && !(layout->flags & ZR_WINDOW_MINIMIZED)) {
        struct zr_rect bounds;
        float scroll_target, scroll_offset, scroll_step;

        /* fill scrollbar style */
        struct zr_scrollbar scroll;
        scroll.rounding = config->rounding[ZR_ROUNDING_SCROLLBAR];
        scroll.background = config->colors[ZR_COLOR_SCROLLBAR];
        scroll.normal = config->colors[ZR_COLOR_SCROLLBAR_CURSOR];
        scroll.hover = config->colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER];
        scroll.active = config->colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE];
        scroll.border = config->colors[ZR_COLOR_BORDER];
        {
            /* vertical scollbar */
            enum zr_widget_status state;
            bounds.x = layout->bounds.x + layout->width;
            bounds.y = layout->clip.y;
            bounds.w = scrollbar_size;
            bounds.h = layout->clip.h;
            if (layout->flags & ZR_WINDOW_BORDER) bounds.h -= 1;

            scroll_offset = layout->offset->y;
            scroll_step = layout->clip.h * 0.10f;
            scroll_target = (float)(int)(layout->at_y - layout->clip.y);
            scroll.has_scrolling = (window == ctx->active);
            scroll_offset = zr_do_scrollbarv(&state, out, bounds, scroll_offset,
                                    scroll_target, scroll_step, &scroll, in);
            layout->offset->y = (unsigned short)scroll_offset;
        }
        {
            /* horizontal scrollbar */
            enum zr_widget_status state;
            bounds.x = layout->bounds.x + window_padding.x;
            if (layout->flags & ZR_WINDOW_SUB) {
                bounds.h = scrollbar_size;
                bounds.y = (layout->flags & ZR_WINDOW_BORDER) ?
                            layout->bounds.y + 1 : layout->bounds.y;
                bounds.y += layout->header_h + layout->menu.h + layout->height;
                bounds.w = layout->clip.w;
            } else if (layout->flags & ZR_WINDOW_DYNAMIC) {
                bounds.h = MIN(scrollbar_size, layout->footer_h);
                bounds.w = layout->bounds.w;
                bounds.y = footer.y;
            } else {
                bounds.h = MIN(scrollbar_size, layout->footer_h);
                bounds.y = layout->bounds.y + window->bounds.h;
                bounds.y -= MAX(layout->footer_h, scrollbar_size);
                bounds.w = layout->width - 2 * window_padding.x;
            }
            scroll_offset = layout->offset->x;
            scroll_target = (float)(int)(layout->max_x - bounds.x);
            scroll_step = layout->max_x * 0.05f;
            scroll.has_scrolling = zr_false;
            scroll_offset = zr_do_scrollbarh(&state, out, bounds, scroll_offset,
                                    scroll_target, scroll_step, &scroll, in);
            layout->offset->x = (unsigned short)scroll_offset;
        }
    }

    /* draw the panel scaler into the right corner of the panel footer and
     * update panel size if user drags the scaler */
    if ((layout->flags & ZR_WINDOW_SCALABLE) && in && !(layout->flags & ZR_WINDOW_MINIMIZED)) {
        struct zr_color col = config->colors[ZR_COLOR_SCALER];
        float scaler_w = MAX(0, scaler_size.x - item_padding.x);
        float scaler_h = MAX(0, scaler_size.y - item_padding.y);
        float scaler_x = (layout->bounds.x + layout->bounds.w) - (item_padding.x + scaler_w);

        float scaler_y;
        if (layout->flags & ZR_WINDOW_DYNAMIC)
            scaler_y = footer.y + layout->footer_h - scaler_size.y;
        else scaler_y = layout->bounds.y + layout->bounds.h - scaler_size.y;
        zr_draw_triangle(out, scaler_x + scaler_w, scaler_y,
            scaler_x + scaler_w, scaler_y + scaler_h, scaler_x, scaler_y + scaler_h, col);

        if (!(window->flags & ZR_WINDOW_ROM)) {
            float prev_x = in->mouse.prev.x;
            float prev_y = in->mouse.prev.y;
            struct zr_vec2 window_size = zr_get_property(ctx, ZR_PROPERTY_SIZE);
            int incursor = ZR_INBOX(prev_x,prev_y,scaler_x,scaler_y,scaler_w,scaler_h);

            if (zr_input_is_mouse_down(in, ZR_BUTTON_LEFT) && incursor) {
                window->bounds.w = MAX(window_size.x, window->bounds.w + in->mouse.delta.x);
                /* draging in y-direction is only possible if static window */
                if (!(layout->flags & ZR_WINDOW_DYNAMIC))
                    window->bounds.h = MAX(window_size.y, window->bounds.h + in->mouse.delta.y);
            }
        }
    }

    if (layout->flags & ZR_WINDOW_BORDER) {
        /* draw the border around the complete panel */
        const float width = (layout->flags & ZR_WINDOW_NO_SCROLLBAR) ?
            layout->width: layout->width + scrollbar_size;
        const float padding_y = (layout->flags & ZR_WINDOW_MINIMIZED) ?
            window->bounds.y + layout->header_h:
            (layout->flags & ZR_WINDOW_DYNAMIC)?
            layout->footer_h + footer.y:
            layout->bounds.y + layout->bounds.h;

        if (window->flags & ZR_WINDOW_BORDER_HEADER)
            zr_draw_line(out, window->bounds.x, window->bounds.y + layout->header_h,
                window->bounds.x + window->bounds.w, window->bounds.y + layout->header_h,
                config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, window->bounds.x, padding_y, window->bounds.x + width,
                padding_y, config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, window->bounds.x, window->bounds.y, window->bounds.x,
                padding_y, config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, window->bounds.x + width, window->bounds.y,
                window->bounds.x + width, padding_y, config->colors[ZR_COLOR_BORDER]);
    }

    if (!(window->flags & ZR_WINDOW_SUB)) {
        /* window is hidden so clear command buffer  */
        if (layout->flags & ZR_WINDOW_HIDDEN)
            zr_command_buffer_reset(&window->buffer);
        /* window is visible and not tab */
        else zr_finish(ctx, window);
    }

    /* ZR_WINDOW_REMOVE_ROM flag was set so remove ZR_WINDOW_ROM */
    if (layout->flags & ZR_WINDOW_REMOVE_ROM) {
        layout->flags &= ~(zr_flags)ZR_WINDOW_ROM;
        layout->flags &= ~(zr_flags)ZR_WINDOW_REMOVE_ROM;
    }
    window->flags = layout->flags;

    /* property garbage collector */
    if (window->property.active && window->property.old != window->property.seq &&
        window->property.active == window->property.prev) {
        zr_zero(&window->property, sizeof(window->property));
    } else {
        window->property.old = window->property.seq;
        window->property.prev = window->property.active;
        window->property.seq = 0;
    }

    /* edit garbage collector */
    if (window->edit.active && window->edit.old != window->edit.seq &&
        window->edit.active == window->edit.prev) {
        zr_zero(&window->edit, sizeof(window->edit));
    } else {
        window->edit.old = window->edit.seq;
        window->edit.prev = window->edit.active;
        window->edit.seq = 0;
    }

    /* contextual gargabe collector */
    if (window->popup.active_con && window->popup.con_old != window->popup.con_count) {
        window->popup.con_count = 0;
        window->popup.con_old = 0;
        window->popup.active_con = 0;
    } else {
        window->popup.con_old = window->popup.con_count;
        window->popup.con_count = 0;
    }
    window->popup.combo_count = 0;
    /* helper to make sure you have a 'zr_layout_push'
     * for every 'zr_layout_pop' */
    ZR_ASSERT(!layout->row.tree_depth);
}

void
zr_menubar_begin(struct zr_context *ctx)
{
    struct zr_panel *layout;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    layout = ctx->current->layout;
    if (layout->flags & ZR_WINDOW_HIDDEN || layout->flags & ZR_WINDOW_MINIMIZED)
        return;

    layout->menu.x = layout->at_x;
    layout->menu.y = layout->bounds.y + layout->header_h;
    layout->menu.w = layout->width;
    layout->menu.offset = *layout->offset;
    layout->offset->y = 0;
}

void
zr_menubar_end(struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_panel *layout;
    struct zr_command_buffer *out;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    if (!ctx || layout->flags & ZR_WINDOW_HIDDEN || layout->flags & ZR_WINDOW_MINIMIZED)
        return;

    out = &win->buffer;
    layout->menu.h = layout->at_y - layout->menu.y;
    layout->clip.y = layout->bounds.y + layout->header_h + layout->menu.h + layout->row.height;
    layout->height -= layout->menu.h;
    *layout->offset = layout->menu.offset;
    layout->clip.h -= layout->menu.h + layout->row.height;
    layout->at_y = layout->menu.y + layout->menu.h;
    zr_draw_scissor(out, layout->clip);
}
/* -------------------------------------------------------------
 *
 *                          LAYOUT
 *
 * --------------------------------------------------------------*/
static void
zr_panel_layout(const struct zr_context *ctx, struct zr_window *win,
    float height, zr_size cols)
{
    const struct zr_style *config;
    const struct zr_color *color;
    struct zr_command_buffer *out;
    struct zr_vec2 item_spacing;
    struct zr_vec2 panel_padding;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    /* prefetch some configuration data */
    layout = win->layout;
    config = &ctx->style;
    out = &win->buffer;
    color = &config->colors[ZR_COLOR_WINDOW];
    item_spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    panel_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);

    /* update the current row and set the current row layout */
    layout->row.index = 0;
    layout->at_y += layout->row.height;
    layout->row.columns = cols;
    layout->row.height = height + item_spacing.y;
    layout->row.item_offset = 0;
    if (layout->flags & ZR_WINDOW_DYNAMIC)
        zr_draw_rect(out,  zr_rect(layout->bounds.x, layout->at_y,
            layout->bounds.w, height + panel_padding.y), 0, *color);
}

static void
zr_row_layout(struct zr_context *ctx, enum zr_layout_format fmt,
    float height, zr_size cols, zr_size width)
{
    /* update the current row and set the current row layout */
    struct zr_window *win;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    zr_panel_layout(ctx, win, height, cols);
    if (fmt == ZR_DYNAMIC)
        win->layout->row.type = ZR_LAYOUT_DYNAMIC_FIXED;
    else win->layout->row.type = ZR_LAYOUT_STATIC_FIXED;

    win->layout->row.item_width = (float)width;
    win->layout->row.ratio = 0;
    win->layout->row.item_offset = 0;
    win->layout->row.filled = 0;
}

void
zr_layout_row_dynamic(struct zr_context *ctx, float height, zr_size cols)
{zr_row_layout(ctx, ZR_DYNAMIC, height, cols, 0);}

void
zr_layout_row_static(struct zr_context *ctx, float height,
    zr_size item_width, zr_size cols)
{zr_row_layout(ctx, ZR_STATIC, height, cols, item_width);}

void
zr_layout_row_begin(struct zr_context *ctx,
    enum zr_layout_format fmt, float row_height, zr_size cols)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;

    zr_panel_layout(ctx, win, row_height, cols);
    if (fmt == ZR_DYNAMIC)
        layout->row.type = ZR_LAYOUT_DYNAMIC_ROW;
    else layout->row.type = ZR_LAYOUT_STATIC_ROW;

    layout->row.ratio = 0;
    layout->row.item_width = 0;
    layout->row.item_offset = 0;
    layout->row.filled = 0;
    layout->row.columns = cols;
}

void
zr_layout_row_push(struct zr_context *ctx, float ratio_or_width)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;

    if (layout->row.type == ZR_LAYOUT_DYNAMIC_ROW) {
        float ratio = ratio_or_width;
        if ((ratio + layout->row.filled) > 1.0f) return;
        if (ratio > 0.0f)
            layout->row.item_width = ZR_SATURATE(ratio);
        else layout->row.item_width = 1.0f - layout->row.filled;
    } else layout->row.item_width = ratio_or_width;
}

void
zr_layout_row_end(struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    layout->row.item_width = 0;
    layout->row.item_offset = 0;
}

void
zr_layout_row(struct zr_context *ctx, enum zr_layout_format fmt,
    float height, zr_size cols, const float *ratio)
{
    zr_size i;
    zr_size n_undef = 0;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    zr_panel_layout(ctx, win, height, cols);
    if (fmt == ZR_DYNAMIC) {
        /* calculate width of undefined widget ratios */
        float r = 0;
        layout->row.ratio = ratio;
        for (i = 0; i < cols; ++i) {
            if (ratio[i] < 0.0f)
                n_undef++;
            else r += ratio[i];
        }
        r = ZR_SATURATE(1.0f - r);
        layout->row.type = ZR_LAYOUT_DYNAMIC;
        layout->row.item_width = (r > 0 && n_undef > 0) ? (r / (float)n_undef):0;
    } else {
        layout->row.ratio = ratio;
        layout->row.type = ZR_LAYOUT_STATIC;
        layout->row.item_width = 0;
        layout->row.item_offset = 0;
    }
    layout->row.item_offset = 0;
    layout->row.filled = 0;
}

void
zr_layout_space_begin(struct zr_context *ctx,
    enum zr_layout_format fmt, float height, zr_size widget_count)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    zr_panel_layout(ctx, win, height, widget_count);
    if (fmt == ZR_STATIC)
        layout->row.type = ZR_LAYOUT_STATIC_FREE;
    else layout->row.type = ZR_LAYOUT_DYNAMIC_FREE;

    layout->row.ratio = 0;
    layout->row.item_width = 0;
    layout->row.item_offset = 0;
    layout->row.filled = 0;
}

void
zr_layout_space_end(struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    layout->row.item_width = 0;
    layout->row.item_height = 0;
    layout->row.item_offset = 0;
    zr_zero(&layout->row.item, sizeof(layout->row.item));
}

void
zr_layout_space_push(struct zr_context *ctx, struct zr_rect rect)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    layout->row.item = rect;
}

struct zr_rect
zr_layout_space_bounds(struct zr_context *ctx)
{
    struct zr_rect ret;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    win = ctx->current;
    layout = win->layout;

    ret.x = layout->clip.x;
    ret.y = layout->clip.y;
    ret.w = layout->clip.w;
    ret.h = layout->row.height;
    return ret;
}

struct zr_vec2
zr_layout_space_to_screen(struct zr_context *ctx, struct zr_vec2 ret)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    win = ctx->current;
    layout = win->layout;

    ret.x += layout->at_x - layout->offset->x;
    ret.y += layout->at_y - layout->offset->y;
    return ret;
}

struct zr_vec2
zr_layout_space_to_local(struct zr_context *ctx, struct zr_vec2 ret)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    win = ctx->current;
    layout = win->layout;

    ret.x += -layout->at_x + layout->offset->x;
    ret.y += -layout->at_y + layout->offset->y;
    return ret;
}

struct zr_rect
zr_layout_space_rect_to_screen(struct zr_context *ctx, struct zr_rect ret)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    win = ctx->current;
    layout = win->layout;

    ret.x += layout->at_x - layout->offset->x;
    ret.y += layout->at_y - layout->offset->y;
    return ret;
}

struct zr_rect
zr_layout_space_rect_to_local(struct zr_context *ctx, struct zr_rect ret)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    win = ctx->current;
    layout = win->layout;

    ret.x += -layout->at_x + layout->offset->x;
    ret.y += -layout->at_y + layout->offset->y;
    return ret;
}

static void
zr_panel_alloc_row(const struct zr_context *ctx, struct zr_window *win)
{
    struct zr_panel *layout = win->layout;
    struct zr_vec2 spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    const float row_height = layout->row.height - spacing.y;
    zr_panel_layout(ctx, win, row_height, layout->row.columns);
}

static void
zr_layout_widget_space(struct zr_rect *bounds, const struct zr_context *ctx,
    struct zr_window *win, int modify)
{
    float panel_padding, panel_spacing, panel_space;
    float item_offset = 0, item_width = 0, item_spacing = 0;
    struct zr_vec2 spacing, padding;

    struct zr_panel *layout;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    ZR_ASSERT(bounds);

    /* cache some configuration data */
    spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);

    /* calculate the useable panel space */
    panel_padding = 2 * padding.x;
    panel_spacing = (float)(layout->row.columns - 1) * spacing.x;
    panel_space  = layout->width - panel_padding - panel_spacing;

    /* calculate the width of one item inside the current layout space */
    switch (layout->row.type) {
    case ZR_LAYOUT_DYNAMIC_FIXED: {
        /* scaling fixed size widgets item width */
        item_width = panel_space / (float)layout->row.columns;
        item_offset = (float)layout->row.index * item_width;
        item_spacing = (float)layout->row.index * spacing.x;
    } break;
    case ZR_LAYOUT_DYNAMIC_ROW: {
        /* scaling single ratio widget width */
        item_width = layout->row.item_width * panel_space;
        item_offset = layout->row.item_offset;
        item_spacing = (float)layout->row.index * spacing.x;

        if (modify) {
            layout->row.item_offset += item_width + spacing.x;
            layout->row.filled += layout->row.item_width;
            layout->row.index = 0;
        }
    } break;
    case ZR_LAYOUT_DYNAMIC_FREE: {
        /* panel width depended free widget placing */
        bounds->x = layout->at_x + (layout->width * layout->row.item.x);
        bounds->x -= layout->offset->x;
        bounds->y = layout->at_y + (layout->row.height * layout->row.item.y);
        bounds->y -= layout->offset->y;
        bounds->w = layout->width  * layout->row.item.w;
        bounds->h = layout->row.height * layout->row.item.h;
        return;
    };
    case ZR_LAYOUT_DYNAMIC: {
        /* scaling arrays of panel width ratios for every widget */
        float ratio;
        ZR_ASSERT(layout->row.ratio);
        ratio = (layout->row.ratio[layout->row.index] < 0) ?
            layout->row.item_width : layout->row.ratio[layout->row.index];

        item_spacing = (float)layout->row.index * spacing.x;
        if (layout->row.index < layout->row.columns-1)
            item_width = (ratio * panel_space) - spacing.x;
        else item_width = (ratio * panel_space);

        item_offset = layout->row.item_offset;
        if (modify) {
            layout->row.item_offset += item_width + spacing.x;
            layout->row.filled += ratio;
        }
    } break;
    case ZR_LAYOUT_STATIC_FIXED: {
        /* non-scaling fixed widgets item width */
        item_width = layout->row.item_width;
        item_offset = (float)layout->row.index * item_width;
        item_spacing = (float)layout->row.index * spacing.x;
    } break;
    case ZR_LAYOUT_STATIC_ROW: {
        /* scaling single ratio widget width */
        item_width = layout->row.item_width;
        item_offset = layout->row.item_offset;
        item_spacing = (float)layout->row.index * spacing.x;
        if (modify) {
            layout->row.item_offset += item_width + spacing.x;
            layout->row.index = 0;
        }
    } break;
    case ZR_LAYOUT_STATIC_FREE: {
        /* free widget placing */
        bounds->x = layout->at_x + layout->row.item.x;
        bounds->w = layout->row.item.w;
        if (((bounds->x + bounds->w) > layout->max_x) && modify)
            layout->max_x = (bounds->x + bounds->w);
        bounds->x -= layout->offset->x;
        bounds->y = layout->at_y + layout->row.item.y;
        bounds->y -= layout->offset->y;
        bounds->h = layout->row.item.h;
        return;
    };
    case ZR_LAYOUT_STATIC: {
        /* non-scaling array of panel pixel width for every widget */
        item_spacing = (float)layout->row.index * spacing.x;
        item_width = layout->row.ratio[layout->row.index];
        item_offset = layout->row.item_offset;
        if (modify) layout->row.item_offset += item_width + spacing.x;
        } break;
    default: ZR_ASSERT(0); break;
    };

    /* set the bounds of the newly allocated widget */
    bounds->w = item_width;
    bounds->h = layout->row.height - spacing.y;
    bounds->y = layout->at_y - layout->offset->y;
    bounds->x = layout->at_x + item_offset + item_spacing + padding.x;
    if (((bounds->x + bounds->w) > layout->max_x) && modify)
        layout->max_x = bounds->x + bounds->w;
    bounds->x -= layout->offset->x;
}

static void
zr_panel_alloc_space(struct zr_rect *bounds, const struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    /* check if the end of the row has been hit and begin new row if so */
    win = ctx->current;
    layout = win->layout;
    if (layout->row.index >= layout->row.columns)
        zr_panel_alloc_row(ctx, win);

    /* calculate widget position and size */
    zr_layout_widget_space(bounds, ctx, win, zr_true);
    layout->row.index++;
}

void
zr_layout_peek(struct zr_rect *bounds, struct zr_context *ctx)
{
    float y;
    zr_size index;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    y = layout->at_y;
    index = layout->row.index;
    if (layout->row.index >= layout->row.columns) {
        layout->at_y += layout->row.height;
        layout->row.index = 0;
    }
    zr_layout_widget_space(bounds, ctx, win, zr_false);
    layout->at_y = y;
    layout->row.index = index;
}

int
zr_layout_push(struct zr_context *ctx, enum zr_layout_node_type type,
    const char *title, enum zr_collapse_states initial_state)
{
    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_style *config;
    struct zr_command_buffer *out;
    const struct zr_input *input;

    struct zr_vec2 item_spacing;
    struct zr_vec2 item_padding;
    struct zr_vec2 panel_padding;
    struct zr_rect header = {0,0,0,0};
    struct zr_rect sym = {0,0,0,0};

    enum zr_widget_status ws;
    enum zr_widget_state widget_state;

    zr_hash title_hash;
    int title_len;
    zr_uint *state = 0;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    /* cache some data */
    win = ctx->current;
    layout = win->layout;
    out = &win->buffer;
    config = &ctx->style;

    item_spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    panel_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);

    /* calculate header bounds and draw background */
    zr_layout_row_dynamic(ctx, config->font.height + 2 * item_padding.y, 1);
    widget_state = zr_widget(&header, ctx);
    if (type == ZR_LAYOUT_TAB)
        zr_draw_rect(out, header, 0, config->colors[ZR_COLOR_TAB_HEADER]);

    /* find or create tab persistent state (open/closed) */
    title_len = (int)zr_strsiz(title);
    title_hash = zr_murmur_hash(title, (int)title_len, ZR_WINDOW_HIDDEN);
    state = zr_find_value(win, title_hash);
    if (!state) {
        state = zr_add_value(ctx, win, title_hash, 0);
        *state = initial_state;
    }

    /* update node state */
    input = (!(layout->flags & ZR_WINDOW_ROM)) ? &ctx->input: 0;
    input = (input && widget_state == ZR_WIDGET_VALID) ? &ctx->input : 0;
    if (zr_button_behavior(&ws, header, input, ZR_BUTTON_DEFAULT))
        *state = (*state == ZR_MAXIMIZED) ? ZR_MINIMIZED : ZR_MAXIMIZED;

    {
        /* and draw closing/open icon */
        enum zr_heading heading;
        struct zr_vec2 points[3];
        heading = (*state == ZR_MAXIMIZED) ? ZR_DOWN : ZR_RIGHT;

        /* calculate the triangle bounds */
        sym.w = sym.h = config->font.height;
        sym.y = header.y + item_padding.y;
        sym.x = header.x + panel_padding.x + item_padding.x;

        /* calculate the triangle points and draw triangle */
        zr_triangle_from_direction(points, sym, 0, 0, heading);
        zr_draw_triangle(&win->buffer,  points[0].x, points[0].y,
            points[1].x, points[1].y, points[2].x, points[2].y, config->colors[ZR_COLOR_TEXT]);

        /* calculate the space the icon occupied */
        sym.w = config->font.height + 2 * item_padding.x;
    }
    {
        /* draw node label */
        struct zr_color color;
        struct zr_rect label;
        struct zr_text text;

        header.w = MAX(header.w, sym.w + item_spacing.y + panel_padding.x);
        label.x = sym.x + sym.w + item_spacing.x;
        label.y = sym.y;
        label.w = header.w - (sym.w + item_spacing.y + panel_padding.x);
        label.h = config->font.height;

        color = (type == ZR_LAYOUT_TAB) ?
            config->colors[ZR_COLOR_TAB_HEADER]:
            config->colors[ZR_COLOR_WINDOW];
        text.padding = zr_vec2(0,0);
        text.background = color;
        text.text = config->colors[ZR_COLOR_TEXT];
        zr_widget_text(out, label, title, zr_strsiz(title), &text,
            ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, &config->font);
    }

    if (type == ZR_LAYOUT_TAB) {
        /* special node with border around the header */
        zr_draw_line(out, header.x, header.y,
            header.x + header.w-1, header.y, config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, header.x, header.y,
            header.x, header.y + header.h, config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, header.x + header.w-1, header.y,
            header.x + header.w-1, header.y + header.h, config->colors[ZR_COLOR_BORDER]);
        zr_draw_line(out, header.x, header.y + header.h,
            header.x + header.w-1, header.y + header.h, config->colors[ZR_COLOR_BORDER]);
    }

    if (*state == ZR_MAXIMIZED) {
        layout->at_x = header.x + layout->offset->x;
        layout->width = MAX(layout->width, 2 * panel_padding.x);
        layout->width -= 2 * panel_padding.x;
        layout->row.tree_depth++;
        return zr_true;
    } else return zr_false;
}

void
zr_layout_pop(struct zr_context *ctx)
{
    struct zr_vec2 panel_padding;
    struct zr_window *win = 0;
    struct zr_panel *layout = 0;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    panel_padding = zr_get_property(ctx, ZR_PROPERTY_PADDING);
    layout->at_x -= panel_padding.x;
    layout->width += 2 * panel_padding.x;
    ZR_ASSERT(layout->row.tree_depth);
    layout->row.tree_depth--;
}
/*----------------------------------------------------------------
 *
 *                      WIDGETS
 *
 * --------------------------------------------------------------*/
void
zr_spacing(struct zr_context *ctx, zr_size cols)
{
    zr_size i, index, rows;
    struct zr_rect nil;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    /* spacing over row boundries */
    win = ctx->current;
    layout = win->layout;
    index = (layout->row.index + cols) % layout->row.columns;
    rows = (layout->row.index + cols) / layout->row.columns;
    if (rows) {
        for (i = 0; i < rows; ++i)
            zr_panel_alloc_row(ctx, win);
        cols = index;
    }

    /* non table layout need to allocate space */
    if (layout->row.type != ZR_LAYOUT_DYNAMIC_FIXED &&
        layout->row.type != ZR_LAYOUT_STATIC_FIXED) {
        for (i = 0; i < cols; ++i)
            zr_panel_alloc_space(&nil, ctx);
    }
    layout->row.index = index;
}

void
zr_seperator(struct zr_context *ctx)
{
    struct zr_vec2 item_padding;
    struct zr_vec2 item_spacing;
    struct zr_rect bounds;

    struct zr_window *win;
    struct zr_panel *layout;
    struct zr_command_buffer *out;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    out = &win->buffer;
    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    item_spacing = zr_get_property(ctx, ZR_PROPERTY_ITEM_SPACING);

    bounds.h = 1;
    bounds.w = MAX(layout->width, 2 * item_spacing.x + 2 * item_padding.x);
    bounds.y = (layout->at_y + layout->row.height + item_padding.y) - layout->offset->y;
    bounds.x = layout->at_x + item_spacing.x + item_padding.x - layout->offset->x;
    bounds.w = bounds.w - (2 * item_spacing.x + 2 * item_padding.x);
    zr_draw_line(out, bounds.x, bounds.y, bounds.x + bounds.w,
        bounds.y + bounds.h, config->colors[ZR_COLOR_BORDER]);
}

enum zr_widget_state
zr_widget(struct zr_rect *bounds, const struct zr_context *ctx)
{
    struct zr_rect *c = 0;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return ZR_WIDGET_INVALID;

    /* allocate space  and check if the widget needs to be updated and drawn */
    win = ctx->current;
    layout = win->layout;
    zr_panel_alloc_space(bounds, ctx);
    c = &layout->clip;
    if (!ZR_INTERSECT(c->x, c->y, c->w, c->h, bounds->x, bounds->y, bounds->w, bounds->h))
        return ZR_WIDGET_INVALID;
    if (!ZR_CONTAINS(bounds->x, bounds->y, bounds->w, bounds->h, c->x, c->y, c->w, c->h))
        return ZR_WIDGET_ROM;
    return ZR_WIDGET_VALID;
}

enum zr_widget_state
zr_widget_fitting(struct zr_rect *bounds, struct zr_context *ctx)
{
    /* update the bounds to stand without padding  */
    enum zr_widget_state state;
    struct zr_window *win;
    struct zr_panel *layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return ZR_WIDGET_INVALID;

    win = ctx->current;
    layout = win->layout;
    state = zr_widget(bounds, ctx);
    if (layout->row.index == 1) {
        bounds->w += ctx->style.properties[ZR_PROPERTY_PADDING].x;
        bounds->x -= ctx->style.properties[ZR_PROPERTY_PADDING].x;
    } else bounds->x -= ctx->style.properties[ZR_PROPERTY_ITEM_PADDING].x;
    if (layout->row.index == layout->row.columns)
        bounds->w += ctx->style.properties[ZR_PROPERTY_PADDING].x;
    else bounds->w += ctx->style.properties[ZR_PROPERTY_ITEM_PADDING].x;
    return state;
}

void
zr_text_colored(struct zr_context *ctx, const char *str, zr_size len,
    zr_flags alignment, struct zr_color color)
{
    struct zr_rect bounds;
    struct zr_text text;
    struct zr_vec2 item_padding;

    struct zr_window *win;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    zr_panel_alloc_space(&bounds, ctx);
    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);

    text.padding.x = item_padding.x;
    text.padding.y = item_padding.y;
    text.background = config->colors[ZR_COLOR_WINDOW];
    text.text = color;
    zr_widget_text(&win->buffer, bounds, str, len, &text, alignment, &config->font);
}

void
zr_text_wrap_colored(struct zr_context *ctx, const char *str,
    zr_size len, struct zr_color color)
{
    struct zr_vec2 item_padding;
    struct zr_rect bounds;
    struct zr_text text;

    struct zr_window *win;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    zr_panel_alloc_space(&bounds, ctx);
    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);

    text.padding.x = item_padding.x;
    text.padding.y = item_padding.y;
    text.background = config->colors[ZR_COLOR_WINDOW];
    text.text = color;
    zr_widget_text_wrap(&win->buffer, bounds, str, len, &text, &config->font);
}

void
zr_text_wrap(struct zr_context *ctx, const char *str, zr_size len)
{zr_text_wrap_colored(ctx, str, len, ctx->style.colors[ZR_COLOR_TEXT]);}

void
zr_text(struct zr_context *ctx, const char *str, zr_size len,
    zr_flags alignment)
{zr_text_colored(ctx, str, len, alignment, ctx->style.colors[ZR_COLOR_TEXT]);}

void
zr_label_colored(struct zr_context *ctx, const char *text,
    zr_flags align, struct zr_color color)
{zr_text_colored(ctx, text, zr_strsiz(text), align, color);}

void
zr_label(struct zr_context *ctx, const char *text, zr_flags align)
{zr_text(ctx, text, zr_strsiz(text), align);}

void
zr_label_wrap(struct zr_context *ctx, const char *str)
{zr_text_wrap(ctx, str, zr_strsiz(str));}

void
zr_label_colored_wrap(struct zr_context *ctx, const char *str, struct zr_color color)
{zr_text_wrap_colored(ctx, str, zr_strsiz(str), color);}

void
zr_image(struct zr_context *ctx, struct zr_image img)
{
    struct zr_vec2 item_padding;
    struct zr_rect bounds;
    struct zr_window *win;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    if (!zr_widget(&bounds, ctx))
        return;

    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    bounds.x += item_padding.x;
    bounds.y += item_padding.y;
    bounds.w -= 2 * item_padding.x;
    bounds.h -= 2 * item_padding.y;
    zr_draw_image(&win->buffer, bounds, &img);
}

enum zr_button_alloc {ZR_BUTTON_NORMAL, ZR_BUTTON_FITTING};
static enum zr_widget_state
zr_button(struct zr_button *button, struct zr_rect *bounds,
    struct zr_context *ctx, enum zr_button_alloc type)
{
    enum zr_widget_state state;
    struct zr_vec2 item_padding;
    const struct zr_style *config = &ctx->style;
    if (type == ZR_BUTTON_NORMAL)
        state = zr_widget(bounds, ctx);
    else state = zr_widget_fitting(bounds, ctx);
    if (!state) return state;

    zr_zero(button, sizeof(*button));
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    button->touch_pad = zr_get_property(ctx, ZR_PROPERTY_TOUCH_PADDING);
    button->rounding = config->rounding[ZR_ROUNDING_BUTTON];
    button->normal = config->colors[ZR_COLOR_BUTTON];
    button->hover = config->colors[ZR_COLOR_BUTTON_HOVER];
    button->active = config->colors[ZR_COLOR_BUTTON_ACTIVE];
    button->border = config->colors[ZR_COLOR_BORDER];
    button->padding.x = item_padding.x;
    button->padding.y = item_padding.y;
    button->border_width = 1;
    return state;
}

int
zr_button_text(struct zr_context *ctx, const char *str,
    enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_text button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_text(&ws, &win->buffer, bounds, str, behavior,
            &button, i, &config->font);
}

int
zr_button_color(struct zr_context *ctx,
   struct zr_color color, enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    button.normal = color;
    button.hover = color;
    button.active = color;
    return zr_do_button(&ws, &win->buffer, bounds, &button, i, behavior, &bounds);
}

int
zr_button_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_symbol button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_symbol(&ws, &win->buffer, bounds, symbol,
            behavior, &button, i, &config->font);
}

int
zr_button_image(struct zr_context *ctx, struct zr_image image,
    enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_icon button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;
    button.padding = zr_vec2(0,0);
    return zr_do_button_image(&ws, &win->buffer, bounds, image, behavior, &button, i);
}

int
zr_button_text_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    const char *text, zr_flags align, enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_text button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_text_symbol(&ws, &win->buffer, bounds, symbol, text, align,
            behavior, &button, &config->font, i);
}

int
zr_button_text_image(struct zr_context *ctx, struct zr_image img,
    const char *text, zr_flags align, enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_text button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current)
        return zr_false;

    win = ctx->current;
    layout = win->layout;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_NORMAL);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_text_image(&ws, &win->buffer, bounds, img, text, align,
            behavior, &button, &config->font, i);
}

int
zr_select(struct zr_context *ctx, const char *str,
    zr_flags align, int value)
{
    struct zr_rect bounds;
    struct zr_text text;
    const struct zr_style *config;
    struct zr_vec2 item_padding;
    struct zr_color background;

    struct zr_window *win;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return value;

    win = ctx->current;
    zr_panel_alloc_space(&bounds, ctx);
    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);

    background = (!value) ? config->colors[ZR_COLOR_WINDOW]:
        config->colors[ZR_COLOR_SELECTABLE];
    if (zr_input_is_mouse_click_in_rect(&ctx->input, ZR_BUTTON_LEFT, bounds)) {
        background = config->colors[ZR_COLOR_SELECTABLE_HOVER];
        if (zr_input_has_mouse_click_in_rect(&ctx->input, ZR_BUTTON_LEFT, bounds)) {
            if (!zr_input_is_mouse_down(&ctx->input, ZR_BUTTON_LEFT))
                value = !value;
        }
    }

    text.padding.x = item_padding.x;
    text.padding.y = item_padding.y;
    text.background = background;
    text.text = (!value) ? config->colors[ZR_COLOR_TEXT] :
        config->colors[ZR_COLOR_SELECTABLE_TEXT];

    zr_draw_rect(&win->buffer, bounds, 0, background);
    zr_widget_text(&win->buffer, bounds, str, zr_strsiz(str),
        &text, align|ZR_TEXT_MIDDLE, &config->font);
    return value;
}

int
zr_selectable(struct zr_context *ctx, const char *str,
    zr_flags align, int *value)
{
    int old = *value;
    int ret = zr_select(ctx, str, align, old);
    *value = ret;
    return ret != old;
}

static enum zr_widget_state
zr_toggle_base(struct zr_toggle *toggle, struct zr_rect *bounds,
    const struct zr_context *ctx)
{
    const struct zr_style *config;
    struct zr_vec2 item_padding;
    enum zr_widget_state state;
    state = zr_widget(bounds, ctx);
    if (!state) return state;

    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    toggle->rounding = 0;
    toggle->padding.x = item_padding.x;
    toggle->padding.y = item_padding.y;
    toggle->font = config->colors[ZR_COLOR_TEXT];
    toggle->font_background = config->colors[ZR_COLOR_WINDOW];
    return state;
}

int
zr_check(struct zr_context *ctx, const char *text, int active)
{
    zr_checkbox(ctx, text, &active);
    return active;
}

int
zr_checkbox(struct zr_context *ctx, const char *text, int *is_active)
{
    int old;
    struct zr_rect bounds;
    struct zr_toggle toggle;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_style *config;
    const struct zr_input *i;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    old = *is_active;
    win = ctx->current;
    layout = win->layout;
    state = zr_toggle_base(&toggle, &bounds, ctx);
    if (!state) return 0;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    toggle.touch_pad = zr_get_property(ctx, ZR_PROPERTY_TOUCH_PADDING);
    toggle.rounding = config->rounding[ZR_ROUNDING_CHECK];
    toggle.cursor = config->colors[ZR_COLOR_TOGGLE_CURSOR];
    toggle.normal = config->colors[ZR_COLOR_TOGGLE];
    toggle.hover = config->colors[ZR_COLOR_TOGGLE_HOVER];
    zr_do_toggle(&ws, &win->buffer, bounds, is_active, text, ZR_TOGGLE_CHECK,
                        &toggle, i, &config->font);
    return old != *is_active;
}

void
zr_radio(struct zr_context *ctx, const char *text, int *active)
{
    *active = zr_option(ctx, text, *active);
}

int
zr_option(struct zr_context *ctx, const char *text, int is_active)
{
    struct zr_rect bounds;
    struct zr_toggle toggle;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return is_active;

    win = ctx->current;
    layout = win->layout;
    state = zr_toggle_base(&toggle, &bounds, ctx);
    if (!state) return is_active;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    toggle.touch_pad = zr_get_property(ctx, ZR_PROPERTY_TOUCH_PADDING);
    toggle.cursor = config->colors[ZR_COLOR_TOGGLE_CURSOR];
    toggle.normal = config->colors[ZR_COLOR_TOGGLE];
    toggle.hover = config->colors[ZR_COLOR_TOGGLE_HOVER];
    zr_do_toggle(&ws, &win->buffer, bounds, &is_active, text, ZR_TOGGLE_OPTION,
                &toggle, i, &config->font);
    return is_active;
}

float
zr_slide_float(struct zr_context *ctx, float min, float val, float max, float step)
{zr_slider_float(ctx, min, &val, max, step); return val;}

int
zr_slide_int(struct zr_context *ctx, int min, int val, int max, int step)
{zr_slider_int(ctx, min, &val, max, step); return val;}

void
zr_slider_float(struct zr_context *ctx, float min_value, float *value,
    float max_value, float value_step)
{
    struct zr_rect bounds;
    struct zr_slider slider;
    struct zr_vec2 item_padding;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    state = zr_widget(&bounds, ctx);
    if (!state) return;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    slider.padding.x = item_padding.x;
    slider.padding.y = item_padding.y;
    slider.bg = config->colors[ZR_COLOR_SLIDER];
    slider.normal = config->colors[ZR_COLOR_SLIDER_CURSOR];
    slider.hover = config->colors[ZR_COLOR_SLIDER_CURSOR_HOVER];
    slider.active = config->colors[ZR_COLOR_SLIDER_CURSOR_ACTIVE];
    slider.border = config->colors[ZR_COLOR_BORDER];
    slider.rounding = config->rounding[ZR_ROUNDING_SLIDER];
    *value = zr_do_slider(&ws, &win->buffer, bounds, min_value, *value, max_value,
                        value_step, &slider, i);
}

void
zr_slider_int(struct zr_context *ctx, int min_value, int *value,
    int max_value, int value_step)
{
    float val = (float)*value;
    zr_slider_float(ctx, (float)min_value, &val, (float)max_value, (float)value_step);
    *value = (int)val;
}

void
zr_progress(struct zr_context *ctx, zr_size *cur_value, zr_size max_value,
    int is_modifiable)
{
    struct zr_rect bounds;
    struct zr_progress prog;
    struct zr_vec2 item_padding;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_style *config;
    const struct zr_input *i;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    state = zr_widget(&bounds, ctx);
    if (!state) return;
    i = (state == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    prog.padding.x = item_padding.x;
    prog.padding.y = item_padding.y;
    prog.border = config->colors[ZR_COLOR_BORDER];
    prog.background = config->colors[ZR_COLOR_PROGRESS];
    prog.normal = config->colors[ZR_COLOR_PROGRESS_CURSOR];
    prog.hover = config->colors[ZR_COLOR_PROGRESS_CURSOR_HOVER];
    prog.active = config->colors[ZR_COLOR_PROGRESS_CURSOR_ACTIVE];
    *cur_value = zr_do_progress(&ws, &win->buffer, bounds, *cur_value, max_value,
                        is_modifiable, &prog, i);
}

static enum zr_widget_state
zr_edit_base(struct zr_rect *bounds, struct zr_edit *field,
    struct zr_context *ctx)
{
    const struct zr_style *config;
    struct zr_vec2 item_padding;
    enum zr_widget_state state = zr_widget(bounds, ctx);
    if (!state) return state;

    config = &ctx->style;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    field->border_size = 1;
    field->scrollbar_width = config->properties[ZR_PROPERTY_SCROLLBAR_SIZE].x;
    field->rounding = config->rounding[ZR_ROUNDING_INPUT];
    field->padding.x = item_padding.x;
    field->padding.y = item_padding.y;
    field->show_cursor = zr_true;
    field->background = config->colors[ZR_COLOR_INPUT];
    field->border = config->colors[ZR_COLOR_BORDER];
    field->cursor = config->colors[ZR_COLOR_INPUT_CURSOR];
    field->text = config->colors[ZR_COLOR_INPUT_TEXT];
    field->scroll.rounding = config->rounding[ZR_ROUNDING_SCROLLBAR];
    field->scroll.background = config->colors[ZR_COLOR_SCROLLBAR];
    field->scroll.normal = config->colors[ZR_COLOR_SCROLLBAR_CURSOR];
    field->scroll.hover = config->colors[ZR_COLOR_SCROLLBAR_CURSOR_HOVER];
    field->scroll.active = config->colors[ZR_COLOR_SCROLLBAR_CURSOR_ACTIVE];
    field->scroll.border = config->colors[ZR_COLOR_BORDER];
    return state;
}

zr_flags
zr_edit_string(struct zr_context *ctx, zr_flags flags,
    char *memory, zr_size *len, zr_size max, zr_filter filter)
{
    zr_flags active;
    struct zr_buffer buffer;
    max = MAX(1, max);
    *len = MIN(*len, max-1);
    zr_buffer_init_fixed(&buffer, memory, max);
    buffer.allocated = *len;
    active = zr_edit_buffer(ctx, flags, &buffer, filter);
    *len = buffer.allocated;
    return active;
}

zr_flags
zr_edit_buffer(struct zr_context *ctx, zr_flags flags,
    struct zr_buffer *buffer, zr_filter filter)
{
    struct zr_window *win;
    struct zr_input *i;
    zr_flags old_flags, ret_flags = 0;

    enum zr_widget_state state;
    struct zr_rect bounds;
    struct zr_edit field;
    zr_hash hash;

    int *active = 0;
    float *scroll = 0;
    zr_size *cursor = 0;
    struct zr_text_selection *sel = 0;

    /* dummy state for non active edit */
    int dummy_active = 0;
    float dummy_scroll = 0;
    zr_size dummy_cursor = 0;
    struct zr_text_selection dummy_sel = {0,0,0};

    /* make sure correct values */
    ZR_ASSERT(ctx);
    ZR_ASSERT(buffer);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    state = zr_edit_base(&bounds, &field, ctx);
    if (!state) return 0;
    i = (state == ZR_WIDGET_ROM || win->layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;
    if ((flags & ZR_EDIT_READ_ONLY)) {
        field.modifiable = 0;
        field.show_cursor = 0;
    } else {
        field.modifiable = 1;
    }

    /* check if edit is currently hot item */
    hash = win->edit.seq++;
    if (win->edit.active && hash == win->edit.name) {
        active = &win->edit.active;
        cursor = &win->edit.cursor;
        scroll = &win->edit.scrollbar;
        sel =  &win->edit.sel;
    } else {
        active = &dummy_active;
        cursor = &dummy_cursor;
        scroll = &dummy_scroll;
        sel =  &dummy_sel;
    }

    old_flags = (*active) ? ZR_EDIT_ACTIVE: ZR_EDIT_INACTIVE;
    if (!flags || flags == ZR_EDIT_CURSOR) {
        int old = *active;
        i = (flags & ZR_EDIT_READ_ONLY) ? 0: i;
        if (!flags) {
            /* simple edit field with only appending and removing at the end of the buffer */
            buffer->allocated = zr_widget_edit(&win->buffer, bounds,
                (char*)buffer->memory.ptr, buffer->allocated, buffer->memory.size,
                active, 0, &field, filter, i, &ctx->style.font);
        } else {
            /* simple edit field cursor based movement, inserting and removing */
            zr_size glyphs = zr_utf_len((const char*)buffer->memory.ptr, buffer->allocated);
            *cursor = MIN(*cursor, glyphs);
            buffer->allocated = zr_widget_edit(&win->buffer, bounds,
                (char*)buffer->memory.ptr, buffer->allocated , buffer->memory.size,
                active, cursor, &field, filter, i, &ctx->style.font);
        }

        if (dummy_active) {
            /* set hot edit widget state */
            win->edit.active = 1;
            win->edit.name = hash;
            win->edit.scrollbar = 0;
            win->edit.sel.begin = 0;
            win->edit.sel.end = 0;
            win->edit.cursor = 0;
        } else if (old && !*active) {
            win->edit.active = 0;
        }
    } else {
        /* editbox based editing either in single line (edit field) or multiline (edit box) */
        struct zr_edit_box box;
        if (flags & ZR_EDIT_CLIPBOARD)
            zr_edit_box_init_buffer(&box, buffer, &ctx->clip, filter);
        else zr_edit_box_init_buffer(&box, buffer, 0, filter);

        box.glyphs = zr_utf_len((const char*)buffer->memory.ptr, buffer->allocated);
        box.active = *active;
        box.filter = filter;
        box.scrollbar = *scroll;
        *cursor = MIN(box.glyphs, *cursor);
        box.cursor = *cursor;

        if (!(flags & ZR_EDIT_CURSOR)) {
            box.sel.begin = box.cursor;
            box.sel.end = box.cursor;
        } else {
            if (!(flags & ZR_EDIT_SELECTABLE)) {
                box.sel.active = 0;
                box.sel.begin = box.cursor;
                box.sel.end = box.cursor;
            } else box.sel = *sel;
        }

        if (flags & ZR_EDIT_MULTILINE)
            zr_widget_edit_box(&win->buffer, bounds, &box, &field, i, &ctx->style.font);
        else zr_widget_edit_field(&win->buffer, bounds, &box, &field, i, &ctx->style.font);

        if (box.active) {
            /* update hot edit widget state */
            *active = 1;
            win->edit.active = 1;
            win->edit.name = hash;
            win->edit.scrollbar = box.scrollbar;
            win->edit.sel = box.sel;
            win->edit.cursor = box.cursor;
            buffer->allocated = box.buffer.allocated;
        } else if (!box.active && *active) {
            win->edit.active = 0;
        }
    }

    if (*active && (flags & ZR_EDIT_SIGCOMIT) &&
        zr_input_is_key_pressed(i, ZR_KEY_ENTER)) {
        ret_flags |= ZR_EDIT_SIGCOMIT;
        *active = 0;
    }

    /* compress edit widget state and state changes into flags */
    ret_flags |= (*active) ? ZR_EDIT_ACTIVE: ZR_EDIT_INACTIVE;
    if (old_flags == ZR_EDIT_INACTIVE && ret_flags & ZR_EDIT_ACTIVE)
        ret_flags |= ZR_EDIT_ACTIVATED;
    else if (old_flags == ZR_EDIT_ACTIVE && ret_flags & ZR_EDIT_INACTIVE)
        ret_flags |= ZR_EDIT_DEACTIVATED;
    return ret_flags;
}

static void
zr_property(struct zr_context *ctx, const char *name,
    float min, float *val, float max, float step,
    float inc_per_pixel, zr_filter filter)
{
    struct zr_rect bounds;
    enum zr_widget_state s;
    enum zr_widget_status ws;
    struct zr_property prop;
    struct zr_vec2 item_padding;

    int *state = 0;
    int old_state = 0;
    zr_hash hash = 0;
    char *buffer = 0;
    zr_size *len = 0;
    zr_size *cursor = 0;

    char dummy_buffer[ZR_MAX_NUMBER_BUFFER];
    int dummy_state = ZR_PROPERTY_DEFAULT;
    zr_size dummy_length = 0;
    zr_size dummy_cursor = 0;

    struct zr_window *win;
    struct zr_panel *layout;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    win = ctx->current;
    layout = win->layout;
    config = &ctx->style;
    s = zr_widget(&bounds, ctx);
    if (!s) return;
    i = (s == ZR_WIDGET_ROM || layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    /* calculate hash from name */
    if (name[0] == '#') {
        hash = zr_murmur_hash(name, (int)zr_strsiz(name), win->property.seq++);
        name++; /* special number hash */
    } else hash = zr_murmur_hash(name, (int)zr_strsiz(name), 42);

    /* check if property is currently hot item */
    if (win->property.active && hash == win->property.name) {
        buffer = win->property.buffer;
        len = &win->property.length;
        cursor = &win->property.cursor;
        state = &win->property.state;
    } else {
        buffer = dummy_buffer;
        len = &dummy_length;
        cursor = &dummy_cursor;
        state = &dummy_state;
    }

    /* execute property widget */
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    prop.border_size = 1;
    prop.rounding = config->rounding[ZR_ROUNDING_PROPERTY];
    prop.padding = item_padding;
    prop.border = config->colors[ZR_COLOR_BORDER];
    prop.normal = config->colors[ZR_COLOR_PROPERTY];
    prop.hover = config->colors[ZR_COLOR_PROPERTY_HOVER];
    prop.active = config->colors[ZR_COLOR_PROPERTY_ACTIVE];
    prop.text = config->colors[ZR_COLOR_TEXT];
    old_state = *state;
    *val = zr_do_property(&ws, &win->buffer, bounds, name, min, *val, max, step,
        inc_per_pixel, buffer, len, state, cursor, &prop, filter, i, &config->font);

    if (*state != ZR_PROPERTY_DEFAULT && !win->property.active) {
        /* current property is now hot */
        win->property.active = 1;
        zr_memcopy(win->property.buffer, buffer, *len);
        win->property.length = *len;
        win->property.cursor = *cursor;
        win->property.state = *state;
        win->property.name = hash;
    }
    /* check if previously active property is now unactive */
    if (*state == ZR_PROPERTY_DEFAULT && old_state != ZR_PROPERTY_DEFAULT)
        win->property.active = 0;
}

void
zr_property_float(struct zr_context *ctx, const char *name,
    float min, float *val, float max, float step, float inc_per_pixel)
{
    zr_property(ctx, name, min, val, max, step, inc_per_pixel, zr_filter_float);
}

void
zr_property_int(struct zr_context *ctx, const char *name,
    int min, int *val, int max, int step, int inc_per_pixel)
{
    float value = (float)*val;
    zr_property(ctx, name, (float)min, &value, (float)max, (float)step,
        (float)inc_per_pixel, zr_filter_decimal);
    *val = (int)value;
}

float
zr_propertyf(struct zr_context *ctx, const char *name, float min, float val,
    float max, float step, float inc_per_pixel)
{
    zr_property_float(ctx, name, (float)min, &val, (float)max,
        (float)step, (float)inc_per_pixel);
    return val;
}

int
zr_propertyi(struct zr_context *ctx, const char *name, int min, int val, int max,
    int step, int inc_per_pixel)
{
    zr_property_int(ctx, name, min, &val, max, step, inc_per_pixel);
    return val;
}

/* -------------------------------------------------------------
 *
 *                          CHART
 *
 * --------------------------------------------------------------*/
void
zr_chart_begin(struct zr_context *ctx, enum zr_chart_type type,
    zr_size count, float min_value, float max_value)
{
    struct zr_rect bounds = {0, 0, 0, 0};
    struct zr_vec2 item_padding;
    struct zr_color color;

    struct zr_command_buffer *out;
    const struct zr_style *config;
    struct zr_chart *chart;
    struct zr_window *win;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout) return;
    if (!zr_widget(&bounds, ctx)) {
        chart = &ctx->current->layout->chart;
        zr_zero(chart, sizeof(*chart));
        return;
    }

    win = ctx->current;
    out = &win->buffer;
    config = &ctx->style;
    chart = &win->layout->chart;

    /* draw chart background */
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    color = (type == ZR_CHART_LINES) ?
        config->colors[ZR_COLOR_PLOT]: config->colors[ZR_COLOR_HISTO];
    zr_draw_rect(out, bounds, config->rounding[ZR_ROUNDING_CHART], color);

    /* setup basic generic chart  */
    zr_zero(chart, sizeof(*chart));
    chart->type = type;
    chart->index = 0;
    chart->count = count;
    chart->min = MIN(min_value, max_value);
    chart->max = MAX(min_value, max_value);
    chart->range = chart->max - chart->min;
    chart->x = bounds.x + item_padding.x;
    chart->y = bounds.y + item_padding.y;
    chart->w = bounds.w - 2 * item_padding.x;
    chart->h = bounds.h - 2 * item_padding.y;
    chart->w = MAX(chart->w, 2 * item_padding.x);
    chart->h = MAX(chart->h, 2 * item_padding.y);
    chart->last.x = 0; chart->last.y = 0;
}

static zr_flags
zr_chart_push_line(struct zr_context *ctx, struct zr_window *win,
    struct zr_chart *g, float value)
{
    zr_flags ret = 0;
    struct zr_vec2 cur;
    struct zr_rect bounds;
    float step, range, ratio;
    struct zr_color color;

    struct zr_panel *layout = win->layout;
    const struct zr_input *i = &ctx->input;
    const struct zr_style *config = &ctx->style;
    struct zr_command_buffer *out = &win->buffer;

    step = g->w / (float)g->count;
    range = g->max - g->min;
    ratio = (value - g->min) / range;

    if (g->index == 0) {
        /* special case for the first data point since it does not have a connection */
        g->last.x = g->x;
        g->last.y = (g->y + g->h) - ratio * (float)g->h;

        bounds.x = g->last.x - 2;
        bounds.y = g->last.y - 2;
        bounds.w = 4;
        bounds.h = 4;

        color = config->colors[ZR_COLOR_PLOT_LINES];
        if (!(layout->flags & ZR_WINDOW_ROM) &&
            ZR_INBOX(i->mouse.pos.x,i->mouse.pos.y, g->last.x-3, g->last.y-3, 6, 6)){
            ret = zr_input_is_mouse_hovering_rect(i, bounds) ? ZR_CHART_HOVERING : 0;
            ret |= (i->mouse.buttons[ZR_BUTTON_LEFT].down &&
                i->mouse.buttons[ZR_BUTTON_LEFT].clicked) ? ZR_CHART_CLICKED: 0;
            color = config->colors[ZR_COLOR_PLOT_HIGHLIGHT];
        }
        zr_draw_rect(out, bounds, 0, color);
        g->index++;
        return ret;
    }

    /* draw a line between the last data point and the new one */
    cur.x = g->x + (float)(step * (float)g->index);
    cur.y = (g->y + g->h) - (ratio * (float)g->h);
    zr_draw_line(out, g->last.x, g->last.y, cur.x, cur.y,
        config->colors[ZR_COLOR_PLOT_LINES]);

    bounds.x = cur.x - 3;
    bounds.y = cur.y - 3;
    bounds.w = 6;
    bounds.h = 6;

    /* user selection of current data point */
    color = config->colors[ZR_COLOR_PLOT_LINES];
    if (!(layout->flags & ZR_WINDOW_ROM)) {
        if (zr_input_is_mouse_hovering_rect(i, bounds)) {
            ret = ZR_CHART_HOVERING;
            ret |= (!i->mouse.buttons[ZR_BUTTON_LEFT].down &&
                i->mouse.buttons[ZR_BUTTON_LEFT].clicked) ? ZR_CHART_CLICKED: 0;
            color = config->colors[ZR_COLOR_PLOT_HIGHLIGHT];
        }
    }
    zr_draw_rect(out, zr_rect(cur.x - 2, cur.y - 2, 4, 4), 0, color);

    /* save current data point position */
    g->last.x = cur.x;
    g->last.y = cur.y;
    g->index++;
    return ret;
}

static zr_flags
zr_chart_push_column(const struct zr_context *ctx, struct zr_window *win,
    struct zr_chart *chart, float value)
{
    struct zr_command_buffer *out = &win->buffer;
    const struct zr_style *config = &ctx->style;
    const struct zr_input *in = &ctx->input;
    struct zr_panel *layout = win->layout;

    float ratio;
    zr_flags ret = 0;
    struct zr_color color;
    struct zr_rect item = {0,0,0,0};

    if (chart->index >= chart->count)
        return zr_false;
    if (chart->count) {
        float padding = (float)(chart->count-1);
        item.w = (chart->w - padding) / (float)(chart->count);
    }

    /* calculate bounds of the current bar chart entry */
    color = config->colors[ZR_COLOR_HISTO_BARS];
    item.h = chart->h * ZR_ABS((value/chart->range));
    if (value >= 0) {
        ratio = (value + ZR_ABS(chart->min)) / ZR_ABS(chart->range);
        item.y = (chart->y + chart->h) - chart->h * ratio;
    } else {
        ratio = (value - chart->max) / chart->range;
        item.y = chart->y + (chart->h * ZR_ABS(ratio)) - item.h;
    }
    item.x = chart->x + ((float)chart->index * item.w);
    item.x = item.x + ((float)chart->index);

    /* user chart bar selection */
    if (!(layout->flags & ZR_WINDOW_ROM) &&
        ZR_INBOX(in->mouse.pos.x,in->mouse.pos.y,item.x,item.y,item.w,item.h)) {
        ret = ZR_CHART_HOVERING;
        ret |= (!in->mouse.buttons[ZR_BUTTON_LEFT].down &&
                in->mouse.buttons[ZR_BUTTON_LEFT].clicked) ? ZR_CHART_CLICKED: 0;
        color = config->colors[ZR_COLOR_HISTO_HIGHLIGHT];
    }
    zr_draw_rect(out, item, 0, color);
    chart->index++;
    return ret;
}

zr_flags
zr_chart_push(struct zr_context *ctx, float value)
{
    struct zr_window *win;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current)
        return zr_false;

    win = ctx->current;
    switch (win->layout->chart.type) {
    case ZR_CHART_LINES:
        return zr_chart_push_line(ctx, win, &win->layout->chart, value);
    case ZR_CHART_COLUMN:
        return zr_chart_push_column(ctx, win, &win->layout->chart, value);
    default:
    case ZR_CHART_MAX:
        return zr_false;
    }
}

void
zr_chart_end(struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_chart *chart;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current)
        return;

    win = ctx->current;
    chart = &win->layout->chart;
    chart->type = ZR_CHART_MAX;
    chart->index = 0;
    chart->count = 0;
    chart->min = 0;
    chart->max = 0;
    chart->x = 0;
    chart->y = 0;
    chart->w = 0;
    chart->h = 0;
}

/* -------------------------------------------------------------
 *
 *                          GROUP
 *
 * --------------------------------------------------------------*/
int
zr_group_begin(struct zr_context *ctx, struct zr_panel *layout,
    const char *title, zr_flags flags)
{
    union {struct zr_scroll *s; zr_uint *i;} value;
    struct zr_rect bounds;
    const struct zr_rect *c;
    struct zr_window panel;
    int title_len;
    zr_hash title_hash;
    struct zr_window *win;

    ZR_ASSERT(ctx);
    ZR_ASSERT(title);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout || !title)
        return 0;

    /* allocate space for the group panel inside the panel */
    win = ctx->current;
    c = &win->layout->clip;
    zr_panel_alloc_space(&bounds, ctx);
    zr_zero(layout, sizeof(*layout));

    /* find group persistent scrollbar value */
    title_len = (int)zr_strsiz(title);
    title_hash = zr_murmur_hash(title, (int)title_len, ZR_WINDOW_SUB);
    value.i = zr_find_value(win, title_hash);
    if (!value.i) {
        value.i = zr_add_value(ctx, win, title_hash, 0);
        *value.i = 0;
    }

    if (!ZR_INTERSECT(c->x, c->y, c->w, c->h, bounds.x, bounds.y, bounds.w, bounds.h) &&
        !(flags & ZR_WINDOW_MOVABLE)) {
        return 0;
    }

    flags |= ZR_WINDOW_SUB;
    if (win->flags & ZR_WINDOW_ROM)
        flags |= ZR_WINDOW_ROM;

    /* initialize a fake window to create the layout from */
    zr_zero(&panel, sizeof(panel));
    panel.bounds = bounds;
    panel.flags = flags;
    panel.scrollbar.x = (unsigned short)value.s->x;
    panel.scrollbar.y = (unsigned short)value.s->y;
    panel.buffer = win->buffer;
    panel.layout = layout;
    ctx->current = &panel;
    zr_panel_begin(ctx, (flags & ZR_WINDOW_TITLE) ? title: 0);

    win->buffer = panel.buffer;
    layout->offset = value.s;
    layout->parent = win->layout;
    win->layout = layout;
    ctx->current = win;
    return 1;
}

void
zr_group_end(struct zr_context *ctx)
{
    struct zr_rect clip;
    struct zr_window pan;

    struct zr_window *win;
    struct zr_panel *parent;
    struct zr_panel *g;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current)
        return;

    /* make sure zr_group_begin was called correctly */
    ZR_ASSERT(ctx->current);
    win = ctx->current;
    ZR_ASSERT(win->layout);
    g = win->layout;
    ZR_ASSERT(g->parent);
    parent = g->parent;

    /* dummy window */
    zr_zero(&pan, sizeof(pan));
    pan.bounds = g->bounds;
    pan.scrollbar.x = (unsigned short)g->offset->x;
    pan.scrollbar.y = (unsigned short)g->offset->y;
    pan.flags = g->flags|ZR_WINDOW_SUB;
    pan.buffer = win->buffer;
    pan.layout = g;
    ctx->current = &pan;

    /* make sure group has correct clipping rectangle */
    zr_unify(&clip, &parent->clip,
        g->bounds.x, g->clip.y - g->header_h,
        g->bounds.x + g->bounds.w+1,
        g->bounds.y + g->bounds.h + 1);
    zr_draw_scissor(&pan.buffer, clip);
    zr_end(ctx);

    win->buffer = pan.buffer;
    zr_draw_scissor(&win->buffer, parent->clip);
    ctx->current = win;
    win->layout = parent;
    win->bounds = parent->bounds;
}

/* --------------------------------------------------------------
 *
 *                          POPUP
 *
 * --------------------------------------------------------------*/
int
zr_popup_begin(struct zr_context *ctx, struct zr_panel *layout,
    enum zr_popup_type type, const char *title, zr_flags flags, struct zr_rect rect)
{
    zr_hash title_hash;
    int title_len;
    zr_size allocated;
    struct zr_window *popup;
    struct zr_window *win;

    ZR_ASSERT(ctx);
    ZR_ASSERT(title);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    ZR_ASSERT(!(win->flags & ZR_WINDOW_POPUP));
    title_len = (int)zr_strsiz(title);
    title_hash = zr_murmur_hash(title, (int)title_len, ZR_WINDOW_POPUP);

    popup = win->popup.win;
    if (!popup) {
        popup = (struct zr_window*)zr_create_window(ctx);
        win->popup.win = popup;
        win->popup.active = 0;
    }

    /* make sure we have to correct popup */
    if (win->popup.name != title_hash) {
        if (!win->popup.active) {
            zr_zero(popup, sizeof(*popup));
            win->popup.name = title_hash;
            win->popup.active = 1;
        } else return 0;
    }

    /* popup position is local to window */
    ctx->current = popup;
    rect.x += win->layout->clip.x;
    rect.y += win->layout->clip.y;

    /* setup popup data */
    popup->parent = win;
    popup->bounds = rect;
    popup->seq = ctx->seq;
    popup->layout = layout;
    popup->flags = flags;
    popup->flags |= ZR_WINDOW_BORDER|ZR_WINDOW_SUB|ZR_WINDOW_POPUP;
    if (type == ZR_POPUP_DYNAMIC)
        popup->flags |= ZR_WINDOW_DYNAMIC;

    popup->buffer = win->buffer;
    zr_start_popup(ctx, win);
    allocated = ctx->memory.allocated;
    zr_draw_scissor(&popup->buffer, zr_null_rect);

    if (zr_panel_begin(ctx, title)) {
        /* popup is running therefore invalidate parent window  */
        win->layout->flags |= ZR_WINDOW_ROM;
        win->layout->flags &= ~(zr_flags)ZR_WINDOW_REMOVE_ROM;
        win->popup.active = 1;
        layout->offset = &popup->scrollbar;
        return 1;
    } else {
        /* popup was closed/is invalid so cleanup */
        win->layout->flags |= ZR_WINDOW_REMOVE_ROM;
        win->layout->popup_buffer.active = 0;
        win->popup.active = 0;
        ctx->memory.allocated = allocated;
        ctx->current = win;
        return 0;
    }
}

static int
zr_nonblock_begin(struct zr_panel *layout, struct zr_context *ctx,
    zr_flags flags, struct zr_rect body, struct zr_rect header)
{
    int is_active = zr_true;
    struct zr_window *popup;
    struct zr_window *win;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    /* popups cannot have popups */
    win = ctx->current;
    ZR_ASSERT(!(win->flags & ZR_WINDOW_POPUP));
    popup = win->popup.win;
    if (!popup) {
        /* create window for nonblocking popup */
        popup = (struct zr_window*)zr_create_window(ctx);
        win->popup.win = popup;
        zr_command_buffer_init(&popup->buffer, &ctx->memory, ZR_CLIPPING_ON);
    } else {
        /* check if user clicked outside the popup and close if so */
        int in_panel, in_body, in_header;
        in_panel = zr_input_is_mouse_click_in_rect(&ctx->input, ZR_BUTTON_LEFT, win->layout->bounds);
        in_body = zr_input_is_mouse_click_in_rect(&ctx->input, ZR_BUTTON_LEFT, body);
        in_header = zr_input_is_mouse_click_in_rect(&ctx->input, ZR_BUTTON_LEFT, header);
        if (!in_body && in_panel && !in_header)
            is_active = zr_false;
    }

    if (!is_active) {
        win->layout->flags |= ZR_WINDOW_REMOVE_ROM;
        return is_active;
    }

    popup->bounds = body;
    popup->parent = win;
    popup->layout = layout;
    popup->flags = flags;
    popup->flags |= ZR_WINDOW_BORDER|ZR_WINDOW_POPUP;
    popup->flags |= ZR_WINDOW_DYNAMIC|ZR_WINDOW_SUB;
    popup->flags |= ZR_WINDOW_NONBLOCK;
    popup->seq = ctx->seq;
    win->popup.active = 1;

    zr_start_popup(ctx, win);
    popup->buffer = win->buffer;
    zr_draw_scissor(&popup->buffer, zr_null_rect);
    ctx->current = popup;

    zr_panel_begin(ctx, 0);
    win->buffer = popup->buffer;
    win->layout->flags |= ZR_WINDOW_ROM;
    layout->offset = &popup->scrollbar;
    return is_active;
}

void
zr_popup_close(struct zr_context *ctx)
{
    struct zr_window *popup;
    ZR_ASSERT(ctx);
    if (!ctx || !ctx->current) return;
    popup = ctx->current;
    ZR_ASSERT(popup->parent);
    ZR_ASSERT(popup->flags & ZR_WINDOW_POPUP);
    popup->flags |= ZR_WINDOW_HIDDEN;
}

void
zr_popup_end(struct zr_context *ctx)
{
    struct zr_window *win;
    struct zr_window *popup;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    popup = ctx->current;
    ZR_ASSERT(popup->parent);
    win = popup->parent;
    if (popup->flags & ZR_WINDOW_HIDDEN) {
        win->layout->flags |= ZR_WINDOW_REMOVE_ROM;
        win->popup.active = 0;
    }
    zr_draw_scissor(&popup->buffer, zr_null_rect);
    zr_end(ctx);

    win->buffer = popup->buffer;
    zr_finish_popup(ctx, win);
    ctx->current = win;
    zr_draw_scissor(&win->buffer, win->layout->clip);
}
/* -------------------------------------------------------------
 *
 *                          TOOLTIP
 *
 * -------------------------------------------------------------- */
int
zr_tooltip_begin(struct zr_context *ctx, struct zr_panel *layout, float width)
{
    int ret;
    struct zr_rect bounds;
    struct zr_window *win;
    const struct zr_input *in;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    /* make sure that no nonblocking popup is currently active */
    win = ctx->current;
    in = &ctx->input;
    if (win->popup.win && (win->popup.win->flags & ZR_WINDOW_NONBLOCK))
        return 0;

    bounds.w = width;
    bounds.h = zr_null_rect.h;
    bounds.x = (in->mouse.pos.x + 1) - win->layout->clip.x;
    bounds.y = (in->mouse.pos.y + 1) - win->layout->clip.y;

    ret = zr_popup_begin(ctx, layout, ZR_POPUP_DYNAMIC,
        "__##Tooltip##__", ZR_WINDOW_NO_SCROLLBAR|ZR_WINDOW_TOOLTIP, bounds);
    if (ret) win->layout->flags &= ~(zr_flags)ZR_WINDOW_ROM;
    return ret;
}

void
zr_tooltip_end(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    if (!ctx || !ctx->current)
        return;
    zr_popup_close(ctx);
    zr_popup_end(ctx);
}

void
zr_tooltip(struct zr_context *ctx, const char *text)
{
    zr_size text_len;
    zr_size text_width;
    zr_size text_height;

    const struct zr_style *config;
    struct zr_vec2 item_padding;
    struct zr_vec2 padding;
    struct zr_panel layout;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    ZR_ASSERT(text);
    if (!ctx || !ctx->current || !ctx->current->layout || !text)
        return;

    /* fetch configuration data */
    config = &ctx->style;
    padding = config->properties[ZR_PROPERTY_PADDING];
    item_padding = config->properties[ZR_PROPERTY_ITEM_PADDING];

    /* calculate size of the text and tooltip */
    text_len = zr_strsiz(text);
    text_width = config->font.width(config->font.userdata,
                        config->font.height, text, text_len);
    text_width += (zr_size)(2 * padding.x + 2 * item_padding.x);
    text_height = (zr_size)(config->font.height + 2 * item_padding.y);

    /* execute tooltip and fill with text */
    if (zr_tooltip_begin(ctx, &layout, (float)text_width)) {
        zr_layout_row_dynamic(ctx, (float)text_height, 1);
        zr_text(ctx, text, text_len, ZR_TEXT_LEFT);
        zr_tooltip_end(ctx);
    }
}

/*
 * -------------------------------------------------------------
 *
 *                          CONTEXTUAL
 *
 * --------------------------------------------------------------
 */
int
zr_contextual_begin(struct zr_context *ctx, struct zr_panel *layout,
    zr_flags flags, struct zr_vec2 size, struct zr_rect trigger_bounds)
{
    static const struct zr_rect null_rect = {0,0,0,0};
    int is_clicked = 0;
    int is_active = 0;
    int is_open = 0;
    int ret = 0;

    struct zr_window *win;
    struct zr_window *popup;
    struct zr_rect body;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    ++win->popup.con_count;

    /* check if currently active contextual is active */
    popup = win->popup.win;
    is_open = (popup && (popup->flags & ZR_WINDOW_CONTEXTUAL) && win->popup.type == ZR_WINDOW_CONTEXTUAL);
    is_clicked = zr_input_mouse_clicked(&ctx->input, ZR_BUTTON_RIGHT, trigger_bounds);
    if (win->popup.active_con && win->popup.con_count != win->popup.active_con)
        return 0;
    if ((is_clicked && is_open && !is_active) || (!is_open && !is_active && !is_clicked))
        return 0;

    /* calculate contextual position on click */
    win->popup.active_con = win->popup.con_count;
    if (is_clicked) {
        body.x = ctx->input.mouse.pos.x;
        body.y = ctx->input.mouse.pos.y;
    } else {
        body.x = popup->bounds.x;
        body.y = popup->bounds.y;
    }
    body.w = size.x;
    body.h = size.y;

    /* start nonblocking contextual popup */
    ret = zr_nonblock_begin(layout, ctx,
            flags|ZR_WINDOW_CONTEXTUAL|ZR_WINDOW_NO_SCROLLBAR, body, null_rect);
    if (ret) win->popup.type = ZR_WINDOW_CONTEXTUAL;
    else {
        win->popup.active_con = 0;
        win->popup.win->flags = 0;
    }
    return ret;
}

static int
zr_contextual_button(struct zr_context *ctx, const char *text,
    zr_flags align, enum zr_button_behavior behavior)
{
    struct zr_button_text button;
    struct zr_rect bounds;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    config = &ctx->style;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_FITTING);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || win->layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    button.base.border_width = 0;
    button.base.normal = config->colors[ZR_COLOR_WINDOW];
    button.base.border = config->colors[ZR_COLOR_WINDOW];
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    button.alignment = align|ZR_TEXT_MIDDLE;
    return zr_do_button_text(&ws, &win->buffer, bounds, text,  behavior,
            &button, i, &config->font);
}

static int
zr_contextual_button_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    const char *text, zr_flags align, enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_text button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    const struct zr_style *config;
    const struct zr_input *i;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_FITTING);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || win->layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
    button.base.border_width = 0;
    button.base.normal = config->colors[ZR_COLOR_WINDOW];
    button.base.border = config->colors[ZR_COLOR_WINDOW];
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_text_symbol(&ws, &win->buffer, bounds, symbol, text, align,
            behavior, &button, &config->font, i);
}

static int
zr_contextual_button_icon(struct zr_context *ctx, struct zr_image img,
    const char *text, zr_flags align, enum zr_button_behavior behavior)
{
    struct zr_rect bounds;
    struct zr_button_text button;
    enum zr_widget_status ws;
    enum zr_widget_state state;

    struct zr_window *win;
    const struct zr_input *i;
    const struct zr_style *config;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    state = zr_button(&button.base, &bounds, ctx, ZR_BUTTON_FITTING);
    if (!state) return zr_false;
    i = (state == ZR_WIDGET_ROM || win->layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input;

    config = &ctx->style;
    button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
    button.base.border_width = 0;
    button.base.normal = config->colors[ZR_COLOR_WINDOW];
    button.base.border = config->colors[ZR_COLOR_WINDOW];
    button.normal = config->colors[ZR_COLOR_TEXT];
    button.hover = config->colors[ZR_COLOR_TEXT_HOVERING];
    button.active = config->colors[ZR_COLOR_TEXT_ACTIVE];
    return zr_do_button_text_image(&ws, &win->buffer, bounds, img, text, align,
                                behavior, &button, &config->font, i);
}

int
zr_contextual_item(struct zr_context *ctx, const char *title, zr_flags align)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    if (zr_contextual_button(ctx, title, align, ZR_BUTTON_DEFAULT)) {
        zr_contextual_close(ctx);
        return zr_true;
    }
    return zr_false;
}

int
zr_contextual_item_icon(struct zr_context *ctx, struct zr_image img,
    const char *title, zr_flags align)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    if (zr_contextual_button_icon(ctx, img, title, align, ZR_BUTTON_DEFAULT)){
        zr_contextual_close(ctx);
        return zr_true;
    }
    return zr_false;
}

int
zr_contextual_item_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    const char *title, zr_flags align)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    if (zr_contextual_button_symbol(ctx, symbol, title, align, ZR_BUTTON_DEFAULT)){
        zr_contextual_close(ctx);
        return zr_true;
    }
    return zr_false;
}

void
zr_contextual_close(struct zr_context *ctx)
{
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return;

    if (!ctx->current)
        return;
    zr_popup_close(ctx);
}

void
zr_contextual_end(struct zr_context *ctx)
{
    struct zr_window *popup;
    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    popup = ctx->current;
    ZR_ASSERT(popup->parent);
    if (popup->flags & ZR_WINDOW_HIDDEN)
        popup->seq = 0;
    zr_popup_end(ctx);
    return;
}
/* -------------------------------------------------------------
 *
 *                          COMBO
 *
 * --------------------------------------------------------------*/
static int
zr_combo_begin(struct zr_panel *layout, struct zr_context *ctx, struct zr_window *win,
    int height, int is_clicked, struct zr_rect header)
{
    int is_open = 0;
    int is_active = 0;
    struct zr_rect body;
    struct zr_window *popup;
    zr_hash hash;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    popup = win->popup.win;
    body.x = header.x;
    body.w = header.w;
    body.y = header.y + header.h-1;
    body.h = (float)height;

    hash = win->popup.combo_count++;
    is_open = (popup && (popup->flags & ZR_WINDOW_COMBO));
    is_active = (popup && (win->popup.name == hash) && win->popup.type == ZR_WINDOW_COMBO);
    if ((is_clicked && is_open && !is_active) || (is_open && !is_active) ||
        (!is_open && !is_active && !is_clicked)) return 0;
    if (!zr_nonblock_begin(layout, ctx, ZR_WINDOW_COMBO|ZR_WINDOW_NO_SCROLLBAR,
            body, zr_rect(0,0,0,0))) return 0;

    win->popup.type = ZR_WINDOW_COMBO;
    win->popup.name = hash;
    return 1;
}

int
zr_combo_begin_text(struct zr_context *ctx, struct zr_panel *layout,
    const char *selected, int height)
{
    const struct zr_input *in;
    struct zr_window *win;
    enum zr_widget_status state;
    enum zr_widget_state s;
    struct zr_vec2 item_padding;
    struct zr_rect header;
    int is_active = zr_false;

    ZR_ASSERT(ctx);
    ZR_ASSERT(selected);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout || !selected)
        return 0;

    win = ctx->current;
    s = zr_widget(&header, ctx);
    if (s == ZR_WIDGET_INVALID)
        return 0;

    in = (win->layout->flags & ZR_WINDOW_ROM || s == ZR_WIDGET_ROM)? 0: &ctx->input;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    if (zr_button_behavior(&state, header, in, ZR_BUTTON_DEFAULT))
        is_active = zr_true;

    /* draw combo box header background and border */
    zr_draw_rect(&win->buffer, header, 0, ctx->style.colors[ZR_COLOR_BORDER]);
    zr_draw_rect(&win->buffer, zr_shrink_rect(header, 1), 0,
        ctx->style.colors[ZR_COLOR_COMBO]);

    {
        /* print currently selected text item */
        struct zr_rect label;
        struct zr_text text;
        struct zr_symbol sym;
        struct zr_rect bounds = {0,0,0,0};
        zr_size text_len = zr_strsiz(selected);

        /* draw selected label */
        text.padding = zr_vec2(0,0);
        text.background = ctx->style.colors[ZR_COLOR_COMBO];
        text.text = ctx->style.colors[ZR_COLOR_TEXT];

        label.x = header.x + item_padding.x;
        label.y = header.y + item_padding.y;
        label.w = header.w - (header.h + 2 * item_padding.x);
        label.h = header.h - 2 * item_padding.y;
        zr_widget_text(&win->buffer, label, selected, text_len, &text,
            ZR_TEXT_LEFT|ZR_TEXT_MIDDLE, &ctx->style.font);

        /* draw open/close symbol */
        bounds.y = label.y + label.h/2 - ctx->style.font.height/2;
        bounds.w = bounds.h = ctx->style.font.height;
        bounds.x = (header.x + header.w) - (bounds.w + 2 * item_padding.x);

        sym.type = ZR_SYMBOL_TRIANGLE_DOWN;
        sym.background = ctx->style.colors[ZR_COLOR_COMBO];
        sym.foreground = ctx->style.colors[ZR_COLOR_TEXT];
        sym.border_width = 1.0f;
        zr_draw_symbol(&win->buffer, &sym, bounds, &ctx->style.font);
    }
    return zr_combo_begin(layout, ctx, win, height, is_active, header);
}

int
zr_combo_begin_color(struct zr_context *ctx, struct zr_panel *layout,
    struct zr_color color, int height)
{
    enum zr_widget_status state;
    enum zr_widget_state s;
    struct zr_vec2 item_padding;
    struct zr_rect header;
    int is_active = zr_false;
    struct zr_window *win;
    const struct zr_input *in;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    s = zr_widget(&header, ctx);
    if (s == ZR_WIDGET_INVALID)
        return 0;

    in = (win->layout->flags & ZR_WINDOW_ROM || s == ZR_WIDGET_ROM)? 0: &ctx->input;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    if (zr_button_behavior(&state, header, in, ZR_BUTTON_DEFAULT))
        is_active = !is_active;

    /* draw combo box header background and border */
    zr_draw_rect(&win->buffer, header, 0, ctx->style.colors[ZR_COLOR_BORDER]);
    zr_draw_rect(&win->buffer, zr_shrink_rect(header, 1), 0,
        ctx->style.colors[ZR_COLOR_COMBO]);

    {
        /* print currently selected string */
        struct zr_rect content;
        struct zr_symbol sym;
        struct zr_rect bounds = {0,0,0,0};

        /* draw color */
        content.h = header.h - 4 * item_padding.y;
        content.y = header.y + 2 * item_padding.y;
        content.x = header.x + 2 * item_padding.x;
        content.w = header.w - (header.h + 4 * item_padding.x);
        zr_draw_rect(&win->buffer, content, 0, color);

        /* draw open/close symbol */
        bounds.y = (header.y + item_padding.y) + (header.h-2.0f*item_padding.y)/2.0f
                    -ctx->style.font.height/2;
        bounds.w = bounds.h = ctx->style.font.height;
        bounds.x = (header.x + header.w) - (bounds.w + 2 * item_padding.x);

        sym.type = ZR_SYMBOL_TRIANGLE_DOWN;
        sym.background = ctx->style.colors[ZR_COLOR_COMBO];
        sym.foreground = ctx->style.colors[ZR_COLOR_TEXT];
        sym.border_width = 1.0f;
        zr_draw_symbol(&win->buffer, &sym, bounds, &ctx->style.font);
    }
    return zr_combo_begin(layout, ctx, win, height, is_active, header);
}

int
zr_combo_begin_image(struct zr_context *ctx, struct zr_panel *layout,
    struct zr_image img, int height)
{
    struct zr_window *win;
    const struct zr_input *in;
    enum zr_widget_status state;
    struct zr_vec2 item_padding;
    struct zr_rect header;
    int is_active = zr_false;
    enum zr_widget_state s;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    s = zr_widget(&header, ctx);
    if (s == ZR_WIDGET_INVALID)
        return 0;

    in = (win->layout->flags & ZR_WINDOW_ROM || s == ZR_WIDGET_ROM)? 0: &ctx->input;
    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    if (zr_button_behavior(&state, header, in, ZR_BUTTON_DEFAULT))
        is_active = !is_active;

    /* draw combo box header background and border */
    zr_draw_rect(&win->buffer, header, 0, ctx->style.colors[ZR_COLOR_BORDER]);
    zr_draw_rect(&win->buffer, zr_shrink_rect(header, 1), 0,
        ctx->style.colors[ZR_COLOR_COMBO]);

    {
        struct zr_rect bounds = {0,0,0,0};
        struct zr_symbol sym;
        struct zr_rect content;

        /* draw image */
        content.h = header.h - 4 * item_padding.y;
        content.y = header.y + 2 * item_padding.y;
        content.x = header.x + 2 * item_padding.x;
        content.w = header.w - (header.h + 4 * item_padding.x);
        zr_draw_image(&win->buffer, content, &img);

        /* draw open/close symbol */
        bounds.y = (header.y + item_padding.y) + (header.h-2.0f*item_padding.y)/2.0f
                    -ctx->style.font.height/2;
        bounds.w = bounds.h = ctx->style.font.height;
        bounds.x = (header.x + header.w) - (bounds.w + 2 * item_padding.x);

        sym.type = ZR_SYMBOL_TRIANGLE_DOWN;
        sym.background = ctx->style.colors[ZR_COLOR_COMBO];
        sym.foreground = ctx->style.colors[ZR_COLOR_TEXT];
        sym.border_width = 1.0f;
        zr_draw_symbol(&win->buffer, &sym, bounds, &ctx->style.font);
    }
    return zr_combo_begin(layout, ctx, win, height, is_active, header);
}

int
zr_combo_begin_icon(struct zr_context *ctx, struct zr_panel *layout,
    const char *selected, struct zr_image img, int height)
{
    struct zr_window *win;
    enum zr_widget_status state;
    struct zr_vec2 item_padding;
    struct zr_rect header;
    int is_active = zr_false;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    if (!zr_widget(&header, ctx))
        return 0;

    item_padding = zr_get_property(ctx, ZR_PROPERTY_ITEM_PADDING);
    if (zr_button_behavior(&state, header, &ctx->input, ZR_BUTTON_DEFAULT))
        is_active = !is_active;

    /* draw combo box header background and border */
    zr_draw_rect(&win->buffer, header, 0, ctx->style.colors[ZR_COLOR_BORDER]);
    zr_draw_rect(&win->buffer, zr_shrink_rect(header, 1), 0,
        ctx->style.colors[ZR_COLOR_COMBO]);

    {
        zr_size text_len;
        struct zr_symbol sym;
        struct zr_rect content;
        struct zr_rect label, icon;
        struct zr_rect bounds = {0,0,0,0};

        content.h = header.h - 4 * item_padding.y;
        content.y = header.y + 2 * item_padding.y;
        content.x = header.x + 2 * item_padding.x;
        content.w = header.w - (header.h + 4 * item_padding.x);

        /* draw icon */
        icon.x = content.x;
        icon.y = content.y;
        icon.h = content.h;
        icon.w = icon.h;
        zr_draw_image(&win->buffer, icon, &img);

        /* draw label */
        label.x = icon.x + icon.w + 2 * item_padding.x;
        label.y = content.y;
        label.w = (content.x + content.w) - (icon.x + icon.w);
        label.h = content.h;
        text_len = zr_strsiz(selected);
        zr_draw_text(&win->buffer, label, selected, text_len, &ctx->style.font,
            ctx->style.colors[ZR_COLOR_WINDOW], ctx->style.colors[ZR_COLOR_TEXT]);

        bounds.y = (header.y + item_padding.y) + (header.h-2.0f*item_padding.y)/2.0f -
            ctx->style.font.height/2;
        bounds.w = bounds.h = ctx->style.font.height;
        bounds.x = (header.x + header.w) - (bounds.w + 2 * item_padding.x);

        /* draw open/close symbol */
        sym.type = ZR_SYMBOL_TRIANGLE_DOWN;
        sym.background = ctx->style.colors[ZR_COLOR_COMBO];
        sym.foreground = ctx->style.colors[ZR_COLOR_TEXT];
        sym.border_width = 1.0f;
        zr_draw_symbol(&win->buffer, &sym, bounds, &ctx->style.font);
    }
    return zr_combo_begin(layout, ctx, win, height, is_active, header);
}

int zr_combo_item(struct zr_context *ctx, const char *title, zr_flags align)
{return zr_contextual_item(ctx, title, align);}

int zr_combo_item_icon(struct zr_context *ctx, struct zr_image img,
    const char *title, zr_flags align)
{return zr_contextual_item_icon(ctx, img, title, align);}

int zr_combo_item_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    const char *title, zr_flags align)
{return zr_contextual_item_symbol(ctx, symbol, title, align);}

void zr_combo_end(struct zr_context *ctx)
{zr_contextual_end(ctx);}

void zr_combo_close(struct zr_context *ctx)
{zr_contextual_close(ctx);}
/*
 * -------------------------------------------------------------
 *
 *                          MENU
 *
 * --------------------------------------------------------------
 */
static int
zr_menu_begin(struct zr_panel *layout, struct zr_context *ctx, struct zr_window *win,
    const char *id, int is_clicked, struct zr_rect header, float width)
{
    int is_open = 0;
    int is_active = 0;
    struct zr_rect body;
    struct zr_window *popup;
    zr_hash hash = zr_murmur_hash(id, (int)zr_strsiz(id), ZR_WINDOW_MENU);

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    body.x = header.x;
    body.w = width;
    body.y = header.y + header.h;
    body.h = (win->layout->bounds.y + win->layout->bounds.h) - body.y;

    popup = win->popup.win;
    is_open = (popup && (popup->flags & ZR_WINDOW_MENU));
    is_active = (popup && (win->popup.name == hash) && win->popup.type == ZR_WINDOW_MENU);
    if ((is_clicked && is_open && !is_active) || (is_open && !is_active) ||
        (!is_open && !is_active && !is_clicked)) return 0;
    if (!zr_nonblock_begin(layout, ctx, ZR_WINDOW_MENU|ZR_WINDOW_NO_SCROLLBAR, body, header))
        return 0;
    win->popup.type = ZR_WINDOW_MENU;
    win->popup.name = hash;
    return 1;
}

int
zr_menu_text_begin(struct zr_context *ctx, struct zr_panel *layout,
    const char *title, float width)
{
    struct zr_window *win;
    const struct zr_input *in;
    struct zr_rect header;
    int is_clicked = zr_false;
    enum zr_widget_status state;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    {
        /* execute menu text button for open/closing the popup */
        struct zr_button_text button;
        zr_zero(&button, sizeof(button));
        if (!zr_button(&button.base, &header, ctx, ZR_BUTTON_NORMAL))
            return 0;

        win = ctx->current;
        button.base.rounding = 0;
        button.base.border_width = 0;
        button.base.border = ctx->style.colors[ZR_COLOR_WINDOW];
        button.base.normal = ctx->style.colors[ZR_COLOR_WINDOW];
        button.base.active = ctx->style.colors[ZR_COLOR_WINDOW];
        button.alignment = ZR_TEXT_CENTERED|ZR_TEXT_MIDDLE;
        button.normal = ctx->style.colors[ZR_COLOR_TEXT];
        button.active = ctx->style.colors[ZR_COLOR_TEXT];
        button.hover = ctx->style.colors[ZR_COLOR_TEXT];
        in = (win->layout->flags & ZR_WINDOW_ROM) ? 0: &ctx->input;
        if (zr_do_button_text(&state, &win->buffer, header,
            title, ZR_BUTTON_DEFAULT, &button, in, &ctx->style.font))
            is_clicked = zr_true;
    }
    return zr_menu_begin(layout, ctx, win, title, is_clicked, header, width);
}

int
zr_menu_icon_begin(struct zr_context *ctx, struct zr_panel *layout,
    const char *id, struct zr_image img, float width)
{
    struct zr_window *win;
    struct zr_rect header;
    int is_clicked = zr_false;
    enum zr_widget_status state;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    {
        /* execute menu icon button for open/closing the popup */
        struct zr_button_icon button;
        zr_zero(&button, sizeof(button));
        if (!zr_button(&button.base, &header, ctx, ZR_BUTTON_NORMAL))
            return 0;

        button.base.rounding = 1;
        button.base.border = ctx->style.colors[ZR_COLOR_BORDER];
        button.base.normal = ctx->style.colors[ZR_COLOR_WINDOW];
        button.base.active = ctx->style.colors[ZR_COLOR_WINDOW];
        button.padding = ctx->style.properties[ZR_PROPERTY_ITEM_PADDING];
        if (zr_do_button_image(&state, &win->buffer, header, img, ZR_BUTTON_DEFAULT,
                &button, (win->layout->flags & ZR_WINDOW_ROM)?0:&ctx->input))
            is_clicked = zr_true;
    }
    return zr_menu_begin(layout, ctx, win, id, is_clicked, header, width);
}

int
zr_menu_symbol_begin(struct zr_context *ctx, struct zr_panel *layout,
    const char *id, enum zr_symbol_type sym, float width)
{
    struct zr_window *win;
    struct zr_rect header;
    int is_clicked = zr_false;
    enum zr_widget_status state;

    ZR_ASSERT(ctx);
    ZR_ASSERT(ctx->current);
    ZR_ASSERT(ctx->current->layout);
    if (!ctx || !ctx->current || !ctx->current->layout)
        return 0;

    win = ctx->current;
    {
        /* execute menu symbol button for open/closing the popup */
        struct zr_button_symbol button;
        zr_zero(&button, sizeof(button));
        if (!zr_button(&button.base, &header, ctx, ZR_BUTTON_NORMAL))
            return 0;

        button.base.rounding = 1;
        button.base.border = ctx->style.colors[ZR_COLOR_BORDER];
        button.base.normal = ctx->style.colors[ZR_COLOR_WINDOW];
        button.base.active = ctx->style.colors[ZR_COLOR_WINDOW];
        button.normal = ctx->style.colors[ZR_COLOR_TEXT];
        button.active = ctx->style.colors[ZR_COLOR_TEXT];
        button.hover = ctx->style.colors[ZR_COLOR_TEXT];
        if (zr_do_button_symbol(&state, &win->buffer, header, sym, ZR_BUTTON_DEFAULT,
                &button, (win->layout->flags & ZR_WINDOW_ROM) ? 0 : &ctx->input,
                &ctx->style.font)) is_clicked = zr_true;
    }
    return zr_menu_begin(layout, ctx, win, id, is_clicked, header, width);
}

int zr_menu_item(struct zr_context *ctx, zr_flags align, const char *title)
{return zr_contextual_item(ctx, title, align);}

int zr_menu_item_icon(struct zr_context *ctx, struct zr_image img,
    const char *title, zr_flags align)
{ return zr_contextual_item_icon(ctx, img, title, align);}

int zr_menu_item_symbol(struct zr_context *ctx, enum zr_symbol_type symbol,
    const char *title, zr_flags align)
{return zr_contextual_item_symbol(ctx, symbol, title, align);}

void zr_menu_close(struct zr_context *ctx)
{zr_contextual_close(ctx);}

void
zr_menu_end(struct zr_context *ctx)
{zr_contextual_end(ctx);}

