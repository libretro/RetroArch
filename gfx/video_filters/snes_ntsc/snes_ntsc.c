/* snes_ntsc 0.2.2. http://www.slack.net/~ant/ */
#ifndef _BLARGG_SNES_NTSC_IMPLEMENTATION_H
#define _BLARGG_SNES_NTSC_IMPLEMENTATION_H

#include <stdlib.h>
#include <string.h>

#include "snes_ntsc.h"
#include "../softfilter.h"

/* Copyright (C) 2006-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

snes_ntsc_setup_t const retroarch_snes_ntsc_monochrome = { 0,-1, 0, 0,.2,  0,.2,-.2,-.2,-1,  1, 0, 0 };
snes_ntsc_setup_t const retroarch_snes_ntsc_composite  = { 0, 0, 0, 0, 0,  0, 0,  0,  0, 0,  1, 0, 0 };
snes_ntsc_setup_t const retroarch_snes_ntsc_svideo     = { 0, 0, 0, 0,.2,  0,.2, -1, -1, 0,  1, 0, 0 };
snes_ntsc_setup_t const retroarch_snes_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.7, -1, -1,-1,  1, 0, 0 };

int hires_blit = 1;

#define alignment_count 3
#define burst_count     3
#define rescale_in      8
#define rescale_out     7

#define artifacts_mid   1.0f
#define fringing_mid    1.0f
#define std_decoder_hue 0

#define rgb_bits        7 /* half normal range to allow for doubled hires pixels */
#define gamma_size      32

#include "snes_ntsc_impl.h"

/* 3 input pixels -> 8 composite samples */
pixel_info_t const retroarch_snes_ntsc_pixels [alignment_count] = {
	{ PIXEL_OFFSET( -4, -9 ), { 1, 1, .6667f, 0 } },
	{ PIXEL_OFFSET( -2, -7 ), {       .3333f, 1, 1, .3333f } },
	{ PIXEL_OFFSET(  0, -5 ), {                  0, .6667f, 1, 1 } },
};

static void merge_kernel_fields( snes_ntsc_rgb_t* io )
{
	int n;
	for ( n = burst_size; n; --n )
	{
		snes_ntsc_rgb_t p0 = io [burst_size * 0] + rgb_bias;
		snes_ntsc_rgb_t p1 = io [burst_size * 1] + rgb_bias;
		snes_ntsc_rgb_t p2 = io [burst_size * 2] + rgb_bias;
		/* merge colors without losing precision */
		io [burst_size * 0] =
				((p0 + p1 - ((p0 ^ p1) & snes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		io [burst_size * 1] =
				((p1 + p2 - ((p1 ^ p2) & snes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		io [burst_size * 2] =
				((p2 + p0 - ((p2 ^ p0) & snes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		++io;
	}
}

static void correct_errors( snes_ntsc_rgb_t color, snes_ntsc_rgb_t* out )
{
	int n;
	for ( n = burst_count; n; --n )
	{
		unsigned i;
		for ( i = 0; i < rgb_kernel_size / 2; i++ )
		{
			snes_ntsc_rgb_t error = color -
					out [i    ] - out [(i+12)%14+14] - out [(i+10)%14+28] -
					out [i + 7] - out [i + 5    +14] - out [i + 3    +28];
			DISTRIBUTE_ERROR( i+3+28, i+5+14, i+7 );
		}
		out += alignment_count * rgb_kernel_size;
	}
}

struct snes_ntsc_init_work
{
	init_t impl;
	/* One entry's kernels, built at full width before they are stored */
	snes_ntsc_rgb_t kernel [snes_ntsc_entry_size];
};

void retroarch_snes_ntsc_init( snes_ntsc_t* ntsc, snes_ntsc_setup_t const* setup )
{
	int merge_fields;
	int entry;
	/* Heap-held: init_t carries the resampled kernel and gamma tables,
	 * and with one entry's kernels beside it makes a 3.6 KiB frame on
	 * stacks whose whole budget is 8 KiB.  The API
	 * is void, so on allocation failure this returns with the table
	 * untouched; every in-tree caller calloc()s the snes_ntsc_t, and a
	 * zeroed table blits black -- the same outcome as the filter
	 * failing to load. */
	struct snes_ntsc_init_work* work = (struct snes_ntsc_init_work*)
			calloc( 1, sizeof *work );
	init_t* impl;
	if ( !work )
		return;
	impl = &work->impl;
	if ( !setup )
		setup = &retroarch_snes_ntsc_composite;
	init( impl, setup );

	merge_fields = setup->merge_fields;
	if ( setup->artifacts <= -1 && setup->fringing <= -1 )
		merge_fields = 1;

	for ( entry = 0; entry < snes_ntsc_palette_size; entry++ )
   {
      /* Reduce number of significant bits of source color. Clearing the
         low bits of R and B were least notictable. Modifying green was too
         noticeable. */
      int ir = entry >> 8 & 0x1E;
      int ig = entry >> 4 & 0x1F;
      int ib = entry << 1 & 0x1E;

      float rr = impl->to_float [ir];
      float gg = impl->to_float [ig];
      float bb = impl->to_float [ib];

      float y, i, q = RGB_TO_YIQ( rr, gg, bb, y, i );

      int r, g, b = YIQ_TO_RGB( y, i, q, impl->to_rgb, int, r, g );
      snes_ntsc_rgb_t rgb = PACK_RGB( r, g, b );

      snes_ntsc_rgb_t* out = work->kernel;
      int n;
      gen_kernel( impl, y, i, q, out );
      if ( merge_fields )
         merge_kernel_fields( out );
      correct_errors( rgb, out );
      for ( n = 0; n < snes_ntsc_entry_size; n++ )
         ntsc->table [entry] [n] = (snes_ntsc_entry_t) out [n];
   }
	free( work );
}


#ifndef SNES_NTSC_NO_BLITTERS

void retroarch_snes_ntsc_blit( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input, long in_row_width,
		int burst_phase, int in_width, int in_height, void* rgb_out, long out_pitch, int first, int last )
{
	int chunk_count = (in_width - 1) / snes_ntsc_in_chunk;
	for ( ; in_height; --in_height )
	{
		SNES_NTSC_IN_T const* line_in = input;
		SNES_NTSC_BEGIN_ROW( ntsc, burst_phase,
				snes_ntsc_black, snes_ntsc_black, SNES_NTSC_ADJ_IN( *line_in ) );
		snes_ntsc_out_t* line_out = (snes_ntsc_out_t*) rgb_out;
		int n;
		++line_in;

		for ( n = chunk_count; n; --n )
		{
			/* order of input and output pixels must not be altered */
			SNES_NTSC_COLOR_IN( 0, SNES_NTSC_ADJ_IN( line_in [0] ) );
			SNES_NTSC_RGB_OUT( 0, line_out [0], SNES_NTSC_OUT_DEPTH );
			SNES_NTSC_RGB_OUT( 1, line_out [1], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 1, SNES_NTSC_ADJ_IN( line_in [1] ) );
			SNES_NTSC_RGB_OUT( 2, line_out [2], SNES_NTSC_OUT_DEPTH );
			SNES_NTSC_RGB_OUT( 3, line_out [3], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 2, SNES_NTSC_ADJ_IN( line_in [2] ) );
			SNES_NTSC_RGB_OUT( 4, line_out [4], SNES_NTSC_OUT_DEPTH );
			SNES_NTSC_RGB_OUT( 5, line_out [5], SNES_NTSC_OUT_DEPTH );
			SNES_NTSC_RGB_OUT( 6, line_out [6], SNES_NTSC_OUT_DEPTH );

			line_in  += 3;
			line_out += 7;
		}

		/* finish final pixels */
		SNES_NTSC_COLOR_IN( 0, snes_ntsc_black );
		SNES_NTSC_RGB_OUT( 0, line_out [0], SNES_NTSC_OUT_DEPTH );
		SNES_NTSC_RGB_OUT( 1, line_out [1], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 1, snes_ntsc_black );
		SNES_NTSC_RGB_OUT( 2, line_out [2], SNES_NTSC_OUT_DEPTH );
		SNES_NTSC_RGB_OUT( 3, line_out [3], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 2, snes_ntsc_black );
		SNES_NTSC_RGB_OUT( 4, line_out [4], SNES_NTSC_OUT_DEPTH );
		SNES_NTSC_RGB_OUT( 5, line_out [5], SNES_NTSC_OUT_DEPTH );
		SNES_NTSC_RGB_OUT( 6, line_out [6], SNES_NTSC_OUT_DEPTH );

		burst_phase = (burst_phase + 1) % snes_ntsc_burst_count;
		input += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}

void retroarch_snes_ntsc_blit_hires( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input, long in_row_width,
		int burst_phase, int in_width, int in_height, void* rgb_out, long out_pitch, int first, int last )
{
	int chunk_count = (in_width - 2) / (snes_ntsc_in_chunk * 2);
	for ( ; in_height; --in_height )
	{
		SNES_NTSC_IN_T const* line_in = input;
		SNES_NTSC_HIRES_ROW( ntsc, burst_phase,
				snes_ntsc_black, snes_ntsc_black, snes_ntsc_black,
				SNES_NTSC_ADJ_IN( line_in [0] ),
				SNES_NTSC_ADJ_IN( line_in [1] ) );
		snes_ntsc_out_t* line_out = (snes_ntsc_out_t*) rgb_out;
		int n;
		line_in += 2;

		for ( n = chunk_count; n; --n )
		{
			/* twice as many input pixels per chunk */
			SNES_NTSC_COLOR_IN( 0, SNES_NTSC_ADJ_IN( line_in [0] ) );
			SNES_NTSC_HIRES_OUT( 0, line_out [0], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 1, SNES_NTSC_ADJ_IN( line_in [1] ) );
			SNES_NTSC_HIRES_OUT( 1, line_out [1], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 2, SNES_NTSC_ADJ_IN( line_in [2] ) );
			SNES_NTSC_HIRES_OUT( 2, line_out [2], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 3, SNES_NTSC_ADJ_IN( line_in [3] ) );
			SNES_NTSC_HIRES_OUT( 3, line_out [3], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 4, SNES_NTSC_ADJ_IN( line_in [4] ) );
			SNES_NTSC_HIRES_OUT( 4, line_out [4], SNES_NTSC_OUT_DEPTH );

			SNES_NTSC_COLOR_IN( 5, SNES_NTSC_ADJ_IN( line_in [5] ) );
			SNES_NTSC_HIRES_OUT( 5, line_out [5], SNES_NTSC_OUT_DEPTH );
			SNES_NTSC_HIRES_OUT( 6, line_out [6], SNES_NTSC_OUT_DEPTH );

			line_in  += 6;
			line_out += 7;
		}

		SNES_NTSC_COLOR_IN( 0, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 0, line_out [0], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 1, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 1, line_out [1], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 2, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 2, line_out [2], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 3, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 3, line_out [3], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 4, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 4, line_out [4], SNES_NTSC_OUT_DEPTH );

		SNES_NTSC_COLOR_IN( 5, snes_ntsc_black );
		SNES_NTSC_HIRES_OUT( 5, line_out [5], SNES_NTSC_OUT_DEPTH );
		SNES_NTSC_HIRES_OUT( 6, line_out [6], SNES_NTSC_OUT_DEPTH );

		burst_phase = (burst_phase + 1) % snes_ntsc_burst_count;
		input += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}


/* Vector blitters.

Read the scalar blitters above as sums over input pixels: a pixel's kernel
is added into fourteen consecutive output pixels, entry by entry. Each
chunk of 7 output pixels is fed by the chunk's own pixels and the two
chunks before it. Kernel slot i (the i in COLOR_IN( i, ... )) is taken in
at output s(i) of its chunk, and its entries for this chunk's outputs x
come from
   - the slot's pixel one chunk back, entries b(i)+7-s(i)+x, for every x;
   - for x < s(i), the pixel two chunks back, entries b(i)+14-s(i)+x;
   - for x >= s(i), this chunk's pixel, entries b(i)-s(i)+x.
That is whole-vector loads and a lane blend. The low-res blitter's slots
have (s, b) = (0, 0), (2, 14), (4, 28); the hires one's (0, 0), (1, 0),
(2, 14), (3, 14), (4, 28), (5, 28). The sums wrap in 32 bits like the
scalar ones, and clamping and packing are the same operations lane-wise,
so the output is the same to the bit.

A row starts with the scalar blitters' own lead-in: the chunk before the
first is black but for its last one (low-res) or two (hires) slots, which
take the first input pixels, and the chunk before that is black; the last
chunk is all black. */

#ifdef SNES_NTSC_VECTOR

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define SNES_NTSC_HAVE_SSE2 1
#include <emmintrin.h>
#else
#define SNES_NTSC_HAVE_NEON 1
#include <arm_neon.h>
#endif

#ifdef SNES_NTSC_HAVE_SSE2
#if defined(_MSC_VER) && !defined(__clang__)
#if _MSC_VER >= 1911
#define SNES_NTSC_HAVE_AVX2 1
#define SNES_NTSC_TARGET_AVX2
#endif
#elif defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
/* PlayStation clang and clang-cl declare only the intrinsics of ISAs
 * the whole build enables, so AVX2 is built only where the baseline
 * has it. */
#ifdef __AVX2__
#define SNES_NTSC_HAVE_AVX2 1
#define SNES_NTSC_TARGET_AVX2
#endif
#elif (defined(__clang__) && __clang_major__ >= 4) \
	|| (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5)
#define SNES_NTSC_HAVE_AVX2 1
#define SNES_NTSC_TARGET_AVX2 __attribute__((target("avx2")))
#endif
#endif

#ifdef SNES_NTSC_HAVE_AVX2
#include <immintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#define SNES_NTSC_ZEROUPPER() _mm256_zeroupper()
#else
#define SNES_NTSC_ZEROUPPER()
#endif
#endif

/* The row loops shared by every vector blitter, for the low-res (3 slots)
 * and hires (6 slots) layouts. CHUNK( out, last ) adds up the chunk from
 * c0.. (this chunk's kernels), p0.. (one back) and q0.. (two back), kept
 * in variables of their own so they stay in registers, and stores its 7
 * pixels at out; when last is 0 it may write an eighth, which the next
 * chunk overwrites. */
#define SNES_NTSC_VEC_ENTRY( v, in ) {\
	unsigned const px_ = SNES_NTSC_ADJ_IN( in );\
	v = SNES_NTSC_IN_FORMAT( ktable, px_ );\
}

#define SNES_NTSC_VEC_ROWS3( CHUNK ) {\
	int const chunk_count = (in_width - 1) / snes_ntsc_in_chunk;\
	for ( ; in_height; --in_height )\
	{\
		char const* ktable = (char const*) ntsc->table +\
				burst_phase * (snes_ntsc_burst_size * sizeof (snes_ntsc_entry_t));\
		snes_ntsc_entry_t const* const black = SNES_NTSC_IN_FORMAT( ktable, snes_ntsc_black );\
		snes_ntsc_entry_t const *c0, *c1, *c2;\
		snes_ntsc_entry_t const *p0 = black, *p1 = black, *p2;\
		snes_ntsc_entry_t const *q0 = black, *q1 = black, *q2 = black;\
		SNES_NTSC_IN_T const* line_in = input;\
		uint16_t* line_out = (uint16_t*) rgb_out;\
		int n;\
		SNES_NTSC_VEC_ENTRY( p2, line_in [0] );\
		++line_in;\
		for ( n = chunk_count; n; --n )\
		{\
			SNES_NTSC_VEC_ENTRY( c0, line_in [0] );\
			SNES_NTSC_VEC_ENTRY( c1, line_in [1] );\
			SNES_NTSC_VEC_ENTRY( c2, line_in [2] );\
			CHUNK( line_out, 0 );\
			q0 = p0; q1 = p1; q2 = p2;\
			p0 = c0; p1 = c1; p2 = c2;\
			line_in  += 3;\
			line_out += 7;\
		}\
		c0 = c1 = c2 = black;\
		CHUNK( line_out, 1 );\
		burst_phase = (burst_phase + 1) % snes_ntsc_burst_count;\
		input += in_row_width;\
		rgb_out = (char*) rgb_out + out_pitch;\
	}\
}

#define SNES_NTSC_VEC_ROWS6( CHUNK ) {\
	int const chunk_count = (in_width - 2) / (snes_ntsc_in_chunk * 2);\
	for ( ; in_height; --in_height )\
	{\
		char const* ktable = (char const*) ntsc->table +\
				burst_phase * (snes_ntsc_burst_size * sizeof (snes_ntsc_entry_t));\
		snes_ntsc_entry_t const* const black = SNES_NTSC_IN_FORMAT( ktable, snes_ntsc_black );\
		snes_ntsc_entry_t const *c0, *c1, *c2, *c3, *c4, *c5;\
		snes_ntsc_entry_t const *p0 = black, *p1 = black, *p2 = black, *p3 = black, *p4, *p5;\
		snes_ntsc_entry_t const *q0 = black, *q1 = black, *q2 = black, *q3 = black,\
				*q4 = black, *q5 = black;\
		SNES_NTSC_IN_T const* line_in = input;\
		uint16_t* line_out = (uint16_t*) rgb_out;\
		int n;\
		SNES_NTSC_VEC_ENTRY( p4, line_in [0] );\
		SNES_NTSC_VEC_ENTRY( p5, line_in [1] );\
		line_in += 2;\
		for ( n = chunk_count; n; --n )\
		{\
			SNES_NTSC_VEC_ENTRY( c0, line_in [0] );\
			SNES_NTSC_VEC_ENTRY( c1, line_in [1] );\
			SNES_NTSC_VEC_ENTRY( c2, line_in [2] );\
			SNES_NTSC_VEC_ENTRY( c3, line_in [3] );\
			SNES_NTSC_VEC_ENTRY( c4, line_in [4] );\
			SNES_NTSC_VEC_ENTRY( c5, line_in [5] );\
			CHUNK( line_out, 0 );\
			q0 = p0; q1 = p1; q2 = p2; q3 = p3; q4 = p4; q5 = p5;\
			p0 = c0; p1 = c1; p2 = c2; p3 = c3; p4 = c4; p5 = c5;\
			line_in  += 6;\
			line_out += 7;\
		}\
		c0 = c1 = c2 = c3 = c4 = c5 = black;\
		CHUNK( line_out, 1 );\
		burst_phase = (burst_phase + 1) % snes_ntsc_burst_count;\
		input += in_row_width;\
		rgb_out = (char*) rgb_out + out_pitch;\
	}\
}

#ifdef SNES_NTSC_HAVE_SSE2
/* Four lanes, starting at output L of the chunk, of slot i's terms. */
static __m128i snes_ntsc_sse2_term( snes_ntsc_entry_t const* c,
		snes_ntsc_entry_t const* p, snes_ntsc_entry_t const* q,
		int s, int b, int L )
{
	__m128i t = _mm_loadu_si128( (__m128i const*) (p + b + 7 - s + L) );
	int nq = s - L; /* lanes from two chunks back */
	if ( nq <= 0 )
		return _mm_add_epi32( t, _mm_loadu_si128( (__m128i const*) (c + b - s + L) ) );
	if ( nq >= 4 )
		return _mm_add_epi32( t, _mm_loadu_si128( (__m128i const*) (q + b + 14 - s + L) ) );
	{
		__m128i m  = _mm_setr_epi32( -1, nq > 1 ? -1 : 0, nq > 2 ? -1 : 0, 0 );
		__m128i vq = _mm_loadu_si128( (__m128i const*) (q + b + 14 - s + L) );
		__m128i vc = _mm_loadu_si128( (__m128i const*) (c + b - s + L) );
		return _mm_add_epi32( t, _mm_or_si128( _mm_and_si128( m, vq ),
				_mm_andnot_si128( m, vc ) ) );
	}
}

/* SNES_NTSC_CLAMP_ and the 16-bit SNES_NTSC_RGB_OUT_, lane-wise; X is
 * the x of those macros (1 low-res, 0 hires). */
#define SNES_NTSC_SSE2_RGB16( raw, X ) {\
	__m128i sub_ = _mm_and_si128( _mm_srli_epi32( raw, 9 - (X) ),\
			_mm_set1_epi32( (int) snes_ntsc_clamp_mask ) );\
	__m128i clamp_ = _mm_sub_epi32( _mm_set1_epi32( (int) snes_ntsc_clamp_add ), sub_ );\
	raw    = _mm_or_si128( raw, clamp_ );\
	clamp_ = _mm_sub_epi32( clamp_, sub_ );\
	raw    = _mm_and_si128( raw, clamp_ );\
	raw    = _mm_or_si128( _mm_or_si128(\
			_mm_and_si128( _mm_srli_epi32( raw, 13 - (X) ), _mm_set1_epi32( 0xF800 ) ),\
			_mm_and_si128( _mm_srli_epi32( raw,  8 - (X) ), _mm_set1_epi32( 0x07E0 ) ) ),\
			_mm_and_si128( _mm_srli_epi32( raw,  4 - (X) ), _mm_set1_epi32( 0x001F ) ) );\
	/* to signed 16 bits for the saturating pack */\
	raw    = _mm_srai_epi32( _mm_slli_epi32( raw, 16 ), 16 );\
}

static void snes_ntsc_sse2_store( uint16_t* out, __m128i lo, __m128i hi, int last )
{
	__m128i px = _mm_packs_epi32( lo, hi );
	if ( !last )
		_mm_storeu_si128( (__m128i*) out, px );
	else
	{
		int pair = _mm_cvtsi128_si32( _mm_srli_si128( px, 8 ) );
		_mm_storel_epi64( (__m128i*) out, px );
		memcpy( out + 4, &pair, sizeof pair );
		out [6] = (uint16_t) _mm_extract_epi16( px, 6 );
	}
}

#define SNES_NTSC_SSE2_CHUNK3( out, last ) {\
	__m128i lo = _mm_add_epi32( _mm_add_epi32(\
			snes_ntsc_sse2_term( c0, p0, q0, 0,  0, 0 ),\
			snes_ntsc_sse2_term( c1, p1, q1, 2, 14, 0 ) ),\
			snes_ntsc_sse2_term( c2, p2, q2, 4, 28, 0 ) );\
	__m128i hi = _mm_add_epi32( _mm_add_epi32(\
			snes_ntsc_sse2_term( c0, p0, q0, 0,  0, 4 ),\
			snes_ntsc_sse2_term( c1, p1, q1, 2, 14, 4 ) ),\
			snes_ntsc_sse2_term( c2, p2, q2, 4, 28, 4 ) );\
	SNES_NTSC_SSE2_RGB16( lo, 1 );\
	SNES_NTSC_SSE2_RGB16( hi, 1 );\
	snes_ntsc_sse2_store( out, lo, hi, last );\
}

#define SNES_NTSC_SSE2_HALF6( L ) _mm_add_epi32( _mm_add_epi32( _mm_add_epi32(\
		snes_ntsc_sse2_term( c0, p0, q0, 0,  0, L ),\
		snes_ntsc_sse2_term( c1, p1, q1, 1,  0, L ) ),\
		_mm_add_epi32(\
		snes_ntsc_sse2_term( c2, p2, q2, 2, 14, L ),\
		snes_ntsc_sse2_term( c3, p3, q3, 3, 14, L ) ) ),\
		_mm_add_epi32(\
		snes_ntsc_sse2_term( c4, p4, q4, 4, 28, L ),\
		snes_ntsc_sse2_term( c5, p5, q5, 5, 28, L ) ) )

#define SNES_NTSC_SSE2_CHUNK6( out, last ) {\
	__m128i lo = SNES_NTSC_SSE2_HALF6( 0 );\
	__m128i hi = SNES_NTSC_SSE2_HALF6( 4 );\
	SNES_NTSC_SSE2_RGB16( lo, 0 );\
	SNES_NTSC_SSE2_RGB16( hi, 0 );\
	snes_ntsc_sse2_store( out, lo, hi, last );\
}

static void snes_ntsc_blit_sse2( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS3( SNES_NTSC_SSE2_CHUNK3 )
}

static void snes_ntsc_blit_hires_sse2( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS6( SNES_NTSC_SSE2_CHUNK6 )
}
#endif /* SNES_NTSC_HAVE_SSE2 */

#ifdef SNES_NTSC_HAVE_AVX2
/* All 8 lanes (7 used) of slot i's terms; M has bit x set for the lanes
 * x < s, a literal for the blend. */
#define SNES_NTSC_AVX2_TERM( i, s, b, M ) (_mm256_add_epi32(\
		_mm256_loadu_si256( (__m256i const*) (p##i + (b) + 7 - (s)) ),\
		(s) == 0 ? _mm256_loadu_si256( (__m256i const*) (c##i + (b)) ) :\
		_mm256_blend_epi32(\
			_mm256_loadu_si256( (__m256i const*) (c##i + (b) - (s)) ),\
			_mm256_loadu_si256( (__m256i const*) (q##i + (b) + 14 - (s)) ), M ) ))

#define SNES_NTSC_AVX2_RGB16( raw, X ) {\
	__m256i sub_ = _mm256_and_si256( _mm256_srli_epi32( raw, 9 - (X) ),\
			_mm256_set1_epi32( (int) snes_ntsc_clamp_mask ) );\
	__m256i clamp_ = _mm256_sub_epi32( _mm256_set1_epi32( (int) snes_ntsc_clamp_add ), sub_ );\
	raw    = _mm256_or_si256( raw, clamp_ );\
	clamp_ = _mm256_sub_epi32( clamp_, sub_ );\
	raw    = _mm256_and_si256( raw, clamp_ );\
	raw    = _mm256_or_si256( _mm256_or_si256(\
			_mm256_and_si256( _mm256_srli_epi32( raw, 13 - (X) ), _mm256_set1_epi32( 0xF800 ) ),\
			_mm256_and_si256( _mm256_srli_epi32( raw,  8 - (X) ), _mm256_set1_epi32( 0x07E0 ) ) ),\
			_mm256_and_si256( _mm256_srli_epi32( raw,  4 - (X) ), _mm256_set1_epi32( 0x001F ) ) );\
}

#define SNES_NTSC_AVX2_STORE( out, raw, last ) {\
	__m128i px = _mm_packus_epi32( _mm256_castsi256_si128( raw ),\
			_mm256_extracti128_si256( raw, 1 ) );\
	if ( !(last) )\
		_mm_storeu_si128( (__m128i*) (out), px );\
	else\
	{\
		int pair = _mm_cvtsi128_si32( _mm_srli_si128( px, 8 ) );\
		_mm_storel_epi64( (__m128i*) (out), px );\
		memcpy( (out) + 4, &pair, sizeof pair );\
		(out) [6] = (uint16_t) _mm_extract_epi16( px, 6 );\
	}\
}

#define SNES_NTSC_AVX2_CHUNK3( out, last ) {\
	__m256i raw = _mm256_add_epi32( _mm256_add_epi32(\
			SNES_NTSC_AVX2_TERM( 0, 0,  0, 0x00 ),\
			SNES_NTSC_AVX2_TERM( 1, 2, 14, 0x03 ) ),\
			SNES_NTSC_AVX2_TERM( 2, 4, 28, 0x0F ) );\
	SNES_NTSC_AVX2_RGB16( raw, 1 );\
	SNES_NTSC_AVX2_STORE( out, raw, last );\
}

#define SNES_NTSC_AVX2_CHUNK6( out, last ) {\
	__m256i raw = _mm256_add_epi32( _mm256_add_epi32( _mm256_add_epi32(\
			SNES_NTSC_AVX2_TERM( 0, 0,  0, 0x00 ),\
			SNES_NTSC_AVX2_TERM( 1, 1,  0, 0x01 ) ),\
			_mm256_add_epi32(\
			SNES_NTSC_AVX2_TERM( 2, 2, 14, 0x03 ),\
			SNES_NTSC_AVX2_TERM( 3, 3, 14, 0x07 ) ) ),\
			_mm256_add_epi32(\
			SNES_NTSC_AVX2_TERM( 4, 4, 28, 0x0F ),\
			SNES_NTSC_AVX2_TERM( 5, 5, 28, 0x1F ) ) );\
	SNES_NTSC_AVX2_RGB16( raw, 0 );\
	SNES_NTSC_AVX2_STORE( out, raw, last );\
}

SNES_NTSC_TARGET_AVX2
static void snes_ntsc_blit_avx2( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS3( SNES_NTSC_AVX2_CHUNK3 )
	SNES_NTSC_ZEROUPPER();
}

SNES_NTSC_TARGET_AVX2
static void snes_ntsc_blit_hires_avx2( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS6( SNES_NTSC_AVX2_CHUNK6 )
	SNES_NTSC_ZEROUPPER();
}
#endif /* SNES_NTSC_HAVE_AVX2 */

#ifdef SNES_NTSC_HAVE_NEON
static uint32x4_t snes_ntsc_neon_term( snes_ntsc_entry_t const* c,
		snes_ntsc_entry_t const* p, snes_ntsc_entry_t const* q,
		int s, int b, int L )
{
	uint32x4_t t = vld1q_u32( p + b + 7 - s + L );
	int nq = s - L;
	if ( nq <= 0 )
		return vaddq_u32( t, vld1q_u32( c + b - s + L ) );
	if ( nq >= 4 )
		return vaddq_u32( t, vld1q_u32( q + b + 14 - s + L ) );
	{
		static uint32_t const masks [3] [4] = {
			{ ~0u, 0, 0, 0 }, { ~0u, ~0u, 0, 0 }, { ~0u, ~0u, ~0u, 0 } };
		uint32x4_t m = vld1q_u32( masks [nq - 1] );
		return vaddq_u32( t, vbslq_u32( m, vld1q_u32( q + b + 14 - s + L ),
				vld1q_u32( c + b - s + L ) ) );
	}
}

#define SNES_NTSC_NEON_RGB16( raw, X ) {\
	uint32x4_t sub_ = vandq_u32( vshrq_n_u32( raw, 9 - (X) ),\
			vdupq_n_u32( (uint32_t) snes_ntsc_clamp_mask ) );\
	uint32x4_t clamp_ = vsubq_u32( vdupq_n_u32( (uint32_t) snes_ntsc_clamp_add ), sub_ );\
	raw    = vorrq_u32( raw, clamp_ );\
	clamp_ = vsubq_u32( clamp_, sub_ );\
	raw    = vandq_u32( raw, clamp_ );\
	raw    = vorrq_u32( vorrq_u32(\
			vandq_u32( vshrq_n_u32( raw, 13 - (X) ), vdupq_n_u32( 0xF800 ) ),\
			vandq_u32( vshrq_n_u32( raw,  8 - (X) ), vdupq_n_u32( 0x07E0 ) ) ),\
			vandq_u32( vshrq_n_u32( raw,  4 - (X) ), vdupq_n_u32( 0x001F ) ) );\
}

static void snes_ntsc_neon_store( uint16_t* out, uint32x4_t lo, uint32x4_t hi, int last )
{
	uint16x4_t a = vmovn_u32( lo );
	uint16x4_t b = vmovn_u32( hi );
	if ( !last )
		vst1q_u16( out, vcombine_u16( a, b ) );
	else
	{
		vst1_u16( out, a );
		vst1_lane_u16( out + 4, b, 0 );
		vst1_lane_u16( out + 5, b, 1 );
		vst1_lane_u16( out + 6, b, 2 );
	}
}

#define SNES_NTSC_NEON_CHUNK3( out, last ) {\
	uint32x4_t lo = vaddq_u32( vaddq_u32(\
			snes_ntsc_neon_term( c0, p0, q0, 0,  0, 0 ),\
			snes_ntsc_neon_term( c1, p1, q1, 2, 14, 0 ) ),\
			snes_ntsc_neon_term( c2, p2, q2, 4, 28, 0 ) );\
	uint32x4_t hi = vaddq_u32( vaddq_u32(\
			snes_ntsc_neon_term( c0, p0, q0, 0,  0, 4 ),\
			snes_ntsc_neon_term( c1, p1, q1, 2, 14, 4 ) ),\
			snes_ntsc_neon_term( c2, p2, q2, 4, 28, 4 ) );\
	SNES_NTSC_NEON_RGB16( lo, 1 );\
	SNES_NTSC_NEON_RGB16( hi, 1 );\
	snes_ntsc_neon_store( out, lo, hi, last );\
}

#define SNES_NTSC_NEON_HALF6( L ) vaddq_u32( vaddq_u32( vaddq_u32(\
		snes_ntsc_neon_term( c0, p0, q0, 0,  0, L ),\
		snes_ntsc_neon_term( c1, p1, q1, 1,  0, L ) ),\
		vaddq_u32(\
		snes_ntsc_neon_term( c2, p2, q2, 2, 14, L ),\
		snes_ntsc_neon_term( c3, p3, q3, 3, 14, L ) ) ),\
		vaddq_u32(\
		snes_ntsc_neon_term( c4, p4, q4, 4, 28, L ),\
		snes_ntsc_neon_term( c5, p5, q5, 5, 28, L ) ) )

#define SNES_NTSC_NEON_CHUNK6( out, last ) {\
	uint32x4_t lo = SNES_NTSC_NEON_HALF6( 0 );\
	uint32x4_t hi = SNES_NTSC_NEON_HALF6( 4 );\
	SNES_NTSC_NEON_RGB16( lo, 0 );\
	SNES_NTSC_NEON_RGB16( hi, 0 );\
	snes_ntsc_neon_store( out, lo, hi, last );\
}

static void snes_ntsc_blit_neon( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS3( SNES_NTSC_NEON_CHUNK3 )
}

static void snes_ntsc_blit_hires_neon( snes_ntsc_t const* ntsc, SNES_NTSC_IN_T const* input,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, int first, int last )
{
	(void) first; (void) last;
	SNES_NTSC_VEC_ROWS6( SNES_NTSC_NEON_CHUNK6 )
}
#endif /* SNES_NTSC_HAVE_NEON */

#endif /* vector blitters */

snes_ntsc_blit_t retroarch_snes_ntsc_pick_blit( int hires, unsigned simd )
{
	(void) simd;
#ifdef SNES_NTSC_HAVE_AVX2
	if ( simd & SOFTFILTER_SIMD_AVX2 )
		return hires ? snes_ntsc_blit_hires_avx2 : snes_ntsc_blit_avx2;
#endif
#ifdef SNES_NTSC_HAVE_SSE2
	return hires ? snes_ntsc_blit_hires_sse2 : snes_ntsc_blit_sse2;
#elif defined(SNES_NTSC_HAVE_NEON)
	return hires ? snes_ntsc_blit_hires_neon : snes_ntsc_blit_neon;
#else
	return hires ? retroarch_snes_ntsc_blit_hires : retroarch_snes_ntsc_blit;
#endif
}
#endif

#endif
