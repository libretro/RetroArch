/***************************************************************************

    libchdr_zstd.c

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
#include <libchdr/libchdr_zstd.h>

#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#else
#include <zstd.h>
#endif

#include <retro_inline.h>
#include <streams/file_stream.h>

/***************************************************************************
 *  ZSTD DECOMPRESSOR
 ***************************************************************************
 */

/*-------------------------------------------------
 *  zstd_codec_init - constructor
 *-------------------------------------------------
 */

chd_error zstd_codec_init(void* codec, uint32_t hunkbytes)
{
#ifdef HAVE_RZSTD
	/* Nothing to construct: rzstd holds no state across calls. */
	(void)codec;
	(void)hunkbytes;
	return CHDERR_NONE;
#else
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;

	zstd_codec->dstream = ZSTD_createDStream();
	if (!zstd_codec->dstream) {
		printf("NO DSTREAM CREATED!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}
	return CHDERR_NONE;
#endif
}

/*-------------------------------------------------
 *  zstd_codec_free
 *-------------------------------------------------
 */

void zstd_codec_free(void* codec)
{
#ifdef HAVE_RZSTD
	(void)codec;
#else
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;

	ZSTD_freeDStream(zstd_codec->dstream);
#endif
}

/*-------------------------------------------------
 *  decompress - decompress data using the ZSTD 
 *  codec
 *-------------------------------------------------
 */
chd_error zstd_codec_decompress(void* codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
#ifdef HAVE_RZSTD
	/* Every caller hands over one whole frame and already knows how
	 * large it expands to, so this is a single call. complen is the
	 * exact frame length in both callers - cdzs splits the blob on the
	 * stored base length - which matters because rzstd treats bytes
	 * left over after the frame as an error rather than ignoring them. */
	size_t wrote = 0;

	(void)codec;

	if (rzstd_decode(dest, destlen, src, complen, &wrote)
			!= RZSTD_PROCESS_END)
		return CHDERR_DECOMPRESSION_ERROR;
	if (wrote != destlen)
		return CHDERR_DECOMPRESSION_ERROR;
	return CHDERR_NONE;
#else
	ZSTD_inBuffer input;
	ZSTD_outBuffer output;

	/* initialize */
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;

	/* reset decompressor */
	size_t zstd_res = ZSTD_initDStream(zstd_codec->dstream);

	input.src   = src;
	input.size  = complen;
	input.pos   = 0;

	output.dst  = dest;
	output.size = destlen;
	output.pos  = 0;

	if (ZSTD_isError(zstd_res)) 
	{
		printf("INITI DSTREAM FAILED!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}

	while ((input.pos < input.size) && (output.pos < output.size))
	{
		zstd_res = ZSTD_decompressStream(zstd_codec->dstream, &output, &input);
		if (ZSTD_isError(zstd_res))
		{
			printf("DECOMPRESSION ERROR IN LOOP\n");
			return CHDERR_DECOMPRESSION_ERROR;
		}
	}
	if (output.pos != output.size)
	{
		printf("OUTPUT DOESN'T MATCH!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}
	return CHDERR_NONE;
#endif
}

/* cdzs */
chd_error cdzs_codec_init(void* codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdzs_codec_data* cdzs = (cdzs_codec_data*) codec;

	/* allocate buffer */
	cdzs->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdzs->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	ret = zstd_codec_init(&cdzs->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zstd_codec_init(&cdzs->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	return CHDERR_NONE;
}

void cdzs_codec_free(void* codec)
{
	cdzs_codec_data* cdzs = (cdzs_codec_data*) codec;
	free(cdzs->buffer);
	zstd_codec_free(&cdzs->base_decompressor);
#ifdef WANT_SUBCODE
	zstd_codec_free(&cdzs->subcode_decompressor);
#endif
}

chd_error cdzs_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	chd_error ret;
	uint32_t framenum;
	uint32_t complen_base;
	cdzs_codec_data* cdzs = (cdzs_codec_data*)codec;

	/* determine header bytes */
	uint32_t frames = destlen / CD_FRAME_SIZE;
	uint32_t complen_bytes = (destlen < 65536) ? 2 : 3;
	uint32_t ecc_bytes = (frames + 7) / 8;
	uint32_t header_bytes = ecc_bytes + complen_bytes;

	/* header_bytes and complen_base are both taken from the hunk, so
	 * neither can be trusted before it is checked. Without this, a
	 * complen_base larger than the hunk makes
	 * `complen - complen_base - header_bytes` underflow to near 2^32 and
	 * `&src[header_bytes + complen_base]` point past the end, handing the
	 * subcode decompressor an out-of-bounds pointer and an enormous
	 * length - a heap overread straight off a malformed image. Reading
	 * the length bytes at src[ecc_bytes..] needs the first check too. */
	if (complen < header_bytes)
		return CHDERR_DECOMPRESSION_ERROR;
	/* extract compressed length of base */
	complen_base = (src[ecc_bytes + 0] << 8) | src[ecc_bytes + 1];
	if (complen_bytes > 2)
		complen_base = (complen_base << 8) | src[ecc_bytes + 2];

	if (complen_base > complen - header_bytes)
		return CHDERR_DECOMPRESSION_ERROR;

	/* A failed hunk has to be reported, not reassembled. Neither return
	 * value was read, so a decode that bailed left cdXX->buffer holding
	 * whatever the previous hunk put there - or, on the first hunk,
	 * uninitialised heap, the buffer being malloc'd - and the loop below
	 * copied that into dest and returned CHDERR_NONE. The caller cannot
	 * tell that apart from a good hunk. cdfl_codec_decompress() already
	 * checks its subcode decode; this brings the other three into line. */
	/* reset and decode */
	ret = zstd_codec_decompress(&cdzs->base_decompressor, &src[header_bytes], complen_base, &cdzs->buffer[0], frames * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#ifdef WANT_SUBCODE
	ret = zstd_codec_decompress(&cdzs->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdzs->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		uint8_t *sector;

		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdzs->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdzs->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
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
