/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/vcos/vcos.h"

#if defined( __KERNEL__ )
#include <linux/string.h>
#include <linux/module.h>
#else
#include <string.h>
#endif

#include <stdarg.h>

/** Like vsnprintf, except it places the output at the specified offset.
  * Output is truncated to fit in buflen bytes, and is guaranteed to be NUL-terminated.
  * Returns the string length before/without truncation.
  */
size_t vcos_safe_vsprintf(char *buf, size_t buflen, size_t offset, const char *fmt, va_list ap)
{
   size_t space = (offset < buflen) ? (buflen - offset) : 0;

   offset += vcos_vsnprintf(buf ? (buf + offset) : NULL, space, fmt, ap);

   return offset;
}

/** Like snprintf, except it places the output at the specified offset.
  * Output is truncated to fit in buflen bytes, and is guaranteed to be NUL-terminated.
  * Returns the string length before/without truncation.
  */
size_t vcos_safe_sprintf(char *buf, size_t buflen, size_t offset, const char *fmt, ...)
{
   size_t space = (offset < buflen) ? (buflen - offset) : 0;
   va_list ap;

   va_start(ap, fmt);

   offset += vcos_vsnprintf(buf ? (buf + offset) : NULL, space, fmt, ap);

   va_end(ap);

   return offset;
}

/** Copies string src to dst at the specified offset.
  * Output is truncated to fit in dstlen bytes, i.e. the string is at most
  * (buflen - 1) characters long. Unlike strncpy, exactly one NUL is written
  * to dst, which is always NUL-terminated.
  * Returns the string length before/without truncation.
  */
size_t vcos_safe_strcpy(char *dst, const char *src, size_t dstlen, size_t offset)
{
   if (offset < dstlen)
   {
      const char *p = src;
      char *endp = dst + dstlen -1;

      dst += offset;

      for (; *p!='\0' && dst != endp; dst++, p++)
         *dst = *p;
      *dst = '\0';
   }
   offset += strlen(src);

   return offset;
}
