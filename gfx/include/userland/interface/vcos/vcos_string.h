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

/*=============================================================================
VideoCore OS Abstraction Layer - public header file
=============================================================================*/

#ifndef VCOS_STRING_H
#define VCOS_STRING_H

/**
  * \file
  *
  * String functions.
  *
  */

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

/** Case insensitive string comparison.
  *
  */

VCOS_INLINE_DECL
int vcos_strcasecmp(const char *s1, const char *s2);

VCOS_INLINE_DECL
int vcos_strncasecmp(const char *s1, const char *s2, size_t n);

VCOSPRE_ int VCOSPOST_ vcos_vsnprintf(char *buf, size_t buflen, const char *fmt, va_list ap);

VCOSPRE_ int VCOSPOST_ vcos_snprintf(char *buf, size_t buflen, const char *fmt, ...);

/** Like vsnprintf, except it places the output at the specified offset.
  * Output is truncated to fit in buflen bytes, and is guaranteed to be NUL-terminated.
  * Returns the string length before/without truncation.
  */
VCOSPRE_ size_t VCOSPOST_ vcos_safe_vsprintf(char *buf, size_t buflen, size_t offset, const char *fmt, va_list ap);

#define VCOS_SAFE_VSPRINTF(buf, offset, fmt, ap) \
   vcos_safe_vsprintf(buf, sizeof(buf) + ((char (*)[sizeof(buf)])buf - &(buf)), offset, fmt, ap)

/** Like snprintf, except it places the output at the specified offset.
  * Output is truncated to fit in buflen bytes, and is guaranteed to be NUL-terminated.
  * Returns the string length before/without truncation.
  */
VCOSPRE_ size_t VCOSPOST_ vcos_safe_sprintf(char *buf, size_t buflen, size_t offset, const char *fmt, ...);

/* The Metaware compiler currently has a bug in its variadic macro handling which
   causes it to append a spurious command to the end of its __VA_ARGS__ data.
   Do not use until this has been fixed (and this comment has been deleted). */

#define VCOS_SAFE_SPRINTF(buf, offset, ...) \
   vcos_safe_sprintf(buf, sizeof(buf) + ((char (*)[sizeof(buf)])buf - &(buf)), offset, __VA_ARGS__)

/** Copies string src to dst at the specified offset.
  * Output is truncated to fit in dstlen bytes, i.e. the string is at most
  * (buflen - 1) characters long. Unlike strncpy, exactly one NUL is written
  * to dst, which is always NUL-terminated.
  * Returns the string length before/without truncation.
  */
VCOSPRE_ size_t VCOSPOST_ vcos_safe_strcpy(char *dst, const char *src, size_t dstlen, size_t offset);

#define VCOS_SAFE_STRCPY(dst, src, offset) \
   vcos_safe_strcpy(dst, src, sizeof(dst) + ((char (*)[sizeof(dst)])dst - &(dst)), offset)

VCOS_STATIC_INLINE
int vcos_strlen(const char *s) { return (int)strlen(s); }

VCOS_STATIC_INLINE
int vcos_strcmp(const char *s1, const char *s2) { return strcmp(s1,s2); }

VCOS_STATIC_INLINE
int vcos_strncmp(const char *cs, const char *ct, size_t count) { return strncmp(cs, ct, count); }

VCOS_STATIC_INLINE
char *vcos_strcpy(char *dst, const char *src) { return strcpy(dst, src); }

VCOS_STATIC_INLINE
char *vcos_strncpy(char *dst, const char *src, size_t count) { return strncpy(dst, src, count); }

VCOS_STATIC_INLINE
void *vcos_memcpy(void *dst, const void *src, size_t n) {  memcpy(dst, src, n);  return dst;  }

VCOS_STATIC_INLINE
void *vcos_memset(void *p, int c, size_t n) { return memset(p, c, n); }

VCOS_STATIC_INLINE
int vcos_memcmp(const void *ptr1, const void *ptr2, size_t count) { return memcmp(ptr1, ptr2, count); }

#ifdef __cplusplus
}
#endif
#endif
