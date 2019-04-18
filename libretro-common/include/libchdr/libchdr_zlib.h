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

#include <zlib.h>
#include "coretypes.h"
#include "chd.h"

#define MAX_ZLIB_ALLOCS				64

/* codec-private data for the ZLIB codec */

typedef struct _zlib_allocator zlib_allocator;
struct _zlib_allocator
{
	UINT32 *				allocptr[MAX_ZLIB_ALLOCS];
};

typedef struct _zlib_codec_data zlib_codec_data;
struct _zlib_codec_data
{
	z_stream				inflater;
	zlib_allocator			allocator;
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

/* zlib compression codec */
chd_error zlib_codec_init(void *codec, uint32_t hunkbytes);

void zlib_codec_free(void *codec);

chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

voidpf zlib_fast_alloc(voidpf opaque, uInt items, uInt size);

void zlib_fast_free(voidpf opaque, voidpf address);

/* cdzl compression codec */
chd_error cdzl_codec_init(void* codec, uint32_t hunkbytes);

void cdzl_codec_free(void* codec);

chd_error cdzl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

#endif /* __LIBCHDR_ZLIB_H__ */
