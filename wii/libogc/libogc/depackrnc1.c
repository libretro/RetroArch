#include <gccore.h>

#define RNC_SIGNATURE 0x524E4301       /* "RNC\001" */

typedef struct {
    unsigned long bitbuf;	       /* holds between 16 and 32 bits */
    int bitcount;		       /* how many bits does bitbuf hold? */
} bit_stream;

typedef struct {
    int num;			       /* number of nodes in the tree */
    struct {
	unsigned long code;
	int codelen;
	int value;
    } table[32];
} huf_table;

static long rnc_crc (void *data, long len);
static void read_huftable (huf_table *h, bit_stream *bs, unsigned char **p);
static unsigned long huf_read (huf_table *h, bit_stream *bs,unsigned char **p);

static void bitread_init (bit_stream *bs, unsigned char **p);
static void bitread_fix (bit_stream *bs, unsigned char **p);
static unsigned long bit_peek (bit_stream *bs, unsigned long mask);
static void bit_advance (bit_stream *bs, int n, unsigned char **p);
static unsigned long bit_read (bit_stream *bs, unsigned long mask,int n, unsigned char **p);

static unsigned long blong (unsigned char *p);
static unsigned long bword (unsigned char *p);
static unsigned long lword (unsigned char *p);

static unsigned long mirror (unsigned long x, int n);

s32 depackrnc1_ulen(void *packed)
{
    unsigned char *p = packed;
    if (blong (p) != RNC_SIGNATURE)
	return RNC_FILE_IS_NOT_RNC;
    return blong (p+4);
}

s32 depackrnc1(void *packed,void *unpacked)
{
    unsigned char *input = packed;
    unsigned char *output = unpacked;
    unsigned char *inputend, *outputend;
    bit_stream bs;
    huf_table raw, dist, len;
    unsigned long ch_count;
    unsigned long ret_len;
    unsigned out_crc;

    if (blong(input) != RNC_SIGNATURE)
	return RNC_FILE_IS_NOT_RNC;
    ret_len = blong (input+4);
    outputend = output + ret_len;
    inputend = input + 18 + blong(input+8);

    input += 18;		       /* skip header */

    /*
     * Check the packed-data CRC. Also save the unpacked-data CRC
     * for later.
     */
    if (rnc_crc(input, inputend-input) != bword(input-4))
	return RNC_PACKED_CRC_ERROR;
    out_crc = bword(input-6);

    bitread_init (&bs, &input);
    bit_advance (&bs, 2, &input);      /* discard first two bits */

    /*
     * Process chunks.
     */
    while (output < outputend) {
		read_huftable (&raw, &bs, &input);
		read_huftable (&dist, &bs, &input);
		read_huftable (&len, &bs, &input);
		ch_count = bit_read (&bs, 0xFFFF, 16, &input);

		while (1) {
			long length, posn;

			length = huf_read (&raw, &bs, &input);
			if (length == -1)
			return RNC_HUF_DECODE_ERROR;
			if (length) {
			while (length--)
				*output++ = *input++;
			bitread_fix (&bs, &input);
			}
			if (--ch_count <= 0)
			break;

			posn = huf_read (&dist, &bs, &input);
			if (posn == -1)
			return RNC_HUF_DECODE_ERROR;
			length = huf_read (&len, &bs, &input);
			if (length == -1)
			return RNC_HUF_DECODE_ERROR;
			posn += 1;
			length += 2;
			while (length--) {
			*output = output[-posn];
			output++;
			}
		}
    }
    if (outputend != output)
	return RNC_FILE_SIZE_MISMATCH;

    if (rnc_crc(outputend-ret_len, ret_len) != out_crc)
	return RNC_UNPACKED_CRC_ERROR;

    return ret_len;
}

/*
 * Read a Huffman table out of the bit stream and data stream given.
 */
static void read_huftable (huf_table *h, bit_stream *bs, unsigned char **p) {
    int i, j, k, num;
    int leaflen[32];
    int leafmax;
    unsigned long codeb;	       /* big-endian form of code */

    num = bit_read (bs, 0x1F, 5, p);
    if (!num)
	return;

    leafmax = 1;
    for (i=0; i<num; i++) {
	leaflen[i] = bit_read (bs, 0x0F, 4, p);
	if (leafmax < leaflen[i])
	    leafmax = leaflen[i];
    }

    codeb = 0L;
    k = 0;
    for (i=1; i<=leafmax; i++) {
	for (j=0; j<num; j++)
	    if (leaflen[j] == i) {
		h->table[k].code = mirror (codeb, i);
		h->table[k].codelen = i;
		h->table[k].value = j;
		codeb++;
		k++;
	    }
	codeb <<= 1;
    }

    h->num = k;
}

/*
 * Read a value out of the bit stream using the given Huffman table.
 */
static unsigned long huf_read (huf_table *h, bit_stream *bs,
			       unsigned char **p) {
    int i;
    unsigned long val;

    for (i=0; i<h->num; i++) {
	unsigned long mask = (1 << h->table[i].codelen) - 1;
	if (bit_peek(bs, mask) == h->table[i].code)
	    break;
    }
    if (i == h->num)
	return -1;
    bit_advance (bs, h->table[i].codelen, p);

    val = h->table[i].value;

    if (val >= 2) {
	val = 1 << (val-1);
	val |= bit_read (bs, val-1, h->table[i].value - 1, p);
    }
    return val;
}

/*
 * Initialises a bit stream with the first two bytes of the packed
 * data.
 */
static void bitread_init (bit_stream *bs, unsigned char **p) {
    bs->bitbuf = lword (*p);
    bs->bitcount = 16;
}

/*
 * Fixes up a bit stream after literals have been read out of the
 * data stream.
 */
static void bitread_fix (bit_stream *bs, unsigned char **p) {
    bs->bitcount -= 16;
    bs->bitbuf &= (1<<bs->bitcount)-1; /* remove the top 16 bits */
    bs->bitbuf |= (lword(*p)<<bs->bitcount);/* replace with what's at *p */
    bs->bitcount += 16;
}

/*
 * Returns some bits.
 */
static unsigned long bit_peek (bit_stream *bs, unsigned long mask) {
    return bs->bitbuf & mask;
}

/*
 * Advances the bit stream.
 */
static void bit_advance (bit_stream *bs, int n, unsigned char **p) {
    bs->bitbuf >>= n;
    bs->bitcount -= n;
    if (bs->bitcount < 16) {
	(*p) += 2;
	bs->bitbuf |= (lword(*p)<<bs->bitcount);
	bs->bitcount += 16;
    }
}

/*
 * Reads some bits in one go (ie the above two routines combined).
 */
static unsigned long bit_read (bit_stream *bs, unsigned long mask,
			       int n, unsigned char **p) {
    unsigned long result = bit_peek (bs, mask);
    bit_advance (bs, n, p);
    return result;
}

/*
 * Return the big-endian longword at p.
 */
static unsigned long blong (unsigned char *p) {
    unsigned long n;
    n = p[0];
    n = (n << 8) + p[1];
    n = (n << 8) + p[2];
    n = (n << 8) + p[3];
    return n;
}

/*
 * Return the big-endian word at p.
 */
static unsigned long bword (unsigned char *p) {
    unsigned long n;
    n = p[0];
    n = (n << 8) + p[1];
    return n;
}

/*
 * Return the little-endian word at p.
 */
static unsigned long lword (unsigned char *p) {
    unsigned long n;
    n = p[1];
    n = (n << 8) + p[0];
    return n;
}

/*
 * Mirror the bottom n bits of x.
 */
static unsigned long mirror (unsigned long x, int n) {
    unsigned long top = 1 << (n-1), bottom = 1;
    while (top > bottom) {
	unsigned long mask = top | bottom;
	unsigned long masked = x & mask;
	if (masked != 0 && masked != mask)
	    x ^= mask;
	top >>= 1;
	bottom <<= 1;
    }
    return x;
}

/*
 * Calculate a CRC, the RNC way. It re-computes its CRC table every
 * time it's run, but who cares? ;-)
 */
static long rnc_crc (void *data, long len) {
    unsigned short crctab[256];
    unsigned short val;
    int i, j;
    unsigned char *p = data;

    for (i=0; i<256; i++) {
	val = i;

	for (j=0; j<8; j++) {
	    if (val & 1)
		val = (val >> 1) ^ 0xA001;
	    else
		val = (val >> 1);
	}
	crctab[i] = val;
    }

    val = 0;
    while (len--) {
	val ^= *p++;
	val = (val >> 8) ^ crctab[val & 0xFF];
    }

    return val;
}
