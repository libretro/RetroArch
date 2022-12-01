/* xdelta3 - delta compression tools and library
   Copyright 2016 Joshua MacDonald

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef _XDELTA3_HASH_H_
#define _XDELTA3_HASH_H_

#include "xdelta3-internal.h"

#if XD3_DEBUG
#define SMALL_HASH_DEBUG1(s,inp)                                  \
  uint32_t debug_state;                                           \
  uint32_t debug_hval = xd3_checksum_hash (& (s)->small_hash,     \
              xd3_scksum (&debug_state, (inp), (s)->smatcher.small_look))
#define SMALL_HASH_DEBUG2(s,inp)                                  \
  XD3_ASSERT (debug_hval == xd3_checksum_hash (& (s)->small_hash, \
              xd3_scksum (&debug_state, (inp), (s)->smatcher.small_look)))
#else
#define SMALL_HASH_DEBUG1(s,inp)
#define SMALL_HASH_DEBUG2(s,inp)
#endif /* XD3_DEBUG */

#if UNALIGNED_OK
#define UNALIGNED_READ32(dest,src) (*(dest)) = (*(uint32_t*)(src))
#else
#define UNALIGNED_READ32(dest,src) memcpy((dest), (src), 4);
#endif

/* These are good hash multipliers for 32-bit and 64-bit LCGs: see
 * "linear congruential generators of different sizes and good lattice
 * structure" */
#define xd3_hash_multiplier32 1597334677U
#define xd3_hash_multiplier64 1181783497276652981ULL

/* TODO: small cksum is hard-coded for 4 bytes (i.e., "look" is unused) */
static inline uint32_t
xd3_scksum (uint32_t *state,
            const uint8_t *base,
            const usize_t look)
{
  UNALIGNED_READ32(state, base);
  return (*state) * xd3_hash_multiplier32;
}
static inline uint32_t
xd3_small_cksum_update (uint32_t *state,
			const uint8_t *base,
			usize_t look)
{
  UNALIGNED_READ32(state, base+1);
  return (*state) * xd3_hash_multiplier32;
}

#if XD3_ENCODER
inline usize_t
xd3_checksum_hash (const xd3_hash_cfg *cfg, const usize_t cksum)
{
  return (cksum >> cfg->shift) ^ (cksum & cfg->mask);
}

#if SIZEOF_USIZE_T == 4
inline uint32_t
xd3_large32_cksum (xd3_hash_cfg *cfg, const uint8_t *base, const usize_t look)
{
  uint32_t h = 0;
  for (usize_t i = 0; i < look; i++) {
    h += base[i] * cfg->powers[i];
  }
  return h;
}

inline uint32_t
xd3_large32_cksum_update (xd3_hash_cfg *cfg, const uint32_t cksum,
			  const uint8_t *base, const usize_t look)
{
  return xd3_hash_multiplier32 * cksum - cfg->multiplier * base[0] + base[look];
}
#endif

#if SIZEOF_USIZE_T == 8
inline uint64_t
xd3_large64_cksum (xd3_hash_cfg *cfg, const uint8_t *base, const usize_t look)
{
  uint64_t h = 0;
  for (usize_t i = 0; i < look; i++) {
    h += base[i] * cfg->powers[i];
  }
  return h;
}

inline uint64_t
xd3_large64_cksum_update (xd3_hash_cfg *cfg, const uint64_t cksum,
			  const uint8_t *base, const usize_t look)
{
  return xd3_hash_multiplier64 * cksum - cfg->multiplier * base[0] + base[look];
}
#endif

static usize_t
xd3_size_hashtable_bits (usize_t slots)
{
  usize_t bits = (SIZEOF_USIZE_T * 8) - 1;
  usize_t i;

  for (i = 3; i <= bits; i += 1)
    {
      if (slots < (1U << i))
	{
	  /* Note: this is the compaction=1 setting measured in
	   * checksum_test */
	  bits = i - 1;
	  break;
	}
    }

  return bits;
}

int
xd3_size_hashtable (xd3_stream   *stream,
		    usize_t       slots,
		    usize_t       look,
		    xd3_hash_cfg *cfg)
{
  usize_t bits = xd3_size_hashtable_bits (slots);

  cfg->size  = (1U << bits);
  cfg->mask  = (cfg->size - 1);
  cfg->shift = (SIZEOF_USIZE_T * 8) - bits;
  cfg->look  = look;

  if ((cfg->powers = 
       (usize_t*) xd3_alloc0 (stream, look, sizeof (usize_t))) == NULL)
    {
      return ENOMEM;
    }

  cfg->powers[look-1] = 1;
  for (int i = look-2; i >= 0; i--)
    {
      cfg->powers[i] = cfg->powers[i+1] * xd3_hash_multiplier;
    }
  cfg->multiplier = cfg->powers[0] * xd3_hash_multiplier;

  return 0;
}

#endif /* XD3_ENCODER */
#endif /* _XDELTA3_HASH_H_ */
