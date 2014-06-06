/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * MD4 message digest
 ****************************************************************************/

#include <stdint.h>
#include <string.h>

/* Structure to save state of computation between the single steps.  */
struct md4_ctx
{
	uint32_t A;
	uint32_t B;
	uint32_t C;
	uint32_t D;

	uint32_t total[2];
	uint32_t buflen;
	uint32_t buffer[32];
};

# define MD4_DIGEST_SIZE 16

# define SWAP(n)							\
  (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))

#define BLOCKSIZE 4096
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

/* MD4 round constants */
#define K1 0x5a827999
#define K2 0x6ed9eba1

/* Round functions.  */
#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define rol(x, n) (((x) << (n)) | ((uint32_t) (x) >> (32 - (n))))
#define R1(a,b,c,d,k,s) a=rol(a+F(b,c,d)+x[k],s);
#define R2(a,b,c,d,k,s) a=rol(a+G(b,c,d)+x[k]+K1,s);
#define R3(a,b,c,d,k,s) a=rol(a+H(b,c,d)+x[k]+K2,s);

/* This array contains the bytes used to pad the buffer to the next
 64-byte boundary.  (RFC 1320, 3.1: Step 1)  */
static const unsigned char fillbuf[64] =
{ 0x80, 0 /* , 0, 0, ...  */};

/* Initialize structure containing state of computation.
 (RFC 1320, 3.3: Step 3)  */
static void md4_init_ctx(struct md4_ctx *ctx)
{
	ctx->A = 0x67452301;
	ctx->B = 0xefcdab89;
	ctx->C = 0x98badcfe;
	ctx->D = 0x10325476;

	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}

/* Copy the 4 byte value from v into the memory location pointed to by *cp,
 If your architecture allows unaligned access this is equivalent to
 * (uint32_t *) cp = v  */
static inline void set_uint32(char *cp, uint32_t v)
{
	memcpy(cp, &v, sizeof v);
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
 must be in little endian byte order.  */
static void *
md4_read_ctx(const struct md4_ctx *ctx, void *resbuf)
{
	char *r = resbuf;
	set_uint32(r + 0 * sizeof ctx->A, SWAP (ctx->A));
	set_uint32(r + 1 * sizeof ctx->B, SWAP (ctx->B));
	set_uint32(r + 2 * sizeof ctx->C, SWAP (ctx->C));
	set_uint32(r + 3 * sizeof ctx->D, SWAP (ctx->D));

	return resbuf;
}

/* Process LEN bytes of BUFFER, accumulating context into CTX.
 It is assumed that LEN % 64 == 0.  */

static void md4_process_block(const void *buffer, size_t len,
		struct md4_ctx *ctx)
{
	const uint32_t *words = buffer;
	size_t nwords = len / sizeof(uint32_t);
	const uint32_t *endp = words + nwords;
	uint32_t x[16];
	uint32_t A = ctx->A;
	uint32_t B = ctx->B;
	uint32_t C = ctx->C;
	uint32_t D = ctx->D;

	/* First increment the byte count.  RFC 1320 specifies the possible
	 length of the file up to 2^64 bits.  Here we only compute the
	 number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		++ctx->total[1];

	/* Process all bytes in the buffer with 64 bytes in each round of
	 the loop.  */
	while (words < endp)
	{
		int t;
		for (t = 0; t < 16; t++)
		{
			x[t] = SWAP (*words);
			words++;
		}

		/* Round 1.  */
		R1 (A, B, C, D, 0, 3);
		R1 (D, A, B, C, 1, 7);
		R1 (C, D, A, B, 2, 11);
		R1 (B, C, D, A, 3, 19);
		R1 (A, B, C, D, 4, 3);
		R1 (D, A, B, C, 5, 7);
		R1 (C, D, A, B, 6, 11);
		R1 (B, C, D, A, 7, 19);
		R1 (A, B, C, D, 8, 3);
		R1 (D, A, B, C, 9, 7);
		R1 (C, D, A, B, 10, 11);
		R1 (B, C, D, A, 11, 19);
		R1 (A, B, C, D, 12, 3);
		R1 (D, A, B, C, 13, 7);
		R1 (C, D, A, B, 14, 11);
		R1 (B, C, D, A, 15, 19);

		/* Round 2.  */
		R2 (A, B, C, D, 0, 3);
		R2 (D, A, B, C, 4, 5);
		R2 (C, D, A, B, 8, 9);
		R2 (B, C, D, A, 12, 13);
		R2 (A, B, C, D, 1, 3);
		R2 (D, A, B, C, 5, 5);
		R2 (C, D, A, B, 9, 9);
		R2 (B, C, D, A, 13, 13);
		R2 (A, B, C, D, 2, 3);
		R2 (D, A, B, C, 6, 5);
		R2 (C, D, A, B, 10, 9);
		R2 (B, C, D, A, 14, 13);
		R2 (A, B, C, D, 3, 3);
		R2 (D, A, B, C, 7, 5);
		R2 (C, D, A, B, 11, 9);
		R2 (B, C, D, A, 15, 13);

		/* Round 3.  */
		R3 (A, B, C, D, 0, 3);
		R3 (D, A, B, C, 8, 9);
		R3 (C, D, A, B, 4, 11);
		R3 (B, C, D, A, 12, 15);
		R3 (A, B, C, D, 2, 3);
		R3 (D, A, B, C, 10, 9);
		R3 (C, D, A, B, 6, 11);
		R3 (B, C, D, A, 14, 15);
		R3 (A, B, C, D, 1, 3);
		R3 (D, A, B, C, 9, 9);
		R3 (C, D, A, B, 5, 11);
		R3 (B, C, D, A, 13, 15);
		R3 (A, B, C, D, 3, 3);
		R3 (D, A, B, C, 11, 9);
		R3 (C, D, A, B, 7, 11);
		R3 (B, C, D, A, 15, 15);

		A = ctx->A += A;
		B = ctx->B += B;
		C = ctx->C += C;
		D = ctx->D += D;
	}
}

/* Process the remaining bytes in the internal buffer and the usual
 prolog according to the standard and write the result to RESBUF.  */
static void *
md4_finish_ctx(struct md4_ctx *ctx, void *resbuf)
{
	/* Take yet unprocessed bytes into account.  */
	uint32_t bytes = ctx->buflen;
	size_t pad;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		++ctx->total[1];

	pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
	memcpy(&((char*) ctx->buffer)[bytes], fillbuf, pad);

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	ctx->buffer[(bytes + pad) / 4] = SWAP (ctx->total[0] << 3);
	ctx->buffer[(bytes + pad) / 4 + 1] = SWAP ((ctx->total[1] << 3) |
			(ctx->total[0] >> 29));

	/* Process last bytes.  */
	md4_process_block(ctx->buffer, bytes + pad + 8, ctx);

	return md4_read_ctx(ctx, resbuf);
}

static void md4_process_bytes(const void *buffer, size_t len,
		struct md4_ctx *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	 both inputs first.  */
	if (ctx->buflen != 0)
	{
		size_t left_over = ctx->buflen;
		size_t add = 128 - left_over > len ? len : 128 - left_over;

		memcpy(&((char*) ctx->buffer)[left_over], buffer, add);
		ctx->buflen += add;

		if (ctx->buflen > 64)
		{
			md4_process_block(ctx->buffer, ctx->buflen & ~63, ctx);

			ctx->buflen &= 63;
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer,
					&((char*) ctx->buffer)[(left_over + add) & ~63],
					ctx->buflen);
		}

		buffer = (const char *) buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 64)
	{
#if !_STRING_ARCH_unaligned
		/* To check alignment gcc has an appropriate operator.  Other
		 compilers don't.  */
# if __GNUC__ >= 2
#  define UNALIGNED_P(p) (((uintptr_t) p) % __alignof__ (uint32_t) != 0)
# else
#  define alignof(type) offsetof (struct { char c; type x; }, x)
#  define UNALIGNED_P(p) (((size_t) p) % alignof (uint32_t) != 0)
# endif
		if (
				UNALIGNED_P (buffer))
				while (len> 64)
				{
					md4_process_block (memcpy (ctx->buffer, buffer, 64), 64, ctx);
					buffer = (const char *) buffer + 64;
					len -= 64;
				}
				else
#endif
				{
					md4_process_block (buffer, len & ~63, ctx);
					buffer = (const char *) buffer + (len & ~63);
					len &= 63;
				}
			}

	/* Move remaining bytes in internal buffer.  */
	if (len > 0)
	{
		size_t left_over = ctx->buflen;

		memcpy(&((char*) ctx->buffer)[left_over], buffer, len);
		left_over += len;
		if (left_over >= 64)
		{
			md4_process_block(ctx->buffer, 64, ctx);
			left_over -= 64;
			memcpy(ctx->buffer, &ctx->buffer[16], left_over);
		}
		ctx->buflen = left_over;
	}
}

/* Compute MD4 message digest for LEN bytes beginning at BUFFER.  The
 result is always in little endian byte order, so that a byte-wise
 output yields to the wanted ASCII representation of the message
 digest.  */
void *
md4_buffer(const char *buffer, size_t len, void *resblock)
{
	struct md4_ctx ctx;

	/* Initialize the computation context.  */
	md4_init_ctx(&ctx);

	/* Process whole buffer but last len % 64 bytes.  */
	md4_process_bytes(buffer, len, &ctx);

	/* Put result in desired memory area.  */
	return md4_finish_ctx(&ctx, resblock);
}
