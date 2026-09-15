/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    bitstream.c

    Helper classes for reading/writing at the bit level.

***************************************************************************/

#include <stdlib.h>
#include <libchdr/bitstream.h>

/***************************************************************************
 *  INLINE FUNCTIONS
 ***************************************************************************
 */

int bitstream_overflow(struct bitstream* bitstream) { return ((bitstream->doffset - bitstream->bits / 8) > bitstream->dlength); }

/*-------------------------------------------------
 *  create_bitstream - constructor
 *-------------------------------------------------
 */

struct bitstream* create_bitstream(const void *src, uint32_t srclength)
{
	struct bitstream* bitstream = (struct bitstream*)malloc(sizeof(struct bitstream));
	bitstream->buffer = 0;
	bitstream->bits = 0;
	bitstream->read = (const uint8_t*)src;
	bitstream->doffset = 0;
	bitstream->dlength = srclength;
	return bitstream;
}


/*-----------------------------------------------------
 *  bitstream_peek - fetch the requested number of bits
 *  but don't advance the input pointer
 *-----------------------------------------------------
 */

uint32_t bitstream_peek(struct bitstream* bitstream, int numbits)
{
	if (numbits == 0)
		return 0;

	/* fetch data if we need more */
	if (numbits > bitstream->bits)
	{
		while (bitstream->bits <= 24)
		{
			/* bits can be negative here. bitstream_peek refills to at
			 * least 25 bits but bitstream_remove will take up to 31, which
			 * a huffman code longer than the data remaining does, so the
			 * accumulator goes into deficit and 24 - bits exceeds 31 - an
			 * out-of-range shift, and undefined.
			 *
			 * The deficit is deliberate and must be preserved:
			 * bitstream_overflow is (doffset - bits/8) > dlength, so a
			 * negative bits is how over-consumption is detected. Clamping
			 * it would hide that. Skip only the OR - the accumulator's
			 * contents are already meaningless once the stream has been
			 * over-consumed - and let doffset and bits advance exactly as
			 * before, so overflow reporting is unchanged. A well-formed
			 * stream never reaches here with bits below zero. */
			if (bitstream->doffset < bitstream->dlength && bitstream->bits >= 0)
				/* Cast before shifting. read[] is a uint8_t, promoted to
				 * int, so a byte from 0x80 up shifted left by 24 - which is
				 * the shift used whenever the accumulator is empty -
				 * overflows a signed int. This is the huffman map decoder's
				 * inner loop, so it runs over every byte of a compressed
				 * CHD's map. */
				bitstream->buffer |= (uint32_t)bitstream->read[bitstream->doffset] << (24 - bitstream->bits);
			bitstream->doffset++;
			bitstream->bits += 8;
		}
	}

	/* return the data */
	return bitstream->buffer >> (32 - numbits);
}


/*-----------------------------------------------------
 *  bitstream_remove - advance the input pointer by the
 *  specified number of bits
 *-----------------------------------------------------
 */

void bitstream_remove(struct bitstream* bitstream, int numbits)
{
	bitstream->buffer <<= numbits;
	bitstream->bits -= numbits;
}


/*-----------------------------------------------------
 *  bitstream_read - fetch the requested number of bits
 *-----------------------------------------------------
 */

uint32_t bitstream_read(struct bitstream* bitstream, int numbits)
{
	uint32_t result = bitstream_peek(bitstream, numbits);
	bitstream_remove(bitstream, numbits);
	return result;
}


/*-------------------------------------------------
 *  read_offset - return the current read offset
 *-------------------------------------------------
 */

uint32_t bitstream_read_offset(struct bitstream* bitstream)
{
	uint32_t result = bitstream->doffset;
	int bits = bitstream->bits;
	while (bits >= 8)
	{
		result--;
		bits -= 8;
	}
	return result;
}


/*-------------------------------------------------
 *  flush - flush to the nearest byte
 *-------------------------------------------------
 */

uint32_t bitstream_flush(struct bitstream* bitstream)
{
	while (bitstream->bits >= 8)
	{
		bitstream->doffset--;
		bitstream->bits -= 8;
	}
	bitstream->bits = bitstream->buffer = 0;
	return bitstream->doffset;
}

