#ifndef __LIBRETRODB_MSGPACK_ENDIAN_H
#define __LIBRETRODB_MSGPACK_ENDIAN_H

#include <stdint.h>
#include <retro_endianness.h>

#ifndef swap64
#define swap64(val)                                             \
	((((uint64_t)(val) & 0x00000000000000ffULL) << 56)      \
	 | (((uint64_t)(val) & 0x000000000000ff00ULL) << 40)    \
	 | (((uint64_t)(val) & 0x0000000000ff0000ULL) << 24)    \
	 | (((uint64_t)(val) & 0x00000000ff000000ULL) << 8)     \
	 | (((uint64_t)(val) & 0x000000ff00000000ULL) >> 8)     \
	 | (((uint64_t)(val) & 0x0000ff0000000000ULL) >> 24)    \
	 | (((uint64_t)(val) & 0x00ff000000000000ULL) >> 40)    \
	 | (((uint64_t)(val) & 0xff00000000000000ULL) >> 56))
#endif


#define httobe64(x) (is_little_endian() ? swap64(x) : (x))
#define httobe32(x) (is_little_endian() ? SWAP32(x) : (x))
#define httobe16(x) (is_little_endian() ? SWAP16(x) : (x))

#define betoht16(x) httobe16(x)
#define betoht32(x) httobe32(x)
#define betoht64(x) httobe64(x)

#endif
