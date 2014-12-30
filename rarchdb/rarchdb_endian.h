#ifndef __RARCHDB_MSGPACK_ENDIAN_H
#define __RARCHDB_MSGPACK_ENDIAN_H

#include <stdint.h>

#ifndef INLINE
#define INLINE inline
#endif

static INLINE uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

#define swap16(val)				\
	((((uint16_t)(val) & 0x00ff) << 8)	\
	 | (((uint16_t)(val) & 0xff00) >> 8))

#define swap32(val)					\
	((((uint32_t)(val) & 0x000000ff) << 24)		\
	 | (((uint32_t)(val) & 0x0000ff00) << 8)	\
	 | (((uint32_t)(val) & 0x00ff0000) >> 8)	\
	 | (((uint32_t)(val) & 0xff000000) >> 24))

#define swap64(val)						\
	((((uint64_t)(val) & 0x00000000000000ffULL) << 56)	\
		 | (((uint64_t)(val) & 0x000000000000ff00ULL) << 40)	\
		 | (((uint64_t)(val) & 0x0000000000ff0000ULL) << 24)	\
		 | (((uint64_t)(val) & 0x00000000ff000000ULL) << 8)	\
		 | (((uint64_t)(val) & 0x000000ff00000000ULL) >> 8)	\
		 | (((uint64_t)(val) & 0x0000ff0000000000ULL) >> 24)	\
		 | (((uint64_t)(val) & 0x00ff000000000000ULL) >> 40)	\
		 | (((uint64_t)(val) & 0xff00000000000000ULL) >> 56))


#define httobe64(x) (is_little_endian() ? swap64(x) : (x))
#define httobe32(x) (is_little_endian() ? swap32(x) : (x))
#define httobe16(x) (is_little_endian() ? swap16(x) : (x))

#define betoht16(x) httobe16(x)
#define betoht32(x) httobe32(x)
#define betoht64(x) httobe64(x)

#endif
