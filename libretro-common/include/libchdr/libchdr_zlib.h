/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    libchr_zlib.h

    Zlib compression wrappers

***************************************************************************/

#pragma once

#ifndef __LIBCHDR_ZLIB_H__
#define __LIBCHDR_ZLIB_H__

#include <stdint.h>

/* The CHD zlib codec can be backed either by zlib or by the clean-room,
 * dependency-free DEFLATE/inflate codec in encodings/deflate.h.  Define
 * CHD_HAVE_ZLIB to use zlib; otherwise the built-in decoder is used.  The
 * public codec interface (zlib_codec_* / cdzl_codec_*) is identical either
 * way, so nothing downstream of this file changes. */
#if !defined(CHD_HAVE_ZLIB) && !defined(CHD_USE_BUILTIN_DEFLATE)
#if defined(HAVE_ZLIB)
#define CHD_HAVE_ZLIB 1
#else
#define CHD_USE_BUILTIN_DEFLATE 1
#endif
#endif

#ifdef CHD_HAVE_ZLIB
#include <zlib.h>
#endif
#include "coretypes.h"
#include "chd.h"

#define MAX_ZLIB_ALLOCS				64

/* codec-private data for the ZLIB codec */

typedef struct _zlib_allocator zlib_allocator;
struct _zlib_allocator
{
	uint32_t *				allocptr[MAX_ZLIB_ALLOCS];
	uint32_t *				allocptr2[MAX_ZLIB_ALLOCS];
};

typedef struct _zlib_codec_data zlib_codec_data;
struct _zlib_codec_data
{
#ifdef CHD_HAVE_ZLIB
	z_stream				inflater;
	zlib_allocator			allocator;
#else
	void				   *inflater;   /* rinflate stream handle */
#endif
};

/* codec-private data for the CDZL codec */
typedef struct _cdzl_codec_data cdzl_codec_data;
struct _cdzl_codec_data {
	/* internal state */
	zlib_codec_data		base_decompressor;
#ifdef WANT_SUBCODE
	zlib_codec_data		subcode_decompressor;
#endif
	uint8_t*			buffer;
};

extern chd_error zlib_codec_init(void *codec, uint32_t hunkbytes);
extern void zlib_codec_free(void *codec);
extern chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* zlib compression codec */
chd_error zlib_codec_init(void *codec, uint32_t hunkbytes);

void zlib_codec_free(void *codec);

chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

#ifdef CHD_HAVE_ZLIB
extern void zlib_allocator_free(voidpf opaque);

voidpf zlib_fast_alloc(voidpf opaque, uInt items, uInt size);

void zlib_fast_free(voidpf opaque, voidpf address);
#endif

/* cdzl compression codec */
chd_error cdzl_codec_init(void* codec, uint32_t hunkbytes);

void cdzl_codec_free(void* codec);

chd_error cdzl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

#endif /* __LIBCHDR_ZLIB_H__ */
