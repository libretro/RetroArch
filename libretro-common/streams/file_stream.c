/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_stream.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#define VFS_ERROR_RETURN_VALUE -1

struct RFILE
{
   struct retro_vfs_file_handle *hfile;
   bool err_flag;
};

static retro_vfs_get_path_t filestream_get_path_cb = NULL;
static retro_vfs_open_t filestream_open_cb         = NULL;
static retro_vfs_close_t filestream_close_cb       = NULL;
static retro_vfs_size_t filestream_size_cb         = NULL;
static retro_vfs_truncate_t filestream_truncate_cb = NULL;
static retro_vfs_tell_t filestream_tell_cb         = NULL;
static retro_vfs_seek_t filestream_seek_cb         = NULL;
static retro_vfs_read_t filestream_read_cb         = NULL;
static retro_vfs_write_t filestream_write_cb       = NULL;
static retro_vfs_flush_t filestream_flush_cb       = NULL;
static retro_vfs_remove_t filestream_remove_cb     = NULL;
static retro_vfs_rename_t filestream_rename_cb     = NULL;

/* VFS Initialization */

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface *
      vfs_iface           = vfs_info->iface;

   filestream_get_path_cb = NULL;
   filestream_open_cb     = NULL;
   filestream_close_cb    = NULL;
   filestream_tell_cb     = NULL;
   filestream_size_cb     = NULL;
   filestream_truncate_cb = NULL;
   filestream_seek_cb     = NULL;
   filestream_read_cb     = NULL;
   filestream_write_cb    = NULL;
   filestream_flush_cb    = NULL;
   filestream_remove_cb   = NULL;
   filestream_rename_cb   = NULL;

   if (
             (vfs_info->required_interface_version <
             FILESTREAM_REQUIRED_VFS_VERSION)
         || !vfs_iface)
      return;

   filestream_get_path_cb = vfs_iface->get_path;
   filestream_open_cb     = vfs_iface->open;
   filestream_close_cb    = vfs_iface->close;
   filestream_size_cb     = vfs_iface->size;
   filestream_truncate_cb = vfs_iface->truncate;
   filestream_tell_cb     = vfs_iface->tell;
   filestream_seek_cb     = vfs_iface->seek;
   filestream_read_cb     = vfs_iface->read;
   filestream_write_cb    = vfs_iface->write;
   filestream_flush_cb    = vfs_iface->flush;
   filestream_remove_cb   = vfs_iface->remove;
   filestream_rename_cb   = vfs_iface->rename;
}

/* Callback wrappers */
bool filestream_exists(const char *path)
{
   RFILE *dummy           = NULL;

   if (!path || !*path)
      return false;
   if (!(dummy = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return false;

   if (filestream_close(dummy) != 0)
      free(dummy);

   dummy = NULL;
   return true;
}

int64_t filestream_get_size(RFILE *stream)
{
   int64_t output;

   if (filestream_size_cb)
      output = filestream_size_cb(stream->hfile);
   else
      output = retro_vfs_file_size_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int64_t filestream_truncate(RFILE *stream, int64_t length)
{
   int64_t output;

   if (filestream_truncate_cb)
      output = filestream_truncate_cb(stream->hfile, length);
   else
      output = retro_vfs_file_truncate_impl(
            (libretro_vfs_implementation_file*)stream->hfile, length);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

RFILE* filestream_open(const char *path, unsigned mode, unsigned hints)
{
   struct retro_vfs_file_handle  *fp = NULL;
   RFILE* output                     = (RFILE*)malloc(sizeof(RFILE));

   if (!output)
      return NULL;

   if (filestream_open_cb)
      fp = (struct retro_vfs_file_handle*)
         filestream_open_cb(path, mode, hints);
   else
      fp = (struct retro_vfs_file_handle*)
         retro_vfs_file_open_impl(path, mode, hints);

   if (!fp)
   {
      free(output);
      return NULL;
   }

   output->err_flag = false;
   output->hfile    = fp;
   return output;
}

char* filestream_gets(RFILE *stream, char *s, size_t len)
{
   int c   = 0;
   char *p = s;
   if (!stream)
      return NULL;

   /* get max bytes or up to a newline */

   for (len--; len > 0; len--)
   {
      if ((c = filestream_getc(stream)) == EOF)
         break;
      *p++ = c;
      if (c == '\n')
         break;
   }
   *p = 0;

   if (p == s && c == EOF)
      return NULL;
   return (s);
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   if (stream && filestream_read(stream, &c, 1) == 1)
      return (int)(unsigned char)c;
   return EOF;
}

/* ---- internal scanf engine ----------------------------------------
 *
 * The previous implementation built a per-conversion sub-format
 * string (e.g. "%d%n") and invoked sscanf() for every specifier.
 * sscanf on most libcs runs strlen() over the entire remaining
 * input and (on glibc) allocates a transient FILE-stream state
 * per call. For input parsed one conversion at a time this is
 * the dominant cost.
 *
 * The native engine below scans common conversions in place
 * (%d %i %u %o %x %X %c %s %[ %n %%) with optional '*' suppress,
 * width, and length modifiers hh/h/l/ll/z. Floats, %p, the
 * j/t/L length modifiers, and wide-char variants fall back to
 * the original sscanf+%n path to preserve exact behavior.
 */

#define FS_LM_NONE  0
#define FS_LM_HH    1
#define FS_LM_H     2
#define FS_LM_L     3
#define FS_LM_LL    4
#define FS_LM_J     5
#define FS_LM_Z     6
#define FS_LM_T     7
#define FS_LM_BIG_L 8

typedef struct
{
   int  width;       /* 0 means unbounded */
   int  length_mod;  /* FS_LM_* */
   char specifier;   /* d i u o x X c s [ n % f e g a F E G A p */
   bool suppress;    /* %* */
   /* For '[' specifiers: bounds of the scanset (after '[' or '[^',
    * up to but not including ']'). */
   const char *set_start;
   const char *set_end;
   bool        set_negate;
} fs_scan_spec_t;

/* Parse one conversion specifier starting at `fmt` (which points
 * just past '%'). Returns updated fmt position, or NULL if the
 * specifier is malformed. */
static const char *fs_parse_spec(const char *fmt, fs_scan_spec_t *sp)
{
   sp->width      = 0;
   sp->length_mod = FS_LM_NONE;
   sp->specifier  = '\0';
   sp->suppress   = false;
   sp->set_start  = NULL;
   sp->set_end    = NULL;
   sp->set_negate = false;

   if (*fmt == '*')
   {
      sp->suppress = true;
      fmt++;
   }
   while (*fmt >= '0' && *fmt <= '9')
   {
      sp->width = sp->width * 10 + (*fmt - '0');
      fmt++;
   }
   switch (*fmt)
   {
      case 'h':
         if (fmt[1] == 'h') { sp->length_mod = FS_LM_HH; fmt += 2; }
         else               { sp->length_mod = FS_LM_H;  fmt += 1; }
         break;
      case 'l':
         if (fmt[1] == 'l') { sp->length_mod = FS_LM_LL; fmt += 2; }
         else               { sp->length_mod = FS_LM_L;  fmt += 1; }
         break;
      case 'j': sp->length_mod = FS_LM_J;     fmt++; break;
      case 'z': sp->length_mod = FS_LM_Z;     fmt++; break;
      case 't': sp->length_mod = FS_LM_T;     fmt++; break;
      case 'L': sp->length_mod = FS_LM_BIG_L; fmt++; break;
      /* MSVC-style "I" size prefix: I64 -> __int64 (==int64_t), I32
       * -> 32-bit (==int), I alone -> ptrdiff_t / size_t.  This is a
       * Microsoft extension some libretro core authors use on
       * older MSVC toolchains that predate full C99 %lld support. */
      case 'I':
         if (fmt[1] == '6' && fmt[2] == '4')
            { sp->length_mod = FS_LM_LL;   fmt += 3; }
         else if (fmt[1] == '3' && fmt[2] == '2')
            { sp->length_mod = FS_LM_NONE; fmt += 3; }
         else
            { sp->length_mod = FS_LM_Z;    fmt += 1; }
         break;
      default: break;
   }
   if (!*fmt)
      return NULL;
   sp->specifier = *fmt++;
   if (sp->specifier == '[')
   {
      if (*fmt == '^')
      {
         sp->set_negate = true;
         fmt++;
      }
      sp->set_start = fmt;
      /* literal ']' as first scanset char is part of the set */
      if (*fmt == ']')
         fmt++;
      while (*fmt && *fmt != ']')
         fmt++;
      if (*fmt != ']')
         return NULL;
      sp->set_end = fmt;
      fmt++;
   }
   return fmt;
}

static void fs_store_int(const fs_scan_spec_t *sp, va_list *args,
      int64_t v, bool is_signed)
{
   if (is_signed)
   {
      switch (sp->length_mod)
      {
         case FS_LM_HH: *va_arg(*args, signed char*) = (signed char)v; break;
         case FS_LM_H:  *va_arg(*args, short*)       = (short)v;       break;
         case FS_LM_L:  *va_arg(*args, long*)        = (long)v;        break;
         case FS_LM_LL: *va_arg(*args, int64_t*)   = (int64_t)v;   break;
         case FS_LM_Z:  *va_arg(*args, size_t*)      = (size_t)v;      break;
         default:       *va_arg(*args, int*)         = (int)v;         break;
      }
   }
   else
   {
      uint64_t uv = (uint64_t)v;
      switch (sp->length_mod)
      {
         case FS_LM_HH: *va_arg(*args, unsigned char*)      = (unsigned char)uv;      break;
         case FS_LM_H:  *va_arg(*args, unsigned short*)     = (unsigned short)uv;     break;
         case FS_LM_L:  *va_arg(*args, unsigned long*)      = (unsigned long)uv;      break;
         case FS_LM_LL: *va_arg(*args, uint64_t*) = (uint64_t)uv; break;
         case FS_LM_Z:  *va_arg(*args, size_t*)             = (size_t)uv;             break;
         default:       *va_arg(*args, unsigned int*)       = (unsigned int)uv;       break;
      }
   }
}

/* Scan one integer conversion. `base` is 0 for %i (auto-detect),
 * else the explicit base (8/10/16). Returns chars consumed, or
 * -1 on parse failure. On success, advances *pp.
 *
 * Width handling: a width of 0 from sp->width means "unlimited" — we
 * normalize that to INT_MAX up front so every subsequent comparison
 * is a single check (no special-casing of "<= 0 means unbounded"
 * which broke when width was legitimately consumed down to 0 by
 * the sign/0x prefix). */
static int fs_scan_int(const char **pp, const fs_scan_spec_t *sp,
      int base, bool is_signed, va_list *args)
{
   const char        *start  = *pp;
   const char        *p      = start;
   const char        *digit_start;
   uint64_t acc    = 0;
   int                neg    = 0;
   int                width  = sp->width > 0 ? sp->width : INT_MAX;
   int                digits = 0;

   /* skip leading whitespace */
   while (isspace((unsigned char)*p))
      p++;

   /* optional sign */
   if ((*p == '+' || *p == '-') && width > 0)
   {
      if (*p == '-')
         neg = 1;
      p++;
      width--;
   }

   /* base autodetect / optional 0x prefix.
    * Note: sscanf's "%x" / "%i" treat "0x" with no following hex
    * digit as a successful match of the value 0, consuming the
    * "0x" (2 chars). We mirror that by counting the leading "0"
    * as our first digit when we commit to base 16 via "0x". */
   if (base == 0)
   {
      if (*p == '0')
      {
         if ((p[1] == 'x' || p[1] == 'X') && width >= 2)
         {
            base    = 16;
            acc     = 0;
            digits  = 1;          /* the "0" counts as a digit */
            p      += 2;
            width  -= 2;
         }
         else
            base = 8;
      }
      else
         base = 10;
   }
   else if (base == 16)
   {
      if (*p == '0' && (p[1] == 'x' || p[1] == 'X') && width >= 2)
      {
         acc     = 0;
         digits  = 1;             /* the "0" counts as a digit */
         p      += 2;
         width  -= 2;
      }
   }

   digit_start = p;
   while (*p && (p - digit_start) < width)
   {
      unsigned char c = (unsigned char)*p;
      int           d;
      if      (c >= '0' && c <= '9') d = c - '0';
      else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
      else                           break;
      if (d >= base)
         break;
      acc = acc * (uint64_t)base + (uint64_t)d;
      p++;
      digits++;
   }

   if (digits == 0)
      return -1;

   if (!sp->suppress)
   {
      int64_t sv = neg
         ? -(int64_t)acc
         :  (int64_t)acc;
      fs_store_int(sp, args, sv, is_signed);
   }
   *pp = p;
   return (int)(p - start);
}

/* %s — skip leading ws, copy non-ws up to width (default unbounded).
 * Always NUL-terminates output. Fails only on no chars matched. */
static int fs_scan_str(const char **pp, const fs_scan_spec_t *sp,
      va_list *args)
{
   const char *start = *pp;
   const char *p     = start;
   char       *out   = NULL;
   int         n     = 0;
   int         width = sp->width;

   while (isspace((unsigned char)*p))
      p++;

   if (!sp->suppress)
      out = va_arg(*args, char*);

   while (*p && !isspace((unsigned char)*p) && (width <= 0 || n < width))
   {
      if (out) out[n] = *p;
      n++;
      p++;
   }
   if (n == 0)
      return -1;
   if (out) out[n] = '\0';
   *pp = p;
   return (int)(p - start);
}

/* %c — read exactly width chars (default 1). Does NOT skip
 * whitespace. Does NOT NUL-terminate. Fails if fewer than width
 * chars available. */
static int fs_scan_char(const char **pp, const fs_scan_spec_t *sp,
      va_list *args)
{
   const char *p     = *pp;
   int         width = sp->width > 0 ? sp->width : 1;
   int         i;
   char       *out   = sp->suppress ? NULL : va_arg(*args, char*);

   for (i = 0; i < width; i++)
   {
      if (!p[i])
         return -1;
      if (out)
         out[i] = p[i];
   }
   *pp = p + width;
   return width;
}

/* Test scanset membership. `set` points just after '[' (or after
 * '[^' if negated); `end` points at the closing ']'. */
static bool fs_scanset_match(const char *set, const char *end,
      bool negate, unsigned char c)
{
   const char *p      = set;
   bool        in_set = false;
   /* literal ']' as first char is part of the set */
   if (p < end && *p == ']')
   {
      if (c == ']') in_set = true;
      p++;
   }
   while (p < end)
   {
      if (p + 2 < end && p[1] == '-' && p[2] != ']')
      {
         unsigned char lo = (unsigned char)p[0];
         unsigned char hi = (unsigned char)p[2];
         if (lo > hi) { unsigned char t = lo; lo = hi; hi = t; }
         if (c >= lo && c <= hi) in_set = true;
         p += 3;
      }
      else
      {
         if ((unsigned char)*p == c) in_set = true;
         p++;
      }
   }
   return negate ? !in_set : in_set;
}

/* %[..] — does NOT skip leading whitespace. */
static int fs_scan_set(const char **pp, const fs_scan_spec_t *sp,
      va_list *args)
{
   const char *start = *pp;
   const char *p     = start;
   char       *out   = sp->suppress ? NULL : va_arg(*args, char*);
   int         n     = 0;
   int         width = sp->width;

   while (*p && (width <= 0 || n < width)
         && fs_scanset_match(sp->set_start, sp->set_end,
               sp->set_negate, (unsigned char)*p))
   {
      if (out) out[n] = *p;
      n++;
      p++;
   }
   if (n == 0)
      return -1;
   if (out) out[n] = '\0';
   *pp = p;
   return n;
}

/* %n — store chars-consumed-so-far into the destination of the
 * appropriate width. Suppressed %n is a no-op. */
static void fs_store_n(const fs_scan_spec_t *sp, va_list *args, int n)
{
   if (sp->suppress)
      return;
   switch (sp->length_mod)
   {
      case FS_LM_HH: *va_arg(*args, signed char*) = (signed char)n; break;
      case FS_LM_H:  *va_arg(*args, short*)       = (short)n;       break;
      case FS_LM_L:  *va_arg(*args, long*)        = (long)n;        break;
      case FS_LM_LL: *va_arg(*args, int64_t*)   = (int64_t)n;   break;
      case FS_LM_Z:  *va_arg(*args, size_t*)      = (size_t)n;      break;
      case FS_LM_J:  *va_arg(*args, intmax_t*)    = (intmax_t)n;    break;
      case FS_LM_T:  *va_arg(*args, ptrdiff_t*)   = (ptrdiff_t)n;   break;
      default:       *va_arg(*args, int*)         = n;              break;
   }
}

/* Decides whether the native engine can handle a given
 * (specifier, length_mod) combination. Combinations that can't be
 * served natively without changing the va_arg destination type
 * (e.g. %ls writes wchar_t*, %jd writes intmax_t*) are routed to
 * fs_fallback which knows the right va_arg dispatch. */
static bool fs_native_handles(char specifier, int length_mod)
{
   switch (specifier)
   {
      case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
         /* j (intmax_t) / t (ptrdiff_t) / non-standard L need fallback. */
         return length_mod == FS_LM_NONE
             || length_mod == FS_LM_HH
             || length_mod == FS_LM_H
             || length_mod == FS_LM_L
             || length_mod == FS_LM_LL
             || length_mod == FS_LM_Z;
      case 's': case 'c': case '[':
         /* l means wchar_t* — fallback. */
         return length_mod == FS_LM_NONE;
      case 'n':
         /* All standard int length mods are handled by fs_store_n. */
         return length_mod == FS_LM_NONE
             || length_mod == FS_LM_HH
             || length_mod == FS_LM_H
             || length_mod == FS_LM_L
             || length_mod == FS_LM_LL
             || length_mod == FS_LM_Z
             || length_mod == FS_LM_J
             || length_mod == FS_LM_T;
      default:
         /* Floats (%f/e/g/a), %p, and anything unknown — fallback. */
         return false;
   }
}

/* Fallback for specifiers and (specifier, length-mod) combinations
 * the native engine does not handle:
 *   - floats (f, e, g, a, F, E, G, A) with any length mod
 *   - %p (pointer)
 *   - integer conversions with the j (intmax_t/uintmax_t),
 *     t (ptrdiff_t/size_t) or L (non-standard) length modifiers
 *   - %ls / %lc / %l[ (wide-char destinations)
 *
 * Behavior matches the prior sscanf+%n approach byte-for-byte. */
static int fs_fallback(const char **pp, const char *fmt_pct,
      const char *fmt_next, const fs_scan_spec_t *sp, va_list *args)
{
   char   subfmt[64];
   size_t fmt_len = (size_t)(fmt_next - fmt_pct);
   int    sublen  = 0;
   int    v       = 0;

   if (fmt_len + 3 > sizeof(subfmt))
      return -1;
   memcpy(subfmt, fmt_pct, fmt_len);
   subfmt[fmt_len]     = '%';
   subfmt[fmt_len + 1] = 'n';
   subfmt[fmt_len + 2] = '\0';

   if (sp->suppress)
   {
      v = sscanf(*pp, subfmt, &sublen);
      if (v == EOF || sublen == 0)
         return -1;
      *pp += sublen;
      return sublen;
   }

   switch (sp->specifier)
   {
      case 'f': case 'e': case 'g': case 'a':
      case 'F': case 'E': case 'G': case 'A':
         switch (sp->length_mod)
         {
            case FS_LM_NONE:
               v = sscanf(*pp, subfmt, va_arg(*args, float*),       &sublen); break;
            case FS_LM_L:
               v = sscanf(*pp, subfmt, va_arg(*args, double*),      &sublen); break;
            case FS_LM_BIG_L:
               v = sscanf(*pp, subfmt, va_arg(*args, long double*), &sublen); break;
            default:
               return -1;
         }
         break;
      case 'p':
         v = sscanf(*pp, subfmt, va_arg(*args, void**), &sublen);
         break;
      /* Signed integer specifiers with length mods we don't handle natively. */
      case 'd': case 'i':
         switch (sp->length_mod)
         {
            case FS_LM_J:
               v = sscanf(*pp, subfmt, va_arg(*args, intmax_t*),  &sublen); break;
            case FS_LM_T:
               v = sscanf(*pp, subfmt, va_arg(*args, ptrdiff_t*), &sublen); break;
            default:
               return -1;
         }
         break;
      /* Unsigned integer specifiers. C99 §7.19.6.2: %t-prefixed
       * unsigned conversions take the unsigned type corresponding
       * to ptrdiff_t, which is size_t for the purposes of va_arg. */
      case 'u': case 'o': case 'x': case 'X':
         switch (sp->length_mod)
         {
            case FS_LM_J:
               v = sscanf(*pp, subfmt, va_arg(*args, uintmax_t*), &sublen); break;
            case FS_LM_T:
               v = sscanf(*pp, subfmt, va_arg(*args, size_t*),    &sublen); break;
            default:
               return -1;
         }
         break;
      /* String-family with wide-char destination. */
      case 's': case 'c': case '[':
         if (sp->length_mod == FS_LM_L)
            v = sscanf(*pp, subfmt, va_arg(*args, wchar_t*), &sublen);
         else
            return -1;
         break;
      default:
         return -1;
   }

   if (v != 1 || sublen == 0)
      return -1;
   *pp += sublen;
   return sublen;
}

int filestream_vscanf(RFILE *stream, const char *format, va_list *args)
{
   char        buf[4096];
   va_list     args_copy;
   const char *bufiter;
   const char *fmt      = format;
   int         ret      = 0;
   int64_t     startpos = filestream_tell(stream);
   int64_t     maxlen   = filestream_read(stream, buf, sizeof(buf) - 1);

   if (maxlen <= 0)
      return EOF;

   buf[maxlen] = '\0';

#ifdef __va_copy
   __va_copy(args_copy, *args);
#else
   va_copy(args_copy, *args);
#endif

   bufiter = buf;

   while (*fmt && *bufiter)
   {
      if (*fmt == '%')
      {
         fs_scan_spec_t  sp;
         const char     *fmt_pct = fmt;  /* points at '%' */
         const char     *fmt_next;
         int             consumed = -1;

         fmt++;  /* past '%' */

         /* literal %% */
         if (*fmt == '%')
         {
            if (*bufiter != '%')
               break;
            bufiter++;
            fmt++;
            continue;
         }

         fmt_next = fs_parse_spec(fmt, &sp);
         if (!fmt_next)
            break;  /* malformed spec — stop, matching prior behavior */

         if (!fs_native_handles(sp.specifier, sp.length_mod))
         {
            consumed = fs_fallback(&bufiter, fmt_pct, fmt_next,
                  &sp, &args_copy);
         }
         else switch (sp.specifier)
         {
            case 'd':
               consumed = fs_scan_int(&bufiter, &sp, 10, true,  &args_copy); break;
            case 'i':
               consumed = fs_scan_int(&bufiter, &sp,  0, true,  &args_copy); break;
            case 'u':
               consumed = fs_scan_int(&bufiter, &sp, 10, false, &args_copy); break;
            case 'o':
               consumed = fs_scan_int(&bufiter, &sp,  8, false, &args_copy); break;
            case 'x':
            case 'X':
               consumed = fs_scan_int(&bufiter, &sp, 16, false, &args_copy); break;
            case 's':
               consumed = fs_scan_str(&bufiter, &sp, &args_copy); break;
            case 'c':
               consumed = fs_scan_char(&bufiter, &sp, &args_copy); break;
            case '[':
               consumed = fs_scan_set(&bufiter, &sp, &args_copy); break;
            case 'n':
               fs_store_n(&sp, &args_copy, (int)(bufiter - buf));
               consumed = 0;  /* succeeds, consumes no input */
               break;
            default:
               /* Unreachable: fs_native_handles returned false for
                * anything not in this switch. */
               consumed = -1;
               break;
         }

         if (consumed < 0)
            break;

         /* %n doesn't count toward the return value; nor do
          * assignment-suppressed conversions. */
         if (!sp.suppress && sp.specifier != 'n')
            ret++;

         /* Suppressed conversion that consumed nothing — bail,
          * matching the prior "sublen == 0" guard. */
         if (sp.suppress && sp.specifier != 'n' && consumed == 0)
            break;

         fmt = fmt_next;
      }
      else if (isspace((unsigned char)*fmt))
      {
         while (isspace((unsigned char)*bufiter))
            bufiter++;
         while (isspace((unsigned char)*fmt))
            fmt++;
      }
      else
      {
         if (*bufiter != *fmt)
            break;
         bufiter++;
         fmt++;
      }
   }

   va_end(args_copy);

   filestream_seek(stream, startpos + (bufiter - buf),
         RETRO_VFS_SEEK_POSITION_START);

   return ret;
}

int filestream_scanf(RFILE *stream, const char* format, ...)
{
   int ret;
   va_list vl;
   va_start(vl, format);
   ret = filestream_vscanf(stream, format, &vl);
   va_end(vl);
   return ret;
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
   int64_t output;

   if (filestream_seek_cb)
      output = filestream_seek_cb(stream->hfile, offset, seek_position);
   else
      output = retro_vfs_file_seek_impl(
            (libretro_vfs_implementation_file*)stream->hfile,
            offset, seek_position);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int filestream_eof(RFILE *stream)
{
   return filestream_tell(stream) == filestream_get_size(stream) ? EOF : 0;
}

int64_t filestream_tell(RFILE *stream)
{
   int64_t output;

   if (filestream_tell_cb)
      output = filestream_tell_cb(stream->hfile);
   else
      output = retro_vfs_file_tell_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

void filestream_rewind(RFILE *stream)
{
   if (!stream)
      return;
   filestream_seek(stream, 0L, RETRO_VFS_SEEK_POSITION_START);
   stream->err_flag = false;
}

int64_t filestream_read(RFILE *stream, void *s, int64_t len)
{
   int64_t output;

   if (filestream_read_cb)
      output = filestream_read_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_read_impl(
            (libretro_vfs_implementation_file*)stream->hfile, s, len);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int filestream_flush(RFILE *stream)
{
   int output;

   if (filestream_flush_cb)
      output = filestream_flush_cb(stream->hfile);
   else
      output = retro_vfs_file_flush_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int filestream_delete(const char *path)
{
   if (filestream_remove_cb)
      return filestream_remove_cb(path);

   return retro_vfs_file_remove_impl(path);
}

int filestream_rename(const char *old_path, const char *new_path)
{
   if (filestream_rename_cb)
      return filestream_rename_cb(old_path, new_path);

   return retro_vfs_file_rename_impl(old_path, new_path);
}

int filestream_copy(const char *src, const char *dst)
{
   char buf[256] = {0};
   int64_t n     = 0;
   int ret       = 0;
   char path_dst[PATH_MAX_LENGTH] = {0};

   RFILE *fp_src = filestream_open(src, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   RFILE *fp_dst = filestream_open(dst, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fp_src || !fp_dst)
      ret = -1;

   if (ret < 0)
      goto close;

   snprintf(path_dst, sizeof(path_dst), "%s", dst);
   path_basedir(path_dst);

   if (!path_is_directory(path_dst))
      path_mkdir(path_dst);

   while ((n = filestream_read(fp_src, buf, sizeof(buf))) > 0 && ret == 0)
   {
      if (filestream_write(fp_dst, buf, n) != n)
         ret = -1;
   }

close:
   if (fp_src)
      filestream_close(fp_src);
   if (fp_dst)
      filestream_close(fp_dst);
   return ret;
}

int filestream_cmp(const char *src, const char *dst)
{
   int ret           = 0;
   RFILE *fp_src     = filestream_open(src, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   RFILE *fp_dst     = filestream_open(dst, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fp_src || !fp_dst || filestream_get_size(fp_src) != filestream_get_size(fp_dst))
      ret = -1;

   if (ret >= 0)
   {
      char buf_src[256] = {0};
      char buf_dst[256] = {0};
      while ((filestream_read(fp_src, buf_src, sizeof(buf_src))) > 0 && ret == 0)
      {
         filestream_read(fp_dst, buf_dst, sizeof(buf_dst));
         ret = memcmp(buf_src, buf_dst, sizeof(buf_src));
      }
   }

   if (fp_src)
   {
      filestream_close(fp_src);
      fp_src = NULL;
   }
   if (fp_dst)
   {
      filestream_close(fp_dst);
      fp_dst = NULL;
   }
   return ret;
}

const char* filestream_get_path(RFILE *stream)
{
   if (filestream_get_path_cb)
      return filestream_get_path_cb(stream->hfile);

   return retro_vfs_file_get_path_impl(
         (libretro_vfs_implementation_file*)stream->hfile);
}

int64_t filestream_write(RFILE *stream, const void *s, int64_t len)
{
   int64_t output;

   if (filestream_write_cb)
      output = filestream_write_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_write_impl(
            (libretro_vfs_implementation_file*)stream->hfile, s, len);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int filestream_putc(RFILE *stream, int c)
{
   char c_char = (char)c;
   if (!stream)
      return EOF;
   return filestream_write(stream, &c_char, 1) == 1
      ? (int)(unsigned char)c
      : EOF;
}

int filestream_vprintf(RFILE *stream, const char* format, va_list args)
{
   static char buffer[8 * 1024];
   int _len = vsnprintf(buffer, sizeof(buffer),
         format, args);
   if (_len < 0)
      return -1;
   else if (_len == 0)
      return 0;
   return (int)filestream_write(stream, buffer, _len);
}

int filestream_printf(RFILE *stream, const char* format, ...)
{
   va_list vl;
   int ret;
   va_start(vl, format);
   ret = filestream_vprintf(stream, format, vl);
   va_end(vl);
   return ret;
}

int filestream_error(RFILE *stream)
{
   return (stream && stream->err_flag);
}

int filestream_close(RFILE *stream)
{
   int output;
   struct retro_vfs_file_handle* fp = stream->hfile;

   if (filestream_close_cb)
      output = filestream_close_cb(fp);
   else
      output = retro_vfs_file_close_impl(
            (libretro_vfs_implementation_file*)fp);

   if (output == 0)
      free(stream);

   return output;
}

int64_t filestream_read_file(const char *path, void **buf, int64_t *len)
{
   int64_t ret              = 0;
   int64_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      *buf = NULL;
      return 0;
   }

   if ((content_buf_size = filestream_get_size(file)) < 0)
      goto error;

   /* Reject sizes that would not survive the cast to size_t for
    * the malloc below.  Pre-patch the only check here was a
    * tautological '(int64_t)(uint64_t)X != X' (which is false
    * for any positive int64_t), and on 32-bit hosts any file
    * larger than ~4 GiB silently truncated through (size_t),
    * the malloc was undersized, and the filestream_read below
    * overran it. */
   if ((uint64_t)content_buf_size + 1 > (uint64_t)((size_t)-1))
      goto error;

   if (!(content_buf = malloc((size_t)(content_buf_size + 1))))
      goto error;

   if ((ret = filestream_read(file, content_buf, (int64_t)content_buf_size)) <
         0)
      goto error;

   if (filestream_close(file) != 0)
      free(file);

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[ret] = '\0';

   if (len)
      *len = ret;

   return 1;

error:
   if (filestream_close(file) != 0)
      free(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

bool filestream_write_file(const char *path, const void *data, int64_t size)
{
   int64_t ret   = 0;
   RFILE *file   = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;
   ret = filestream_write(file, data, size);
   if (filestream_close(file) != 0)
      free(file);
   return (ret == size);
}

char *filestream_getline(RFILE *stream)
{
   char  *newline     = NULL;
   char  *newline_tmp = NULL;
   size_t cur_size    = 256;
   size_t idx         = 0;
   int    in;

   if (!stream)
      return NULL;

   newline = (char*)malloc(cur_size + 1);
   if (!newline)
      return NULL;

   in = filestream_getc(stream);
   if (in == EOF)
   {
      free(newline);
      return NULL;
   }

   while (in != EOF && in != '\n')
   {
      newline[idx++] = (char)in;
      if (idx == cur_size)
      {
         cur_size   *= 2;
         newline_tmp = (char*)realloc(newline, cur_size + 1);
         if (!newline_tmp)
         {
            free(newline);
            return NULL;
         }
         newline = newline_tmp;
      }
      in = filestream_getc(stream);
   }

   /* Shrink to fit if we overallocated significantly */
   if (cur_size > idx + 64)
   {
      newline_tmp = (char*)realloc(newline, idx + 1);
      if (newline_tmp)
         newline = newline_tmp;
   }

   newline[idx] = '\0';
   return newline;
}

libretro_vfs_implementation_file* filestream_get_vfs_handle(RFILE *stream)
{
   return (libretro_vfs_implementation_file*)stream->hfile;
}
