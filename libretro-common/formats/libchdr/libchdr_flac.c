/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    flac.c

    FLAC compression wrappers

***************************************************************************/

#include <string.h>
#include <boolean.h>

#include <libchdr/flac.h>
#include <formats/rflac.h>

/***************************************************************************
 *  FLAC DECODER
 ***************************************************************************
 */

static void flac_decoder_write_callback(void *userdata, void *buffer, size_t bytes);


/* getters (valid after reset) */
/*static uint32_t sample_rate(flac_decoder *decoder)  { return decoder->sample_rate; }*/
static uint8_t channels(flac_decoder *decoder)  { return decoder->channels; }
/*static uint8_t bits_per_sample(flac_decoder *decoder) { return decoder->bits_per_sample; }*/

/*-------------------------------------------------
 *  flac_decoder - constructor
 *-------------------------------------------------
 */

int flac_decoder_init(flac_decoder *decoder)
{
	decoder->decoder = NULL;
	decoder->sample_rate = 0;
	decoder->channels = 0;
	decoder->bits_per_sample = 0;
	decoder->compressed_offset = 0;
	decoder->compressed_start = NULL;
	decoder->compressed_length = 0;
	decoder->compressed2_start = NULL;
	decoder->compressed2_length = 0;
	decoder->uncompressed_offset = 0;
	decoder->uncompressed_length = 0;
	decoder->uncompressed_swap = 0;
	return 0;
}

/*-------------------------------------------------
 *  flac_decoder - destructor
 *-------------------------------------------------
 */

void flac_decoder_free(flac_decoder* decoder)
{
	if ((decoder != NULL) && (decoder->decoder != NULL)) {
		rflac_free(decoder->decoder);
		decoder->decoder = NULL;
	}
}

/*-------------------------------------------------
 *  reset - reset state with the original
 *  parameters
 *-------------------------------------------------
 */

static int flac_decoder_internal_reset(flac_decoder* decoder)
{
	/* A CHD hunk is a bare run of FLAC frames whose geometry the
	 * container records, so the decoder is told it directly. There is
	 * no header in the data and none is fabricated to stand in for
	 * one. */
	rflac_format_t fmt;
	decoder->compressed_offset = 0;
	flac_decoder_free(decoder);
	fmt.sample_rate     = decoder->sample_rate;
	fmt.channels        = decoder->channels;
	fmt.bits_per_sample = 16;
	fmt.block_size      = decoder->block_size;
	decoder->decoder    = rflac_new_raw(&fmt);
	if (decoder->decoder == NULL)
		return 0;
	rflac_set_in(decoder->decoder, decoder->compressed2_start,
			decoder->compressed2_length);
	return 1;
}

/*-------------------------------------------------
 *  reset - reset state with new memory parameters
 *  and a custom-generated header
 *-------------------------------------------------
 */

int flac_decoder_reset(flac_decoder* decoder, uint32_t sample_rate, uint8_t num_channels, uint32_t block_size, const void *buffer, uint32_t length)
{
	/* No header is built. The stream carries none, and the geometry a
	 * fabricated one would have stated is passed to the decoder
	 * directly instead. The block size keeps the value the old
	 * template encoded, which counts samples across all channels
	 * rather than PCM frames. */
	decoder->sample_rate        = sample_rate;
	decoder->channels           = num_channels;
	decoder->bits_per_sample    = 16;
	decoder->block_size         = block_size * num_channels;
	decoder->compressed_start   = NULL;
	decoder->compressed_length  = 0;
	decoder->compressed2_start  = (const uint8_t *)(buffer);
	decoder->compressed2_length = length;
	return flac_decoder_internal_reset(decoder);
}

/*-------------------------------------------------
 *  decode_interleaved - decode to an interleaved
 *  sound stream
 *-------------------------------------------------
 */

int flac_decoder_decode_interleaved(flac_decoder* decoder, int16_t *samples, uint32_t num_samples, int swap_endian)
{
#define	BUFFER	2352	/* bytes per CD audio sector */
	int16_t buffer[BUFFER];
	uint32_t buf_samples = BUFFER / channels(decoder);

	/* configure the uncompressed buffer */
	memset(decoder->uncompressed_start, 0, sizeof(decoder->uncompressed_start));
	decoder->uncompressed_start[0] = samples;
	decoder->uncompressed_offset = 0;
	decoder->uncompressed_length = num_samples;
	decoder->uncompressed_swap = swap_endian;

	/* loop until we get everything we want */
	while (decoder->uncompressed_offset < decoder->uncompressed_length) {
		uint32_t frames = (num_samples < buf_samples ? num_samples : buf_samples);
		size_t   got = 0;
		rflac_set_out_s16(decoder->decoder, buffer, frames);
		while (got < frames) {
			size_t rd = 0, wr = 0;
			int e = rflac_process(decoder->decoder, &rd, &wr);
			decoder->compressed_offset += rd;
			got += wr;
			if (e == RFLAC_PROCESS_ERROR)
				return 0;
			if (wr == 0)
				break;
		}
		if (got == 0)
			return 0;
		flac_decoder_write_callback(decoder, buffer, got*sizeof(*buffer)*channels(decoder));
		num_samples -= (uint32_t)got;
	}
	return 1;
}

/*-------------------------------------------------
 *  finish - finish up the decode
 *-------------------------------------------------
 */

uint32_t flac_decoder_finish(flac_decoder* decoder)
{
	/* The decoder reports what it consumed as it goes, so this is a
	 * running total rather than something reconstructed afterwards.
	 * It used to be recovered by replaying the bitstream reader's L1
	 * and L2 cache arithmetic, because nothing exposed the figure. */
	uint32_t position = decoder->compressed_offset;
	flac_decoder_free(decoder);
	return position;
}



/*-------------------------------------------------
 *  write_callback - handle writes to the output
 *  stream
 *-------------------------------------------------
 */

static void flac_decoder_write_callback(void *userdata, void *buffer, size_t bytes)
{
	int sampnum, chan;
	int shift, blocksize;
	flac_decoder * decoder = (flac_decoder *)userdata;
	int16_t *sampbuf = (int16_t *)buffer;
	int sampch = channels(decoder);
	uint32_t offset = decoder->uncompressed_offset;
	uint16_t usample;

	/* interleaved case */
	shift = decoder->uncompressed_swap ? 8 : 0;
	blocksize = bytes / (sampch * sizeof(sampbuf[0]));
	if (decoder->uncompressed_start[1] == NULL)
	{
		int16_t *dest = decoder->uncompressed_start[0] + offset * sampch;
		for (sampnum = 0; sampnum < blocksize && offset < decoder->uncompressed_length; sampnum++, offset++)
			for (chan = 0; chan < sampch; chan++) {
				usample = (uint16_t)*sampbuf++;
				*dest++ = (int16_t)((usample << shift) | (usample >> shift));
			}
	}

	/* non-interleaved case */
	else
	{
		for (sampnum = 0; sampnum < blocksize && offset < decoder->uncompressed_length; sampnum++, offset++)
			for (chan = 0; chan < sampch; chan++) {
				usample = (uint16_t)*sampbuf++;
				if (decoder->uncompressed_start[chan] != NULL)
					decoder->uncompressed_start[chan][offset] = (int16_t) ((usample << shift) | (usample >> shift));
			}
	}
	decoder->uncompressed_offset = offset;
}


