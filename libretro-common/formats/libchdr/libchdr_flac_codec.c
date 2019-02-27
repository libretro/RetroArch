/***************************************************************************

    libchdr_flac_codec.c

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
#include <libchdr/flac.h>
#include <libchdr/huffman.h>
#include <zlib.h>

#include <retro_inline.h>
#include <streams/file_stream.h>

#define TRUE 1
#define FALSE 0

/***************************************************************************
 *  CD FLAC DECOMPRESSOR
 ***************************************************************************
 */

/*------------------------------------------------------
 *  cdfl_codec_blocksize - return the optimal block size
 *------------------------------------------------------
 */

static uint32_t cdfl_codec_blocksize(uint32_t bytes)
{
	/* determine FLAC block size, which must be 16-65535
	 * clamp to 2k since that's supposed to be the sweet spot */
	uint32_t hunkbytes = bytes / 4;
	while (hunkbytes > 2048)
		hunkbytes /= 2;
	return hunkbytes;
}

chd_error cdfl_codec_init(void *codec, uint32_t hunkbytes)
{
#ifdef WANT_SUBCODE
   chd_error ret;
#endif
   uint16_t native_endian = 0;
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	cdfl->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdfl->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	/* determine whether we want native or swapped samples */
	*(uint8_t *)(&native_endian) = 1;
	cdfl->swap_endian = (native_endian & 1);

#ifdef WANT_SUBCODE
	/* init zlib inflater */
	ret = zlib_codec_init(&cdfl->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	/* flac decoder init */
	flac_decoder_init(&cdfl->decoder);
	if (cdfl->decoder.decoder == NULL)
		return CHDERR_OUT_OF_MEMORY;

	return CHDERR_NONE;
}

void cdfl_codec_free(void *codec)
{
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;
	flac_decoder_free(&cdfl->decoder);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdfl->subcode_decompressor);
#endif
	if (cdfl->buffer)
		free(cdfl->buffer);
}

chd_error cdfl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	uint32_t framenum;
   uint8_t *buffer;
#ifdef WANT_SUBCODE
   uint32_t offset;
   chd_error ret;
#endif
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;

	/* reset and decode */
	uint32_t frames = destlen / CD_FRAME_SIZE;

	if (!flac_decoder_reset(&cdfl->decoder, 44100, 2, cdfl_codec_blocksize(frames * CD_MAX_SECTOR_DATA), src, complen))
		return CHDERR_DECOMPRESSION_ERROR;
	buffer = &cdfl->buffer[0];
	if (!flac_decoder_decode_interleaved(&cdfl->decoder, (int16_t *)(buffer), frames * CD_MAX_SECTOR_DATA/4, cdfl->swap_endian))
		return CHDERR_DECOMPRESSION_ERROR;

#ifdef WANT_SUBCODE
	/* inflate the subcode data */
	offset = flac_decoder_finish(&cdfl->decoder);
	ret = zlib_codec_decompress(&cdfl->subcode_decompressor, src + offset, complen - offset, &cdfl->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#else
	flac_decoder_finish(&cdfl->decoder);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdfl->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdfl->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif
	}

	return CHDERR_NONE;
}
