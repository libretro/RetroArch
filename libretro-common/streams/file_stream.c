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
#include <encodings/utf.h>

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

/* Size of the per-handle sequential lookahead buffer filled by
 * filestream_gets()/filestream_getc().  Large enough that the
 * per-call cost of crossing the VFS interface (which for a core
 * running under a frontend-supplied VFS is an indirect call across
 * the libretro ABI) amortizes to nothing, small enough to be an
 * acceptable lazy per-handle heap cost on memory-constrained
 * platforms.  Handles that never touch gets/getc never allocate it.
 * Overridable for platforms that want a different trade-off. */
#ifndef FILESTREAM_RBUF_LEN
#define FILESTREAM_RBUF_LEN 16384
#endif

struct RFILE
{
   struct retro_vfs_file_handle *hfile;
   bool err_flag;
   /* Sequential read lookahead.  Only filestream_rbuf_fill() ever
    * puts bytes here, and only filestream_gets()/filestream_getc()
    * trigger a fill; every other operation on the handle either
    * serves from it (read, tell) or discards it (seek, write,
    * truncate, flush) so the logical stream position stays exact.
    *
    * Invariants while rbuf_len != 0:
    *   - rbuf_pos <= rbuf_len
    *   - the underlying handle sits at rbuf_file_off + rbuf_len
    *   - the logical position is   rbuf_file_off + rbuf_pos
    * rbuf_len == 0 means inactive: the underlying position is the
    * logical position, as before this buffer existed.
    *
    * Note that code holding the raw handle from
    * filestream_get_vfs_handle() bypasses this layer; the in-tree
    * callers of that function (cdrom, disc tasks) never mix it with
    * gets/getc on the same RFILE, and out-of-tree code that does
    * was already bypassing RFILE state (err_flag) anyway. */
   uint8_t *rbuf;
   size_t rbuf_cap;
   size_t rbuf_len;
   size_t rbuf_pos;
   int64_t rbuf_file_off;
   /* C89 7.9.5.3: on an update stream, output followed by input (or
    * input followed by output, short of EOF) requires an intervening
    * file positioning or flush call.  Backends over stdio really do
    * misbehave without it - reads after writes return stale bytes.
    * Track the last data direction and interpose a no-op seek on
    * transitions, at this layer so every VFS backend is covered. */
   signed char last_io;
};

#define FILESTREAM_LAST_IO_NONE  0
#define FILESTREAM_LAST_IO_READ  1
#define FILESTREAM_LAST_IO_WRITE 2

static int64_t filestream_raw_read(RFILE *stream, void *s, int64_t len);
static int64_t filestream_raw_seek(RFILE *stream,
      int64_t offset, int seek_position);
static int64_t filestream_raw_tell(RFILE *stream);
static void filestream_rbuf_discard(RFILE *stream);
static int filestream_rbuf_fill(RFILE *stream);

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

   /* The lookahead may hold bytes past the new length; drop it and
    * restore the underlying position first. */
   if (stream)
      filestream_rbuf_discard(stream);

   if (filestream_truncate_cb)
      output = filestream_truncate_cb(stream->hfile, length);
   else
      output = retro_vfs_file_truncate_impl(
            (libretro_vfs_implementation_file*)stream->hfile, length);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

/* Drop the lookahead, rewinding the underlying handle over any bytes
 * read ahead but not yet consumed, so the underlying position becomes
 * the logical position again.  Must run before anything that hands the
 * underlying position to the VFS directly (write, truncate, flush).
 *
 * If the rewind itself fails the handle is one that cannot seek, in
 * which case interleaving buffered reads with writes was never going
 * to be coherent on it; err_flag is set by the failed raw seek. */
static void filestream_rbuf_discard(RFILE *stream)
{
   if (stream->rbuf_len != 0)
   {
      size_t unconsumed = stream->rbuf_len - stream->rbuf_pos;
      if (unconsumed != 0)
         filestream_raw_seek(stream, -(int64_t)unconsumed,
               RETRO_VFS_SEEK_POSITION_CURRENT);
      stream->rbuf_pos = 0;
      stream->rbuf_len = 0;
   }
}

/* Refill the lookahead from the current underlying position.
 * Returns 1 with at least one byte buffered, 0 at end of file or on a
 * read error (the raw read has set err_flag for the latter, matching
 * what the old one-byte filestream_read() did), or -1 when no buffer
 * could be allocated or the handle cannot report a position - in both
 * of those cases the caller falls back to the unbuffered byte path,
 * which behaves exactly as this function did before the lookahead
 * existed. */
static int filestream_rbuf_fill(RFILE *stream)
{
   int64_t off;
   int64_t got;

   if (!stream->rbuf)
   {
      if (!(stream->rbuf = (uint8_t*)malloc(FILESTREAM_RBUF_LEN)))
         return -1;
      stream->rbuf_cap = FILESTREAM_RBUF_LEN;
   }

   if ((off = filestream_raw_tell(stream)) < 0)
      return -1;

   if ((got = filestream_raw_read(stream, stream->rbuf,
               (int64_t)stream->rbuf_cap)) <= 0)
      return 0;

   stream->rbuf_file_off = off;
   stream->rbuf_pos      = 0;
   stream->rbuf_len      = (size_t)got;
   return 1;
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

   output->err_flag      = false;
   output->hfile         = fp;
   output->rbuf          = NULL;
   output->rbuf_cap      = 0;
   output->rbuf_len      = 0;
   output->rbuf_pos      = 0;
   output->rbuf_file_off = 0;
   output->last_io       = FILESTREAM_LAST_IO_NONE;
   return output;
}

char* filestream_gets(RFILE *stream, char *s, size_t len)
{
   size_t done    = 0;
   size_t want;
   bool   hit_end = false;

   /* len == 0 leaves no room even for the terminator; the previous
    * implementation decremented the size_t straight past zero and
    * wrote unbounded output.  fgets() with n <= 0 returns NULL on
    * every libc worth matching, so do that. */
   if (!stream || len == 0)
      return NULL;

   /* get max bytes or up to a newline */
   want = len - 1;

   /* Zero-copy path: when the handle carries a file mapping
    * (RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS, or a frontend
    * mapped-ptr callback), find the line bound with memchr() over the
    * mapping and copy exactly the bytes returned.  Only taken with an
    * empty lookahead so the two never both hold stream position. */
   if (want != 0 && stream->rbuf_pos == stream->rbuf_len)
   {
      int64_t map_len    = 0;
      const uint8_t *map = filestream_get_mapped_ptr(stream, &map_len);
      if (map)
      {
         int64_t pos = filestream_tell(stream);
         if (pos >= 0)
         {
            size_t n = 0;
            if (pos < map_len)
            {
               const uint8_t *nl;
               uint64_t avail = (uint64_t)(map_len - pos);
               n = want;
               if ((uint64_t)n > avail)
                  n = (size_t)avail;
               if ((nl = (const uint8_t*)memchr(map + pos, '\n', n)))
                  n = (size_t)(nl - (map + pos)) + 1;
               memcpy(s, map + pos, n);
               if (n != 0)
                  filestream_seek(stream, pos + (int64_t)n,
                        RETRO_VFS_SEEK_POSITION_START);
            }
            s[n] = '\0';
            if (n == 0)
               return NULL;
            return s;
         }
         /* Position unknown; the buffered path below reads
          * sequentially and does not need it. */
      }
   }

   while (done < want)
   {
      const uint8_t *span;
      const uint8_t *nl;
      size_t take;
      bool newline = false;

      if (stream->rbuf_pos == stream->rbuf_len)
      {
         int filled = filestream_rbuf_fill(stream);
         if (filled == 0)
         {
            hit_end = true;
            break;
         }
         if (filled < 0)
         {
            /* No lookahead available on this handle; degrade to the
             * historical byte loop for the remainder. */
            int c = 0;
            while (done < want)
            {
               if ((c = filestream_getc(stream)) == EOF)
               {
                  hit_end = true;
                  break;
               }
               s[done++] = (char)c;
               if (c == '\n')
                  break;
            }
            break;
         }
      }

      span = stream->rbuf + stream->rbuf_pos;
      take = stream->rbuf_len - stream->rbuf_pos;
      if (take > want - done)
         take = want - done;
      if ((nl = (const uint8_t*)memchr(span, '\n', take)))
      {
         take    = (size_t)(nl - span) + 1;
         newline = true;
      }
      memcpy(s + done, span, take);
      done             += take;
      stream->rbuf_pos += take;
      if (stream->rbuf_pos == stream->rbuf_len)
      {
         stream->rbuf_pos = 0;
         stream->rbuf_len = 0;
      }
      if (newline)
         break;
   }

   s[done] = '\0';

   if (done == 0 && hit_end)
      return NULL;
   return (s);
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   int filled;

   if (!stream)
      return EOF;

   if (stream->rbuf_pos < stream->rbuf_len)
   {
      int out = (int)stream->rbuf[stream->rbuf_pos++];
      if (stream->rbuf_pos == stream->rbuf_len)
      {
         stream->rbuf_pos = 0;
         stream->rbuf_len = 0;
      }
      return out;
   }

   if ((filled = filestream_rbuf_fill(stream)) > 0)
   {
      int out = (int)stream->rbuf[stream->rbuf_pos++];
      if (stream->rbuf_pos == stream->rbuf_len)
      {
         stream->rbuf_pos = 0;
         stream->rbuf_len = 0;
      }
      return out;
   }
   if (filled == 0)
      return EOF;

   /* No lookahead on this handle; historical one-byte read. */
   if (filestream_raw_read(stream, &c, 1) == 1)
      return (int)(unsigned char)c;
   return EOF;
}

/* ---- internal scanf engine ----------------------------------------
 *
 * The previous implementation walked the format string and for each
 * %-spec built a per-conversion sub-format string (e.g. "%d%n") and
 * invoked sscanf() for every specifier. sscanf on most libcs runs
 * strlen() over the entire remaining input on every call to compute
 * the internal FILE-stream length, and glibc additionally allocates
 * transient state per call. For input parsed one conversion at a
 * time this is the dominant cost.
 *
 * The native engine below scans every supported conversion in place
 * with no sscanf call anywhere in the path:
 *
 *   %d %i %u %o %x %X      - via fs_scan_int (custom digit loop)
 *   %f %e %g %a (+caps)    - via fs_scan_float (uses strtod, which
 *                            unlike sscanf does not strlen the buffer
 *                            or allocate per-call state)
 *   %p                     - via fs_scan_pointer (hex parse)
 *   %s %c %[..]            - via fs_scan_str/char/set (byte loop)
 *   %ls %lc %l[..]         - via fs_scan_wide (UTF-8 decoding via
 *                            libretro-common's utf8_walk; no
 *                            mbrtowc dependency)
 *   %n                     - direct write of chars-consumed-so-far
 *   %%                     - literal match
 *
 * with the full grammar: '*' assignment suppression, decimal width,
 * and length modifiers hh / h / l / ll / z / j / t / L (plus the
 * MSVC-only I / I32 / I64 size prefixes that some legacy core code
 * still uses). */

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
         case FS_LM_LL: *va_arg(*args, int64_t*)     = (int64_t)v;     break;
         case FS_LM_Z:  *va_arg(*args, size_t*)      = (size_t)v;      break;
         case FS_LM_J:  *va_arg(*args, intmax_t*)    = (intmax_t)v;    break;
         case FS_LM_T:  *va_arg(*args, ptrdiff_t*)   = (ptrdiff_t)v;   break;
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
         case FS_LM_LL: *va_arg(*args, uint64_t*)           = (uint64_t)uv;           break;
         case FS_LM_Z:  *va_arg(*args, size_t*)             = (size_t)uv;             break;
         case FS_LM_J:  *va_arg(*args, uintmax_t*)          = (uintmax_t)uv;          break;
         case FS_LM_T:  *va_arg(*args, size_t*)             = (size_t)uv;             break;
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

/* Decides whether the engine can serve a (specifier, length_mod)
 * combination at all. All standard combinations are now handled
 * natively (no sscanf calls remain). Returns false only for an
 * unrecognized specifier or an out-of-spec length modifier
 * (e.g. %hf, %jc), in which case the dispatch breaks the parse
 * loop the same way the prior engine did on malformed input. */
static bool fs_native_handles(char specifier, int length_mod)
{
   switch (specifier)
   {
      case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
         /* Integers: every standard length modifier is supported.
          * The MSVC "I" prefix maps to LL/NONE/Z in fs_parse_spec. */
         return length_mod == FS_LM_NONE
             || length_mod == FS_LM_HH
             || length_mod == FS_LM_H
             || length_mod == FS_LM_L
             || length_mod == FS_LM_LL
             || length_mod == FS_LM_Z
             || length_mod == FS_LM_J
             || length_mod == FS_LM_T;
      case 'f': case 'e': case 'g': case 'a':
      case 'F': case 'E': case 'G': case 'A':
         /* Floats: NONE -> float, L -> double, BIG_L -> long double. */
         return length_mod == FS_LM_NONE
             || length_mod == FS_LM_L
             || length_mod == FS_LM_BIG_L;
      case 's': case 'c': case '[':
         /* Strings/chars/scansets: NONE writes char, L writes wchar_t. */
         return length_mod == FS_LM_NONE
             || length_mod == FS_LM_L;
      case 'p':
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
         return false;
   }
}

/* Scan a floating-point conversion.  Handles %f / %e / %g / %a and
 * their capitalized siblings.  Uses strtod() — which is C89 and,
 * unlike sscanf, does not run strlen() across the rest of the
 * buffer or allocate any per-call FILE-stream state.  Width is
 * honored by copying at most `width` non-whitespace bytes into a
 * stack scratch buffer, NUL-terminating, and letting strtod parse
 * that.  For %Lf, strtod's double result is cast to long double:
 * on platforms where long double is wider than double (notably
 * x86 with 80-bit extended), this loses precision relative to a
 * hypothetical strtold call — but strtold is C99 and not available
 * in strict MSVC C89, and %Lf via filestream_scanf is essentially
 * unused, so we accept the trade. */
static int fs_scan_float(const char **pp, const fs_scan_spec_t *sp,
      va_list *args)
{
   const char  *start = *pp;
   const char  *p     = start;
   char        *endp;
   double       val;
   ptrdiff_t    advance;

   while (isspace((unsigned char)*p))
      p++;
   if (!*p)
      return -1;

   if (sp->width > 0)
   {
      char  tmp[64];
      int   w     = sp->width;
      int   i;
      if (w >= (int)sizeof(tmp))
         w = (int)sizeof(tmp) - 1;
      for (i = 0; i < w && p[i] && !isspace((unsigned char)p[i]); i++)
         tmp[i] = p[i];
      tmp[i] = '\0';
      val = strtod(tmp, &endp);
      if (endp == tmp)
         return -1;
      advance = endp - tmp;
   }
   else
   {
      val = strtod(p, &endp);
      if (endp == p)
         return -1;
      advance = endp - p;
   }
   p += advance;

   if (!sp->suppress)
   {
      switch (sp->length_mod)
      {
         case FS_LM_NONE:  *va_arg(*args, float*)       = (float)val;       break;
         case FS_LM_L:     *va_arg(*args, double*)      = val;              break;
         case FS_LM_BIG_L: *va_arg(*args, long double*) = (long double)val; break;
         default: return -1;
      }
   }
   *pp = p;
   return (int)(p - start);
}

/* %p — implementation-defined per C99, but the de-facto format
 * everywhere (glibc, musl, BSD, MSVC's CRT for the common case)
 * is hex digits with an optional "0x"/"0X" prefix, optionally
 * preceded by whitespace.  Parse that, store as void*. */
static int fs_scan_pointer(const char **pp, const fs_scan_spec_t *sp,
      va_list *args)
{
   const char *start = *pp;
   const char *p     = start;
   const char *digit_start;
   uint64_t    acc    = 0;
   int         width  = sp->width > 0 ? sp->width : INT_MAX;
   int         digits = 0;

   while (isspace((unsigned char)*p))
      p++;

   if (*p == '0' && (p[1] == 'x' || p[1] == 'X') && width >= 2)
   {
      p     += 2;
      width -= 2;
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
      acc = (acc << 4) | (uint64_t)d;
      p++;
      digits++;
   }
   if (digits == 0)
      return -1;
   if (!sp->suppress)
      *va_arg(*args, void**) = (void*)(uintptr_t)acc;
   *pp = p;
   return (int)(p - start);
}

/* Wide-character scanset membership.  Set elements in the format
 * string are encoded as bytes; we promote each to wchar_t by zero
 * extension via unsigned char.  This is exact for ASCII set chars
 * (the typical case) and is the same convention glibc uses for
 * wide scansets with ASCII format strings. */
static bool fs_wscanset_match(const char *set, const char *end,
      bool negate, wchar_t c)
{
   const char *p      = set;
   bool        in_set = false;
   if (p < end && *p == ']')
   {
      if (c == (wchar_t)']') in_set = true;
      p++;
   }
   while (p < end)
   {
      if (p + 2 < end && p[1] == '-' && p[2] != ']')
      {
         wchar_t lo = (wchar_t)(unsigned char)p[0];
         wchar_t hi = (wchar_t)(unsigned char)p[2];
         if (lo > hi) { wchar_t t = lo; lo = hi; hi = t; }
         if (c >= lo && c <= hi) in_set = true;
         p += 3;
      }
      else
      {
         if ((wchar_t)(unsigned char)*p == c) in_set = true;
         p++;
      }
   }
   return negate ? !in_set : in_set;
}

/* Wide-char scanf primitive.  `mode` selects the variant:
 *   's' — skip ws, read non-ws wide chars until ws/width/end; NUL-terminate
 *   'c' — read exactly width wide chars (default 1); no ws skip; no NUL
 *   '[' — read wide chars while scanset matches; NUL-terminate
 *
 * Multibyte input is decoded as UTF-8 via libretro-common's stateless
 * utf8_walk(); this avoids any dependency on the host libc's
 * mbrtowc/mbstate_t (absent in DJGPP and other older toolchains) and
 * gives the same behavior across platforms. The lead byte's UTF-8
 * width is checked against the buffer's NUL boundary before walking
 * so truncated input never reads past the terminator. */
static int fs_scan_wide(const char **pp, const fs_scan_spec_t *sp,
      char mode, va_list *args)
{
   const char *start = *pp;
   const char *p     = start;
   wchar_t    *out;
   int         n     = 0;
   int         width = sp->width;
   int         max_n;

   if (mode == 's')
   {
      while (isspace((unsigned char)*p))
         p++;
      max_n = (width > 0) ? width : INT_MAX;
   }
   else if (mode == 'c')
      max_n = (width > 0) ? width : 1;
   else /* '[' */
      max_n = (width > 0) ? width : INT_MAX;

   out = sp->suppress ? NULL : va_arg(*args, wchar_t*);

   while (n < max_n)
   {
      const char   *before;
      unsigned char lead;
      int           needed;
      int           k;
      uint32_t      cp;

      if (!*p)
         break;
      if (mode == 's' && isspace((unsigned char)*p))
         break;

      /* Bounds-check the UTF-8 sequence width against the NUL
       * boundary before letting utf8_walk advance past it. */
      lead = (unsigned char)*p;
      if      (lead < 0x80) needed = 1;
      else if (lead < 0xC0) return -1;  /* stray continuation byte */
      else if (lead < 0xE0) needed = 2;
      else if (lead < 0xF0) needed = 3;
      else                  needed = 4;
      for (k = 1; k < needed; k++)
         if (!p[k])
            return -1;

      before = p;
      cp     = utf8_walk(&p);

      if (mode == '[' && !fs_wscanset_match(sp->set_start, sp->set_end,
               sp->set_negate, (wchar_t)cp))
      {
         p = before;  /* codepoint rejected — do not consume */
         break;
      }
      if (out)
         out[n] = (wchar_t)cp;
      n++;
   }

   /* %lc must read exactly max_n wide chars; falling short is failure. */
   if (mode == 'c' && n < max_n)
      return -1;
   /* %ls / %l[ must match at least one wide char. */
   if (mode != 'c' && n == 0)
      return -1;
   /* %ls and %l[ NUL-terminate; %lc does not. */
   if (mode != 'c' && out)
      out[n] = 0;

   *pp = p;
   return (int)(p - start);
}

int filestream_vscanf(RFILE *stream, const char *format, va_list *args)
{
   /* The scan window is heap rather than a local: at char buf[4096]
    * this was a 4368-byte frame, over half of the 8 KiB a GEKKO
    * thread gets (STACKSIZE in rthreads/gx_pthread.h).  Shrinking it
    * instead would have been the cheaper change and the wrong one -
    * the window is how far a single conversion may reach, so a
    * smaller one fails differently on long input rather than merely
    * more slowly.  One allocation per call is nothing beside the read
    * this function already performs. */
   char       *buf       = (char*)malloc(FILESTREAM_SCANF_WINDOW);
   va_list     args_copy;
   const char *bufiter;
   const char *fmt      = format;
   int         ret      = 0;
   int64_t     startpos;
   int64_t     maxlen;

   if (!buf)
      return EOF;

   startpos = filestream_tell(stream);
   maxlen   = filestream_read(stream, buf, FILESTREAM_SCANF_WINDOW - 1);

   if (maxlen <= 0)
   {
      free(buf);
      return EOF;
   }

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
            break;  /* unsupported combination — stop, matching prior behavior */

         switch (sp.specifier)
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
            case 'f': case 'e': case 'g': case 'a':
            case 'F': case 'E': case 'G': case 'A':
               consumed = fs_scan_float(&bufiter, &sp, &args_copy); break;
            case 'p':
               consumed = fs_scan_pointer(&bufiter, &sp, &args_copy); break;
            case 's':
               if (sp.length_mod == FS_LM_L)
                  consumed = fs_scan_wide(&bufiter, &sp, 's', &args_copy);
               else
                  consumed = fs_scan_str(&bufiter, &sp, &args_copy);
               break;
            case 'c':
               if (sp.length_mod == FS_LM_L)
                  consumed = fs_scan_wide(&bufiter, &sp, 'c', &args_copy);
               else
                  consumed = fs_scan_char(&bufiter, &sp, &args_copy);
               break;
            case '[':
               if (sp.length_mod == FS_LM_L)
                  consumed = fs_scan_wide(&bufiter, &sp, '[', &args_copy);
               else
                  consumed = fs_scan_set(&bufiter, &sp, &args_copy);
               break;
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

   /* Seek before the free: the new position is derived from how far
    * bufiter walked into buf. */
   filestream_seek(stream, startpos + (bufiter - buf),
         RETRO_VFS_SEEK_POSITION_START);

   free(buf);
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

static int64_t filestream_raw_seek(RFILE *stream,
      int64_t offset, int seek_position)
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
   else
      /* A successful positioning call satisfies the update-stream
       * transition rule in both directions. */
      stream->last_io = FILESTREAM_LAST_IO_NONE;

   return output;
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
   if (stream && stream->rbuf_len != 0)
   {
      int64_t output;
      int64_t adjusted = offset;

      /* The underlying handle sits past the logical position by the
       * unconsumed lookahead; a relative seek must be taken from the
       * logical position. */
      if (seek_position == RETRO_VFS_SEEK_POSITION_CURRENT)
         adjusted -= (int64_t)(stream->rbuf_len - stream->rbuf_pos);

      output = filestream_raw_seek(stream, adjusted, seek_position);

      /* Every successful seek drops the lookahead rather than trying
       * to serve targets inside it: matching stdio, where fseek()
       * discards the read buffer, keeps rewind-and-reread of
       * synthetic files (procfs/sysfs pollers) returning fresh
       * bytes instead of cached ones.  On failure the underlying
       * position is unchanged, so the lookahead is still valid and
       * the logical position observably does not move. */
      if (output != VFS_ERROR_RETURN_VALUE)
      {
         stream->rbuf_pos = 0;
         stream->rbuf_len = 0;
      }
      return output;
   }
   return filestream_raw_seek(stream, offset, seek_position);
}

int filestream_eof(RFILE *stream)
{
   return filestream_tell(stream) == filestream_get_size(stream) ? EOF : 0;
}

static int64_t filestream_raw_tell(RFILE *stream)
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

int64_t filestream_tell(RFILE *stream)
{
   /* While the lookahead is active the logical position is tracked
    * here, a fill-time snapshot plus consumed bytes; no VFS call. */
   if (stream && stream->rbuf_len != 0)
      return stream->rbuf_file_off + (int64_t)stream->rbuf_pos;
   return filestream_raw_tell(stream);
}

void filestream_rewind(RFILE *stream)
{
   if (!stream)
      return;
   filestream_seek(stream, 0L, RETRO_VFS_SEEK_POSITION_START);
   stream->err_flag = false;
}

static int64_t filestream_raw_read(RFILE *stream, void *s, int64_t len)
{
   int64_t output;

   /* Update-stream rule: input directly after output needs a
    * positioning call between them, or stdio-backed VFS paths hand
    * back stale bytes.  A zero-byte relative seek is the cheapest
    * legal one. */
   if (stream->last_io == FILESTREAM_LAST_IO_WRITE)
      filestream_raw_seek(stream, 0, RETRO_VFS_SEEK_POSITION_CURRENT);
   stream->last_io = FILESTREAM_LAST_IO_READ;

   if (filestream_read_cb)
      output = filestream_read_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_read_impl(
            (libretro_vfs_implementation_file*)stream->hfile, s, len);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;

   return output;
}

int64_t filestream_read(RFILE *stream, void *s, int64_t len)
{
   if (stream && stream->rbuf_pos < stream->rbuf_len && len > 0)
   {
      int64_t rest;
      size_t drained = stream->rbuf_len - stream->rbuf_pos;

      if ((uint64_t)len < (uint64_t)drained)
         drained = (size_t)len;
      memcpy(s, stream->rbuf + stream->rbuf_pos, drained);
      stream->rbuf_pos += drained;
      if (stream->rbuf_pos == stream->rbuf_len)
      {
         stream->rbuf_pos = 0;
         stream->rbuf_len = 0;
      }
      if ((int64_t)drained == len)
         return len;

      /* err_flag is set by the raw read on failure; having already
       * moved bytes to the caller, report the short count the way a
       * partially satisfied stdio read would. */
      if ((rest = filestream_raw_read(stream,
                  (uint8_t*)s + drained, len - (int64_t)drained)) < 0)
         return (int64_t)drained;
      return (int64_t)drained + rest;
   }
   return filestream_raw_read(stream, s, len);
}

int filestream_flush(RFILE *stream)
{
   int output;

   /* Match stdio fflush() on an update stream: synchronize the
    * underlying position with the logical one. */
   if (stream)
      filestream_rbuf_discard(stream);

   if (filestream_flush_cb)
      output = filestream_flush_cb(stream->hfile);
   else
      output = retro_vfs_file_flush_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->err_flag = true;
   else if (stream)
      /* A flush also satisfies the update-stream transition rule. */
      stream->last_io = FILESTREAM_LAST_IO_NONE;

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

bool filestream_matches_buf(const char *path, const void *data, size_t len)
{
   const uint8_t *mem     = (const uint8_t*)data;
   const uint8_t *map     = NULL;
   int64_t        map_len = 0;
   bool           match   = false;
   RFILE         *file;

   if (!path || !*path || (len && !data))
      return false;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)))
      return false;

   /* The cheap answer, before a byte moves.  Callers that slurped the
    * file to memcmp it were paying a full-size allocation and a
    * full-file read to learn this, and learning it last. */
   if (filestream_get_size(file) != (int64_t)len)
      goto done;

   if ((map = filestream_get_mapped_ptr(file, &map_len)))
   {
      /* A view that does not span the file would compare the wrong
       * bytes; fall through to the copying path rather than guess. */
      if (map_len == (int64_t)len)
      {
         match = (len == 0) || (memcmp(map, mem, len) == 0);
         goto done;
      }
      if (filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_START) != 0)
         goto done;
   }

   {
      /* FILESTREAM_MATCHES_BUF_WINDOW, which is sized by the smallest
       * thread stack in the tree rather than by throughput.  GEKKO
       * threads get 8 KiB (STACKSIZE in rthreads/gx_pthread.h), 3DS
       * 32 KiB (ctr_pthread.h) and Vita 64 KiB - and this is
       * libretro-common API, so a caller on a spawned thread is not
       * hypothetical.  (psp_pthread.h declares 8 KiB too, but nothing
       * includes it: rthreads.c reaches for gx_pthread.h under GEKKO
       * and ctr_pthread.h under _3DS, and PSP falls through to plain
       * pthreads.  The 8 KiB floor is GEKKO's.)
       *
       * Bigger reads are faster, and at or above the VFS's own 64 KiB
       * stdio buffer they skip it entirely: measured on the unchanged
       * case, which reads every byte, 1 KiB against 4 KiB against
       * 64 KiB is 1753 / 1432 / 1153 us on an 8 MiB file.  The
       * platforms paying that are the ones with no memory mapping -
       * the mapped branch above copies nothing - which are the same
       * ones with the 8 KiB stacks, and this runs when a save is
       * written rather than in a frame. */
      uint8_t window[FILESTREAM_MATCHES_BUF_WINDOW];
      size_t  off = 0;

      match = true;
      while (off < len)
      {
         int64_t got;
         size_t  want = len - off;

         if (want > sizeof(window))
            want = sizeof(window);

         if ((got = filestream_read(file, window, (int64_t)want))
               != (int64_t)want)
         {
            match = false;
            break;
         }
         /* Stop at the first difference: the caller asked a yes/no
          * question and a 'no' is final. */
         if (memcmp(window, mem + off, (size_t)got) != 0)
         {
            match = false;
            break;
         }
         off += (size_t)got;
      }
   }

done:
   if (filestream_close(file) != 0)
      free(file);
   return match;
}

/* Optional extension: a VFS provider that CAN expose a mapping for
 * some of its handles (a local-first hybrid, say) registers this and
 * answers per handle; NULL from the callback means unmapped, same as
 * everywhere else. Without a registration, an installed VFS keeps the
 * conservative answer below. */
static filestream_mapped_ptr_cb_t filestream_mapped_ptr_cb;

void filestream_set_mapped_ptr_cb(filestream_mapped_ptr_cb_t cb)
{
   filestream_mapped_ptr_cb = cb;
}

const uint8_t *filestream_get_mapped_ptr(RFILE *stream, int64_t *len)
{
   if (len)
      *len = 0;
   if (!stream)
      return NULL;
   if (filestream_read_cb)
   {
      if (filestream_mapped_ptr_cb)
         return filestream_mapped_ptr_cb(stream->hfile, len);
      /* A frontend- or core-supplied VFS is driven entirely through
       * the callbacks and has no mapping this side of them. */
      return NULL;
   }
   return retro_vfs_file_get_mapped_ptr_impl(
         (libretro_vfs_implementation_file*)stream->hfile, len);
}

int64_t filestream_write(RFILE *stream, const void *s, int64_t len)
{
   int64_t output;

   /* Reads may have run ahead of the logical position; put the
    * underlying handle back on it before writing through it.  The
    * discard's rewind doubles as the read-to-write transition seek;
    * when there is nothing to discard, interpose one (update-stream
    * rule, mirror of the one in filestream_raw_read). */
   if (stream)
   {
      filestream_rbuf_discard(stream);
      if (stream->last_io == FILESTREAM_LAST_IO_READ)
         filestream_raw_seek(stream, 0, RETRO_VFS_SEEK_POSITION_CURRENT);
      stream->last_io = FILESTREAM_LAST_IO_WRITE;
   }

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
   {
      free(stream->rbuf);
      free(stream);
   }

   return output;
}

/* Read an already-open stream to EOF, growing the buffer as it goes.
 *
 * For the path below that sizes its allocation from
 * filestream_get_size(), a file reporting zero bytes yields a zero-
 * byte read - so procfs, whose entries all stat as size 0, came back
 * as an empty string *and a success return*.  Callers cannot tell
 * that from a genuinely empty file, and did not: the /proc/apm
 * battery probe, the /proc/modules sg check in cdrom.c and the
 * dingux /proc/jz/battery reader all silently saw nothing.  (Which
 * is also why mem_stats.c reads /proc/meminfo through a raw open()
 * and read() rather than this helper.)
 *
 * A stat size is a hint, not a contract - synthetic filesystems have
 * no length until they are read, and a growing file's is stale the
 * moment it is taken - so when there is no usable hint, read until
 * the stream says stop.  Returns bytes read, or -1.
 */
static int64_t filestream_read_file_to_eof(RFILE *file, void **buf)
{
   /* One page holds essentially every procfs entry this is used for,
    * so the doubling below rarely runs at all. */
   size_t   cap  = 4096;
   size_t   used = 0;
   char    *data = (char*)malloc(cap);

   if (!data)
      return -1;

   for (;;)
   {
      int64_t got;

      /* Keep a spare byte for the NUL, and grow before reading so a
       * read is never issued with zero space. */
      if (used + 1 >= cap)
      {
         char  *tmp;
         size_t want = cap * 2;

         /* Refuse a doubling that would wrap, rather than allocating
          * a smaller buffer than the offsets below assume. */
         if (want < cap)
         {
            free(data);
            return -1;
         }
         if (!(tmp = (char*)realloc(data, want)))
         {
            free(data);
            return -1;
         }
         data = tmp;
         cap  = want;
      }

      if ((got = filestream_read(file, data + used,
                  (int64_t)(cap - used - 1))) < 0)
      {
         free(data);
         return -1;
      }
      if (got == 0)
         break;

      used += (size_t)got;
   }

   data[used] = '\0';
   *buf       = data;
   return (int64_t)used;
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

   /* No usable size hint: read to EOF instead of trusting the zero.
    * See filestream_read_file_to_eof() above. */
   if (content_buf_size == 0)
   {
      if ((ret = filestream_read_file_to_eof(file, &content_buf)) < 0)
         goto error;

      if (filestream_close(file) != 0)
         free(file);

      *buf = content_buf;
      if (len)
         *len = ret;
      return 1;
   }

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
