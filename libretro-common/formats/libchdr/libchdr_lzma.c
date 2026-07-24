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
#include <libchdr/libchdr_zlib.h>
#include <libchdr/minmax.h>
#include <libchdr/cdrom.h>
#include <libchdr/huffman.h>
#include <libchdr/lzma.h>

#include <retro_inline.h>
#include <streams/file_stream.h>

/***************************************************************************
 *  LZMA ALLOCATOR HELPER
 ***************************************************************************
 */


/*-------------------------------------------------
 *  lzma_compute_dictionary_size
 *
 *  The decoder only needs the dictionary size the encoder would have
 *  written into the properties byte stream. Deriving it by actually
 *  constructing an LZMA encoder (LzmaEncProps_Normalize +
 *  LzmaEnc_SetProps + LzmaEnc_WriteProperties, as this file
 *  historically did) drags the whole encoder half of the LZMA SDK
 *  into every build for a handful of arithmetic.
 *
 *  For the fixed inputs used here (level 9, reduceSize = hunkbytes)
 *  the encoder's answer is: the level default (1 << 28 on 64-bit
 *  hosts, 1 << 26 on 32-bit), reduced - when it exceeds the hunk
 *  size - to the smallest value of the form 2 << i or 3 << i that
 *  covers it, with a 4 KiB floor.  Verified byte-identical to the
 *  encoder-derived properties across every chdman CD hunk size and
 *  the power-of-two boundaries around them.
 *
 *  Note this is NOT the computation the libchdr fork ships: its
 *  decode-only derivation applies the reduce clamp as
 *  MAX(min(4K, reduce), dictSize), which never reduces anything, so
 *  every cdlz codec allocates the full level-9 default - a measured
 *  256 MiB LzmaDec dictionary per open CHD where the encoder used
 *  24 KiB.  Decode still succeeds (any dictionary at least as large
 *  as the encoder's works), which is how it went unnoticed.
 *-------------------------------------------------
 */

static uint32_t lzma_compute_dictionary_size(uint32_t hunkbytes)
{
	/* level-9 default dictionary for the host word size, as
	 * LzmaEncProps_Normalize computes it */
	uint32_t dictSize = (sizeof(size_t) <= 4)
		? ((uint32_t)1 << 26) : ((uint32_t)1 << 28);

	if (dictSize > hunkbytes)
	{
		/* reduce to the smallest {2,3} << i covering hunkbytes; the
		 * i = 11 start gives the SDK's 4 KiB floor.  Every ladder
		 * value below 2 MiB is already in LzmaEnc_WriteProperties'
		 * aligned form, and every one at or above it is a 1 MiB
		 * multiple, so no further alignment step is needed. */
		unsigned i;
		for (i = 11; i <= 30; i++)
		{
			uint32_t v = (uint32_t)2 << i;
			if (hunkbytes <= v) { dictSize = v; break; }
			v = (uint32_t)3 << i;
			if (hunkbytes <= v) { dictSize = v; break; }
		}
	}
	return dictSize;
}

chd_error lzma_codec_init(void* codec, uint32_t hunkbytes)
{
	unsigned int i;
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;
	const uint32_t dictSize = lzma_compute_dictionary_size(hunkbytes);
	uint8_t decoder_props[RLZMA_PROPS_SIZE];

	/* properties byte 0 encodes (pb * 5 + lp) * 9 + lc for the encoder
	 * defaults lc=3 lp=0 pb=2 -> 93; bytes 1..4 the dictionary size LE */
	decoder_props[0] = 93;
	for (i = 0; i < RLZMA_PROPS_SIZE - 1; ++i)
		decoder_props[1 + i] = (uint8_t)((dictSize >> (8 * i)) & 0xFF);

	/* No allocation: the decoder state is inline and the dictionary is
	 * the caller's own output buffer. */
	if (rlzma_dec_init(&lzma_codec->decoder, decoder_props) != RLZMA_OK)
		return CHDERR_DECOMPRESSION_ERROR;

	return CHDERR_NONE;
}

/*-------------------------------------------------
 *  lzma_codec_free
 *-------------------------------------------------
 */

void lzma_codec_free(void* codec)
{
	/* rlzma holds no resources. */
	(void)codec;
}

/*-------------------------------------------------
 *  decompress - decompress data using the LZMA
 *  codec
 *-------------------------------------------------
 */

chd_error lzma_codec_decompress(void* codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;

	/* Decodes straight into dest, which doubles as the dictionary window.
	 * The decoder resets itself, so hunks stay independent. */
	if (rlzma_dec_decode(&lzma_codec->decoder, dest, destlen, src, complen) != RLZMA_OK)
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

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	ret = lzma_codec_init(&cdlz->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zlib_codec_init(&cdlz->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	return CHDERR_NONE;
}

void cdlz_codec_free(void* codec)
{
	cdlz_codec_data* cdlz = (cdlz_codec_data*) codec;
	free(cdlz->buffer);
	lzma_codec_free(&cdlz->base_decompressor);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdlz->subcode_decompressor);
#endif
}

chd_error cdlz_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
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
	zlib_codec_decompress(&cdlz->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdlz->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		uint8_t *sector;

		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdlz->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdlz->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif

#ifdef WANT_RAW_DATA_SECTOR
		/* reconstitute the ECC data and sync header */
		sector = (uint8_t *)&dest[framenum * CD_FRAME_SIZE];
		if ((src[framenum / 8] & (1 << (framenum % 8))) != 0)
		{
			const uint8_t s_cd_sync_header[12] = { 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00 };
			memcpy(sector, s_cd_sync_header, sizeof(s_cd_sync_header));
			ecc_generate(sector);
		}
#endif
	}
	return CHDERR_NONE;
}
