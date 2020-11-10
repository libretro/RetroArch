/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    lzma.h

    LZMA compression wrappers

***************************************************************************/

#pragma once

#ifndef __LIBCHDR_LZMA_H__
#define __LIBCHDR_LZMA_H__

#include <stdint.h>

#include <LzmaEnc.h>
#include <LzmaDec.h>

#include <libchdr/libchdr_zlib.h>

/* codec-private data for the LZMA codec */
#define MAX_LZMA_ALLOCS 64

typedef struct _lzma_allocator lzma_allocator;
struct _lzma_allocator
{
	void *(*Alloc)(void *p, size_t size);
 	void (*Free)(void *p, void *address); /* address can be 0 */
	void (*FreeSz)(void *p, void *address, size_t size); /* address can be 0 */
	uint32_t*	allocptr[MAX_LZMA_ALLOCS];
	uint32_t*	allocptr2[MAX_LZMA_ALLOCS];
};

typedef struct _lzma_codec_data lzma_codec_data;
struct _lzma_codec_data
{
	CLzmaDec		decoder;
	lzma_allocator	allocator;
};

/* codec-private data for the CDLZ codec */
typedef struct _cdlz_codec_data cdlz_codec_data;
struct _cdlz_codec_data {
	/* internal state */
	lzma_codec_data		base_decompressor;
#ifdef WANT_SUBCODE
	zlib_codec_data		subcode_decompressor;
#endif
	uint8_t*			buffer;
};

chd_error lzma_codec_init(void* codec, uint32_t hunkbytes);

void lzma_codec_free(void* codec);

/*-------------------------------------------------
 *  decompress - decompress data using the LZMA
 *  codec
 *-------------------------------------------------
 */

chd_error lzma_codec_decompress(void* codec, const uint8_t *src,
      uint32_t complen, uint8_t *dest, uint32_t destlen);

chd_error cdlz_codec_init(void* codec, uint32_t hunkbytes);

void cdlz_codec_free(void* codec);

chd_error cdlz_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

#endif /* __LIBCHDR_LZMA_H__ */
