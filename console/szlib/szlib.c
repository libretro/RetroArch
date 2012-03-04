/* infcodes.c -- process literals and length/distance pairs
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

#include "szlib.h"

const char inflate_copyright[] =
   " inflate 1.1.4 Copyright 1995-2002 Mark Adler ";

/* And'ing with mask[n] masks the lower n bits */
uInt inflate_mask[17] = 
{
	0x0000,
	0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
	0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

typedef enum {
      METHOD_INFLATE,   /* waiting for method byte */
      FLAG_INFLATE,     /* waiting for flag byte */
      DICT4_INFLATE,    /* four dictionary check bytes to go */
      DICT3_INFLATE,    /* three dictionary check bytes to go */
      DICT2_INFLATE,    /* two dictionary check bytes to go */
      DICT1_INFLATE,    /* one dictionary check byte to go */
      DICT0_INFLATE,    /* waiting for inflateSetDictionary */
      BLOCKS_INFLATE,   /* decompressing blocks */
      CHECK4_INFLATE,   /* four check bytes to go */
      CHECK3_INFLATE,   /* three check bytes to go */
      CHECK2_INFLATE,   /* two check bytes to go */
      CHECK1_INFLATE,   /* one check byte to go */
      DONE_INFLATE,     /* finished check, done */
      BAD_INFLATE}      /* got an error--stay here */
inflate_mode;

/* inflate private state */
struct internal_state
{
	/* mode */
	inflate_mode  mode;   /* current inflate mode */

	/* mode dependent information */
	union {
		uInt method;        /* if FLAGS, method byte */
		struct {
			uLong was;                /* computed check value */
			uLong need;               /* stream check value */
		} check;            /* if CHECK, check values to compare */
		uInt marker;        /* if BAD, inflateSync's marker bytes count */
	} sub;        /* submode */

	/* mode independent information */
	int  nowrap;          /* flag for no wrapper */
	uInt wbits;           /* log2(window size)  (8..15, defaults to 15) */
	inflate_blocks_statef *blocks;            /* current inflate_blocks state */
};

static const uInt border[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};


typedef enum {        /* waiting for "i:"=input, "o:"=output, "x:"=nothing */
      START,    /* x: set up for LEN */
      LEN,      /* i: get length/literal/eob next */
      LENEXT,   /* i: getting length extra (have base) */
      DIST,     /* i: get distance next */
      DISTEXT,  /* i: getting distance extra */
      COPY,     /* o: copying bytes in window, waiting for space */
      LIT,      /* o: got literal, waiting for output space */
      WASH,     /* o: got eob, possibly still output waiting */
      END,      /* x: got eob and all data flushed */
      BADCODE}  /* x: got error */
inflate_codes_mode;

static int huft_build OF((
    uIntf *,            /* code lengths in bits */
    uInt,               /* number of codes */
    uInt,               /* number of "simple" codes */
    const uIntf *,      /* list of base values for non-simple codes */
    const uIntf *,      /* list of extra bits for non-simple codes */
    inflate_huft * FAR*,/* result: starting table */
    uIntf *,            /* maximum lookup bits (returns actual) */
    inflate_huft *,     /* space for trees */
    uInt *,             /* hufts used in space */
    uIntf * ));         /* space for values */

/* Tables for deflate from PKZIP's appnote.txt. */
static const uInt cplens[31] = { /* Copy lengths for literal codes 257..285 */
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
        /* see note #13 above about 258 */
static const uInt cplext[31] = { /* Extra bits for literal codes 257..285 */
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 112, 112}; /* 112==invalid */
static const uInt cpdist[30] = { /* Copy offsets for distance codes 0..29 */
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
static const uInt cpdext[30] = { /* Extra bits for distance codes */
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5552

#define DO1_ADLER(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2_ADLER(buf,i)  DO1_ADLER(buf,i); DO1_ADLER(buf,i+1);
#define DO4_ADLER(buf,i)  DO2_ADLER(buf,i); DO2_ADLER(buf,i+2);
#define DO8_ADLER(buf,i)  DO4_ADLER(buf,i); DO4_ADLER(buf,i+4);
#define DO16_ADLER(buf)   DO8_ADLER(buf,0); DO8_ADLER(buf,8);

/* If BMAX needs to be larger than 16, then h and x[] should be uLong. */
#define BMAX 15         /* maximum bit length of any code */

/* inflate codes private state */
struct inflate_codes_state
{
	/* mode */
	inflate_codes_mode mode;      /* current inflate_codes mode */

	/* mode dependent information */
	uInt len;
	union {
		struct {
			inflate_huft *tree;       /* pointer into tree */
			uInt need;                /* bits needed */
		} code;             /* if LEN or DIST, where in tree */
		uInt lit;           /* if LIT, literal */
		struct {
			uInt get;                 /* bits to get for extra */
			uInt dist;                /* distance back to copy from */
		} copy;             /* if EXT or COPY, where and how much */
	} sub;                /* submode */

	/* mode independent information */
	Byte lbits;           /* ltree bits decoded per branch */
	Byte dbits;           /* dtree bits decoder per branch */
	inflate_huft *ltree;          /* literal/length/eob tree */
	inflate_huft *dtree;          /* distance tree */
};


inflate_codes_statef *inflate_codes_new(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td, z_streamp z)
{
	inflate_codes_statef *c;

	if ((c = (inflate_codes_statef *)
				ZALLOC(z,1,sizeof(struct inflate_codes_state))) != 0)
	{
		c->mode = START;
		c->lbits = (Byte)bl;
		c->dbits = (Byte)bd;
		c->ltree = tl;
		c->dtree = td;
	}
	return c;
}

/* inffast.c -- process literals and length/distance pairs fast
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

static int inflate_fast(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td, inflate_blocks_statef *s, z_streamp z)
{
	inflate_huft *t;      /* temporary pointer */
	uInt e;               /* extra bits or operation */
	uLong b;              /* bit buffer */
	uInt k;               /* bits in bit buffer */
	Bytef *p;             /* input data pointer */
	uInt n;               /* bytes available there */
	Bytef *q;             /* output window write pointer */
	uInt m;               /* bytes to end of window or read pointer */
	uInt ml;              /* mask for literal/length tree */
	uInt md;              /* mask for distance tree */
	uInt c;               /* bytes to copy */
	uInt d;               /* distance back to copy from */
	Bytef *r;             /* copy source pointer */

	/* load input, output, bit values */
	LOAD
	
	/* initialize masks */
	ml = inflate_mask[bl];
	md = inflate_mask[bd];

	/* do until not enough input or output space for fast loop */
	do
	{                          /* assume called with m >= 258 && n >= 10 */
		/* get literal/length code */
		while(k<(20)){b|=((uLong)NEXTBYTE)<<k;k+=8;} /* max bits for literal/length code */
		if ((e = (t = tl + ((uInt)b & ml))->word.what.Exop) == 0)
		{
			DUMPBITS(t->word.what.Bits)
				*q++ = (Byte)t->base;
			m--;
			continue;
		}
		do
		{
			DUMPBITS(t->word.what.Bits)
				if (e & 16)
				{
					/* get extra bits for length */
					e &= 15;
					c = t->base + ((uInt)b & inflate_mask[e]);
					DUMPBITS(e)

						/* decode distance base of block to copy */
						while(k<(15)){b|=((uLong)NEXTBYTE)<<k;k+=8;} /* max bits for distance code */
					e = (t = td + ((uInt)b & md))->word.what.Exop;
					do
					{
						DUMPBITS(t->word.what.Bits)
							if (e & 16)
							{
								/* get extra bits to add to distance base */
								e &= 15;
								while(k<(e)){b|=((uLong)NEXTBYTE)<<k;k+=8;} /* get extra bits (up to 13) */
								d = t->base + ((uInt)b & inflate_mask[e]);
								DUMPBITS(e)

									/* do the copy */
									m -= c;
								r = q - d;
								if (r < s->window)                  /* wrap if needed */
								{
									do {
										r += s->end - s->window;        /* force pointer in window */
									} while (r < s->window);          /* covers invalid distances */
									e = s->end - r;
									if (c > e)
									{
										c -= e;                         /* wrapped copy */
										do {
											*q++ = *r++;
										} while (--e);
										r = s->window;
										do {
											*q++ = *r++;
										} while (--c);
									}
									else                              /* normal copy */
									{
										*q++ = *r++;  c--;
										*q++ = *r++;  c--;
										do {
											*q++ = *r++;
										} while (--c);
									}
								}
								else                                /* normal copy */
								{
									*q++ = *r++;  c--;
									*q++ = *r++;  c--;
									do {
										*q++ = *r++;
									} while (--c);
								}
								break;
							}
							else if ((e & 64) == 0)
							{
								t += t->base;
								e = (t += ((uInt)b & inflate_mask[e]))->word.what.Exop;
							}
							else
							{
								z->msg = (char*)"invalid distance code";
								c = z->avail_in-n;
								c = (k >> 3) < c ? k >> 3 : c;
								n+=c;
								p-=c;
								k-=c<<3;
								UPDATE
									return Z_DATA_ERROR;
							}
					}while (1);
					break;
				}
			if ((e & 64) == 0)
			{
				t += t->base;
				if ((e = (t += ((uInt)b & inflate_mask[e]))->word.what.Exop) == 0)
				{
					DUMPBITS(t->word.what.Bits)
						*q++ = (Byte)t->base;
					m--;
					break;
				}
			}
			else if (e & 32)
			{
				c = z->avail_in-n;
				c = (k >> 3) < c ? k >> 3 : c;
				n+=c;
				p-=c;
				k-=c<<3;
				UPDATE
					return Z_STREAM_END;
			}
			else
			{
				z->msg = (char*)"invalid literal/length code";
				c = z->avail_in-n;
				c = (k >> 3) < c ? k >> 3 : c;
				n+=c;
				p-=c;
				k-=c<<3;
				UPDATE
					return Z_DATA_ERROR;
			}
		}while (1);
	} while (m >= 258 && n >= 10);

	/* not enough input or output--restore pointers and return */
	c = z->avail_in-n;
	c = (k >> 3) < c ? k >> 3 : c;
	n+=c;
	p-=c;
	k-=c<<3;
	UPDATE
	return Z_OK;
}

int inflate_codes(inflate_blocks_statef *s, z_streamp z, int r)
{
	uInt j;               /* temporary storage */
	inflate_huft *t;      /* temporary pointer */
	uInt e;               /* extra bits or operation */
	uLong b;              /* bit buffer */
	uInt k;               /* bits in bit buffer */
	Bytef *p;             /* input data pointer */
	uInt n;               /* bytes available there */
	Bytef *q;             /* output window write pointer */
	uInt m;               /* bytes to end of window or read pointer */
	Bytef *f;             /* pointer to copy strings from */
	inflate_codes_statef *c = s->sub.decode.codes;  /* codes state */

	/* copy input/output information to locals (UPDATE macro restores) */
	LOAD

		/* process input and output based on current state */
		while (1) switch (c->mode)
		{             /* waiting for "i:"=input, "o:"=output, "x:"=nothing */
			case START:         /* x: set up for LEN */
				if (m >= 258 && n >= 10)
				{
					UPDATE
						r = inflate_fast(c->lbits, c->dbits, c->ltree, c->dtree, s, z);
					LOAD
						if (r != Z_OK)
						{
							c->mode = r == Z_STREAM_END ? WASH : BADCODE;
							break;
						}
				}
				c->sub.code.need = c->lbits;
				c->sub.code.tree = c->ltree;
				c->mode = LEN;
			case LEN:           /* i: get length/literal/eob next */
				j = c->sub.code.need;
				NEEDBITS(j)
					t = c->sub.code.tree + ((uInt)b & inflate_mask[j]);
				DUMPBITS(t->word.what.Bits)
					e = (uInt)(t->word.what.Exop);
				if (e == 0)               /* literal */
				{
					c->sub.lit = t->base;
					c->mode = LIT;
					break;
				}
				if (e & 16)               /* length */
				{
					c->sub.copy.get = e & 15;
					c->len = t->base;
					c->mode = LENEXT;
					break;
				}
				if ((e & 64) == 0)        /* next table */
				{
					c->sub.code.need = e;
					c->sub.code.tree = t + t->base;
					break;
				}
				if (e & 32)               /* end of block */
				{
					c->mode = WASH;
					break;
				}
				c->mode = BADCODE;        /* invalid code */
				z->msg = (char*)"invalid literal/length code";
				r = Z_DATA_ERROR;
				LEAVE
			case LENEXT:        /* i: getting length extra (have base) */
					j = c->sub.copy.get;
					NEEDBITS(j)
						c->len += (uInt)b & inflate_mask[j];
					DUMPBITS(j)
						c->sub.code.need = c->dbits;
					c->sub.code.tree = c->dtree;
					c->mode = DIST;
			case DIST:          /* i: get distance next */
					j = c->sub.code.need;
					NEEDBITS(j)
						t = c->sub.code.tree + ((uInt)b & inflate_mask[j]);
					DUMPBITS(t->word.what.Bits)
						e = (uInt)(t->word.what.Exop);
					if (e & 16)               /* distance */
					{
						c->sub.copy.get = e & 15;
						c->sub.copy.dist = t->base;
						c->mode = DISTEXT;
						break;
					}
					if ((e & 64) == 0)        /* next table */
					{
						c->sub.code.need = e;
						c->sub.code.tree = t + t->base;
						break;
					}
					c->mode = BADCODE;        /* invalid code */
					z->msg = (char*)"invalid distance code";
					r = Z_DATA_ERROR;
					LEAVE
			case DISTEXT:       /* i: getting distance extra */
						j = c->sub.copy.get;
						NEEDBITS(j)
							c->sub.copy.dist += (uInt)b & inflate_mask[j];
						DUMPBITS(j)
							c->mode = COPY;
			case COPY:          /* o: copying bytes in window, waiting for space */
						f = q - c->sub.copy.dist;
						while (f < s->window)             /* modulo window size-"while" instead */
							f += s->end - s->window;        /* of "if" handles invalid distances */
						while (c->len)
						{
							NEEDOUT
								OUTBYTE(*f++)
								if (f == s->end)
									f = s->window;
							c->len--;
						}
						c->mode = START;
						break;
			case LIT:           /* o: got literal, waiting for output space */
						NEEDOUT
							OUTBYTE(c->sub.lit)
							c->mode = START;
						break;
			case WASH:          /* o: got eob, possibly more output */
						if (k > 7)        /* return unused byte, if any */
						{
							k -= 8;
							n++;
							p--;            /* can always return one */
						}
						FLUSH
							if (s->read != s->write)
								LEAVE
									c->mode = END;
			case END:
						r = Z_STREAM_END;
						LEAVE
			case BADCODE:       /* x: got error */
							r = Z_DATA_ERROR;
							LEAVE
			default:
								r = Z_STREAM_ERROR;
								LEAVE
		}
}

/* inftrees.c -- generate Huffman trees for efficient decoding
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

static int huft_build(uIntf *b, uInt n, uInt s, const uIntf *d, const uIntf *e, inflate_huft * FAR *t, uIntf *m, inflate_huft *hp, uInt *hn, uIntf *v)
{
	uInt a;                       /* counter for codes of length k */
	uInt c[BMAX+1];               /* bit length count table */
	uInt f;                       /* i repeats in table every f entries */
	int g;                        /* maximum code length */
	int h;                        /* table level */
	register uInt i;              /* counter, current code */
	register uInt j;              /* counter */
	register int k;               /* number of bits in current code */
	int l;                        /* bits per table (returned in m) */
	uInt mask;                    /* (1 << w) - 1, to avoid cc -O bug on HP */
	register uIntf *p;            /* pointer into c[], b[], or v[] */
	inflate_huft *q;              /* points to current table */
	struct inflate_huft_s r;      /* table entry for structure assignment */
	inflate_huft *u[BMAX];        /* table stack */
	register int w;               /* bits before this table == (l * h) */
	uInt x[BMAX+1];               /* bit offsets, then code stack */
	uIntf *xp;                    /* pointer into x */
	int y;                        /* number of dummy codes added */
	uInt z;                       /* number of entries in current table */


	/* Generate counts for each bit length */
	p = c;
#define C0_INFTREES *p++ = 0;
#define C2_INFTREES C0_INFTREES C0_INFTREES C0_INFTREES C0_INFTREES
#define C4_INFTREES C2_INFTREES C2_INFTREES C2_INFTREES C2_INFTREES
	C4_INFTREES                            /* clear c[]--assume BMAX+1 is 16 */
		p = b;  i = n;
	do {
		c[*p++]++;                  /* assume all entries <= BMAX */
	} while (--i);
	if (c[0] == n)                /* null input--all zero length codes */
	{
		*t = (inflate_huft *)0;
		*m = 0;
		return Z_OK;
	}


	/* Find minimum and maximum length, bound *m by those */
	l = *m;
	for (j = 1; j <= BMAX; j++)
		if (c[j])
			break;
	k = j;                        /* minimum code length */
	if ((uInt)l < j)
		l = j;
	for (i = BMAX; i; i--)
		if (c[i])
			break;
	g = i;                        /* maximum code length */
	if ((uInt)l > i)
		l = i;
	*m = l;


	/* Adjust last length count to fill out codes, if needed */
	for (y = 1 << j; j < i; j++, y <<= 1)
		if ((y -= c[j]) < 0)
			return Z_DATA_ERROR;
	if ((y -= c[i]) < 0)
		return Z_DATA_ERROR;
	c[i] += y;


	/* Generate starting offsets into the value table for each length */
	x[1] = j = 0;
	p = c + 1;  xp = x + 2;
	while (--i) {                 /* note that i == g from above */
		*xp++ = (j += *p++);
	}


	/* Make a table of values in order of bit lengths */
	p = b;  i = 0;
	do {
		if ((j = *p++) != 0)
			v[x[j]++] = i;
	} while (++i < n);
	n = x[g];                     /* set n to length of v */


	/* Generate the Huffman codes and for each, make the table entries */
	x[0] = i = 0;                 /* first Huffman code is zero */
	p = v;                        /* grab values in bit order */
	h = -1;                       /* no tables yet--level -1 */
	w = -l;                       /* bits decoded == (l * h) */
	u[0] = (inflate_huft *)0;        /* just to keep compilers happy */
	q = (inflate_huft *)0;   /* ditto */
	z = 0;                        /* ditto */

	/* go through the bit lengths (k already is bits in shortest code) */
	for (; k <= g; k++)
	{
		a = c[k];
		while (a--)
		{
			/* here i is the Huffman code of length k bits for value *p */
			/* make tables up to required level */
			while (k > w + l)
			{
				h++;
				w += l;                 /* previous table always l bits */

				/* compute minimum size table less than or equal to l bits */
				z = g - w;
				z = z > (uInt)l ? l : z;        /* table size upper limit */
				if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
				{                       /* too few codes for k-w bit table */
					f -= a + 1;           /* deduct codes from patterns left */
					xp = c + k;
					if (j < z)
						while (++j < z)     /* try smaller tables up to z bits */
						{
							if ((f <<= 1) <= *++xp)
								break;          /* enough codes to use up j bits */
							f -= *xp;         /* else deduct codes from patterns */
						}
				}
				z = 1 << j;             /* table entries for j-bit table */

				/* allocate new table */
				if (*hn + z > MANY)     /* (note: doesn't matter for fixed) */
					return Z_DATA_ERROR;  /* overflow of MANY */
				u[h] = q = hp + *hn;
				*hn += z;

				/* connect to last table, if there is one */
				if (h)
				{
					x[h] = i;             /* save pattern for backing up */
					r.word.what.Bits = (Byte)l;     /* bits to dump before this table */
					r.word.what.Exop = (Byte)j;     /* bits in this table */
					j = i >> (w - l);
					r.base = (uInt)(q - u[h-1] - j);   /* offset to this table */
					u[h-1][j] = r;        /* connect to last table */
				}
				else
					*t = q;               /* first table is returned result */
			}

			/* set up table entry in r */
			r.word.what.Bits = (Byte)(k - w);
			if (p >= v + n)
				r.word.what.Exop = 128 + 64;      /* out of values--invalid code */
			else if (*p < s)
			{
				r.word.what.Exop = (Byte)(*p < 256 ? 0 : 32 + 64);     /* 256 is end-of-block */
				r.base = *p++;          /* simple code is just the value */
			}
			else
			{
				r.word.what.Exop = (Byte)(e[*p - s] + 16 + 64);/* non-simple--look up in lists */
				r.base = d[*p++ - s];
			}

			/* fill code-like entries with r */
			f = 1 << (k - w);
			for (j = i >> w; j < z; j += f)
				q[j] = r;

			/* backwards increment the k-bit code i */
			for (j = 1 << (k - 1); i & j; j >>= 1)
				i ^= j;
			i ^= j;

			/* backup over finished tables */
			mask = (1 << w) - 1;      /* needed on HP, cc -O bug */
			while ((i & mask) != x[h])
			{
				h--;                    /* don't need to update q */
				w -= l;
				mask = (1 << w) - 1;
			}
		}
	}


	/* Return Z_BUF_ERROR if we were given an incomplete table */
	return y != 0 && g != 1 ? Z_BUF_ERROR : Z_OK;
}


int inflate_trees_bits(uIntf *c, uIntf *bb, inflate_huft * FAR *tb, inflate_huft *hp, z_streamp z)
{
	int r;
	uInt hn = 0;          /* hufts used in space */
	uIntf *v;             /* work area for huft_build */

	if ((v = (uIntf*)ZALLOC(z, 19, sizeof(uInt))) == 0)
		return Z_MEM_ERROR;
	r = huft_build(c, 19, 19, (uIntf*)0, (uIntf*)0,
			tb, bb, hp, &hn, v);
	if (r == Z_DATA_ERROR)
		z->msg = (char*)"oversubscribed dynamic bit lengths tree";
	else if (r == Z_BUF_ERROR || *bb == 0)
	{
		z->msg = (char*)"incomplete dynamic bit lengths tree";
		r = Z_DATA_ERROR;
	}
	ZFREE(z, v);
	return r;
}


int inflate_trees_dynamic(uInt nl, uInt nd, uIntf *c, uIntf *bl, uIntf *bd, inflate_huft * FAR *tl, inflate_huft * FAR *td, inflate_huft *hp, z_streamp z)
{
	int r;
	uInt hn = 0;          /* hufts used in space */
	uIntf *v;             /* work area for huft_build */

	/* allocate work area */
	if ((v = (uIntf*)ZALLOC(z, 288, sizeof(uInt))) == 0)
		return Z_MEM_ERROR;

	/* build literal/length tree */
	r = huft_build(c, nl, 257, cplens, cplext, tl, bl, hp, &hn, v);
	if (r != Z_OK || *bl == 0)
	{
		if (r == Z_DATA_ERROR)
			z->msg = (char*)"oversubscribed literal/length tree";
		else if (r != Z_MEM_ERROR)
		{
			z->msg = (char*)"incomplete literal/length tree";
			r = Z_DATA_ERROR;
		}
		ZFREE(z, v);
		return r;
	}

	/* build distance tree */
	r = huft_build(c + nl, nd, 0, cpdist, cpdext, td, bd, hp, &hn, v);
	if (r != Z_OK || (*bd == 0 && nl > 257))
	{
		if (r == Z_DATA_ERROR)
			z->msg = (char*)"oversubscribed distance tree";
		else if (r == Z_BUF_ERROR) {
			z->msg = (char*)"incomplete distance tree";
			r = Z_DATA_ERROR;
		}
		else if (r != Z_MEM_ERROR)
		{
			z->msg = (char*)"empty distance tree with lengths";
			r = Z_DATA_ERROR;
		}
		ZFREE(z, v);
		return r;
	}

	/* done */
	ZFREE(z, v);
	return Z_OK;
}

static uInt fixed_bl = 9;
static uInt fixed_bd = 5;
static inflate_huft fixed_tl[] = {
    {{{96,7}},256}, {{{0,8}},80}, {{{0,8}},16}, {{{84,8}},115},
    {{{82,7}},31}, {{{0,8}},112}, {{{0,8}},48}, {{{0,9}},192},
    {{{80,7}},10}, {{{0,8}},96}, {{{0,8}},32}, {{{0,9}},160},
    {{{0,8}},0}, {{{0,8}},128}, {{{0,8}},64}, {{{0,9}},224},
    {{{80,7}},6}, {{{0,8}},88}, {{{0,8}},24}, {{{0,9}},144},
    {{{83,7}},59}, {{{0,8}},120}, {{{0,8}},56}, {{{0,9}},208},
    {{{81,7}},17}, {{{0,8}},104}, {{{0,8}},40}, {{{0,9}},176},
    {{{0,8}},8}, {{{0,8}},136}, {{{0,8}},72}, {{{0,9}},240},
    {{{80,7}},4}, {{{0,8}},84}, {{{0,8}},20}, {{{85,8}},227},
    {{{83,7}},43}, {{{0,8}},116}, {{{0,8}},52}, {{{0,9}},200},
    {{{81,7}},13}, {{{0,8}},100}, {{{0,8}},36}, {{{0,9}},168},
    {{{0,8}},4}, {{{0,8}},132}, {{{0,8}},68}, {{{0,9}},232},
    {{{80,7}},8}, {{{0,8}},92}, {{{0,8}},28}, {{{0,9}},152},
    {{{84,7}},83}, {{{0,8}},124}, {{{0,8}},60}, {{{0,9}},216},
    {{{82,7}},23}, {{{0,8}},108}, {{{0,8}},44}, {{{0,9}},184},
    {{{0,8}},12}, {{{0,8}},140}, {{{0,8}},76}, {{{0,9}},248},
    {{{80,7}},3}, {{{0,8}},82}, {{{0,8}},18}, {{{85,8}},163},
    {{{83,7}},35}, {{{0,8}},114}, {{{0,8}},50}, {{{0,9}},196},
    {{{81,7}},11}, {{{0,8}},98}, {{{0,8}},34}, {{{0,9}},164},
    {{{0,8}},2}, {{{0,8}},130}, {{{0,8}},66}, {{{0,9}},228},
    {{{80,7}},7}, {{{0,8}},90}, {{{0,8}},26}, {{{0,9}},148},
    {{{84,7}},67}, {{{0,8}},122}, {{{0,8}},58}, {{{0,9}},212},
    {{{82,7}},19}, {{{0,8}},106}, {{{0,8}},42}, {{{0,9}},180},
    {{{0,8}},10}, {{{0,8}},138}, {{{0,8}},74}, {{{0,9}},244},
    {{{80,7}},5}, {{{0,8}},86}, {{{0,8}},22}, {{{192,8}},0},
    {{{83,7}},51}, {{{0,8}},118}, {{{0,8}},54}, {{{0,9}},204},
    {{{81,7}},15}, {{{0,8}},102}, {{{0,8}},38}, {{{0,9}},172},
    {{{0,8}},6}, {{{0,8}},134}, {{{0,8}},70}, {{{0,9}},236},
    {{{80,7}},9}, {{{0,8}},94}, {{{0,8}},30}, {{{0,9}},156},
    {{{84,7}},99}, {{{0,8}},126}, {{{0,8}},62}, {{{0,9}},220},
    {{{82,7}},27}, {{{0,8}},110}, {{{0,8}},46}, {{{0,9}},188},
    {{{0,8}},14}, {{{0,8}},142}, {{{0,8}},78}, {{{0,9}},252},
    {{{96,7}},256}, {{{0,8}},81}, {{{0,8}},17}, {{{85,8}},131},
    {{{82,7}},31}, {{{0,8}},113}, {{{0,8}},49}, {{{0,9}},194},
    {{{80,7}},10}, {{{0,8}},97}, {{{0,8}},33}, {{{0,9}},162},
    {{{0,8}},1}, {{{0,8}},129}, {{{0,8}},65}, {{{0,9}},226},
    {{{80,7}},6}, {{{0,8}},89}, {{{0,8}},25}, {{{0,9}},146},
    {{{83,7}},59}, {{{0,8}},121}, {{{0,8}},57}, {{{0,9}},210},
    {{{81,7}},17}, {{{0,8}},105}, {{{0,8}},41}, {{{0,9}},178},
    {{{0,8}},9}, {{{0,8}},137}, {{{0,8}},73}, {{{0,9}},242},
    {{{80,7}},4}, {{{0,8}},85}, {{{0,8}},21}, {{{80,8}},258},
    {{{83,7}},43}, {{{0,8}},117}, {{{0,8}},53}, {{{0,9}},202},
    {{{81,7}},13}, {{{0,8}},101}, {{{0,8}},37}, {{{0,9}},170},
    {{{0,8}},5}, {{{0,8}},133}, {{{0,8}},69}, {{{0,9}},234},
    {{{80,7}},8}, {{{0,8}},93}, {{{0,8}},29}, {{{0,9}},154},
    {{{84,7}},83}, {{{0,8}},125}, {{{0,8}},61}, {{{0,9}},218},
    {{{82,7}},23}, {{{0,8}},109}, {{{0,8}},45}, {{{0,9}},186},
    {{{0,8}},13}, {{{0,8}},141}, {{{0,8}},77}, {{{0,9}},250},
    {{{80,7}},3}, {{{0,8}},83}, {{{0,8}},19}, {{{85,8}},195},
    {{{83,7}},35}, {{{0,8}},115}, {{{0,8}},51}, {{{0,9}},198},
    {{{81,7}},11}, {{{0,8}},99}, {{{0,8}},35}, {{{0,9}},166},
    {{{0,8}},3}, {{{0,8}},131}, {{{0,8}},67}, {{{0,9}},230},
    {{{80,7}},7}, {{{0,8}},91}, {{{0,8}},27}, {{{0,9}},150},
    {{{84,7}},67}, {{{0,8}},123}, {{{0,8}},59}, {{{0,9}},214},
    {{{82,7}},19}, {{{0,8}},107}, {{{0,8}},43}, {{{0,9}},182},
    {{{0,8}},11}, {{{0,8}},139}, {{{0,8}},75}, {{{0,9}},246},
    {{{80,7}},5}, {{{0,8}},87}, {{{0,8}},23}, {{{192,8}},0},
    {{{83,7}},51}, {{{0,8}},119}, {{{0,8}},55}, {{{0,9}},206},
    {{{81,7}},15}, {{{0,8}},103}, {{{0,8}},39}, {{{0,9}},174},
    {{{0,8}},7}, {{{0,8}},135}, {{{0,8}},71}, {{{0,9}},238},
    {{{80,7}},9}, {{{0,8}},95}, {{{0,8}},31}, {{{0,9}},158},
    {{{84,7}},99}, {{{0,8}},127}, {{{0,8}},63}, {{{0,9}},222},
    {{{82,7}},27}, {{{0,8}},111}, {{{0,8}},47}, {{{0,9}},190},
    {{{0,8}},15}, {{{0,8}},143}, {{{0,8}},79}, {{{0,9}},254},
    {{{96,7}},256}, {{{0,8}},80}, {{{0,8}},16}, {{{84,8}},115},
    {{{82,7}},31}, {{{0,8}},112}, {{{0,8}},48}, {{{0,9}},193},
    {{{80,7}},10}, {{{0,8}},96}, {{{0,8}},32}, {{{0,9}},161},
    {{{0,8}},0}, {{{0,8}},128}, {{{0,8}},64}, {{{0,9}},225},
    {{{80,7}},6}, {{{0,8}},88}, {{{0,8}},24}, {{{0,9}},145},
    {{{83,7}},59}, {{{0,8}},120}, {{{0,8}},56}, {{{0,9}},209},
    {{{81,7}},17}, {{{0,8}},104}, {{{0,8}},40}, {{{0,9}},177},
    {{{0,8}},8}, {{{0,8}},136}, {{{0,8}},72}, {{{0,9}},241},
    {{{80,7}},4}, {{{0,8}},84}, {{{0,8}},20}, {{{85,8}},227},
    {{{83,7}},43}, {{{0,8}},116}, {{{0,8}},52}, {{{0,9}},201},
    {{{81,7}},13}, {{{0,8}},100}, {{{0,8}},36}, {{{0,9}},169},
    {{{0,8}},4}, {{{0,8}},132}, {{{0,8}},68}, {{{0,9}},233},
    {{{80,7}},8}, {{{0,8}},92}, {{{0,8}},28}, {{{0,9}},153},
    {{{84,7}},83}, {{{0,8}},124}, {{{0,8}},60}, {{{0,9}},217},
    {{{82,7}},23}, {{{0,8}},108}, {{{0,8}},44}, {{{0,9}},185},
    {{{0,8}},12}, {{{0,8}},140}, {{{0,8}},76}, {{{0,9}},249},
    {{{80,7}},3}, {{{0,8}},82}, {{{0,8}},18}, {{{85,8}},163},
    {{{83,7}},35}, {{{0,8}},114}, {{{0,8}},50}, {{{0,9}},197},
    {{{81,7}},11}, {{{0,8}},98}, {{{0,8}},34}, {{{0,9}},165},
    {{{0,8}},2}, {{{0,8}},130}, {{{0,8}},66}, {{{0,9}},229},
    {{{80,7}},7}, {{{0,8}},90}, {{{0,8}},26}, {{{0,9}},149},
    {{{84,7}},67}, {{{0,8}},122}, {{{0,8}},58}, {{{0,9}},213},
    {{{82,7}},19}, {{{0,8}},106}, {{{0,8}},42}, {{{0,9}},181},
    {{{0,8}},10}, {{{0,8}},138}, {{{0,8}},74}, {{{0,9}},245},
    {{{80,7}},5}, {{{0,8}},86}, {{{0,8}},22}, {{{192,8}},0},
    {{{83,7}},51}, {{{0,8}},118}, {{{0,8}},54}, {{{0,9}},205},
    {{{81,7}},15}, {{{0,8}},102}, {{{0,8}},38}, {{{0,9}},173},
    {{{0,8}},6}, {{{0,8}},134}, {{{0,8}},70}, {{{0,9}},237},
    {{{80,7}},9}, {{{0,8}},94}, {{{0,8}},30}, {{{0,9}},157},
    {{{84,7}},99}, {{{0,8}},126}, {{{0,8}},62}, {{{0,9}},221},
    {{{82,7}},27}, {{{0,8}},110}, {{{0,8}},46}, {{{0,9}},189},
    {{{0,8}},14}, {{{0,8}},142}, {{{0,8}},78}, {{{0,9}},253},
    {{{96,7}},256}, {{{0,8}},81}, {{{0,8}},17}, {{{85,8}},131},
    {{{82,7}},31}, {{{0,8}},113}, {{{0,8}},49}, {{{0,9}},195},
    {{{80,7}},10}, {{{0,8}},97}, {{{0,8}},33}, {{{0,9}},163},
    {{{0,8}},1}, {{{0,8}},129}, {{{0,8}},65}, {{{0,9}},227},
    {{{80,7}},6}, {{{0,8}},89}, {{{0,8}},25}, {{{0,9}},147},
    {{{83,7}},59}, {{{0,8}},121}, {{{0,8}},57}, {{{0,9}},211},
    {{{81,7}},17}, {{{0,8}},105}, {{{0,8}},41}, {{{0,9}},179},
    {{{0,8}},9}, {{{0,8}},137}, {{{0,8}},73}, {{{0,9}},243},
    {{{80,7}},4}, {{{0,8}},85}, {{{0,8}},21}, {{{80,8}},258},
    {{{83,7}},43}, {{{0,8}},117}, {{{0,8}},53}, {{{0,9}},203},
    {{{81,7}},13}, {{{0,8}},101}, {{{0,8}},37}, {{{0,9}},171},
    {{{0,8}},5}, {{{0,8}},133}, {{{0,8}},69}, {{{0,9}},235},
    {{{80,7}},8}, {{{0,8}},93}, {{{0,8}},29}, {{{0,9}},155},
    {{{84,7}},83}, {{{0,8}},125}, {{{0,8}},61}, {{{0,9}},219},
    {{{82,7}},23}, {{{0,8}},109}, {{{0,8}},45}, {{{0,9}},187},
    {{{0,8}},13}, {{{0,8}},141}, {{{0,8}},77}, {{{0,9}},251},
    {{{80,7}},3}, {{{0,8}},83}, {{{0,8}},19}, {{{85,8}},195},
    {{{83,7}},35}, {{{0,8}},115}, {{{0,8}},51}, {{{0,9}},199},
    {{{81,7}},11}, {{{0,8}},99}, {{{0,8}},35}, {{{0,9}},167},
    {{{0,8}},3}, {{{0,8}},131}, {{{0,8}},67}, {{{0,9}},231},
    {{{80,7}},7}, {{{0,8}},91}, {{{0,8}},27}, {{{0,9}},151},
    {{{84,7}},67}, {{{0,8}},123}, {{{0,8}},59}, {{{0,9}},215},
    {{{82,7}},19}, {{{0,8}},107}, {{{0,8}},43}, {{{0,9}},183},
    {{{0,8}},11}, {{{0,8}},139}, {{{0,8}},75}, {{{0,9}},247},
    {{{80,7}},5}, {{{0,8}},87}, {{{0,8}},23}, {{{192,8}},0},
    {{{83,7}},51}, {{{0,8}},119}, {{{0,8}},55}, {{{0,9}},207},
    {{{81,7}},15}, {{{0,8}},103}, {{{0,8}},39}, {{{0,9}},175},
    {{{0,8}},7}, {{{0,8}},135}, {{{0,8}},71}, {{{0,9}},239},
    {{{80,7}},9}, {{{0,8}},95}, {{{0,8}},31}, {{{0,9}},159},
    {{{84,7}},99}, {{{0,8}},127}, {{{0,8}},63}, {{{0,9}},223},
    {{{82,7}},27}, {{{0,8}},111}, {{{0,8}},47}, {{{0,9}},191},
    {{{0,8}},15}, {{{0,8}},143}, {{{0,8}},79}, {{{0,9}},255}
  };

static inflate_huft fixed_td[] = {
    {{{80,5}},1}, {{{87,5}},257}, {{{83,5}},17}, {{{91,5}},4097},
    {{{81,5}},5}, {{{89,5}},1025}, {{{85,5}},65}, {{{93,5}},16385},
    {{{80,5}},3}, {{{88,5}},513}, {{{84,5}},33}, {{{92,5}},8193},
    {{{82,5}},9}, {{{90,5}},2049}, {{{86,5}},129}, {{{192,5}},24577},
    {{{80,5}},2}, {{{87,5}},385}, {{{83,5}},25}, {{{91,5}},6145},
    {{{81,5}},7}, {{{89,5}},1537}, {{{85,5}},97}, {{{93,5}},24577},
    {{{80,5}},4}, {{{88,5}},769}, {{{84,5}},49}, {{{92,5}},12289},
    {{{82,5}},13}, {{{90,5}},3073}, {{{86,5}},193}, {{{192,5}},24577}
  };

int inflate_trees_fixed(uIntf * bl, uIntf *bd, inflate_huft * FAR * tl, inflate_huft * FAR * td, z_streamp z)
{
	*bl = fixed_bl;
	*bd = fixed_bd;
	*tl = fixed_tl;
	*td = fixed_td;
	return Z_OK;
}

/* infblock.c -- interpret and process block types to last block
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

void inflate_blocks_reset(inflate_blocks_statef *s, z_streamp z, uLongf *c)
{
	if (c != 0)
		*c = s->check;
	if (s->mode == BTREE || s->mode == DTREE)
		ZFREE(z, s->sub.trees.blens);
	if (s->mode == CODES)
		ZFREE(z, s->sub.decode.codes);
	s->mode = TYPE;
	s->bitk = 0;
	s->bitb = 0;
	s->read = s->write = s->window;
	if (s->checkfn != 0)
		z->adler = s->check = (*s->checkfn)(0L, (const Bytef *)0, 0);
}

inflate_blocks_statef *inflate_blocks_new(z_streamp z, check_func c, uInt w)
{
	inflate_blocks_statef *s;

	if ((s = (inflate_blocks_statef *)ZALLOC(z,1,sizeof(struct inflate_blocks_state))) == 0)
		return s;
	if ((s->hufts = (inflate_huft *)ZALLOC(z, sizeof(inflate_huft), MANY)) == 0)
	{
		ZFREE(z, s);
		return 0;
	}
	if ((s->window = (Bytef *)ZALLOC(z, 1, w)) == 0)
	{
		ZFREE(z, s->hufts);
		ZFREE(z, s);
		return 0;
	}
	s->end = s->window + w;
	s->checkfn = c;
	s->mode = TYPE;
	inflate_blocks_reset(s, z, 0);
	return s;
}


int inflate_blocks(inflate_blocks_statef *s, z_streamp z, int r)
{
	uInt t;               /* temporary storage */
	uLong b;              /* bit buffer */
	uInt k;               /* bits in bit buffer */
	Bytef *p;             /* input data pointer */
	uInt n;               /* bytes available there */
	Bytef *q;             /* output window write pointer */
	uInt m;               /* bytes to end of window or read pointer */

	/* copy input/output information to locals (UPDATE macro restores) */
	LOAD

		/* process input based on current state */
		while (1) switch (s->mode)
		{
			case TYPE:
				NEEDBITS(3)
					t = (uInt)b & 7;
				s->last = t & 1;
				switch (t >> 1)
				{
					case 0:                         /* stored */
						DUMPBITS(3)
							t = k & 7;                    /* go to byte boundary */
						DUMPBITS(t)
							s->mode = LENS;               /* get length of stored block */
						break;
					case 1:                         /* fixed */
						{
							uInt bl, bd;
							inflate_huft *tl, *td;

							inflate_trees_fixed(&bl, &bd, &tl, &td, z);
							s->sub.decode.codes = inflate_codes_new(bl, bd, tl, td, z);
							if (s->sub.decode.codes == 0)
							{
								r = Z_MEM_ERROR;
								LEAVE
							}
						}
						DUMPBITS(3)
							s->mode = CODES;
						break;
					case 2:                         /* dynamic */
						DUMPBITS(3)
							s->mode = TABLE;
						break;
					case 3:                         /* illegal */
						DUMPBITS(3)
							s->mode = BAD;
						z->msg = (char*)"invalid block type";
						r = Z_DATA_ERROR;
						LEAVE
				}
				break;
			case LENS:
				NEEDBITS(32)
					if ((((~b) >> 16) & 0xffff) != (b & 0xffff))
					{
						s->mode = BAD;
						z->msg = (char*)"invalid stored block lengths";
						r = Z_DATA_ERROR;
						LEAVE
					}
				s->sub.left = (uInt)b & 0xffff;
				b = k = 0;                      /* dump bits */
				s->mode = s->sub.left ? STORED : (s->last ? DRY : TYPE);
				break;
			case STORED:
				if (n == 0)
					LEAVE
						NEEDOUT
						t = s->sub.left;
				if (t > n) t = n;
				if (t > m) t = m;
				memcpy(q, p, t);
				p += t;  n -= t;
				q += t;  m -= t;
				if ((s->sub.left -= t) != 0)
					break;
				s->mode = s->last ? DRY : TYPE;
				break;
			case TABLE:
				NEEDBITS(14)
					s->sub.trees.table = t = (uInt)b & 0x3fff;
				t = 258 + (t & 0x1f) + ((t >> 5) & 0x1f);
				if ((s->sub.trees.blens = (uIntf*)ZALLOC(z, t, sizeof(uInt))) == 0)
				{
					r = Z_MEM_ERROR;
					LEAVE
				}
				DUMPBITS(14)
					s->sub.trees.index = 0;
				s->mode = BTREE;
			case BTREE:
				while (s->sub.trees.index < 4 + (s->sub.trees.table >> 10))
				{
					NEEDBITS(3)
						s->sub.trees.blens[border[s->sub.trees.index++]] = (uInt)b & 7;
					DUMPBITS(3)
				}
				while (s->sub.trees.index < 19)
					s->sub.trees.blens[border[s->sub.trees.index++]] = 0;
				s->sub.trees.bb = 7;
				t = inflate_trees_bits(s->sub.trees.blens, &s->sub.trees.bb,
						&s->sub.trees.tb, s->hufts, z);
				if (t != Z_OK)
				{
					r = t;
					if (r == Z_DATA_ERROR)
					{
						ZFREE(z, s->sub.trees.blens);
						s->mode = BAD;
					}
					LEAVE
				}
				s->sub.trees.index = 0;
				s->mode = DTREE;
			case DTREE:
				while (t = s->sub.trees.table,
						s->sub.trees.index < 258 + (t & 0x1f) + ((t >> 5) & 0x1f))
				{
					inflate_huft *h;
					uInt i, j, c;

					t = s->sub.trees.bb;
					NEEDBITS(t)
						h = s->sub.trees.tb + ((uInt)b & inflate_mask[t]);
					t = h->word.what.Bits;
					c = h->base;
					if (c < 16)
					{
						DUMPBITS(t)
							s->sub.trees.blens[s->sub.trees.index++] = c;
					}
					else /* c == 16..18 */
					{
						i = c == 18 ? 7 : c - 14;
						j = c == 18 ? 11 : 3;
						NEEDBITS(t + i)
							DUMPBITS(t)
							j += (uInt)b & inflate_mask[i];
						DUMPBITS(i)
							i = s->sub.trees.index;
						t = s->sub.trees.table;
						if (i + j > 258 + (t & 0x1f) + ((t >> 5) & 0x1f) ||
								(c == 16 && i < 1))
						{
							ZFREE(z, s->sub.trees.blens);
							s->mode = BAD;
							z->msg = (char*)"invalid bit length repeat";
							r = Z_DATA_ERROR;
							LEAVE
						}
						c = c == 16 ? s->sub.trees.blens[i - 1] : 0;
						do {
							s->sub.trees.blens[i++] = c;
						} while (--j);
						s->sub.trees.index = i;
					}
				}
				s->sub.trees.tb = 0;
				{
					uInt bl, bd;
					inflate_huft *tl, *td;
					inflate_codes_statef *c;

					bl = 9;         /* must be <= 9 for lookahead assumptions */
					bd = 6;         /* must be <= 9 for lookahead assumptions */
					t = s->sub.trees.table;
					t = inflate_trees_dynamic(257 + (t & 0x1f), 1 + ((t >> 5) & 0x1f),
							s->sub.trees.blens, &bl, &bd, &tl, &td,
							s->hufts, z);
					if (t != Z_OK)
					{
						if (t == (uInt)Z_DATA_ERROR)
						{
							ZFREE(z, s->sub.trees.blens);
							s->mode = BAD;
						}
						r = t;
						LEAVE
					}
					if ((c = inflate_codes_new(bl, bd, tl, td, z)) == 0)
					{
						r = Z_MEM_ERROR;
						LEAVE
					}
					s->sub.decode.codes = c;
				}
				ZFREE(z, s->sub.trees.blens);
				s->mode = CODES;
			case CODES:
				UPDATE
					if ((r = inflate_codes(s, z, r)) != Z_STREAM_END)
						return inflate_flush(s, z, r);
				r = Z_OK;
				ZFREE(z, s->sub.decode.codes);
				LOAD
					if (!s->last)
					{
						s->mode = TYPE;
						break;
					}
				s->mode = DRY;
			case DRY:
				FLUSH
					if (s->read != s->write)
						LEAVE
							s->mode = DONE;
			case DONE:
				r = Z_STREAM_END;
				LEAVE
			case BAD:
					r = Z_DATA_ERROR;
					LEAVE
			default:
						r = Z_STREAM_ERROR;
						LEAVE
		}
}


int inflate_blocks_free(inflate_blocks_statef *s, z_streamp z)
{
	inflate_blocks_reset(s, z, 0);
	ZFREE(z, s->window);
	ZFREE(z, s->hufts);
	ZFREE(z, s);
	return Z_OK;
}

/* inflate_util.c -- data and routines common to blocks and codes
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/* copy as much as possible from the sliding window to the output area */
int inflate_flush(inflate_blocks_statef *s, z_streamp z, int r)
{
	uInt n;
	Bytef *p;
	Bytef *q;

	/* local copies of source and destination pointers */
	p = z->next_out;
	q = s->read;

	/* compute number of bytes to copy as far as end of window */
	n = (uInt)((q <= s->write ? s->write : s->end) - q);
	if (n > z->avail_out) n = z->avail_out;
	if (n && r == Z_BUF_ERROR) r = Z_OK;

	/* update counters */
	z->avail_out -= n;
	z->total_out += n;

	/* update check information */
	if (s->checkfn != 0)
		z->adler = s->check = (*s->checkfn)(s->check, q, n);

	/* copy as far as end of window */
	memcpy(p, q, n);
	p += n;
	q += n;

	/* see if more to copy at beginning of window */
	if (q == s->end)
	{
		/* wrap pointers */
		q = s->window;
		if (s->write == s->end)
			s->write = s->window;

		/* compute bytes to copy */
		n = (uInt)(s->write - q);
		if (n > z->avail_out) n = z->avail_out;
		if (n && r == Z_BUF_ERROR) r = Z_OK;

		/* update counters */
		z->avail_out -= n;
		z->total_out += n;

		/* update check information */
		if (s->checkfn != 0)
			z->adler = s->check = (*s->checkfn)(s->check, q, n);

		/* copy */
		memcpy(p, q, n);
		p += n;
		q += n;
	}

	/* update pointers */
	z->next_out = p;
	s->read = q;

	/* done */
	return r;
}

/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

const char *z_errmsg[10] =
{
	"need dictionary",     /* Z_NEED_DICT       2  */
	"stream end",          /* Z_STREAM_END      1  */
	"",                    /* Z_OK              0  */
	"file error",          /* Z_ERRNO         (-1) */
	"stream error",        /* Z_STREAM_ERROR  (-2) */
	"data error",          /* Z_DATA_ERROR    (-3) */
	"insufficient memory", /* Z_MEM_ERROR     (-4) */
	"buffer error",        /* Z_BUF_ERROR     (-5) */
	"incompatible version",/* Z_VERSION_ERROR (-6) */
	""
};

const char * ZEXPORT zlibVersion (void)
{
	return ZLIB_VERSION;
}

const char * ZEXPORT zError(int err)
{
	return z_errmsg[Z_NEED_DICT-(err)];
}

voidpf zcalloc (voidpf opaque, unsigned items, unsigned size)
{
	if (opaque) items += size - size; /* make compiler happy */
	return (voidpf)calloc(items, size);
}

void  zcfree (voidpf opaque, voidpf ptr)
{
	free(ptr);
	if (opaque) return; /* make compiler happy */
}


/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

static const uLongf crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

#define DO1_CRC32(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2_CRC32(buf)  DO1_CRC32(buf); DO1_CRC32(buf);
#define DO4_CRC32(buf)  DO2_CRC32(buf); DO2_CRC32(buf);
#define DO8_CRC32(buf)  DO4_CRC32(buf); DO4_CRC32(buf);

uLong ZEXPORT crc32(uLong crc, const Bytef *buf, uInt len)
{
	if (buf == 0) return 0L;
	crc = crc ^ 0xffffffffL;
	while (len >= 8)
	{
		DO8_CRC32(buf);
		len -= 8;
	}
	if (len) do {
		DO1_CRC32(buf);
	} while (--len);
	return crc ^ 0xffffffffL;
}

/* inflate.c -- zlib interface to inflate modules
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */


int ZEXPORT inflateReset(z_streamp z)
{
	if (z == 0 || z->state == 0)
		return Z_STREAM_ERROR;
	z->total_in = z->total_out = 0;
	z->msg = 0;
	z->state->mode = z->state->nowrap ? BLOCKS_INFLATE : METHOD_INFLATE;
	inflate_blocks_reset(z->state->blocks, z, 0);
	return Z_OK;
}

int ZEXPORT inflateEnd(z_streamp z)
{
	if (z == 0 || z->state == 0 || z->zfree == 0)
		return Z_STREAM_ERROR;
	if (z->state->blocks != 0)
		inflate_blocks_free(z->state->blocks, z);
	ZFREE(z, z->state);
	z->state = 0;
	return Z_OK;
}

uLong ZEXPORT adler32(uLong adler, const Bytef *buf, uInt len)
{
	unsigned long s1 = adler & 0xffff;
	unsigned long s2 = (adler >> 16) & 0xffff;
	int k;

	if (buf == 0) return 1L;

	while (len > 0) {
		k = len < NMAX ? len : NMAX;
		len -= k;
		while (k >= 16) {
			DO16_ADLER(buf);
			buf += 16;
			k -= 16;
		}
		if (k != 0) do {
			s1 += *buf++;
			s2 += s1;
		} while (--k);
		s1 %= BASE;
		s2 %= BASE;
	}
	return (s2 << 16) | s1;
}


int ZEXPORT inflateInit2_(z_streamp z, int w, const char * version, int stream_size)
{
	if (version == 0 || version[0] != ZLIB_VERSION[0] ||
			stream_size != sizeof(z_stream))
		return Z_VERSION_ERROR;

	/* initialize state */
	if (z == 0)
		return Z_STREAM_ERROR;
	z->msg = 0;
	if (z->zalloc == 0)
	{
		z->zalloc = zcalloc;
		z->opaque = (voidpf)0;
	}
	if (z->zfree == 0) z->zfree = zcfree;
	if ((z->state = (struct internal_state FAR *)
				ZALLOC(z,1,sizeof(struct internal_state))) == 0)
		return Z_MEM_ERROR;
	z->state->blocks = 0;

	/* handle undocumented nowrap option (no zlib header or check) */
	z->state->nowrap = 0;
	if (w < 0)
	{
		w = - w;
		z->state->nowrap = 1;
	}

	/* set window size */
	if (w < 8 || w > 15)
	{
		inflateEnd(z);
		return Z_STREAM_ERROR;
	}
	z->state->wbits = (uInt)w;

	/* create inflate_blocks state */
	if ((z->state->blocks =
				inflate_blocks_new(z, z->state->nowrap ? 0 : adler32, (uInt)1 << w))
			== 0)
	{
		inflateEnd(z);
		return Z_MEM_ERROR;
	}

	/* reset state */
	inflateReset(z);
	return Z_OK;
}


int ZEXPORT inflateInit_(z_streamp z, const char * version, int stream_size)
{
	return inflateInit2_(z, DEF_WBITS, version, stream_size);
}

#define NEEDBYTE_INFLATE {if(z->avail_in==0)return r;r=f;}
#define NEXTBYTE_INFLATE (z->avail_in--,z->total_in++,*z->next_in++)

int ZEXPORT inflate(z_streamp z, int f)
{
	int r;
	uInt b;

	if (z == 0 || z->state == 0 || z->next_in == 0)
		return Z_STREAM_ERROR;
	f = f == Z_FINISH ? Z_BUF_ERROR : Z_OK;
	r = Z_BUF_ERROR;
	while (1) switch (z->state->mode)
	{
		case METHOD_INFLATE:
			NEEDBYTE_INFLATE
				if (((z->state->sub.method = NEXTBYTE_INFLATE) & 0xf) != Z_DEFLATED)
				{
					z->state->mode = BAD_INFLATE;
					z->msg = (char*)"unknown compression method";
					z->state->sub.marker = 5;       /* can't try inflateSync */
					break;
				}
			if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
			{
				z->state->mode = BAD_INFLATE;
				z->msg = (char*)"invalid window size";
				z->state->sub.marker = 5;       /* can't try inflateSync */
				break;
			}
			z->state->mode = FLAG_INFLATE;
		case FLAG_INFLATE:
			NEEDBYTE_INFLATE
				b = NEXTBYTE_INFLATE;
			if (((z->state->sub.method << 8) + b) % 31)
			{
				z->state->mode = BAD_INFLATE;
				z->msg = (char*)"incorrect header check";
				z->state->sub.marker = 5;       /* can't try inflateSync */
				break;
			}
			if (!(b & PRESET_DICT))
			{
				z->state->mode = BLOCKS_INFLATE;
				break;
			}
			z->state->mode = DICT4_INFLATE;
		case DICT4_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need = (uLong)NEXTBYTE_INFLATE << 24;
			z->state->mode = DICT3_INFLATE;
		case DICT3_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE << 16;
			z->state->mode = DICT2_INFLATE;
		case DICT2_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE << 8;
			z->state->mode = DICT1_INFLATE;
		case DICT1_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE;
			z->adler = z->state->sub.check.need;
			z->state->mode = DICT0_INFLATE;
			return Z_NEED_DICT;
		case DICT0_INFLATE:
			z->state->mode = BAD_INFLATE;
			z->msg = (char*)"need dictionary";
			z->state->sub.marker = 0;       /* can try inflateSync */
			return Z_STREAM_ERROR;
		case BLOCKS_INFLATE:
			r = inflate_blocks(z->state->blocks, z, r);
			if (r == Z_DATA_ERROR)
			{
				z->state->mode = BAD_INFLATE;
				z->state->sub.marker = 0;       /* can try inflateSync */
				break;
			}
			if (r == Z_OK)
				r = f;
			if (r != Z_STREAM_END)
				return r;
			r = f;
			inflate_blocks_reset(z->state->blocks, z, &z->state->sub.check.was);
			if (z->state->nowrap)
			{
				z->state->mode = DONE_INFLATE;
				break;
			}
			z->state->mode = CHECK4_INFLATE;
		case CHECK4_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need = (uLong)NEXTBYTE_INFLATE << 24;
			z->state->mode = CHECK3_INFLATE;
		case CHECK3_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE << 16;
			z->state->mode = CHECK2_INFLATE;
		case CHECK2_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE << 8;
			z->state->mode = CHECK1_INFLATE;
		case CHECK1_INFLATE:
			NEEDBYTE_INFLATE
				z->state->sub.check.need += (uLong)NEXTBYTE_INFLATE;

			if (z->state->sub.check.was != z->state->sub.check.need)
			{
				z->state->mode = BAD_INFLATE;
				z->msg = (char*)"incorrect data check";
				z->state->sub.marker = 5;       /* can't try inflateSync */
				break;
			}
			z->state->mode = DONE_INFLATE;
		case DONE_INFLATE:
			return Z_STREAM_END;
		case BAD_INFLATE:
			return Z_DATA_ERROR;
		default:
			return Z_STREAM_ERROR;
	}
}

/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include <stdio.h>

#ifdef _MSC_VER
#pragma warning (disable:4996)
#endif

#ifndef Z_BUFSIZE
#  ifdef MAXSEG_64K
#    define Z_BUFSIZE 4096 /* minimize memory usage for 16-bit DOS */
#  else
#    define Z_BUFSIZE 16384
#  endif
#endif
#ifndef Z_PRINTF_BUFSIZE
#  define Z_PRINTF_BUFSIZE 4096
#endif

static int gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

typedef struct gz_stream {
    z_stream stream;
    int      z_err;   /* error code for last stream operation */
    int      z_eof;   /* set if end of input file */
    FILE     *file;   /* .gz file */
    Byte     *inbuf;  /* input buffer */
    Byte     *outbuf; /* output buffer */
    uLong    crc;     /* crc32 of uncompressed data */
    char     *msg;    /* error message */
    char     *path;   /* path name for debugging only */
    int      transparent; /* 1 if input file is not a .gz file */
    char     mode;    /* 'w' or 'r' */
    long     startpos; /* start of compressed data in file (header skipped) */
} gz_stream;


static void   check_header OF((gz_stream *s));
static int    destroy      OF((gz_stream *s));
static uLong  getLong      OF((gz_stream *s));

static gzFile gz_open (const char * path, const char *mode, int fd)
{
	int err;
	char *p = (char*)mode;
	gz_stream *s;
	char fmode[80]; /* copy of mode, without the compression level */
	char *m = fmode;

	if (!path || !mode) return 0;

	s = (gz_stream *)malloc(sizeof(gz_stream));
	if (!s) return 0;

	s->stream.zalloc = (alloc_func)0;
	s->stream.zfree = (free_func)0;
	s->stream.opaque = (voidpf)0;
	s->stream.next_in = s->inbuf = 0;
	s->stream.next_out = s->outbuf = 0;
	s->stream.avail_in = s->stream.avail_out = 0;
	s->file = NULL;
	s->z_err = Z_OK;
	s->z_eof = 0;
	s->crc = crc32(0L, 0, 0);
	s->msg = NULL;
	s->transparent = 0;

	s->path = (char*)malloc(strlen(path)+1);
	if (s->path == NULL) {
		return destroy(s), (gzFile)0;
	}
	strcpy(s->path, path); /* do this early for debugging */

	s->mode = '\0';
	do {
		if (*p == 'r') s->mode = 'r';
		if (*p == 'w' || *p == 'a') s->mode = 'w';
		if (*p >= '0' && *p <= '9') {
		} else if (*p == 'f') {
		} else if (*p == 'h') {
		} else {
			*m++ = *p; /* copy the mode */
		}
	} while (*p++ && m != fmode + sizeof(fmode));
	if (s->mode == '\0') return destroy(s), (gzFile)0;

	if (s->mode == 'w') {
		err = Z_STREAM_ERROR;
		if (err != Z_OK || s->outbuf == 0) {
			return destroy(s), (gzFile)0;
		}
	} else {
		s->stream.next_in  = s->inbuf = (Byte*)malloc(Z_BUFSIZE);

		err = inflateInit2(&(s->stream), -MAX_WBITS);
		/* windowBits is passed < 0 to tell that there is no zlib header.
		 * Note that in this case inflate *requires* an extra "dummy" byte
		 * after the compressed stream in order to complete decompression and
		 * return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
		 * present after the compressed stream.
		 */
		if (err != Z_OK || s->inbuf == 0) {
			return destroy(s), (gzFile)0;
		}
	}
	s->stream.avail_out = Z_BUFSIZE;

	errno = 0;
	s->file = fopen((path), (fmode));
	if (s->file == NULL) {
		return destroy(s), (gzFile)0;
	}
	if (s->mode == 'w') {
		/* Write a very simple .gz header:
		 */
		fprintf(s->file, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
				Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, OS_CODE);
		s->startpos = 10L;
		/* We use 10L instead of ftell(s->file) to because ftell causes an
		 * fflush on some systems. This version of the library doesn't use
		 * startpos anyway in write mode, so this initialization is not
		 * necessary.
		 */
	} else {
		check_header(s); /* skip the .gz header */
		s->startpos = (ftell(s->file) - s->stream.avail_in);
	}

	return (gzFile)s;
}

gzFile ZEXPORT gzopen (const char * path, const char * mode)
{
	return gz_open (path, mode, -1);
}

gzFile ZEXPORT gzdopen (int fd, const char * mode)
{
	char name[20];

	if (fd < 0) return (gzFile)0;
	sprintf(name, "<fd:%d>", fd); /* for debugging */

	return gz_open (name, mode, fd);
}

static int get_byte(s)
    gz_stream *s;
{
    if (s->z_eof) return EOF;
    if (s->stream.avail_in == 0) {
	errno = 0;
	s->stream.avail_in = fread(s->inbuf, 1, Z_BUFSIZE, s->file);
	if (s->stream.avail_in == 0) {
	    s->z_eof = 1;
	    if (ferror(s->file)) s->z_err = Z_ERRNO;
	    return EOF;
	}
	s->stream.next_in = s->inbuf;
    }
    s->stream.avail_in--;
    return *(s->stream.next_in)++;
}

static void check_header(gz_stream *s)
{
	int method; /* method byte */
	int flags;  /* flags byte */
	uInt len;
	int c;

	/* Check the gzip magic header */
	for (len = 0; len < 2; len++) {
		c = get_byte(s);
		if (c != gz_magic[len]) {
			if (len != 0) s->stream.avail_in++, s->stream.next_in--;
			if (c != EOF) {
				s->stream.avail_in++, s->stream.next_in--;
				s->transparent = 1;
			}
			s->z_err = s->stream.avail_in != 0 ? Z_OK : Z_STREAM_END;
			return;
		}
	}
	method = get_byte(s);
	flags = get_byte(s);
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		s->z_err = Z_DATA_ERROR;
		return;
	}

	/* Discard time, xflags and OS code: */
	for (len = 0; len < 6; len++) (void)get_byte(s);

	if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len  =  (uInt)get_byte(s);
		len += ((uInt)get_byte(s))<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0 && get_byte(s) != EOF) ;
	}
	if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
		while ((c = get_byte(s)) != 0 && c != EOF) ;
	}
	if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while ((c = get_byte(s)) != 0 && c != EOF) ;
	}
	if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		for (len = 0; len < 2; len++) (void)get_byte(s);
	}
	s->z_err = s->z_eof ? Z_DATA_ERROR : Z_OK;
}

static int destroy (gz_stream *s)
{
	int err = Z_OK;

	if (!s) return Z_STREAM_ERROR;

	if(s->msg)
		free(s->msg);

	if (s->stream.state != NULL) {
		if (s->mode == 'w') {
			err = Z_STREAM_ERROR;
		} else if (s->mode == 'r') {
			err = inflateEnd(&(s->stream));
		}
	}
	if (s->file != NULL && fclose(s->file)) {
		err = Z_ERRNO;
	}
	if (s->z_err < 0) err = s->z_err;

	if(s->inbuf)
		free(s->inbuf);
	if(s->outbuf)
		free(s->outbuf);
	if(s->path)
		free(s->path);
	if(s)
		free(s);
	return err;
}

int ZEXPORT gzread (gzFile file, voidp buf, unsigned len)
{
	gz_stream *s = (gz_stream*)file;
	Bytef *start = (Bytef*)buf; /* starting point for crc computation */
	Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */

	if (s == NULL || s->mode != 'r') return Z_STREAM_ERROR;

	if (s->z_err == Z_DATA_ERROR || s->z_err == Z_ERRNO) return -1;
	if (s->z_err == Z_STREAM_END) return 0;  /* EOF */

	next_out = (Byte*)buf;
	s->stream.next_out = (Bytef*)buf;
	s->stream.avail_out = len;

	while (s->stream.avail_out != 0) {

		if (s->transparent) {
			/* Copy first the lookahead bytes: */
			uInt n = s->stream.avail_in;
			if (n > s->stream.avail_out) n = s->stream.avail_out;
			if (n > 0) {
				memcpy(s->stream.next_out, s->stream.next_in, n);
				next_out += n;
				s->stream.next_out = next_out;
				s->stream.next_in   += n;
				s->stream.avail_out -= n;
				s->stream.avail_in  -= n;
			}
			if (s->stream.avail_out > 0) {
				s->stream.avail_out -= fread(next_out, 1, s->stream.avail_out,
						s->file);
			}
			len -= s->stream.avail_out;
			s->stream.total_in  += (uLong)len;
			s->stream.total_out += (uLong)len;
			if (len == 0) s->z_eof = 1;
			return (int)len;
		}
		if (s->stream.avail_in == 0 && !s->z_eof) {

			errno = 0;
			s->stream.avail_in = fread(s->inbuf, 1, Z_BUFSIZE, s->file);
			if (s->stream.avail_in == 0) {
				s->z_eof = 1;
				if (ferror(s->file)) {
					s->z_err = Z_ERRNO;
					break;
				}
			}
			s->stream.next_in = s->inbuf;
		}
		s->z_err = inflate(&(s->stream), Z_NO_FLUSH);

		if (s->z_err == Z_STREAM_END) {
			/* Check CRC and original size */
			s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));
			start = s->stream.next_out;

			if (getLong(s) != s->crc) {
				s->z_err = Z_DATA_ERROR;
			} else {
				(void)getLong(s);
				/* The uncompressed length returned by above getlong() may
				 * be different from s->stream.total_out) in case of
				 * concatenated .gz files. Check for such files:
				 */
				check_header(s);
				if (s->z_err == Z_OK) {
					uLong total_in = s->stream.total_in;
					uLong total_out = s->stream.total_out;

					inflateReset(&(s->stream));
					s->stream.total_in = total_in;
					s->stream.total_out = total_out;
					s->crc = crc32(0L, 0, 0);
				}
			}
		}
		if (s->z_err != Z_OK || s->z_eof) break;
	}
	s->crc = crc32(s->crc, start, (uInt)(s->stream.next_out - start));

	return (int)(len - s->stream.avail_out);
}

int ZEXPORT gzgetc(gzFile file)
{
	unsigned char c;

	return gzread(file, &c, 1) == 1 ? c : -1;
}

char * ZEXPORT gzgets(gzFile file, char *buf, int len)
{
	char *b = buf;
	if (buf == 0 || len <= 0) return 0;

	while (--len > 0 && gzread(file, buf, 1) == 1 && *buf++ != '\n') ;
	*buf = '\0';
	return b == buf && len > 0 ? 0 : b;
}

z_off_t ZEXPORT gzseek (gzFile file, z_off_t offset, int whence)
{
	gz_stream *s = (gz_stream*)file;

	if (s == NULL || whence == SEEK_END ||
			s->z_err == Z_ERRNO || s->z_err == Z_DATA_ERROR) {
		return -1L;
	}

	if (s->mode == 'w')
		return -1L;
	/* Rest of function is for reading only */

	/* compute absolute position */
	if (whence == SEEK_CUR) {
		offset += s->stream.total_out;
	}
	if (offset < 0) return -1L;

	if (s->transparent) {
		/* map to fseek */
		s->stream.avail_in = 0;
		s->stream.next_in = s->inbuf;
		if (fseek(s->file, offset, SEEK_SET) < 0) return -1L;

		s->stream.total_in = s->stream.total_out = (uLong)offset;
		return offset;
	}

	/* For a negative seek, rewind and use positive seek */
	if ((uLong)offset >= s->stream.total_out) {
		offset -= s->stream.total_out;
	} else if (gzrewind(file) < 0) {
		return -1L;
	}
	/* offset is now the number of bytes to skip. */

	if (offset != 0 && s->outbuf == 0) {
		s->outbuf = (Byte*)malloc(Z_BUFSIZE);
	}
	while (offset > 0)  {
		int size = Z_BUFSIZE;
		if (offset < Z_BUFSIZE) size = (int)offset;

		size = gzread(file, s->outbuf, (uInt)size);
		if (size <= 0) return -1L;
		offset -= size;
	}
	return (z_off_t)s->stream.total_out;
}

int ZEXPORT gzrewind (gzFile file)
{
	gz_stream *s = (gz_stream*)file;

	if (s == NULL || s->mode != 'r') return -1;

	s->z_err = Z_OK;
	s->z_eof = 0;
	s->stream.avail_in = 0;
	s->stream.next_in = s->inbuf;
	s->crc = crc32(0L, 0, 0);

	if (s->startpos == 0) { /* not a compressed file */
		rewind(s->file);
		return 0;
	}

	(void) inflateReset(&s->stream);
	return fseek(s->file, s->startpos, SEEK_SET);
}

z_off_t ZEXPORT gztell (gzFile file)
{
	return gzseek(file, 0L, SEEK_CUR);
}

int ZEXPORT gzeof (gzFile file)
{
	gz_stream *s = (gz_stream*)file;

	return (s == NULL || s->mode != 'r') ? 0 : s->z_eof;
}

static uLong getLong (gz_stream *s)
{
	uLong x = (uLong)get_byte(s);
	int c;

	x += ((uLong)get_byte(s))<<8;
	x += ((uLong)get_byte(s))<<16;
	c = get_byte(s);
	if (c == EOF) s->z_err = Z_DATA_ERROR;
	x += ((uLong)c)<<24;
	return x;
}

int ZEXPORT gzclose (gzFile file)
{
	gz_stream *s = (gz_stream*)file;

	if (s == NULL) return Z_STREAM_ERROR;

	if (s->mode == 'w')
		return Z_STREAM_ERROR;

	return destroy((gz_stream*)file);
}

const char*  ZEXPORT gzerror (gzFile file, int *errnum)
{
	char *m;
	gz_stream *s = (gz_stream*)file;

	if (s == NULL) {
		*errnum = Z_STREAM_ERROR;
		return (const char*)ERR_MSG(Z_STREAM_ERROR);
	}
	*errnum = s->z_err;
	if (*errnum == Z_OK) return (const char*)"";

	m =  (char*)(*errnum == Z_ERRNO ? zstrerror(errno) : s->stream.msg);

	if (m == NULL || *m == '\0') m = (char*)ERR_MSG(s->z_err);

	if(s->msg)
		free(s->msg);
	s->msg = (char*)malloc(strlen(s->path) + strlen(m) + 3);
	strcpy(s->msg, s->path);
	strcat(s->msg, ": ");
	strcat(s->msg, m);
	return (const char*)s->msg;
}
