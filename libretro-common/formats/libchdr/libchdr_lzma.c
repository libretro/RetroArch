/***************************************************************************

    libchdr_lzma_codec.c

    MAME Compressed Hunks of Data file format

****************************************************************************

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libchdr/chd.h>
#include <libchdr/minmax.h>
#include <libchdr/cdrom.h>
#include <libchdr/lzma.h>
#include <libchdr/huffman.h>
#include <zlib.h>

#include <retro_inline.h>
#include <streams/file_stream.h>

#define TRUE 1
#define FALSE 0

/***************************************************************************
 *  LZMA ALLOCATOR HELPER
 ***************************************************************************
 */

/*-------------------------------------------------
 *  lzma_fast_alloc - fast malloc for lzma, which
 *  allocates and frees memory frequently
 *-------------------------------------------------
 */

static void *lzma_fast_alloc(void *p, size_t size)
{
	int scan;
    uint32_t *addr        = NULL;
	lzma_allocator *codec = (lzma_allocator *)(p);

	/* compute the size, rounding to the nearest 1k */
	size = (size + 0x3ff) & ~0x3ff;

	/* reuse a hunk if we can */
	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		uint32_t *ptr = codec->allocptr[scan];
		if (ptr != NULL && size == *ptr)
		{
			/* set the low bit of the size so we don't match next time */
			*ptr |= 1;
			return ptr + 1;
		}
	}

	/* alloc a new one and put it into the list */
	addr = (uint32_t *)malloc(sizeof(uint32_t) * (size + sizeof(uint32_t)));
	if (!addr)
		return NULL;

	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		if (codec->allocptr[scan] == NULL)
		{
			codec->allocptr[scan] = addr;
			break;
		}
	}

	/* set the low bit of the size so we don't match next time */
	*addr = (uint32_t)(size | 1);
	return addr + 1;
}

/*-------------------------------------------------
 *  lzma_fast_free - fast free for lzma, which
 *  allocates and frees memory frequently
 *-------------------------------------------------
 */
static void lzma_fast_free(void *p, void *address)
{
	int scan;
   uint32_t *ptr;
   lzma_allocator *codec;
	if (address == NULL)
		return;

	codec = (lzma_allocator *)(p);

	/* find the hunk */
	ptr = (uint32_t *)(address) - 1;
	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		if (ptr == codec->allocptr[scan])
		{
			/* clear the low bit of the size to allow matches */
			*ptr &= ~1;
			return;
		}
	}
}

/*-------------------------------------------------
 *  lzma_allocator_init
 *-------------------------------------------------
 */

void lzma_allocator_init(void* p)
{
	lzma_allocator *codec = (lzma_allocator *)(p);

	/* reset pointer list */
	memset(codec->allocptr, 0, sizeof(codec->allocptr));
	codec->Alloc = lzma_fast_alloc;
	codec->Free = lzma_fast_free;
}

/*-------------------------------------------------
 *  lzma_allocator_free
 *-------------------------------------------------
 */

void lzma_allocator_free(void* p )
{
	lzma_allocator *codec = (lzma_allocator *)(p);

	/* free our memory */
	int i;
	for (i = 0 ; i < MAX_LZMA_ALLOCS ; i++)
	{
		if (codec->allocptr[i] != NULL)
			free(codec->allocptr[i]);
	}
}

/***************************************************************************
 *  LZMA DECOMPRESSOR
 ***************************************************************************
 */

/*-------------------------------------------------
 *  lzma_codec_init - constructor
 *-------------------------------------------------
 */

chd_error lzma_codec_init(void* codec, uint32_t hunkbytes)
{
	CLzmaEncProps encoder_props;
   CLzmaEncHandle enc;
	uint8_t decoder_props[LZMA_PROPS_SIZE];
   lzma_allocator* alloc;
   size_t props_size;
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;

	/* construct the decoder */
	LzmaDec_Construct(&lzma_codec->decoder);

	/* FIXME: this code is written in a way that makes it impossible to safely upgrade the LZMA SDK
	 * This code assumes that the current version of the encoder imposes the same requirements on the
	 * decoder as the encoder used to produce the file.  This is not necessarily true.  The format
	 * needs to be changed so the encoder properties are written to the file.

	 * configure the properties like the compressor did */
	LzmaEncProps_Init(&encoder_props);
	encoder_props.level = 9;
	encoder_props.reduceSize = hunkbytes;
	LzmaEncProps_Normalize(&encoder_props);

	/* convert to decoder properties */
	alloc = &lzma_codec->allocator;
	lzma_allocator_init(alloc);
	enc = LzmaEnc_Create((ISzAlloc*)alloc);
	if (!enc)
		return CHDERR_DECOMPRESSION_ERROR;
	if (LzmaEnc_SetProps(enc, &encoder_props) != SZ_OK)
	{
		LzmaEnc_Destroy(enc, (ISzAlloc*)&alloc, (ISzAlloc*)&alloc);
		return CHDERR_DECOMPRESSION_ERROR;
	}
	props_size = sizeof(decoder_props);
	if (LzmaEnc_WriteProperties(enc, decoder_props, &props_size) != SZ_OK)
	{
		LzmaEnc_Destroy(enc, (ISzAlloc*)alloc, (ISzAlloc*)alloc);
		return CHDERR_DECOMPRESSION_ERROR;
	}
	LzmaEnc_Destroy(enc, (ISzAlloc*)alloc, (ISzAlloc*)alloc);

	/* do memory allocations */
	if (LzmaDec_Allocate(&lzma_codec->decoder, decoder_props, LZMA_PROPS_SIZE, (ISzAlloc*)alloc) != SZ_OK)
		return CHDERR_DECOMPRESSION_ERROR;

	/* Okay */
	return CHDERR_NONE;
}

/*-------------------------------------------------
 *  lzma_codec_free
 *-------------------------------------------------
 */

void lzma_codec_free(void* codec)
{
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;
	lzma_allocator* alloc = &lzma_codec->allocator;

	/* free memory */
	LzmaDec_Free(&lzma_codec->decoder, (ISzAlloc*)&lzma_codec->allocator);
	lzma_allocator_free(alloc);
}

/*-------------------------------------------------
 *  decompress - decompress data using the LZMA
 *  codec
 *-------------------------------------------------
 */

chd_error lzma_codec_decompress(void* codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	ELzmaStatus status;
   SRes res;
   size_t consumedlen, decodedlen;
	/* initialize */
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;
	LzmaDec_Init(&lzma_codec->decoder);

	/* decode */
	consumedlen = complen;
	decodedlen = destlen;
	res = LzmaDec_DecodeToBuf(&lzma_codec->decoder, dest, &decodedlen, src, &consumedlen, LZMA_FINISH_END, &status);
	if ((res != SZ_OK && res != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) || consumedlen != complen || decodedlen != destlen)
		return CHDERR_DECOMPRESSION_ERROR;
	return CHDERR_NONE;
}

/* cdlz */
chd_error cdlz_codec_init(void* codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdlz_codec_data* cdlz = (cdlz_codec_data*) codec;

	/* allocate buffer */
	cdlz->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdlz->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	ret = lzma_codec_init(&cdlz->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zlib_codec_init(&cdlz->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	return CHDERR_NONE;
}

void cdlz_codec_free(void* codec)
{
	cdlz_codec_data* cdlz = (cdlz_codec_data*) codec;

	lzma_codec_free(&cdlz->base_decompressor);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdlz->subcode_decompressor);
#endif
	if (cdlz->buffer)
		free(cdlz->buffer);
}

chd_error cdlz_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
#ifdef WANT_RAW_DATA_SECTOR
	uint8_t *sector;
#endif
	uint32_t framenum;
	cdlz_codec_data* cdlz = (cdlz_codec_data*)codec;

	/* determine header bytes */
	uint32_t frames = destlen / CD_FRAME_SIZE;
	uint32_t complen_bytes = (destlen < 65536) ? 2 : 3;
	uint32_t ecc_bytes = (frames + 7) / 8;
	uint32_t header_bytes = ecc_bytes + complen_bytes;

	/* extract compressed length of base */
	uint32_t complen_base = (src[ecc_bytes + 0] << 8) | src[ecc_bytes + 1];
	if (complen_bytes > 2)
		complen_base = (complen_base << 8) | src[ecc_bytes + 2];

	/* reset and decode */
	lzma_codec_decompress(&cdlz->base_decompressor, &src[header_bytes], complen_base, &cdlz->buffer[0], frames * CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
	if (header_bytes + complen_base >= complen)
		return CHDERR_DECOMPRESSION_ERROR;
	zlib_codec_decompress(&cdlz->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdlz->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdlz->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdlz->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif

#ifdef WANT_RAW_DATA_SECTOR
		/* reconstitute the ECC data and sync header */
		sector = (uint8_t *)&dest[framenum * CD_FRAME_SIZE];
		if ((src[framenum / 8] & (1 << (framenum % 8))) != 0)
		{
			memcpy(sector, s_cd_sync_header, sizeof(s_cd_sync_header));
			ecc_generate(sector);
		}
#endif
	}
	return CHDERR_NONE;
}
