/***************************************************************************

    libchdr_zlib.c

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
#include <libchdr/huffman.h>
#include <libchdr/libchdr_zlib.h>
#include <zlib.h>

#include <retro_inline.h>
#include <streams/file_stream.h>

#define TRUE 1
#define FALSE 0

/* cdzl */

chd_error cdzl_codec_init(void *codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	cdzl->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdzl->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	ret = zlib_codec_init(&cdzl->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zlib_codec_init(&cdzl->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	return CHDERR_NONE;
}

void cdzl_codec_free(void *codec)
{
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;

	zlib_codec_free(&cdzl->base_decompressor);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdzl->subcode_decompressor);
#endif
	if (cdzl->buffer)
		free(cdzl->buffer);
}

chd_error cdzl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
#ifdef WANT_RAW_DATA_SECTOR
	uint8_t *sector;
#endif
	uint32_t framenum;
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;

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
	zlib_codec_decompress(&cdzl->base_decompressor, &src[header_bytes], complen_base, &cdzl->buffer[0], frames * CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
	zlib_codec_decompress(&cdzl->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdzl->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdzl->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdzl->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
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

/***************************************************************************
    ZLIB COMPRESSION CODEC
***************************************************************************/

/*-------------------------------------------------
    zlib_codec_init - initialize the ZLIB codec
-------------------------------------------------*/

chd_error zlib_codec_init(void *codec, uint32_t hunkbytes)
{
	int zerr;
	chd_error err;
	zlib_codec_data *data = (zlib_codec_data*)codec;

	/* clear the buffers */
	memset(data, 0, sizeof(zlib_codec_data));

	/* init the inflater first */
	data->inflater.next_in = (Bytef *)data;	/* bogus, but that's ok */
	data->inflater.avail_in = 0;
	data->inflater.zalloc = zlib_fast_alloc;
	data->inflater.zfree = zlib_fast_free;
	data->inflater.opaque = &data->allocator;
	zerr = inflateInit2(&data->inflater, -MAX_WBITS);

	/* convert errors */
	if (zerr == Z_MEM_ERROR)
		err = CHDERR_OUT_OF_MEMORY;
	else if (zerr != Z_OK)
		err = CHDERR_CODEC_ERROR;
	else
		err = CHDERR_NONE;

	return err;
}

/*-------------------------------------------------
    zlib_codec_free - free data for the ZLIB
    codec
-------------------------------------------------*/

void zlib_codec_free(void *codec)
{
	zlib_codec_data *data = (zlib_codec_data *)codec;

	/* deinit the streams */
	if (data != NULL)
	{
		int i;
      zlib_allocator alloc;

		inflateEnd(&data->inflater);

		/* free our fast memory */
		alloc = data->allocator;
		for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
			if (alloc.allocptr[i])
				free(alloc.allocptr[i]);
	}
}

/*-------------------------------------------------
    zlib_codec_decompress - decomrpess data using
    the ZLIB codec
-------------------------------------------------*/

chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	zlib_codec_data *data = (zlib_codec_data *)codec;
	int zerr;

	/* reset the decompressor */
	data->inflater.next_in = (Bytef *)src;
	data->inflater.avail_in = complen;
	data->inflater.total_in = 0;
	data->inflater.next_out = (Bytef *)dest;
	data->inflater.avail_out = destlen;
	data->inflater.total_out = 0;
	zerr = inflateReset(&data->inflater);
	if (zerr != Z_OK)
		return CHDERR_DECOMPRESSION_ERROR;

	/* do it */
	zerr = inflate(&data->inflater, Z_FINISH);
    (void)zerr;
	if (data->inflater.total_out != destlen)
		return CHDERR_DECOMPRESSION_ERROR;

	return CHDERR_NONE;
}

/*-------------------------------------------------
    zlib_fast_alloc - fast malloc for ZLIB, which
    allocates and frees memory frequently
-------------------------------------------------*/

voidpf zlib_fast_alloc(voidpf opaque, uInt items, uInt size)
{
	zlib_allocator *alloc = (zlib_allocator *)opaque;
	UINT32 *ptr;
	int i;

	/* compute the size, rounding to the nearest 1k */
	size = (size * items + 0x3ff) & ~0x3ff;

	/* reuse a hunk if we can */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
	{
		ptr = alloc->allocptr[i];
		if (ptr && size == *ptr)
		{
			/* set the low bit of the size so we don't match next time */
			*ptr |= 1;
			return ptr + 1;
		}
	}

	/* alloc a new one */
	ptr = (UINT32 *)malloc(size + sizeof(UINT32));
	if (!ptr)
		return NULL;

	/* put it into the list */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
		if (!alloc->allocptr[i])
		{
			alloc->allocptr[i] = ptr;
			break;
		}

	/* set the low bit of the size so we don't match next time */
	*ptr = size | 1;
	return ptr + 1;
}

/*-------------------------------------------------
    zlib_fast_free - fast free for ZLIB, which
    allocates and frees memory frequently
-------------------------------------------------*/

void zlib_fast_free(voidpf opaque, voidpf address)
{
	zlib_allocator *alloc = (zlib_allocator *)opaque;
	UINT32 *ptr = (UINT32 *)address - 1;
	int i;

	/* find the hunk */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
		if (ptr == alloc->allocptr[i])
		{
			/* clear the low bit of the size to allow matches */
			*ptr &= ~1;
			return;
		}
}
