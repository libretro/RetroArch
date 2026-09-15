/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_strtoll.c).
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

/* strtoll/strtoull are C99.  The MSVC CRT grew _strtoi64/_strtoui64 in
 * Visual Studio 2005; compat/msvc.h maps onto those from _MSC_VER 1400
 * on.  Visual C++ 6.0, 2002 and 2003 have neither, and _atoi64 is not a
 * substitute - it takes no base and no end pointer, so a caller that
 * relies on either silently misbehaves rather than failing to build.
 *
 * THIS FILE HAS NOT BEEN VALIDATED ON PLATFORMS BESIDES MSVC */
#if defined(_MSC_VER) && _MSC_VER < 1400

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#ifndef _I64_MAX
#define _I64_MAX 9223372036854775807i64
#endif

#ifndef _I64_MIN
#define _I64_MIN (-9223372036854775807i64 - 1)
#endif

#ifndef _UI64_MAX
#define _UI64_MAX 0xffffffffffffffffui64
#endif

/* Accumulates into 64 bits unsigned regardless of signedness, the way
 * the BSD and glibc implementations do, and hands the sign and the
 * overflow flag back to the caller to apply the correct clamp.  Both
 * wrappers then behave exactly as C99 specifies, so a call site can be
 * written once and mean the same thing on every compiler. */
static unsigned __int64 strtox_retro__(const char *nptr, char **endptr,
      int base, int *is_neg, int *is_ovf)
{
   const char       *p = nptr;
   unsigned __int64 acc = 0;
   unsigned __int64 cutoff;
   unsigned __int64 ubase;
   int cutlim;
   int any             = 0;

   *is_neg             = 0;
   *is_ovf             = 0;

   if (!p || (base != 0 && (base < 2 || base > 36)))
   {
      if (endptr)
         *endptr = (char*)nptr;
      return 0;
   }

   while (isspace((unsigned char)*p))
      p++;

   if (*p == '-')
   {
      *is_neg = 1;
      p++;
   }
   else if (*p == '+')
      p++;

   /* A "0x" that is not followed by a hex digit is not a prefix: the
    * conversion takes the leading zero and stops at the 'x'. */
   if (     (base == 0 || base == 16)
         && p[0] == '0'
         && (p[1] == 'x' || p[1] == 'X')
         && isxdigit((unsigned char)p[2]))
   {
      p   += 2;
      base = 16;
   }
   else if (base == 0)
      base = (*p == '0') ? 8 : 10;

   ubase  = (unsigned __int64)base;
   cutoff = _UI64_MAX / ubase;
   cutlim = (int)(_UI64_MAX % ubase);

   for (;;)
   {
      int d;
      unsigned char c = (unsigned char)*p;

      if (c >= '0' && c <= '9')
         d = (int)(c - '0');
      else if (c >= 'a' && c <= 'z')
         d = (int)(c - 'a') + 10;
      else if (c >= 'A' && c <= 'Z')
         d = (int)(c - 'A') + 10;
      else
         break;

      if (d >= base)
         break;

      if (acc > cutoff || (acc == cutoff && d > cutlim))
         *is_ovf = 1;
      else
         acc = acc * ubase + (unsigned __int64)d;

      any = 1;
      p++;
   }

   /* No digits consumed: the subject sequence is empty, so the end
    * pointer goes back to the start of the string, sign and all. */
   if (endptr)
      *endptr = any ? (char*)p : (char*)nptr;

   return any ? acc : 0;
}

__int64 strtoll_retro__(const char *nptr, char **endptr, int base)
{
   int is_neg;
   int is_ovf;
   unsigned __int64 acc = strtox_retro__(nptr, endptr, base,
         &is_neg, &is_ovf);

   if (is_neg)
   {
      /* _I64_MIN has no positive counterpart, so it is compared
       * against in unsigned form before the negation. */
      unsigned __int64 lim = (unsigned __int64)_I64_MAX + 1;

      if (is_ovf || acc > lim)
      {
         errno = ERANGE;
         return _I64_MIN;
      }
      if (acc == lim)
         return _I64_MIN;
      return -(__int64)acc;
   }

   if (is_ovf || acc > (unsigned __int64)_I64_MAX)
   {
      errno = ERANGE;
      return _I64_MAX;
   }

   return (__int64)acc;
}

unsigned __int64 strtoull_retro__(const char *nptr, char **endptr, int base)
{
   int is_neg;
   int is_ovf;
   unsigned __int64 acc = strtox_retro__(nptr, endptr, base,
         &is_neg, &is_ovf);

   if (is_ovf)
   {
      errno = ERANGE;
      return _UI64_MAX;
   }

   /* C99 requires a negated value here, not a rejection. */
   return is_neg ? (unsigned __int64)(0 - acc) : acc;
}

#endif
